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

#ifndef LNM_LOGDATACONTROLLER_H
#define LNM_LOGDATACONTROLLER_H

#include "common/maptypes.h"

#include <QObject>
#include <QVector>

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
namespace sc {
class SimConnectUserAircraft;
}
namespace userdata {
class LogdataManager;

}
}
}

namespace map {
struct MapSearchResult;

struct MapUserpoint;
struct MapLogbookEntry;

}

class MainWindow;
class LogStatisticsDialog;
class QAction;

/*
 * Methods to edit, add, delete, import and export logbook entries. Also creates entries for flight events.
 */
class LogdataController :
  public QObject
{
  Q_OBJECT

public:
  LogdataController(atools::fs::userdata::LogdataManager *logdataManager, MainWindow *parent);
  virtual ~LogdataController();

  /* Show edit dialog and save changes to the database if accepted for the given ids */
  void editLogEntries(const QVector<int>& ids);
  void addLogEntry();

  /* Show message box and delete entries with the given ids */
  void deleteLogEntries(const QVector<int>& ids);

  /* Import and export from a predefined CSV format */
  void importCsv();
  void exportCsv();

  /* Import X-Plane text logbook */
  void importXplane();

  /* Show search tab and raise window */
  void  showSearch();

  void saveState();
  void restoreState();

  /* Edit and delete log entries from map context menu */
  void editLogEntryFromMap(int id);
  void deleteLogEntryFromMap(int id);

  /* Takeoff and landing events from map widget which will create a log entry if enabled */
  void aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                       float averageTasKts);

  /* Get record or struct from database */
  map::MapLogbookEntry getLogEntryById(int id);
  atools::sql::SqlRecord getLogEntryRecordById(int id);

  /* Get various statistical information for departure times */
  void getFlightStatsTime(QDateTime& earliest, QDateTime& latest, QDateTime& earliestSim, QDateTime& latestSim);

  /* Flight plant distances in NM for logbook entries */
  void getFlightStatsDistance(float& distTotal, float& distMax, float& distAverage);

  /* Trip Time in hours */
  void getFlightStatsTripTime(float& timeMaximum, float& timeAverage, float& timeMaximumSim, float& timeAverageSim);

  /* Various numbers */
  void getFlightStatsAirports(int& numDepartAirports, int& numDestAirports);
  void getFlightStatsAircraft(int& numTypes, int& numRegistrations, int& numNames, int& numSimulators);

  /* Simulator to number of logbook entries */
  void getFlightStatsSimulator(QVector<std::pair<int, QString> >& numSimulators);

  /* Make the non-modal statistics dialog visible */
  void showStatistics();

  /* Log database */
  atools::sql::SqlDatabase *getDatabase() const;

  /* Update units */
  void optionsChanged();

  /* Convert legacy logbook entries from userdata to the new logbook */
  void convertUserdata();

  /* Resets detection of flight */
  void resetTakeoffLandingDetection();

signals:
  /* Sent after database modification to update the search result table */
  void refreshLogSearch(bool loadAll, bool keepSelection);

  /* Issue a redraw of the map */
  void logDataChanged();

  /* Show search after converting or importing entries */
  void showInSearch(map::MapObjectTypes type, const atools::sql::SqlRecord& record, bool select);

private:
  /* Create a logbook entry on takeoff and update it on landing */
  void createTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff, float flownDistanceNm,
                            float averageTasKts);

  /* Callback function for X-Plane import */
  static void fetchAirportCoordinates(atools::geo::Pos& pos, QString& name, const QString& airportIdent);

  /* Remember last aircraft for fuel calculations */
  const atools::fs::sc::SimConnectUserAircraft *aircraftAtTakeoff = nullptr;
  int logEntryId = -1;

  LogStatisticsDialog *statsDialog = nullptr;

  atools::fs::userdata::LogdataManager *manager;
  atools::gui::Dialog *dialog;
  MainWindow *mainWindow;

};

#endif // LNM_LOGDATACONTROLLER_H
