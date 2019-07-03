/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "perf/aircraftperfcontroller.h"
#include "perf/aircraftperfdialog.h"

#include "gui/mainwindow.h"
#include "gui/helphandler.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "gui/widgetutil.h"
#include "navapp.h"
#include "route/route.h"
#include "geo/calculations.h"
#include "weather/windreporter.h"
#include "common/formatter.h"
#include "fs/perf/aircraftperf.h"
#include "gui/tools.h"
#include "gui/dialog.h"
#include "ui_mainwindow.h"
#include "common/unit.h"
#include "common/tabindexes.h"
#include "util/htmlbuilder.h"
#include "gui/filehistoryhandler.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "navapp.h"
#include "perf/perfmergedialog.h"
#include "route/routealtitude.h"
#include "gui/widgetstate.h"
#include "fs/perf/aircraftperfhandler.h"
#include "fs/sc/simconnectdata.h"

#include <QDebug>
#include <QUrlQuery>

using atools::fs::perf::AircraftPerf;
using atools::fs::perf::AircraftPerfHandler;
using atools::util::HtmlBuilder;

AircraftPerfController::AircraftPerfController(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  perf = new AircraftPerf();

  // Remember original font for resizing in options
  infoFontPtSize = static_cast<float>(ui->textBrowserAircraftPerformanceReport->font().pointSizeF());

  QStringList paths({QApplication::applicationDirPath()});
  ui->textBrowserAircraftPerformanceReport->setSearchPaths(paths);
  ui->textBrowserAircraftPerformanceCurrent->setSearchPaths(paths);

  fileHistory = new atools::gui::FileHistoryHandler(this, lnm::AIRCRAFT_PERF_FILENAMESRECENT,
                                                    ui->menuAircraftPerformanceRecent,
                                                    ui->actionAircraftPerformanceClearMenu);

  connect(fileHistory, &atools::gui::FileHistoryHandler::fileSelected,
          this, &AircraftPerfController::loadFile);
  connect(ui->textBrowserAircraftPerformanceReport, &QTextBrowser::anchorClicked,
          this, &AircraftPerfController::anchorClicked);
  connect(ui->textBrowserAircraftPerformanceCurrent, &QTextBrowser::anchorClicked,
          this, &AircraftPerfController::anchorClicked);

  // Pass wind change with a delay to avoid lagging mouse wheel
  connect(&windChangeTimer, &QTimer::timeout, this, &AircraftPerfController::windChangedDelayed);
  windChangeTimer.setSingleShot(true);

  connect(ui->checkBoxAircraftPerformanceWindMan, &QCheckBox::toggled,
          this, &AircraftPerfController::manualWindToggled);

  // Widgets are only updated if visible - update on visbility changes of dock or tabs
  connect(ui->tabWidgetRoute, &QTabWidget::currentChanged, this, &AircraftPerfController::visibilityChanged);
  connect(ui->dockWidgetRoute, &QDockWidget::visibilityChanged, this, &AircraftPerfController::visibilityChanged);

  // Create performance handler for background collection
  perfHandler = new AircraftPerfHandler(this);
  connect(perfHandler, &AircraftPerfHandler::flightSegmentChanged, this, &AircraftPerfController::flightSegmentChanged);
}

AircraftPerfController::~AircraftPerfController()
{
  windChangeTimer.stop();
  delete fileHistory;
  delete perfHandler;
  delete perf;
}

