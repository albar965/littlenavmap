/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
#include "common/filecheck.h"
#include "perf/aircraftperfdialog.h"

#include "gui/mainwindow.h"
#include "gui/helphandler.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "gui/widgetutil.h"
#include "util/average.h"
#include "app/navapp.h"
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
#include "perf/perfmergedialog.h"
#include "route/routealtitude.h"
#include "gui/widgetstate.h"
#include "fs/perf/aircraftperfhandler.h"
#include "fs/sc/simconnectdata.h"
#include "gui/tabwidgethandler.h"

#include <QDebug>
#include <QUrlQuery>
#include <QStringBuilder>

using atools::fs::perf::AircraftPerf;
using atools::fs::perf::AircraftPerfHandler;
using atools::util::HtmlBuilder;
namespace ahtml = atools::util::html;

AircraftPerfController::AircraftPerfController(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  perf = new AircraftPerf();

  // Calculate fuel flow average over ten seconds
  fuelFlowGroundspeedAverage = new atools::util::MovingAverageTime(10000);

  lastSimData = new atools::fs::sc::SimConnectData();
  // Remember original font for resizing in options
  infoFontPtSize = static_cast<float>(ui->textBrowserAircraftPerformanceReport->font().pointSizeF());

  QStringList paths({QApplication::applicationDirPath()});
  ui->textBrowserAircraftPerformanceReport->setSearchPaths(paths);
  ui->textBrowserAircraftPerformanceCurrent->setSearchPaths(paths);

  fileHistory = new atools::gui::FileHistoryHandler(this, lnm::AIRCRAFT_PERF_FILENAMESRECENT,
                                                    ui->menuAircraftPerformanceRecent,
                                                    ui->actionAircraftPerformanceClearMenu);

  connect(fileHistory, &atools::gui::FileHistoryHandler::fileSelected, this, &AircraftPerfController::loadFile);
  connect(ui->textBrowserAircraftPerformanceReport, &QTextBrowser::anchorClicked, this, &AircraftPerfController::anchorClicked);
  connect(ui->textBrowserAircraftPerformanceCurrent, &QTextBrowser::anchorClicked, this, &AircraftPerfController::anchorClicked);

  // Pass wind change with a delay to avoid lagging mouse wheel
  connect(&windChangeTimer, &QTimer::timeout, this, &AircraftPerfController::windChangedDelayed);
  windChangeTimer.setSingleShot(true);

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
  delete lastSimData;
  delete fuelFlowGroundspeedAverage;
}

void AircraftPerfController::create()
{
  qDebug() << Q_FUNC_INFO;

  mainWindow->showAircraftPerformance();

  // Initialize with default parameters and set current simulator
  AircraftPerf editPerf;
  editPerf.resetToDefault(NavApp::getCurrentSimulatorShortName());

  bool edited = false;
  // Check for saving current profile
  if(checkForChanges())
  {
    bool saveClicked = false;
    if(editInternal(editPerf, tr("Create"), true /* newPerf */, saveClicked))
    {
      // New profile created
      currentFilepath.clear();
      *perf = editPerf;
      edited = true;
      changed = true;

      if(saveClicked)
        save();
    }
  }

  if(edited)
    windChangeTimer.stop();

  // Update change flags in tab and window title
  updateActionStates();

  if(edited)
  {
    updateReport();
    emit aircraftPerformanceChanged(perf);
    NavApp::setStatusMessage(tr("Aircraft performance created."));
  }
}

void AircraftPerfController::edit()
{
  qDebug() << Q_FUNC_INFO;

  bool saveClicked = false;
  bool edited = editInternal(*perf, tr("Edit"), false /* newPerf */, saveClicked);

  if(edited)
    changed = true;

  if(saveClicked)
    save();

  if(edited)
    windChangeTimer.stop();

  // Update change flags in tab and window title
  updateActionStates();

  if(edited)
  {
    updateReport();
    emit aircraftPerformanceChanged(perf);
    NavApp::setStatusMessage(tr("Aircraft performance changed."));
  }
}

bool AircraftPerfController::editInternal(atools::fs::perf::AircraftPerf& editPerf, const QString& modeText, bool newPerf,
                                          bool& saveClicked)
{
  qDebug() << Q_FUNC_INFO;

  AircraftPerfDialog dialog(mainWindow, editPerf, modeText, newPerf);
  if(dialog.exec() == QDialog::Accepted)
  {
    editPerf = dialog.getAircraftPerf();
    saveClicked = dialog.isSaveClicked();
    return true;
  }
  return false;
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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
  }

  updateActionStates();
  updateReport();
  emit aircraftPerformanceChanged(perf);
}

