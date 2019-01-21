/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel albar965@mailbox.org
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

#include "query/infoquery.h"
#include "query/procedurequery.h"
#include "connect/connectclient.h"
#include "query/mapquery.h"
#include "query/airspacequery.h"
#include "query/airportquery.h"
#include "db/databasemanager.h"
#include "fs/db/databasemeta.h"
#include "mapgui/mapwidget.h"
#include "gui/mainwindow.h"
#include "route/routecontroller.h"
#include "common/elevationprovider.h"
#include "fs/common/magdecreader.h"
#include "fs/common/morareader.h"
#include "common/updatehandler.h"
#include "userdata/userdatacontroller.h"
#include "online/onlinedatacontroller.h"
#include "search/searchcontroller.h"
#include "common/vehicleicons.h"
#include "mapgui/aprongeometrycache.h"
#include "gui/stylehandler.h"
#include "weather/weatherreporter.h"
#include "fs/weather/metar.h"
#include "perf/aircraftperfcontroller.h"

#include "ui_mainwindow.h"

#include <marble/MarbleModel.h>

#include <QIcon>
#include <QSplashScreen>

AirportQuery *NavApp::airportQuerySim = nullptr;
AirportQuery *NavApp::airportQueryNav = nullptr;
MapQuery *NavApp::mapQuery = nullptr;
AirspaceQuery *NavApp::airspaceQuery = nullptr;
AirspaceQuery *NavApp::airspaceQueryOnline = nullptr;
InfoQuery *NavApp::infoQuery = nullptr;
ProcedureQuery *NavApp::procedureQuery = nullptr;
ApronGeometryCache *NavApp::apronGeometryCache = nullptr;

ConnectClient *NavApp::connectClient = nullptr;
DatabaseManager *NavApp::databaseManager = nullptr;
MainWindow *NavApp::mainWindow = nullptr;
ElevationProvider *NavApp::elevationProvider = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMeta = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMetaNav = nullptr;
QSplashScreen *NavApp::splashScreen = nullptr;

atools::fs::common::MagDecReader *NavApp::magDecReader = nullptr;
atools::fs::common::MoraReader *NavApp::moraReader = nullptr;
UpdateHandler *NavApp::updateHandler = nullptr;
UserdataController *NavApp::userdataController = nullptr;
OnlinedataController *NavApp::onlinedataController = nullptr;
AircraftPerfController *NavApp::aircraftPerfController = nullptr;
VehicleIcons *NavApp::vehicleIcons = nullptr;
StyleHandler *NavApp::styleHandler = nullptr;

bool NavApp::shuttingDown = false;
bool NavApp::loadingDatabase = false;

NavApp::NavApp(int& argc, char **argv, int flags)
  : atools::gui::Application(argc, argv, flags)
{
  setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  setApplicationName("Little Navmap");
  setOrganizationName("ABarthel");
  setOrganizationDomain("abarthel.org");

  setApplicationVersion("2.2.4.rc1"); // VERSION_NUMBER - Little Navmap
}

NavApp::~NavApp()
{
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
  databaseManager->openAllDatabases();
  userdataController = new UserdataController(databaseManager->getUserdataManager(), mainWindow);

  databaseMeta = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  magDecReader = new atools::fs::common::MagDecReader();
  magDecReader->readFromTable(*databaseManager->getDatabaseSim());

  moraReader = new atools::fs::common::MoraReader(databaseManager->getDatabaseMora());
  moraReader->readFromTable();

  vehicleIcons = new VehicleIcons();

  // Need to set this later to avoid circular database dependency
  userdataController->setMagDecReader(magDecReader);

  // Create a CSV backup - not needed since the database is backed up now
  // userdataController->backup();
  // Clear temporary userpoints
  userdataController->clearTemporary();

  onlinedataController = new OnlinedataController(databaseManager->getOnlinedataManager(), mainWindow);
  onlinedataController->initQueries();

  aircraftPerfController = new AircraftPerfController(mainWindow);

  mapQuery = new MapQuery(mainWindow, databaseManager->getDatabaseSim(), databaseManager->getDatabaseNav(),
                          databaseManager->getDatabaseUser());
  mapQuery->initQueries();

  airspaceQuery = new AirspaceQuery(mainWindow, databaseManager->getDatabaseNav(), false /* online database */);
  airspaceQuery->initQueries();

  airspaceQueryOnline = new AirspaceQuery(mainWindow, databaseManager->getDatabaseOnline(), true /* online database */);
  airspaceQueryOnline->initQueries();

  airportQuerySim = new AirportQuery(mainWindow, databaseManager->getDatabaseSim(), false /* nav */);
  airportQuerySim->initQueries();

  airportQueryNav = new AirportQuery(mainWindow, databaseManager->getDatabaseNav(), true /* nav */);
  airportQueryNav->initQueries();

  infoQuery = new InfoQuery(databaseManager->getDatabaseSim(), databaseManager->getDatabaseNav());
  infoQuery->initQueries();

  procedureQuery = new ProcedureQuery(databaseManager->getDatabaseNav());
  procedureQuery->initQueries();

  apronGeometryCache = new ApronGeometryCache();

  connectClient = new ConnectClient(mainWindow);

  updateHandler = new UpdateHandler(mainWindow);

  styleHandler = new StyleHandler();

  // The check will be called on main window shown
}

