/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "logbook/logdatacontroller.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/unit.h"
#include "db/undoredoprogress.h"
#include "exception.h"
#include "fs/gpx/gpxio.h"
#include "fs/perf/aircraftperf.h"
#include "fs/pln/flightplanio.h"
#include "fs/userdata/logdatamanager.h"
#include "fs/util/coordinates.h"
#include "geo/aircrafttrail.h"
#include "geo/calculations.h"
#include "gui/choicedialog.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/errorhandler.h"
#include "gui/helphandler.h"
#include "gui/mainwindow.h"
#include "gui/sqlquerydialog.h"
#include "gui/textdialog.h"
#include "logbook/logdataconverter.h"
#include "logbook/logdatadialog.h"
#include "logbook/logstatisticsdialog.h"
#include "options/optiondata.h"
#include "perf/aircraftperfcontroller.h"
#include "query/airportquery.h"
#include "query/querymanager.h"
#include "route/route.h"
#include "route/routealtitude.h"
#include "search/logdatasearch.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"
#include "sql/sqlcolumn.h"
#include "sql/sqlrecord.h"
#include "sql/sqltransaction.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "zip/gzip.h"

#include <QDebug>
#include <QStandardPaths>
#include <QStringBuilder>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::sql::SqlColumn;
using atools::geo::Pos;
using atools::fs::pln::FlightplanIO;

LogdataController::LogdataController(atools::fs::userdata::LogdataManager *logdataManager, MainWindow *parent)
  : manager(logdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);

  // Do not use a parent to allow the window moving to back
  statsDialog = new LogStatisticsDialog(nullptr, this);

  // Add to dock handler to enable auto raise and closing on exit
  NavApp::addDialogToDockHandler(statsDialog);

  connect(this, &LogdataController::logDataChanged, statsDialog, &LogStatisticsDialog::logDataChanged);
  connect(this, &LogdataController::logDataChanged, manager, &atools::sql::DataManagerBase::updateUndoRedoActions);

  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionSearchLogdataUndo, &QAction::triggered, this, &LogdataController::undoTriggered);
  connect(ui->actionSearchLogdataRedo, &QAction::triggered, this, &LogdataController::redoTriggered);

  manager->setMaximumUndoSteps(50);
  manager->setTextSuffix(tr("Logbook Entry", "Log singular"), tr("Logbook Entries", "Log plural"));
  manager->setActions(ui->actionSearchLogdataUndo, ui->actionSearchLogdataRedo);
}

LogdataController::~LogdataController()
{
  NavApp::removeDialogFromDockHandler(statsDialog);
  delete statsDialog;
  delete aircraftAtTakeoff;
  delete dialog;
}

void LogdataController::undoTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(manager->canUndo())
  {
    SqlTransaction transaction(manager->getDatabase());
    UndoRedoProgress progress(mainWindow, tr("Little Navmap - Undoing Logbook changes"), tr("Undoing changes ..."));
    manager->setProgressCallback(std::bind(&UndoRedoProgress::callback, &progress, std::placeholders::_1, std::placeholders::_2));

    progress.start();
    manager->undo();
    progress.stop();

    manager->setProgressCallback(nullptr);
    if(!progress.isCanceled())
    {
      qDebug() << Q_FUNC_INFO << "Committing";
      transaction.commit();
      manager->clearGeometryCache();
      aircraftTrailCache.clear();

      emit refreshLogSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      emit logDataChanged();
    }
    else
    {
      qDebug() << Q_FUNC_INFO << "Rolling back";
      transaction.rollback();
    }
  }
}

void LogdataController::redoTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(manager->canRedo())
  {
    SqlTransaction transaction(manager->getDatabase());
    UndoRedoProgress progress(mainWindow, tr("Little Navmap - Redoing Logbook changes"), tr("Redoing changes ..."));
    manager->setProgressCallback(std::bind(&UndoRedoProgress::callback, &progress, std::placeholders::_1, std::placeholders::_2));

    progress.start();
    manager->redo();
    progress.stop();

    manager->setProgressCallback(nullptr);
    if(!progress.isCanceled())
    {
      qDebug() << Q_FUNC_INFO << "Committing";
      transaction.commit();
      manager->clearGeometryCache();
      aircraftTrailCache.clear();

      emit refreshLogSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      emit logDataChanged();
    }
    else
    {
      qDebug() << Q_FUNC_INFO << "Rolling back";
      transaction.rollback();
    }
  }
}

void LogdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_LOG);
}

void LogdataController::saveState() const
{
  saveLogEntryId();
}

void LogdataController::restoreState()
{
  restoreLogEntryId();
  manager->updateUndoRedoActions();
}

void LogdataController::resetWindowLayout()
{
  statsDialog->resetWindowLayout();
}

void LogdataController::optionsChanged()
{
  statsDialog->optionsChanged();
}

void LogdataController::saveLogEntryId() const
{
  atools::settings::Settings::instance().setValue(lnm::LOGDATA_ENTRY_ID, logEntryId);
}

void LogdataController::restoreLogEntryId()
{
  logEntryId = atools::settings::Settings::instance().valueInt(lnm::LOGDATA_ENTRY_ID);
}

void LogdataController::fontChanged(const QFont& font)
{
  statsDialog->fontChanged(font);
}