void AircraftPerfController::loadFile(const QString& perfFile)
{
  qDebug() << Q_FUNC_INFO << perfFile;

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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
    fileHistory->removeFile(perfFile);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
    fileHistory->removeFile(perfFile);
  }

  updateActionStates();
  updateReport();
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
      updateReport();
      updateReportCurrent();
      mainWindow->showAircraftPerformance();
      emit aircraftPerformanceChanged(perf);
      NavApp::setStatusMessage(tr("Aircraft performance merged."));

      if(dialog.isSaveClicked())
        save();
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

void AircraftPerfController::calculateFuelAndTimeTo(FuelTimeResult& result, float distanceToDest, float distanceToNext,
                                                    int activeLeg) const
{

  const atools::fs::sc::SimConnectUserAircraft& ac = NavApp::getUserAircraft();
  NavApp::getAltitudeLegs().calculateFuelAndTimeTo(result, distanceToDest, distanceToNext, *perf,
                                                   ac.getFuelFlowPPH(), ac.getFuelFlowGPH(),
                                                   ac.getGroundSpeedKts(), activeLeg);
}

float AircraftPerfController::getBlockFuel() const
{
  return NavApp::getAltitudeLegs().getBlockFuel(*perf);
}

float AircraftPerfController::getTripFuel() const
{
  return NavApp::getAltitudeLegs().getTripFuel();
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
    updateReport();
    emit aircraftPerformanceChanged(perf);
    NavApp::setStatusMessage(tr("Aircraft performance merged."));

    if(dialog.isSaveClicked())
      save();
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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    noPerfLoaded();
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    noPerfLoaded();
  }
  updateActionStates();
  updateReport();
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
      NavApp::closeSplashScreen();
      atools::gui::ErrorHandler(mainWindow).handleException(e);
      retval = false;
    }
    catch(...)
    {
      NavApp::closeSplashScreen();
      atools::gui::ErrorHandler(mainWindow).handleUnknownException();
      retval = false;
    }

    emit aircraftPerformanceChanged(perf);
    updateActionStates();
    updateReport();
    return retval;
  }
}

bool AircraftPerfController::saveAsStr(const QString& string) const
{
  qDebug() << Q_FUNC_INFO;
  bool retval = false;

  try
  {
    // Load performance to get the filename
    AircraftPerf aperf;
    aperf.loadXmlStr(string);

    QString filename = atools::cleanFilename(aperf.getName()) % ".lnmperf";
    QString perfFile = saveAsFileDialog(filename);
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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    retval = false;
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    retval = false;
  }
  return retval;
}

QString AircraftPerfController::saveAsFileDialog(const QString& filepath, bool *oldFormat) const
{
  QString nameFilter;

  if(oldFormat != nullptr)
    nameFilter = tr("Aircraft Performance Files %1;;Aircraft Performance Files old Format %2;;All Files (*)").
                 arg(lnm::FILE_PATTERN_AIRCRAFT_PERF).arg(lnm::FILE_PATTERN_AIRCRAFT_PERF_INI);
  else
    nameFilter = tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF);

  int nameFilterIndex = 0;
  QString file = atools::gui::Dialog(mainWindow).saveFileDialog(
    tr("Save Aircraft Performance File"), nameFilter, "lnmperf", "AircraftPerformance/",
    QString(), filepath, false /* confirm overwrite */, false /* autoNumberFilename */, &nameFilterIndex);

  if(oldFormat != nullptr)
    *oldFormat = nameFilterIndex == 1;

  return file;
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
    bool oldFormat = false;
    QString perfFile = saveAsFileDialog(currentFilepath.isEmpty() ?
                                        atools::cleanFilename(perf->getName()) % ".lnmperf" :
                                        QFileInfo(currentFilepath).fileName(), &oldFormat);
    if(!perfFile.isEmpty())
    {
      currentFilepath = perfFile;
      if(oldFormat)
        perf->saveIni(perfFile);
      else
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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    retval = false;
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
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

  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl % "AIRCRAFTPERF.html", lnm::helpLanguageOnline());
}

void AircraftPerfController::helpClickedPerfCollect() const
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl % "AIRCRAFTPERFCOLL.html", lnm::helpLanguageOnline());
}

