/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel albar965@mailbox.org
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
#include "atools.h"
#include "geo/calculations.h"
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
#include "route/routealtitude.h"
#include "gui/widgetstate.h"
#include "fs/perf/aircraftperfhandler.h"

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

  fileHistory = new atools::gui::FileHistoryHandler(this, lnm::AIRCRAFT_PERF_FILENAMESRECENT,
                                                    ui->menuAircraftPerformanceRecent,
                                                    ui->actionAircraftPerformanceClearMenu);

  connect(fileHistory, &atools::gui::FileHistoryHandler::fileSelected,

          this, &AircraftPerfController::loadFile);
  connect(ui->textBrowserAircraftPerformanceReport, &QTextBrowser::anchorClicked,
          this, &AircraftPerfController::anchorClicked);

  // Pass wind change with a delay to avoid lagging mouse wheel
  connect(&windChangeTimer, &QTimer::timeout, this, &AircraftPerfController::windChangedDelayed);
  windChangeTimer.setSingleShot(true);
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
    QString(), perf != nullptr ? perf->getName() : QString());

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

void AircraftPerfController::collectToggled()
{
  qDebug() << Q_FUNC_INFO;

  Ui::MainWindow *ui = NavApp::getMainUi();

  if(ui->actionAircraftPerformanceCollect->isChecked())
    startCollecting();
  else
    stopCollecting();

  updateActionStates();
  updateReport();
}

void AircraftPerfController::startCollecting()
{
  if(collectPerformanceDialog())
  {
    perfHandler = new AircraftPerfHandler(this);
    perfHandler->setCruiseAltitude(NavApp::getAltitudeLegs().getCruiseAltitide());

    // Null all values to allow average calculation
    perf->setNull();

    perfHandler->setAircraftPerformance(perf);
    changed = true;
    connect(perfHandler, &AircraftPerfHandler::flightSegmentChanged,
            this, &AircraftPerfController::flightSegmentChanged);
    NavApp::setStatusMessage(tr("Performance data collection started."));
    emit aircraftPerformanceChanged(perf);
  }
  else
    // User canceled out of info dialog
    stopCollecting();
}

void AircraftPerfController::stopCollecting()
{
  if(perfHandler != nullptr)
  {
    disconnect(perfHandler, &AircraftPerfHandler::flightSegmentChanged,
               this, &AircraftPerfController::flightSegmentChanged);
    perfHandler->deleteLater();
    perfHandler = nullptr;
    NavApp::setStatusMessage(tr("Performance data collection stopped."));
    emit aircraftPerformanceChanged(perf);
  }

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionAircraftPerformanceCollect->blockSignals(true);
  ui->actionAircraftPerformanceCollect->setChecked(false);
  ui->actionAircraftPerformanceCollect->blockSignals(false);
}

void AircraftPerfController::helpClicked() const
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::HELP_ONLINE_URL + "AIRCRAFTPERF.html",
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

bool AircraftPerfController::collectPerformanceDialog()
{
  QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(
    lnm::HELP_ONLINE_URL + "AIRCRAFTPERF.html#aircraft-performance-collect",
    lnm::helpLanguageOnline());

  QString doNotShow = tr("Do not &show this dialog again.");
  QString message = tr("<p>Aircraft Performance will be collected automtically during flight.</p>"
                         "<p>Note that no performance information like fuel estimates or time to waypoint "
                           "is displayed during this process.</p>"
                           "<p>Top of climb and top of descent will be shown based on "
                             "a 3 nm per 1000 ft rule of thumb.</p>"
                             "<ol><li>Connect to the simulator,</li>"
                               "<li>load a flight plan,</li>"
                                 "<li>place your aircraft at the departure airport and</li>"
                                   "<li>fly the flight plan as precise as possible.</li></ol>"
                                     "<p>Performance collection will stop on touch down at destination.</p>"
                                       "<p>Do not change the flight plan cruise altitude while collecting information.</p>"
                                         "<p><b>Click the link below for more information:<br/><br/>"
                                         "<a href=\"%1\">Online Manual - Aircraft Performance</a></b><br/></p>"
                                           "<p><b>Start performance collection now?</b></p>")
                    .arg(url.toString());

  int retval = atools::gui::Dialog(mainWindow).showQuestionMsgBox(
    lnm::ACTIONS_SHOW_START_PERF_COLLECTION, message, doNotShow,
    QMessageBox::No | QMessageBox::Yes, QMessageBox::No, QMessageBox::Yes);

  switch(retval)
  {
    case QMessageBox::Yes:
      // Start collection

      if(!checkForChanges())
        // Save of currently modified canceled
        return false;

      perf->setNull();
      changed = true;
      currentFilepath.clear();
      return true;

    case QMessageBox::No:
      return false;

  }
  return false;
}

float AircraftPerfController::getWindDir() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  return ui->spinBoxAircraftPerformanceWindDirection->value();
}

float AircraftPerfController::getWindSpeed() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  return Unit::rev(ui->spinBoxAircraftPerformanceWindSpeed->value(), Unit::speedKtsF);
}

