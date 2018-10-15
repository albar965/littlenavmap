/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "util/htmlbuilder.h"
#include "gui/filehistoryhandler.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "navapp.h"
#include "route/routealtitude.h"
#include "gui/widgetstate.h"

#include <QDebug>
#include <QUrlQuery>

using atools::fs::perf::AircraftPerf;

AircraftPerfController::AircraftPerfController(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Remember original font for resizing in options
  infoFontPtSize = static_cast<float>(ui->textBrowserAircraftPerformanceReport->font().pointSizeF());

  fileHistory = new atools::gui::FileHistoryHandler(this, lnm::AIRCRAFT_PERF_FILENAMESRECENT,
                                                    ui->menuAircraftPerformanceRecent,
                                                    ui->actionAircraftPerformanceClearMenu);

  connect(fileHistory, &atools::gui::FileHistoryHandler::fileSelected,

          this, &AircraftPerfController::loadFile);
  connect(ui->textBrowserAircraftPerformanceReport, &QTextBrowser::anchorClicked,
          this, &AircraftPerfController::anchorClicked);

  connect(&windChangeTimer, &QTimer::timeout, this, &AircraftPerfController::windChangedDelayed);
  windChangeTimer.setSingleShot(true);
}

AircraftPerfController::~AircraftPerfController()
{
  windChangeTimer.stop();
  delete fileHistory;
  delete perf;
}

void AircraftPerfController::create()
{
  qDebug() << Q_FUNC_INFO;

  if(checkForChanges())
  {
    delete perf;
    perf = new AircraftPerf();
    currentFilepath.clear();
    changed = false;

    AircraftPerfDialog dialog(mainWindow, *perf);
    if(dialog.exec() == QDialog::Accepted)
    {
      *perf = dialog.getAircraftPerf();
      changed = true;
      windChangeTimer.stop();
      NavApp::setStatusMessage(tr("Aircraft performance created."));
    }
    else
      noPerfLoaded();
  }

  updateActionStates();
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
        delete perf;
        perf = new AircraftPerf();

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
        delete perf;
        perf = new AircraftPerf();

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

  updateActionStates();
  updateReport();
  return retval;
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
}

void AircraftPerfController::helpClicked()
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
}

void AircraftPerfController::routeAltitudeChanged(float altitudeFeet)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(altitudeFeet);
  updateReport();
}

float AircraftPerfController::getRouteCruiseSpeedKts()
{
  // Use default if nothing loaded
  return perf != nullptr ? perf->getCruiseSpeed() : atools::fs::perf::DEFAULT_SPEED;
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

  ui->spinBoxAircraftPerformanceWindDirection->setEnabled(perf != nullptr);
  ui->spinBoxAircraftPerformanceWindSpeed->setEnabled(perf != nullptr);
  ui->actionAircraftPerformanceSave->setEnabled(perf != nullptr && !currentFilepath.isEmpty());
  ui->actionAircraftPerformanceSaveAs->setEnabled(perf != nullptr);
  ui->actionAircraftPerformanceEdit->setEnabled(perf != nullptr);
  ui->pushButtonAircraftPerformanceEdit->setEnabled(perf != nullptr);
  ui->pushButtonAircraftPerformanceSave->setEnabled(perf != nullptr && !currentFilepath.isEmpty());
  ui->pushButtonAircraftPerformanceSaveAs->setEnabled(perf != nullptr && !currentFilepath.isEmpty());
}