bool AircraftPerfController::checkForChanges()
{
  if(!changed)
    return true;

  QMessageBox msgBox(mainWindow);
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

float AircraftPerfController::getManualWindDirDeg() const
{
  return NavApp::getMainUi()->spinBoxAircraftPerformanceWindDirection->value();
}

float AircraftPerfController::getManualWindSpeedKts() const
{
  return Unit::rev(NavApp::getMainUi()->spinBoxAircraftPerformanceWindSpeed->value(), Unit::speedKtsF);
}

float AircraftPerfController::getManualWindAltFt() const
{
  return Unit::rev(NavApp::getMainUi()->spinBoxAircraftPerformanceWindAlt->value(), Unit::altFeetF);
}

bool AircraftPerfController::isWindManual() const
{
  return NavApp::getMainUi()->actionMapShowWindManual->isChecked();
}

float AircraftPerfController::cruiseAlt()
{
  float alt = NavApp::getAltitudeLegs().getCruiseAltitude();

  if(atools::almostEqual(alt, 0.f) || alt > map::INVALID_ALTITUDE_VALUE / 2.f)
    alt = NavApp::getRouteCruiseAltFtWidget();

  return alt;
}

void AircraftPerfController::routeChanged(bool, bool)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

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

void AircraftPerfController::routeAltitudeChanged(float)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  perfHandler->setCruiseAltitude(cruiseAlt());
  updateReport();
  updateReportCurrent();
  updateActionStates();
}

void AircraftPerfController::warningChanged()
{
  updateReport();
}

void AircraftPerfController::flightSegmentChanged(const atools::fs::perf::FlightSegment& flightSegment)
{
  qDebug() << Q_FUNC_INFO << AircraftPerfHandler::getFlightSegmentString(flightSegment);

  updateActionStates();
  updateReportCurrent();
  NavApp::setStatusMessage(tr("Flight segment %1.").
                           arg(AircraftPerfHandler::getFlightSegmentString(flightSegment)),
                           true /* addToLog */);
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

  connect(ui->spinBoxAircraftPerformanceWindDirection, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
  connect(ui->spinBoxAircraftPerformanceWindSpeed, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &AircraftPerfController::windBoxesChanged);
  connect(ui->spinBoxAircraftPerformanceWindAlt, QOverload<int>::of(&QSpinBox::valueChanged),
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
        ui->tabWidgetRoute->setTabText(idx, ui->tabWidgetRoute->tabText(idx) % tr(" *"));
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
  NavApp::updateWindowTitle();

  ui->actionAircraftPerformanceRestart->setEnabled(perfHandler->hasFlightSegment());
  ui->pushButtonAircraftPerfCollectRestart->setEnabled(perfHandler->hasFlightSegment());

  bool manualWind = isWindManual();
  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(manualWind);
  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(manualWind);
  ui->spinBoxAircraftPerformanceWindAlt->setEnabled(manualWind);
  atools::gui::util::showHideLayoutElements({ui->horizontalLayoutManWind}, {}, manualWind, true /* disable */);
}

void AircraftPerfController::updateReport()
{
  errorTooltips.clear();

  Ui::MainWindow *ui = NavApp::getMainUi();
  HtmlBuilder html(true /* background color */);

  bool visible = NavApp::getRouteTabHandler()->getCurrentTabId() == rc::AIRCRAFT && ui->dockWidgetRoute->isVisible();
  if(visible)
  {
    // Write HTML report ================================================================

    // Icon, name and aircraft type =======================================================
    QStringList headerList({perf->getName(), perf->getAircraftType(), perf->getSimulator()});
    headerList.removeAll(QString());
    QString header = headerList.join(tr(" - "));

    if(!header.isEmpty())
    {
      html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(), QSize(symbolSize, symbolSize));
      html.nbsp().nbsp();

      html.text(header, ahtml::BOLD | ahtml::BIG);
    }

    // Display fuel estimates ==========================================================
    const RouteAltitude& altitudeLegs = NavApp::getAltitudeLegs();
    if(NavApp::getRouteConst().isEmpty())
      html.p().
      warning(tr("No Flight Plan.")).
      pEnd();
    else if(altitudeLegs.isEmpty())
      html.p().
      warning(tr("Cannot calculate fuel report.")).br().
      warning(tr("Invalid Flight Plan.")).
      pEnd();
    else if(altitudeLegs.hasUnflyableLegs())
      html.p().
      warning(tr("Cannot calculate fuel report.")).br().
      warning(tr("Flight plan has unflyable legs where head wind is larger than cruise speed.")).
      pEnd();
    else if(!altitudeLegs.isValidProfile())
      html.p().
      warning(tr("Cannot calculate fuel report.")).br().
      warning(tr("Possible reasons:<br/>"
                 "- The flight plan is too short or the cruise altitude is too high.<br/>"
                 "- Climb and descent speeds in the aircraft performance data are too low.<br/>"
                 "- Departure or destination airport elevation is above cruise altitude.<br/>"
                 "- Cruise altitude violates one or more procedure altitude restrictions.")).
      pEnd();
  }

  // Fuel reports prints all or a part of it depending on valid altitude legs
  // Update report even if tab is not visible to update error messages
  fuelReport(html, false /* print */, visible);

  if(visible)
  {
    // Description and file =======================================================
    if(!perf->getDescription().isEmpty())
    {
      html.p().b(tr("Remarks")).pEnd();
      html.table(1, 2, 0, 100, html.getRowBackColor());
      html.tr().td(perf->getDescription(), ahtml::AUTOLINK).trEnd();
      html.tableEnd();
    }

    // File path  =======================================================
    fuelReportFilepath(html, false /* print */);

#ifdef DEBUG_INFORMATION_PERF
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
  } // if(NavApp::getRouteTabHandler()->getCurrentTabId() == rc::AIRCRAFT && ui->dockWidgetRoute->isVisible())

  // Update error message label
  NavApp::updateErrorLabel();
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

    QStringList header({curPerfLbs.getName(), curPerfLbs.getAircraftType(), curPerfLbs.getSimulator()});
    header.removeAll(QString());

    if(!header.isEmpty())
      html.text(header.join(tr(" - ")), ahtml::BOLD | ahtml::BIG);
    else
      html.text(tr("Unknown Aircraft"), ahtml::BOLD | ahtml::BIG);

    ahtml::Flags flags = ahtml::ALIGN_RIGHT;
    FuelTool ft(curPerfLbs);

    // Display performance collection progress ==========================================================
    if(NavApp::isRouteEmpty())
      html.p().b(tr("No flight plan.")).pEnd();
    // else if(!NavApp::isValidProfile())
    // html.p().b(tr("Elevation profile not valid.")).pEnd();
    else if(segment == atools::fs::perf::NONE)
      html.p().b(tr("No flight detected.")).pEnd();
    else
    {
      html.p().b(tr("Aircraft")).pEnd();
      html.table();
      html.row2(tr("Current flight segment: "), perfHandler->getCurrentFlightSegmentString() %
                (perfHandler->isFinished() ? tr(", <b>Finished.</b>") : QString()), ahtml::NO_ENTITIES);
      html.row2If(tr("Aircraft status: "), perfHandler->getAircraftStatusTexts().join(tr(", ")));
      html.tableEnd();
      html.pEnd();
    }

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
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(curPerfLbs.getClimbVertSpeed()) % tr(" <b>▲</b>"), ahtml::NO_ENTITIES | flags);
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
      html.row2(tr("Vertical Speed:"), Unit::speedVertFpm(-curPerfLbs.getDescentVertSpeed()) % tr(" <b>▼</b>"), ahtml::NO_ENTITIES | flags);
      html.row2(tr("Fuel Flow:"), ft.flowWeightVolLocal(curPerfLbs.getDescentFuelFlow()), flags);
      html.tableEnd();
    }

    atools::gui::util::updateTextEdit(ui->textBrowserAircraftPerformanceCurrent, html.getHtml(),
                                      false /*scrollToTop*/, true /* keep selection */);
  }
}