void AircraftPerfController::routeChanged(bool geometryChanged, bool newFlightplan)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(geometryChanged);
  Q_UNUSED(newFlightplan);
  updateReport();
  updateActionStates();
}

void AircraftPerfController::routeAltitudeChanged(float altitudeFeet)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(altitudeFeet);

  if(perfHandler != nullptr)
    perfHandler->setCruiseAltitude(NavApp::getAltitudeLegs().getCruiseAltitide());

  updateReport();
  updateActionStates();
}

void AircraftPerfController::flightSegmentChanged(const atools::fs::perf::FlightSegment& flightSegment)
{
  qDebug() << Q_FUNC_INFO << AircraftPerfHandler::getFlightSegmentString(flightSegment);

  if(flightSegment == atools::fs::perf::DESTINATION_TAXI || flightSegment == atools::fs::perf::DESTINTATION_PARKING)
  {
    stopCollecting();
    updateActionStates();
  }
  updateReport();
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
  connect(ui->actionAircraftPerformanceCollect, &QAction::toggled, this, &AircraftPerfController::collectToggled);

  connect(ui->pushButtonAircraftPerformanceNew, &QPushButton::clicked, this, &AircraftPerfController::create);
  connect(ui->pushButtonAircraftPerformanceEdit, &QPushButton::clicked, this, &AircraftPerfController::edit);
  connect(ui->pushButtonAircraftPerformanceLoad, &QPushButton::clicked, this, &AircraftPerfController::load);
  connect(ui->pushButtonAircraftPerformanceSave, &QPushButton::clicked, this, &AircraftPerfController::save);
  connect(ui->pushButtonAircraftPerformanceSaveAs, &QPushButton::clicked, this, &AircraftPerfController::saveAs);

  connect(ui->pushButtonAircraftPerformanceHelp, &QPushButton::clicked, this, &AircraftPerfController::helpClicked);

  connect(ui->spinBoxAircraftPerformanceWindDirection, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windChanged);
  connect(ui->spinBoxAircraftPerformanceWindSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windChanged);
}

void AircraftPerfController::updateActionStates()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool routeValid = NavApp::getRouteSize() >= 2;

  // Switch of all loading if collecting performance
  if(isCollecting())
    fileHistory->disableAll();
  else
    fileHistory->enableAll();

  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(routeValid && !isCollecting());
  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(routeValid && !isCollecting());

  ui->actionAircraftPerformanceNew->setEnabled(!isCollecting());
  ui->pushButtonAircraftPerformanceNew->setEnabled(!isCollecting());

  ui->actionAircraftPerformanceLoad->setEnabled(!isCollecting());
  ui->pushButtonAircraftPerformanceLoad->setEnabled(!isCollecting());

  ui->actionAircraftPerformanceEdit->setEnabled(!isCollecting());
  ui->pushButtonAircraftPerformanceEdit->setEnabled(!isCollecting());
}