void LogdataController::styleChanged()
{
  statsDialog->styleChanged();
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

void LogdataController::getFlightStatsTime(QDateTime& earliest, QDateTime& latest, QDateTime& earliestSim,
                                           QDateTime& latestSim)
{
  manager->getFlightStatsTime(earliest, latest, earliestSim, latestSim);
}

void LogdataController::getFlightStatsDistance(float& distTotal, float& distMax, float& distAverage)
{
  manager->getFlightStatsDistance(distTotal, distMax, distAverage);
}

void LogdataController::getFlightStatsAirports(int& numDepartAirports, int& numDestAirports)
{
  manager->getFlightStatsAirports(numDepartAirports, numDestAirports);
}

void LogdataController::getFlightStatsTripTime(float& timeMaximum, float& timeAverage, float& timeTotal,
                                               float& timeMaximumSim, float& timeAverageSim, float& timeTotalSim)
{
  manager->getFlightStatsTripTime(timeMaximum, timeAverage, timeTotal, timeMaximumSim, timeAverageSim, timeTotalSim);
}

void LogdataController::getFlightStatsAircraft(int& numTypes, int& numRegistrations, int& numNames, int& numSimulators)
{
  manager->getFlightStatsAircraft(numTypes, numRegistrations, numNames, numSimulators);
}

void LogdataController::getFlightStatsSimulator(QVector<std::pair<int, QString> >& numSimulators)
{
  manager->getFlightStatsSimulator(numSimulators);
}

void LogdataController::statisticsLogbookShow()
{
  statsDialog->show();
  statsDialog->raise();
  statsDialog->activateWindow();
}

atools::sql::SqlDatabase *LogdataController::getDatabase() const
{
  return manager->getDatabase();
}

void LogdataController::aircraftHasPassedTakeoffPoint(const atools::fs::sc::SimConnectUserAircraft&)
{
  aircraftPassedTakeoffPoint = true;
}

void LogdataController::aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  aircraftPassedTakeoffPoint = true;
  createTakeoffLanding(aircraft, true /*takeoff*/, 0.f);
}

void LogdataController::aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm)
{
  aircraftPassedTakeoffPoint = true;
  createTakeoffLanding(aircraft, false /*takeoff*/, flownDistanceNm);
}

void LogdataController::createTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff, float flownDistanceNm)
{
  if(NavApp::getMainUi()->actionLogdataCreateLogbook->isChecked())
  {
    // Get nearest airport on takeoff/landing and runway ==============================================
    map::MapRunwayEnd runwayEnd;
    map::MapAirport airport;
    if(!QueryManager::instance()->getQueriesGui()->getAirportQuerySim()->getBestRunwayEndForPosAndCourse(runwayEnd, airport,
                                                                                                         aircraft.getPosition(),
                                                                                                         aircraft.getTrackDegTrue(),
                                                                                                         aircraft.isHelicopter()))
    {
      // Not even an airport was found - log but continue anyway
      qWarning() << Q_FUNC_INFO << "No airport found near aircraft at" << (takeoff ? "takeoff" : "landing")
                 << aircraft.getPosition() << aircraft.getTrackDegTrue();

      // Log warning message in status bar but continue
      NavApp::setStatusMessage(tr("No airport found near aircraft after detecting %1.").arg(takeoff ? tr("takeoff") : tr("landing")),
                               true /* addToLog */);
    }

    // Get last record if there is already one ==============================================
    atools::sql::SqlRecord record;
    if(logEntryId >= 0)
      record = manager->getRecord(logEntryId);

    if(takeoff)
    {
      // Record takeoff ==============================================
      if(recordIsTakeoff(record) && doesFlightMatchRecord(record, aircraft))
        // There is already a takeoff and the flight matches by aircraft type and simulator
        NavApp::setStatusMessage(tr("Ignoring takeoff since last logbook entry already recorded one."), true /* addToLog */);
      else
        // Always create a new record and a new logEntryId on takeoff
        recordTakeoff(aircraft, airport, runwayEnd);
    }
    else if(!record.isEmpty())
    {
      // Record landing ==============================================
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << record;
#endif

      if(doesFlightMatchRecord(record, aircraft) && recordIsTakeoff(record))
        // Found previous record without landing and aircraft and sim are still the same - update previous record with landing
        recordLanding(record, aircraft, airport, runwayEnd, flownDistanceNm);
      else
        NavApp::setStatusMessage(tr("Previous logbook entry with takeoff does not match current flight."), true /* addToLog */);
    }
    else
      NavApp::setStatusMessage(tr("Previous takeoff logbook entry not found for landing."), true /* addToLog */);
  }

  qInfo() << Q_FUNC_INFO << "takeoff" << takeoff << "logEntryId" << logEntryId;
}

bool LogdataController::doesFlightMatchRecord(const atools::sql::SqlRecord& record, const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  return record.valueStr("aircraft_type", QString()) == aircraft.getAirplaneModel() &&
         record.valueStr("simulator", QString()) == NavApp::getCurrentSimulatorShortName();
}

bool LogdataController::recordIsTakeoff(const atools::sql::SqlRecord& record)
{
  // Requires not empty record, departure set and destination empty
  return !record.isEmpty() &&
         !record.valueStr("departure_ident", QString()).isEmpty() &&
         record.valueStr("destination_ident", QString()).isEmpty();
}

void LogdataController::recordTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft, const map::MapAirport& airport,
                                      const map::MapRunwayEnd& runwayEnd)
{
  // Build record for new logbook entry =======================================
  SqlRecord record = manager->getEmptyRecord();

  record.setNull("logbook_id"); // Set to null to avoid unique constraint mismatches - select id automatically
  record.setValue("aircraft_name", aircraft.getAirplaneType()); // varchar(250),
  record.setValue("aircraft_type", aircraft.getAirplaneModel()); // varchar(250),
  record.setValue("aircraft_registration", aircraft.getAirplaneRegistration()); // varchar(50),
  record.setValue("flightplan_number", aircraft.getAirplaneFlightnumber()); // varchar(100),
  record.setValue("flightplan_cruise_altitude", NavApp::getRouteCruiseAltitudeFt()); // integer,
  record.setValue("flightplan_file", atools::nativeCleanPath(NavApp::getCurrentRouteFilePath())); // varchar(1024),
  record.setValue("performance_file", atools::nativeCleanPath(NavApp::getCurrentAircraftPerfFilePath())); // varchar(1024),
  record.setValue("block_fuel", NavApp::getAltitudeLegs().getBlockFuel(NavApp::getAircraftPerformance())); // integer,
  record.setValue("trip_fuel", NavApp::getAltitudeLegs().getTripFuel()); // integer,
  record.setValue("grossweight", aircraft.getAirplaneTotalWeightLbs()); // integer,

  QString ident = recordAirportOrCoordinates(record, aircraft, airport, runwayEnd, "departure");

  // Sqlite TEXT as ISO8601 strings (2021-05-16T23:55:00.259+02:00).
  // TEXT as ISO8601 strings ("YYYY-MM-DD HH:MM:SS.SSS")
  record.setValue("departure_time", atools::currentIsoWithOffset()); // varchar(100),

  // 2021-05-16T21:50:24.973Z
  record.setValue("departure_time_sim", aircraft.getZuluTime()); // varchar(100),

  record.setValue("simulator", NavApp::getCurrentSimulatorShortName()); // varchar(50),
  record.setValue("route_string", NavApp::getRouteStringLogbook()); // varchar(1024),

  // Clear separate logbook track =========================
  NavApp::deleteAircraftTrailLogbook();

  // Record flight plan and aircraft performance =========================
  recordFlightplanAndPerf(record);

  // Determine fuel type =========================
  float weightVolRatio = 0.f;
  bool jetfuel = aircraft.isJetfuel(weightVolRatio);
  if(weightVolRatio > 0.f)
    record.setValue("is_jetfuel", jetfuel); // integer,

  // Add to database and remember created id
  SqlTransaction transaction(manager->getDatabase());
  manager->insertOneRecord(record);
  transaction.commit();

  logEntryId = manager->getCurrentId();
  saveLogEntryId();

  logChanged(false /* load all */, true /* keep selection */);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << record;
#endif

  aircraftAtTakeoff = new atools::fs::sc::SimConnectUserAircraft(aircraft);

  NavApp::setStatusMessage(tr("Logbook entry for %1 at %2%3 added.").arg(tr("departure")).arg(ident).
                           arg(runwayEnd.isValid() ? tr(" runway %1").arg(runwayEnd.name) : QString()), true /* addToLog */);
}

