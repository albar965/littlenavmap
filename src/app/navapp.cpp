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

#include "app/navapp.h"

#include "airspace/airspacecontroller.h"

#include "atools.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/updatehandler.h"
#include "common/vehicleicons.h"
#include "connect/connectclient.h"
#include "db/databasemanager.h"
#include "exception.h"
#include "fs/common/magdecreader.h"
#include "fs/common/morareader.h"
#include "fs/db/databasemeta.h"
#include "fs/perf/aircraftperf.h"
#include "geo/aircrafttrail.h"
#include "gui/dataexchange.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/stylehandler.h"
#include "logbook/logdatacontroller.h"
#include "logging/logginghandler.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/mapdetailhandler.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapthemehandler.h"
#include "mapgui/mapwidget.h"
#include "online/onlinedatacontroller.h"
#include "perf/aircraftperfcontroller.h"
#include "profile/profilewidget.h"
#include "query/procedurequery.h"
#include "query/querymanager.h"
#include "route/routealtitude.h"
#include "route/routecontroller.h"
#include "routestring/routestringwriter.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"
#include "track/trackcontroller.h"
#include "userdata/userdatacontroller.h"
#include "weather/weatherreporter.h"
#include "web/webcontroller.h"
#include "web/webmapcontroller.h"

#include "ui_mainwindow.h"

#include <marble/MarbleModel.h>

#include <QIcon>

ConnectClient *NavApp::connectClient = nullptr;
DatabaseManager *NavApp::databaseManager = nullptr;
MainWindow *NavApp::mainWindow = nullptr;
ElevationProvider *NavApp::elevationProvider = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMetaSim = nullptr;
atools::fs::db::DatabaseMeta *NavApp::databaseMetaNav = nullptr;

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

atools::gui::DataExchange *NavApp::dataExchange = nullptr;

bool NavApp::closeCalled = false;
bool NavApp::loadingDatabase = false;
bool NavApp::mainWindowVisible = false;

using atools::settings::Settings;

NavApp::NavApp(int& argc, char **argv, int flags)
  : atools::gui::Application(argc, argv, flags)
{
  initApplication();
}

NavApp::~NavApp()
{
  ATOOLS_DELETE(dataExchange);
}

void NavApp::initApplication()
{
  setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  setApplicationName(lnm::OPTIONS_APPLICATION);
  setOrganizationName(lnm::OPTIONS_APPLICATION_ORGANIZATION);
  setOrganizationDomain(lnm::OPTIONS_APPLICATION_DOMAIN);
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

  airspaceController = new AirspaceController(mainWindow);

  connectClient = new ConnectClient(mainWindow);
  updateHandler = new UpdateHandler(mainWindow);
  styleHandler = new StyleHandler(mainWindow);
  webController = new WebController(mainWindow);
}

void NavApp::initQueries()
{
  qDebug() << Q_FUNC_INFO;
  onlinedataController->initQueries();
}

void NavApp::showElevationProviderErrors()
{
  qDebug() << Q_FUNC_INFO;
  if(elevationProvider != nullptr)
    elevationProvider->showErrors();
}

void NavApp::initElevationProvider()
{
  qDebug() << Q_FUNC_INFO;
  if(elevationProvider != nullptr)
    elevationProvider->init(mainWindow->getElevationModel());
}

void NavApp::deInitWebController()
{
  ATOOLS_DELETE_LOG(webController);
}

void NavApp::deInit()
{
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
  ATOOLS_DELETE_LOG(databaseManager);
  ATOOLS_DELETE_LOG(databaseMetaSim);
  ATOOLS_DELETE_LOG(databaseMetaNav);
  ATOOLS_DELETE_LOG(magDecReader);
  ATOOLS_DELETE_LOG(moraReader);
  ATOOLS_DELETE_LOG(vehicleIcons);
}

const atools::gui::DataExchange *NavApp::getDataExchangeConst()
{
  return dataExchange;
}

atools::gui::DataExchange *NavApp::getDataExchange()
{
  return dataExchange;
}

