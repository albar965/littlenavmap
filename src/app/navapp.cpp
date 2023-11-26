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

#include "app/navapp.h"

#include "airspace/airspacecontroller.h"
#include "atools.h"
#include "common/aircrafttrail.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/updatehandler.h"
#include "common/vehicleicons.h"
#include "connect/connectclient.h"
#include "db/databasemanager.h"
#include "exception.h"
#include "fs/perf/aircraftperf.h"
#include "fs/common/magdecreader.h"
#include "fs/common/morareader.h"
#include "fs/db/databasemeta.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/stylehandler.h"
#include "mapgui/mapthemehandler.h"
#include "route/routealtitude.h"
#include "util/properties.h"
#include "logbook/logdatacontroller.h"
#include "logging/logginghandler.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/mapdetailhandler.h"
#include "mapgui/mapwidget.h"
#include "online/onlinedatacontroller.h"
#include "perf/aircraftperfcontroller.h"
#include "profile/profilewidget.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "query/waypointtrackquery.h"
#include "route/routecontroller.h"
#include "routestring/routestringwriter.h"
#include "search/searchcontroller.h"
#include "track/trackcontroller.h"
#include "userdata/userdatacontroller.h"
#include "weather/weatherreporter.h"
#include "web/webcontroller.h"
#include "web/webmapcontroller.h"
#include "app/dataexchange.h"
#include "settings/settings.h"

#include "ui_mainwindow.h"

#include <marble/MarbleModel.h>

#include <QIcon>
#include <QSplashScreen>

AirportQuery *NavApp::airportQuerySim = nullptr;
AirportQuery *NavApp::airportQueryNav = nullptr;
InfoQuery *NavApp::infoQuery = nullptr;
ProcedureQuery *NavApp::procedureQuery = nullptr;

ConnectClient *NavApp::connectClient = nullptr;
DatabaseManager *NavApp::databaseManager = nullptr;
MainWindow *NavApp::mainWindow = nullptr;
ElevationProvider *NavApp::elevationProvider = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMetaSim = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMetaNav = nullptr;
QSplashScreen *NavApp::splashScreen = nullptr;

atools::fs::common::MagDecReader *NavApp::magDecReader = nullptr;
atools::fs::common::MoraReader *NavApp::moraReader = nullptr;
UpdateHandler *NavApp::updateHandler = nullptr;
UserdataController *NavApp::userdataController = nullptr;
MapMarkHandler *NavApp::mapMarkHandler = nullptr;
MapAirportHandler *NavApp::mapAirportHandler = nullptr;
MapDetailHandler *NavApp::mapDetailHandler = nullptr;
LogdataController *NavApp::logdataController = nullptr;
OnlinedataController *NavApp::onlinedataController = nullptr;
TrackController *NavApp::trackController = nullptr;
AircraftPerfController *NavApp::aircraftPerfController = nullptr;
AirspaceController *NavApp::airspaceController = nullptr;
VehicleIcons *NavApp::vehicleIcons = nullptr;
StyleHandler *NavApp::styleHandler = nullptr;

WebController *NavApp::webController = nullptr;

atools::util::Properties *NavApp::startupOptions = nullptr;
DataExchange *NavApp::dataExchange = nullptr;

bool NavApp::closeCalled = false;
bool NavApp::shuttingDown = false;
bool NavApp::loadingDatabase = false;
bool NavApp::mainWindowVisible = false;

using atools::settings::Settings;

NavApp::NavApp(int& argc, char **argv, int flags)
  : atools::gui::Application(argc, argv, flags)
{
  startupOptions = new atools::util::Properties;
  initApplication();
}

NavApp::~NavApp()
{
  ATOOLS_DELETE(dataExchange);
  ATOOLS_DELETE(startupOptions);
}

void NavApp::initApplication()
{
  setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  setApplicationName("Little Navmap");
  setOrganizationName("ABarthel");
  setOrganizationDomain("littlenavmap.org");
  setApplicationVersion(VERSION_NUMBER_LITTLENAVMAP);
}

NavApp *NavApp::navAppInstance()
{
  return dynamic_cast<NavApp *>(QCoreApplication::instance());
}

