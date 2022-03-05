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

#include "navapp.h"

#include "airspace/airspacecontroller.h"
#include "common/aircrafttrack.h"
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
#include "fs/weather/metar.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/stylehandler.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapairporthandler.h"
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

#include "query/waypointquery.h"
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
LogdataController *NavApp::logdataController = nullptr;
OnlinedataController *NavApp::onlinedataController = nullptr;
TrackController *NavApp::trackController = nullptr;
AircraftPerfController *NavApp::aircraftPerfController = nullptr;
AirspaceController *NavApp::airspaceController = nullptr;
VehicleIcons *NavApp::vehicleIcons = nullptr;
StyleHandler *NavApp::styleHandler = nullptr;

WebController *NavApp::webController = nullptr;

bool NavApp::shuttingDown = false;
bool NavApp::loadingDatabase = false;
bool NavApp::mainWindowVisible = false;

NavApp::NavApp(int& argc, char **argv, int flags)
  : atools::gui::Application(argc, argv, flags)
{
  initApplication();
}

NavApp::~NavApp()
{
}

void NavApp::initApplication()
{
  setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  setApplicationName("Little Navmap");
  setOrganizationName("ABarthel");
  setOrganizationDomain("littlenavmap.org");
  setApplicationVersion("2.7.8.develop"); // VERSION_NUMBER - Little Navmap
}

NavApp *NavApp::navAppInstance()
{
  return dynamic_cast<NavApp *>(QCoreApplication::instance());
}

void NavApp::init(MainWindow *mainWindowParam)
{
  qDebug() << Q_FUNC_INFO;

  NavApp::mainWindow = mainWindowParam;
  databaseManager = new DatabaseManager(mainWindow);
  databaseManager->openAllDatabases(); // Only readonly databases
  databaseManager->loadLanguageIndex(); // MSFS translations from table "translation"

  userdataController = new UserdataController(databaseManager->getUserdataManager(), mainWindow);
  logdataController = new LogdataController(databaseManager->getLogdataManager(), mainWindow);

  mapMarkHandler = new MapMarkHandler(mainWindow);
  mapAirportHandler = new MapAirportHandler(mainWindow);

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  magDecReader = new atools::fs::common::MagDecReader();
  readMagDecFromDatabase();

  moraReader = new atools::fs::common::MoraReader(databaseManager->getDatabaseNav(), databaseManager->getDatabaseSim());
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
  elevationProvider = new ElevationProvider(mainWindow, mainWindow->getElevationModel());
}

void NavApp::deInit()
{
  qDebug() << Q_FUNC_INFO;

  qDebug() << Q_FUNC_INFO << "delete webController";
  delete webController;
  webController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete styleHandler";
  delete styleHandler;
  styleHandler = nullptr;

  qDebug() << Q_FUNC_INFO << "delete userdataController";
  delete userdataController;
  userdataController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete mapMarkHandler";
  delete mapMarkHandler;
  mapMarkHandler = nullptr;

  qDebug() << Q_FUNC_INFO << "delete mapAirportHandler";
  delete mapAirportHandler;
  mapAirportHandler = nullptr;

  qDebug() << Q_FUNC_INFO << "delete logdataController";
  delete logdataController;
  logdataController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete onlinedataController";
  delete onlinedataController;
  onlinedataController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airwayController";
  delete trackController;
  trackController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete aircraftPerfController";
  delete aircraftPerfController;
  aircraftPerfController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airspaceController";
  delete airspaceController;
  airspaceController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete updateHandler";
  delete updateHandler;
  updateHandler = nullptr;

  qDebug() << Q_FUNC_INFO << "delete connectClient";
  delete connectClient;
  connectClient = nullptr;

  qDebug() << Q_FUNC_INFO << "delete elevationProvider";
  delete elevationProvider;
  elevationProvider = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airportQuery";
  delete airportQuerySim;
  airportQuerySim = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airportQueryNav";
  delete airportQueryNav;
  airportQueryNav = nullptr;

  qDebug() << Q_FUNC_INFO << "delete infoQuery";
  delete infoQuery;
  infoQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete approachQuery";
  delete procedureQuery;
  procedureQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete databaseManager";
  delete databaseManager;
  databaseManager = nullptr;

  qDebug() << Q_FUNC_INFO << "delete databaseMeta";
  delete databaseMetaSim;
  databaseMetaSim = nullptr;

  qDebug() << Q_FUNC_INFO << "delete databaseMetaNav";
  delete databaseMetaNav;
  databaseMetaNav = nullptr;

  qDebug() << Q_FUNC_INFO << "delete magDecReader";
  delete magDecReader;
  magDecReader = nullptr;

  qDebug() << Q_FUNC_INFO << "delete moraReader";
  delete moraReader;
  moraReader = nullptr;

  qDebug() << Q_FUNC_INFO << "delete vehicleIcons";
  delete vehicleIcons;
  vehicleIcons = nullptr;

  qDebug() << Q_FUNC_INFO << "delete splashScreen";
  delete splashScreen;
  splashScreen = nullptr;
}