bool NavApp::initDataExchange()
{
  if(dataExchange == nullptr)
    dataExchange = new atools::gui::DataExchange(
      Settings::instance().getAndStoreValue(lnm::OPTIONS_DATAEXCHANGE_DEBUG, false).toBool(), lnm::PROGRAM_GUID);

  // Check for commands from other instances in shared memory segment
  // Not connected to main window yet - so messages will be ignored
  // Start timer early to update timestamp and avoid double instances
  dataExchange->startTimer();

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
      // Show dialog if something went wrong but do not exit
      atools::gui::ErrorHandler(mainWindow).handleException(e, tr("While reading magnetic declination from database:"));
    }
    catch(...)
    {
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
  if(webController != nullptr)
    webController->preDatabaseLoad();
  moraReader->preDatabaseLoad();
  airspaceController->preDatabaseLoad();
  trackController->preDatabaseLoad();
  logdataController->preDatabaseLoad();
  QueryManager::instance()->deInitQueries();

  ATOOLS_DELETE_LOG(databaseMetaSim);
  ATOOLS_DELETE_LOG(databaseMetaNav);
}

void NavApp::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  databaseMetaSim = new atools::fs::db::DatabaseMeta(getDatabaseSim());
  databaseMetaNav = new atools::fs::db::DatabaseMeta(getDatabaseNav());

  readMagDecFromDatabase();

  QueryManager::instance()->initQueries();
  moraReader->readFromTable(getDatabaseNav(), getDatabaseSim());
  airspaceController->postDatabaseLoad();
  logdataController->postDatabaseLoad();
  trackController->postDatabaseLoad();
  if(webController != nullptr)
    webController->postDatabaseLoad();
  loadingDatabase = false;
}

Ui::MainWindow *NavApp::getMainUi()
{
  return mainWindow != nullptr ? mainWindow->getUi() : nullptr;
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
  return (isConnected() || getUserAircraft().isDebug()) && isUserAircraftValid() && getUserAircraft().isFlying();
}

bool NavApp::isConnectedAndAircraft()
{
  return (isConnected() || getUserAircraft().isDebug()) && isUserAircraftValid();
}

bool NavApp::isUserAircraftValid()
{
  return mainWindow->getMapWidget()->getUserAircraft().isFullyValid();
}

bool NavApp::hasAircraftPassedTakeoffPoint()
{
  return logdataController->hasAircraftPassedTakeoffPoint();
}

bool NavApp::checkSimConnect()
{
  if(connectClient != nullptr)
    return connectClient->checkSimConnect();
  else
    return false;
}

void NavApp::pauseSimConnect()
{
  if(connectClient != nullptr)
    return connectClient->pauseSimConnect();
}

void NavApp::resumeSimConnect()
{
  if(connectClient != nullptr)
    return connectClient->resumeSimConnect();
}

