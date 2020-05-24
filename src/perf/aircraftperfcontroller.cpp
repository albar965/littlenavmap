/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "common/fueltool.h"
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
#include "gui/tabwidgethandler.h"

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
  connect(ui->dockWidgetRoute, &QDockWidget::visibilityChanged, this, &AircraftPerfController::tabVisibilityChanged);

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

  mainWindow->showAircraftPerformance();

  AircraftPerf editPerf;
  editPerf.resetToDefault();
  if(editInternal(editPerf, tr("Create")))
  {
    if(checkForChanges())
    {
      *perf = editPerf;
      currentFilepath.clear();
      changed = true;

      updateActionStates();
      NavApp::setStatusMessage(tr("Aircraft performance created."));
      emit aircraftPerformanceChanged(perf);
    }
  }
}

bool AircraftPerfController::editInternal(atools::fs::perf::AircraftPerf& editPerf, const QString& modeText)
{
  qDebug() << Q_FUNC_INFO;

  AircraftPerfDialog dialog(mainWindow, editPerf, modeText);
  if(dialog.exec() == QDialog::Accepted)
  {
    editPerf = dialog.getAircraftPerf();
    return true;
  }
  else
    return false;
}

void AircraftPerfController::edit()
{
  qDebug() << Q_FUNC_INFO;

  if(editInternal(*perf, tr("Edit")))
  {
    changed = true;
    windChangeTimer.stop();
    NavApp::setStatusMessage(tr("Aircraft performance changed."));
    updateActionStates();
    emit aircraftPerformanceChanged(perf);
  }
}

void AircraftPerfController::loadStr(const QString& string)
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    if(checkForChanges())
    {
      currentFilepath.clear();
      perf->loadXmlStr(string);
      changed = false;
      windChangeTimer.stop();
      mainWindow->showAircraftPerformance();
      NavApp::setStatusMessage(tr("Aircraft performance loaded."));
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
  }
  catch(...)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
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
        mainWindow->showAircraftPerformance();
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
      mainWindow->showAircraftPerformance();
      NavApp::setStatusMessage(tr("Aircraft performance merged."));
    }
  }
}