void NavApp::init(MainWindow *mainWindowParam)
{
  qDebug() << Q_FUNC_INFO;

  NavApp::mainWindow = mainWindowParam;

  elevationProvider = new ElevationProvider(mainWindow);

  databaseManager = new DatabaseManager(mainWindow);
  databaseManager->openAllDatabases(); // Only readonly databases
  databaseManager->loadLanguageIndex(); // MSFS translations from table "translation"
  databaseManager->loadAircraftIndex(); // MSFS aircraft.cfg properties

  userdataController = new UserdataController(databaseManager->getUserdataManager(), mainWindow);
  logdataController = new LogdataController(databaseManager->getLogdataManager(), mainWindow);

  mapMarkHandler = new MapMarkHandler(mainWindow);
  mapAirportHandler = new MapAirportHandler(mainWindow);
  mapDetailHandler = new MapDetailHandler(mainWindow);

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  magDecReader = new atools::fs::common::MagDecReader();
  readMagDecFromDatabase();

  moraReader = new atools::fs::common::MoraReader(getDatabaseNav(), getDatabaseSim());
  moraReader->readFromTable();

  vehicleIcons = new VehicleIcons();

  // Need to set this later to avoid circular database dependency
  userdataController->setMagDecReader(magDecReader);

  // Create a CSV backup - not needed since the database is backed up now
  // userdataController->backup();
  // Clear temporary userpoints
  userdataController->clearTemporary();

  onlinedataController = new OnlinedataController(databaseManager->getOnlinedataManager(), mainWindow);

  trackController = new TrackController(databaseManager->getTrackManager(), mainWindow);

  aircraftPerfController = new AircraftPerfController(mainWindow);

  airspaceController = new AirspaceController(mainWindow,
                                              databaseManager->getDatabaseSimAirspace(),
                                              databaseManager->getDatabaseNavAirspace(),
                                              databaseManager->getDatabaseUserAirspace(),
                                              databaseManager->getDatabaseOnline());

  airportQuerySim = new AirportQuery(databaseManager->getDatabaseSim(), false /* nav */);

  airportQueryNav = new AirportQuery(databaseManager->getDatabaseNav(), true /* nav */);

  infoQuery = new InfoQuery(databaseManager->getDatabaseSim(),
                            databaseManager->getDatabaseNav(),
                            databaseManager->getDatabaseTrack());

  procedureQuery = new ProcedureQuery(databaseManager->getDatabaseNav());

  connectClient = new ConnectClient(mainWindow);

  updateHandler = new UpdateHandler(mainWindow);

  styleHandler = new StyleHandler(mainWindow);

  webController = new WebController(mainWindow);
}

void NavApp::initQueries()
{
  qDebug() << Q_FUNC_INFO;
  onlinedataController->initQueries();
  airportQuerySim->initQueries();
  airportQueryNav->initQueries();
  infoQuery->initQueries();
  procedureQuery->initQueries();
}

void NavApp::initElevationProvider()
{
  qDebug() << Q_FUNC_INFO;
  if(elevationProvider != nullptr)
    elevationProvider->init(mainWindow->getElevationModel());
}

void NavApp::deInit()
{
  ATOOLS_DELETE_LOG(dataExchange);
  ATOOLS_DELETE_LOG(webController);
  ATOOLS_DELETE_LOG(styleHandler);
  ATOOLS_DELETE_LOG(userdataController);
  ATOOLS_DELETE_LOG(mapMarkHandler);
  ATOOLS_DELETE_LOG(mapAirportHandler);
  ATOOLS_DELETE_LOG(mapDetailHandler);
  ATOOLS_DELETE_LOG(logdataController);
  ATOOLS_DELETE_LOG(onlinedataController);
  ATOOLS_DELETE_LOG(trackController);
  ATOOLS_DELETE_LOG(aircraftPerfController);
  ATOOLS_DELETE_LOG(airspaceController);
  ATOOLS_DELETE_LOG(updateHandler);
  ATOOLS_DELETE_LOG(connectClient);
  ATOOLS_DELETE_LOG(elevationProvider);
  ATOOLS_DELETE_LOG(airportQuerySim);
  ATOOLS_DELETE_LOG(airportQueryNav);
  ATOOLS_DELETE_LOG(infoQuery);
  ATOOLS_DELETE_LOG(procedureQuery);
  ATOOLS_DELETE_LOG(databaseManager);
  ATOOLS_DELETE_LOG(databaseMetaSim);
  ATOOLS_DELETE_LOG(databaseMetaNav);
  ATOOLS_DELETE_LOG(magDecReader);
  ATOOLS_DELETE_LOG(moraReader);
  ATOOLS_DELETE_LOG(vehicleIcons);
  ATOOLS_DELETE_LOG(splashScreen);
}

const DataExchange *NavApp::getDataExchangeConst()
{
  return dataExchange;
}

DataExchange *NavApp::getDataExchange()
{
  return dataExchange;
}

bool NavApp::initDataExchange()
{
  if(dataExchange == nullptr)
    dataExchange = new DataExchange;

  return dataExchange->isExit();
}

void NavApp::deInitDataExchange()
{
  ATOOLS_DELETE_LOG(dataExchange);
}