void LogdataController::recordLanding(atools::sql::SqlRecord& record, const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                      const map::MapAirport& airport, const map::MapRunwayEnd& runwayEnd, float flownDistanceNm)
{
  // Update takeoff record with landing data ===========================================
  record.setValue("distance", NavApp::getRouteConst().getTotalDistance()); // integer,
  record.setValue("distance_flown", flownDistanceNm); // integer,
  if(aircraftAtTakeoff != nullptr)
    record.setValue("used_fuel", aircraftAtTakeoff->getFuelTotalWeightLbs() - aircraft.getFuelTotalWeightLbs()); // integer,

  QString ident = recordAirportOrCoordinates(record, aircraft, airport, runwayEnd, "destination");

  QDateTime destinationTime = QDateTime::fromString(atools::currentIsoWithOffset(), Qt::ISODateWithMs);
  QDateTime destinationTimeSim = aircraft.getZuluTime();

  // Adapt times that are a result of wrong time jumps/warps resulting in one day offset
  // If the destinationTime is earlier than departure_time, the value returned is negative.
  if(record.valueDateTime("departure_time").secsTo(destinationTime) < 0)
    destinationTime = destinationTime.addDays(1);

  // If the destinationTimeSim is earlier than departure_time_sim, the value returned is negative.
  if(record.valueDateTime("departure_time_sim").secsTo(destinationTimeSim) < 0)
    destinationTimeSim = destinationTimeSim.addDays(1);

  // Sqlite TEXT as ISO8601 strings (2021-05-16T23:55:00.259+02:00).
  // TEXT as ISO8601 strings ("YYYY-MM-DD HH:MM:SS.SSS")
  record.setValue("destination_time", destinationTime); // varchar(100),

  // 2021-05-16T21:50:24.973Z
  record.setValue("destination_time_sim", destinationTimeSim); // varchar(100),

  // Update flight plan and aircraft performance =========================
  recordFlightplanAndPerf(record);

  // Save GPX with simplified flight plan and trail =========================
  const atools::fs::pln::Flightplan flightplan =
    NavApp::getRouteConst().updatedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_GPX).getFlightplanConst();
  record.setValue("aircraft_trail", atools::fs::gpx::GpxIO().saveGpxGz(NavApp::getAircraftTrailLogbook().toGpxData(flightplan)));

  // Clear separate logbook track =========================
  NavApp::deleteAircraftTrailLogbook();

  // Determine fuel type again =========================
  float weightVolRatio = 0.f;
  bool jetfuel = aircraft.isJetfuel(weightVolRatio);
  if(weightVolRatio > 0.f)
    record.setValue("is_jetfuel", jetfuel); // integer,

  SqlTransaction transaction(manager->getDatabase());
  manager->updateRecords(record, {logEntryId});
  transaction.commit();

  logChanged(false /* load all */, false /* keep selection */);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << record;
#endif

  NavApp::setStatusMessage(tr("Logbook entry for %1 at %2%3 updated.").
                           arg(tr("arrival")).
                           arg(ident).
                           arg(runwayEnd.isValid() ? tr(" runway %1").arg(runwayEnd.name) : QString()), true /* addToLog */);

  logEntryId = -1;
  saveLogEntryId();
}

QString LogdataController::recordAirportOrCoordinates(atools::sql::SqlRecord& record,
                                                      const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                                      const map::MapAirport& airport, const map::MapRunwayEnd& runwayEnd,
                                                      const QString& prefix)
{
  QString ident;
  if(airport.isValid())
  {
    // Record airport ==================================
    ident = airport.displayIdent();
    record.setValue(prefix % "_ident", ident); // varchar(10),
    record.setValue(prefix % "_name", airport.name); // varchar(200),
    record.setValue(prefix % "_lonx", airport.position.getLonX()); // integer,
    record.setValue(prefix % "_laty", airport.position.getLatY()); // integer,
    record.setValue(prefix % "_alt", airport.position.getAltitude()); // integer,

    if(runwayEnd.isValid())
      record.setValue(prefix % "_runway", runwayEnd.name); // varchar(200),
  }
  else
  {
    // Record off airport coordinates ==================================
    ident = atools::fs::util::toDegMinFormat(aircraft.getPosition());
    record.setValue(prefix % "_ident", ident); // varchar(10),
    record.setValue(prefix % "_lonx", aircraft.getPosition().getLonX()); // integer,
    record.setValue(prefix % "_laty", aircraft.getPosition().getLatY()); // integer,
    record.setValue(prefix % "_alt", aircraft.getActualAltitudeFt()); // integer,
  }
  return ident;
}

