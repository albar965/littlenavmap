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

#ifndef NAVAPPLICATION_H
#define NAVAPPLICATION_H

#include "gui/application.h"

#include "common/mapflags.h"
#include "fs/fspaths.h"

class AircraftPerfController;
class AircraftTrail;
class TrackController;
class AirspaceController;
class ConnectClient;
class DatabaseManager;
class ElevationProvider;
class InfoController;
class LogdataController;
class LogdataSearch;
class MainWindow;
class MapPaintWidget;
class MapWidget;
class OnlinedataController;
class OptionsDialog;
class ProcedureQuery;
class QMainWindow;
class Route;
class RouteAltitude;
class RouteController;
class SearchController;
class StyleHandler;
class UpdateHandler;
class UserdataController;
class UserdataIcons;
class UserdataSearch;
class VehicleIcons;
class WeatherReporter;
class WebController;
class WindReporter;
class MapMarkHandler;
struct MapAirportHandler;
class MapDetailHandler;
class TrackManager;
class MapThemeHandler;
class QAction;
class WeatherContextHandler;

namespace map {
struct MapAirport;
}
namespace atools {

namespace win {
class ActivationContext;
}
namespace gui {

class DataExchange;
class TabWidgetHandler;
}

namespace geo {
class Pos;
class Rect;
}

namespace fs {

namespace scenery {
class LanguageJson;
class AircraftIndex;
}

namespace perf {
class AircraftPerf;
}
namespace sc {
class SimConnectUserAircraft;
class SimConnectAircraft;
class SimConnectData;
}

namespace userdata {
class UserdataManager;
class LogdataManager;
}

namespace common {
class MagDecReader;
class MoraReader;
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
 * Facade that keeps most important handler, window and query classes for static access.
 * Initialized and deinitialized in main window.
 *
 * Not all getters refer to aggregated values but are rather delegates that help to minimize dependencies.
 *
 * See the delegated methods for documentation.
 */
class NavApp :
  public atools::gui::Application
{
  Q_DECLARE_TR_FUNCTIONS(NavApp)

public:
  NavApp(int& argc, char **argv, int flags = ApplicationFlags);
  virtual ~NavApp() override;

  NavApp(const NavApp& other) = delete;
  NavApp& operator=(const NavApp& other) = delete;

  static NavApp *navAppInstance();

  /* Creates all aggregated objects */
  static void init(MainWindow *mainWindowParam);
  static void initQueries();

  /* Needs map widget first */
  static void initElevationProvider();
  static void showElevationProviderErrors();

  /* Deletes all aggregated objects */
  static void deInit();

  /* Have to delete early before deleting map widget */
  static void deInitWebController();

  static void checkForUpdates(int channelOpts, bool manual, bool startup, bool forceDebug);
  static void updateChannels(int channelOpts);

  static void optionsChanged();
  static void preDatabaseLoad();
  static void postDatabaseLoad();

  static Ui::MainWindow *getMainUi();

  /* true if startup is completed and main window is visible */
  static bool isMainWindowVisible()
  {
    return mainWindowVisible;
  }

  static void setMainWindowVisible();

  static void setStayOnTop(QWidget *widget);

  static bool isFetchAiAircraft();
  static bool isFetchAiShip();
  static bool isConnected();
  static bool isConnectedActive();
  static bool isNetworkConnect();
  static bool isXpConnect();
  static bool isSimConnect();
  static bool isConnectedAndAircraft();
  static bool isConnectedAndAircraftFlying();
  static bool isUserAircraftValid();
  static bool hasAircraftPassedTakeoffPoint();

  static bool checkSimConnect();
  static void pauseSimConnect();
  static void resumeSimConnect();

  /* Check for availability in database */
  static bool isMoraAvailable();

  static float getTakeoffFlownDistanceNm();
  static QDateTime getTakeoffDateTime();

  static const atools::fs::sc::SimConnectUserAircraft& getUserAircraft();
  static const atools::fs::sc::SimConnectData& getSimConnectData();
  static const atools::geo::Pos& getUserAircraftPos();

  static atools::win::ActivationContext *getActivationContext();

  static void updateAllMaps();

  static const QList<atools::fs::sc::SimConnectAircraft>& getAiAircraft();

  static const map::MapTypes getShownMapTypes();
  static const map::MapDisplayTypes getShownMapDisplayTypes();
  static const map::MapAirspaceFilter& getShownMapAirspaces();

  static const Route& getRouteConst();
  static Route& getRoute();
  static void updateRouteCycleMetadata();

  /* Get a generic route string for logbook entry */
  static QString getRouteStringLogbook();
  static const atools::geo::Rect& getRouteRect();

  static const RouteAltitude& getAltitudeLegs();
  static float getGroundBufferForLegFt(int legIndex);

  static float getRouteCruiseSpeedKts();
  static float getRouteCruiseAltitudeFt();
  static float getRouteCruiseAltFtWidget();

  static bool isRouteEmpty();
  static bool isValidProfile();

  /* Currently selected simulator database */
  static atools::fs::FsPaths::SimulatorType getCurrentSimulatorDb();
  static bool isAirportDatabaseXPlane(bool navdata);
  static bool isDatabaseXPlane();
  static QString getCurrentSimulatorBasePath();
  static QString getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type);
  static QString getSimulatorBasePathBest(const QList<atools::fs::FsPaths::SimulatorType>& types);
  static QString getSimulatorFilesPathBest(const QList<atools::fs::FsPaths::SimulatorType>& types, const QString& defaultPath);
  static bool hasSimulator(atools::fs::FsPaths::SimulatorType type);
  static bool hasAnyMsSimulator();
  static bool hasInstalledSimulator(atools::fs::FsPaths::SimulatorType type);