void AircraftPerfController::create()
{
  qDebug() << Q_FUNC_INFO;

  if(checkForChanges())
  {
    // Ok to overwrite
    perf->resetToDefault();
    currentFilepath.clear();
    changed = false;
  }

  updateActionStates();
  NavApp::setStatusMessage(tr("Aircraft performance created."));
  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::edit()
{
  qDebug() << Q_FUNC_INFO;

  AircraftPerfDialog dialog(mainWindow, *perf);
  if(dialog.exec() == QDialog::Accepted)
  {
    *perf = dialog.getAircraftPerf();
    changed = true;
    windChangeTimer.stop();
    NavApp::setStatusMessage(tr("Aircraft performance changed."));
  }
  updateActionStates();
  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::loadFile(const QString& perfFile)
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    if(checkForChanges())
    {
      if(!perfFile.isEmpty())
      {
        currentFilepath = perfFile;
        perf->load(perfFile);
        changed = false;
        fileHistory->addFile(perfFile);
        windChangeTimer.stop();
        NavApp::setStatusMessage(tr("Aircraft performance loaded."));
      }
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
    fileHistory->removeFile(perfFile);
  }
  catch(...)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
    fileHistory->removeFile(perfFile);
  }

  updateActionStates();
  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::loadAndMerge()
{
  qDebug() << Q_FUNC_INFO;

  QString perfFile = atools::gui::Dialog(mainWindow).openFileDialog(
    tr("Open Aircraft Performance File for merge"),
    tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
    "AircraftPerformance/");

  if(!perfFile.isEmpty())
  {
    atools::fs::perf::AircraftPerf perfFrom;
    perfFrom.load(perfFile);
    fileHistory->addFile(perfFile);

    PerfMergeDialog dialog(mainWindow, perfFrom, *perf, true /* show all */);
    if(dialog.exec() == QDialog::Accepted)
    {
      changed = dialog.hasChanged();
      windChangeTimer.stop();
      updateActionStates();
      NavApp::setStatusMessage(tr("Aircraft performance merged."));
    }
  }
}

void AircraftPerfController::restartCollect()
{
  int result = atools::gui::Dialog(mainWindow).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_TRAIL,
                                  tr("Reset performance collection and loose all current values?"),
                                  tr("Do not &show this dialog again."),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    perfHandler->reset();
    updateReportCurrent();
    updateActionStates();
  }
}

void AircraftPerfController::mergeCollected()
{
  qDebug() << Q_FUNC_INFO;

  PerfMergeDialog dialog(mainWindow, perfHandler->getAircraftPerformance(), *perf, false /* show all */);
  if(dialog.exec() == QDialog::Accepted)
  {
    changed = dialog.hasChanged();
    windChangeTimer.stop();
    updateActionStates();
    NavApp::setStatusMessage(tr("Aircraft performance merged."));
  }
}

void AircraftPerfController::load()
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    if(checkForChanges())
    {
      QString perfFile = atools::gui::Dialog(mainWindow).openFileDialog(
        tr("Open Aircraft Performance File"),
        tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
        "AircraftPerformance/");

      if(!perfFile.isEmpty())
      {
        currentFilepath = perfFile;
        perf->load(perfFile);
        changed = false;
        fileHistory->addFile(perfFile);
        windChangeTimer.stop();
        NavApp::setStatusMessage(tr("Aircraft performance loaded."));
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
  }
  updateActionStates();
  emit aircraftPerformanceChanged(perf);
}

bool AircraftPerfController::save()
{
  qDebug() << Q_FUNC_INFO;

  if(currentFilepath.isEmpty())
    // Not save yet - use save as
    return saveAs();
  else
  {
    bool retval = true;
    try
    {
      perf->save(currentFilepath);
      fileHistory->addFile(currentFilepath);
      changed = false;
      NavApp::setStatusMessage(tr("Aircraft performance saved."));
    }
    catch(atools::Exception& e)
    {
      NavApp::deleteSplashScreen();
      atools::gui::ErrorHandler(mainWindow).handleException(e);
      retval = false;
    }
    catch(...)
    {
      NavApp::deleteSplashScreen();
      atools::gui::ErrorHandler(mainWindow).handleUnknownException();
      retval = false;
    }

    emit aircraftPerformanceChanged(perf);
    updateActionStates();
    updateReport();
    return retval;
  }
}

bool AircraftPerfController::saveAs()
{
  qDebug() << Q_FUNC_INFO;
  bool retval = false;
  QString perfFile = atools::gui::Dialog(mainWindow).saveFileDialog(
    tr("Save Aircraft Performance File"),
    tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
    "lnmperf", "AircraftPerformance/",
    QString(), currentFilepath.isEmpty() ? perf->getName() + ".lnmperf" : QFileInfo(currentFilepath).fileName(),
    false /* confirm overwrite */, true /* auto number */);

  try
  {
    if(!perfFile.isEmpty())
    {
      currentFilepath = perfFile;
      perf->save(perfFile);
      changed = false;
      retval = true;
      fileHistory->addFile(perfFile);
      NavApp::setStatusMessage(tr("Aircraft performance saved."));
      emit aircraftPerformanceChanged(perf);
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    retval = false;
  }
  catch(...)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    retval = false;
  }

  updateActionStates();
  updateReport();
  return retval;
}

void AircraftPerfController::helpClickedPerf() const
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "AIRCRAFTPERF.html",
                                           lnm::helpLanguageOnline());
}

void AircraftPerfController::helpClickedPerfCollect() const
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "AIRCRAFTPERFCOLL.html",
                                           lnm::helpLanguageOnline());
}