void NavApp::initElevationProvider()
{
  elevationProvider = new ElevationProvider(mainWindow, mainWindow->getElevationModel());
}

void NavApp::deInit()
{
  qDebug() << Q_FUNC_INFO;

  qDebug() << Q_FUNC_INFO << "delete styleHandler ";
  delete styleHandler;
  styleHandler = nullptr;

  qDebug() << Q_FUNC_INFO << "delete userdataController";
  delete userdataController;
  userdataController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete onlinedataController";
  delete onlinedataController;
  onlinedataController = nullptr;

  qDebug() << Q_FUNC_INFO << "delete aircraftPerfController";
  delete aircraftPerfController;
  aircraftPerfController = nullptr;

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

  qDebug() << Q_FUNC_INFO << "delete mapQuery";
  delete mapQuery;
  mapQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airspaceQuery";
  delete airspaceQuery;
  airspaceQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete airspaceQueryOnline";
  delete airspaceQueryOnline;
  airspaceQueryOnline = nullptr;

  qDebug() << Q_FUNC_INFO << "delete infoQuery";
  delete infoQuery;
  infoQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete approachQuery";
  delete procedureQuery;
  procedureQuery = nullptr;

  qDebug() << Q_FUNC_INFO << "delete apronGeometryCache";
  delete apronGeometryCache;
  apronGeometryCache = nullptr;

  qDebug() << Q_FUNC_INFO << "delete databaseManager";
  delete databaseManager;
  databaseManager = nullptr;

  qDebug() << Q_FUNC_INFO << "delete databaseMeta";
  delete databaseMeta;
  databaseMeta = nullptr;

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

void NavApp::checkForUpdates(int channelOpts, bool manuallyTriggered)
{
  updateHandler->checkForUpdates(static_cast<opts::UpdateChannels>(channelOpts), manuallyTriggered);
}

void NavApp::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;
}

void NavApp::preDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  loadingDatabase = true;
  infoQuery->deInitQueries();
  airportQuerySim->deInitQueries();
  airportQueryNav->deInitQueries();
  mapQuery->deInitQueries();
  airspaceQuery->deInitQueries();
  airspaceQueryOnline->deInitQueries();
  procedureQuery->deInitQueries();

  apronGeometryCache->clear();

  delete databaseMeta;
  databaseMeta = nullptr;

  delete databaseMetaNav;
  databaseMetaNav = nullptr;
}

void NavApp::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  databaseMeta = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  magDecReader->readFromTable(*getDatabaseSim());
  moraReader->readFromTable(*getDatabaseMora());

  airportQuerySim->initQueries();
  airportQueryNav->initQueries();
  mapQuery->initQueries();
  airspaceQuery->initQueries();
  airspaceQueryOnline->initQueries();
  infoQuery->initQueries();
  procedureQuery->initQueries();
  loadingDatabase = false;
}

Ui::MainWindow *NavApp::getMainUi()
{
  return mainWindow->getUi();
}

bool NavApp::isFetchAiAircraft()
{
  return connectClient->isFetchAiAircraft();
}

bool NavApp::isFetchAiShip()
{
  return connectClient->isFetchAiShip();
}

bool NavApp::isConnected()
{
  return NavApp::getConnectClient()->isConnected();
}

bool NavApp::isConnectedAndAircraft()
{
  return (isConnected() && isUserAircraftValid()) || getUserAircraft().isDebug();
}

bool NavApp::isUserAircraftValid()
{
  return mainWindow->getMapWidget()->getUserAircraft().isValid();
}

