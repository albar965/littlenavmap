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

#include "userdata/logdatacontroller.h"

#include "fs/userdata/logdatamanager.h"
#include "common/constants.h"
#include "sql/sqlrecord.h"
#include "navapp.h"
#include "route/routealtitude.h"
#include "route/route.h"
#include "atools.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "query/airportquery.h"
#include "userdata/logdatadialog.h"
#include "search/logdatasearch.h"
#include "settings/settings.h"
#include "common/maptypes.h"
#include "sql/sqltransaction.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "common/maptypesfactory.h"

#include <QDebug>
#include <QStandardPaths>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::geo::Pos;

LogdataController::LogdataController(atools::fs::userdata::LogdataManager *logdataManager, MainWindow *parent)
  : manager(logdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
}

LogdataController::~LogdataController()
{
  delete aircraftAtTakeoff;
  delete dialog;
}

void LogdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  ui->tabWidgetSearch->setCurrentIndex(si::SEARCH_LOG);
}

void LogdataController::saveState()
{
}

void LogdataController::restoreState()
{
}

void LogdataController::deleteLogEntryFromMap(int id)
{
  deleteLogEntries({id});
}

map::MapLogbookEntry LogdataController::getLogEntryById(int id)
{
  map::MapLogbookEntry obj;
  MapTypesFactory().fillLogbookEntry(manager->getRecord(id), obj);
  return obj;
}

atools::sql::SqlRecord LogdataController::getLogEntryRecordById(int id)
{
  return manager->getRecord(id);
}

void LogdataController::aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  createTakoffLanding(aircraft, true /*takeoff*/, 0.f, 0.f);
}

void LogdataController::aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                                        float averageTasKts)
{
  createTakoffLanding(aircraft, false /*takeoff*/, flownDistanceNm, averageTasKts);
}