  static bool hasAnyXplaneSimulator();
  static bool hasXplane11Simulator();
  static bool hasXplane12Simulator();

  /* Selected navdatabase in menu */
  static bool isNavdataAll();
  static bool isNavdataOff();
  static bool isNavdataMixed();

  static OptionsDialog *getOptionsDialog();

  /* Get full path to language dependent "Flight Simulator X Files" or "Flight Simulator X-Dateien",
   * etc. Returns the documents path if FS files cannot be found. */
  static QString getCurrentSimulatorFilesPath();

  /* Get the short name (FSX, FSXSE, P3DV3, P3DV2) of the currently selected simulator. */
  static QString getCurrentSimulatorShortName();
  static QString getCurrentSimulatorShortDisplayName();
  static QString getCurrentSimulatorName();
  static bool hasSidStarInDatabase();
  static bool hasRouteTypeInDatabase();
  static bool hasDataInDatabase();
  static bool hasDataInSimDatabase();

  static void logDatabaseMeta();

  /* Simulator scenery data */
  static atools::sql::SqlDatabase *getDatabaseSim();

  /* External update from navaids or same as above */
  static atools::sql::SqlDatabase *getDatabaseNav();

  static atools::fs::userdata::UserdataManager *getUserdataManager();
  static UserdataIcons *getUserdataIcons();
  static UserdataSearch *getUserdataSearch();

  static atools::fs::userdata::LogdataManager *getLogdataManager();
  static LogdataSearch *getLogdataSearch();

  static atools::sql::SqlDatabase *getDatabaseTrack();

  static atools::sql::SqlDatabase *getDatabaseUser();
  static atools::sql::SqlDatabase *getDatabaseUserAirspace();
  static atools::sql::SqlDatabase *getDatabaseLogbook();
  static atools::sql::SqlDatabase *getDatabaseOnline();

  static ElevationProvider *getElevationProvider();
  static bool isGlobeOfflineProvider();
  static bool isGlobeDirValid();

  static WeatherReporter *getWeatherReporter();
  static WeatherContextHandler *getWeatherContextHandler();
  static void getAirportMetarWind(float& windDirectionDeg, float& windSpeedKts, const map::MapAirport& airport, bool stationOnly);

  static WindReporter *getWindReporter();

  static void updateWindowTitle();
  static void updateErrorLabel();
  static void setStatusMessage(const QString& message, bool addToLog = false, bool popup = false);

  /* Get main window in different variations to avoid including it */
  static QWidget *getQMainWidget();
  static QMainWindow *getQMainWindow();
  static MainWindow *getMainWindow();
  static void addDialogToDockHandler(QDialog *dialog);
  static void removeDialogFromDockHandler(QDialog *dialog);
  static QList<QAction *> getMainWindowActions();

  static MapWidget *getMapWidgetGui();
  static MapPaintWidget *getMapPaintWidgetGui();
  static RouteController *getRouteController();
  static atools::gui::TabWidgetHandler *getRouteTabHandler();
  static const InfoController *getInfoController();
  static QFont getTextBrowserInfoFont();
  static MapThemeHandler *getMapThemeHandler();

  static DatabaseManager *getDatabaseManager();

  /* MSFS translations from table "translation" */
  static const atools::fs::scenery::LanguageJson& getLanguageIndex();

  /* Aircraft config read from MSFS folders to get more user aircraft details */
  static atools::fs::scenery::AircraftIndex& getAircraftIndex();

  static ConnectClient *getConnectClient();

  /* Can be null while compiling database */
  static const atools::fs::db::DatabaseMeta *getDatabaseMetaSim();
  static const atools::fs::db::DatabaseMeta *getDatabaseMetaNav();

  static QString getDatabaseAiracCycleSim();
  static QString getDatabaseAiracCycleNav();

