/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_LOGDATACONTROLLER_H
#define LNM_LOGDATACONTROLLER_H

#include "common/maptypes.h"

#include <QCache>
#include <QObject>
#include <QVector>

class AircraftTrail;
namespace atools {
namespace sql {
class SqlRecord;
class SqlDatabase;
}

namespace geo {
class Pos;
}
namespace gui {
class Dialog;
class ErrorHandler;
class HelpHandler;
}
namespace fs {

namespace gpx {
class GpxData;
}
namespace pln {
class Flightplan;
}
namespace sc {
class SimConnectUserAircraft;
}
namespace userdata {
class LogdataManager;

}
}
}

namespace map {
struct MapResult;

struct MapUserpoint;
struct MapLogbookEntry;

}

class MainWindow;
class LogStatisticsDialog;
class LogdataDialog;
class QAction;
/*
 * Methods to edit, add, delete, import and export logbook entries. Also creates entries for flight events.
 */
class LogdataController :
  public QObject
{
  Q_OBJECT

public:
  explicit LogdataController(atools::fs::userdata::LogdataManager *logdataManager, MainWindow *parent);
  virtual ~LogdataController() override;

  LogdataController(const LogdataController& other) = delete;
  LogdataController& operator=(const LogdataController& other) = delete;

  /* Show edit dialog and save changes to the database if accepted for the given ids */
  void editLogEntries(const QVector<int>& ids);
  void addLogEntry();
  void cleanupLogEntries();

  /* Show message box and delete entries with the given ids */
  void deleteLogEntries(const QSet<int>& ids);

  /* Import and export from a predefined CSV format. Import does not commit. */
  void importCsv();
  void exportCsv();

  /* Import X-Plane text logbook - does not commit. */
  void importXplane();

  /* Show search tab and raise window */
  void  showSearch();

  void saveState() const;
  void restoreState();

  /* Reset saved settings and position of LogStatisticsDialog */
  void resetWindowLayout();

  /* Edit and delete log entries from map context menu */
  void editLogEntryFromMap(int id);
  void deleteLogEntryFromMap(int id);

  /* Signaled if aircraft was or is close to the departure point on the runway */
  void aircraftHasPassedTakeoffPoint(const atools::fs::sc::SimConnectUserAircraft&);

  /* Takeoff and landing events from map widget which will create a log entry if enabled */
  void aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm);

  /* Get record or struct from database */
  map::MapLogbookEntry getLogEntryById(int id);
  atools::sql::SqlRecord getLogEntryRecordById(int id);

  /* Get various statistical information for departure times */
  void getFlightStatsTime(QDateTime& earliest, QDateTime& latest, QDateTime& earliestSim, QDateTime& latestSim);

  /* Flight plant distances in NM for logbook entries */
  void getFlightStatsDistance(float& distTotal, float& distMax, float& distAverage);

  /* Trip Time in hours */
  void getFlightStatsTripTime(float& timeMaximum, float& timeAverage, float& timeTotal, float& timeMaximumSim,
                              float& timeAverageSim, float& timeTotalSim);

  /* Various numbers */
  void getFlightStatsAirports(int& numDepartAirports, int& numDestAirports);
  void getFlightStatsAircraft(int& numTypes, int& numRegistrations, int& numNames, int& numSimulators);

  /* Simulator to number of logbook entries */
  void getFlightStatsSimulator(QVector<std::pair<int, QString> >& numSimulators);

  /* Make the non-modal statistics dialog visible */
  void statisticsLogbookShow();

  /* Log database */
  atools::sql::SqlDatabase *getDatabase() const;

  /* Update units */
  void optionsChanged();

  void fontChanged(const QFont& font);
  void styleChanged();

  /* Convert legacy logbook entries from userdata to the new logbook */
  void convertUserdata();

  /* Resets detection of flight */
  void resetTakeoffLandingDetection();

  const atools::fs::gpx::GpxData *getGpxData(int id);
  const AircraftTrail *getAircraftTrail(int id);

  /* Clear caches */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Show flight plan, trail and direct options changed */
  void displayOptionsChanged();

  /* true if the respective action is enabled in the context menu */
  bool isDirectPreviewShown();
  bool isRoutePreviewShown();
  bool isTrackPreviewShown();

  /* true if the files are attached and length is > 0 */
  bool hasRouteAttached(int id);
  bool hasPerfAttached(int id);
  bool hasTrackAttached(int id);

  /* File attachements - push buttons or context menu actions from search */
  void planOpen(atools::sql::SqlRecord *record, QWidget *parent); /* Open as current */
  void planAdd(atools::sql::SqlRecord *record, QWidget *parent); /* Attach new plan to logbook entry */
  void planSaveAs(atools::sql::SqlRecord *record, QWidget *parent); /* Save attached LNMPLN plan to file */
  void gpxAdd(atools::sql::SqlRecord *record, QWidget *parent); /* Attach new GPX to logbook entry */
  void gpxSaveAs(atools::sql::SqlRecord *record, QWidget *parent); /* Save attached GPX plan to file */
  void perfOpen(atools::sql::SqlRecord *record, QWidget *parent); /* Open as current */
  void perfAdd(atools::sql::SqlRecord *record, QWidget *parent); /* Attach new performance to logbook entry */
  void perfSaveAs(atools::sql::SqlRecord *record, QWidget *parent); /* Save attached performance to file */

  /* Aircraft was close to departure point on runway. Value is remembered. */
  bool hasAircraftPassedTakeoffPoint() const
  {
    return aircraftPassedTakeoffPoint;
  }