void NavApp::checkForUpdates(int channelOpts, bool manuallyTriggered, bool forceDebug)
{
  updateHandler->checkForUpdates(static_cast<opts::UpdateChannels>(channelOpts), manuallyTriggered, forceDebug);
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
  moraReader->postDatabaseLoad();
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

bool NavApp::isConnectedNetwork()
{
  return connectClient->isConnectedNetwork();
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
  return mainWindow->getRouteController()->getRoute();
}

Route& NavApp::getRoute()
{
  return mainWindow->getRouteController()->getRoute();
}

void NavApp::updateRouteCycleMetadata()
{
  getRoute().updateRouteCycleMetadata();
}

QString NavApp::getRouteString()
{
  return RouteStringWriter().createStringForRoute(getRouteConst(), NavApp::getRouteCruiseSpeedKts(),
                                                  rs::DEFAULT_OPTIONS);
}

const atools::geo::Rect& NavApp::getRouteRect()
{
  return mainWindow->getRouteController()->getRoute().getBoundingRect();
}

const RouteAltitude& NavApp::getAltitudeLegs()
{
  return mainWindow->getRouteController()->getRoute().getAltitudeLegs();
}

float NavApp::getRouteCruiseSpeedKts()
{
  return aircraftPerfController->getRouteCruiseSpeedKts();
}

float NavApp::getRouteCruiseAltFt()
{
  return getRoute().getCruisingAltitudeFeet();
}

float NavApp::getRouteCruiseAltFtWidget()
{
  return getRouteController()->getCruiseAltitudeWidget();
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

QString NavApp::getSimulatorFilesPathBest(const QVector<atools::fs::FsPaths::SimulatorType>& types)
{
  return databaseManager->getSimulatorFilesPathBest(types);
}

QString NavApp::getSimulatorBasePathBest(const QVector<atools::fs::FsPaths::SimulatorType>& types)
{
  return databaseManager->getSimulatorBasePathBest(types);
}

bool NavApp::hasSimulator(atools::fs::FsPaths::SimulatorType type)
{
  return atools::fs::FsPaths::hasSimulator(type);
}

bool NavApp::hasAnyMsSimulator()
{
  return atools::fs::FsPaths::hasAnyMsSimulator();
}

bool NavApp::hasXplaneSimulator()
{
  return atools::fs::FsPaths::hasXplaneSimulator();
}

bool NavApp::isNavdataAll()
{
  return databaseManager->getNavDatabaseStatus() == dm::NAVDATABASE_ALL;
}

bool NavApp::isNavdataMixed()
{
  return databaseManager->getNavDatabaseStatus() == dm::NAVDATABASE_MIXED;
}

bool NavApp::isNavdataOff()
{
  return databaseManager->getNavDatabaseStatus() == dm::NAVDATABASE_OFF;
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
  return atools::fs::FsPaths::typeToName(getCurrentSimulatorDb());
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

void NavApp::showFlightPlan()
{
  mainWindow->showFlightPlan();
}

void NavApp::showRouteCalc()
{
  mainWindow->showRouteCalc();
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

WeatherReporter *NavApp::getWeatherReporter()
{
  return mainWindow != nullptr ? mainWindow->getWeatherReporter() : nullptr;
}

WindReporter *NavApp::getWindReporter()
{
  return mainWindow->getWindReporter();
}

atools::fs::weather::Metar NavApp::getAirportWeather(const QString& airportIcao, const atools::geo::Pos& airportPos)
{
  return mainWindow->getWeatherReporter()->getAirportWeather(airportIcao, airportPos,
                                                             mainWindow->getMapWidget()->getMapWeatherSource());
}

map::MapWeatherSource NavApp::getAirportWeatherSource()
{
  return mainWindow->getMapWidget()->getMapWeatherSource();
}

void NavApp::updateWindowTitle()
{
  mainWindow->updateWindowTitle();
}

void NavApp::updateErrorLabels()
{
  mainWindow->updateErrorLabels();
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

const AircraftTrack& NavApp::getAircraftTrack()
{
  return getMapWidgetGui()->getAircraftTrack();
}

const AircraftTrack& NavApp::getAircraftTrackLogbook()
{
  return getMapWidgetGui()->getAircraftTrackLogbook();
}

void NavApp::deleteAircraftTrackLogbook()
{
  return getMapWidgetGui()->deleteAircraftTrackLogbook();
}

bool NavApp::isAircraftTrackEmpty()
{
  return getAircraftTrack().isEmpty();
}

map::MapTypes NavApp::getShownMapFeatures()
{
  return mainWindow->getMapWidget()->getShownMapFeatures();
}

map::MapAirspaceFilter NavApp::getShownMapAirspaces()
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

  splashScreen->showMessage(QObject::tr("Version %5 (revision %6)").
                            arg(Application::applicationVersion()).arg(GIT_REVISION),
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