void AircraftPerfController::fuelReportFilepath(atools::util::HtmlBuilder& html, bool print)
{
  if(!currentFilepath.isEmpty() && print)
    // Use a simple layout for printing
    html.p().b(tr("Performance File:")).nbsp().nbsp().small(currentFilepath).pEnd();
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

    html.row2(tr("Minimum runway:"), runwayTxt.join(tr(", ")), ahtml::ALIGN_RIGHT);
  }
}

void AircraftPerfController::fuelReport(atools::util::HtmlBuilder& html, bool print, bool visible)
{
  const RouteAltitude& altLegs = NavApp::getAltitudeLegs();

  bool hasLegs = altLegs.size() > 1;
  bool valid = NavApp::isValidProfile() && !NavApp::getAltitudeLegs().hasUnflyableLegs();

  if(print)
    // Include header here if printing
    html.h2(tr("Aircraft Performance %1 - %2").arg(perf->getName()).arg(perf->getAircraftType()), ahtml::BOLD | ahtml::BIG);

  FuelTool ft(perf);

  // Warnings and errors paragraph - always shown =================================================
  if(!print)
  {
    // Collect unset values ===========================================
    QStringList errs;
    if(perf->getAircraftType().isEmpty())
      errs.append(tr("aircraft type"));
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
    {
      QString msg((errs.size() == 1 ? tr("Invalid value for %1.") : tr("Invalid values for %1.")).arg(errs.join(tr(", "))));
      errorTooltips.append(msg);

      if(visible)
        html.p().error(msg).pEnd();
    }

    // Collect inconsitencies ===========================================
    if(visible)
    {
      QStringList extraErrors;
      // Climb and descent fuel flow compared to cruise ==================
      if(perf->getCruiseFuelFlow() >= 0.1f)
      {
        if(perf->getClimbFuelFlow() >= 0.1f && perf->getClimbFuelFlow() < perf->getCruiseFuelFlow())
          extraErrors.append(tr("Climb fuel flow is smaller than cruise fuel flow."));
        if(perf->getDescentFuelFlow() >= 0.1f && perf->getDescentFuelFlow() > perf->getCruiseFuelFlow() * 1.2f)
          extraErrors.append(tr("Descent fuel flow is much higher than cruise fuel flow."));
      }

      // Climb and descent fuel speed compared to cruise ==================
      if(perf->getCruiseSpeed() >= 0.1f)
      {
        if(perf->getClimbSpeed() > 0.1f && perf->getClimbSpeed() > perf->getCruiseSpeed())
          extraErrors.append(tr("Climb speed is higher than cruise speed."));
        if(perf->getDescentSpeed() > 0.1f && perf->getDescentSpeed() < perf->getCruiseSpeed() * 0.5f)
          extraErrors.append(tr("Descent speed is much smaller than cruise speed."));
      }

      // Aircraft ICAO type ======================================
      if(perf->getAircraftType().isEmpty())
        extraErrors.append(tr("Aircraft type is empty. Use an official ICAO code like \"B738\", \"BE9L\" or \"C172\"."));
      // else if(!perf->isAircraftTypeValid())
      // extraErrors.append(tr("Aircraft type \"%1\" is not valid. Use official ICAO codes like \"B738\", \"BE9L\" or \"C172\".").
      // arg(perf->getAircraftType()));

      if(!extraErrors.isEmpty())
      {
        html.p();
        html.warning(tr("Possible problems found:")).br();
        for(const QString& err : qAsConst(extraErrors))
          html.warning(err).br();
        html.pEnd();
      }
    } // if(visible)

    // Other issues
    if(hasLegs && perf->getUsableFuel() > 1.f && altLegs.getBlockFuel(*perf) > perf->getUsableFuel())
    {
      QString msg(tr("Block fuel of %1 exceeds usable of %2.").
                  arg(ft.weightVolLocal(altLegs.getBlockFuel(*perf))).
                  arg(ft.weightVolLocal(perf->getUsableFuel())));
      errorTooltips.append(msg);

      if(visible)
        html.p().error(msg).pEnd();
    }

    if(perf->getUsableFuel() > 1.f && perf->getReserveFuel() > perf->getUsableFuel())
    {
      QString msg(tr("Reserve fuel of %1 exceeds usable of %2.").
                  arg(ft.weightVolLocal(perf->getReserveFuel())).
                  arg(ft.weightVolLocal(perf->getUsableFuel())));
      errorTooltips.append(msg);

      if(visible)
        html.p().error(msg).pEnd();
    }

    if(!perf->getAircraftType().isEmpty())
    {
      if(NavApp::getMainUi()->actionAircraftPerformanceWarnMismatch->isChecked())
      {
        QString model = lastSimData->getUserAircraft().getAirplaneModel();
        if(!perf->isDefault() && !model.isEmpty() && perf->getAircraftType() != model)
        {
          QString msg(tr("User aircraft type \"%1\" in simulator is not equal to type \"%2\" used in performance file.\n"
                         "Load the matching aircraft performance file or adapt the field \"Aircraft type\" in the currently loaded file.").
                      arg(model).arg(perf->getAircraftType()));
          errorTooltips.append(msg);

          if(visible)
            html.p().warning(msg).pEnd();
        }
      }
    }
  } // if(!print)

  if(visible || print)
  {
    // Aircraft data table - always shown =======================================================
    ahtml::Flags flags = ahtml::ALIGN_RIGHT;
    html.p().b(tr("Aircraft")).pEnd();

    QString text = tr("Estimated range with reserve:");
    html.table();

    if(perf->getUsableFuel() > 1.f)
    {
      if(perf->getCruiseSpeed() > 1.f)
      {
        if(perf->getCruiseFuelFlow() > 1.f)
        {
          float enduranceHours, enduranceNm;
          getEnduranceFull(enduranceHours, enduranceNm);
          html.row2(text, tr("%1, %2").arg(Unit::distNm(enduranceNm)).arg(formatter::formatMinutesHoursLong(enduranceHours)), flags);
        }
        else
          html.row2Warning(text, tr("Cruise fuel flow not set"));
      }
      else
        html.row2Warning(text, tr("Cruise speed not set"));
    }
    else
      html.row2Warning(text, tr("Usable fuel not set"));

    fuelReportRunway(html);
    html.tableEnd();

    if(hasLegs && valid)
    {
      // Flight plan table - shown if plan and profile is valid =================================================
      html.p().b(tr("Flight Plan")).pEnd();
      html.table();

      if(altLegs.getTravelTimeHours() > 0.f)
        html.row2(tr("Distance and Time:"), tr("%1, %2").
                  arg(Unit::distNm(altLegs.getTotalDistance())).
                  arg(formatter::formatMinutesHoursLong(altLegs.getTravelTimeHours())), ahtml::BOLD | flags);
      else
        html.row2(tr("Distance:"), tr("%1").
                  arg(Unit::distNm(altLegs.getTotalDistance())), ahtml::BOLD | flags);

      html.row2(tr("Average Ground Speed:"), Unit::speedKts(altLegs.getAverageGroundSpeed()), flags);
      html.row2(tr("True Airspeed at Cruise:"), Unit::speedKts(perf->getCruiseSpeed()), flags);

      float mach = atools::geo::tasToMachFromAlt(altLegs.getCruiseAltitude(), static_cast<float>(perf->getCruiseSpeed()));
      if(mach > 0.4f)
        html.row2(tr("Mach at cruise:"), QLocale().toString(mach, 'f', 2), flags);

      // Wind =======================================================
      windText(html, tr("Average wind total"), altLegs.getWindSpeedAverage(),
               altLegs.getWindDirectionAverage(), altLegs.getHeadWindAverage());
      windText(html, tr("Average wind at cruise"), altLegs.getWindSpeedCruiseAverage(),
               altLegs.getWindDirectionCruiseAverage(), altLegs.getCruiseHeadWind());

      html.tableEnd();
    } // if(hasLegs)

    // Fuel Plan table - always shown =======================================================
    html.p().b(valid ? tr("Fuel Plan") : tr("Fuel")).pEnd();
    html.table();

    html.row2(tr("Fuel Type:"), perf->isAvgas() ? tr("Avgas") : tr("Jetfuel"), flags);

    if(perf->getUsableFuel() > 0.f)
      html.row2(tr("Usable Fuel:"), ft.weightVolLocal(perf->getUsableFuel()), flags);

    float tripFuel = altLegs.getTripFuel();
    if(tripFuel > 0.f && valid)
      html.row2(tr("Trip Fuel:"), ft.weightVolLocalOther(tripFuel, true /* bold */), ahtml::NO_ENTITIES | flags);

    float blockFuel = altLegs.getBlockFuel(*perf);
    if(blockFuel > 0.f && valid)
    {
      QString percent = perf->getUsableFuel() > 1.f ? tr("<br/><b>%1 % of usable Fuel</b>").
                        arg(100.f / perf->getUsableFuel() * blockFuel, 0, 'f', 0) : QString();

      QString block = HtmlBuilder::textStr(ft.weightVolLocalOther(blockFuel, true /* bold */),
                                           ahtml::BIG | ahtml::BOLD | ahtml::NO_ENTITIES);
      html.row2(tr("Block Fuel:"), block % percent, ahtml::NO_ENTITIES | flags);

      html.row2(tr("Fuel at Destination:"), ft.weightVolLocal(altLegs.getDestinationFuel(*perf)), flags);

      if(altLegs.getAlternateFuel() > 0.f)
        html.row2(tr("Alternate Fuel:"), ft.weightVolLocal(altLegs.getAlternateFuel()), flags);
    }

    // Fuel information printed always =====================================================

    if(perf->getReserveFuel() > 0.f)
      html.row2(tr("Reserve Fuel:"), ft.weightVolLocal(perf->getReserveFuel()), flags);

    if(perf->getTaxiFuel() > 0.f)
      html.row2(tr("Taxi Fuel:"), ft.weightVolLocal(perf->getTaxiFuel()), flags);

    if(perf->getExtraFuel() > 0.f)
      html.row2(tr("Extra Fuel:"), ft.weightVolLocal(perf->getExtraFuel()), flags);

    if(perf->getContingencyFuel() > 0.f)
    {
      if(hasLegs && valid)
        html.row2(tr("Contingency Fuel:"), tr("%1 %, %2").
                  arg(perf->getContingencyFuel(), 0, 'f', 0).
                  arg(ft.weightVolLocal(altLegs.getContingencyFuel(*perf))), flags);
      else
        html.row2(tr("Contingency Fuel:"), tr("%1 %").arg(perf->getContingencyFuel(), 0, 'f', 0), flags);
    }

    html.tableEnd();

    // Climb and descent phases =======================================================
    if(hasLegs && valid)
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
}

void AircraftPerfController::getEnduranceFull(float& enduranceHours, float& enduranceNm)
{
  if(perf->getUsableFuel() > 1.f && perf->getCruiseSpeed() > 1.f && perf->getCruiseFuelFlow() > 1.f)
  {
    float realFuelFlow = perf->getCruiseFuelFlow() * perf->getContingencyFuelFactor();
    enduranceHours = (perf->getUsableFuel() - perf->getReserveFuel()) / realFuelFlow;
    enduranceNm = enduranceHours * perf->getCruiseSpeed();
  }
  else
  {
    enduranceHours = map::INVALID_TIME_VALUE;
    enduranceNm = map::INVALID_DISTANCE_VALUE;
  }
}

void AircraftPerfController::getEnduranceAverage(float& enduranceHours, float& enduranceNm)
{
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = lastSimData->getUserAircraftConst();

  enduranceHours = map::INVALID_TIME_VALUE;
  enduranceNm = map::INVALID_DISTANCE_VALUE;

  if(userAircraft.isValid() && userAircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT &&
     userAircraft.getFuelFlowPPH() > 1.0f && userAircraft.getGroundSpeedKts() > map::MIN_GROUND_SPEED)
  {
    float fuelFlowPph = 0.f, groundspeedKts = 0.f;
    fuelFlowGroundspeedAverage->getAverages(fuelFlowPph, groundspeedKts);

    if(fuelFlowPph > 0.f && groundspeedKts > 0.f)
    {
      float realFuelFlow = fuelFlowPph * perf->getContingencyFuelFactor();
      enduranceHours = std::max((userAircraft.getFuelTotalWeightLbs() - perf->getReserveFuelLbs()) / realFuelFlow, 0.f);
      enduranceNm = enduranceHours * groundspeedKts;
    }
  }
}

void AircraftPerfController::windText(atools::util::HtmlBuilder& html, const QString& label, float windSpeed,
                                      float windDirection, float headWind) const
{
  WindReporter *windReporter = NavApp::getWindReporter();

  QStringList windText;
  if(isWindManual() || windReporter->hasOnlineWindData())
  {
    if(std::abs(windSpeed) >= 1.f)
      // Display direction and speed if wind is not manually selected and available ====================
      windText.append(tr("%1°T, %2").
                      arg(windDirection, 0, 'f', 0).
                      arg(Unit::speedKts(windSpeed)));

    // Display manual wind - only head- or tailwind =======================
    if(std::abs(headWind) >= 1.f)
    {
      QString windType;
      QString windPtr;
      if(headWind >= 1.f)
      {
        windPtr = tr("▼");
        windType = tr("headwind");
      }
      else if(headWind <= -1.f)
      {
        windPtr = tr("▲");
        windType = tr("tailwind");
      }
      windText.append(tr("%1 %2 %3").arg(windPtr).arg(Unit::speedKts(std::abs(headWind))).arg(windType));
    }
  }

  QString head = tr("%1 (%2):");
  if(!windText.isEmpty())
    html.row2(head.arg(label).arg(windReporter->getSourceText()), windText.join(tr("\n")), ahtml::ALIGN_RIGHT);
  else
    html.row2(head.arg(label).arg(windReporter->getSourceText()), windReporter->isWindManual() ?
              tr("No head- or tailwind") : tr("No wind"), ahtml::ALIGN_RIGHT);
}

void AircraftPerfController::saveState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  fileHistory->saveState();
  settings.setValue(lnm::AIRCRAFT_PERF_FILENAME, currentFilepath);

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::AIRCRAFT_PERF_WIDGETS).save({ui->spinBoxAircraftPerformanceWindSpeed,
                                                             ui->spinBoxAircraftPerformanceWindDirection,
                                                             ui->spinBoxAircraftPerformanceWindAlt});
}