void AircraftPerfController::restartCollection(bool quiet)
{
  int result = QMessageBox::Yes;

  if(!quiet)
    result = atools::gui::Dialog(mainWindow).
             showQuestionMsgBox(lnm::ACTIONS_SHOW_RESET_PERF,
                                tr("Reset performance collection and lose all current values?"),
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

bool AircraftPerfController::calculateFuelAndTimeTo(float& fuelLbsToDest, float& fuelGalToDest,
                                                    float& fuelLbsToTod, float& fuelGalToTod,
                                                    float& timeToDest, float& timeToTod,
                                                    float distanceToDest, int activeLeg) const
{

  const atools::fs::sc::SimConnectUserAircraft& ac = NavApp::getUserAircraft();
  return NavApp::getAltitudeLegs().calculateFuelAndTimeTo(fuelLbsToDest, fuelGalToDest,
                                                          fuelLbsToTod, fuelGalToTod,
                                                          timeToDest, timeToTod,
                                                          distanceToDest, *perf,
                                                          ac.getFuelFlowPPH(), ac.getFuelFlowGPH(),
                                                          ac.getGroundSpeedKts(),
                                                          activeLeg);
}

void AircraftPerfController::mergeCollected()
{
  qDebug() << Q_FUNC_INFO;

  PerfMergeDialog dialog(mainWindow, perfHandler->getAircraftPerformanceLbs(), *perf, false /* show all */);
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
        mainWindow->showAircraftPerformance();
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
      perf->saveXml(currentFilepath);
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

bool AircraftPerfController::saveAsStr(const QString& string)
{
  qDebug() << Q_FUNC_INFO;
  bool retval = false;

  try
  {
    QString perfFile = saveAsFileDialog();
    if(!perfFile.isEmpty())
    {
      QFile file(perfFile);
      if(file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << string.toUtf8();
        file.close();
      }
      else
        atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("Cannot save file."));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    retval = false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    retval = false;
  }
  return retval;
}

QString AircraftPerfController::saveAsFileDialog() const
{
  return atools::gui::Dialog(mainWindow).saveFileDialog(
    tr("Save Aircraft Performance File"),
    tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
    "lnmperf", "AircraftPerformance/",
    QString(), currentFilepath.isEmpty() ? perf->getName() + ".lnmperf" : QFileInfo(currentFilepath).fileName(),
    false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);
}

QString AircraftPerfController::openFileDialog() const
{
  return atools::gui::Dialog(mainWindow).openFileDialog(
    tr("Open Aircraft Performance File"),
    tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
    "AircraftPerformance/");
}

bool AircraftPerfController::saveAs()
{
  qDebug() << Q_FUNC_INFO;
  bool retval = false;

  try
  {
    QString perfFile = saveAsFileDialog();
    if(!perfFile.isEmpty())
    {
      currentFilepath = perfFile;
      perf->saveXml(perfFile);
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

float AircraftPerfController::cruiseAlt()
{
  float alt = NavApp::getAltitudeLegs().getCruiseAltitide();

  if(atools::almostEqual(alt, 0.f) || alt > map::INVALID_ALTITUDE_VALUE / 2.f)
    alt = NavApp::getRouteCruiseAltFtWidget();

  return alt;
}

void AircraftPerfController::routeChanged(bool geometryChanged, bool newFlightplan)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(geometryChanged);
  Q_UNUSED(newFlightplan);
  perfHandler->setCruiseAltitude(cruiseAlt());
  updateReport();
  updateReportCurrent();
  updateActionStates();
}

void AircraftPerfController::updateReports()
{
  updateReport();
  updateReportCurrent();
}

void AircraftPerfController::routeAltitudeChanged(float altitudeFeet)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(altitudeFeet);

  perfHandler->setCruiseAltitude(cruiseAlt());
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
  connect(ui->actionAircraftPerformanceRestart, &QAction::triggered, this, &AircraftPerfController::restartCollection);

  connect(ui->pushButtonAircraftPerformanceNew, &QPushButton::clicked, this, &AircraftPerfController::create);
  connect(ui->pushButtonAircraftPerformanceEdit, &QPushButton::clicked, this, &AircraftPerfController::edit);
  connect(ui->pushButtonAircraftPerformanceLoad, &QPushButton::clicked, this, &AircraftPerfController::load);
  connect(ui->pushButtonAircraftPerformanceSave, &QPushButton::clicked, this, &AircraftPerfController::save);
  connect(ui->pushButtonAircraftPerformanceSaveAs, &QPushButton::clicked, this, &AircraftPerfController::saveAs);

  connect(ui->pushButtonAircraftPerfCollectMerge, &QPushButton::clicked, this, &AircraftPerfController::mergeCollected);
  connect(ui->pushButtonAircraftPerfCollectRestart, &QPushButton::clicked, this,
          &AircraftPerfController::restartCollection);

  connect(ui->pushButtonAircraftPerformanceHelp, &QPushButton::clicked, this, &AircraftPerfController::helpClickedPerf);
  connect(ui->pushButtonAircraftPerfCollectHelp, &QPushButton::clicked, this,
          &AircraftPerfController::helpClickedPerfCollect);

  connect(ui->spinBoxAircraftPerformanceWindDirection, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
  connect(ui->spinBoxAircraftPerformanceWindSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
}

void AircraftPerfController::updateTabTiltle()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  int idx = NavApp::getRouteTabHandler()->getIndexForId(rc::AIRCRAFT);
  if(idx != -1)
  {
    if(changed)
    {
      if(!ui->tabWidgetRoute->tabText(idx).endsWith(tr(" *")))
        ui->tabWidgetRoute->setTabText(idx, ui->tabWidgetRoute->tabText(idx) + tr(" *"));
    }
    else
      ui->tabWidgetRoute->setTabText(idx, ui->tabWidgetRoute->tabText(idx).replace(tr(" *"), QString()));
  }
}

void AircraftPerfController::updateActionStates()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Update tab title to indicate change ========================================
  updateTabTiltle();

  bool routeValid = NavApp::getRouteConst().getSizeWithoutAlternates() >= 2;

  ui->actionAircraftPerformanceRestart->setEnabled(perfHandler->hasFlightSegment());
  ui->pushButtonAircraftPerfCollectRestart->setEnabled(perfHandler->hasFlightSegment());

  bool manWind = ui->checkBoxAircraftPerformanceWindMan->isChecked();
  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(manWind && routeValid);
  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(manWind && routeValid);
  atools::gui::util::showHideLayoutElements({ui->horizontalLayoutManWind}, manWind, {});
}

void AircraftPerfController::updateReport()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(NavApp::getRouteTabHandler()->getCurrentTabId() == rc::AIRCRAFT && ui->dockWidgetRoute->isVisible())
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
      fuelReport(html);

    // Description and file =======================================================
    if(!perf->getDescription().isEmpty())
    {
      html.p().b(tr("Remarks")).pEnd();
      html.table(1, 2, 0, 100, html.getRowBackColor());
      html.tr().td(perf->getDescription(), atools::util::html::AUTOLINK).trEnd();
      html.tableEnd();
    }

    // File path  =======================================================
    fuelReportFilepath(html, false /* print */);

#ifdef DEBUG_INFORMATION
    html.hr().pre("Headwind: climb " + Unit::speedKts(altitudeLegs.getClimbHeadWind()) +
                  ", cruise " + Unit::speedKts(altitudeLegs.getCruiseHeadWind()) +
                  ", descent " + Unit::speedKts(altitudeLegs.getDescentHeadWind()) +
                  ", avg " + Unit::speedKts(altitudeLegs.getHeadWindAverage()) + "\n" +

                  "GS: climb " + Unit::speedKts(altitudeLegs.getClimbSpeedWindCorrected()) +
                  ", cruise " + Unit::speedKts(altitudeLegs.getCruiseSpeedWindCorrected()) +
                  ", descent " + Unit::speedKts(altitudeLegs.getDescentSpeedWindCorrected()) +
                  ", avg " + Unit::speedKts(altitudeLegs.getAverageGroundSpeed()) + "\n" +

                  "Fuel: climb " + QString::number(altitudeLegs.getClimbFuel(), 'f', 1) +
                  ", cruise " + QString::number(altitudeLegs.getCruiseFuel(), 'f', 1) +
                  ", descent " + QString::number(altitudeLegs.getDescentFuel(), 'f', 1) + "\n" +

                  "Dist: climb " + Unit::distNm(altitudeLegs.getTopOfClimbDistance()) +
                  ", cruise " + Unit::distNm(altitudeLegs.getCruiseDistance()) +
                  ", descent " + Unit::distNm(altitudeLegs.getTopOfDescentFromDestination()) + "\n" +

                  "Time: climb " + QString::number(altitudeLegs.getClimbTime(), 'f', 2) + " (" +
                  formatter::formatMinutesHours(altitudeLegs.getClimbTime()) + ")" +
                  ", cruise " + QString::number(altitudeLegs.getCruiseTime(), 'f', 2) + " (" +
                  formatter::formatMinutesHours(altitudeLegs.getCruiseTime()) + ")" +
                  ", descent " + QString::number(altitudeLegs.getDescentTime(), 'f', 2) + " (" +
                  formatter::formatMinutesHours(altitudeLegs.getDescentTime()) + ")" +
                  ", all " + QString::number(altitudeLegs.getTravelTimeHours(), 'f', 2) + " (" +
                  formatter::formatMinutesHours(altitudeLegs.getTravelTimeHours()) + ")");
#endif

    atools::gui::util::updateTextEdit(ui->textBrowserAircraftPerformanceReport, html.getHtml(),
                                      false /*scrollToTop*/, true /* keep selection */);
  }
}

void AircraftPerfController::updateReportCurrent()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(NavApp::getRouteTabHandler()->getCurrentTabId() == rc::COLLECTION && ui->dockWidgetRoute->isVisible())
  {
    // Write HTML report ================================================================
    HtmlBuilder html(true /* background color */);

    const atools::fs::perf::AircraftPerf& curPerfLbs = perfHandler->getAircraftPerformanceLbs();
    atools::fs::perf::FlightSegment segment = perfHandler->getCurrentFlightSegment();

    // Icon, name and aircraft type =======================================================
    html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(),
             QSize(symbolSize, symbolSize));
    html.nbsp().nbsp();

    QStringList header({curPerfLbs.getName(), curPerfLbs.getAircraftType()});
    header.removeAll(QString());

    if(!header.isEmpty())
      html.text(header.join(tr(" - ")), atools::util::html::BOLD | atools::util::html::BIG);
    else
      html.text(tr("Unknown Aircraft"), atools::util::html::BOLD | atools::util::html::BIG);

    // if(NavApp::getRouteConst().size() < 2)
    // html.p().warning(tr("No valid flight plan.")).pEnd();

    atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;
    FuelTool ft(curPerfLbs);

    // Display performance collection progress ==========================================================
    if(segment != atools::fs::perf::NONE)
    {
      html.p().b(tr("Aircraft")).pEnd();
      html.table();
      html.row2(tr("Current flight segment: "), perfHandler->getCurrentFlightSegmentString() +
                (perfHandler->isFinished() ? tr(", <b>Finished.</b>") : QString()), atools::util::html::NO_ENTITIES);
      html.row2If(tr("Aircraft status: "), perfHandler->getAircraftStatusTexts().join(tr(", ")));
      html.tableEnd();
      html.pEnd();
    }
    else
      html.p().b(tr("No flight detected.")).pEnd();

    if(segment >= atools::fs::perf::DEPARTURE_TAXI)
    {
      html.p().b(tr("Fuel")).pEnd();
      html.table();
      html.row2(tr("Fuel Type:"), curPerfLbs.isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
      html.row2(tr("Total Fuel Consumed:"), ft.weightVolLocal(perfHandler->getTotalFuelConsumedLbs()), flags);
      html.row2(tr("Taxi Fuel:"), ft.weightVolLocal(curPerfLbs.getTaxiFuel()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CLIMB)
    {
      html.p().b(tr("Average Performance")).br().b(tr("Climb")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerfLbs.getClimbSpeed()), flags);
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(curPerfLbs.getClimbVertSpeed()) + tr(" <b>▲</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ft.flowWeightVolLocal(curPerfLbs.getClimbFuelFlow()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CRUISE)
    {
      html.p().b(tr("Cruise")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerfLbs.getCruiseSpeed()), flags);
      html.row2(tr("Fuel Flow:"), ft.flowWeightVolLocal(curPerfLbs.getCruiseFuelFlow()), flags);
      html.tableEnd();
    }
    if(segment >= atools::fs::perf::DESCENT)
    {
      html.p().b(tr("Descent")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(curPerfLbs.getDescentSpeed()), flags);
      // Descent speed is always positive
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(-curPerfLbs.getDescentVertSpeed()) + tr(" <b>▼</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ft.flowWeightVolLocal(curPerfLbs.getDescentFuelFlow()), flags);
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
  return atools::fs::perf::AircraftPerf::detectFormat(file) != atools::fs::perf::FORMAT_NONE;
}

float AircraftPerfController::getFuelReserveAtDestinationLbs() const
{
  return perf->getReserveFuelLbs() +
         (perf->useFuelAsVolume() ?
          atools::geo::fromGalToLbs(perf->isJetFuel(), NavApp::getAltitudeLegs().getAlternateFuel()) :
          NavApp::getAltitudeLegs().getAlternateFuel());
}

float AircraftPerfController::getFuelReserveAtDestinationGal() const
{
  return perf->getReserveFuelGal() +
         (perf->useFuelAsVolume() ?
          NavApp::getAltitudeLegs().getAlternateFuel() :
          atools::geo::fromLbsToGal(perf->isJetFuel(), NavApp::getAltitudeLegs().getAlternateFuel()));
}

void AircraftPerfController::fuelReportRunway(atools::util::HtmlBuilder& html)
{
  QStringList runwayTxt;
  if(perf->getRunwayType() != atools::fs::perf::SOFT || perf->getMinRunwayLength() > 0.f)
  {
    switch(perf->getRunwayType())
    {
      case atools::fs::perf::SOFT:
        runwayTxt.append(tr("Hard and soft surface"));
        break;
      case atools::fs::perf::HARD:
        runwayTxt.append(tr("Hard surface"));
        break;
      case atools::fs::perf::WATER:
        runwayTxt.append(tr("Water"));
        break;
      case atools::fs::perf::WATER_LAND:
        runwayTxt.append(tr("Amphibian"));
        break;
    }

    if(perf->getMinRunwayLength() > 0.f)
      runwayTxt.append(Unit::distShortFeet(perf->getMinRunwayLength()));

    html.row2(tr("Minimum runway:"), runwayTxt.join(tr(", ")), atools::util::html::ALIGN_RIGHT);
  }
}

void AircraftPerfController::fuelReport(atools::util::HtmlBuilder& html, bool print)
{
  const RouteAltitude& altLegs = NavApp::getAltitudeLegs();

  bool hasLegs = altLegs.size() > 1;

  if(print)
    // Include header here if printing
    html.h2(tr("Aircraft Performance %1 - %2").arg(perf->getName()).arg(perf->getAircraftType()),
            atools::util::html::BOLD | atools::util::html::BIG);

  FuelTool ft(perf);

  // Warnings and errors =================================================
  if(!print)
  {
    if(NavApp::getRoute().size() == 1)
      html.p().b(tr("Flight Plan not valid.")).pEnd();
    else if(!hasLegs)
      html.p().b(tr("No Flight Plan loaded.")).pEnd();

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

    if(hasLegs)
    {
      if(perf->getUsableFuel() > 1.f)
        if(altLegs.getBlockFuel(*perf) > perf->getUsableFuel())
          html.p().error(tr("Block fuel exceeds usable of %1.").arg(ft.weightVolLocal(perf->getUsableFuel()))).pEnd();

    }

    if(perf->getUsableFuel() > 1.f && perf->getReserveFuel() > perf->getUsableFuel())
      html.p().error(tr("Reserve fuel bigger than usable.")).pEnd();

    if(perf->getAircraftType().isEmpty())
      html.p().warning(tr("Aircraft type is not set.")).pEnd();
    else
    {
      QString model = NavApp::getUserAircraft().getAirplaneModel();
      if(!model.isEmpty() && perf->getAircraftType() != model)
        html.p().
        warning(tr("Airplane model does not match:")).br().
        warning(tr("Simulator \"%1\" ≠ Performance File \"%2\".").arg(model).arg(perf->getAircraftType())).
        pEnd();
    }
  } // if(!print)

  // Aircraft data =======================================================
  atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;
  html.p().b(tr("Aircraft")).pEnd();

  QString text = tr("Estimated range with reserve:");
  html.table();
  html.row2(tr("Fuel Type:"), perf->isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
  if(perf->getUsableFuel() > 1.f)
  {
    html.row2(tr("Usable Fuel:"), ft.weightVolLocal(perf->getUsableFuel()), flags);

    if(perf->getCruiseSpeed() > 1.f)
    {
      if(perf->getCruiseFuelFlow() > 1.f)
      {
        float enduranceHours = (perf->getUsableFuel() - perf->getReserveFuel()) / perf->getCruiseFuelFlow();
        html.row2(text, tr("%1, %2").
                  arg(Unit::distNm(enduranceHours * perf->getCruiseSpeed())).
                  arg(formatter::formatMinutesHoursLong(enduranceHours)), flags);
      }
      else
        html.row2Warning(text, tr("Cruise fuel flow not set"));
    }
    else
      html.row2Warning(text, tr("Cruise speed not set"));
  }
  else
    html.row2Warning(text, tr("Usable fuel not set"));

  if(hasLegs)
  {
    fuelReportRunway(html);
    html.tableEnd();

    // Flight plan =================================================
    html.p().b(tr("Flight Plan")).pEnd();
    html.table();

    if(altLegs.getTravelTimeHours() > 0.f)
      html.row2(tr("Distance and Time:"), tr("%1, %2").
                arg(Unit::distNm(altLegs.getTotalDistance())).
                arg(formatter::formatMinutesHoursLong(altLegs.getTravelTimeHours())),
                atools::util::html::BOLD | flags);
    else
      html.row2(tr("Distance:"), tr("%1").
                arg(Unit::distNm(altLegs.getTotalDistance())),
                atools::util::html::BOLD | flags);

    html.row2(tr("Average Ground Speed:"), Unit::speedKts(altLegs.getAverageGroundSpeed()), flags);
    html.row2(tr("True Airspeed at Cruise:"), Unit::speedKts(perf->getCruiseSpeed()), flags);

    float mach = atools::geo::tasToMachFromAlt(altLegs.getCruiseAltitide(),
                                               static_cast<float>(perf->getCruiseSpeed()));
    if(mach > 0.4f)
      html.row2(tr("Mach at cruise:"), QLocale().toString(mach, 'f', 2), flags);

    // Wind =======================================================
    QStringList windText;

    WindReporter *windReporter = NavApp::getWindReporter();

    if(!isWindManual() && windReporter->hasWindData() && std::abs(altLegs.getWindSpeedAverage()) >= 1.f)
    {
      // Display direction and speed if wind is not manually selected and available ====================
      windText.append(tr("%1°T, %2").
                      arg(altLegs.getWindDirection(), 0, 'f', 0).
                      arg(Unit::speedKts(altLegs.getWindSpeedAverage())));
    }

    QString windType;
    if(isWindManual() || windReporter->hasWindData())
    {
      // Display manual wind - only head- or tailwind =======================
      float headWind = altLegs.getHeadWindAverage();
      if(std::abs(headWind) >= 1.f)
      {
        QString windPtr;
        if(headWind >= 1.f)
        {
          windPtr = tr("◄");
          windType = tr("headwind");
        }
        else if(headWind <= -1.f)
        {
          windPtr = tr("►");
          windType = tr("tailwind");
        }
        windText.append(tr("%1 %2 %3").arg(windPtr).arg(Unit::speedKts(std::abs(headWind))).arg(windType));
      }
    }

    QString head = tr("Average wind (%1):");
    if(!windText.isEmpty())
      html.row2(head.arg(windReporter->getSourceText()), windText.join(tr("\n")), flags);
    else
      html.row2(head.arg(windReporter->getSourceText()),
                windReporter->isWindManual() ? tr("No head- or tailwind") : tr("No wind"), flags);

    // Fuel Plan =======================================================
    html.tableEnd();

    html.p().b(tr("Fuel Plan")).pEnd();
    html.table();

    html.row2(tr("Trip Fuel:"), ft.weightVolLocalOther(altLegs.getTripFuel(), true /* bold */),
              atools::util::html::NO_ENTITIES | flags);
    float blockFuel = altLegs.getBlockFuel(*perf);
    QString percent = perf->getUsableFuel() >
                      1.f ? tr("<br/>%1 % of usable Fuel").arg(100.f / perf->getUsableFuel() * blockFuel, 0, 'f',
                                                               0) : QString();
    html.row2(tr("Block Fuel:"), tr("%1%2").
              arg(ft.weightVolLocalOther(blockFuel, true /* bold */)).
              arg(percent), atools::util::html::NO_ENTITIES | flags);
    html.row2(tr("Fuel at Destination:"), ft.weightVolLocal(altLegs.getDestinationFuel(*perf)), flags);

    if(altLegs.getAlternateFuel() > 0.f)
      html.row2(tr("Alternate Fuel:"), ft.weightVolLocal(altLegs.getAlternateFuel()), flags);
  } // if(hasLegs)

  // Fuel information printed always =====================================================
  html.row2(tr("Reserve Fuel:"), ft.weightVolLocal(perf->getReserveFuel()), flags);

  if(perf->getTaxiFuel() > 0.f)
    html.row2(tr("Taxi Fuel:"), ft.weightVolLocal(perf->getTaxiFuel()), flags);

  if(perf->getExtraFuel() > 0.f)
    html.row2(tr("Extra Fuel:"), ft.weightVolLocal(perf->getExtraFuel()), flags);

  if(perf->getContingencyFuel() > 0.f)
  {
    if(hasLegs)
      html.row2(tr("Contingency Fuel:"), tr("%1 %, %2").
                arg(perf->getContingencyFuel(), 0, 'f', 0).
                arg(ft.weightVolLocal(altLegs.getContingencyFuel(*perf))), flags);
    else
      html.row2(tr("Contingency Fuel:"), tr("%1 %").arg(perf->getContingencyFuel(), 0, 'f', 0), flags);
  }

  if(!hasLegs)
    fuelReportRunway(html);

  html.tableEnd();

  // Climb and descent phases =======================================================
  if(hasLegs)
  {
    html.p().b(tr("Climb and Descent")).pEnd();
    html.table();
    if(perf->isClimbValid())
    {
      html.row2(tr("Climb:"), tr("%1 at %2, %3° Flight Path Angle").
                arg(Unit::speedVertFpm(perf->getClimbVertSpeed())).
                arg(Unit::speedKts(perf->getClimbSpeed())).
                arg(QLocale().toString(perf->getClimbFlightPathAngle(altLegs.getClimbHeadWind()), 'f', 1)));

      if(altLegs.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
        html.row2(tr("Time to Climb:"),
                  formatter::formatMinutesHoursLong(NavApp::getAltitudeLegs().getClimbTime()));
    }
    else
      html.row2(tr("Climb not valid"));

    if(perf->isDescentValid())
    {
      float wind = altLegs.getDescentHeadWind();
      html.row2(tr("Descent:"), tr("%1 at %2, %3° Flight Path Angle").
                arg(Unit::speedVertFpm(perf->getDescentVertSpeed())).
                arg(Unit::speedKts(perf->getDescentSpeed())).
                arg(QLocale().toString(-perf->getDescentFlightPathAngle(wind), 'f', 1)));
      html.row2(tr("Descent Rule of Thumb:"), tr("%1 per %2 %3").
                arg(Unit::distNm(1.f / perf->getDescentRateFtPerNm(wind) * Unit::rev(1000.f, Unit::altFeetF))).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr()));
    }
    else
      html.row2(tr("Descent not valid"));
    html.tableEnd();
  } // if(hasLegs)

  if(print && !perf->getDescription().isEmpty())
  {
    html.p().b(tr("Remarks")).pEnd();
    html.table(1).row2(QString(), perf->getDescription()).tableEnd();
  }
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

  // Need to initialize this late since route controller is not valid when AircraftPerfController constructor is called
  connect(NavApp::getRouteTabHandler(), &atools::gui::TabWidgetHandler::tabChanged,
          this, &AircraftPerfController::tabVisibilityChanged);
  connect(NavApp::getRouteTabHandler(), &atools::gui::TabWidgetHandler::tabOpened,
          this, &AircraftPerfController::updateTabTiltle);

  fileHistory->restoreState();
  loadFile(settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME));

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRCRAFT_PERF_WIDGETS, true /* visibility */, true /* block signals */);
  state.restore({ui->spinBoxAircraftPerformanceWindSpeed,
                 ui->spinBoxAircraftPerformanceWindDirection,
                 ui->checkBoxAircraftPerformanceWindMan});

  perfHandler->setCruiseAltitude(cruiseAlt());
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

void AircraftPerfController::tabVisibilityChanged()
{
  qDebug() << Q_FUNC_INFO;
  updateReport();
  updateReportCurrent();
}