void LogdataController::logChanged(bool loadAll, bool keepSelection)
{
  // Clear cache and update map screen index
  manager->clearGeometryCache();
  aircraftTrailCache.clear();
  manager->updateUndoRedoActions();

  emit logDataChanged();

  // Reload search
  emit refreshLogSearch(loadAll, keepSelection, true /* force */);
}

void LogdataController::recordFlightplanAndPerf(atools::sql::SqlRecord& record)
{
  atools::fs::pln::Flightplan fp = NavApp::getRouteConst().
                                   updatedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_LNMPLN).getFlightplanConst();

  if(fp.isEmpty())
    record.setNull("flightplan"); // no plan
  else
    record.setValue("flightplan", FlightplanIO().saveLnmGz(fp)); // blob

  record.setValue("aircraft_perf", NavApp::getAircraftPerformance().saveXmlGz()); // blob
}

void LogdataController::resetTakeoffLandingDetection()
{
  ATOOLS_DELETE_LOG(aircraftAtTakeoff);
  aircraftPassedTakeoffPoint = false;
  logEntryId = -1;
  saveLogEntryId();
}

bool LogdataController::isDirectPreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowDirect->isChecked();
}

bool LogdataController::isRoutePreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowRoute->isChecked();
}

bool LogdataController::isTrackPreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowTrack->isChecked();
}

bool LogdataController::hasRouteAttached(int id)
{
  return manager->hasRouteAttached(id);
}

bool LogdataController::hasPerfAttached(int id)
{
  return manager->hasPerfAttached(id);
}

bool LogdataController::hasTrackAttached(int id)
{
  return manager->hasTrackAttached(id);
}

void LogdataController::preDatabaseLoad()
{
  // no-op
}

void LogdataController::postDatabaseLoad()
{
  manager->clearGeometryCache();
  aircraftTrailCache.clear();
}

void LogdataController::displayOptionsChanged()
{
  manager->clearGeometryCache();
}

const atools::fs::gpx::GpxData *LogdataController::getGpxData(int id)
{
  return manager->getGpxData(id);
}

const AircraftTrail *LogdataController::getAircraftTrail(int id)
{
  AircraftTrail *aircraftTrail = aircraftTrailCache.object(id);
  if(aircraftTrail == nullptr)
  {
    aircraftTrail = new AircraftTrail();
    aircraftTrail->fillTrailFromGpxData(*getGpxData(id));
    aircraftTrailCache.insert(id, aircraftTrail);
  }
  return aircraftTrail;
}

void LogdataController::editLogEntryFromMap(int id)
{
  qDebug() << Q_FUNC_INFO;

  editLogEntries({id});
}

void LogdataController::connectDialogSignals(LogdataDialog *dialogParam)
{
  connect(dialogParam, &LogdataDialog::planOpen, this, &LogdataController::planOpen);
  connect(dialogParam, &LogdataDialog::planAdd, this, &LogdataController::planAdd);
  connect(dialogParam, &LogdataDialog::planSaveAs, this, &LogdataController::planSaveAs);
  connect(dialogParam, &LogdataDialog::gpxAdd, this, &LogdataController::gpxAdd);
  connect(dialogParam, &LogdataDialog::gpxSaveAs, this, &LogdataController::gpxSaveAs);
  connect(dialogParam, &LogdataDialog::perfOpen, this, &LogdataController::perfOpen);
  connect(dialogParam, &LogdataDialog::perfAdd, this, &LogdataController::perfAdd);
  connect(dialogParam, &LogdataDialog::perfSaveAs, this, &LogdataController::perfSaveAs);
}

