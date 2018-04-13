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

#ifndef NAVAPPLICATION_H
#define NAVAPPLICATION_H

#include "gui/application.h"

#include "common/mapflags.h"
#include "fs/fspaths.h"

class AirportQuery;
class MapQuery;
class AirspaceQuery;
class InfoQuery;
class ProcedureQuery;
class Route;
class MainWindow;
class ConnectClient;
class DatabaseManager;
class QMainWindow;
class RouteController;
class MapWidget;
class WeatherReporter;
class ElevationProvider;
class AircraftTrack;
class QSplashScreen;
class UpdateHandler;
class UserdataController;
class OnlinedataController;
class UserdataIcons;
class UserdataSearch;
class VehicleIcons;

namespace atools {

namespace geo {
class Pos;
}

namespace fs {

namespace userdata {
class UserdataManager;
}

namespace common {
class MagDecReader;
}

namespace db {
class DatabaseMeta;
}
}

namespace sql {
class SqlDatabase;
}
}

namespace Ui {
class MainWindow;
}

/*
 * Keeps most important handler, window and query classes for static access.
 * Initialized and deinitialized in main window.
 *
 * Not all getters refer to aggregated values but are rather delegates that help to minimize dependencies.
 */
class NavApp :
  public atools::gui::Application
{
public:
  NavApp(int& argc, char **argv, int flags = ApplicationFlags);
  virtual ~NavApp();

  static NavApp *navAppInstance();

  /* Creates all aggregated objects */
  static void init(MainWindow *mainWindowParam);

  /* Needs map widget first */
  static void initElevationProvider();

  /* Deletes all aggregated objects */
  static void deInit();

  static void checkForUpdates(int channelOpts, bool manuallyTriggered);

  static void optionsChanged();
  static void preDatabaseLoad();
  static void postDatabaseLoad();

  static Ui::MainWindow *getMainUi();

  static bool isConnected();
  static bool isUserAircraftValid();

  static map::MapObjectTypes getShownMapFeatures();
  static map::MapAirspaceFilter getShownMapAirspaces();

  static AirportQuery *getAirportQuerySim();
  static AirportQuery *getAirportQueryNav();
  static MapQuery *getMapQuery();

  /* Nav data as source */
  static AirspaceQuery *getAirspaceQuery();

  /* Online network data as source */
  static AirspaceQuery *getAirspaceQueryOnline();

  static InfoQuery *getInfoQuery();
  static ProcedureQuery *getProcedureQuery();
  static const Route& getRouteConst();
  static Route& getRoute();
  static float getSpeedKts();

  /* Currently selected simulator database */
  static atools::fs::FsPaths::SimulatorType getCurrentSimulatorDb();
  static QString getCurrentSimulatorBasePath();
  static QString getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type);
  static bool isNavdataOnly();

  /* Get full path to language dependent "Flight Simulator X Files" or "Flight Simulator X-Dateien",
   * etc. Returns the documents path if FS files cannot be found. */
  static QString getCurrentSimulatorFilesPath();

  /* Get the short name (FSX, FSXSE, P3DV3, P3DV2) of the currently selected simulator. */
  static QString getCurrentSimulatorShortName();
  static bool hasSidStarInDatabase();
  static bool hasDataInDatabase();

  /* Simulator scenery data */
  static atools::sql::SqlDatabase *getDatabaseSim();

  /* External update from navaids or same as above */
  static atools::sql::SqlDatabase *getDatabaseNav();

  static atools::fs::userdata::UserdataManager *getUserdataManager();
  static UserdataIcons *getUserdataIcons();
  static UserdataSearch *getUserdataSearch();

  static atools::sql::SqlDatabase *getDatabaseUser();
  static atools::sql::SqlDatabase *getDatabaseOnline();

  static ElevationProvider *getElevationProvider();

  static WeatherReporter *getWeatherReporter();

  static void updateWindowTitle();
  static void setStatusMessage(const QString& message);

  /* Get main window in different variations to avoid including it */
  static QWidget *getQMainWidget();
  static QMainWindow *getQMainWindow();
  static MainWindow *getMainWindow();

  static MapWidget *getMapWidget();
  static RouteController *getRouteController();

  static DatabaseManager *getDatabaseManager();

  static ConnectClient *getConnectClient();

  static const atools::fs::db::DatabaseMeta *getDatabaseMetaSim();
  static const atools::fs::db::DatabaseMeta *getDatabaseMetaNav();

  static QString getDatabaseAiracCycleSim();
  static QString getDatabaseAiracCycleNav();

  /* True if the database contains any airspaces or boundaries. */
  static bool hasDatabaseAirspaces();

  /* True if online data and ATC centers are available */
  static bool hasOnlineData();

  static QString getOnlineNetwork();
  static bool isOnlineNetworkActive();

  static const AircraftTrack& getAircraftTrack();

  static void initSplashScreen();
  static void finishSplashScreen();
  static void deleteSplashScreen();

  static bool isShuttingDown();
  static void setShuttingDown(bool value);

  static float getMagVar(const atools::geo::Pos& pos, float defaultValue = 0.f);

  static UpdateHandler *getUpdateHandler();

  static UserdataController *getUserdataController();
  static OnlinedataController *getOnlinedataController();

  static atools::fs::common::MagDecReader *getMagDecReader();

  static VehicleIcons *getVehicleIcons();

private:
  /* Database query helpers and caches */
  static AirportQuery *airportQuerySim, *airportQueryNav;
  static MapQuery *mapQuery;
  static AirspaceQuery *airspaceQuery, *airspaceQueryOnline;
  static InfoQuery *infoQuery;
  static ProcedureQuery *procedureQuery;
  static ElevationProvider *elevationProvider;

  /* Most important handlers */
  static ConnectClient *connectClient;
  static DatabaseManager *databaseManager;
  static atools::fs::common::MagDecReader *magDecReader;
  static UserdataController *userdataController;
  static OnlinedataController *onlinedataController;

  /* Main window is not aggregated */
  static MainWindow *mainWindow;

  static atools::fs::db::DatabaseMeta *databaseMeta;
  static atools::fs::db::DatabaseMeta *databaseMetaNav;
  static QSplashScreen *splashScreen;

  static UpdateHandler *updateHandler;
  static VehicleIcons *vehicleIcons;

  static bool shuttingDown;
};

#endif // NAVAPPLICATION_H