bool NavApp::isMoraAvailable()
{
  return moraReader->isDataAvailable();
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

bool NavApp::isDatabaseXPlane()
{
  return getDatabaseManager()->isDatabaseXPlane();
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

QString NavApp::getCurrentSimulatorShortDisplayName()
{
  return atools::fs::FsPaths::typeToShortDisplayName(getCurrentSimulatorDb());
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
    qInfo() << Q_FUNC_INFO << *databaseMetaNav;
  else
    qDebug() << Q_FUNC_INFO << "databaseMetaNav == nullptr";

  qDebug() << Q_FUNC_INFO << "databaseMetaSim";
  if(databaseMetaSim != nullptr)
    qInfo() << Q_FUNC_INFO << *databaseMetaSim;
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

void NavApp::showFlightplan()
{
  mainWindow->showFlightplan();
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

void NavApp::getReportFiles(QStringList& crashReportFiles, QString& reportFilename, bool issueReport)
{
  // Settings and files have to be saved before
  Settings& settings = Settings::instance();
  crashReportFiles.append(settings.valueStr(lnm::ROUTE_FILENAME));
  crashReportFiles.append(settings.valueStr(lnm::AIRCRAFT_PERF_FILENAME));
  crashReportFiles.append(Settings::getConfigFilename(".lnmpln"));
  crashReportFiles.append(Settings::getConfigFilename(lnm::AIRCRAFT_TRACK_SUFFIX));
  crashReportFiles.append(Settings::getConfigFilename(lnm::STACKTRACE_SUFFIX, lnm::CRASHREPORTS_DIR));
  crashReportFiles.append(Settings::getFilename());

  // Add all log files last to catch any error which appear while compressing
  crashReportFiles.append(atools::logging::LoggingHandler::getLogFiles(true /* includeBackups */));

  reportFilename = Settings::getConfigFilename(issueReport ? lnm::ISSUEREPORT_SUFFIX : lnm::CRASHREPORT_SUFFIX, lnm::CRASHREPORTS_DIR);

  // Remove not existing files =================================
  for(QString& filename : crashReportFiles)
  {
    if(!filename.isEmpty() && !QFile::exists(filename))
      filename.clear();
  }
  crashReportFiles.removeAll(QString());
}

void NavApp::recordStartNavApp()
{
#ifndef DEBUG_DISABLE_CRASH_REPORT

  QStringList crashReportFiles;
  QString reportFilename;
  getReportFiles(crashReportFiles, reportFilename, false /* issueReport */);

  Application::recordStartAndDetectCrash(nullptr, Settings::getConfigFilename(".running"), reportFilename, crashReportFiles,
                                         lnm::helpOnlineUrl, "CRASHREPORT.html", lnm::helpLanguageOnline());
#endif
  // Keep command line options to avoid using the wrong configuration folder
}

void NavApp::createIssueReport(const QStringList& additionalFiles)
{
  qDebug() << Q_FUNC_INFO;

  QString reportFilename;
  QStringList crashReportFiles;
  getReportFiles(crashReportFiles, reportFilename, true /* issueReport */);
  crashReportFiles.append(additionalFiles);

  Application::createReport(mainWindow, reportFilename, crashReportFiles, lnm::helpOnlineUrl, "ISSUEREPORT.html",
                            lnm::helpLanguageOnline());
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

bool NavApp::hasNatTracks()
{
  return trackController->hasNatTracks();
}

bool NavApp::hasPacotsTracks()
{
  return trackController->hasPacotsTracks();
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

bool NavApp::isGuiStyleDark()
{
  return styleHandler->isGuiStyleDark();
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

void NavApp::getAirportMetarWind(float& windDirectionDeg, float& windSpeedKts, const map::MapAirport& airport, bool stationOnly)
{
  mainWindow->getWeatherReporter()->getAirportMetarWind(windDirectionDeg, windSpeedKts, airport, stationOnly);
}

void NavApp::updateWindowTitle()
{
  mainWindow->updateWindowTitle();
}

void NavApp::updateErrorLabel()
{
  getRouteController()->updateFooterErrorLabel();
}

void NavApp::setStatusMessage(const QString& message, bool addToLog, bool popup)
{
  mainWindow->setStatusMessage(message, addToLog, popup);
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

bool NavApp::isDebugMovingAircraft()
{
  return mainWindow->isDebugMovingAircraft();
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
  return mainWindow != nullptr ? mainWindow->getRouteController() : nullptr;
}

atools::gui::TabWidgetHandler *NavApp::getRouteTabHandler()
{
  return getRouteController()->getTabHandler();
}

const InfoController *NavApp::getInfoController()
{
  return mainWindow != nullptr ? mainWindow->getInfoController() : nullptr;
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

const QString& NavApp::getCurrentRouteFilePath()
{
  return mainWindow->getRouteController()->getRouteFilePath();
}

const QString& NavApp::getCurrentAircraftPerfFilePath()
{
  return aircraftPerfController->getCurrentFilename();
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

bool NavApp::isWebControllerRunning()
{
  return webController != nullptr ? webController->isListenerRunning() : false;
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

const map::MapTypes NavApp::getShownMapTypes()
{
  return mainWindow->getMapWidget()->getShownMapTypes();
}

const map::MapDisplayTypes NavApp::getShownMapDisplayTypes()
{
  return mainWindow->getMapWidget()->getShownMapDisplayTypes();
}

const map::MapAirspaceFilter& NavApp::getShownMapAirspaces()
{
  return mainWindow->getMapWidget()->getShownAirspaces();
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

atools::win::ActivationContext *NavApp::getActivationContext()
{
  return getConnectClient()->getActivationContext();
}