void NavApp::checkForUpdates(int channelOpts, bool manual, bool startup, bool forceDebug)
{
  UpdateHandler::UpdateReason reason = UpdateHandler::UPDATE_REASON_UNKNOWN;
  if(manual)
    reason = UpdateHandler::UPDATE_REASON_MANUAL;
  else if(startup)
    reason = UpdateHandler::UPDATE_REASON_STARTUP;
  else if(forceDebug)
    reason = UpdateHandler::UPDATE_REASON_FORCE;

  updateChannels(channelOpts);
  updateHandler->checkForUpdates(reason);
}

void NavApp::updateChannels(int channelOpts)
{
  updateHandler->setChannelOpts(static_cast<opts::UpdateChannels>(channelOpts));
}

void NavApp::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;
}

void NavApp::readMagDecFromDatabase()
{
  if(hasDataInDatabase())
  {
    try
    {
      magDecReader->readFromTable(*getDatabaseSim());
    }
    catch(atools::Exception& e)
    {
      closeSplashScreen();
      // Show dialog if something went wrong but do not exit
      atools::gui::ErrorHandler(mainWindow).handleException(e, tr("While reading magnetic declination from database:"));
    }
    catch(...)
    {
      closeSplashScreen();
      atools::gui::ErrorHandler(mainWindow).
      handleUnknownException(tr("While reading magnetic declination from database:"));
    }
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Empty database falling back to WMM";
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    magDecReader->readFromWmm();
    QGuiApplication::restoreOverrideCursor();
  }

  qDebug() << Q_FUNC_INFO << "Mag decl ref date" << magDecReader->getReferenceDate()
           << magDecReader->getWmmVersion();
}

void NavApp::setMainWindowVisible()
{
  qDebug() << Q_FUNC_INFO;
  mainWindowVisible = true;
}

void NavApp::setStayOnTop(QWidget *widget)
{
  mainWindow->setStayOnTop(widget);
}

void NavApp::preDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  loadingDatabase = true;
  infoQuery->deInitQueries();
  airportQuerySim->deInitQueries();
  airportQueryNav->deInitQueries();
  procedureQuery->deInitQueries();
  moraReader->preDatabaseLoad();
  airspaceController->preDatabaseLoad();
  trackController->preDatabaseLoad();
  logdataController->preDatabaseLoad();

  delete databaseMetaSim;
  databaseMetaSim = nullptr;

  delete databaseMetaNav;
  databaseMetaNav = nullptr;
}

void NavApp::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  readMagDecFromDatabase();

  airportQuerySim->initQueries();
  airportQueryNav->initQueries();
  infoQuery->initQueries();
  procedureQuery->initQueries();
  moraReader->readFromTable(getDatabaseNav(), getDatabaseSim());
  airspaceController->postDatabaseLoad();
  logdataController->postDatabaseLoad();
  trackController->postDatabaseLoad();
  loadingDatabase = false;
}

Ui::MainWindow *NavApp::getMainUi()
{
  return mainWindow->getUi();
}

bool NavApp::isMainWindowVisible()
{
  return mainWindowVisible;
}

bool NavApp::isFetchAiAircraft()
{
  return connectClient->isFetchAiAircraft();
}

bool NavApp::isFetchAiShip()
{
  return connectClient->isFetchAiShip();
}

bool NavApp::isConnectedActive()
{
  return connectClient->isConnectedActive();
}

bool NavApp::isConnected()
{
  return connectClient->isConnected();
}

bool NavApp::isNetworkConnect()
{
  return connectClient->isNetworkConnect();
}

bool NavApp::isXpConnect()
{
  return connectClient->isXpConnect();
}

bool NavApp::isSimConnect()
{
  return connectClient->isSimConnect();
}

bool NavApp::isConnectedAndAircraftFlying()
{
  return (isConnected() && isUserAircraftValid() && getUserAircraft().isFlying()) || getUserAircraft().isDebug();
}

bool NavApp::isConnectedAndAircraft()
{
  return (isConnected() && isUserAircraftValid()) || getUserAircraft().isDebug();
}

bool NavApp::isUserAircraftValid()
{
  return mainWindow->getMapWidget()->getUserAircraft().isFullyValid();
}

bool NavApp::isMoraAvailable()
{
  return moraReader->isDataAvailable();
}

bool NavApp::isHoldingsAvailable()
{
  return getMapQueryGui()->hasHoldings();
}

bool NavApp::isAirportMsaAvailable()
{
  return getMapQueryGui()->hasAirportMsa();
}

bool NavApp::isGlsAvailable()
{
  return getMapQueryGui()->hasGls();
}

float NavApp::getTakeoffFlownDistanceNm()
{
  return mainWindow->getMapWidget()->getTakeoffFlownDistanceNm();
}