void LogdataController::editLogEntries(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO << ids;

  if(ids.isEmpty())
    return;

  SqlRecord rec = manager->getRecord(ids.constFirst());
  if(!rec.isEmpty())
  {
    LogdataDialog dlg(mainWindow, ids.size() == 1 ? ld::EDIT_ONE : ld::EDIT_MULTIPLE);
    connectDialogSignals(&dlg);
    dlg.restoreState();

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << rec;
    qDebug() << Q_FUNC_INFO << ids;
#endif

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlTransaction transaction(manager->getDatabase());
      manager->updateRecords(dlg.getRecord(), QSet<int>(ids.constBegin(), ids.constEnd()));
      transaction.commit();

      logChanged(false /* load all */, true /* keep selection */);

      NavApp::setStatusMessage(tr("%1 logbook %2 updated.").arg(ids.size()).arg(ids.size() == 1 ? tr("entry") : tr("entries")));
    }
    dlg.saveState();
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void LogdataController::prefillLogEntry(atools::sql::SqlRecord& rec)
{
  const Route& route = NavApp::getRouteConst();

  const atools::fs::sc::SimConnectUserAircraft& aircraft = NavApp::getUserAircraft();
  if(aircraft.isValid())
  {
    rec.setValue("aircraft_name", aircraft.getAirplaneType()); // varchar(250),
    rec.setValue("aircraft_type", aircraft.getAirplaneModel()); // varchar(250),
    rec.setValue("aircraft_registration", aircraft.getAirplaneRegistration()); // varchar(50),
    rec.setValue("flightplan_number", aircraft.getAirplaneFlightnumber()); // varchar(100),
  }

  rec.setValue("aircraft_type", NavApp::getAircraftPerformance().getAircraftType());
  rec.setValue("simulator", NavApp::getCurrentSimulatorName());
  rec.setValue("route_string", NavApp::getRouteStringLogbook());
  rec.setValue("flightplan_cruise_altitude", NavApp::getRouteCruiseAltitudeFt());
  rec.setValue("block_fuel", NavApp::getAircraftPerfController()->getBlockFuel());
  rec.setValue("trip_fuel", NavApp::getAircraftPerfController()->getTripFuel());
  rec.setValue("is_jetfuel", NavApp::getAircraftPerfController()->getAircraftPerformance().isJetFuel());
  rec.setValue("distance", route.getTotalDistance());

  // Departure information =============================================
  QString departRw = route.getDepartureRunwayName(), destRw = route.getArrivalRunwayName();
  if(route.hasValidDeparture())
  {
    const RouteLeg& departureAirportLeg = route.getDepartureAirportLeg();
    rec.setValue("departure_ident", departureAirportLeg.getIdent());
    rec.setValue("departure_name", departureAirportLeg.getName());
    rec.setValue("departure_runway", departRw);
    rec.setValue("departure_lonx", departureAirportLeg.getPosition().getLonX());
    rec.setValue("departure_laty", departureAirportLeg.getPosition().getLatY());
    rec.setValue("departure_alt", departureAirportLeg.getAltitude()); // integer,
  }

  // Destination information =============================================
  if(route.hasValidDestination())
  {
    const RouteLeg& destinationAirportLeg = route.getDestinationAirportLeg();
    rec.setValue("destination_ident", destinationAirportLeg.getIdent());
    rec.setValue("destination_name", destinationAirportLeg.getName());
    rec.setValue("destination_runway", destRw);
    rec.setValue("destination_lonx", destinationAirportLeg.getPosition().getLonX());
    rec.setValue("destination_laty", destinationAirportLeg.getPosition().getLatY());
    rec.setValue("destination_alt", destinationAirportLeg.getAltitude()); // integer,
  }

  // File attachements =============================================

  planAttachLnmpln(&rec, NavApp::getCurrentRouteFilePath(), mainWindow);
  perfAttachLnmperf(&rec, NavApp::getCurrentAircraftPerfFilePath(), mainWindow);
  gpxAttach(&rec, mainWindow, true /* currentTrack */);

  // Filenames ======================================================
  rec.setValue("flightplan_file", atools::nativeCleanPath(NavApp::getCurrentRouteFilePath()));
  rec.setValue("performance_file", atools::nativeCleanPath(NavApp::getCurrentAircraftPerfFilePath()));
}

void LogdataController::addLogEntry()
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getEmptyRecord();

  // Prefill record for add dialog with values from current program state
  prefillLogEntry(rec);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << rec;
#endif

  LogdataDialog dlg(mainWindow, ld::ADD);
  connectDialogSignals(&dlg);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << rec;
#endif

    // Add to database
    SqlTransaction transaction(manager->getDatabase());

    SqlRecord newRec = dlg.getRecord();

    // Set id to null to avoid unique constraint mismatches - select id automatically
    if(newRec.contains("logbook_id"))
      newRec.setNull("logbook_id");

    manager->insertOneRecord(newRec);
    transaction.commit();

    logChanged(false /* load all */, false /* keep selection */);

    NavApp::setStatusMessage(tr("Logbook entry added."));
  }
  dlg.saveState();
}

void LogdataController::cleanupLogEntries()
{
  // Keep ids stable since they are used to save state
  enum {SHORT_DISTANCE, DEPARTURE_AND_DESTINATION_EQUAL, DEPARTURE_OR_DESTINATION_EMPTY, SHOW_PREVIEW, SHORT_DISTANCE_BOX};

  qDebug() << Q_FUNC_INFO;

  float distNm = -1.f;

  // Create a dialog with tree checkboxes =====================
  atools::gui::ChoiceDialog choiceDialog(mainWindow, QCoreApplication::applicationName() % tr(" - Cleanup Logbook"),
                                         tr("Select criteria for cleanup.\nNote that you can undo this change."),
                                         lnm::SEARCHTAB_LOGDATA_CLEANUP_DIALOG, "LOGBOOK.html#logbook-cleanup");

  choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

  choiceDialog.addCheckBox(SHORT_DISTANCE, tr("&Shorter than"),
                           tr("Removes all entries having a too small flown distance."), true /* checked */);

  // Add a spin box for the distance =======================
  QDoubleSpinBox *spinBox = new QDoubleSpinBox(&choiceDialog);
  spinBox->setDecimals(1);
  spinBox->setValue(5.);
  spinBox->setRange(1., 10800.);
  spinBox->setSingleStep(1.);
  spinBox->setSuffix(Unit::replacePlaceholders(tr(" %dist%")));
  choiceDialog.addWidget(SHORT_DISTANCE_BOX, spinBox);

  choiceDialog.addCheckBox(DEPARTURE_AND_DESTINATION_EQUAL, tr("&Departure and destination ident equal"),
                           tr("Removes all entries where the idents of departure and destination are the same. E.g. pattern work."),
                           false /* checked */);
  choiceDialog.addCheckBox(DEPARTURE_OR_DESTINATION_EMPTY, tr("&Departure or destination ident empty or not an airport"),
                           tr("Removes incomplete entries where the flight was terminated early or off-airport, for example."),
                           false /* checked */);
  choiceDialog.addSpacer();
  choiceDialog.addLine();
  choiceDialog.addCheckBox(SHOW_PREVIEW, tr("Show a &preview before deleting logbook entries"),
                           tr("Shows a dialog window with all logbook entries to be deleted before removing them."), true /* checked */);

  // Disable ok button if not at least one of these is checked
  choiceDialog.setRequiredAnyChecked({SHORT_DISTANCE, DEPARTURE_AND_DESTINATION_EQUAL, DEPARTURE_OR_DESTINATION_EMPTY});
  connect(&choiceDialog, &atools::gui::ChoiceDialog::buttonToggled, this, [&choiceDialog](int id, bool checked) {
    if(id == SHORT_DISTANCE)
      choiceDialog.enableWidget(SHORT_DISTANCE_BOX, checked);
  });

  choiceDialog.restoreState();

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    bool deleteEntries = false;

    // Prepare - replace null values with empty strings
    manager->preCleanup();

    // Show preview table ===============================================
    if(choiceDialog.isButtonChecked(SHOW_PREVIEW))
    {
      QVector<atools::sql::SqlColumn> previewCols({
        SqlColumn("departure_time", tr("Departure\nReal Time")),
        SqlColumn("aircraft_name", tr("Aircraft\nModel")),
        SqlColumn("aircraft_type", tr("Aircraft\nType")),
        SqlColumn("aircraft_registration", tr("Aircraft\nRegistration")),
        SqlColumn("simulator", tr("Simulator")),
        SqlColumn("departure_ident", tr("Departure\nIdent")),
        SqlColumn("departure_name", tr("Departure")),
        SqlColumn("destination_ident", tr("Destination\nIdent")),
        SqlColumn("destination_name", tr("Destination")),
        SqlColumn("distance_flown", Unit::replacePlaceholders(tr("Distance\nFlown %dist%"))),
        SqlColumn("flightplan", tr("Flight Plan\nattached")),
        SqlColumn("aircraft_perf", tr("Aircraft Performance\nattached")),
        SqlColumn("aircraft_trail", tr("Aircraft Trail\nattached")),
        SqlColumn("description", tr("Remarks")),
      });

      if(choiceDialog.isButtonChecked(SHORT_DISTANCE))
        distNm = Unit::rev(static_cast<float>(spinBox->value()), Unit::distNmF); // Convert from current units to NM

      // Get query for preview
      QString queryStr = manager->getCleanupPreview(choiceDialog.isButtonChecked(DEPARTURE_AND_DESTINATION_EQUAL),
                                                    choiceDialog.isButtonChecked(DEPARTURE_OR_DESTINATION_EMPTY),
                                                    distNm, previewCols);

      // Callback for data formatting
      atools::gui::SqlQueryDialogDataFunc dataFunc([&previewCols](int column, const QVariant& data, Qt::ItemDataRole role) -> QVariant {
                                                   const QString& colname = previewCols.at(column).getName();

                                                   if(role == Qt::TextAlignmentRole)
                                                   {
                                                     if(colname == "departure_time" || colname == "distance_flown")
                                                       return Qt::AlignRight;
                                                   }
                                                   else if(role == Qt::DisplayRole)
                                                   {
                                                     if(colname == "departure_time")
                                                       return data.toDateTime();
                                                     else if(colname == "distance_flown")
                                                       return Unit::distNm(data.toFloat(), false /* addUnit */);
                                                     else if(colname == "flightplan" || colname == "aircraft_perf" ||
                                                             colname == "aircraft_trail")
                                                       return data.toByteArray().isEmpty() ? tr("-") : tr("✓");
                                                   }

                                                   return data;
        });

      // Build preview dialog ===============================================
      atools::gui::SqlQueryDialog previewDialog(mainWindow, QCoreApplication::applicationName() % tr(" - Cleanup Preview"),
                                                tr("These logbook entries will be deleted.\nNote that you can undo this change."),
                                                lnm::SEARCHTAB_USERDATA_CLEANUP_PREVIEW, "LOGBOOK.html#cleanup-logbook-entries",
                                                tr("&Delete Logbook entries"));
      previewDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
      previewDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());
      previewDialog.initQuery(manager->getDatabase(), queryStr, previewCols, dataFunc);

      if(previewDialog.exec() == QDialog::Accepted)
        deleteEntries = true;
    }
    else
      deleteEntries = true;

    int removed = 0;
    if(deleteEntries)
    {
      // Dialog ok - remove entries ===============================================
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      removed = manager->cleanupLogEntries(choiceDialog.isButtonChecked(DEPARTURE_AND_DESTINATION_EQUAL),
                                           choiceDialog.isButtonChecked(DEPARTURE_OR_DESTINATION_EMPTY),
                                           distNm);
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();
    }

    // Undo prepare - replace empty strings with null values
    manager->postCleanup();

    if(deleteEntries)
    {
      // Send messages ===============================================
      if(removed > 0)
        logChanged(false /* load all */, false /* keep selection */);
      NavApp::setStatusMessage(tr("%1 logbook %2 deleted.").arg(removed).arg(removed == 1 ? tr("entry") : tr("entries")));
    }
  }
}