  /* True if online data and ATC centers are available */
  static bool hasOnlineData();

  static QString getOnlineNetworkTranslated();
  static bool isOnlineNetworkActive();

  static bool isAircraftTrailEmpty();
  static const AircraftTrail& getAircraftTrail();

  static int getMaxStoredTrailEntries();

  static const AircraftTrail& getAircraftTrailLogbook();
  static void deleteAircraftTrailLogbook();

  /* Main window close event about to exit */
  static bool isCloseCalled();
  static void setCloseCalled(bool value = true);

  /* true if map window is maximized */
  static bool isFullScreen();

  static float getMagVar(const atools::geo::Pos& pos, float defaultValue = 0.f);

  static UpdateHandler *getUpdateHandler();

  static UserdataController *getUserdataController();
  static LogdataController *getLogdataController();
  static OnlinedataController *getOnlinedataController();
  static AircraftPerfController *getAircraftPerfController();
  static SearchController *getSearchController();
  static const atools::fs::perf::AircraftPerf& getAircraftPerformance();

  static TrackController *getTrackController();
  static bool hasTracks();
  static bool hasNatTracks();
  static bool hasPacotsTracks();
  static bool hasTracksEnabled();
  static TrackManager *getTrackManager();

  static AirspaceController *getAirspaceController();

  static atools::fs::common::MagDecReader *getMagDecReader();

  static atools::fs::common::MoraReader *getMoraReader();

  static VehicleIcons *getVehicleIcons();

  /* Not entirely reliable since other modules might be initialized later */
  static bool isLoadingDatabase();

  static QString getCurrentGuiStyleDisplayName();
  static bool isGuiStyleDark();

  static bool isDarkMapTheme();

  static StyleHandler *getStyleHandler();

  static map::MapWeatherSource getMapWeatherSourceText();
  static map::MapWeatherSource getMapWeatherSource();
  static bool isMapWeatherShown();

  static const QString& getCurrentRouteFilePath();
  static const QString& getCurrentAircraftPerfFilePath();
  static const QString& getCurrentAircraftPerfName();
  static const QString& getCurrentAircraftPerfAircraftType();

  static bool isWebControllerRunning();
  static WebController *getWebController();
  static MapPaintWidget *getMapPaintWidgetWeb();

  static MapMarkHandler *getMapMarkHandler();
  static MapAirportHandler *getMapAirportHandler();
  static MapDetailHandler *getMapDetailHandler();

  static void showFlightplan();
  static void showAircraftPerformance();
  static void showLogbookSearch();
  static void showUserpointSearch();

  /* Creates a lock file and shows a warning dialog if this is already present from a former crash.
   * Sets safe mode if user chooses to skip file loading.
   * Always creates a crash report in case of previous unsafe exit. */
  static void recordStartNavApp();

  /* Create manual issue report */
  static void createIssueReport(const QStringList& additionalFiles);

  /* true if tooltips in menus are visible */
  static bool isMenuToolTipsVisible();

  /* Fake aircraft movement enabled in debug menu */
  static bool isDebugMovingAircraft();

  static void setToolTipsEnabledMainMenu(bool enabled);

  static const atools::gui::DataExchange *getDataExchangeConst();
  static atools::gui::DataExchange *getDataExchange();
  static bool initDataExchange();
  static void deInitDataExchange();

private:
  static void initApplication();
  static void readMagDecFromDatabase();
  static void getReportFiles(QStringList& crashReportFiles, QString& reportFilename, bool issueReport);

  /* Database query helpers and caches */
  static ElevationProvider *elevationProvider;

  /* Most important handlers */
  static ConnectClient *connectClient;
  static DatabaseManager *databaseManager;
  static atools::fs::common::MagDecReader *magDecReader;

  /* minimum off route altitude from nav database */
  static atools::fs::common::MoraReader *moraReader;
  static UserdataController *userdataController;
  static MapMarkHandler *mapMarkHandler;
  static MapAirportHandler *mapAirportHandler;
  static MapDetailHandler *mapDetailHandler;
  static LogdataController *logdataController;
  static OnlinedataController *onlinedataController;
  static TrackController *trackController;
  static AircraftPerfController *aircraftPerfController;
  static AirspaceController *airspaceController;

  /* Main window is not aggregated */
  static MainWindow *mainWindow;

  static atools::fs::db::DatabaseMeta *databaseMetaSim;
  static atools::fs::db::DatabaseMeta *databaseMetaNav;

  static UpdateHandler *updateHandler;
  static VehicleIcons *vehicleIcons;
  static StyleHandler *styleHandler;

  static WebController *webController;

  static atools::gui::DataExchange *dataExchange;

  static bool loadingDatabase;
  static bool closeCalled;
  static bool mainWindowVisible;
};

#endif // NAVAPPLICATION_H