QDateTime NavApp::getTakeoffDateTime()
{
  return mainWindow->getMapWidget()->getTakeoffDateTime();
}

const atools::fs::sc::SimConnectUserAircraft& NavApp::getUserAircraft()
{
  return mainWindow->getMapWidget()->getUserAircraft();
}

const atools::fs::sc::SimConnectData& NavApp::getSimConnectData()
{
  return mainWindow->getMapWidget()->getSimConnectData();
}

const atools::geo::Pos& NavApp::getUserAircraftPos()
{
  return mainWindow->getMapWidget()->getUserAircraft().getPosition();
}

void NavApp::updateAllMaps()
{
  if(mainWindow->getMapWidget() != nullptr)
    mainWindow->getMapWidget()->update();

  if(mainWindow->getProfileWidget() != nullptr)
    mainWindow->getProfileWidget()->update();
}

const QVector<atools::fs::sc::SimConnectAircraft>& NavApp::getAiAircraft()
{
  return mainWindow->getMapWidget()->getAiAircraft();
}

AirportQuery *NavApp::getAirportQuerySim()
{
  return airportQuerySim;
}

AirportQuery *NavApp::getAirportQueryNav()
{
  return airportQueryNav;
}

MapQuery *NavApp::getMapQueryGui()
{
  return getMapPaintWidgetGui()->getMapQuery();
}

AirwayTrackQuery *NavApp::getAirwayTrackQueryGui()
{
  return getMapPaintWidgetGui()->getAirwayTrackQuery();
}

WaypointTrackQuery *NavApp::getWaypointTrackQueryGui()
{
  return getMapPaintWidgetGui()->getWaypointTrackQuery();
}

atools::geo::Pos NavApp::getAirportPos(const QString& ident)
{
  return airportQuerySim->getAirportPosByIdent(ident);
}

InfoQuery *NavApp::getInfoQuery()
{
  return infoQuery;
}

ProcedureQuery *NavApp::getProcedureQuery()
{
  return procedureQuery;
}

const Route& NavApp::getRouteConst()
{
  return mainWindow->getRouteController()->getRouteConst();
}

Route& NavApp::getRoute()
{
  return mainWindow->getRouteController()->getRoute();
}

void NavApp::updateRouteCycleMetadata()
{
  getRoute().updateRouteCycleMetadata();
}

QString NavApp::getRouteStringLogbook()
{
  return RouteStringWriter().createStringForRoute(getRouteConst(), NavApp::getRouteCruiseSpeedKts(), rs::DEFAULT_OPTIONS_LOGBOOK);
}

const atools::geo::Rect& NavApp::getRouteRect()
{
  return mainWindow->getRouteController()->getRouteConst().getBoundingRect();
}

const RouteAltitude& NavApp::getAltitudeLegs()
{
  return mainWindow->getRouteController()->getRouteConst().getAltitudeLegs();
}

float NavApp::getGroundBufferForLegFt(int legIndex)
{
  if(mainWindow->getProfileWidget() != nullptr)
    return mainWindow->getProfileWidget()->getGroundBufferForLegFt(legIndex);
  else
    return map::INVALID_ALTITUDE_VALUE;
}

float NavApp::getRouteCruiseSpeedKts()
{
  return aircraftPerfController->getRouteCruiseSpeedKts();
}

float NavApp::getRouteCruiseAltitudeFt()
{
  return getRoute().getCruiseAltitudeFt();
}

float NavApp::getRouteCruiseAltFtWidget()
{
  return getRouteController()->getCruiseAltitudeWidget();
}

bool NavApp::isRouteEmpty()
{
  return getRouteConst().isEmpty();
}

bool NavApp::isValidProfile()
{
  return getRouteConst().getAltitudeLegs().isValidProfile();
}

atools::fs::FsPaths::SimulatorType NavApp::getCurrentSimulatorDb()
{
  return getDatabaseManager()->getCurrentSimulator();
}

bool NavApp::isAirportDatabaseXPlane(bool navdata)
{
  return getDatabaseManager()->isAirportDatabaseXPlane(navdata);
}

QString NavApp::getCurrentSimulatorFilesPath()
{
  return atools::fs::FsPaths::getFilesPath(getCurrentSimulatorDb());
}

QString NavApp::getCurrentSimulatorBasePath()
{
  return databaseManager->getCurrentSimulatorBasePath();
}

QString NavApp::getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type)
{
  return databaseManager->getSimulatorBasePath(type);
}

QString NavApp::getSimulatorFilesPathBest(const QVector<atools::fs::FsPaths::SimulatorType>& types, const QString& defaultPath)
{
  return databaseManager->getSimulatorFilesPathBest(types, defaultPath);
}