bool AircraftPerfController::checkForChanges()
{
  if(!changed)
    return true;

  QMessageBox msgBox;
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText(tr("Aircraft Performance has been changed."));
  msgBox.setInformativeText(tr("Save changes?"));
  msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel);

  int retval = msgBox.exec();

  switch(retval)
  {
    case QMessageBox::Save:
      if(currentFilepath.isEmpty())
        return saveAs();
      else
        return save();

    case QMessageBox::No:
      // ok to erase
      return true;

    case QMessageBox::Cancel:
      // stop any change erasing actions
      return false;
  }
  return false;
}

float AircraftPerfController::getWindDir() const
{
  return NavApp::getMainUi()->spinBoxAircraftPerformanceWindDirection->value();
}

float AircraftPerfController::getWindSpeed() const
{
  return Unit::rev(NavApp::getMainUi()->spinBoxAircraftPerformanceWindSpeed->value(), Unit::speedKtsF);
}

bool AircraftPerfController::isWindManual() const
{
  return NavApp::getMainUi()->checkBoxAircraftPerformanceWindMan->isChecked();
}

void AircraftPerfController::routeChanged(bool geometryChanged, bool newFlightplan)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(geometryChanged);
  Q_UNUSED(newFlightplan);
  perfHandler->setCruiseAltitude(NavApp::getAltitudeLegs().getCruiseAltitide());
  updateReport();
  updateReportCurrent();
  updateActionStates();
}

void AircraftPerfController::windUpdated()
{
  updateReport();
  updateReportCurrent();
}

void AircraftPerfController::routeAltitudeChanged(float altitudeFeet)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(altitudeFeet);

  perfHandler->setCruiseAltitude(NavApp::getAltitudeLegs().getCruiseAltitide());
  updateReport();
  updateReportCurrent();
  updateActionStates();
}

void AircraftPerfController::flightSegmentChanged(const atools::fs::perf::FlightSegment& flightSegment)
{
  qDebug() << Q_FUNC_INFO << AircraftPerfHandler::getFlightSegmentString(flightSegment);

  updateActionStates();
  updateReportCurrent();
  NavApp::setStatusMessage(tr("Flight segment %1.").
                           arg(AircraftPerfHandler::getFlightSegmentString(flightSegment).toLower()));
}

bool AircraftPerfController::isClimbValid() const
{
  return perf->isClimbValid();
}

bool AircraftPerfController::isDescentValid() const
{
  return perf->isDescentValid();
}

float AircraftPerfController::getRouteCruiseSpeedKts()
{
  // Use default if nothing loaded
  return perf->getCruiseSpeed();
}

void AircraftPerfController::connectAllSlots()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionAircraftPerformanceNew, &QAction::triggered, this, &AircraftPerfController::create);
  connect(ui->actionAircraftPerformanceLoad, &QAction::triggered, this, &AircraftPerfController::load);
  connect(ui->actionAircraftPerformanceSave, &QAction::triggered, this, &AircraftPerfController::save);
  connect(ui->actionAircraftPerformanceSaveAs, &QAction::triggered, this, &AircraftPerfController::saveAs);
  connect(ui->actionAircraftPerformanceEdit, &QAction::triggered, this, &AircraftPerfController::edit);

  connect(ui->actionAircraftPerformanceLoadMerge, &QAction::triggered, this, &AircraftPerfController::loadAndMerge);
  connect(ui->actionAircraftPerformanceMerge, &QAction::triggered, this, &AircraftPerfController::mergeCollected);
  connect(ui->actionAircraftPerformanceRestart, &QAction::triggered, this, &AircraftPerfController::restartCollect);

  connect(ui->pushButtonAircraftPerformanceNew, &QPushButton::clicked, this, &AircraftPerfController::create);
  connect(ui->pushButtonAircraftPerformanceEdit, &QPushButton::clicked, this, &AircraftPerfController::edit);
  connect(ui->pushButtonAircraftPerformanceLoad, &QPushButton::clicked, this, &AircraftPerfController::load);
  connect(ui->pushButtonAircraftPerformanceSave, &QPushButton::clicked, this, &AircraftPerfController::save);
  connect(ui->pushButtonAircraftPerformanceSaveAs, &QPushButton::clicked, this, &AircraftPerfController::saveAs);

  connect(ui->pushButtonAircraftPerfCollectMerge, &QPushButton::clicked, this, &AircraftPerfController::mergeCollected);
  connect(ui->pushButtonAircraftPerfCollectRestart, &QPushButton::clicked, this,
          &AircraftPerfController::restartCollect);

  connect(ui->pushButtonAircraftPerformanceHelp, &QPushButton::clicked, this, &AircraftPerfController::helpClickedPerf);
  connect(ui->pushButtonAircraftPerfCollectHelp, &QPushButton::clicked, this,
          &AircraftPerfController::helpClickedPerfCollect);

  connect(ui->spinBoxAircraftPerformanceWindDirection, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
  connect(ui->spinBoxAircraftPerformanceWindSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
}

void AircraftPerfController::updateActionStates()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Update tab title to indicate change ========================================
  if(changed)
  {
    if(!ui->tabWidgetRoute->tabText(rc::AIRCRAFT).endsWith(tr(" *")))
      ui->tabWidgetRoute->setTabText(rc::AIRCRAFT, ui->tabWidgetRoute->tabText(rc::AIRCRAFT) + tr(" *"));
  }
  else
    ui->tabWidgetRoute->setTabText(rc::AIRCRAFT,
                                   ui->tabWidgetRoute->tabText(rc::AIRCRAFT).replace(tr(" *"), QString()));

  bool routeValid = NavApp::getRouteConst().getSizeWithoutAlternates() >= 2;

  ui->actionAircraftPerformanceMerge->setEnabled(perfHandler->isFinished());
  ui->pushButtonAircraftPerfCollectMerge->setEnabled(perfHandler->isFinished());

  ui->actionAircraftPerformanceRestart->setEnabled(perfHandler->hasFlightSegment());
  ui->pushButtonAircraftPerfCollectRestart->setEnabled(perfHandler->hasFlightSegment());

  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(routeValid);
  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(routeValid);

  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(ui->checkBoxAircraftPerformanceWindMan->isChecked());
  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(ui->checkBoxAircraftPerformanceWindMan->isChecked());
}