void LogdataController::deleteLogEntries(const QSet<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  QString txt = ids.size() == 1 ? tr("entry") : tr("entries");
  SqlTransaction transaction(manager->getDatabase());
  manager->deleteRows(ids);
  transaction.commit();

  logChanged(false /* load all */, false /* keep selection */);

  NavApp::setStatusMessage(tr("%1 logbook %2 deleted.").arg(ids.size()).arg(txt));
}

void LogdataController::importXplane()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_12);
    if(xpBasePath.isEmpty())
      xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_11);

    if(xpBasePath.isEmpty())
      xpBasePath = atools::documentsDir();
    else
      xpBasePath = atools::buildPathNoCase({xpBasePath, "Output", "logbooks"});

    QString file = dialog->openFileDialog(
      tr("Open X-Plane Logbook File"),
      tr("X-Plane Logbook Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_XPLANE_LOGBOOK), "Logdata/XPlane",
      xpBasePath);

    int numImported = 0;
    if(!file.isEmpty())
    {
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      numImported += manager->importXplane(file, fetchAirportCoordinates);
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();

      NavApp::setStatusMessage(tr("Imported %1 %2 X-Plane logbook.").arg(numImported).
                               arg(numImported == 1 ? tr("entry") : tr("entries")));

      logChanged(false /* load all */, false /* keep selection */);

      // Enable more search options to show user the search criteria
      NavApp::getMainUi()->actionLogdataSearchShowMoreOptions->setChecked(true);

      /*: The text "Imported from X-Plane logbook" has to match the one in atools::fs::userdata::LogdataManager::importXplane */
      emit showInSearch(map::LOGBOOK,
                        atools::sql::SqlRecord().appendFieldAndValue("description",
                                                                     tr("*Imported from X-Plane logbook*")),
                        false /* select */);
    }
  }
  catch(atools::Exception& e)
  {
    QGuiApplication::restoreOverrideCursor();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    QGuiApplication::restoreOverrideCursor();
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
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      numImported += manager->importCsv(file);
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();

      NavApp::setStatusMessage(tr("Imported %1 %2 from CSV file.").arg(numImported).
                               arg(numImported == 1 ? tr("entry") : tr("entries")));
      mainWindow->showLogbookSearch();
      logChanged(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    QGuiApplication::restoreOverrideCursor();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    QGuiApplication::restoreOverrideCursor();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Keep ids stable since they are used to save state
    enum {SELECTED, APPEND, HEADER, EXPORTPLAN, EXPORTPERF, EXPORTGPX};

    // Build a choice dialog with several checkboxes =========================
    atools::gui::ChoiceDialog choiceDialog(mainWindow, QCoreApplication::applicationName() % tr(" - Logbook Export"),
                                           tr("Select export options for logbook"), lnm::LOGDATA_EXPORT_CSV,
                                           "LOGBOOK.html#import-and-export");
    choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
    choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

    QString attachmentToolTip = tr("Content of attached file will be added to the exported CSV if selected.\n"
                                   "Note that not all programs will be able to read this.\n"
                                   "Columns will be empty on export if disabled.");

    int numSelected = NavApp::getLogdataSearch()->getSelectedRowCount();
    choiceDialog.addCheckBox(APPEND, tr("&Append to an already present file"),
                             tr("File header will be ignored if this is enabled."), false);
    choiceDialog.addCheckBox(SELECTED, tr("Export &selected entries only"), QString(), true,
                             numSelected == 0 /* disabled */);
    choiceDialog.addCheckBox(HEADER, tr("Add a &header to the first line"), QString(), false);
    choiceDialog.addLine();
    choiceDialog.addCheckBox(EXPORTPLAN, tr("&Flight plan in LNMPLN format"), attachmentToolTip, false);
    choiceDialog.addCheckBox(EXPORTPERF, tr("&Aircraft performance"), attachmentToolTip, false);
    choiceDialog.addCheckBox(EXPORTGPX, tr(
                               "&GPX file containing flight plan points and trail"), attachmentToolTip, false);
    choiceDialog.addSpacer();

    // Disable/enable header depending on append option
    connect(&choiceDialog, &atools::gui::ChoiceDialog::buttonToggled, this, [&choiceDialog](int id, bool checked) {
      if(id == APPEND)
        choiceDialog.disableWidget(HEADER, checked);
    });

    choiceDialog.restoreState();

    if(choiceDialog.exec() == QDialog::Accepted)
    {
      QString file = dialog->saveFileDialog(
        tr("Export Logbook Entry CSV File"),
        tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), ".csv", "Logdata/Csv",
        QString(), QString(), choiceDialog.isButtonChecked(APPEND));

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(choiceDialog.isButtonChecked(SELECTED))
          ids = NavApp::getLogdataSearch()->getSelectedIds();

        int numExported = manager->exportCsv(file, ids,
                                             choiceDialog.isButtonChecked(EXPORTPLAN),
                                             choiceDialog.isButtonChecked(EXPORTPERF),
                                             choiceDialog.isButtonChecked(EXPORTGPX),
                                             choiceDialog.isButtonChecked(HEADER) && !choiceDialog.isButtonChecked(APPEND),
                                             choiceDialog.isButtonChecked(APPEND));
        NavApp::setStatusMessage(tr("%1 logbook %2 exported.").
                                 arg(numExported).arg(numExported == 1 ? tr("entry") : tr("entries")));
      }
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

void LogdataController::convertUserdata()
{
  qDebug() << Q_FUNC_INFO;

  int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_LOGBOOK_CONVERSION,
                                          tr("This will convert all userpoints of type "
                                             "\"Logbook\" to logbook entries.<br/><br/>"
                                             "This works best if you did not modify the field "
                                             "\"Description\" in the userpoints and if "
                                             "you did not insert entries manually.<br/><br/>"
                                             "Note that not all fields can be converted automatically.<br/><br/>"
                                             "The created log entries can be found by searching "
                                             "for<br/>\"*Converted from userdata*\"<br/>"
                                             "in the field &quot;Remarks&quot;.<br/><br/>"
                                             "Continue?"),
                                          tr("Do not &show this dialog again and run the conversion."),
                                          QMessageBox::Yes | QMessageBox::No | QMessageBox::Help,
                                          QMessageBox::No, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    LogdataConverter converter(NavApp::getDatabaseUser(), manager, QueryManager::instance()->getQueriesGui()->getAirportQuerySim());

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Do the conversion ===================================
    int numCreated = converter.convertFromUserdata();

    QString resultText = tr("Created %1 log entries.").arg(numCreated);

    if(!converter.getErrors().isEmpty())
    {
      // Show errors and warnings ======================
      atools::util::HtmlBuilder html(true);

      html.p(tr("Logbook Conversion"), atools::util::html::BOLD | atools::util::html::BIG);

      html.p(resultText);

      html.p(tr("Conversion Errors/Warnings"), atools::util::html::BOLD | atools::util::html::BIG);
      html.p(tr("Some warnings might appear because of terminated flights, "
                "repeated landings and/or takeoffs. These can be ignored."));
      html.ol();
      for(const QString& err : converter.getErrors())
        html.li(err, atools::util::html::NO_ENTITIES);
      html.olEnd();

      TextDialog error(mainWindow, QCoreApplication::applicationName() % tr(" - Conversion Errors"),
                       "LOGBOOK.html#convert-errors"); // anchor for future use
      error.setHtmlMessage(html.getHtml(), true /* print to log */);
      QGuiApplication::restoreOverrideCursor();
      error.exec();
    }
    else
    {
      // No errors ======================
      QGuiApplication::restoreOverrideCursor();
      atools::gui::Dialog::information(mainWindow, resultText);
    }

    mainWindow->showLogbookSearch();

    logChanged(false /* load all */, false /* keep selection */);

    // Enable more search options to show user the search criteria
    NavApp::getMainUi()->actionLogdataSearchShowMoreOptions->setChecked(true);

    /*: The text "Converted from userdata" has to match the one in LogdataConverter::convertFromUserdata */
    emit showInSearch(map::LOGBOOK,
                      atools::sql::SqlRecord().appendFieldAndValue("description", tr("*Converted from userdata*")),
                      false /* select */);
  }
  else if(result == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl % "LOGBOOK.html#convert",
                                             lnm::helpLanguageOnline());
}