QString NavApp::getSimulatorBasePathBest(const QVector<atools::fs::FsPaths::SimulatorType>& types)
{
  return databaseManager->getSimulatorBasePathBest(types);
}

bool NavApp::hasSimulator(atools::fs::FsPaths::SimulatorType type)
{
  return atools::fs::FsPaths::hasSimulator(type);
}

bool NavApp::hasInstalledSimulator(atools::fs::FsPaths::SimulatorType type)
{
  return databaseManager->hasInstalledSimulator(type);
}

bool NavApp::hasAnyMsSimulator()
{
  return atools::fs::FsPaths::hasAnyMsSimulator();
}

bool NavApp::hasAnyXplaneSimulator()
{
  return atools::fs::FsPaths::hasAnyXplaneSimulator();

}

bool NavApp::hasXplane11Simulator()
{
  return atools::fs::FsPaths::hasXplane11Simulator();
}

bool NavApp::hasXplane12Simulator()
{
  return atools::fs::FsPaths::hasXplane12Simulator();
}

bool NavApp::isNavdataAll()
{
  return databaseManager->getNavDatabaseStatus() == navdb::ALL;
}

bool NavApp::isNavdataMixed()
{
  return databaseManager->getNavDatabaseStatus() == navdb::MIXED;
}

bool NavApp::isNavdataOff()
{
  return databaseManager->getNavDatabaseStatus() == navdb::OFF;
}

OptionsDialog *NavApp::getOptionsDialog()
{
  return mainWindow->getOptionsDialog();
}

QString NavApp::getCurrentSimulatorShortName()
{
  return atools::fs::FsPaths::typeToShortName(getCurrentSimulatorDb());
}

QString NavApp::getCurrentSimulatorName()
{
  return atools::fs::FsPaths::typeToDisplayName(getCurrentSimulatorDb());
}

bool NavApp::hasSidStarInDatabase()
{
  return databaseMetaNav != nullptr ? databaseMetaNav->hasSidStar() : false;
}

bool NavApp::hasRouteTypeInDatabase()
{
  return databaseMetaNav != nullptr ? databaseMetaNav->hasRouteType() : false;
}

bool NavApp::hasDataInDatabase()
{
  return databaseMetaSim != nullptr ? databaseMetaSim->hasData() : false;
}

bool NavApp::hasDataInSimDatabase()
{
  return databaseManager->hasDataInSimDatabase();
}

void NavApp::logDatabaseMeta()
{
  qDebug() << Q_FUNC_INFO << "databaseMetaNav";
  if(databaseMetaNav != nullptr)
    databaseMetaNav->logInfo();
  else
    qDebug() << Q_FUNC_INFO << "databaseMetaNav == nullptr";

  qDebug() << Q_FUNC_INFO << "databaseMetaSim";
  if(databaseMetaSim != nullptr)
    databaseMetaSim->logInfo();
  else
    qDebug() << Q_FUNC_INFO << "databaseMetaSim == nullptr";
}

atools::sql::SqlDatabase *NavApp::getDatabaseSim()
{
  return getDatabaseManager()->getDatabaseSim();
}

atools::sql::SqlDatabase *NavApp::getDatabaseNav()
{
  return getDatabaseManager()->getDatabaseNav();
}

atools::fs::userdata::UserdataManager *NavApp::getUserdataManager()
{
  return databaseManager->getUserdataManager();
}

UserdataIcons *NavApp::getUserdataIcons()
{
  return userdataController->getUserdataIcons();
}

UserdataSearch *NavApp::getUserdataSearch()
{
  return mainWindow->getSearchController()->getUserdataSearch();
}

TrackManager *NavApp::getTrackManager()
{
  return databaseManager->getTrackManager();
}

LogdataSearch *NavApp::getLogdataSearch()
{
  return mainWindow->getSearchController()->getLogdataSearch();
}

atools::sql::SqlDatabase *NavApp::getDatabaseTrack()
{
  return getDatabaseManager()->getDatabaseTrack();
}

atools::fs::userdata::LogdataManager *NavApp::getLogdataManager()
{
  return databaseManager->getLogdataManager();
}

UserdataController *NavApp::getUserdataController()
{
  return userdataController;
}

MapMarkHandler *NavApp::getMapMarkHandler()
{
  return mapMarkHandler;
}

MapAirportHandler *NavApp::getMapAirportHandler()
{
  return mapAirportHandler;
}

MapDetailHandler *NavApp::getMapDetailHandler()
{
  return mapDetailHandler;
}

void NavApp::showFlightPlan()
{
  mainWindow->showFlightPlan();
}

void NavApp::showAircraftPerformance()
{
  mainWindow->showAircraftPerformance();
}