signals:
  /* Sent after database modification to update the search result table */
  void refreshLogSearch(bool loadAll, bool keepSelection, bool force);

  /* Issue a redraw of the map */
  void logDataChanged();

  /* Show search after converting or importing entries */
  void showInSearch(map::MapTypes type, const atools::sql::SqlRecord& record, bool select);

private:
  /* Create a logbook entry on takeoff and update it on landing */
  void createTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff,
                            float flownDistanceNm);
  void recordTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft, const map::MapAirport& airport,
                     const map::MapRunwayEnd& runwayEnd);
  void recordLanding(atools::sql::SqlRecord& record, const atools::fs::sc::SimConnectUserAircraft& aircraft, const map::MapAirport& airport,
                     const map::MapRunwayEnd& runwayEnd, float flownDistanceNm);
  QString recordAirportOrCoordinates(atools::sql::SqlRecord& record, const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                     const map::MapAirport& airport, const map::MapRunwayEnd& runwayEnd, const QString& prefix);

  /* Needs departure set but no destination */
  bool recordIsTakeoff(const atools::sql::SqlRecord& record);

  /* Matches if same aircraft and same simulator */
  bool doesFlightMatchRecord(const atools::sql::SqlRecord& record, const atools::fs::sc::SimConnectUserAircraft& aircraft);

  /* Prefill record for add dialog with values from current program state */
  void prefillLogEntry(atools::sql::SqlRecord& rec);

  /* Callback function for X-Plane import */
  static void fetchAirportCoordinates(atools::geo::Pos& pos, QString& name, const QString& airportIdent);

  /* Attach the current flight plan and performance file to the record as Gzipped XML files */
  void recordFlightplanAndPerf(atools::sql::SqlRecord& record);

  /* Connect buttons in dialog with above */
  void connectDialogSignals(LogdataDialog *dialog);

  /* Emit signals for changed */
  void logChanged(bool loadAll, bool keepSelection);

  void planAttachLnmpln(atools::sql::SqlRecord *record, const QString& filename, QWidget *parent);
  void perfAttachLnmperf(atools::sql::SqlRecord *record, const QString& filename, QWidget *parent);
  void gpxAttach(atools::sql::SqlRecord *record, QWidget *parent, bool currentTrack);

  QString buildFilename(const atools::sql::SqlRecord *record, const atools::fs::pln::Flightplan& flightplan, const QString& suffix);

  void undoTriggered();
  void redoTriggered();

  /* Load and save logEntryId to configuration file */
  void saveLogEntryId() const;
  void restoreLogEntryId();

  int logEntryId = -1;

  /* Remember last aircraft for fuel calculations */
  const atools::fs::sc::SimConnectUserAircraft *aircraftAtTakeoff = nullptr;

  bool aircraftPassedTakeoffPoint = false;

  LogStatisticsDialog *statsDialog = nullptr;

  atools::fs::userdata::LogdataManager *manager;
  atools::gui::Dialog *dialog;
  MainWindow *mainWindow;

  QCache<int, AircraftTrail> aircraftTrailCache;
};

#endif // LNM_LOGDATACONTROLLER_H