void AircraftPerfController::updateReport()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetRoute->currentIndex() == rc::AIRCRAFT && ui->textBrowserAircraftPerformanceReport->isVisible())
  {
    // Write HTML report ================================================================
    HtmlBuilder html(true /* background color */);

    // Icon, name and aircraft type =======================================================
    html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(),
             QSize(symbolSize, symbolSize));
    html.nbsp().nbsp();

    QStringList header({perf->getName(), perf->getAircraftType()});
    header.removeAll(QString());
    html.text(header.join(tr(" - ")), atools::util::html::BOLD | atools::util::html::BIG);

    // Display fuel estimates ==========================================================
    const RouteAltitude& altitudeLegs = NavApp::getAltitudeLegs();
    if(altitudeLegs.hasUnflyableLegs())
      html.p().error(tr("Flight plan has unflyable legs where head wind is larger than cruise speed.")).pEnd();
    else
    {
      if(altitudeLegs.size() > 1)
        fuelReport(html);
      else
        html.p().b(tr("No Flight Plan loaded.")).pEnd();
    }

    // Description and file =======================================================
    if(!perf->getDescription().isEmpty() || !currentFilepath.isEmpty())
      html.hr();

    if(!perf->getDescription().isEmpty())
    {
      html.p().b(tr("Performance File Description")).pEnd();
      html.table();
      html.row2(QString(), perf->getDescription(), atools::util::html::AUTOLINK);
      html.tableEnd();
    }

    // File path  =======================================================
    fuelReportFilepath(html, false /* print */);

    atools::gui::util::updateTextEdit(ui->textBrowserAircraftPerformanceReport, html.getHtml(),
                                      false /*scrollToTop*/, true /* keep selection */);
  }
}