void NavApp::showLogbookSearch()
{
  mainWindow->showLogbookSearch();
}

void NavApp::showUserpointSearch()
{
  mainWindow->showUserpointSearch();
}

QString NavApp::getStartupOptionStr(const QString& key)
{
  return startupOptions->getPropertyStr(key);
}

QStringList NavApp::getStartupOptionStrList(const QString& key)
{
  return startupOptions->getPropertyStrList(key);
}

void NavApp::addStartupOptionStr(const QString& key, const QString& value)
{
  startupOptions->setPropertyStr(key, value);
}

void NavApp::addStartupOptionStrList(const QString& key, const QStringList& value)
{
  startupOptions->setPropertyStrList(key, value);
}

void NavApp::clearStartupOptions()
{
  startupOptions->clear();
}

void getCrashReportFiles(QStringList& crashReportFiles, QString& reportFilename, bool manual)
{
  // Collect all files which should be skipped on startup
  Settings& settings = Settings::instance();
  crashReportFiles.append(settings.valueStr(lnm::ROUTE_FILENAME));
  crashReportFiles.append(settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME));
  crashReportFiles.append(Settings::getConfigFilename(lnm::AIRCRAFT_TRACK_SUFFIX));
  crashReportFiles.append(Settings::getFilename());
  crashReportFiles.append(Settings::getConfigFilename(".lnmpln"));

  // Add log files last to catch any error which appear while compressing
  crashReportFiles.append(atools::logging::LoggingHandler::getLogFiles());

  reportFilename = Settings::getConfigFilename(manual ? "_issuereport.zip" : "_crashreport.zip", "crashreports");
}

void NavApp::recordStartNavApp()
{
#ifndef DEBUG_DISABLE_CRASH_REPORT

  QStringList crashReportFiles;
  QString reportFilename;
  getCrashReportFiles(crashReportFiles, reportFilename, false /* manual */);

  Application::recordStart(nullptr, Settings::getConfigFilename(".running"), reportFilename, crashReportFiles,
                           lnm::helpOnlineUrl, "en");
#endif
  // Keep command line options to avoid using the wrong configuration folder
}

QString NavApp::buildCrashReportNavAppManual()
{
  QString reportFilename;
  QStringList crashReportFiles;
  getCrashReportFiles(crashReportFiles, reportFilename, true /* manual */);

  Application::buildCrashReport(reportFilename, crashReportFiles);
  return reportFilename;
}

void NavApp::setToolTipsEnabledMainMenu(bool enabled)
{
  // Enable tooltips for all menus
  // The state is set programmatically in context menus from first file menu
  QList<QAction *> stack;
  stack.append(NavApp::getMainUi()->menuBar->actions());
  while(!stack.isEmpty())
  {
    QMenu *menu = stack.takeLast()->menu();
    if(menu != nullptr)
    {
      menu->setToolTipsVisible(enabled);
      const QList<QAction *> actions = menu->actions();
      for(QAction *sub : actions)
      {
        if(sub->menu() != nullptr)
          stack.append(sub);
      }
    }
  }

  if(mainWindow != nullptr)
    // Need to update the recent menus
    mainWindow->setToolTipsEnabledMainMenu(enabled);
}

atools::util::Properties& NavApp::getStartupOptionsConst()
{
  return *startupOptions;
}

LogdataController *NavApp::getLogdataController()
{
  return logdataController;
}

OnlinedataController *NavApp::getOnlinedataController()
{
  return onlinedataController;
}

TrackController *NavApp::getTrackController()
{
  return trackController;
}

bool NavApp::hasTracks()
{
  return trackController->hasTracks();
}

bool NavApp::hasTracksEnabled()
{
  return trackController->hasTracksEnabled();
}

AircraftPerfController *NavApp::getAircraftPerfController()
{
  return aircraftPerfController;
}

SearchController *NavApp::getSearchController()
{
  return mainWindow->getSearchController();
}

const atools::fs::perf::AircraftPerf& NavApp::getAircraftPerformance()
{
  return aircraftPerfController->getAircraftPerformance();
}

AirspaceController *NavApp::getAirspaceController()
{
  return airspaceController;
}

bool NavApp::hasAnyAirspaces()
{
  return getAirspaceController()->hasAnyAirspaces();
}

atools::fs::common::MagDecReader *NavApp::getMagDecReader()
{
  return magDecReader;
}

VehicleIcons *NavApp::getVehicleIcons()
{
  return vehicleIcons;
}

bool NavApp::isLoadingDatabase()
{
  return loadingDatabase;
}

QString NavApp::getCurrentGuiStyleDisplayName()
{
  return styleHandler->getCurrentGuiStyleDisplayName();
}