void AircraftPerfController::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Need to initialize this late since route controller is not valid when AircraftPerfController constructor is called
  connect(NavApp::getRouteTabHandler(), &atools::gui::TabWidgetHandler::tabChanged, this, &AircraftPerfController::tabVisibilityChanged);
  connect(NavApp::getRouteTabHandler(), &atools::gui::TabWidgetHandler::tabOpened, this, &AircraftPerfController::updateTabTiltle);

  fileHistory->restoreState();

  // Load last used performance file or the one passed on the command line
  if(!NavApp::isSafeMode())
  {
    QString perfFile;
    fc::fromStartupProperties(NavApp::getStartupOptionsConst(), nullptr, nullptr, &perfFile);

    if(perfFile.isEmpty() && OptionData::instance().getFlags() & opts::STARTUP_LOAD_PERF)
      perfFile = settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME);

    if(!perfFile.isEmpty())
    {
      QString message = atools::checkFileMsg(perfFile);
      if(message.isEmpty())
        loadFile(perfFile);
      else
      {
        // No file or not readable
        NavApp::closeSplashScreen();
        QMessageBox::warning(mainWindow, QApplication::applicationName(), message);
      }
    }
  }

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRCRAFT_PERF_WIDGETS, true /* visibility */, true /* block signals */);
  state.restore({ui->spinBoxAircraftPerformanceWindSpeed,
                 ui->spinBoxAircraftPerformanceWindDirection,
                 ui->spinBoxAircraftPerformanceWindAlt});

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