void AircraftPerfController::updateReport()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Update tab title to indicate change ========================================
  if(changed)
  {
    if(!ui->tabWidgetRoute->tabText(1).endsWith(tr(" *")))
      ui->tabWidgetRoute->setTabText(1, ui->tabWidgetRoute->tabText(1) + tr(" *"));
  }
  else
    ui->tabWidgetRoute->setTabText(1, ui->tabWidgetRoute->tabText(1).replace(tr(" *"), QString()));

  // Write HTML report ================================================================
  atools::util::HtmlBuilder html(true /* background color */);

  if(perf != nullptr)
  {
    // Icon, name and aircraft type =======================================================
    html.img(QIcon(":/littlenavmap/resources/icons/aircraftperf.svg"), QString(), QString(),
             QSize(symbolSize, symbolSize));
    html.nbsp().nbsp();

    html.text(tr("%1 - %2").arg(perf->getName()).arg(perf->getAircraftType()),
              atools::util::html::BOLD | atools::util::html::BIG);

    const RouteAltitude& altitudeLegs = NavApp::getAltitudeLegs();
    if(altitudeLegs.hasUnflyableLegs())
    {
      html.p(tr("<span style=\"background-color: #ff0000; color: #ffffff; font-weight:bold;\">"
                  "Flight plan has unflyable legs where head wind is larger than cruise speed.</span>"),
             atools::util::html::NO_ENTITIES);
    }
    else
    {
      // Flight data =======================================================
      html.p().b(tr("Flight")).pEnd();
      html.table();
      html.row2(tr("Distance and Time:"), tr("%1, %2").
                arg(Unit::distNm(altitudeLegs.getTotalDistance())).
                arg(formatter::formatMinutesHoursLong(altitudeLegs.getTravelTimeHours())));
      html.row2(tr("Average Ground Speed:"), Unit::speedKts(altitudeLegs.getAverageGroundSpeed()));
      html.row2(tr("True Airspeed at Cruise:"), Unit::speedKts(perf->getCruiseSpeed()));

      float mach = atools::geo::tasToMachFromAlt(altitudeLegs.getCruiseAltitide(),
                                                 static_cast<float>(perf->getCruiseSpeed()));
      if(mach > 0.4f)
        html.row2(tr("Mach at cruise:"), QLocale().toString(mach, 'f', 2));
      html.tableEnd();

      // Fuel data =======================================================
      atools::util::html::Flags flags = atools::util::html::ALIGN_RIGHT;
      html.p().b(tr("Fuel")).pEnd();
      html.table();
      bool fuelAsVol = perf->getFuelUnit() == atools::fs::perf::VOLUME;
      float tripFuel = altitudeLegs.getTripFuel();
      html.row2(tr("Trip Fuel:"), Unit::fuelLbsGallon(tripFuel, true, fuelAsVol),
                atools::util::html::BOLD | flags);
      float blockFuel = atools::roundToInt(altitudeLegs.getTripFuel() * perf->getContingencyFuelFactor()) +
                        perf->getTaxiFuel() + perf->getExtraFuel() + perf->getReserveFuel();
      html.row2(tr("Block Fuel:"), Unit::fuelLbsGallon(blockFuel, true, fuelAsVol),
                atools::util::html::BOLD | flags);
      float destFuel = blockFuel - tripFuel - perf->getTaxiFuel();
      html.row2(tr("Fuel at Destination:"), Unit::fuelLbsGallon(destFuel, true, fuelAsVol), flags);
      html.row2(tr("Reserve Fuel:"), Unit::fuelLbsGallon(perf->getReserveFuel(), true, fuelAsVol), flags);
      html.row2(tr("Taxi Fuel:"), Unit::fuelLbsGallon(perf->getTaxiFuel(), true, fuelAsVol), flags);
      html.row2(tr("Extra Fuel:"), Unit::fuelLbsGallon(perf->getExtraFuel(), true, fuelAsVol), flags);
      html.row2(tr("Contingency Fuel:"), tr("%1 %, %2").
                arg(perf->getContingencyFuel()).
                arg(Unit::fuelLbsGallon(altitudeLegs.getTripFuel() * (perf->getContingencyFuelFactor() - 1.f)),
                    fuelAsVol), flags);
      html.tableEnd();

      // Climb and descent phases =======================================================
      html.p().b(tr("Climb and Descent")).pEnd();
      html.table();
      html.row2(tr("Climb:"), tr("%1 at %2, %3° flight path angle").
                arg(Unit::speedVertFpm(perf->getClimbVertSpeed())).
                arg(Unit::speedKts(perf->getClimbSpeed())).
                arg(QLocale().toString(perf->getClimbFlightPathAngle(), 'f', 1)));
      html.row2(tr("Descent:"), tr("%1 at %2, %3° flight path angle").
                arg(Unit::speedVertFpm(perf->getDescentVertSpeed())).
                arg(Unit::speedKts(perf->getDescentSpeed())).
                arg(QLocale().toString(perf->getDescentFlightPathAngle(), 'f', 1)));
      html.row2(tr("Descent Rule:"), tr("%1 %2 per %3 %4").
                arg(Unit::altFeetF(1.f / perf->getDescentVertSpeed() * 1000.f), 0, 'f', 1).
                arg(Unit::getUnitDistStr()).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr()));
      html.tableEnd();
    }

    // Description and file =======================================================
    html.hr();
    html.p().b(tr("Performance File Description")).pEnd();
    html.table();
    html.row2(QString(), perf->getDescription(), atools::util::html::AUTOLINK);
    html.tableEnd();

    html.p().b(tr("Performance File")).pEnd();
    html.table();

    // Show link inactive if file does not exist
    atools::util::HtmlBuilder link(html.cleared());
    if(QFileInfo::exists(currentFilepath))
      link.a(currentFilepath, QString("lnm://show?filepath=%1").arg(currentFilepath), atools::util::html::LINK_NO_UL);
    else
      link.text(currentFilepath);

    html.row2(QString(), link.getHtml(), atools::util::html::NO_ENTITIES | atools::util::html::SMALL);
    html.tableEnd();
  }

  atools::gui::util::updateTextEdit(ui->textBrowserAircraftPerformanceReport, html.getHtml(),
                                    false /*scrollToTop*/, true /* keep selection */);
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
  delete perf;
  perf = nullptr;
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