void AircraftPerfController::updateReportCurrent()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(ui->tabWidgetRoute->currentIndex() == rc::COLLECTION && ui->textBrowserAircraftPerformanceCurrent->isVisible())
  {
    // Write HTML report ================================================================
    HtmlBuilder html(true /* background color */);

    const atools::fs::perf::AircraftPerf& curPerf = perfHandler->getAircraftPerformance();
    atools::fs::perf::FlightSegment segment = perfHandler->getCurrentFlightSegment();

    // Icon, name and aircraft type =======================================================
    html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(),
             QSize(symbolSize, symbolSize));
    html.nbsp().nbsp();

    QStringList header({curPerf.getName(), curPerf.getAircraftType()});
    header.removeAll(QString());
    html.text(header.join(tr(" - ")), atools::util::html::BOLD | atools::util::html::BIG);

    atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;

    // Display performance collection progress ==========================================================
    html.p().b(tr("Current flight segment: %1.%2").
               arg(perfHandler->getCurrentFlightSegmentString()).
               arg(perfHandler->isFinished() ? tr(" Finished.") : QString())).pEnd();

    if(segment >= atools::fs::perf::DEPARTURE_TAXI)
    {
      html.p().b(tr("Fuel")).pEnd();
      html.table();
      html.row2(tr("Fuel Type:"), curPerf.isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
      html.row2(tr("Total Fuel Consumed:"), fuelWeightVolLocal(perfHandler->getTotalFuelConsumed()), flags);
      html.row2(tr("Taxi Fuel:"), fuelWeightVolLocal(curPerf.getTaxiFuel()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CLIMB)
    {
      html.p().b(tr("Average Performance")).br().b(tr("Climb")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerf.getClimbSpeed()), flags);
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(curPerf.getClimbVertSpeed()) + tr(" <b>▲</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ffWeightVolLocal(curPerf.getClimbFuelFlow()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CRUISE)
    {
      html.p().b(tr("Cruise")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerf.getCruiseSpeed()), flags);
      html.row2(tr("Fuel Flow:"), ffWeightVolLocal(curPerf.getCruiseFuelFlow()), flags);
      html.tableEnd();
    }
    if(segment >= atools::fs::perf::DESCENT)
    {
      html.p().b(tr("Descent")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerf.getDescentSpeed()), flags);
      // Descent speed is always positive
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(-curPerf.getDescentVertSpeed()) + tr(" <b>▼</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ffWeightVolLocal(curPerf.getDescentFuelFlow()), flags);
      html.tableEnd();
    }

    atools::gui::util::updateTextEdit(ui->textBrowserAircraftPerformanceCurrent, html.getHtml(),
                                      false /*scrollToTop*/, true /* keep selection */);
  }
}

void AircraftPerfController::fuelReportFilepath(atools::util::HtmlBuilder& html, bool print)
{
  if(!currentFilepath.isEmpty())
  {
    if(!print)
    {
      html.p().b(tr("Performance File")).pEnd();
      html.table();

      // Show link inactive if file does not exist
      HtmlBuilder link(html.cleared());
      if(QFileInfo::exists(currentFilepath))
        link.a(currentFilepath, QString("lnm://show?filepath=%1").arg(currentFilepath), atools::util::html::LINK_NO_UL);
      else
        link.text(currentFilepath);

      html.row2(QString(), link.getHtml(), atools::util::html::NO_ENTITIES | atools::util::html::SMALL);
      html.tableEnd();
    }
    else
      // Use a simple layout for printing
      html.p().b(tr("Performance File:")).nbsp().nbsp().small(currentFilepath).pEnd();
  }
}

bool AircraftPerfController::isPerformanceFile(const QString& file)
{
  QStringList lines = atools::probeFile(file, 30);
  return lines.contains("[options]") && lines.contains("[perf]");
}

void AircraftPerfController::fuelReport(atools::util::HtmlBuilder& html, bool print)
{
  if(print)
    // Include header here if printing
    html.h2(tr("Aircraft Performance %1 - %2").arg(perf->getName()).arg(perf->getAircraftType()),
            atools::util::html::BOLD | atools::util::html::BIG);

  const RouteAltitude& altitudeLegs = NavApp::getAltitudeLegs();

  if(!print)
  {
    QStringList errs;
    if(perf->getUsableFuel() < 0.1f)
      errs.append(tr("usable fuel"));
    if(perf->getReserveFuel() < 0.1f)
      errs.append(tr("reserve fuel"));
    if(perf->getClimbFuelFlow() < 0.1f)
      errs.append(tr("climb fuel flow"));
    if(perf->getCruiseFuelFlow() < 0.1f)
      errs.append(tr("cruise fuel flow"));
    if(perf->getDescentFuelFlow() < 0.1f)
      errs.append(tr("descent fuel flow"));
    if(perf->getAlternateFuelFlow() < 0.1f)
      errs.append(tr("alternate fuel flow"));

    if(!errs.isEmpty())
      html.p().error((errs.size() == 1 ? tr("Invalid value for %1.") : tr("Invalid values for %1.")).
                     arg(errs.join(tr(", ")))).pEnd();

    if(perf->getUsableFuel() > 1.f)
      if(altitudeLegs.getBlockFuel(*perf) > perf->getUsableFuel())
        html.p().error(tr("Block fuel exceeds usable of %1.").arg(fuelWeightVolLocal(perf->getUsableFuel()))).pEnd();

    if(perf->getUsableFuel() > 1.f && perf->getReserveFuel() > perf->getUsableFuel())
      html.p().error(tr("Reserve fuel bigger than usable.")).pEnd();

    if(perf->getAircraftType().isEmpty())
      html.p().warning(tr("Aircraft type is not set.")).pEnd();
    else
    {
      QString model = NavApp::getUserAircraft().getAirplaneModel();
      if(!model.isEmpty() && perf->getAircraftType() != model)
        html.p().warning(tr("Airplane model does not match:\nSimulator %1 ≠ Performance File %2.").
                         arg(model).arg(perf->getAircraftType())).pEnd();
    }
  }

  // Flight data =======================================================
  atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;
  html.p().b(tr("Flight Plan")).pEnd();
  html.table();

  if(altitudeLegs.getTravelTimeHours() > 0.f)
    html.row2(tr("Distance and Time:"), tr("%1, %2").
              arg(Unit::distNm(altitudeLegs.getTotalDistance())).
              arg(formatter::formatMinutesHoursLong(altitudeLegs.getTravelTimeHours())),
              atools::util::html::BOLD | flags);
  else
    html.row2(tr("Distance:"), tr("%1").
              arg(Unit::distNm(altitudeLegs.getTotalDistance())),
              atools::util::html::BOLD | flags);

  html.row2(tr("Average Ground Speed:"), Unit::speedKts(altitudeLegs.getAverageGroundSpeed()), flags);
  html.row2(tr("True Airspeed at Cruise:"), Unit::speedKts(perf->getCruiseSpeed()), flags);

  float mach = atools::geo::tasToMachFromAlt(altitudeLegs.getCruiseAltitide(),
                                             static_cast<float>(perf->getCruiseSpeed()));
  if(mach > 0.4f)
    html.row2(tr("Mach at cruise:"), QLocale().toString(mach, 'f', 2), flags);

  // Wind =======================================================
  QStringList windText;

  if(!isWindManual())
  {
    if(NavApp::getWindReporter()->hasWindData())
    {
      if(std::abs(altitudeLegs.getWindSpeed()) >= 1.f)
      {
        windText.append(tr("%1°T, %2").
                        arg(altitudeLegs.getWindDirection(), 0, 'f', 0).
                        arg(Unit::speedKts(altitudeLegs.getWindSpeed())));
      }
    }
    else
      windText.append(tr("Wind source disabled"));
  }

  if(isWindManual() || NavApp::getWindReporter()->hasWindData())
  {
    // Headwind =======================
    float headWind = altitudeLegs.getHeadWind();
    if(std::abs(headWind) >= 1.f)
    {
      QString windPtr;
      if(headWind >= 1.f)
        windPtr = tr("◄");
      else if(headWind <= -1.f)
        windPtr = tr("►");
      windText.append(tr("%1 %2").arg(windPtr).arg(Unit::speedKts(std::abs(headWind))));
    }
  }

  html.row2(tr("Average %1Wind:").arg(isWindManual() ? tr("Manual ") : QString()),
            (windText.isEmpty() ? tr("None") : windText.join(tr(", "))), flags);

  html.tableEnd();

  // Fuel data =======================================================
  html.p().b(tr("Fuel Plan")).pEnd();
  html.table();
  html.row2(tr("Fuel Type:"), perf->isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
  html.row2(tr("Trip Fuel:"), tr("<b>%1</b> (%2)").
            arg(fuelWeightVolLocal(altitudeLegs.getTripFuel())).
            arg(fuelWeightVolOther(altitudeLegs.getTripFuel())), atools::util::html::NO_ENTITIES | flags);
  float blockFuel = altitudeLegs.getBlockFuel(*perf);
  QString percent = perf->getUsableFuel() >
                    1.f ? tr("<br/>%1 % of usable Fuel").arg(100.f / perf->getUsableFuel() * blockFuel, 0, 'f',
                                                             0) : QString();
  html.row2(tr("Block Fuel:"), tr("<b>%1</b> (%2)%3").
            arg(fuelWeightVolLocal(blockFuel)).
            arg(fuelWeightVolOther(blockFuel)).
            arg(percent),
            atools::util::html::NO_ENTITIES | flags);
  html.row2(tr("Fuel at Destination:"), fuelWeightVolLocal(altitudeLegs.getDestinationFuel(*perf)), flags);

  if(altitudeLegs.getAlternateFuel() > 0.f)
    html.row2(tr("Alternate Fuel:"), fuelWeightVolLocal(altitudeLegs.getAlternateFuel()), flags);

  html.row2(tr("Reserve Fuel:"), fuelWeightVolLocal(perf->getReserveFuel()), flags);

  if(perf->getTaxiFuel() > 0.f)
    html.row2(tr("Taxi Fuel:"), fuelWeightVolLocal(perf->getTaxiFuel()), flags);

  if(perf->getExtraFuel() > 0.f)
    html.row2(tr("Extra Fuel:"), fuelWeightVolLocal(perf->getExtraFuel()), flags);

  if(perf->getContingencyFuel() > 0.f)
    html.row2(tr("Contingency Fuel:"), tr("%1 %, %2").
              arg(perf->getContingencyFuel(), 0, 'f', 0).
              arg(fuelWeightVolLocal(altitudeLegs.getContingencyFuel(*perf))), flags);
  html.tableEnd();

  // Climb and descent phases =======================================================
  html.p().b(tr("Climb and Descent")).pEnd();
  html.table();
  if(perf->isClimbValid())
  {
    html.row2(tr("Climb:"), tr("%1 at %2, %3° Flight Path Angle").
              arg(Unit::speedVertFpm(perf->getClimbVertSpeed())).
              arg(Unit::speedKts(perf->getClimbSpeed())).
              arg(QLocale().toString(perf->getClimbFlightPathAngle(), 'f', 1)));

    if(altitudeLegs.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
      html.row2(tr("Time to Climb:"),
                formatter::formatMinutesHoursLong(altitudeLegs.getTopOfClimbDistance() /
                                                  perf->getClimbSpeed()));
  }
  else
    html.row2(tr("Climb not valid"));

  if(perf->isDescentValid())
  {
    html.row2(tr("Descent:"), tr("%1 at %2, %3° Flight Path Angle").
              arg(Unit::speedVertFpm(perf->getDescentVertSpeed())).
              arg(Unit::speedKts(perf->getDescentSpeed())).
              arg(QLocale().toString(-perf->getDescentFlightPathAngle(), 'f', 1)));
    html.row2(tr("Descent Rule of Thumb:"), tr("%1 per %2 %3").
              arg(Unit::distNm(1.f / perf->getDescentRateFtPerNm() * Unit::rev(1000.f, Unit::altFeetF))).
              arg(QLocale().toString(1000.f, 'f', 0)).
              arg(Unit::getUnitAltStr()));
  }
  else
    html.row2(tr("Descent not valid"));
  html.tableEnd();

  if(print && !perf->getDescription().isEmpty())
  {
    html.p().b(tr("Performance File Description")).pEnd();
    html.table().row2(QString(), perf->getDescription()).tableEnd();
  }
}

QString fuelWeightVol(atools::fs::perf::AircraftPerf *perf, opts::UnitFuelAndWeight unitFuelAndWeight,
                      float valueLbsGal)
{
  static const QString STR("%1 %2 %3 %4");

  // Source value is always lbs or gallon depending on setting in perf
  // convert according to unitFuelAndWeight or pass through
  using namespace atools::geo;
  switch(unitFuelAndWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(perf->useFuelAsVolume())
        // Pass volume through and convert volume to weight
        return STR.
               arg(fromGalToLbs(perf->isJetFuel(), valueLbsGal), 0, 'f', 0).
               arg(Unit::getSuffixFuelWeightLbs()).
               arg(valueLbsGal, 0, 'f', 0).
               arg(Unit::getSuffixFuelVolGal());
      else
        // Pass weight through and convert weight to volume
        return STR.
               arg(valueLbsGal, 0, 'f', 0).
               arg(Unit::getSuffixFuelWeightLbs()).
               arg(fromLbsToGal(perf->isJetFuel(), valueLbsGal), 0, 'f', 0).
               arg(Unit::getSuffixFuelVolGal());

    case opts::FUEL_WEIGHT_LITER_KG:
      if(perf->useFuelAsVolume())
        // Convert to metric and pass volume  through and convert volume to weight
        return STR.
               arg(fromLiterToKg(perf->isJetFuel(), lbsToKg(valueLbsGal)), 0, 'f', 0).
               arg(Unit::getSuffixFuelWeightKg()).
               arg(gallonToLiter(valueLbsGal), 0, 'f', 0).
               arg(Unit::getSuffixFuelVolLiter());
      else
        // Convert to metric and  pass weight through and convert weight to volume
        return STR.
               arg(lbsToKg(valueLbsGal), 0, 'f', 0).
               arg(Unit::getSuffixFuelWeightKg()).
               arg(fromLbsToGal(perf->isJetFuel(), gallonToLiter(valueLbsGal)), 0, 'f', 0).
               arg(Unit::getSuffixFuelVolLiter());
  }
  return QString();
}

QString AircraftPerfController::fuelWeightVolLocal(float valueLbsGal)
{
  // Convert to locally selected unit
  return fuelWeightVol(perf, OptionData::instance().getUnitFuelAndWeight(), valueLbsGal);
}

QString AircraftPerfController::fuelWeightVolOther(float valueLbsGal)
{
  // Convert to opposite of locally selected unit (lbs/gal vs. kg/l and vice versa)
  switch(OptionData::instance().getUnitFuelAndWeight())
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return fuelWeightVol(perf, opts::FUEL_WEIGHT_LITER_KG, valueLbsGal);

    case opts::FUEL_WEIGHT_LITER_KG:
      return fuelWeightVol(perf, opts::FUEL_WEIGHT_GAL_LBS, valueLbsGal);
  }
  return QString();
}

QString AircraftPerfController::ffWeightVolLocal(float valueLbsGal)
{
  if(perf->useFuelAsVolume())
    return Unit::ffLbsAndGal(atools::geo::fromGalToLbs(perf->isJetFuel(), valueLbsGal), valueLbsGal);
  else
    return Unit::ffLbsAndGal(valueLbsGal, atools::geo::fromLbsToGal(perf->isJetFuel(), valueLbsGal));
}

void AircraftPerfController::saveState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  fileHistory->saveState();
  settings.setValue(lnm::AIRCRAFT_PERF_FILENAME, currentFilepath);

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::AIRCRAFT_PERF_WIDGETS).save({ui->spinBoxAircraftPerformanceWindSpeed,
                                                             ui->spinBoxAircraftPerformanceWindDirection,
                                                             ui->checkBoxAircraftPerformanceWindMan});
}