void LogdataController::createTakoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff,
                                            float flownDistanceNm, float averageTasKts)
{
  Q_UNUSED(averageTasKts);

  if(NavApp::getMainUi()->actionLogdataCreateLogbook->isChecked())
  {
    // Get nearest airport on takeoff/landing and runway
    map::MapRunwayEnd runwayEnd;
    map::MapAirport airport;
    if(!NavApp::getAirportQuerySim()->getBestRunwayEndForPosAndCourse(runwayEnd, airport,
                                                                      aircraft.getPosition(),
                                                                      aircraft.getTrackDegTrue()))
    {
      // Not even an airport was found
      qWarning() << Q_FUNC_INFO << "No runway found for aircraft"
                 << aircraft.getPosition() << aircraft.getTrackDegTrue();
      return;
    }

    QString departureArrivalText = takeoff ? tr("Departure") : tr("Arrival");
    QString runwayText = runwayEnd.isValid() ? tr(" runway %1").arg(runwayEnd.name) : QString();

    if(takeoff)
    {
      // Build record for new logbook entry =======================================
      SqlRecord record = manager->getEmptyRecord();
      record.setValue("aircraft_name", aircraft.getAirplaneType()); // varchar(250),
      record.setValue("aircraft_type", aircraft.getAirplaneModel()); // varchar(250),
      record.setValue("aircraft_registration", aircraft.getAirplaneRegistration()); // varchar(50),
      record.setValue("flightplan_number", aircraft.getAirplaneFlightnumber()); // varchar(100),
      record.setValue("flightplan_cruise_altitude", NavApp::getRouteCruiseAltFt()); // integer,
      record.setValue("flightplan_file", NavApp::getCurrentRouteFilepath()); // varchar(1024),
      record.setValue("performance_file", NavApp::getCurrentAircraftPerfFilepath()); // varchar(1024),
      record.setValue("block_fuel", NavApp::getAltitudeLegs().getBlockFuel(NavApp::getAircraftPerformance())); // integer,
      record.setValue("trip_fuel", NavApp::getAltitudeLegs().getTripFuel()); // integer,
      record.setValue("grossweight", aircraft.getAirplaneTotalWeightLbs()); // integer,
      record.setValue("departure_ident", airport.ident); // varchar(10),
      record.setValue("departure_name", airport.name); // varchar(200),
      record.setValue("departure_runway", runwayEnd.name); // varchar(200),
      record.setValue("departure_lonx", airport.position.getLonX()); // integer,
      record.setValue("departure_laty", airport.position.getLatY()); // integer,
      record.setValue("departure_alt", airport.position.getAltitude()); // integer,
      record.setValue("departure_time", QDateTime::currentDateTime()); // varchar(100),
      record.setValue("departure_time_sim", aircraft.getZuluTime()); // varchar(100),
      record.setValue("simulator", NavApp::getCurrentSimulatorName()); // varchar(50),

      // Determine fuel type =========================
      float weightVolRatio = 0.f;
      bool jetfuel = aircraft.isJetfuel(weightVolRatio);
      if(weightVolRatio > 0.f)
        record.setValue("is_jetfuel", jetfuel); // integer,

      // Add to database and remember created id
      SqlTransaction transaction(manager->getDatabase());
      manager->insertByRecord(record, &logEntryId);
      transaction.commit();

      emit refreshLogSearch(false /* load all */, true /* keep selection */);
      emit logDataChanged();
      mainWindow->setStatusMessage(tr("Logbook Entry for %1 at %2%3 added.").
                                   arg(departureArrivalText).
                                   arg(airport.ident).
                                   arg(runwayText));
    }
    else if(logEntryId >= 0)
    {
      // Update takeoff record with landing data ===========================================
      atools::sql::SqlRecord record = manager->getRecord(logEntryId);
      record.setValue("distance", NavApp::getRoute().getTotalDistance()); // integer,
      record.setValue("distance_flown", flownDistanceNm); // integer,
      if(aircraftAtTakeoff != nullptr)
        record.setValue("used_fuel", aircraftAtTakeoff->getFuelTotalWeightLbs() - aircraft.getFuelTotalWeightLbs()); // integer,
      record.setValue("destination_ident", airport.ident); // varchar(10),
      record.setValue("destination_name", airport.name); // varchar(200),
      record.setValue("destination_runway", runwayEnd.name); // varchar(200),
      record.setValue("destination_lonx", airport.position.getLonX()); // integer,
      record.setValue("destination_laty", airport.position.getLatY()); // integer,
      record.setValue("destination_alt", airport.position.getAltitude()); // integer,
      record.setValue("destination_time", QDateTime::currentDateTime()); // varchar(100),
      record.setValue("destination_time_sim", aircraft.getZuluTime()); // varchar(100),

      // Determine fuel type again =========================
      float weightVolRatio = 0.f;
      bool jetfuel = aircraft.isJetfuel(weightVolRatio);
      if(weightVolRatio > 0.f)
        record.setValue("is_jetfuel", jetfuel); // integer,

      // record.setValue("plan_geometry", dummy); // blob,
      // record.setValue("trail_geometry", dummy); // blob

      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(record, {logEntryId});
      transaction.commit();

      emit refreshLogSearch(false /* load all */, false /* keep selection */);
      emit logDataChanged();
      mainWindow->setStatusMessage(tr("Logbook Entry for %1 at %2%3 updated.").
                                   arg(departureArrivalText).
                                   arg(airport.ident).
                                   arg(runwayText));

      logEntryId = -1;
    }
    else
      qWarning() << Q_FUNC_INFO << "no previous takeoff";
  }

  delete aircraftAtTakeoff;
  aircraftAtTakeoff = nullptr;

  if(takeoff)
    aircraftAtTakeoff = new atools::fs::sc::SimConnectUserAircraft(aircraft);
}

void LogdataController::editLogEntryFromMap(int id)
{
  qDebug() << Q_FUNC_INFO;
  editLogEntries({id});
}