void LogdataController::fetchAirportCoordinates(atools::geo::Pos& pos, QString& name, const QString& airportIdent)
{
  map::MapAirport airport = QueryManager::instance()->getQueriesGui()->getAirportQuerySim()->getAirportByIdent(airportIdent);

  if(airport.isValid())
  {
    pos = airport.position;
    name = airport.name;
  }
}

void LogdataController::planOpen(atools::sql::SqlRecord *record, QWidget *parent)
{
  try
  {
    qDebug() << Q_FUNC_INFO;
    // Open flight plan in table replacing the current plan - attachement is always LNMPLN
    mainWindow->routeOpenFileLnmLogdataStr(QString(atools::zip::gzipDecompress(record->value("flightplan").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::planAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  planAttachLnmpln(record, mainWindow->routeOpenFileDialog(), parent);
}

void LogdataController::planAttachLnmpln(atools::sql::SqlRecord *record, const QString& filename, QWidget *parent)
{
  try
  {
    // Store a new flight plan as logbook entry attachement - loaded plan can be any supported format
    // Plan is always attached in LNMPLN format
    if(atools::checkFile(Q_FUNC_INFO, filename))
    {
      // Load flight plan in any supported format
      atools::fs::pln::Flightplan flightplan;
      atools::fs::pln::FlightplanIO().load(flightplan, filename);

      if(!flightplan.isEmpty())
        // Store in LNMPLN format
        record->setValue("flightplan", atools::fs::pln::FlightplanIO().saveLnmGz(flightplan));
      else
        record->setNull("flightplan");
    }
    else
      record->setNull("flightplan");
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::planSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Load LNMPLN into flight plan
    atools::fs::pln::Flightplan flightplan;
    atools::fs::pln::FlightplanIO().loadLnmGz(flightplan, record->value("flightplan").toByteArray());

    // Build filename
    QString defFilename = buildFilename(record, flightplan, ".lnmpln");
    QString filename = mainWindow->routeSaveFileDialogLnm(defFilename);
    if(!filename.isEmpty())
      atools::fs::pln::FlightplanIO().saveLnm(flightplan, filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::gpxAttach(atools::sql::SqlRecord *record, QWidget *parent, bool currentTrack)
{
  try
  {
    const AircraftTrail& track = currentTrack ? NavApp::getAircraftTrail() : NavApp::getAircraftTrailLogbook();

    if(!track.isEmpty() || !NavApp::getRouteConst().isEmpty())
    {
      const atools::fs::pln::Flightplan flightplan =
        NavApp::getRouteConst().updatedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_GPX).getFlightplanConst();
      record->setValue("aircraft_trail", atools::fs::gpx::GpxIO().saveGpxGz(track.toGpxData(flightplan)));
    }
    else
      record->setNull("aircraft_trail");

  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::gpxAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString filename = dialog->openFileDialog(
      tr("Open GPX"),
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "Route/Gpx", atools::documentsDir());

    if(!filename.isEmpty())
    {
      QFile file(filename);
      if(file.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");

        // Decompress GPX and save as is into the database
        record->setValue("aircraft_trail", atools::zip::gzipCompress(stream.readAll().toUtf8()));

        file.close();
      }
      else
        atools::gui::ErrorHandler(parent).handleIOError(file, tr("Cannot load file."));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

QString LogdataController::buildFilename(const atools::sql::SqlRecord *record,
                                         const atools::fs::pln::Flightplan& flightplan, const QString& suffix)
{
  if(!flightplan.isEmpty())
    // Flight plan is valid - extract name from plan object
    return flightplan.getFilenamePattern(OptionData::instance().getFlightplanPattern(), suffix, Unit::getUnitAlt() == opts::ALT_METER);
  else if(record != nullptr)
  {
    // No flight plan - extract name from SQL record and values currently set in the GUI
    const Route& route = NavApp::getRouteConst();
    QString type = route.getFlightplanConst().getFlightplanTypeStr();
    return atools::fs::pln::Flightplan::getFilenamePattern(OptionData::instance().getFlightplanPattern(), type,
                                                           record->valueStr("departure_name"),
                                                           record->valueStr("departure_ident"),
                                                           record->valueStr("destination_name"),
                                                           record->valueStr("destination_ident"), suffix,
                                                           atools::roundToInt(Unit::altFeetF(route.getCruiseAltitudeFt())));
  }

  return tr("Empty Flight Plan") % suffix;
}

void LogdataController::gpxSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Get flight plan for file name building =========
    atools::fs::pln::Flightplan flightplan;
    try
    {
      // Older versions of LNM attached empty and invalid flight plans. Do not show an exception to the user here.
      QString plan = QString(atools::zip::gzipDecompress(record->value("flightplan").toByteArray()));
      atools::fs::pln::FlightplanIO().loadLnmStr(flightplan, plan);
    }
    catch(atools::Exception& e)
    {
      qDebug() << Q_FUNC_INFO << "Error reading flight plan" << e.what();
    }

    QString defFilename = buildFilename(record, flightplan, ".gpx");

    QString filename = dialog->saveFileDialog(
      tr("Save GPX"),
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "lnmpln", "Route/Gpx", atools::documentsDir(), defFilename, false /* confirm overwrite */);

    if(!filename.isEmpty())
    {
      QFile file(filename);
      if(file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        // Decompress and save track as is ==============
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << QString(atools::zip::gzipDecompress(record->value("aircraft_trail").toByteArray())).toUtf8();
        file.close();
      }
      else
        atools::gui::ErrorHandler(parent).handleIOError(file, tr("Cannot save file."));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::perfAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  perfAttachLnmperf(record, NavApp::getAircraftPerfController()->openFileDialog(), parent);
}

void LogdataController::perfAttachLnmperf(atools::sql::SqlRecord *record, const QString& filename, QWidget *parent)
{
  try
  {
    if(atools::checkFile(Q_FUNC_INFO, filename))
    {
      // Load aircraft performance in any format (INI or XML) ===================
      atools::fs::perf::AircraftPerf perf;
      perf.load(filename);

      // Save in new XML format ============
      record->setValue("aircraft_perf", perf.saveXmlGz());
    }
    else
      record->setNull("aircraft_perf");
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::perfOpen(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    NavApp::getAircraftPerfController()->loadStr(QString(atools::zip::gzipDecompress(record->value("aircraft_perf").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::perfSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    NavApp::getAircraftPerfController()->saveAsStr(QString(atools::zip::gzipDecompress(record->value("aircraft_perf").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}