bool NavApp::isCurrentGuiStyleNight()
{
  return styleHandler->isCurrentGuiStyleNight();
}

StyleHandler *NavApp::getStyleHandler()
{
  return styleHandler;
}

atools::fs::common::MoraReader *NavApp::getMoraReader()
{
  return moraReader;
}

atools::sql::SqlDatabase *NavApp::getDatabaseUser()
{
  return databaseManager->getDatabaseUser();
}

atools::sql::SqlDatabase *NavApp::getDatabaseUserAirspace()
{
  return databaseManager->getDatabaseUserAirspace();
}

atools::sql::SqlDatabase *NavApp::getDatabaseLogbook()
{
  return databaseManager->getDatabaseLogbook();
}

atools::sql::SqlDatabase *NavApp::getDatabaseOnline()
{
  return onlinedataController->getDatabase();
}

ElevationProvider *NavApp::getElevationProvider()
{
  return elevationProvider;
}

bool NavApp::isGlobeOfflineProvider()
{
  return elevationProvider->isGlobeOfflineProvider();
}

bool NavApp::isGlobeDirValid()
{
  return ElevationProvider::isGlobeDirValid();
}

WeatherReporter *NavApp::getWeatherReporter()
{
  return mainWindow != nullptr ? mainWindow->getWeatherReporter() : nullptr;
}

WeatherContextHandler *NavApp::getWeatherContextHandler()
{
  return mainWindow->getWeatherContextHandler();
}

WindReporter *NavApp::getWindReporter()
{
  return mainWindow->getWindReporter();
}

void NavApp::getAirportWind(int& windDirectionDeg, float& windSpeedKts, const map::MapAirport& airport, bool stationOnly)
{
  mainWindow->getWeatherReporter()->getAirportWind(windDirectionDeg, windSpeedKts, airport, stationOnly);
}

map::MapWeatherSource NavApp::getAirportWeatherSource()
{
  return mainWindow->getMapWidget()->getMapWeatherSource();
}

void NavApp::updateWindowTitle()
{
  mainWindow->updateWindowTitle();
}

void NavApp::updateErrorLabel()
{
  getRouteController()->updateFooterErrorLabel();
}

void NavApp::setStatusMessage(const QString& message, bool addToLog)
{
  mainWindow->setStatusMessage(message, addToLog);
}

QWidget *NavApp::getQMainWidget()
{
  return mainWindow;
}

QMainWindow *NavApp::getQMainWindow()
{
  return mainWindow;
}

MainWindow *NavApp::getMainWindow()
{
  return mainWindow;
}

void NavApp::addDialogToDockHandler(QDialog *dialog)
{
  mainWindow->addDialogToDockHandler(dialog);
}

void NavApp::removeDialogFromDockHandler(QDialog *dialog)
{
  mainWindow->removeDialogFromDockHandler(dialog);
}

QList<QAction *> NavApp::getMainWindowActions()
{
  return mainWindow->getMainWindowActions();
}

bool NavApp::isMenuToolTipsVisible()
{
  return getMainUi()->menuFile->toolTipsVisible();
}

MapWidget *NavApp::getMapWidgetGui()
{
  return mainWindow->getMapWidget();
}

MapPaintWidget *NavApp::getMapPaintWidgetGui()
{
  return mainWindow->getMapWidget();
}

map::MapWeatherSource NavApp::getMapWeatherSource()
{
  return mainWindow->getMapWidget()->getMapWeatherSource();
}

bool NavApp::isMapWeatherShown()
{
  return getMainUi()->actionMapShowAirportWeather->isChecked();
}

RouteController *NavApp::getRouteController()
{
  return mainWindow->getRouteController();
}

atools::gui::TabWidgetHandler *NavApp::getRouteTabHandler()
{
  return getRouteController()->getTabHandler();
}

const InfoController *NavApp::getInfoController()
{
  return mainWindow->getInfoController();
}

QFont NavApp::getTextBrowserInfoFont()
{
  return getMainUi()->textBrowserAirportInfo->font();
}

MapThemeHandler *NavApp::getMapThemeHandler()
{
  return mainWindow->getMapThemeHandler();
}

bool NavApp::isDarkMapTheme()
{
  return getMapThemeHandler()->isDarkTheme(getMapPaintWidgetGui()->getCurrentThemeId());
}

const QString& NavApp::getCurrentRouteFilepath()
{
  return mainWindow->getRouteController()->getRouteFilepath();
}

const QString& NavApp::getCurrentAircraftPerfFilepath()
{
  return aircraftPerfController->getCurrentFilepath();
}

const QString& NavApp::getCurrentAircraftPerfName()
{
  return aircraftPerfController->getAircraftPerformance().getName();
}

