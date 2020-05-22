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

#include "query/infoquery.h"
#include "query/procedurequery.h"
#include "connect/connectclient.h"
#include "query/mapquery.h"
#include "query/waypointtrackquery.h"
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
#include "logbook/logdatacontroller.h"
#include "online/onlinedatacontroller.h"
#include "search/searchcontroller.h"
#include "common/vehicleicons.h"
#include "gui/stylehandler.h"
#include "weather/weatherreporter.h"
#include "fs/weather/metar.h"
#include "perf/aircraftperfcontroller.h"
#include "web/webcontroller.h"
#include "exception.h"
#include "gui/errorhandler.h"
#include "airspace/airspacecontroller.h"
#include "mapgui/mapmarkhandler.h"
#include "routestring/routestringwriter.h"
#include "track/trackcontroller.h"

#include "query/waypointquery.h"
#include "ui_mainwindow.h"

#include <marble/MarbleModel.h>

#include <QIcon>
#include <QSplashScreen>

AirportQuery *NavApp::airportQuerySim = nullptr;
AirportQuery *NavApp::airportQueryNav = nullptr;
MapQuery *NavApp::mapQuery = nullptr;
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
  setApplicationVersion("2.5.0.develop"); // VERSION_NUMBER - Little Navmap
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

  userdataController = new UserdataController(databaseManager->getUserdataManager(), mainWindow);
  logdataController = new LogdataController(databaseManager->getLogdataManager(), mainWindow);
  mapMarkHandler = new MapMarkHandler(mainWindow);

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  magDecReader = new atools::fs::common::MagDecReader();
  readMagDecFromDatabase();

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

  trackController = new TrackController(databaseManager->getTrackManager(), mainWindow);

  aircraftPerfController = new AircraftPerfController(mainWindow);

  mapQuery = new MapQuery(databaseManager->getDatabaseSim(), databaseManager->getDatabaseNav(),
                          databaseManager->getDatabaseUser());
  mapQuery->initQueries();

  airspaceController = new AirspaceController(mainWindow,
                                              databaseManager->getDatabaseSimAirspace(),
                                              databaseManager->getDatabaseNavAirspace(),
                                              databaseManager->getDatabaseUserAirspace(),
                                              databaseManager->getDatabaseOnline());

  airportQuerySim = new AirportQuery(databaseManager->getDatabaseSim(), false /* nav */);
  airportQuerySim->initQueries();

  airportQueryNav = new AirportQuery(databaseManager->getDatabaseNav(), true /* nav */);
  airportQueryNav->initQueries();

  infoQuery = new InfoQuery(databaseManager->getDatabaseSim(),
                            databaseManager->getDatabaseNav(),
                            databaseManager->getDatabaseTrack());
  infoQuery->initQueries();

  procedureQuery = new ProcedureQuery(databaseManager->getDatabaseNav());
  procedureQuery->initQueries();

  connectClient = new ConnectClient(mainWindow);

  updateHandler = new UpdateHandler(mainWindow);

  styleHandler = new StyleHandler(mainWindow);

  webController = new WebController(mainWindow);
}

void NavApp::initElevationProvider()
{
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

  qDebug() << Q_FUNC_INFO << "delete mapQuery";
  delete mapQuery;
  mapQuery = nullptr;

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
  procedureQuery->deInitQueries();
  airspaceController->preDatabaseLoad();
  trackController->preDatabaseLoad();
  logdataController->preDatabaseLoad();

  delete databaseMetaSim;
  databaseMetaSim = nullptr;

  delete databaseMetaNav;
  databaseMetaNav = nullptr;
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
      deleteSplashScreen();
      // Show dialog if something went wrong but do not exit
      atools::gui::ErrorHandler(mainWindow).handleException(e, tr("While reading magnetic declination from database:"));
    }
    catch(...)
    {
      deleteSplashScreen();
      atools::gui::ErrorHandler(mainWindow).
      handleUnknownException(tr("While reading magnetic declination from database:"));
    }
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Empty database falling back to WMM";
    magDecReader->readFromWmm();
  }

  qDebug() << Q_FUNC_INFO << "Mag decl ref date" << magDecReader->getReferenceDate()
           << magDecReader->getWmmVersion();
}

void NavApp::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  readMagDecFromDatabase();

  moraReader->readFromTable(*getDatabaseMora());

  airportQuerySim->initQueries();
  airportQueryNav->initQueries();
  mapQuery->initQueries();
  infoQuery->initQueries();
  procedureQuery->initQueries();
  airspaceController->postDatabaseLoad();
  trackController->postDatabaseLoad();
  logdataController->postDatabaseLoad();
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

bool NavApp::isConnectedNetwork()
{
  return NavApp::getConnectClient()->isConnectedNetwork();
}

bool NavApp::isSimConnect()
{
  return NavApp::getConnectClient()->isSimConnect();
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

const atools::geo::Pos& NavApp::getUserAircraftPos()
{
  return mainWindow->getMapWidget()->getUserAircraft().getPosition();
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

AirwayTrackQuery *NavApp::getAirwayTrackQuery()
{
  return trackController->getAirwayTrackQuery();
}

WaypointTrackQuery *NavApp::getWaypointTrackQuery()
{
  return trackController->getWaypointTrackQuery();
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

MapPaintWidget *NavApp::getMapPaintWidget()
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

QString NavApp::getMapCopyright()
{
  return mainWindow->getMapWidget()->getMapCopyright();
}

const QString& NavApp::getCurrentRouteFilepath()
{
  return mainWindow->getRouteController()->getCurrentRouteFilepath();
}

const QString& NavApp::getCurrentAircraftPerfFilepath()
{
  return aircraftPerfController->getCurrentFilepath();
}

WebController *NavApp::getWebController()
{
  return webController;
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