void AircraftPerfController::updateReport()
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

  // Write HTML report ================================================================
  HtmlBuilder html(true /* background color */);

  // Icon, name and aircraft type =======================================================
  html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(),
           QSize(symbolSize, symbolSize));
  html.nbsp().nbsp();

  html.text(tr("%1 - %2").arg(perf->getName()).arg(perf->getAircraftType()),
            atools::util::html::BOLD | atools::util::html::BIG);
  atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;

  if(isCollecting())
  {
    // Display performance collection progress ==========================================================
    html.p().text(tr("Collecting performance information."), atools::util::html::BOLD).br().
    text(tr("Current flight segment: %1.").
         arg(perfHandler->getCurrentFlightSegmentString()));

    atools::fs::perf::FlightSegment segment = perfHandler->getCurrentFlightSegment();

    if(segment >= atools::fs::perf::DEPARTURE_TAXI)
    {
      html.p().b(tr("Fuel")).pEnd();
      html.table();
      html.row2(tr("Fuel Type:"), perf->isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
      html.row2(tr("Total Fuel Consumed:"), fuelLbsGal(perfHandler->getTotalFuelConsumed()), flags);
      html.row2(tr("Taxi Fuel:"), fuelLbsGal(perf->getTaxiFuel()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CLIMB)
    {
      html.p().b(tr("Average Performance")).br().b(tr("Climb")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(perf->getClimbSpeed()), flags);
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(perf->getClimbVertSpeed()) + tr(" <b>▲</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ffLbsGal(perf->getClimbFuelFlow()), flags);
      html.tableEnd();
    }

    if(segment >= atools::fs::perf::CRUISE)
    {
      html.p().b(tr("Cruise")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(perf->getCruiseSpeed()), flags);
      html.row2(tr("Fuel Flow:"), ffLbsGal(perf->getCruiseFuelFlow()), flags);
      html.tableEnd();
    }
    if(segment >= atools::fs::perf::DESCENT)
    {
      html.p().b(tr("Descent")).pEnd();
      html.table();
      html.row2(tr("True Airspeed:"), Unit::speedKts(perf->getDescentSpeed()), flags);
      // Descent speed is always positive
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(-perf->getDescentVertSpeed()) + tr(" <b>▼</b>"),
                atools::util::html::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ffLbsGal(perf->getDescentFuelFlow()), flags);
      html.tableEnd();
    }
  }
  else
  {
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
  QStringList lines = atools::probeFile(file);
  return file.endsWith(".lnmperf", Qt::CaseInsensitive) ||
         lines.at(0).startsWith("[options]") || lines.at(0).startsWith("[perf]");
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
    if(perf->getReserveFuel() < 1.0f)
      errs.append(tr("reserve fuel"));
    if(perf->getClimbFuelFlow() < 0.1f)
      errs.append(tr("climb fuel flow"));
    if(perf->getCruiseFuelFlow() < 0.1f)
      errs.append(tr("cruise fuel flow"));
    if(perf->getDescentFuelFlow() < 0.1f)
      errs.append(tr("descent fuel flow"));

    if(!errs.isEmpty())
      html.p().error((errs.size() == 1 ? tr("Invalid value for %1.") : tr("Invalid values for %1.")).
                     arg(errs.join(tr(", ")))).pEnd();
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
  html.tableEnd();

  // Fuel data =======================================================
  html.p().b(tr("Fuel Plan")).pEnd();
  html.table();
  html.row2(tr("Fuel Type:"), perf->isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);
  float tripFuel = altitudeLegs.getTripFuel();
  html.row2(tr("Trip Fuel:"), fuelLbsGal(tripFuel), atools::util::html::BOLD | flags);
  float blockFuel = (altitudeLegs.getTripFuel() * perf->getContingencyFuelFactor()) +
                    perf->getTaxiFuel() + perf->getExtraFuel() + perf->getReserveFuel();
  html.row2(tr("Block Fuel:"), fuelLbsGal(blockFuel), atools::util::html::BOLD | flags);
  float destFuel = blockFuel - tripFuel - perf->getTaxiFuel();
  if(atools::almostEqual(destFuel, 0.f, 0.1f))
    // Avoid -0 case
    destFuel = 0.f;
  html.row2(tr("Fuel at Destination:"), fuelLbsGal(destFuel), flags);
  html.row2(tr("Reserve Fuel:"), fuelLbsGal(perf->getReserveFuel()), flags);
  html.row2(tr("Taxi Fuel:"), fuelLbsGal(perf->getTaxiFuel()), flags);
  html.row2(tr("Extra Fuel:"), fuelLbsGal(perf->getExtraFuel()), flags);

  float contFuel = altitudeLegs.getTripFuel() * (perf->getContingencyFuelFactor() - 1.f);
  html.row2(tr("Contingency Fuel:"), tr("%1 %, %2").
            arg(perf->getContingencyFuel(), 0, 'f', 0).
            arg(fuelLbsGal(contFuel)), flags);
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
              arg(Unit::distNm(1.f / perf->getDescentRateFtPerNm() * 1000.f)).
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

QString AircraftPerfController::fuelLbsGal(float valueLbsGal)
{
  if(perf->useFuelAsVolume())
    return Unit::fuelLbsAndGal(AircraftPerf::fromGalToLbs(perf->isJetFuel(), valueLbsGal), valueLbsGal);
  else
    return Unit::fuelLbsAndGal(valueLbsGal, AircraftPerf::fromLbsToGal(perf->isJetFuel(), valueLbsGal));
}

QString AircraftPerfController::ffLbsGal(float valueLbsGal)
{
  if(perf->useFuelAsVolume())
    return Unit::ffLbsAndGal(AircraftPerf::fromGalToLbs(perf->isJetFuel(), valueLbsGal), valueLbsGal);
  else
    return Unit::ffLbsAndGal(valueLbsGal, AircraftPerf::fromLbsToGal(perf->isJetFuel(), valueLbsGal));
}

void AircraftPerfController::saveState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  fileHistory->saveState();
  settings.setValue(lnm::AIRCRAFT_PERF_FILENAME, currentFilepath);

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::AIRCRAFT_PERF_WIDGETS).save({ui->spinBoxAircraftPerformanceWindSpeed,
                                                             ui->spinBoxAircraftPerformanceWindDirection});
}

void AircraftPerfController::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  fileHistory->restoreState();
  loadFile(settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME));

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRCRAFT_PERF_WIDGETS, true, true);
  state.restore({ui->spinBoxAircraftPerformanceWindSpeed, ui->spinBoxAircraftPerformanceWindDirection});

  optionsChanged();
  updateActionStates();
  updateReport();
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
  }

  symbolSize = ui->textBrowserAircraftPerformanceReport->fontMetrics().height() * 4 / 3;

  updateReport();
}

void AircraftPerfController::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(perfHandler != nullptr)
  {
    // Pass to handler for averaging
    perfHandler->simDataChanged(simulatorData);

    qint64 currentSampleTime = QDateTime::currentMSecsSinceEpoch();
    if(currentSampleTime > reportLastSampleTimeMs + 1000)
    {
      // Update report every second
      reportLastSampleTimeMs = currentSampleTime;
      changed = true;
      updateReport();
    }
  }
}

void AircraftPerfController::windChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Start delayed wind update
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