const QString& NavApp::getCurrentAircraftPerfAircraftType()
{
  return aircraftPerfController->getAircraftPerformance().getAircraftType();
}

WebController *NavApp::getWebController()
{
  return webController;
}

MapPaintWidget *NavApp::getMapPaintWidgetWeb()
{
  if(webController != nullptr && webController->getWebMapController() != nullptr)
    return webController->getWebMapController()->getMapPaintWidget();
  else
    return nullptr;
}

DatabaseManager *NavApp::getDatabaseManager()
{
  return databaseManager;
}

const atools::fs::scenery::LanguageJson& NavApp::getLanguageIndex()
{
  return databaseManager->getLanguageIndex();
}

atools::fs::scenery::AircraftIndex& NavApp::getAircraftIndex()
{
  return databaseManager->getAircraftIndex();
}

ConnectClient *NavApp::getConnectClient()
{
  return connectClient;
}

QString NavApp::getDatabaseAiracCycleSim()
{
  return databaseMetaSim != nullptr ? databaseMetaSim->getAiracCycle() : QString();
}

QString NavApp::getDatabaseAiracCycleNav()
{
  return databaseMetaNav != nullptr ? databaseMetaNav->getAiracCycle() : QString();
}

bool NavApp::hasOnlineData()
{
  return onlinedataController->hasData();
}

QString NavApp::getOnlineNetworkTranslated()
{
  return onlinedataController->getNetworkTranslated();
}

bool NavApp::isOnlineNetworkActive()
{
  return onlinedataController->isNetworkActive();
}

const atools::fs::db::DatabaseMeta *NavApp::getDatabaseMetaSim()
{
  return databaseMetaSim;
}

const atools::fs::db::DatabaseMeta *NavApp::getDatabaseMetaNav()
{
  return databaseMetaNav;
}

const AircraftTrail& NavApp::getAircraftTrail()
{
  return getMapWidgetGui()->getAircraftTrail();
}

const AircraftTrail& NavApp::getAircraftTrailLogbook()
{
  return getMapWidgetGui()->getAircraftTrailLogbook();
}

void NavApp::deleteAircraftTrailLogbook()
{
  return getMapWidgetGui()->deleteAircraftTrailLogbook();
}

bool NavApp::isAircraftTrailEmpty()
{
  return getAircraftTrail().isEmpty();
}

map::MapTypes NavApp::getShownMapTypes()
{
  return mainWindow->getMapWidget()->getShownMapTypes();
}

map::MapDisplayTypes NavApp::getShownMapDisplayTypes()
{
  return mainWindow->getMapWidget()->getShownMapDisplayTypes();
}

const map::MapAirspaceFilter& NavApp::getShownMapAirspaces()
{
  return mainWindow->getMapWidget()->getShownAirspaces();
}

bool NavApp::isShuttingDown()
{
  return shuttingDown;
}

void NavApp::setShuttingDown(bool value)
{
  qDebug() << Q_FUNC_INFO << value;

  shuttingDown = value;
}

bool NavApp::isCloseCalled()
{
  return closeCalled;
}

void NavApp::setCloseCalled(bool value)
{
  closeCalled = value;
}

bool NavApp::isFullScreen()
{
  return mainWindow->isFullScreen();
}

float NavApp::getMagVar(const atools::geo::Pos& pos, float defaultValue)
{
  if(magDecReader != nullptr && magDecReader->isValid())
    return magDecReader->getMagVar(pos);
  else
    return defaultValue;
}

UpdateHandler *NavApp::getUpdateHandler()
{
  return updateHandler;
}

void NavApp::initSplashScreen()
{
  qDebug() << Q_FUNC_INFO;

  QPixmap pixmap(":/littlenavmap/resources/icons/splash.png");
  splashScreen = new QSplashScreen(pixmap);
  splashScreen->show();

  processEvents();

#if defined(WINARCH64)
  QString applicationVersion = QApplication::applicationVersion() + tr(" 64-bit");
#elif defined(WINARCH32)
  QString applicationVersion = QApplication::applicationVersion() + tr(" 32-bit");
#else
  QString applicationVersion = QApplication::applicationVersion();
#endif

  splashScreen->showMessage(QObject::tr("Version %5 (revision %6)").
                            arg(applicationVersion).arg(GIT_REVISION_LITTLENAVMAP),
                            Qt::AlignRight | Qt::AlignBottom, Qt::black);

  processEvents(QEventLoop::ExcludeUserInputEvents);
}

void NavApp::finishSplashScreen()
{
  qDebug() << Q_FUNC_INFO;

  if(splashScreen != nullptr)
    splashScreen->finish(mainWindow);
}

void NavApp::closeSplashScreen()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(splashScreen != nullptr)
    splashScreen->close();
}