void LogdataController::editLogEntries(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO << ids;

  SqlRecord rec = manager->getRecord(ids.first());
  if(!rec.isEmpty())
  {
    LogdataDialog dlg(mainWindow, ids.size() == 1 ? ld::EDIT_ONE : ld::EDIT_MULTIPLE);
    dlg.restoreState();

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(dlg.getRecord(), ids);
      transaction.commit();

      emit refreshLogSearch(false /* load all */, true /* keep selection */);
      emit logDataChanged();

      mainWindow->setStatusMessage(tr("%1 logbook %2 updated.").
                                   arg(ids.size()).arg(ids.size() == 1 ? tr("entry") : tr("entries")));
    }
    dlg.saveState();
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void LogdataController::addLogEntry()
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getEmptyRecord();

  qDebug() << Q_FUNC_INFO << rec;

  LogdataDialog dlg(mainWindow, ld::ADD);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    qDebug() << Q_FUNC_INFO << rec;

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertByRecord(dlg.getRecord());
    transaction.commit();

    emit refreshLogSearch(false /* load all */, false /* keep selection */);
    emit logDataChanged();
    mainWindow->setStatusMessage(tr("Logbook entry added."));
  }
  dlg.saveState();
}

void LogdataController::deleteLogEntries(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  QString txt = ids.size() == 1 ? tr("entry") : tr("entries");
  int retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Delete %1 logbook %2?").arg(ids.size()).arg(txt) +
                          tr("\n\nThis cannot be undone."), QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

  if(retval == QMessageBox::Yes)
  {
    SqlTransaction transaction(manager->getDatabase());
    manager->removeRows(ids);
    transaction.commit();

    emit refreshLogSearch(false /* load all */, false /* keep selection */);
    emit logDataChanged();
    mainWindow->setStatusMessage(tr("%1 logbook %2 deleted.").
                                 arg(ids.size()).
                                 arg(txt));
  }
}

void LogdataController::importXplane()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
    if(xpBasePath.isEmpty())
      xpBasePath = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
    else
      xpBasePath = atools::buildPathNoCase({xpBasePath, "Output", "logbooks"});

    QString file = dialog->openFileDialog(
      tr("Open X-Plane Logbook File"),
      tr("X-Plane Logbook Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_XPLANE_LOGBOOK), "Logdata/XPlane",
      xpBasePath);

    int numImported = 0;
    if(!file.isEmpty())
    {
      numImported += manager->importXplane(file, fetchAirportCoordinates);
      mainWindow->setStatusMessage(tr("Imported %1 %2 X-Plane logbook.").arg(numImported).
                                   arg(numImported == 1 ? tr("entry") : tr("entries")));
      emit refreshLogSearch(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString file = dialog->openFileDialog(
      tr("Open Logbook CSV File"),
      tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), "Logdata/Csv");

    int numImported = 0;
    if(!file.isEmpty())
    {
      numImported += manager->importCsv(file);
      mainWindow->setStatusMessage(tr("Imported %1 %2 from CSV file.").arg(numImported).
                                   arg(numImported == 1 ? tr("entry") : tr("entries")));
      emit refreshLogSearch(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString file = dialog->saveFileDialog(
      tr("Export Logbook Entry CSV File"),
      tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV),
      ".csv",
      "Logdata/Csv");

    if(!file.isEmpty())
    {
      int numExported = manager->exportCsv(file);
      mainWindow->setStatusMessage(tr("%1 logbook %2 exported.").
                                   arg(numExported).arg(numExported == 1 ? tr("entry") : tr("entries")));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::fetchAirportCoordinates(atools::geo::Pos& pos, QString& name, const QString& airportIdent)
{
  map::MapAirport airport = NavApp::getAirportQuerySim()->getAirportByIdent(airportIdent);

  if(airport.isValid())
  {
    pos = airport.position;
    name = airport.name;
  }
}