void AircraftPerfController::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  fileHistory->restoreState();
  loadFile(settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME));

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRCRAFT_PERF_WIDGETS, true /* visibility */, true /* block signals */);
  state.restore({ui->spinBoxAircraftPerformanceWindSpeed,
                 ui->spinBoxAircraftPerformanceWindDirection,
                 ui->checkBoxAircraftPerformanceWindMan});

  perfHandler->setCruiseAltitude(NavApp::getAltitudeLegs().getCruiseAltitide());
  perfHandler->start();

  optionsChanged();
}

void AircraftPerfController::optionsChanged()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QFont f = ui->textBrowserAircraftPerformanceReport->font();
  float newSize = infoFontPtSize * OptionData::instance().getGuiPerfReportTextSize() / 100.f;
  if(newSize > 0.1f)
  {
    f.setPointSizeF(newSize);
    ui->textBrowserAircraftPerformanceReport->setFont(f);
    ui->textBrowserAircraftPerformanceCurrent->setFont(f);
  }

  symbolSize = ui->textBrowserAircraftPerformanceReport->fontMetrics().height() * 4 / 3;

  updateActionStates();
  updateReport();
  updateReportCurrent();
}

void AircraftPerfController::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  // Pass to handler for averaging
  perfHandler->simDataChanged(simulatorData);

  // Update report every second
  qint64 currentSampleTime = QDateTime::currentMSecsSinceEpoch();
  if(currentSampleTime > reportLastSampleTimeMs + 1000)
  {
    reportLastSampleTimeMs = currentSampleTime;
    updateReportCurrent();
  }
}

void AircraftPerfController::windBoxesChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Start delayed wind update - calls windChangedDelayed
  windChangeTimer.start(500);
}

void AircraftPerfController::windChangedDelayed()
{
  qDebug() << Q_FUNC_INFO;
  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::noPerfLoaded()
{
  // Invalidate performance file after an error - state "none loaded"
  perf->resetToDefault();
  changed = false;
  currentFilepath.clear();
}

void AircraftPerfController::anchorClicked(const QUrl& url)
{
  QUrlQuery query(url);

  if(url.scheme() == "lnm" && url.host() == "show" && query.hasQueryItem("filepath"))
    // Show path in any OS dependent file manager. Selects the file in Windows Explorer.
    atools::gui::showInFileManager(query.queryItemValue("filepath"), mainWindow);
  else
    atools::gui::anchorClicked(mainWindow, url);
}

void AircraftPerfController::manualWindToggled()
{
  updateActionStates();
  updateReport();
  updateReportCurrent();

  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::visibilityChanged()
{
  updateReport();
  updateReportCurrent();
}