void AircraftPerfController::connectedToSimulator()
{
  currentReportLastSampleTimeMs = reportLastSampleTimeMs = 0L; // Force update on next simDataChanged
  *lastSimData = atools::fs::sc::SimConnectData();
}

void AircraftPerfController::disconnectedFromSimulator()
{
  *lastSimData = atools::fs::sc::SimConnectData();
  updateReports();
}

void AircraftPerfController::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  *lastSimData = simulatorData;

#ifdef DEBUG_INFORMATION_PERF_SIMDATA
  qDebug() << Q_FUNC_INFO << simulatorData.getUserAircraftConst().getZuluTime().toString(Qt::ISODateWithMs)
           << simulatorData.getUserAircraftConst().getZuluTime().toMSecsSinceEpoch();
  qDebug() << Q_FUNC_INFO << simulatorData.getUserAircraftConst().getLocalTime().toString(Qt::ISODateWithMs)
           << simulatorData.getUserAircraftConst().getLocalTime().toMSecsSinceEpoch();
#endif

  // Pass to handler for averaging
  perfHandler->simDataChanged(simulatorData, NavApp::getCurrentSimulatorShortName());

  if(simulatorData.isUserAircraftValid())
  {
    const atools::fs::sc::SimConnectUserAircraft& userAircraft = simulatorData.getUserAircraftConst();

    if(userAircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT &&
       userAircraft.getFuelFlowPPH() > 1.0f && userAircraft.getGroundSpeedKts() > map::MIN_GROUND_SPEED)
      fuelFlowGroundspeedAverage->addSamples(userAircraft.getFuelFlowPPH(), userAircraft.getGroundSpeedKts(),
                                             userAircraft.getZuluTime().toMSecsSinceEpoch());

#ifdef DEBUG_INFORMATION_PERF_SIMDATA
    static qint64 lastTime = 0L;
    qDebug() << Q_FUNC_INFO << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
    qDebug() << Q_FUNC_INFO << "diff"
             << (userAircraft.getZuluTime().toMSecsSinceEpoch() - lastTime)
             << userAircraft.getZuluTime().toMSecsSinceEpoch() << lastTime;

    qDebug() << Q_FUNC_INFO << "zulu" << userAircraft.getZuluTime().toString(Qt::ISODateWithMs);
    lastTime = userAircraft.getZuluTime().toMSecsSinceEpoch();
    qDebug() << Q_FUNC_INFO << "fuelFlowGroundspeedAverage->size()" << fuelFlowGroundspeedAverage->size();
#endif
  }

  // Update report every second
  qint64 currentSampleTime = QDateTime::currentMSecsSinceEpoch();
  if(currentSampleTime > currentReportLastSampleTimeMs + 1000)
  {
    currentReportLastSampleTimeMs = currentSampleTime;
    updateReportCurrent();
  }

  // Update current fuel report every second
  currentSampleTime = QDateTime::currentMSecsSinceEpoch();
  if(currentSampleTime > reportLastSampleTimeMs + 5000)
  {
    reportLastSampleTimeMs = currentSampleTime;
    updateReport();
  }
}

void AircraftPerfController::windBoxesChanged()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  // Start delayed wind update - calls windChangedDelayed()
  windChangeTimer.start(500);
}

void AircraftPerfController::windChangedDelayed()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  NavApp::getWindReporter()->updateManualRouteWinds();
  emit windChanged();
}

void AircraftPerfController::noPerfLoaded()
{
  // Invalidate performance file after an error - state "none loaded"
  perf->resetToDefault(QString());
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

void AircraftPerfController::tabVisibilityChanged()
{
  qDebug() << Q_FUNC_INFO;
  updateReport();
  updateReportCurrent();
}

bool AircraftPerfController::hasErrors() const
{
  return !errorTooltips.isEmpty();
}

QStringList AircraftPerfController::getErrorStrings() const
{
#ifdef DEBUG_INFORMATION
  if(!errorTooltips.isEmpty())
    qDebug() << Q_FUNC_INFO << errorTooltips;
#endif

  return errorTooltips;
}

void AircraftPerfController::updateMenuTooltips()
{
  fileHistory->updateMenuTooltips();
}