const atools::fs::sc::SimConnectUserAircraft& NavApp::getUserAircraft()
{
  return mainWindow->getMapWidget()->getUserAircraft();
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

MapQuery *NavApp::getMapQuery()
{
  return mapQuery;
}

AirspaceQuery *NavApp::getAirspaceQuery()
{
  return airspaceQuery;
}

AirspaceQuery *NavApp::getAirspaceQueryOnline()
{
  return airspaceQueryOnline;
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

int NavApp::getRouteSize()
{
  return mainWindow->getRouteController()->getRoute().size();
}

const RouteAltitude& NavApp::getAltitudeLegs()
{
  return mainWindow->getRouteController()->getRoute().getAltitudeLegs();
}

float NavApp::getRouteCruiseSpeedKts()
{
  return aircraftPerfController->getRouteCruiseSpeedKts();
}

atools::fs::FsPaths::SimulatorType NavApp::getCurrentSimulatorDb()
{
  return getDatabaseManager()->getCurrentSimulator();
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

QString NavApp::getCurrentSimulatorShortName()
{
  return atools::fs::FsPaths::typeToShortName(getCurrentSimulatorDb());
}

bool NavApp::hasSidStarInDatabase()
{
  return databaseMetaNav->hasSidStar();
}

bool NavApp::hasDataInDatabase()
{
  return databaseMeta->hasData();
}

atools::sql::SqlDatabase *NavApp::getDatabaseSim()
{
  return getDatabaseManager()->getDatabaseSim();
}

atools::sql::SqlDatabase *NavApp::getDatabaseNav()
{
  return getDatabaseManager()->getDatabaseNav();
}

atools::sql::SqlDatabase *NavApp::getDatabaseMora()
{
  return getDatabaseManager()->getDatabaseMora();
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

UserdataController *NavApp::getUserdataController()
{
  return userdataController;
}

OnlinedataController *NavApp::getOnlinedataController()
{
  return onlinedataController;
}

AircraftPerfController *NavApp::getAircraftPerfController()
{
  return aircraftPerfController;
}

bool NavApp::isCollectingPerformance()
{
  return aircraftPerfController->isCollecting();
}

const atools::fs::perf::AircraftPerf& NavApp::getAircraftPerformance()
{
  return aircraftPerfController->getAircraftPerformance();
}

atools::fs::common::MagDecReader *NavApp::getMagDecReader()
{
  return magDecReader;
}

VehicleIcons *NavApp::getVehicleIcons()
{
  return vehicleIcons;
}

ApronGeometryCache *NavApp::getApronGeometryCache()
{
  return apronGeometryCache;
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
  return mainWindow->getWeatherReporter();
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

void NavApp::setStatusMessage(const QString& message)
{
  mainWindow->setStatusMessage(message);
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

MapWidget *NavApp::getMapWidget()
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

const QString& NavApp::getCurrentRouteFilepath()
{
  return mainWindow->getRouteController()->getCurrentRouteFilepath();
}

const QString& NavApp::getCurrentAircraftPerfFilepath()
{
  return aircraftPerfController->getCurrentFilepath();
}

DatabaseManager *NavApp::getDatabaseManager()
{
  return databaseManager;
}

ConnectClient *NavApp::getConnectClient()
{
  return connectClient;
}

QString NavApp::getDatabaseAiracCycleSim()
{
  return databaseMeta->getAiracCycle();
}

QString NavApp::getDatabaseAiracCycleNav()
{
  return databaseMetaNav->getAiracCycle();
}

bool NavApp::hasDatabaseAirspaces()
{
  return databaseMetaNav->hasAirspaces();
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
  return databaseMeta;
}

const atools::fs::db::DatabaseMeta *NavApp::getDatabaseMetaNav()
{
  return databaseMetaNav;
}

const AircraftTrack& NavApp::getAircraftTrack()
{
  return getMapWidget()->getAircraftTrack();
}

map::MapObjectTypes NavApp::getShownMapFeatures()
{
  return mainWindow->getMapWidget()->getShownMapFeatures();
}

map::MapAirspaceFilter NavApp::getShownMapAirspaces()
{
  return mainWindow->getMapWidget()->getShownAirspaces();
}

void NavApp::deleteSplashScreen()
{
  qDebug() << Q_FUNC_INFO;

  if(splashScreen != nullptr)
    splashScreen->close();
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
                            Qt::AlignRight | Qt::AlignBottom, Qt::white);

  processEvents(QEventLoop::ExcludeUserInputEvents);
}

void NavApp::finishSplashScreen()
{
  qDebug() << Q_FUNC_INFO;

  if(splashScreen != nullptr)
    splashScreen->finish(mainWindow);
}
