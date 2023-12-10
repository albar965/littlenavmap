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

#include "gui/mainwindow.h"

#include "airspace/airspacecontroller.h"
#include "app/dataexchange.h"
#include "atools.h"
#include "common/constants.h"
#include "common/dirtool.h"
#include "common/elevationprovider.h"
#include "common/filecheck.h"
#include "common/mapcolors.h"
#include "common/settingsmigrate.h"
#include "common/unit.h"
#include "connect/connectclient.h"
#include "db/databasemanager.h"
#include "exception.h"
#include "fs/gpx/gpxio.h"
#include "fs/perf/aircraftperf.h"
#include "gui/application.h"
#include "gui/dialog.h"
#include "gui/dockwidgethandler.h"
#include "gui/errorhandler.h"
#include "gui/filehistoryhandler.h"
#include "gui/helphandler.h"
#include "gui/messagebox.h"
#include "gui/messagesettings.h"
#include "gui/statusbareventfilter.h"
#include "gui/stylehandler.h"
#include "gui/tabwidgethandler.h"
#include "gui/timedialog.h"
#include "gui/tools.h"
#include "gui/translator.h"
#include "gui/widgetstate.h"
#include "info/infocontroller.h"
#include "logbook/logdatacontroller.h"
#include "logging/loggingguiabort.h"
#include "logging/logginghandler.h"
#include "mapgui/imageexportdialog.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/mapdetailhandler.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapthemehandler.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "options/optionsdialog.h"
#include "perf/aircraftperfcontroller.h"
#include "print/printsupport.h"
#include "profile/profilewidget.h"
#include "query/airportquery.h"
#include "query/procedurequery.h"
#include "route/routecontroller.h"
#include "routeexport/routeexport.h"
#include "routeexport/simbriefhandler.h"
#include "routestring/routestringdialog.h"
#include "routestring/routestringwriter.h"
#include "search/airportsearch.h"
#include "search/logdatasearch.h"
#include "search/navsearch.h"
#include "search/onlinecentersearch.h"
#include "search/onlineclientsearch.h"
#include "search/onlineserversearch.h"
#include "search/proceduresearch.h"
#include "search/searchcontroller.h"
#include "search/userdatasearch.h"
#include "settings/settings.h"
#include "track/trackcontroller.h"
#include "userdata/userdatacontroller.h"
#include "util/htmlbuilder.h"
#include "util/version.h"
#include "weather/weathercontexthandler.h"
#include "weather/weatherreporter.h"
#include "weather/windreporter.h"
#include "web/webcontroller.h"
#include "common/updatehandler.h"

#include <marble/MarbleAboutDialog.h>
#include <marble/MarbleModel.h>
#include <marble/HttpDownloadManager.h>
#include <marble/MarbleDirs.h>

#include <QCloseEvent>
#include <QScreen>
#include <QWindow>
#include <QSslSocket>
#include <QEvent>
#include <QMimeData>
#include <QClipboard>
#include <QProgressDialog>
#include <QThread>
#include <QStringBuilder>

#include "ui_mainwindow.h"

const static int MAX_STATUS_MESSAGES = 10;
const static int CLOCK_TIMER_MS = 1000;
const static int RENDER_STATUS_TIMER_MS = 5000;
const static int SHRINK_STATUS_BAR_TIMER_MS = 10000;

using namespace Marble;
using atools::settings::Settings;
using atools::gui::FileHistoryHandler;
using atools::gui::MapPosHistory;
using atools::gui::HelpHandler;
using atools::gui::DockWidgetHandler;

MainWindow::MainWindow()
  : QMainWindow(nullptr), ui(new Ui::MainWindow)
{
  qDebug() << Q_FUNC_INFO << "constructor";

  aboutMessage =
    tr("<p style='white-space:pre'>is a free open source flight planner, navigation tool, moving map,<br/>"
       "airport search and airport information system<br/>"
       "for X-Plane 11, X-Plane 12, Flight Simulator X, Prepar3D and Microsoft Flight Simulator 2020.</p>"
       "<p>"
         "<b>"
           "If you would like to show your appreciation you can donate&nbsp;"
           "<a href=\"%1\">here"
           "</a>."
         "</b>"
       "</p>"
       "<p>This software is licensed under "
         "<a href=\"http://www.gnu.org/licenses/gpl-3.0\">GPL3"
         "</a> or any later version."
       "</p>"
       "<p>The source code for this application is available at "
         "<a href=\"https://github.com/albar965\">GitHub"
         "</a>."
       "</p>"
       "<p>More about my projects at "
         "<a href=\"https://www.littlenavmap.org\">www.littlenavmap.org"
         "</a>."
       "</p>"
       "<p>"
         "<b>Copyright 2015-2023 Alexander Barthel"
         "</b>"
       "</p>").arg(lnm::helpDonateUrl);

  layoutWarnText = tr("The option \"Allow to undock map window\" in the layout file is "
                      "different than the currently set option.\n"
                      "The layout might not be restored properly.\n\n"
                      "Apply the loaded window layout anyway?");
  // Show a dialog on fatal log events like asserts
  atools::logging::LoggingGuiAbortHandler::setGuiAbortFunction(this);

  try
  {
    // Have to handle exceptions here since no message handler is active yet and no atools::Application method can catch it

    ui->setupUi(this);

#if defined(Q_OS_MACOS)
    // Reset menu roles to no role if not manually set to avoid side effects from goofy Qt behavior on macOS
    QList<QAction *> stack;
    stack.append(ui->menuBar->actions());
    while(!stack.isEmpty())
    {
      QAction *action = stack.takeFirst();

      // Reset role to do nothing
      if(action->menuRole() == QAction::TextHeuristicRole)
        action->setMenuRole(QAction::NoRole);

      QMenu *menu = action->menu();
      if(menu != nullptr)
      {
        // Add actions from (sub)menu
        for(QAction *sub : menu->actions())
        {
          if(sub != nullptr)
            stack.append(sub);
        }
      }
    }
#endif

    // Try to avoid short popping up on startup
    ui->labelProfileInfo->hide();

    setAcceptDrops(true);
    defaultToolbarIconSize = iconSize();

    // Show tooltips also for inactive windows (e.g. if a floating window is active)
    setAttribute(Qt::WA_AlwaysShowToolTips);

    // Create misc GUI handlers ============================================
    dialog = new atools::gui::Dialog(this);
    errorHandler = new atools::gui::ErrorHandler(this);
    helpHandler = new atools::gui::HelpHandler(this, aboutMessage, GIT_REVISION_LITTLENAVMAP);

    // Create dock and mainwindow handler ============================================
    toolbars.append({ui->toolBarMain, ui->toolBarMap, ui->toolBarMapOptions, ui->toolBarRoute, ui->toolBarView, ui->toolBarAirspaces,
                     ui->toolBarTools});

    atools::settings::Settings& settings = atools::settings::Settings::instance();
    dockHandler =
      new atools::gui::DockWidgetHandler(this,
                                         // Add all available dock widgets here ==========================
                                         {ui->dockWidgetAircraft, ui->dockWidgetSearch, ui->dockWidgetProfile,
                                          ui->dockWidgetInformation, ui->dockWidgetRoute},
                                         // Add all available toolbars  here =============================
                                         toolbars,
                                         settings.getAndStoreValue(lnm::OPTIONS_DOCKHANDLER_DEBUG, false).toBool());

    marbleAboutDialog = new Marble::MarbleAboutDialog(this);
    marbleAboutDialog->setApplicationTitle(QApplication::applicationName());

    routeExport = new RouteExport(this);
    simbriefHandler = new SimBriefHandler(this);

    qDebug() << Q_FUNC_INFO << "Creating OptionsDialog";
    optionsDialog = new OptionsDialog(this);
    // Has to load the state now so options are available for all controller and manager classes
    optionsDialog->restoreState();
    optionsChanged();

    // Dialog is opened with asynchronous open()
    connect(optionsDialog, &QDialog::finished, this, [this](int result) {
      if(result == QDialog::Accepted)
        setStatusMessage(tr("Options changed."));
    });

    // Setup central widget ==================================================
    // Set one pixel fixed width
    // QWidget *centralWidget = new QWidget(this);
    // centralWidget->setWindowFlags(windowFlags() & ~(Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus));
    // centralWidget->setMinimumSize(1, 1);
    // centralWidget->resize(1, 1);
    // centralWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    // centralWidget->hide(); // Potentially messes up docking windows (i.e. Profile dock cannot be shrinked) in certain configurations.
    // setCentralWidget(centralWidget);
    if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
      centralWidget()->hide();

    setupUi();

    // Load all map feature colors
    mapcolors::loadColors();

    Unit::init();

    // Remember original title
    mainWindowTitle = windowTitle();

    // Prepare database and queries
    qDebug() << Q_FUNC_INFO << "Creating DatabaseManager";

    NavApp::init(this);

    // Initialize and connect the data exchange which sends properties from other started instances
    const DataExchange *dataExchange = NavApp::getDataExchangeConst();
    connect(dataExchange, &DataExchange::activateMain, this, &MainWindow::activateWindow);
    connect(dataExchange, &DataExchange::activateMain, this, &MainWindow::raise);
    connect(dataExchange, &DataExchange::loadRoute, this, &MainWindow::routeOpenFile);
    connect(dataExchange, &DataExchange::loadRouteDescr, this, &MainWindow::routeOpenDescr);
    connect(dataExchange, &DataExchange::loadLayout, this, &MainWindow::loadLayoutDelayed);
    connect(dataExchange, &DataExchange::loadPerf, NavApp::getAircraftPerfController(), &AircraftPerfController::loadFile);

    NavApp::getStyleHandler()->insertMenuItems(ui->menuWindowStyle);
    NavApp::getStyleHandler()->restoreState();
    mapcolors::init();
    updateStatusBarStyle();

    // Add actions for flight simulator database switch in main menu
    NavApp::getDatabaseManager()->insertSimSwitchActions();

    qDebug() << Q_FUNC_INFO << "Creating WeatherReporter";
    weatherReporter = new WeatherReporter(this, NavApp::getCurrentSimulatorDb());
    weatherContextHandler = new WeatherContextHandler(weatherReporter, NavApp::getConnectClient());

    qDebug() << Q_FUNC_INFO << "Creating WindReporter";
    windReporter = new WindReporter(this, NavApp::getCurrentSimulatorDb());
    windReporter->addToolbarButton();

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for flight plans";
    routeFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMES_RECENT, ui->menuRecentRoutes, ui->actionRecentRoutesClear);

    qDebug() << Q_FUNC_INFO << "Creating RouteController";
    routeController = new RouteController(this, ui->tableViewRoute);

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for KML files";
    kmlFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMESKML_RECENT, ui->menuRecentKml, ui->actionClearKmlMenu);

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for layout files";
    layoutFileHistory = new FileHistoryHandler(this, lnm::LAYOUT_RECENT, ui->menuWindowLayoutRecent, ui->actionWindowLayoutClearRecent);
    layoutFileHistory->setFirstItemShortcut("Ctrl+Shift+W");

    mapThemeHandler = new MapThemeHandler(this);
    mapThemeHandler->loadThemes();

    // Create map widget and replace dummy widget in window
    qDebug() << Q_FUNC_INFO << "Creating MapWidget";
    mapWidget = new MapWidget(this);
    if(OptionData::instance().getFlags2() & opts2::MAP_ALLOW_UNDOCK)
    {
      ui->verticalLayoutMap->replaceWidget(ui->widgetDummyMap, mapWidget);
      ui->dockWidgetMap->show();
    }
    else
    {
      setCentralWidget(mapWidget);
      ui->dockWidgetMap->hide();
    }

    // Fill theme handler and menus after setting up map widget
    mapThemeHandler->setupMapThemesUi();

    // Init a few late objects since these depend on the map widget instance
    NavApp::initQueries();

    // Create elevation profile widget and replace dummy widget in window
    qDebug() << Q_FUNC_INFO << "Creating ProfileWidget";
    profileWidget = new ProfileWidget(ui->scrollAreaProfile->viewport());
    ui->scrollAreaProfile->setWidget(profileWidget);
    profileWidget->show();

    // Have to create searches in the same order as the tabs
    qDebug() << Q_FUNC_INFO << "Creating SearchController";
    searchController = new SearchController(this, ui->tabWidgetSearch);
    searchController->createAirportSearch(ui->tableViewAirportSearch);
    searchController->createNavSearch(ui->tableViewNavSearch);

    searchController->createProcedureSearch(ui->treeWidgetApproachSearch);

    searchController->createUserdataSearch(ui->tableViewUserdata);
    searchController->createLogdataSearch(ui->tableViewLogdata);

    searchController->createOnlineClientSearch(ui->tableViewOnlineClientSearch);
    searchController->createOnlineCenterSearch(ui->tableViewOnlineCenterSearch);
    searchController->createOnlineServerSearch(ui->tableViewOnlineServerSearch);

    qDebug() << Q_FUNC_INFO << "Creating InfoController";
    infoController = new InfoController(this);

    qDebug() << Q_FUNC_INFO << "Creating PrintSupport";
    printSupport = new PrintSupport(this);

    setStatusMessage(tr("Started."));

    qDebug() << Q_FUNC_INFO << "Connecting slots";
    connectAllSlots();
    NavApp::getAircraftPerfController()->connectAllSlots();

    // Add toolbar button for map marks like holds, patterns and others
    // Order here defines order of buttons on toolbar
    NavApp::getMapMarkHandler()->addToolbarButton();
    NavApp::getMapAirportHandler()->addToolbarButton();
    NavApp::getMapDetailHandler()->addToolbarButton();

    // Add user defined points toolbar button and submenu items
    NavApp::getUserdataController()->addToolbarButton();

    qDebug() << Q_FUNC_INFO << "Reading settings";
    restoreStateMain();

    // Update window states based on actions
    allowDockingWindows();
    allowMovingWindows();
    hideTitleBar();

    updateActionStates();
    updateMarkActionStates();
    updateHighlightActionStates();

    NavApp::getAirspaceController()->updateButtonsAndActions();
    updateOnlineActionStates();

    qDebug() << Q_FUNC_INFO << "Setting theme";
    updateMapKeys(); // First update keys in GUI map widget - web API not started yet
    mapThemeHandler->changeMapTheme();
    mapThemeHandler->changeMapProjection();

    // Wait until everything is set up and update map
    updateMapObjectsShown();

    profileWidget->updateProfileShowFeatures();

    updateWindowTitle();

    // Enable or disable tooltips - call later since it needs the map window
    optionsDialog->updateTooltipOption();

    // Update clock =====================
    clockTimer.setInterval(CLOCK_TIMER_MS);
    connect(&clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer.start();

    // Reset render status - change to done after ten seconds =====================
    renderStatusTimer.setInterval(RENDER_STATUS_TIMER_MS);
    renderStatusTimer.setSingleShot(true);
    connect(&renderStatusTimer, &QTimer::timeout, this, &MainWindow::renderStatusReset);

    shrinkStatusBarTimer.setInterval(SHRINK_STATUS_BAR_TIMER_MS);
    shrinkStatusBarTimer.setSingleShot(true);
    connect(&shrinkStatusBarTimer, &QTimer::timeout, this, &MainWindow::shrinkStatusBar);

    // Print the size of all container classes to detect overflow or memory leak conditions
    // Do this every 30 seconds if enabled with "[Options] StorageDebug=true" in ini file
    if(Settings::instance().getAndStoreValue(lnm::OPTIONS_STORAGE_DEBUG, false).toBool())
    {
      debugDumpContainerSizesTimer.setInterval(30000);
      connect(&debugDumpContainerSizesTimer, &QTimer::timeout, this, &MainWindow::debugDumpContainerSizes);
      debugDumpContainerSizesTimer.start();
    }

    qDebug() << Q_FUNC_INFO << "Constructor done";
  }
  // Exit application if something goes wrong
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }

#ifdef DEBUG_INFORMATION

  QAction *debugAction1 = new QAction("DEBUG - Dump Route", ui->menuHelp);
  debugAction1->setShortcut(QKeySequence("Ctrl+F1"));
  debugAction1->setShortcutContext(Qt::ApplicationShortcut);
  this->addAction(debugAction1);

  QAction *debugAction2 = new QAction("DEBUG - Dump Flightplan", ui->menuHelp);
  this->addAction(debugAction2);

  QAction *debugAction3 = new QAction("DEBUG - Force Check updates", ui->menuHelp);
  this->addAction(debugAction3);

  QAction *debugAction4 = new QAction("DEBUG - Reload flight plan", ui->menuHelp);
  this->addAction(debugAction4);

  QAction *debugAction5 = new QAction("DEBUG - Open flight plan in editor", ui->menuHelp);
  this->addAction(debugAction5);

  QAction *debugAction6 = new QAction("DEBUG - Open perf in editor", ui->menuHelp);
  this->addAction(debugAction6);

  QAction *debugAction7 = new QAction("DEBUG - Dump map layers", ui->menuHelp);
  this->addAction(debugAction7);

  QAction *debugAction8 = new QAction("DEBUG - Reset update timestamp to -2 days", ui->menuHelp);
  this->addAction(debugAction8);

  ui->menuHelp->addSeparator();
  ui->menuHelp->addSeparator();
  ui->menuHelp->addAction(debugAction1);
  ui->menuHelp->addAction(debugAction2);
  ui->menuHelp->addAction(debugAction3);
  ui->menuHelp->addAction(debugAction4);
  ui->menuHelp->addAction(debugAction5);
  ui->menuHelp->addAction(debugAction6);
  ui->menuHelp->addAction(debugAction7);
  ui->menuHelp->addAction(debugAction8);

  connect(debugAction1, &QAction::triggered, this, &MainWindow::debugActionTriggered1);
  connect(debugAction2, &QAction::triggered, this, &MainWindow::debugActionTriggered2);
  connect(debugAction3, &QAction::triggered, this, &MainWindow::debugActionTriggered3);
  connect(debugAction4, &QAction::triggered, this, &MainWindow::debugActionTriggered4);
  connect(debugAction5, &QAction::triggered, this, &MainWindow::debugActionTriggered5);
  connect(debugAction6, &QAction::triggered, this, &MainWindow::debugActionTriggered6);
  connect(debugAction7, &QAction::triggered, this, &MainWindow::debugActionTriggered7);
  connect(debugAction8, &QAction::triggered, this, &MainWindow::debugActionTriggered8);

#endif

}

MainWindow::~MainWindow()
{
  qDebug() << Q_FUNC_INFO;

  NavApp::setShuttingDown();

  clockTimer.stop();

  weatherUpdateTimer.stop();

  // Close all queries
  preDatabaseLoad();

  // Set all pointers to null to catch errors for late access
  NavApp::removeDialogFromDockHandler(routeStringDialog);
  ATOOLS_DELETE_LOG(routeStringDialog);
  ATOOLS_DELETE_LOG(routeController);
  ATOOLS_DELETE_LOG(searchController);
  ATOOLS_DELETE_LOG(weatherReporter);
  ATOOLS_DELETE_LOG(windReporter);
  ATOOLS_DELETE_LOG(profileWidget);
  ATOOLS_DELETE_LOG(marbleAboutDialog);
  ATOOLS_DELETE_LOG(infoController);
  ATOOLS_DELETE_LOG(printSupport);
  ATOOLS_DELETE_LOG(routeFileHistory);
  ATOOLS_DELETE_LOG(kmlFileHistory);
  ATOOLS_DELETE_LOG(layoutFileHistory);
  ATOOLS_DELETE_LOG(optionsDialog);
  ATOOLS_DELETE_LOG(mapWidget);
  ATOOLS_DELETE_LOG(dialog);
  ATOOLS_DELETE_LOG(errorHandler);
  ATOOLS_DELETE_LOG(helpHandler);
  ATOOLS_DELETE_LOG(actionGroupMapProjection);
  ATOOLS_DELETE_LOG(actionGroupMapSunShading);
  ATOOLS_DELETE_LOG(actionGroupMapWeatherSource);
  ATOOLS_DELETE_LOG(actionGroupMapWeatherWindSource);
  ATOOLS_DELETE_LOG(routeExport);
  ATOOLS_DELETE_LOG(weatherContextHandler);
  ATOOLS_DELETE_LOG(simbriefHandler);
  ATOOLS_DELETE_LOG(mapThemeHandler);

  qDebug() << Q_FUNC_INFO << "NavApplication::deInit()";
  NavApp::deInit();

  qDebug() << Q_FUNC_INFO << "Unit::deInit()";
  Unit::deInit();

  ATOOLS_DELETE_LOG(ui);

  ATOOLS_DELETE_LOG(dockHandler);

  // Delete settings singleton
  qDebug() << Q_FUNC_INFO << "Settings::shutdown()";

  if(NavApp::isRestartProcess())
    Settings::clearAndShutdown();
  else
    Settings::shutdown();

  // Free translations
  atools::gui::Translator::unload();

  atools::logging::LoggingGuiAbortHandler::resetGuiAbortFunction();
}

#ifdef DEBUG_INFORMATION

void MainWindow::debugActionTriggered1()
{
  qDebug() << "======================================================================================";
  qDebug() << Q_FUNC_INFO;
  qDebug() << NavApp::getRouteConst();
  qDebug() << "======================================================================================";
}

void MainWindow::debugActionTriggered2()
{
  qDebug() << "======================================================================================";
  qDebug() << Q_FUNC_INFO;
  qDebug() << NavApp::getRouteConst().getFlightplanConst();
  qDebug() << "======================================================================================";
}

void MainWindow::debugActionTriggered3()
{
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(), false /* manual */, false /* startup */, true /* forceDebug */);
}

void MainWindow::debugActionTriggered4()
{
  QString file = routeController->getRouteFilepath();
  routeController->loadFlightplan(file);
}

void MainWindow::debugActionTriggered5()
{
  helpHandler->openFile(routeController->getRouteFilepath());
}

void MainWindow::debugActionTriggered6()
{
  helpHandler->openFile(NavApp::getAircraftPerfController()->getCurrentFilepath());
}

void MainWindow::debugActionTriggered7()
{
  mapWidget->dumpMapLayers();
}

void MainWindow::debugActionTriggered8()
{
  Settings::instance().setValueVar(lnm::OPTIONS_UPDATE_LAST_CHECKED, QDateTime::currentDateTime().toSecsSinceEpoch() - 3600L * 48L);
}

#endif

void MainWindow::updateMap() const
{
  mapWidget->update();
}

void MainWindow::updateClock() const
{
  timeLabel->setText(QDateTime::currentDateTimeUtc().toString("d   HH:mm:ss UTC "));
  timeLabel->setToolTip(tr("Day of month and UTC time.\n%1\nLocal: %2")
                        .arg(QDateTime::currentDateTimeUtc().toString())
                        .arg(QDateTime::currentDateTime().toString()));
  timeLabel->setMinimumWidth(timeLabel->width());
}

/* Show map legend and bring information dock to front */
void MainWindow::showNavmapLegend()
{
  // Show it in browser
  HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineLegendUrl, lnm::helpLanguageOnline());
  setStatusMessage(tr("Opened map legend in browser."));
}

/* Check manually for updates as triggered by the action */
void MainWindow::checkForUpdates()
{
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(), true /* manual */, false /* startup */, false /* forceDebug */);
}

void MainWindow::showOnlineDownloads()
{
  HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineDownloadsUrl, lnm::helpLanguageOnline());
}

void MainWindow::showChangelog()
{
  HelpHandler::openFile(this, QApplication::applicationDirPath() % atools::SEP % "CHANGELOG.txt");
}

void MainWindow::showDonationPage()
{
  HelpHandler::openUrlWeb(this, lnm::helpDonateUrl);
}

void MainWindow::showFaqPage()
{
  HelpHandler::openUrlWeb(this, lnm::helpFaqUrl);
}

void MainWindow::showOfflineHelp()
{
  HelpHandler::openFile(this, HelpHandler::getHelpFile(lnm::helpOfflineFile, OptionData::instance().getLanguage()));
}

void MainWindow::openLogFile()
{
  for(const QString& log : atools::logging::LoggingHandler::getLogFiles())
    HelpHandler::openFile(this, log);
}

void MainWindow::openConfigFile()
{
  HelpHandler::openFile(this, Settings::getFilename());
}

/* User clicked "show in browser" in legend */
void MainWindow::legendAnchorClicked(const QUrl& url)
{
  if(url.scheme() == "lnm" && url.host() == "legend")
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl % "LEGEND.html", lnm::helpLanguageOnline());
  else
    HelpHandler::openUrl(this, url);

  setStatusMessage(tr("Opened legend link in browser."));
}

void MainWindow::setupUi()
{
  // Reduce large icons on mac once intially
#if defined(Q_OS_MACOS)

  for(QToolBar *toolbar : toolbars)
  {
    QSizeF size = toolbar->iconSize();
    size *= 0.72f;
    toolbar->setIconSize(size.toSize());
  }
#endif

  // Projection menu items
  actionGroupMapProjection = new QActionGroup(ui->menuViewProjection);
  actionGroupMapProjection->setObjectName("actionGroupMapProjection");
  ui->actionMapProjectionMercator->setActionGroup(actionGroupMapProjection);
  ui->actionMapProjectionSpherical->setActionGroup(actionGroupMapProjection);

  // Sun shading sub menu
  actionGroupMapSunShading = new QActionGroup(ui->menuSunShading);
  actionGroupMapSunShading->setObjectName("actionGroupMapSunShading");
  actionGroupMapSunShading->addAction(ui->actionMapShowSunShadingSimulatorTime);
  actionGroupMapSunShading->addAction(ui->actionMapShowSunShadingRealTime);
  actionGroupMapSunShading->addAction(ui->actionMapShowSunShadingUserTime);

  // Weather source sub menu
  actionGroupMapWeatherSource = new QActionGroup(ui->menuMapShowAirportWeatherSource);
  actionGroupMapWeatherSource->setObjectName("actionGroupMapWeatherSource");
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherDisabled);
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherSimulator);
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherActiveSky);
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherNoaa);
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherVatsim);
  actionGroupMapWeatherSource->addAction(ui->actionMapShowWeatherIvao);

  // Weather source sub menu
  actionGroupMapWeatherWindSource = new QActionGroup(ui->menuHighAltitudeWindSource);
  actionGroupMapWeatherWindSource->setObjectName("actionGroupMapWeatherWindSource");
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindDisabled);
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindManual);
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindSimulator);
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindNOAA);

  // Tool button in flight plan report

  ui->toolButtonAircraftPerformanceWind->setMenu(new QMenu(ui->toolButtonAircraftPerformanceWind));
  QMenu *buttonMenu = ui->toolButtonAircraftPerformanceWind->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->addActions({ui->actionMapShowWindDisabled, ui->actionMapShowWindManual,
                          ui->actionMapShowWindSimulator, ui->actionMapShowWindNOAA});

  // Update dock widget actions with tooltip, status tip and keypress
  ui->dockWidgetSearch->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/searchdock.svg"));
  ui->dockWidgetSearch->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+1")));
  ui->dockWidgetSearch->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                       arg(ui->dockWidgetSearch->windowTitle()));
  ui->dockWidgetSearch->toggleViewAction()->setStatusTip(ui->dockWidgetSearch->toggleViewAction()->toolTip());

  ui->dockWidgetRoute->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/routedock.svg"));
  ui->dockWidgetRoute->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+2")));
  ui->dockWidgetRoute->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                      arg(ui->dockWidgetRoute->windowTitle()));
  ui->dockWidgetRoute->toggleViewAction()->setStatusTip(ui->dockWidgetRoute->toggleViewAction()->toolTip());

  ui->dockWidgetInformation->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/infodock.svg"));
  ui->dockWidgetInformation->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+4")));
  ui->dockWidgetInformation->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                            arg(ui->dockWidgetInformation->windowTitle().
                                                                toLower()));
  ui->dockWidgetInformation->toggleViewAction()->setStatusTip(ui->dockWidgetInformation->toggleViewAction()->toolTip());

  ui->dockWidgetProfile->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/profiledock.svg"));
  ui->dockWidgetProfile->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+5")));
  ui->dockWidgetProfile->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                        arg(ui->dockWidgetProfile->windowTitle()));
  ui->dockWidgetProfile->toggleViewAction()->setStatusTip(ui->dockWidgetProfile->toggleViewAction()->toolTip());

  ui->dockWidgetAircraft->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/aircraftdock.svg"));
  ui->dockWidgetAircraft->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+6")));
  ui->dockWidgetAircraft->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                         arg(ui->dockWidgetAircraft->windowTitle()));
  ui->dockWidgetAircraft->toggleViewAction()->setStatusTip(ui->dockWidgetAircraft->toggleViewAction()->toolTip());

  // Connect to methods internally
  dockHandler->connectDockWindows();

  // Add dock actions to main menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->dockWidgetSearch->toggleViewAction(),
                               ui->dockWidgetRoute->toggleViewAction(),
                               ui->dockWidgetInformation->toggleViewAction(),
                               ui->dockWidgetProfile->toggleViewAction(),
                               ui->dockWidgetAircraft->toggleViewAction()});

  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->toolBarMain->toggleViewAction(),
                               ui->toolBarMap->toggleViewAction(),
                               ui->toolBarMapOptions->toggleViewAction(),
                               ui->toolBarRoute->toggleViewAction(),
                               ui->toolBarAirspaces->toggleViewAction(),
                               ui->toolBarView->toggleViewAction(),
                               ui->toolBarTools->toggleViewAction()});
  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to toolbar
  ui->toolBarView->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetInformation->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetProfile->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetAircraft->toggleViewAction());

  // ==============================================================
  // Create labels for the statusbar
  connectStatusLabel = new QLabel();
  connectStatusLabel->setText(tr("Not connected."));
  connectStatusLabel->setToolTip(tr("Simulator connection status."));
  ui->statusBar->addPermanentWidget(connectStatusLabel);
  connectStatusLabel->setMinimumWidth(connectStatusLabel->width());

  mapVisibleLabel = new QLabel();
  ui->statusBar->addPermanentWidget(mapVisibleLabel);

  mapDetailLabel = new QLabel();
  mapDetailLabel->setToolTip(tr("Map detail level."));
  ui->statusBar->addPermanentWidget(mapDetailLabel);

  mapRenderStatusLabel = new QLabel();
  mapRenderStatusLabel->setToolTip(tr("Map rendering and download status."));
  ui->statusBar->addPermanentWidget(mapRenderStatusLabel);

  mapDistanceLabel = new QLabel();
  mapDistanceLabel->setToolTip(tr("Map view distance to ground."));
  ui->statusBar->addPermanentWidget(mapDistanceLabel);

  mapPositionLabel = new QLabel();
  mapPositionLabel->setToolTip(tr("Coordinates and elevation at cursor position."));
  ui->statusBar->addPermanentWidget(mapPositionLabel);

  mapMagvarLabel = new QLabel();
  mapMagvarLabel->setToolTip(tr("Magnetic declination at cursor position."));
  ui->statusBar->addPermanentWidget(mapMagvarLabel);

  timeLabel = new QLabel();
  timeLabel->setToolTip(tr("Day of month and UTC time."));
  ui->statusBar->addPermanentWidget(timeLabel);

  // Status bar takes ownership of filter which handles tooltip on click
  ui->statusBar->installEventFilter(new StatusBarEventFilter(ui->statusBar, connectStatusLabel));

  connect(ui->statusBar, &QStatusBar::messageChanged, this, &MainWindow::statusMessageChanged);
}

void MainWindow::updateStatusBarStyle()
{
  Qt::AlignmentFlag align = Qt::AlignCenter;
  QFrame::Shadow shadow = connectStatusLabel->frameShadow();
  QFrame::Shape shape = connectStatusLabel->frameShape();
  bool adjustFrame = false;

  // Adjust shadow and shape of status bar labels but not for macOS
#ifndef Q_OS_MACOS
  if(NavApp::getStyleHandler() != nullptr)
  {
    QString style = NavApp::getStyleHandler()->getCurrentGuiStyleDisplayName();
    if(style.compare(StyleHandler::STYLE_FUSION, Qt::CaseInsensitive) == 0)
    {
      shadow = QFrame::Sunken;
      shape = QFrame::StyledPanel;
      adjustFrame = true;
    }
    else if(style.compare(StyleHandler::STYLE_NIGHT, Qt::CaseInsensitive) == 0)
    {
      shadow = QFrame::Sunken;
      shape = QFrame::Box;
      adjustFrame = true;
    }
    // Windows styles already use a box
  }

  if(adjustFrame)
  {
    connectStatusLabel->setFrameShadow(shadow);
    connectStatusLabel->setFrameShape(shape);
    connectStatusLabel->setMargin(1);

    mapVisibleLabel->setFrameShadow(shadow);
    mapVisibleLabel->setFrameShape(shape);
    mapVisibleLabel->setMargin(1);

    mapDetailLabel->setFrameShadow(shadow);
    mapDetailLabel->setFrameShape(shape);
    mapDetailLabel->setMargin(1);

    mapRenderStatusLabel->setFrameShadow(shadow);
    mapRenderStatusLabel->setFrameShape(shape);
    mapRenderStatusLabel->setMargin(1);

    mapDistanceLabel->setFrameShadow(shadow);
    mapDistanceLabel->setFrameShape(shape);
    mapDistanceLabel->setMargin(1);

    mapPositionLabel->setFrameShadow(shadow);
    mapPositionLabel->setFrameShape(shape);
    mapPositionLabel->setMargin(1);

    mapMagvarLabel->setFrameShadow(shadow);
    mapMagvarLabel->setFrameShape(shape);
    mapMagvarLabel->setMargin(1);

    timeLabel->setFrameShadow(shadow);
    timeLabel->setFrameShape(shape);
    timeLabel->setMargin(1);
  }
#endif

  // Set a minimum width - the labels grow (but do not shrink) with content changes
  connectStatusLabel->setAlignment(align);
  connectStatusLabel->setMinimumWidth(20);

  mapVisibleLabel->setAlignment(align);
  mapVisibleLabel->setMinimumWidth(20);

  mapDetailLabel->setAlignment(align);
  mapDetailLabel->setMinimumWidth(20);

  mapRenderStatusLabel->setAlignment(align);
  mapRenderStatusLabel->setMinimumWidth(20);

  mapDistanceLabel->setAlignment(align);
  mapDistanceLabel->setMinimumWidth(20);

  mapPositionLabel->setAlignment(align);
  mapPositionLabel->setMinimumWidth(20);
  mapPositionLabel->setText(tr(" — "));

  mapMagvarLabel->setAlignment(align);
  mapMagvarLabel->setMinimumWidth(20);
  mapMagvarLabel->setText(tr(" — "));

  timeLabel->setAlignment(align);
  timeLabel->setMinimumWidth(20);
}

void MainWindow::clearProcedureCache()
{
  NavApp::getProcedureQuery()->clearCache();
}

void MainWindow::connectAllSlots()
{
  // Options dialog ===================================================================
  // Notify others of options change
  // The units need to be called before all others
  connect(optionsDialog, &OptionsDialog::optionsChanged, &Unit::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, &NavApp::optionsChanged);

  // Need to clean cache to regenerate some text if units have changed
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::clearProcedureCache);

  // Reset weather context first
  connect(optionsDialog, &OptionsDialog::optionsChanged, weatherContextHandler, &WeatherContextHandler::clearWeatherContext);

  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getAirspaceController(), &AirspaceController::optionsChanged);

  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateMapObjectsShown);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateActionStates);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::distanceChanged);

  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getMapAirportHandler(), &MapAirportHandler::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, weatherReporter, &WeatherReporter::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, windReporter, &WindReporter::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, searchController, &SearchController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, routeController, &RouteController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, infoController, &InfoController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, mapWidget, &MapPaintWidget::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, profileWidget, &ProfileWidget::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getOnlinedataController(), &OnlinedataController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getLogdataController(), &LogdataController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getElevationProvider(), &ElevationProvider::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getAircraftPerfController(), &AircraftPerfController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getTrackController(), &TrackController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::saveStateNow);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::optionsChanged);

  // Options dialog font ===================================================================
  QGuiApplication *app = dynamic_cast<QGuiApplication *>(QCoreApplication::instance());
  if(app != nullptr)
  {
    connect(app, &QGuiApplication::fontChanged, this, &MainWindow::fontChanged);
    connect(app, &QGuiApplication::fontChanged, NavApp::getLogdataController(), &LogdataController::fontChanged);
    connect(app, &QGuiApplication::fontChanged, routeController, &RouteController::fontChanged);
    connect(app, &QGuiApplication::fontChanged, infoController, &InfoController::fontChanged);
    connect(app, &QGuiApplication::fontChanged, routeStringDialog, &RouteStringDialog::fontChanged);
    connect(app, &QGuiApplication::fontChanged, optionsDialog, &OptionsDialog::fontChanged);
    connect(app, &QGuiApplication::fontChanged, profileWidget, &ProfileWidget::fontChanged);
    connect(app, &QGuiApplication::fontChanged, menuWidget(), &QWidget::setFont);
  }

  // Warning when selecting export options ===================================================================
  connect(ui->actionRouteSaveApprWaypointsOpt, &QAction::toggled, routeExport, &RouteExport::warnExportOptionsFromMenu);
  connect(ui->actionRouteSaveSidStarWaypointsOpt, &QAction::toggled, routeExport, &RouteExport::warnExportOptionsFromMenu);
  connect(ui->actionRouteSaveAirwayWaypointsOpt, &QAction::toggled, routeExport, &RouteExport::warnExportOptionsFromMenu);

  // Updated manually in dialog
  // connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getWebController(), &WebController::optionsChanged);

  // Style handler ===================================================================
  // Save complete state due to crashes in Qt
  AircraftPerfController *perfController = NavApp::getAircraftPerfController();
  const StyleHandler *styleHandler = NavApp::getStyleHandler();
  connect(styleHandler, &StyleHandler::preStyleChange, this, &MainWindow::saveStateNow);
  connect(styleHandler, &StyleHandler::styleChanged, mapcolors::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, infoController, &InfoController::optionsChanged);
  connect(styleHandler, &StyleHandler::styleChanged, routeController, &RouteController::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, searchController, &SearchController::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, mapWidget, &MapPaintWidget::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, profileWidget, &ProfileWidget::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, perfController, &AircraftPerfController::optionsChanged);
  connect(styleHandler, &StyleHandler::styleChanged, infoController, &InfoController::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, optionsDialog, &OptionsDialog::styleChanged);
  connect(styleHandler, &StyleHandler::styleChanged, this, &MainWindow::updateStatusBarStyle);
  connect(styleHandler, &StyleHandler::styleChanged, NavApp::getLogdataController(), &LogdataController::styleChanged);

  // WindReporter ===================================================================================
  // Wind has to be calculated first - receive routeChanged signal first
  connect(routeController, &RouteController::routeChanged, windReporter, &WindReporter::updateToolButtonState);
  connect(perfController, &AircraftPerfController::aircraftPerformanceChanged, windReporter, &WindReporter::updateToolButtonState);

  // Aircraft performance signals =======================================================
  connect(ui->actionAircraftPerformanceWarnMismatch, &QAction::toggled, perfController, &AircraftPerfController::warningChanged);

  connect(perfController, &AircraftPerfController::aircraftPerformanceChanged, routeController,
          &RouteController::aircraftPerformanceChanged);
  connect(perfController, &AircraftPerfController::windChanged, routeController, &RouteController::windUpdated);
  connect(perfController, &AircraftPerfController::windChanged, profileWidget, &ProfileWidget::windUpdated);
  connect(perfController, &AircraftPerfController::windChanged, mapWidget, &MapWidget::windDisplayUpdated);

  connect(routeController, &RouteController::routeChanged, perfController, &AircraftPerfController::routeChanged);
  connect(routeController, &RouteController::routeAltitudeChanged, perfController, &AircraftPerfController::routeAltitudeChanged);

  // Route export signals =======================================================
  connect(routeExport, &RouteExport::optionsUpdated, this, &MainWindow::updateActionStates);
  connect(routeExport, &RouteExport::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(routeExport, &RouteExport::selectDepartureParking, routeController, &RouteController::selectDepartureParking);

  // Fetch and upload to/from SimBrief
  connect(ui->actionRouteSendToSimBrief, &QAction::triggered, simbriefHandler, &SimBriefHandler::sendRouteToSimBrief);
  connect(ui->actionRouteFetchFromSimBrief, &QAction::triggered, simbriefHandler, &SimBriefHandler::fetchRouteFromSimBrief);

  // Route controller signals =======================================================
  connect(routeController, &RouteController::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(routeController, &RouteController::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(routeController, &RouteController::changeMark, mapWidget, &MapWidget::changeSearchMark);
  connect(routeController, &RouteController::routeChanged, mapWidget, &MapPaintWidget::routeChanged);
  connect(routeController, &RouteController::routeAltitudeChanged, mapWidget, &MapPaintWidget::routeAltitudeChanged);
  connect(routeController, &RouteController::preRouteCalc, profileWidget, &ProfileWidget::preRouteCalc);
  connect(routeController, &RouteController::showInformation, infoController, &InfoController::showInformation);
  connect(routeController, &RouteController::addUserpointFromMap,
          NavApp::getUserdataController(), &UserdataController::addUserpointFromMap);
  connect(routeController, &RouteController::showProcedures, searchController->getProcedureSearch(), &ProcedureSearch::showProcedures);

  // Update rubber band in map window if user hovers over profile
  connect(profileWidget, &ProfileWidget::highlightProfilePoint, mapWidget, &MapPaintWidget::changeProfileHighlight);
  connect(profileWidget, &ProfileWidget::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(profileWidget, &ProfileWidget::profileAltCalculationFinished, routeController, &RouteController::updateModelTimeFuelWindAlt);

  connect(routeController, &RouteController::routeChanged, profileWidget, &ProfileWidget::routeChanged);
  connect(routeController, &RouteController::routeAltitudeChanged, profileWidget, &ProfileWidget::routeAltitudeChanged);
  connect(routeController, &RouteController::routeChanged, this, &MainWindow::updateActionStates);
  connect(routeController, &RouteController::routeInsert, this, &MainWindow::routeInsert);
  connect(routeController, &RouteController::addAirportMsa, mapWidget, &MapWidget::addMsaMark);

  connect(routeController, &RouteController::routeChanged, NavApp::updateWindowTitle);
  connect(routeController, &RouteController::routeChanged, infoController, &InfoController::routeChanged);

  // Add departure and dest runway actions separately to windows since their shortcuts overlap with context menu shortcuts
  const static QList<QAction *> ACTIONS({ui->actionShowDepartureCustom, ui->actionShowApproachCustom});
  mapWidget->addActions(ACTIONS);
  ui->dockWidgetInformation->addActions(ACTIONS);
  ui->dockWidgetAircraft->addActions(ACTIONS);
  ui->dockWidgetProfile->addActions(ACTIONS);
  ui->actionShowDepartureCustom->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionShowApproachCustom->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(ui->actionShowDepartureCustom, &QAction::triggered, routeController, &RouteController::showCustomDepartureMainMenu);
  connect(ui->actionShowApproachCustom, &QAction::triggered, routeController, &RouteController::showCustomApproachMainMenu);

  // Airport search ===================================================================================
  AirportSearch *airportSearch = searchController->getAirportSearch();
  connect(airportSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(airportSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(airportSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(airportSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(airportSearch, &SearchBaseTable::showProcedures, searchController->getProcedureSearch(), &ProcedureSearch::showProcedures);
  connect(airportSearch, &SearchBaseTable::showCustomApproach, routeController, &RouteController::showCustomApproach);
  connect(airportSearch, &SearchBaseTable::showCustomDeparture, routeController, &RouteController::showCustomDeparture);
  connect(airportSearch, &SearchBaseTable::routeSetDeparture, routeController, &RouteController::routeSetDeparture);
  connect(airportSearch, &SearchBaseTable::routeSetDestination, routeController, &RouteController::routeSetDestination);
  connect(airportSearch, &SearchBaseTable::routeAddAlternate, routeController, &RouteController::routeAddAlternate);
  connect(airportSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);
  connect(airportSearch, &SearchBaseTable::routeDirectTo, routeController, &RouteController::routeDirectTo);
  connect(airportSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);
  connect(airportSearch, &SearchBaseTable::addAirportMsa, mapWidget, &MapWidget::addMsaMark);
  connect(airportSearch, &SearchBaseTable::addUserpointFromMap, NavApp::getUserdataController(), &UserdataController::addUserpointFromMap);

  // Nav search ===================================================================================
  NavSearch *navSearch = searchController->getNavSearch();
  connect(navSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(navSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(navSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(navSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);
  connect(navSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);
  connect(navSearch, &SearchBaseTable::addAirportMsa, mapWidget, &MapWidget::addMsaMark);

  // Userdata search ===================================================================================
  UserdataSearch *userSearch = searchController->getUserdataSearch();
  connect(userSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(userSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(userSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(userSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);
  connect(userSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);

  // Logbook search ===================================================================================
  LogdataSearch *logSearch = searchController->getLogdataSearch();
  connect(logSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(logSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(logSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(logSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);
  connect(logSearch, &SearchBaseTable::routeSetDeparture, routeController, &RouteController::routeSetDeparture);
  connect(logSearch, &SearchBaseTable::routeSetDestination, routeController, &RouteController::routeSetDestination);
  connect(logSearch, &SearchBaseTable::routeAddAlternate, routeController, &RouteController::routeAddAlternate);

  // User data ===================================================================================
  UserdataController *userdataController = NavApp::getUserdataController();
  // Import ================
  connect(ui->actionUserdataImportCSV, &QAction::triggered, userdataController, &UserdataController::importCsv);
  connect(ui->actionUserdataImportGarminGTN, &QAction::triggered, userdataController, &UserdataController::importGarmin);
  connect(ui->actionUserdataImportUserfixDat, &QAction::triggered, userdataController, &UserdataController::importXplaneUserFixDat);

  // Export ================
  connect(ui->actionUserdataExportCSV, &QAction::triggered, userdataController, &UserdataController::exportCsv);
  connect(ui->actionUserdataExportGarminGTN, &QAction::triggered, userdataController, &UserdataController::exportGarmin);
  connect(ui->actionUserdataExportUserfixDat, &QAction::triggered, userdataController, &UserdataController::exportXplaneUserFixDat);
  connect(ui->actionUserdataExportXmlBgl, &QAction::triggered, userdataController, &UserdataController::exportBglXml);

  connect(userdataController, &UserdataController::userdataChanged, infoController, &InfoController::updateAllInformation);
  connect(userdataController, &UserdataController::userdataChanged, this, &MainWindow::updateMapObjectsShown);
  connect(userdataController, &UserdataController::refreshUserdataSearch, userSearch, &UserdataSearch::refreshData);

  // Map marks, holds, etc.  ===================================================================================
  MapMarkHandler *mapMarkHandler = NavApp::getMapMarkHandler();
  connect(mapMarkHandler, &MapMarkHandler::updateMarkTypes, this, &MainWindow::updateMapObjectsShown);

  // Airports  ===================================================================================
  MapAirportHandler *mapAirportHandler = NavApp::getMapAirportHandler();
  connect(mapAirportHandler, &MapAirportHandler::updateAirportTypes, this, &MainWindow::updateMapObjectsShown);

  // Logbook ===================================================================================
  LogdataController *logdataController = NavApp::getLogdataController();
  connect(logdataController, &LogdataController::refreshLogSearch, logSearch, &LogdataSearch::refreshData);
  connect(logdataController, &LogdataController::logDataChanged, mapWidget, &MapWidget::updateLogEntryScreenGeometry);
  connect(logdataController, &LogdataController::logDataChanged, this, &MainWindow::updateMapObjectsShown);
  connect(logdataController, &LogdataController::logDataChanged, infoController, &InfoController::updateAllInformation);

  connect(mapWidget, &MapWidget::aircraftTakeoff, logdataController, &LogdataController::aircraftTakeoff);
  connect(mapWidget, &MapWidget::aircraftLanding, logdataController, &LogdataController::aircraftLanding);

  connect(logdataController, &LogdataController::showInSearch, searchController, &SearchController::showInSearch);

  connect(ui->actionLogdataShowStatistics, &QAction::triggered, logdataController, &LogdataController::statisticsLogbookShow);
  connect(ui->actionLogdataImportCSV, &QAction::triggered, logdataController, &LogdataController::importCsv);
  connect(ui->actionLogdataExportCSV, &QAction::triggered, logdataController, &LogdataController::exportCsv);
  connect(ui->actionLogdataImportXplane, &QAction::triggered, logdataController, &LogdataController::importXplane);
  connect(ui->actionLogdataConvertUserdata, &QAction::triggered,
          logdataController, &LogdataController::convertUserdata);

  connect(searchController->getLogdataSearch(), &SearchBaseTable::loadRouteFile, this, &MainWindow::routeOpenFile);
  connect(searchController->getLogdataSearch(), &SearchBaseTable::loadPerfFile,
          NavApp::getAircraftPerfController(), &AircraftPerfController::loadFile);

  // Online search ===================================================================================
  OnlineClientSearch *clientSearch = searchController->getOnlineClientSearch();
  OnlineCenterSearch *centerSearch = searchController->getOnlineCenterSearch();
  OnlineServerSearch *serverSearch = searchController->getOnlineServerSearch();
  OnlinedataController *onlinedataController = NavApp::getOnlinedataController();

  // Online client search ===================================================================================
  connect(clientSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(clientSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(clientSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(clientSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(clientSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);

  // Online center search ===================================================================================
  connect(centerSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(centerSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(centerSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(centerSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(centerSearch, &SearchBaseTable::selectionChanged, searchController, &SearchController::searchSelectionChanged);

  // Remove/add buttons and tabs
  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged,
          this, &MainWindow::updateOnlineActionStates);

  // Update search
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated, clientSearch, &OnlineClientSearch::refreshData);
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated, centerSearch, &OnlineCenterSearch::refreshData);
  connect(onlinedataController, &OnlinedataController::onlineServersUpdated, serverSearch, &OnlineServerSearch::refreshData);

  // Clear cache and update map widget
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated,
          NavApp::getAirspaceController(), &AirspaceController::onlineClientAndAtcUpdated);
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated, mapWidget, &MapPaintWidget::onlineClientAndAtcUpdated);
  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged, mapWidget, &MapPaintWidget::onlineNetworkChanged);

  // Update info
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated, infoController,
          &InfoController::onlineClientAndAtcUpdated);
  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged, infoController, &InfoController::onlineNetworkChanged);

  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged,
          NavApp::getAirspaceController(), &AirspaceController::updateButtonsAndActions);
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated,
          NavApp::getAirspaceController(), &AirspaceController::updateButtonsAndActions);

  // Approach controller ===================================================================
  ProcedureSearch *procedureSearch = searchController->getProcedureSearch();
  connect(procedureSearch, &ProcedureSearch::procedureSelected, this, &MainWindow::procedureSelected);
  connect(procedureSearch, &ProcedureSearch::proceduresSelected, this, &MainWindow::proceduresSelected);
  connect(procedureSearch, &ProcedureSearch::procedureLegSelected, this, &MainWindow::procedureLegSelected);
  connect(procedureSearch, &ProcedureSearch::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(procedureSearch, &ProcedureSearch::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(procedureSearch, &ProcedureSearch::routeInsertProcedure, routeController, &RouteController::routeAddProcedure);
  connect(procedureSearch, &ProcedureSearch::showInformation, infoController, &InfoController::showInformation);

  connect(ui->actionResetLayout, &QAction::triggered, this, &MainWindow::resetWindowLayout);
  connect(ui->actionResetTabs, &QAction::triggered, this, &MainWindow::resetTabLayout);
  connect(ui->actionResetAllSettings, &QAction::triggered, this, &MainWindow::resetAllSettings);
  connect(ui->actionCreateACrashReport, &QAction::triggered, this, &MainWindow::createIssueReport);

  connect(infoController, &InfoController::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(infoController, &InfoController::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(infoController, &InfoController::showProcedures, searchController->getProcedureSearch(), &ProcedureSearch::showProcedures);

  // Use this event to show scenery library dialog on first start after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);

  // Scenery library menu ============================================================
  DatabaseManager *databaseManager = NavApp::getDatabaseManager();
  connect(ui->actionLoadAirspaces, &QAction::triggered, NavApp::getAirspaceController(), &AirspaceController::loadAirspaces);
  connect(ui->actionReloadScenery, &QAction::triggered, databaseManager, &DatabaseManager::loadScenery);
  connect(ui->actionValidateSceneryLibrarySettings, &QAction::triggered, databaseManager, &DatabaseManager::checkSceneryOptionsManual);
  connect(ui->actionDatabaseFiles, &QAction::triggered, this, &MainWindow::showDatabaseFiles);
  connect(ui->actionShowMapCache, &QAction::triggered, this, &MainWindow::showShowMapCache);
  connect(ui->actionShowMapInstallation, &QAction::triggered, this, &MainWindow::showMapInstallation);

  connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::openOptionsDialog);
  connect(ui->actionResetMessages, &QAction::triggered, this, &MainWindow::resetMessages);
  connect(ui->actionCreateDirStructure, &QAction::triggered, this, &MainWindow::runDirToolManual);
  connect(ui->actionSaveAllNow, &QAction::triggered, this, &MainWindow::saveStateNow);

  // Windows menu ============================================================
  connect(ui->actionShowFloatingWindows, &QAction::triggered, this, &MainWindow::raiseFloatingWindows);
  connect(ui->actionWindowStayOnTop, &QAction::toggled, this, &MainWindow::stayOnTop);
  connect(ui->actionShowAllowDocking, &QAction::toggled, this, &MainWindow::allowDockingWindows);
  connect(ui->actionShowAllowMoving, &QAction::toggled, this, &MainWindow::allowMovingWindows);
  connect(ui->actionShowWindowTitleBar, &QAction::toggled, this, &MainWindow::hideTitleBar);
  connect(ui->actionShowFullscreenMap, &QAction::toggled, this, &MainWindow::fullScreenMapToggle);

  // File menu ============================================================
  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);

  // Flight plan file actions ============================================================
  connect(ui->actionRouteResetAll, &QAction::triggered, mapMarkHandler, &MapMarkHandler::routeResetAll);

  connect(ui->actionRouteCenter, &QAction::triggered, this, &MainWindow::routeCenter);
  connect(ui->actionRouteNew, &QAction::triggered, this, &MainWindow::routeNew);
  connect(ui->actionRouteNewFromString, &QAction::triggered, this, &MainWindow::routeFromStringCurrent);
  connect(ui->actionRouteOpen, &QAction::triggered, this, &MainWindow::routeOpen);
  connect(ui->actionRouteAppend, &QAction::triggered, this, &MainWindow::routeAppend);
  connect(ui->actionRouteTableAppend, &QAction::triggered, this, &MainWindow::routeAppend);
  connect(ui->actionRouteSaveSelection, &QAction::triggered, this, &MainWindow::routeSaveSelection);
  connect(ui->actionRouteSave, &QAction::triggered, this, &MainWindow::routeSaveLnm);
  connect(ui->actionRouteSaveAs, &QAction::triggered, this, &MainWindow::routeSaveAsLnm);

  // Flight plan export actions =====================================================================
  /* *INDENT-OFF* */
  connect(ui->actionRouteSaveAsPln, &QAction::triggered, this, [this]() { routeExport->routeExportPlnMan(); });
  connect(ui->actionRouteSaveAsPlnMsfs, &QAction::triggered, this, [this]() { routeExport->routeExportPlnMsfsMan(); });
  connect(ui->actionRouteSaveAsFms11, &QAction::triggered, this, [this]() { routeExport->routeExportFms11Man(); });
  connect(ui->actionRouteSaveAsFlightGear, &QAction::triggered, this, [this]() { routeExport->routeExportFlightgearMan(); });
  connect(ui->actionRouteSaveAll, &QAction::triggered, this, [this]() { routeExport->routeMultiExport(); });
  connect(ui->actionRouteSaveAllOptions, &QAction::triggered, this, [this]() { routeExport->routeMultiExportOptions(); });

  connect(ui->actionRouteSaveAsHtml, &QAction::triggered, this, [this]() { routeExport->routeExportHtmlMan(); });

  // Online export options
  connect(ui->actionRouteSaveAsVfp, &QAction::triggered, this, [this]() { routeExport->routeExportVfpMan(); });
  connect(ui->actionRouteSaveAsIvap, &QAction::triggered, this, [this]() { routeExport->routeExportIvapMan(); });
  connect(ui->actionRouteSaveAsXIvap, &QAction::triggered, this, [this]() { routeExport->routeExportXIvapMan(); });

  // GPX export and import options
  connect(ui->actionSaveAircraftTrailToGPX, &QAction::triggered, this, [this]() { routeExport->routeExportGpxMan(); });
  connect(ui->actionLoadAircraftTrailFromGPX, &QAction::triggered, this, &MainWindow::trailLoadGpx);
  connect(ui->actionAppendAircraftTrailFromGPX, &QAction::triggered, this, &MainWindow::trailAppendGpx);

  /* *INDENT-ON* */

  connect(ui->actionRouteShowSkyVector, &QAction::triggered, this, &MainWindow::openInSkyVector);

  connect(routeFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::routeOpenRecent);

  connect(ui->actionPrintMap, &QAction::triggered, printSupport, &PrintSupport::printMap);
  connect(ui->actionPrintFlightplan, &QAction::triggered, printSupport, &PrintSupport::printFlightplan);
  connect(ui->actionSaveMapAsImage, &QAction::triggered, this, &MainWindow::mapSaveImage);
  connect(ui->actionSaveMapAsImageAviTab, &QAction::triggered, this, &MainWindow::mapSaveImageAviTab);
  connect(ui->actionMapCopyClipboard, &QAction::triggered, this, &MainWindow::mapCopyToClipboard);

  // KML actions
  connect(ui->actionLoadKml, &QAction::triggered, this, &MainWindow::kmlOpen);
  connect(ui->actionClearKml, &QAction::triggered, this, &MainWindow::kmlClear);
  connect(kmlFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::kmlOpenRecent);

  // Flight plan calculation ========================================================================
  connect(ui->actionRouteCalcDirect, &QAction::triggered, routeController, &RouteController::calculateDirect);
  connect(ui->actionRouteCalc, &QAction::triggered, routeController, &RouteController::calculateRouteWindowShow);
  connect(ui->actionRouteReverse, &QAction::triggered, routeController, &RouteController::reverseRoute);
  connect(ui->actionRouteCopyString, &QAction::triggered, routeController, &RouteController::routeStringToClipboard);
  connect(ui->actionRouteAdjustAltitude, &QAction::triggered, routeController, &RouteController::adjustFlightplanAltitude);

  // Help menu ========================================================================
  connect(ui->actionHelpUserManualContents, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineMainUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualStart, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineStartUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualTutorials, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineTutorialsUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualMainMenu, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineMainMenuUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualMapDisplay, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineMapDisplayUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualAircraftPerf, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineAircraftPerfUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualFlightPlanning, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineFlightPlanningUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpUserManualUserInterface, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineUserInterfaceUrl, lnm::helpLanguageOnline());
  });

  // Legend ===============================================
  connect(ui->actionHelpUserManualLegend, &QAction::triggered, this, [this](bool)->void {
    helpHandler->openHelpUrlWeb(lnm::helpOnlineLegendUrl, lnm::helpLanguageOnline());
  });

  connect(ui->actionHelpContentsOffline, &QAction::triggered, this, &MainWindow::showOfflineHelp);
  connect(ui->actionHelpDownloads, &QAction::triggered, this, &MainWindow::showOnlineDownloads);
  connect(ui->actionHelpChangelog, &QAction::triggered, this, &MainWindow::showChangelog);
  connect(ui->actionHelpAbout, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::about);
  connect(ui->actionHelpAboutQt, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::aboutQt);
  connect(ui->actionHelpCheckUpdates, &QAction::triggered, this, &MainWindow::checkForUpdates);
  connect(ui->actionHelpDonate, &QAction::triggered, this, &MainWindow::showDonationPage);
  connect(ui->actionHelpFaq, &QAction::triggered, this, &MainWindow::showFaqPage);
  connect(ui->actionHelpOpenLogFile, &QAction::triggered, this, &MainWindow::openLogFile);
  connect(ui->actionHelpOpenConfigFile, &QAction::triggered, this, &MainWindow::openConfigFile);

  // Map widget related connections
  connect(mapWidget, &MapWidget::showInSearch, searchController, &SearchController::showInSearch);
  // Connect the map widget to the position label.
  connect(mapWidget, &MapPaintWidget::distanceChanged, this, &MainWindow::distanceChanged);
  connect(mapWidget, &MapPaintWidget::renderStateChanged, this, &MainWindow::renderStatusChanged);
  connect(mapWidget, &MapPaintWidget::updateActionStates, this, &MainWindow::updateActionStates);
  connect(mapWidget, &MapWidget::showInformation, infoController, &InfoController::showInformation);
  connect(mapWidget, &MapWidget::showProcedures, searchController->getProcedureSearch(), &ProcedureSearch::showProcedures);
  connect(mapWidget, &MapWidget::showCustomApproach, routeController, &RouteController::showCustomApproach);
  connect(mapWidget, &MapWidget::showCustomDeparture, routeController, &RouteController::showCustomDeparture);
  connect(mapWidget, &MapPaintWidget::shownMapFeaturesChanged, routeController, &RouteController::shownMapFeaturesChanged);
  connect(mapWidget, &MapWidget::showInRoute, this, &MainWindow::actionShortcutFlightPlanTriggered);
  connect(mapWidget, &MapWidget::showInRoute, routeController, &RouteController::showInRoute);
  connect(mapWidget, &MapWidget::addUserpointFromMap, NavApp::getUserdataController(), &UserdataController::addUserpointFromMap);
  connect(mapWidget, &MapWidget::editUserpointFromMap, NavApp::getUserdataController(), &UserdataController::editUserpointFromMap);
  connect(mapWidget, &MapWidget::deleteUserpointFromMap, NavApp::getUserdataController(), &UserdataController::deleteUserpointFromMap);
  connect(mapWidget, &MapWidget::moveUserpointFromMap, NavApp::getUserdataController(), &UserdataController::moveUserpointFromMap);

  connect(mapWidget, &MapWidget::editLogEntryFromMap, NavApp::getLogdataController(), &LogdataController::editLogEntryFromMap);
  connect(mapWidget, &MapWidget::exitFullScreenPressed, this, &MainWindow::exitFullScreenPressed);

  connect(mapWidget, &MapWidget::routeInsertProcedure, routeController, &RouteController::routeAddProcedure);

  // Map needs to restore title bar state when floating
  connect(ui->dockWidgetMap, &QDockWidget::topLevelChanged, this, &MainWindow::mapDockTopLevelChanged);

  // Window menu ======================================
  connect(layoutFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::layoutOpenRecent);
  connect(ui->actionWindowLayoutOpen, &QAction::triggered, this, &MainWindow::layoutOpen);
  connect(ui->actionWindowLayoutSaveAs, &QAction::triggered, this, &MainWindow::layoutSaveAs);

  // Map object/feature display
  connect(ui->actionMapShowMinimumAltitude, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAirportWeather, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowCities, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowGrid, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVor, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowNdb, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWp, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowHolding, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAirportMsa, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVictorAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowJetAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowTracks, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowRoute, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowTocTod, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAlternate, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);

  connect(ui->actionMapShowIls, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowGls, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  /* *INDENT-OFF* */
  connect(ui->actionMapShowIls,  &QAction::toggled, profileWidget, &ProfileWidget::showIlsChanged);
  connect(ui->actionMapShowGls,  &QAction::toggled, profileWidget, &ProfileWidget::showIlsChanged);
  /* *INDENT-ON* */

  // MARK_RANGE  MARK_DISTANCE MARK_HOLDING  MARK_PATTERNS MARK_MSA
  connect(ui->actionMapHideAllRangeRings, &QAction::triggered, mapMarkHandler, &MapMarkHandler::clearRangeRings);
  connect(ui->actionMapHideAllDistanceMarkers, &QAction::triggered, mapMarkHandler, &MapMarkHandler::clearDistanceMarkers);
  connect(ui->actionMapHideAllHoldings, &QAction::triggered, mapMarkHandler, &MapMarkHandler::clearHoldings);
  connect(ui->actionMapHideAllPatterns, &QAction::triggered, mapMarkHandler, &MapMarkHandler::clearPatterns);
  connect(ui->actionMapHideAllMsa, &QAction::triggered, mapMarkHandler, &MapMarkHandler::clearMsa);

  // Logbook view options ============================================
  connect(ui->actionSearchLogdataShowDirect, &QAction::toggled, logdataController, &LogdataController::displayOptionsChanged);
  connect(ui->actionSearchLogdataShowRoute, &QAction::toggled, logdataController, &LogdataController::displayOptionsChanged);
  connect(ui->actionSearchLogdataShowTrack, &QAction::toggled, logdataController, &LogdataController::displayOptionsChanged);

  connect(ui->actionSearchLogdataShowDirect, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionSearchLogdataShowRoute, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionSearchLogdataShowTrack, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);

  connect(ui->actionMapShowAirportWeather, &QAction::toggled, infoController, &InfoController::updateAirportWeather);

  // Clear selection and highlights
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, routeController, &RouteController::clearTableSelection);
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, searchController, &SearchController::clearSelection);
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, mapWidget, &MapPaintWidget::clearSearchHighlights);
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, mapWidget, &MapPaintWidget::clearAirspaceHighlights);
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, mapWidget, &MapPaintWidget::clearAirwayHighlights);
  connect(ui->actionMapClearAllHighlights, &QAction::triggered, this, &MainWindow::updateHighlightActionStates);

  connect(ui->actionInfoApproachShowMissedAppr, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);

  connect(ui->actionMapShowCompassRose, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowCompassRoseAttach, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowEndurance, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowSelectedAltRange, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowTurnPath, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraft, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftAi, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftOnline, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftAiBoat, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionShowAirspaces, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapResetSettings, &QAction::triggered, this, &MainWindow::resetMapObjectsShown);

  // Airspace sources ======
  AirspaceController *airspaceController = NavApp::getAirspaceController();
  connect(airspaceController, &AirspaceController::updateAirspaceSources, this, &MainWindow::updateMapObjectsShown);
  connect(airspaceController, &AirspaceController::updateAirspaceSources, this, &MainWindow::updateAirspaceSources);

  // Other airspace signals ======
  connect(airspaceController, &AirspaceController::updateAirspaceTypes, this, &MainWindow::updateAirspaceTypes);
  connect(airspaceController, &AirspaceController::userAirspacesUpdated,
          NavApp::getOnlinedataController(), &OnlinedataController::userAirspacesUpdated);

  // Connect airspace manger signals to database manager signals
  connect(airspaceController, &AirspaceController::preDatabaseLoadAirspaces, databaseManager, &DatabaseManager::preDatabaseLoad);
  connect(airspaceController, &AirspaceController::postDatabaseLoadAirspaces, databaseManager, &DatabaseManager::postDatabaseLoad);

  connect(ui->actionMapShowAircraft, &QAction::toggled, profileWidget, &ProfileWidget::updateProfileShowFeatures);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, profileWidget, &ProfileWidget::updateProfileShowFeatures);

  // Airway/tracks =======================================================
  TrackController *trackController = NavApp::getTrackController();
  connect(trackController, &TrackController::postTrackLoad, routeController, &RouteController::clearAirwayNetworkCache);
  connect(trackController, &TrackController::postTrackLoad, infoController, &InfoController::tracksChanged);
  connect(trackController, &TrackController::postTrackLoad, this, &MainWindow::updateMapObjectsShown);
  connect(trackController, &TrackController::postTrackLoad, routeController, &RouteController::tracksChanged);
  connect(trackController, &TrackController::postTrackLoad, mapWidget, &MapPaintWidget::postTrackLoad);

  connect(ui->actionRouteDownloadTracks, &QAction::toggled, trackController, &TrackController::downloadToggled);
  connect(ui->actionRouteDownloadTracksNow, &QAction::triggered, trackController, &TrackController::startDownload);
  connect(ui->actionRouteDeleteTracks, &QAction::triggered, trackController, &TrackController::deleteTracks);

  // Weather source =======================================================
  connect(ui->actionMapShowWeatherDisabled, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWeatherSimulator, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWeatherActiveSky, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWeatherNoaa, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWeatherVatsim, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWeatherIvao, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);

  // Update map weather source highlights =======================================================
  connect(ui->actionMapShowWeatherDisabled, &QAction::toggled, infoController, &InfoController::updateAirportWeather);
  connect(ui->actionMapShowWeatherSimulator, &QAction::toggled, infoController, &InfoController::updateAirportWeather);
  connect(ui->actionMapShowWeatherActiveSky, &QAction::toggled, infoController, &InfoController::updateAirportWeather);
  connect(ui->actionMapShowWeatherNoaa, &QAction::toggled, infoController, &InfoController::updateAirportWeather);
  connect(ui->actionMapShowWeatherVatsim, &QAction::toggled, infoController, &InfoController::updateAirportWeather);
  connect(ui->actionMapShowWeatherIvao, &QAction::toggled, infoController, &InfoController::updateAirportWeather);

  // Update airport index in weather for changed simulator database
  connect(ui->actionMapShowWeatherDisabled, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherSimulator, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherActiveSky, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherNoaa, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherVatsim, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherIvao, &QAction::toggled, weatherReporter, &WeatherReporter::updateAirportWeather);

  // Sun shading =======================================================
  connect(ui->actionMapShowSunShading, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowSunShadingSimulatorTime, &QAction::triggered, this, &MainWindow::sunShadingTimeChanged);
  connect(ui->actionMapShowSunShadingRealTime, &QAction::triggered, this, &MainWindow::sunShadingTimeChanged);
  connect(ui->actionMapShowSunShadingUserTime, &QAction::triggered, this, &MainWindow::sunShadingTimeChanged);
  connect(ui->actionMapShowSunShadingSetTime, &QAction::triggered, this, &MainWindow::sunShadingTimeSet);

  // Update information after updateMapObjectsShownr updated the flags ============================
  connect(ui->actionMapShowAircraft, &QAction::toggled, infoController, &InfoController::updateAllInformation);
  connect(ui->actionMapShowAircraftAi, &QAction::toggled, infoController, &InfoController::updateAllInformation);
  connect(ui->actionMapShowAircraftOnline, &QAction::toggled, infoController, &InfoController::updateAllInformation);
  connect(ui->actionMapShowAircraftAiBoat, &QAction::toggled, infoController, &InfoController::updateAllInformation);

  // Order is important here. First let the mapwidget delete the track then notify the profile
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered,
          this, [this]() {
    deleteAircraftTrail(false /* quiet */);
  });

  connect(ui->actionMapShowMark, &QAction::triggered, mapWidget, &MapWidget::showSearchMark);
  connect(ui->actionMapShowHome, &QAction::triggered, mapWidget, &MapWidget::showHome);
  connect(ui->actionMapJumpCoordinatesMain, &QAction::triggered, mapWidget, &MapWidget::jumpCoordinates);
  connect(ui->actionMapAircraftCenter, &QAction::toggled, mapWidget, &MapPaintWidget::showAircraft);
  connect(ui->actionMapAircraftCenterNow, &QAction::triggered, mapWidget, &MapPaintWidget::showAircraftNow);
  connect(ui->actionMapShowGridConfig, &QAction::triggered, mapWidget, &MapWidget::showGridConfiguration);

  // Update jump back
  connect(ui->actionMapAircraftCenter, &QAction::toggled, mapWidget, &MapPaintWidget::jumpBackToAircraftCancel);

  // Map history ===========================================
  connect(ui->actionMapBack, &QAction::triggered, mapWidget, &MapWidget::historyBack);
  connect(ui->actionMapNext, &QAction::triggered, mapWidget, &MapWidget::historyNext);
  connect(mapWidget->getHistory(), &MapPosHistory::historyChanged, this, &MainWindow::updateMapHistoryActions);

  // Map details ===========================================
  MapDetailHandler *mapDetailHandler = NavApp::getMapDetailHandler();
  connect(ui->actionMapDetailsMore, &QAction::triggered, mapDetailHandler, &MapDetailHandler::increaseMapDetail);
  connect(ui->actionMapDetailsLess, &QAction::triggered, mapDetailHandler, &MapDetailHandler::decreaseMapDetail);
  connect(ui->actionMapDetailsDefault, &QAction::triggered, mapDetailHandler, &MapDetailHandler::defaultMapDetail);

  connect(mapDetailHandler, &MapDetailHandler::updateDetailLevel, mapWidget, &MapWidget::setMapDetail);
  connect(mapDetailHandler, &MapDetailHandler::updateDetailLevel, this, &MainWindow::updateMapObjectsShown);

  // Route editing ========================================
  connect(routeController, &RouteController::routeSelectionChanged, this, &MainWindow::routeSelectionChanged);
  connect(ui->actionRouteSelectParking, &QAction::triggered, routeController, &RouteController::selectDepartureParking);

  connect(mapWidget, &MapWidget::routeSetStart, routeController, &RouteController::routeSetDeparture);
  connect(mapWidget, &MapWidget::routeSetParkingStart, routeController, &RouteController::routeSetParking);
  connect(mapWidget, &MapWidget::routeSetHelipadStart, routeController, &RouteController::routeSetHelipad);
  connect(mapWidget, &MapWidget::routeSetDest, routeController, &RouteController::routeSetDestination);
  connect(mapWidget, &MapWidget::routeAddAlternate, routeController, &RouteController::routeAddAlternate);
  connect(mapWidget, &MapWidget::routeAdd, routeController, &RouteController::routeAdd);
  connect(mapWidget, &MapWidget::directTo, routeController, &RouteController::routeDirectTo);
  connect(mapWidget, &MapWidget::routeReplace, routeController, &RouteController::routeReplace);

  // Messages about database query result status - use queued to avoid blocking paint
  connect(mapWidget, &MapPaintWidget::resultTruncated, this, &MainWindow::resultTruncated, Qt::QueuedConnection);

  connect(mapWidget, &MapWidget::userAircraftValidChanged, this, &MainWindow::updateActionStates);

  connect(databaseManager, &DatabaseManager::preDatabaseLoad, this, &MainWindow::preDatabaseLoad);
  connect(databaseManager, &DatabaseManager::postDatabaseLoad, this, &MainWindow::postDatabaseLoad);

  connect(ui->actionAboutMarble, &QAction::triggered, marbleAboutDialog, &Marble::MarbleAboutDialog::exec);

  // Connect menu ========================================================================
  ConnectClient *connectClient = NavApp::getConnectClient();
  connect(ui->actionConnectSimulator, &QAction::triggered, connectClient, &ConnectClient::connectToServerDialog);
  connect(ui->actionConnectSimulatorToggle, &QAction::toggled, connectClient, &ConnectClient::connectToggle);

  // Deliver first to route controller to update active leg and distances
  connect(connectClient, &ConnectClient::dataPacketReceived, routeController, &RouteController::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived, mapWidget, &MapWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived, profileWidget, &ProfileWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived, infoController, &InfoController::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived, NavApp::getAircraftPerfController(), &AircraftPerfController::simDataChanged);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          NavApp::getAircraftPerfController(), &AircraftPerfController::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          NavApp::getAircraftPerfController(), &AircraftPerfController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator, routeController, &RouteController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator, this, &MainWindow::sunShadingTimeChanged);

  // Map widget needs to clear track first
  connect(connectClient, &ConnectClient::connectedToSimulator, mapWidget, &MapPaintWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, mapWidget, &MapPaintWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, this, &MainWindow::updateActionStates);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, this, &MainWindow::updateActionStates);

  // Do not show update dialogs while connected
  connect(connectClient, &ConnectClient::connectedToSimulator, NavApp::getUpdateHandler(), &UpdateHandler::disableUpdateCheck);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, NavApp::getUpdateHandler(), &UpdateHandler::enableUpdateCheck);

  connect(connectClient, &ConnectClient::connectedToSimulator, infoController, &InfoController::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, infoController, &InfoController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, profileWidget, &ProfileWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, profileWidget, &ProfileWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, routeController, &RouteController::updateFooterErrorLabel);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, routeController, &RouteController::updateFooterErrorLabel);

  connect(connectClient, &ConnectClient::aiFetchOptionsChanged, this, &MainWindow::updateActionStates);

  connect(mapWidget, &MapPaintWidget::aircraftTrackPruned, profileWidget, &ProfileWidget::aircraftTrailPruned);

  // Weather update ===================================================
  connect(weatherReporter, &WeatherReporter::weatherUpdated, mapWidget, &MapWidget::updateTooltip);
  connect(weatherReporter, &WeatherReporter::weatherUpdated, infoController, &InfoController::updateAirportWeather);
  connect(weatherReporter, &WeatherReporter::weatherUpdated, mapWidget, &MapPaintWidget::weatherUpdated);
  connect(weatherReporter, &WeatherReporter::weatherUpdated, procedureSearch, &ProcedureSearch::weatherUpdated);

  connect(connectClient, &ConnectClient::weatherUpdated, mapWidget, &MapPaintWidget::weatherUpdated);
  connect(connectClient, &ConnectClient::weatherUpdated, mapWidget, &MapWidget::updateTooltip);
  connect(connectClient, &ConnectClient::weatherUpdated, infoController, &InfoController::updateAirportWeather);

  // Wind update ===================================================
  connect(windReporter, &WindReporter::windUpdated, routeController, &RouteController::windUpdated);
  connect(windReporter, &WindReporter::windUpdated, profileWidget, &ProfileWidget::windUpdated);
  connect(windReporter, &WindReporter::windUpdated, perfController, &AircraftPerfController::updateReports);

  connect(windReporter, &WindReporter::windDisplayUpdated, this, &MainWindow::updateMapObjectsShown);
  connect(windReporter, &WindReporter::windDisplayUpdated, this, &MainWindow::updateActionStates);
  connect(windReporter, &WindReporter::windDisplayUpdated, mapWidget, &MapWidget::windDisplayUpdated);

  connect(&weatherUpdateTimer, &QTimer::timeout, this, &MainWindow::weatherUpdateTimeout);

  // Webserver
  connect(ui->actionRunWebserver, &QAction::toggled, this, &MainWindow::toggleWebserver);
  connect(ui->actionOpenWebserver, &QAction::triggered, this, &MainWindow::openWebserver);
  connect(NavApp::getWebController(), &WebController::webserverStatusChanged, this, &MainWindow::webserverStatusChanged);

  // Shortcut menu
  connect(ui->actionShortcutMap, &QAction::triggered, this, &MainWindow::actionShortcutMapTriggered);
  connect(ui->actionShortcutProfile, &QAction::triggered, this, &MainWindow::actionShortcutProfileTriggered);
  connect(ui->actionShortcutAirportSearch, &QAction::triggered, this, &MainWindow::actionShortcutAirportSearchTriggered);
  connect(ui->actionShortcutNavaidSearch, &QAction::triggered, this, &MainWindow::actionShortcutNavaidSearchTriggered);
  connect(ui->actionShortcutUserpointSearch, &QAction::triggered, this, &MainWindow::actionShortcutUserpointSearchTriggered);
  connect(ui->actionShortcutLogbookSearch, &QAction::triggered, this, &MainWindow::actionShortcutLogbookSearchTriggered);
  connect(ui->actionShortcutFlightPlan, &QAction::triggered, this, &MainWindow::actionShortcutFlightPlanTriggered);
  connect(ui->actionShortcutRouteCalc, &QAction::triggered, this, &MainWindow::actionShortcutCalcRouteTriggered);
  connect(ui->actionShortcutAircraftPerformance, &QAction::triggered, this, &MainWindow::actionShortcutAircraftPerformanceTriggered);
  connect(ui->actionShortcutAirportInformation, &QAction::triggered, this, &MainWindow::actionShortcutAirportInformationTriggered);
  connect(ui->actionShortcutAirportWeather, &QAction::triggered, this, &MainWindow::actionShortcutAirportWeatherTriggered);
  connect(ui->actionShortcutNavaidInformation, &QAction::triggered, this, &MainWindow::actionShortcutNavaidInformationTriggered);
  connect(ui->actionShortcutAircraftProgress, &QAction::triggered, this, &MainWindow::actionShortcutAircraftProgressTriggered);

  // Check for database file modifications on application activation
  connect(atools::gui::Application::applicationInstance(), &atools::gui::Application::applicationStateChanged,
          databaseManager, &DatabaseManager::checkForChangedNavAndSimDatabases);
}

void MainWindow::actionShortcutMapTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(OptionData::instance().getFlags2() & opts2::MAP_ALLOW_UNDOCK)
  {
    ui->dockWidgetMap->show();
    ui->dockWidgetMap->activateWindow();
    DockWidgetHandler::raiseFloatingDockWidget(ui->dockWidgetMap);
  }
  mapWidget->activateWindow();
  mapWidget->setFocus();
}

void MainWindow::actionShortcutProfileTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetProfile);
  profileWidget->activateWindow();
  profileWidget->setFocus();
}

void MainWindow::actionShortcutAirportSearchTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetSearch);
  searchController->setCurrentSearchTabId(si::SEARCH_AIRPORT);
  ui->lineEditAirportTextSearch->setFocus();
  ui->lineEditAirportTextSearch->selectAll();
}

void MainWindow::actionShortcutNavaidSearchTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetSearch);
  searchController->setCurrentSearchTabId(si::SEARCH_NAV);
  ui->lineEditNavIcaoSearch->setFocus();
  ui->lineEditNavIcaoSearch->selectAll();
}

void MainWindow::actionShortcutUserpointSearchTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetSearch);
  searchController->setCurrentSearchTabId(si::SEARCH_USER);
  ui->lineEditUserdataIdent->setFocus();
  ui->lineEditUserdataIdent->selectAll();
}

void MainWindow::actionShortcutLogbookSearchTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetSearch);
  searchController->setCurrentSearchTabId(si::SEARCH_LOG);
  ui->lineEditLogdataAirport->setFocus();
  ui->lineEditLogdataAirport->selectAll();
}

void MainWindow::actionShortcutFlightPlanTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetRoute);
  NavApp::getRouteTabHandler()->setCurrentTab(rc::ROUTE);
  ui->tableViewRoute->setFocus();
}

void MainWindow::actionShortcutCalcRouteTriggered()
{
  routeController->calculateRouteWindowShow();
}

void MainWindow::actionShortcutAircraftPerformanceTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetRoute);
  NavApp::getRouteTabHandler()->setCurrentTab(rc::AIRCRAFT);
  ui->textBrowserAircraftPerformanceReport->setFocus();
}

void MainWindow::actionShortcutAirportInformationTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetInformation);
  infoController->setCurrentInfoTabIndex(ic::INFO_AIRPORT);
  infoController->setCurrentAirportInfoTabIndex(ic::INFO_AIRPORT_OVERVIEW);
  ui->textBrowserAirportInfo->setFocus();
}

void MainWindow::actionShortcutAirportWeatherTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetInformation);
  infoController->setCurrentInfoTabIndex(ic::INFO_AIRPORT);
  infoController->setCurrentAirportInfoTabIndex(ic::INFO_AIRPORT_WEATHER);
  ui->textBrowserWeatherInfo->setFocus();
}

void MainWindow::actionShortcutNavaidInformationTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetInformation);
  infoController->setCurrentInfoTabIndex(ic::INFO_NAVAID);
  ui->textBrowserNavaidInfo->setFocus();
}

void MainWindow::actionShortcutAircraftProgressTriggered()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetAircraft);
  infoController->setCurrentAircraftTabIndex(ic::AIRCRAFT_USER_PROGRESS);
  ui->textBrowserAircraftProgressInfo->setFocus();
}

/* Update the info weather */
void MainWindow::weatherUpdateTimeout()
{
  // if(connectClient != nullptr && connectClient->isConnected() && infoController != nullptr)
  infoController->updateAirportWeather();
}

/* Menu item */
void MainWindow::showDatabaseFiles()
{
  helpHandler->openFile(NavApp::getDatabaseManager()->getDatabaseDirectory());
}

/* Menu item */
void MainWindow::showShowMapCache()
{
  // Windows: C:\Users\YOURUSERNAME\AppData\Local\.marble\data
  // Linux and macOS: $HOME/.local/share/marble
  helpHandler->openFile(Marble::MarbleDirs::localPath() % atools::SEP % "maps" % atools::SEP % "earth");
}

/* Menu item */
void MainWindow::showMapInstallation()
{
  QString cacheMapThemeDir = OptionData::instance().getCacheMapThemeDir();

  QString msg = atools::checkDirMsg(cacheMapThemeDir);
  if(msg.isEmpty())
    helpHandler->openFile(cacheMapThemeDir);
  else
    QMessageBox::warning(this, QApplication::applicationName(),
                         msg % tr("\n\nSet the path to additional map themes in options on page \"Cache and Files\""));
}

/* Updates label and tooltip for connection status */
void MainWindow::setConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  if(!text.isEmpty())
    connectionStatus = text;
  connectionStatusTooltip = tooltipText;
  updateConnectionStatusMessageText();
}

void MainWindow::setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  onlineConnectionStatus = text;
  onlineConnectionStatusTooltip = tooltipText;
  updateConnectionStatusMessageText();
}

void MainWindow::updateConnectionStatusMessageText()
{
  if(onlineConnectionStatus.isEmpty())
    connectStatusLabel->setText(connectionStatus);
  else
    connectStatusLabel->setText(tr("%1/%2").arg(connectionStatus).arg(onlineConnectionStatus));

  if(onlineConnectionStatusTooltip.isEmpty())
    connectStatusLabel->setToolTip(connectionStatusTooltip);
  else
    connectStatusLabel->setToolTip(tr("Simulator:\n%1\n\nOnline Network:\n%2").
                                   arg(connectionStatusTooltip).arg(onlineConnectionStatusTooltip));
  connectStatusLabel->setMinimumWidth(connectStatusLabel->width());
}

/* Updates label and tooltip for objects shown on map */
void MainWindow::setMapObjectsShownMessageText(const QString& text, const QString& tooltipText)
{
  mapVisibleLabel->setText(text);
  mapVisibleLabel->setToolTip(tooltipText);
  mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
}

const ElevationModel *MainWindow::getElevationModel()
{
  return mapWidget->model()->elevationModel();
}

/* Called after each query */
void MainWindow::resultTruncated()
{
  mapVisibleLabel->setText(atools::util::HtmlBuilder::errorMessage(tr("Too many objects")));
  mapVisibleLabel->setToolTip(tr("Too many objects to show on map.\n"
                                 "Display might be incomplete.\n"
                                 "Reduce map details in the \"View\" menu.",
                                 "Keep menu item in sync with menu translation"));
  mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
}

void MainWindow::distanceChanged()
{
  // #ifdef DEBUG_INFORMATION
  // qDebug() << Q_FUNC_INFO << "minimumZoom" << mapWidget->minimumZoom() << "maximumZoom" << mapWidget->maximumZoom()
  // << "step" << mapWidget->zoomStep() << "distance" << mapWidget->distance() << "zoom" << mapWidget->zoom();
  // #endif
  float dist = Unit::distMeterF(static_cast<float>(mapWidget->distance() * 1000.f));
  QString distStr = QLocale().toString(dist, 'f', dist < 20.f ? (dist < 0.2f ? 2 : 1) : 0);
  if(distStr.endsWith(QString(QLocale().decimalPoint()) % "0"))
    distStr.chop(2);

  QString text = distStr % " " % Unit::getUnitDistStr();

#ifdef DEBUG_INFORMATION
  text += QString("[%1km][%2z]").arg(mapWidget->distance(), 0, 'f', 2).arg(mapWidget->zoom());
#endif

  mapDistanceLabel->setText(text);
  mapDistanceLabel->setMinimumWidth(mapDistanceLabel->width());
}

void MainWindow::renderStatusReset()
{
  // Force reset to complete to avoid forever "Waiting"
  renderStatusUpdateLabel(Marble::Complete, false /* forceUpdate */);
}

void MainWindow::renderStatusUpdateLabel(RenderStatus status, bool forceUpdate)
{
  if(status != lastRenderStatus || forceUpdate)
  {
    switch(status)
    {
      case Marble::Complete:
        mapRenderStatusLabel->setText(tr("Done"));
        break;
      case Marble::WaitingForUpdate:
        mapRenderStatusLabel->setText(tr("Updating"));
        break;
      case Marble::WaitingForData:
        mapRenderStatusLabel->setText(tr("Loading"));
        break;
      case Marble::Incomplete:
        mapRenderStatusLabel->setText(tr("Incomplete"));
        break;
    }
    lastRenderStatus = status;
    mapRenderStatusLabel->setMinimumWidth(mapRenderStatusLabel->width());
  }
}

void MainWindow::renderStatusChanged(const RenderState& state)
{
  RenderStatus status = state.status();
  renderStatusUpdateLabel(status, false /* forceUpdate */);

  if(status == Marble::WaitingForUpdate || status == Marble::WaitingForData)
    // Reset forever lasting waiting status if Marble cannot fetch tiles
    renderStatusTimer.start();
  else if(status == Marble::Complete || status == Marble::Incomplete)
    renderStatusTimer.stop();
}

/* Route center action */
void MainWindow::routeCenter()
{
  if(!NavApp::getRouteConst().isFlightplanEmpty())
  {
    mapWidget->showRect(routeController->getBoundingRect(), false);
    setStatusMessage(tr("Flight plan shown on map."));
  }
}

void MainWindow::shrinkStatusBar()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << statusBar()->geometry() << QCursor::pos();
#endif

  // Do not shrink status bar if cursor is above
  if(!statusBar()->rect().contains(statusBar()->mapFromGlobal(QCursor::pos())))
  {
    mapPositionLabel->clear();
    mapPositionLabel->setText(tr(" — "));
    mapPositionLabel->setMinimumWidth(20);
    mapPositionLabel->resize(20, mapPositionLabel->height());

    mapMagvarLabel->clear();
    mapMagvarLabel->setText(tr(" — "));
    mapMagvarLabel->setMinimumWidth(20);
    mapMagvarLabel->resize(20, mapMagvarLabel->height());
  }
  else
    shrinkStatusBarTimer.start();
}

void MainWindow::updateMapPosLabel(const atools::geo::Pos& pos, int x, int y)
{
  Q_UNUSED(x)
  Q_UNUSED(y)

  if(pos.isValid())
  {
    QString text(Unit::coords(pos));

    if(NavApp::isGlobeOfflineProvider() && pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      text += tr(" / ") % Unit::altMeter(pos.getAltitude());

    float magVar = NavApp::getMagVar(pos);
    QString magVarText = map::magvarText(magVar, true /* short text */);
#ifdef DEBUG_INFORMATION
    magVarText = QString("%1 [%2]").arg(magVarText).arg(magVar, 0, 'f', 2);
#endif

    mapMagvarLabel->setText(magVarText);
    mapMagvarLabel->setMinimumWidth(mapMagvarLabel->width());

#ifdef DEBUG_INFORMATION
    text.append(QString(" [%1,%2]").arg(x).arg(y));
#endif

    mapPositionLabel->setText(text);
    mapPositionLabel->setMinimumWidth(mapPositionLabel->width());

    // Stop status bar time to avoid shrinking
    shrinkStatusBarTimer.stop();
  }
  else
  {
    mapPositionLabel->setText(tr(" — "));
    mapPositionLabel->setMinimumWidth(mapPositionLabel->width());
    mapMagvarLabel->setText(tr(" — "));

    // Reduce status fields bar after timeout
    shrinkStatusBarTimer.start();
  }
}

/* Updates main window title with simulator type, flight plan name and change status */
void MainWindow::updateWindowTitle()
{
  QString newTitle = mainWindowTitle;
  navdb::Status navDbStatus = NavApp::getDatabaseManager()->getNavDatabaseStatus();

  atools::util::Version version(NavApp::applicationVersion());

#if defined(WINARCH64)
  QString applicationVersion = version.getVersionString() % tr(" 64-bit");
#elif defined(WINARCH32)
  QString applicationVersion = version.getVersionString() % tr(" 32-bit");
#else
  QString applicationVersion = version.getVersionString();
#endif

  // Program version and revision ==========================================
  if(version.isStable() || version.isReleaseCandidate() || version.isBeta())
    newTitle += tr(" %1").arg(applicationVersion);
  else
    newTitle += tr(" %1 (%2)").arg(applicationVersion).arg(GIT_REVISION_LITTLENAVMAP);

  // Database information  ==========================================
  // Simulator database =========
  if(navDbStatus == navdb::ALL)
    newTitle += tr(" - (%1)").arg(NavApp::getCurrentSimulatorShortName());
  else
  {
    newTitle += tr(" - %1").arg(NavApp::getCurrentSimulatorShortName());

    if(!NavApp::getDatabaseAiracCycleSim().isEmpty())
      newTitle += tr(" %1").arg(NavApp::getDatabaseAiracCycleSim());
  }

  // Nav database =========
  if(navDbStatus == navdb::ALL)
    newTitle += tr(" / N");
  else if(navDbStatus == navdb::MIXED)
    newTitle += tr(" / N");
  else if(navDbStatus == navdb::OFF)
    newTitle += tr(" / (N)");

  if((navDbStatus == navdb::ALL || navDbStatus == navdb::MIXED) &&
     !NavApp::getDatabaseAiracCycleNav().isEmpty())
    newTitle += tr(" %1").arg(NavApp::getDatabaseAiracCycleNav());

  // Flight plan name  ==========================================
  if(!routeController->getRouteFilepath().isEmpty())
    newTitle += tr(" - %1%2").
                arg(QFileInfo(routeController->getRouteFilepath()).fileName()).
                arg(routeController->hasChanged() ? tr(" *") : QString());
  else if(routeController->hasChanged())
    newTitle += tr(" - *");

  // Performance name  ==========================================
  if(!NavApp::getCurrentAircraftPerfFilepath().isEmpty())
    newTitle += tr(" - %1%2").
                arg(QFileInfo(NavApp::getCurrentAircraftPerfFilepath()).fileName()).
                arg(NavApp::getAircraftPerfController()->hasChanged() ? tr(" *") : QString());
  else if(NavApp::getAircraftPerfController()->hasChanged())
    newTitle += tr(" - *");

  if(!NavApp::getOnlineNetworkTranslated().isEmpty())
    newTitle += tr(" - %1").arg(NavApp::getOnlineNetworkTranslated());

#ifndef QT_NO_DEBUG
  newTitle += " - DEBUG";
#endif

  // Add a star to the flight plan tab if changed
  routeController->updateRouteTabChangedStatus();

  setWindowTitle(newTitle);
}

void MainWindow::setToolTipsEnabledMainMenu(bool enabled)
{
  // NavApp function disabled tooltips in whole menu - enable them again
  if(!enabled)
  {
    if(routeFileHistory != nullptr)
      routeFileHistory->updateMenuTooltips();
    if(kmlFileHistory != nullptr)
      kmlFileHistory->updateMenuTooltips();
    if(layoutFileHistory != nullptr)
      layoutFileHistory->updateMenuTooltips();
    if(NavApp::getAircraftPerfController() != nullptr)
      NavApp::getAircraftPerfController()->updateMenuTooltips();
  }
}

void MainWindow::deleteProfileAircraftTrail()
{
  profileWidget->deleteAircraftTrail();
}

void MainWindow::deleteAircraftTrail(bool quiet)
{
  int result = QMessageBox::Yes;

  if(!quiet)
    result =
      dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_TRAIL, tr("Delete user aircraft trail?"), tr("Do not &show this dialog again."),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    mapWidget->deleteAircraftTrail();
    profileWidget->deleteAircraftTrail();
    updateActionStates();
    setStatusMessage(QString(tr("Aircraft trail removed from map.")));
  }
}

/* Ask user if flight plan can be deleted when quitting.
 * @return true continue with new flight plan, exit, etc. */
bool MainWindow::routeCheckForChanges()
{
  if(!routeController->hasChanged())
    return true;

  QMessageBox msgBox(this);
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText(routeController->getRouteConst().isEmpty() ?
                 tr("Flight Plan has been changed.\n"
                    "There are changes which can be restored by using undo.") :
                 tr("Flight Plan has been changed."));
  msgBox.setInformativeText(tr("Save changes?"));
  msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel);

  int retval = msgBox.exec();

  switch(retval)
  {
    case QMessageBox::Save:
      if(routeController->getRouteFilepath().isEmpty())
        return routeSaveAsLnm();
      else
        return routeSaveLnm();

    case QMessageBox::No:
      // ok to erase flight plan
      return true;

    case QMessageBox::Cancel:
      // stop any flight plan erasing actions
      return false;
  }
  return false;
}

void MainWindow::routeFromStringCurrent()
{
  // Open or raises a dialog that allows to create a new route from a string
  qDebug() << Q_FUNC_INFO;

  // Create dialog on demand the first call
  if(routeStringDialog == nullptr)
  {
    if(!backgroundHintRouteStringShown)
    {
      dialog->showInfoMsgBox(lnm::ROUTE_STRING_DIALOG_BACKGROUND_HINT,
                             tr("Note that you can put the flight plan route description window in the background and "
                                "modify the flight plan on the map or flight plan table in parallel."),
                             tr("Do not &show this dialog again."));
      backgroundHintRouteStringShown = true;
    }

    // No parent to create non-modal dialog
    routeStringDialog = new RouteStringDialog(nullptr, QString());

    // Load last content
    routeStringDialog->restoreState();

    // Add to dock handler to enable auto raise and closing on exit
    NavApp::addDialogToDockHandler(routeStringDialog);

    // Connect signals from and to non-modal dialog
    connect(routeStringDialog, &RouteStringDialog::routeFromFlightplan, this, &MainWindow::routeFromFlightplan);
    connect(routeController, &RouteController::routeChanged, routeStringDialog, &RouteStringDialog::updateButtonState);
    connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, routeStringDialog, &RouteStringDialog::styleChanged);
  }

  routeStringDialog->show();
  routeStringDialog->activateWindow();
  routeStringDialog->raise();
}

void MainWindow::routeFromStringSimBrief(const QString& routeString)
{
  // Create a new modal/blocking dialog which allows to edit the route string
  // Has its own state settings
  qDebug() << Q_FUNC_INFO << routeString;
  RouteStringDialog routeStrDialog(this, "SimBrief" /* settingsSuffixParam */);

  // Load dialog settings but not last content
  routeStrDialog.restoreState();
  routeStrDialog.addRouteDescription(routeString);

  if(routeStrDialog.exec() == QDialog::Accepted)
  {
    if(!routeStrDialog.getFlightplan().isEmpty())
      // Load plan and mark it as changed - do not use undo stack
      routeFromFlightplan(routeStrDialog.getFlightplan(), !routeStrDialog.isAltitudeIncluded() /* adjustAltitude */,
                          true /* changed */, false /* undo */);
    else
      qWarning() << "Flight plan is null";
  }
  routeStrDialog.saveState();
}

void MainWindow::routeFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude, bool changed, bool undo)
{
  qDebug() << Q_FUNC_INFO;

  // Check for changes and show question dialog unless undo stack is used
  if(undo || routeCheckForChanges())
  {
    // Transfer flag to new flightplan to avoid silently overwriting non LNMPLN files with own format
    bool lnmpln = routeController->getRouteConst().getFlightplanConst().isLnmFormat();
    routeController->loadFlightplan(flightplan, atools::fs::pln::LNM_PLN, QString(),
                                    changed, adjustAltitude, undo, false /* warnAltitude */);
    routeController->getRoute().getFlightplan().setLnmFormat(lnmpln);
    if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
      routeCenter();
    showFlightPlan();
    setStatusMessage(tr("Created new flight plan."));
  }
}

/* Called from menu or toolbar by action */
void MainWindow::routeNew()
{
  if(routeCheckForChanges())
  {
    routeController->newFlightplan();
    mapWidget->update();
    showFlightPlan();
    setStatusMessage(tr("Created new empty flight plan."));
  }
}

/* called from AirportSearch (random flight plan generator) */
void MainWindow::routeNewFromAirports(map::MapAirport departure, map::MapAirport destination)
{
  if(routeCheckForChanges())
  {
    routeController->newFlightplan();
    routeController->routeSetDeparture(departure);
    routeController->routeSetDestination(destination);
    mapWidget->update();
    showFlightPlan();
    routeCenter();
    setStatusMessage(tr("Created new flight plan with departure and destination airport."));
  }
}

/* Called from menu or toolbar by action */
void MainWindow::routeOpen()
{
  routeOpenFile(QString());
}

QString MainWindow::routeOpenFileDialog()
{
  return dialog->openFileDialog(tr("Open Flight Plan"), tr("Flight Plan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN_LOAD),
                                "Route/LnmPln", atools::documentsDir());
}

void MainWindow::routeOpenDescr(const QString& routeString)
{
  if(routeCheckForChanges())
  {
    routeController->loadFlightplanRouteStr(routeString);

    if(OptionData::instance().getFlags().testFlag(opts::GUI_CENTER_ROUTE))
      routeCenter();
    showFlightPlan();
    setStatusMessage(tr("Flight plan opened from route description."));
  }
}

void MainWindow::routeOpenFile(QString filepath)
{
  qDebug() << Q_FUNC_INFO << filepath;
  if(routeCheckForChanges())
  {
    if(filepath.isEmpty())
      filepath = routeOpenFileDialog();

    if(!filepath.isEmpty())
    {
      if(routeController->loadFlightplan(filepath))
      {
        routeFileHistory->addFile(filepath);
        if(OptionData::instance().getFlags().testFlag(opts::GUI_CENTER_ROUTE))
          routeCenter();
        showFlightPlan();
        setStatusMessage(tr("Flight plan opened."));
      }
    }
  }
  saveFileHistoryStates();
}

void MainWindow::routeOpenFileLnmStr(const QString& string)
{
  if(routeCheckForChanges())
  {
    if(routeController->loadFlightplanLnmStr(string))
    {
      if(OptionData::instance().getFlags().testFlag(opts::GUI_CENTER_ROUTE))
        routeCenter();
      showFlightPlan();
      setStatusMessage(tr("Flight plan opened."));
    }
  }
}

void MainWindow::trailLoadGpx()
{
  QString file = dialog->openFileDialog(tr("Open and Replace GPX Trail"), tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
                                        "Route/Gpx", atools::documentsDir());
  trailLoadGpxFile(file);
}

void MainWindow::trailLoadGpxFile(const QString& file)
{
  qDebug() << Q_FUNC_INFO << file;

  if(!file.isEmpty())
  {
    if(atools::fs::gpx::GpxIO::isGpxFile(file))
    {
      bool replace = true;

      if(mapWidget->getAircraftTrailSize() > 0)
        replace = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_REPLACE_TRAIL,
                                             tr("Replace the current user aircraft trail?"),
                                             tr("Do not &show this dialog again and replace trail."),
                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;

      if(replace)
        mapWidget->loadAircraftTrail(file);
    }
    else
      QMessageBox::warning(this, QApplication::applicationName(), tr("The file \"%1\" is no valid GPX file.").arg(file));
  }
}

void MainWindow::trailAppendGpx()
{
  QString file = dialog->openFileDialog(tr("Open and Append GPX Trail"), tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
                                        "Route/Gpx", atools::documentsDir());

  qDebug() << Q_FUNC_INFO << file;

  if(!file.isEmpty())
  {
    if(atools::fs::gpx::GpxIO::isGpxFile(file))
      mapWidget->appendAircraftTrail(file);
    else
      QMessageBox::warning(this, QApplication::applicationName(), tr("The file \"%1\" is no valid GPX file.").arg(file));
  }
}

bool MainWindow::routeSaveSelection()
{
  // Have to get a simple copy of the plan to be able to extract new name
  const Route routeForSelection = routeController->getRouteForSelection();

  QString routeFile = routeSaveFileDialogLnm(routeForSelection.buildDefaultFilename(".lnmpln"));

  if(!routeFile.isEmpty())
  {
    if(routeController->saveFlightplanLnmAsSelection(routeFile))
    {
      routeFileHistory->addFile(routeFile);
      updateActionStates();
      setStatusMessage(tr("Flight plan saved."));
      saveFileHistoryStates();
      return true;
    }
  }
  return false;
}

/* Called from menu or toolbar by action - append flight plan to current one */
void MainWindow::routeAppend()
{
  QString routeFile = dialog->openFileDialog(tr("Append Flight Plan"),
                                             tr("Flight Plan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN_LOAD),
                                             "Route/LnmPln", atools::documentsDir());

  if(!routeFile.isEmpty())
  {
    if(routeController->insertFlightplan(routeFile,
                                         routeController->getRouteConst().getDestinationAirportLegIndex() + 1 /* append */))
    {
      routeFileHistory->addFile(routeFile);
      if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
        routeCenter();
      showFlightPlan();
      setStatusMessage(tr("Flight plan appended."));
    }
  }
  saveFileHistoryStates();
}

/* Called by route controller - insert flight plan into current one */
void MainWindow::routeInsert(int insertBefore)
{
  QString routeFile = dialog->openFileDialog(
    tr("Insert info Flight Plan"),
    tr("Flight Plan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN_LOAD),
    "Route/LnmPln", atools::documentsDir());

  if(!routeFile.isEmpty())
  {
    if(routeController->insertFlightplan(routeFile, insertBefore))
    {
      routeFileHistory->addFile(routeFile);
      if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
        routeCenter();
      setStatusMessage(tr("Flight plan inserted."));
    }
  }
  saveFileHistoryStates();
}

/* Called from menu or toolbar by action */
void MainWindow::routeOpenRecent(const QString& routeFile)
{
  if(routeCheckForChanges())
  {
    if(QFile::exists(routeFile))
    {
      if(routeController->loadFlightplan(routeFile))
      {
        if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
          routeCenter();
        showFlightPlan();
        setStatusMessage(tr("Flight plan opened."));
      }
    }
    else
    {
      NavApp::closeSplashScreen();

      // File not valid remove from history
      atools::gui::Dialog::warning(this, tr("File \"%1\" does not exist").arg(routeFile));
      routeFileHistory->removeFile(routeFile);
    }
  }
  saveFileHistoryStates();
}

void MainWindow::routeSaveLnmExported(const QString& filename)
{
  // Update filename and undo status
  routeController->saveFlightplanLnmExported(filename);

  // Add to recent files
  routeFileHistory->addFile(filename);

  updateActionStates();
  saveFileHistoryStates();
}

/* Called from menu or toolbar by action */
bool MainWindow::routeSaveLnm()
{
  // Show a simple warning for the rare case that altitude is null ===============
  if(atools::almostEqual(NavApp::getRouteConst().getCruiseAltitudeFt(), 0.f, 10.f))
    dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_CRUISE_ZERO_WARNING,
                           tr("Flight plan cruise altitude is zero.\n"
                              "A simulator might not be able to load the flight plan."),
                           tr("Do not &show this dialog again."));

  if(!routeController->isLnmFormatFlightplan())
  {
    // Forbid saving of other formats than LNMPLN directly =========================================
    atools::gui::DialogButtonList buttonList =
    {
      atools::gui::DialogButton(QString(), QMessageBox::Cancel),
      atools::gui::DialogButton(tr("Save &as LNMPLN ..."), QMessageBox::Save),
      atools::gui::DialogButton(QString(), QMessageBox::Help)
    };

    // Ask before saving file
    int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_SAVE_WARNING,
                                            tr("<p>You cannot save this file directly.<br/>"
                                               "Use the export function instead.</p>"
                                               "<p>Save using the LNMPLN format now?</p>"),
                                            tr("Do not &show this dialog again and save as LNMPLN."),
                                            buttonList, QMessageBox::Cancel, QMessageBox::Save);

    if(result == QMessageBox::Cancel)
      return false;
    else if(result == QMessageBox::Help)
    {
      atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl % "FLIGHTPLANFMT.html", lnm::helpLanguageOnline());
      return false;
    }
  }

  if(routeController->getRouteFilepath().isEmpty() || !routeController->doesLnmFilenameMatchRoute() ||
     !routeController->isLnmFormatFlightplan())
    // No filename or plan has changed - save as ================================
    return routeSaveAsLnm();
  else
  {
    // Save as LNMPLN =====================================================
    if(routeController->saveFlightplanLnm())
    {
      routeFileHistory->addFile(routeController->getRouteFilepath());
      updateActionStates();
      setStatusMessage(tr("Flight plan saved."));
      saveFileHistoryStates();
      return true;
    }
  }
  return false;
}

QString MainWindow::routeSaveFileDialogLnm(const QString& filename)
{
  return dialog->saveFileDialog(
    tr("Save Flight Plan as LNMPLN Format"),
    tr("Flight Plan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_LNMPLN), "lnmpln", "Route/LnmPln", atools::documentsDir(),
    filename.isEmpty() ? NavApp::getRouteConst().buildDefaultFilename(".lnmpln") : filename, false /* confirm overwrite */);
}

bool MainWindow::routeSaveAsLnm()
{
  QString routeFile = routeSaveFileDialogLnm();

  if(!routeFile.isEmpty())
  {
    if(routeController->saveFlightplanLnmAs(routeFile))
    {
      routeFileHistory->addFile(routeFile);
      updateActionStates();
      setStatusMessage(tr("Flight plan saved."));
      saveFileHistoryStates();
      return true;
    }
  }
  return false;
}

bool MainWindow::openInSkyVector()
{
  // https://skyvector.com/?fpl=%20EDDH%20AMLU7C%20AMLUH%20M852%20POVEL%20GALMA%20UM736%20DOSEL%20DETSA%20NIKMA%20T369%20RITEB%20RITE4B%20LIRF

  QString route = RouteStringWriter().createStringForRoute(NavApp::getRouteConst(), NavApp::getRouteCruiseSpeedKts(),
                                                           rs::START_AND_DEST | rs::SKYVECTOR_COORDS);

  HelpHandler::openUrlWeb(this, "https://skyvector.com/?fpl=" % route);
  return true;
}

/* Called from menu or toolbar by action. Remove all KML from map */
void MainWindow::kmlClear()
{
  mapWidget->clearKmlFiles();
  updateActionStates();
  setStatusMessage(tr("Google Earth KML files removed from map."));
}

/* Called from menu or toolbar by action */
void MainWindow::kmlOpen()
{
  QString kmlFile = dialog->openFileDialog(
    tr("Google Earth KML"),
    tr("Google Earth KML %1;;All Files (*)").arg(lnm::FILE_PATTERN_KML),
    "Kml/", QString());

  QString errors = atools::checkFileMsg(kmlFile);
  if(errors.isEmpty())
  {
    if(mapWidget->addKmlFile(kmlFile))
    {
      kmlFileHistory->addFile(kmlFile);
      updateActionStates();
      setStatusMessage(tr("Google Earth KML file opened."));
    }
    else
      setStatusMessage(tr("Opening Google Earth KML file failed."));

  }
  else
    QMessageBox::warning(this, QApplication::applicationName(),
                         tr("Cannot load file. Reason:\n\n%1").arg(kmlFile));
}

/* Called from menu or toolbar by action */
void MainWindow::kmlOpenRecent(const QString& kmlFile)
{
  QString errors = atools::checkFileMsg(kmlFile);
  if(errors.isEmpty())
  {
    if(mapWidget->addKmlFile(kmlFile))
    {
      updateActionStates();
      setStatusMessage(tr("Google Earth KML file opened."));
    }
    else
    {
      kmlFileHistory->removeFile(kmlFile);
      setStatusMessage(tr("Opening Google Earth KML file failed."));
    }
  }
  else
    QMessageBox::warning(this, QApplication::applicationName(),
                         tr("Cannot load file. Reason:\n\n%1").arg(kmlFile));
}

void MainWindow::layoutOpen()
{
  QString layoutFile = dialog->openFileDialog(tr("Window Layout"),
                                              tr("Window Layout Files %1;;All Files (*)").
                                              arg(lnm::FILE_PATTERN_LAYOUT), "WindowLayout/", QString());

  if(!layoutFile.isEmpty())
  {
    if(layoutOpenInternal(layoutFile))
      layoutFileHistory->addFile(layoutFile);
  }
}

void MainWindow::layoutSaveAs()
{

  QString layoutFile = dialog->saveFileDialog(
    tr("Window Layout"),
    tr("Window Layout Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_LAYOUT),
    "lnmlayout", "WindowLayout/", atools::documentsDir(), QString(), false /* confirm overwrite */);

  if(!layoutFile.isEmpty())
  {
    try
    {
      dockHandler->saveWindowState(layoutFile, OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK));
      layoutFileHistory->addFile(layoutFile);
      setStatusMessage(tr("Window layout saved."));
    }
    catch(atools::Exception& e)
    {
      NavApp::closeSplashScreen();
      atools::gui::ErrorHandler(this).handleException(e);
    }
  }
}

/* Called from menu or toolbar by action */
void MainWindow::layoutOpenRecent(const QString& layoutFile)
{
  if(!layoutOpenInternal(layoutFile))
    layoutFileHistory->removeFile(layoutFile);
}

void MainWindow::layoutOpenDrag(const QString& layoutFile)
{
  if(layoutOpenInternal(layoutFile))
    layoutFileHistory->addFile(layoutFile);
  else
    layoutFileHistory->removeFile(layoutFile);
}

bool MainWindow::layoutOpenInternal(const QString& layoutFile)
{
  try
  {
    if(dockHandler->loadWindowState(layoutFile, OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK),
                                    layoutWarnText))
    {
      dockHandler->currentStateToWindow();

      QTimer::singleShot(200, dockHandler, &atools::gui::DockWidgetHandler::currentStateToWindow);

      setStatusMessage(tr("Window layout loaded and restored."));

      ui->actionShowStatusbar->blockSignals(true);
      ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
      ui->actionShowStatusbar->blockSignals(false);

      return true;
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(this).handleException(e);
  }
  return false;
}

bool MainWindow::createMapImage(QPixmap& pixmap, const QString& dialogTitle, const QString& optionPrefx, QString *json)
{
  ImageExportDialog exportDialog(this, dialogTitle, optionPrefx, mapWidget->width(), mapWidget->height());
  int retval = exportDialog.exec();
  if(retval == QDialog::Accepted)
  {
    if(exportDialog.isCurrentView())
    {
      // Copy image as is from current view
      mapWidget->showOverlays(false, false /* show scale */);
      pixmap = mapWidget->mapScreenShot();
      mapWidget->showOverlays(true, false /* show scale */);

      if(json != nullptr)
        *json = mapWidget->createAvitabJson();
    }
    else
    {
      // Create a map widget clone with the desired resolution
      MapPaintWidget paintWidget(this, false /* no real widget - hidden */);
      paintWidget.setActive(); // Activate painting
      paintWidget.setKeepWorldRect(); // Center world rectangle when resizing

      paintWidget.setAvoidBlurredMap(exportDialog.isAvoidBlurredMap());
      paintWidget.setAdjustOnResize(exportDialog.isAvoidBlurredMap());

      // Copy all map settings
      paintWidget.copySettings(*mapWidget);

      // Copy visible rectangle
      paintWidget.copyView(*mapWidget);
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);

      // Prepare drawing by painting a dummy image
      paintWidget.prepareDraw(exportDialog.getSize().width(), exportDialog.getSize().height());
      QGuiApplication::restoreOverrideCursor();

      // Create a progress dialog
      int numSeconds = 60;
      QString label = tr("Waiting up to %1 seconds for map download ...\n");
      QProgressDialog progress(label.arg(numSeconds), tr("&Ignore Downloads and Continue"), 0, numSeconds, this);
      progress.setWindowModality(Qt::WindowModal);
      progress.setMinimumDuration(0);
      progress.show();
      QApplication::processEvents();

      // Get download job information and update progress text
      int queuedJobs = -1, activeJobs = -1;
      connect(paintWidget.model()->downloadManager(), &HttpDownloadManager::progressChanged, this,
              [&progress, &queuedJobs, &activeJobs, &numSeconds, &label](int active, int queued) -> void
      {
        progress.setLabelText(label.arg(numSeconds) % tr("%1 downloads active and %2 downloads queued.").
                              arg(active).arg(queued));
        queuedJobs = queued;
        activeJobs = active;
      });

      // Loop until seconds are over
      for(int i = 0; i < numSeconds; i++)
      {
        progress.setValue(i);
        QApplication::processEvents();

        if(progress.wasCanceled() || paintWidget.renderStatus() == Marble::Complete ||
           (queuedJobs == 0 && activeJobs == 0))
          break;

        QThread::sleep(1);
      }
      progress.setValue(numSeconds);

      // Now draw the actual image including navaids
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      pixmap = paintWidget.getPixmap(exportDialog.getSize());
      QGuiApplication::restoreOverrideCursor();

      if(json != nullptr)
        // Create Avitab reference if needed
        *json = paintWidget.createAvitabJson();
    }
    PrintSupport::drawWatermark(QPoint(0, pixmap.height()), &pixmap);
    return true;
  }
  return false;
}

void MainWindow::mapSaveImage()
{
  QPixmap pixmap;
  if(createMapImage(pixmap, tr(" - Save Map as Image"), lnm::IMAGE_EXPORT_DIALOG))
  {
    int filterIndex = -1;

    QString imageFile = dialog->saveFileDialog(
      tr("Save Map as Image"),
      tr("JPG Image Files (*.jpg *.jpeg);;PNG Image Files (*.png);;BMP Image Files (*.bmp);;All Files (*)"),
      "jpg", "MainWindow/",
      atools::fs::FsPaths::getFilesPath(NavApp::getCurrentSimulatorDb()), tr("Little Navmap Map %1.jpg").
      arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")),
      false /* confirm overwrite */,
      false /* autoNumber */,
      &filterIndex);

    if(!imageFile.isEmpty())
    {
      const char *format = nullptr;
      if(!imageFile.endsWith("jpg", Qt::CaseInsensitive) && !imageFile.endsWith("jpeg", Qt::CaseInsensitive) &&
         !imageFile.endsWith("png", Qt::CaseInsensitive) && !imageFile.endsWith("bmp", Qt::CaseInsensitive))
      {
        // Did not give file extension - check by looking at selected filter
        if(filterIndex == 0)
          format = "jpg";
        else if(filterIndex == 1)
          format = "png";
        else if(filterIndex == 2)
          format = "bmp";
      }

      if(!pixmap.save(imageFile, format, 95))
        atools::gui::Dialog::warning(this, tr("Error saving image.\n" "Only JPG, PNG and BMP are allowed."));
      else
        setStatusMessage(tr("Map image saved."));
    }
  }
}

void MainWindow::mapSaveImageAviTab()
{
  if(mapWidget->projection() == Marble::Mercator)
  {
    QPixmap pixmap;
    QString json;
    if(createMapImage(pixmap, tr(" - Save Map as Image for AviTab"), lnm::IMAGE_EXPORT_AVITAB_DIALOG, &json))
    {
      if(!json.isEmpty())
      {
        QString defaultFileName;

        if(routeController->getRouteConst().isEmpty())
          defaultFileName = tr("LittleNavmap_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmm"));
        else
          defaultFileName = NavApp::getRouteConst().buildDefaultFilenameShort("_", ".jpg");

        int filterIndex = -1;

        QString imageFile = dialog->saveFileDialog(
          tr("Save Map as Image for AviTab"),
          tr("JPG Image Files (*.jpg *.jpeg);;PNG Image Files (*.png);;All Files (*)"),
          "jpg", "MainWindow/AviTab",
          NavApp::getCurrentSimulatorBasePath() %
          atools::SEP % "Resources" % atools::SEP % "plugins" % atools::SEP % "AviTab" %
          atools::SEP % "MapTiles" % atools::SEP % "Mercator", defaultFileName,
          false /* confirm overwrite */,
          false /* autoNumber */,
          &filterIndex);

        if(!imageFile.isEmpty())
        {
          const char *format = nullptr;
          if(!imageFile.endsWith("jpg", Qt::CaseInsensitive) && !imageFile.endsWith("jpeg", Qt::CaseInsensitive) &&
             !imageFile.endsWith("png", Qt::CaseInsensitive))
          {
            // Did not give file extension - check by looking at selected filter
            if(filterIndex == 0)
              format = "jpg";
            else if(filterIndex == 1)
              format = "png";
          }

          if(!pixmap.save(imageFile, format, 95))
            atools::gui::Dialog::warning(this, tr("Error saving image.\n" "Only JPG and PNG are allowed."));
          else
          {
            QFile jsonFile(imageFile % ".json");
            if(jsonFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
              QTextStream stream(&jsonFile);
              stream << json.toUtf8();

              setStatusMessage(tr("Map image saved."));
            }
            else
              atools::gui::ErrorHandler(this).handleIOError(jsonFile, tr("Error saving JSON."));
          }
        }
      }
      else
        QMessageBox::warning(this, QApplication::applicationName(),
                             tr("Map does not cover window.\n"
                                "Ensure that the map fills the window completely."));
    }
  }
  else
    QMessageBox::warning(this, QApplication::applicationName(),
                         tr("You have to switch to the Mercator map projection before saving the image."));
}

void MainWindow::mapCopyToClipboard()
{
  QPixmap pixmap;
  if(createMapImage(pixmap, tr(" - Copy Map Image to Clipboard"), lnm::IMAGE_EXPORT_DIALOG))
  {
    // Copy formatted and plain text to clipboard
    QMimeData *data = new QMimeData;
    data->setImageData(pixmap);
    QGuiApplication::clipboard()->setMimeData(data);
    setStatusMessage(tr("Map image copied to clipboard."));
  }
}

void MainWindow::sunShadingTimeChanged()
{
  qDebug() << Q_FUNC_INFO;

  if(ui->actionMapShowSunShadingRealTime->isChecked() || ui->actionMapShowSunShadingUserTime->isChecked())
    // User time defaults to current time until set
    mapWidget->setSunShadingDateTime(QDateTime::currentDateTimeUtc());
  else if(ui->actionMapShowSunShadingSimulatorTime->isChecked())
  {
    if(!NavApp::isConnectedAndAircraft())
      // Use current time if not connected
      mapWidget->setSunShadingDateTime(QDateTime::currentDateTimeUtc());
    // else  Updated by simDataChanged
  }

  mapWidget->updateSunShadingOption();
}

void MainWindow::sunShadingTimeSet()
{
  qDebug() << Q_FUNC_INFO;

  // Initalize dialog with current time
  TimeDialog timeDialog(this, mapWidget->getSunShadingDateTime());
  timeDialog.exec();
}

/* Selection in flight plan table has changed */
void MainWindow::routeSelectionChanged(int selected, int total)
{
  Q_UNUSED(selected)
  Q_UNUSED(total)
  QList<int> result;
  routeController->getSelectedRouteLegs(result);
  mapWidget->changeRouteHighlights(result);
  updateHighlightActionStates();
}

void MainWindow::procedureSelected(const proc::MapProcedureRef& ref)
{
  proceduresSelectedInternal({ref}, false /* previewAll */);
}

void MainWindow::proceduresSelected(const QVector<proc::MapProcedureRef>& refs)
{
  proceduresSelectedInternal(refs, true /* previewAll */);
}

void MainWindow::proceduresSelectedInternal(const QVector<proc::MapProcedureRef>& refs, bool previewAll)
{
  QVector<proc::MapProcedureLegs> procedures;
  ProcedureQuery *procedureQuery = NavApp::getProcedureQuery();

  if(previewAll)
    // Loading might take longer for some airports
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  for(const proc::MapProcedureRef& ref: refs)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "approachId" << ref.procedureId << "transitionId" << ref.transitionId << "legId" << ref.legId;
#endif

    map::MapAirport airport = NavApp::getAirportQueryNav()->getAirportById(ref.airportId);

    if(refs.isEmpty())
    {
      if(previewAll)
        // Mulit preview for all procedures of an airport
        mapWidget->changeProcedureHighlights(QVector<proc::MapProcedureLegs>());
      else
        // Single procedure selection by user
        mapWidget->changeProcedureHighlight(proc::MapProcedureLegs());
    }
    else
    {
      if(ref.hasProcedureAndTransitionIds())
      {
        // Load transition including approach
        const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(airport, ref.transitionId);
        if(legs != nullptr)
          procedures.append(*legs);
        else
          qWarning() << "Transition not found" << ref.transitionId;
      }
      else if(ref.hasProcedureOnlyIds())
      {
        proc::MapProcedureRef curRef;
        if(!previewAll)
          // Only one procedure in preview - try to keep transition if user moved selection to approach leg
          curRef = mapWidget->getProcedureHighlight().ref;

        if(ref.airportId == curRef.airportId && ref.procedureId == curRef.procedureId &&
           !ref.hasTransitionId() && curRef.hasTransitionId() && ref.isLeg())
        {
          // Approach leg selected - keep preview of current transition
          proc::MapProcedureRef r = ref;
          r.transitionId = curRef.transitionId;
          const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(airport, r.transitionId);
          if(legs != nullptr)
            procedures.append(*legs);
          else
            qWarning() << "Transition not found" << r.transitionId;
        }
        else
        {
          const proc::MapProcedureLegs *legs = procedureQuery->getProcedureLegs(airport, ref.procedureId);
          if(legs != nullptr)
            procedures.append(*legs);
          else
            qWarning() << "Approach not found" << ref.procedureId;
        }
      }
    }
  }

  if(!procedures.isEmpty())
  {
    if(!previewAll)
      // Use  default color
      procedures[0].previewColor = mapcolors::highlightProcedureColor;
    else
    {
      // Use color palette for multi preview
      for(int i = 0; i < procedures.size(); i++)
        procedures[i].previewColor = mapcolors::highlightProcedureColorTable[i % mapcolors::highlightProcedureColorTable.size()];
    }
  }

  if(previewAll)
    mapWidget->changeProcedureHighlights(procedures);
  else
    mapWidget->changeProcedureHighlight(procedures.value(0));

  if(previewAll)
    QGuiApplication::restoreOverrideCursor();

  updateHighlightActionStates();
}

void MainWindow::procedureLegSelected(const proc::MapProcedureRef& ref)
{
  ProcedureQuery *procedureQuery = NavApp::getProcedureQuery();
  const proc::MapProcedureLeg *leg = nullptr;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "approachId" << ref.procedureId << "transitionId" << ref.transitionId << "legId" << ref.legId;
#endif

  if(ref.legId != -1)
  {
    map::MapAirport airport = NavApp::getAirportQueryNav()->getAirportById(ref.airportId);
    if(ref.transitionId != -1)
      leg = procedureQuery->getTransitionLeg(airport, ref.legId);
    else
      leg = procedureQuery->getProcedureLeg(airport, ref.procedureId, ref.legId);
  }

  if(leg != nullptr)
    mapWidget->changeProcedureLegHighlight(*leg);
  else
    mapWidget->changeProcedureLegHighlight(proc::MapProcedureLeg());

  updateHighlightActionStates();
}

void MainWindow::updateAirspaceTypes(const map::MapAirspaceFilter& filter)
{
  mapWidget->setShowMapAirspaces(filter);
  mapWidget->updateMapObjectsShown();
}

void MainWindow::updateAirspaceSources()
{
  mapWidget->setShowMapAirspaces(mapWidget->getShownAirspaces());
  mapWidget->updateMapObjectsShown();
}

void MainWindow::resetMapObjectsShown()
{
  qDebug() << Q_FUNC_INFO;

  mapWidget->resetSettingActionsToDefault();
  mapWidget->resetSettingsToDefault();
  NavApp::getUserdataController()->resetSettingsToDefault();

  NavApp::getAirspaceController()->resetSettingsToDefault();
  NavApp::getWindReporter()->resetSettingsToDefault();
  NavApp::getMapMarkHandler()->resetSettingsToDefault();
  NavApp::getMapAirportHandler()->resetSettingsToDefault();
  NavApp::getMapDetailHandler()->defaultMapDetail();

  mapWidget->updateMapObjectsShown();

  mapWidget->update();
  profileWidget->update();

  setStatusMessage(tr("Map settings reset."));
}

/* A button like airport, vor, ndb, etc. was pressed - update the map */
void MainWindow::updateMapObjectsShown()
{
  mapWidget->updateMapObjectsShown();
  profileWidget->update();
  updateActionStates();
}

/* Map history has changed */
void MainWindow::updateMapHistoryActions(int minIndex, int curIndex, int maxIndex)
{
  ui->actionMapBack->setEnabled(curIndex > minIndex);
  ui->actionMapNext->setEnabled(curIndex < maxIndex);
}

void MainWindow::openOptionsDialog()
{
#if defined(Q_OS_MACOS)
  if(QApplication::activeModalWidget() != nullptr)
  {
    QMessageBox::warning(this, QApplication::applicationName(),
                         tr("Close other dialog boxes before opening preferences"));
    return;
  }
#endif
  NavApp::setStayOnTop(optionsDialog);
  optionsDialog->open();
}

void MainWindow::addDialogToDockHandler(QDialog *dialogWidget)
{
  dockHandler->addDialogWidget(dialogWidget);
}

void MainWindow::removeDialogFromDockHandler(QDialog *dialogWidget)
{
  dockHandler->removeDialogWidget(dialogWidget);
}

void MainWindow::resetMessages()
{
  messages::resetAllMessages();
  setStatusMessage(tr("All message dialogs reset."));
}

/* Set a general status message */
void MainWindow::setStatusMessage(const QString& message, bool addToLog)
{
  if(addToLog && !message.isEmpty())
  {
    statusMessages.append({QDateTime::currentDateTime(), message});

    bool removed = false;
    while(statusMessages.size() > MAX_STATUS_MESSAGES)
    {
      statusMessages.removeFirst();
      removed = true;
    }

    QStringList msg(tr("<p style='white-space:pre'><b>Messages:</b>"));

    if(removed)
      // Indicate overflow
      msg.append(tr("..."));

    for(int i = 0; i < statusMessages.size(); i++)
      msg.append(tr("%1: %2").arg(QLocale().toString(statusMessages.at(i).timestamp.time(), tr("hh:mm:ss"))).
                 arg(statusMessages.at(i).message));
    ui->statusBar->setToolTip(msg.join(tr("<br/>")) % tr("</p>"));
  }

  ui->statusBar->showMessage(message);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "Message" << message;
#endif
}

void MainWindow::statusMessageChanged(const QString& text)
{
  if(text.isEmpty())
  {
    // Field is cleared. Show number of messages in otherwise empty field.
    if(statusMessages.isEmpty())
      ui->statusBar->showMessage(tr("No Messages"));
    else
      ui->statusBar->showMessage(tr("%1 %2").
                                 arg(statusMessages.size()).
                                 arg(statusMessages.size() > 1 ? tr("Messages") : tr("Message")));
  }
}

void MainWindow::setDetailLabelText(const QString& text)
{
  mapDetailLabel->setText(text);
  mapDetailLabel->setMinimumWidth(mapDetailLabel->width());
}

/* Called by window shown event when the main window is visible the first time */
void MainWindow::mainWindowShown()
{
  qDebug() << Q_FUNC_INFO << "enter";

  // This shows a warning dialog if failing - start it later within the event loop to avoid a freeze
  QTimer::singleShot(0, this, &NavApp::showElevationProviderErrors);

  // Show a warning if map theme folders do not exist
  QTimer::singleShot(0, this, &MapThemeHandler::validateMapThemeDirectories);

  // Need to set the font again to pass it on to all menus since these are opened later
  qDebug() << Q_FUNC_INFO << "QApplication::font()" << QApplication::font();
  QApplication::setFont(QApplication::font());

  // Set empty to disable arbitrary messages from map view changes
  setStatusMessage(QString());

  // Enable dock handler
  dockHandler->setHandleDockViews(true);

  qDebug() << Q_FUNC_INFO << "UI font" << font();

  // Postpone loading of KML etc. until now when everything is set up
  mapWidget->mainWindowShown();
  profileWidget->mainWindowShown();

  mapWidget->showSavedPosOnStartup();

  // Show a warning if SSL was not intiaized properly. Can happen if the redist packages are not installed.
  if(!QSslSocket::supportsSsl())
  {
    NavApp::closeSplashScreen();

    QString message = tr("<p>Error initializing SSL subsystem.</p>"
                           "<p>The program will not be able to use encrypted network connections<br/>"
                           "(i.e. HTTPS) that are needed to check for updates or<br/>"
                           "to load online maps.</p>");

#if defined(Q_OS_WIN32)
    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineInstallRedistUrl, lnm::helpLanguageOnline());
    QString message2 = tr("<p><a href=\"%1\">Click here for more information in the Little Navmap online manual</a></p>").
                       arg(url.toString());
    message += message2;
#endif

    dialog->showWarnMsgBox(lnm::ACTIONS_SHOW_SSL_FAILED, message, tr("Do not &show this dialog again."));
  }

  NavApp::logDatabaseMeta();

  // If enabled connect to simulator without showing dialog
  NavApp::getConnectClient()->tryConnectOnStartup();

  // Start weather downloads
  weatherUpdateTimeout();

  // Update the weather every 15 seconds if connected
  weatherUpdateTimer.setInterval(Settings::instance().getAndStoreValue(lnm::OPTIONS_WEATHER_UPDATE_RATE_SIM, 15000).toInt());
  weatherUpdateTimer.start();

  optionsDialog->checkOfficialOnlineUrls();

  // Start regular download of online network files
  NavApp::getOnlinedataController()->startProcessing();

  // Start webserver
  if(ui->actionRunWebserver->isChecked())
  {
    NavApp::getWebController()->startServer();
    updateMapKeys(); // Update API keys and theme dir in web map widget
  }
  webserverStatusChanged(NavApp::getWebController()->isRunning());

  renderStatusUpdateLabel(Marble::Complete, true /* forceUpdate */);

  // Do delayed dock window formatting and fullscreen state after widget layout is done
  QTimer::singleShot(100, this, &MainWindow::mainWindowShownDelayed);

  if(ui->actionRouteDownloadTracks->isChecked())
    QTimer::singleShot(1000, NavApp::getTrackController(), &TrackController::startDownloadStartup);

  // Log screen information ==============
  const QList<QScreen *> screens = QGuiApplication::screens();
  for(QScreen *screen : screens)
    qDebug() << Q_FUNC_INFO
             << (screen == QGuiApplication::primaryScreen() ? "Primary Screen" : "Screen")
             << "name" << screen->name() << "model" << screen->model() << "manufacturer" << screen->manufacturer()
             << "geo" << screen->geometry() << "available geo" << screen->availableGeometry()
             << "available virtual geo" << screen->availableVirtualGeometry();

  // Check for commands from other instances in shared memory segment
  NavApp::getDataExchange()->startTimer();

  qDebug() << Q_FUNC_INFO << "leave";
}

void MainWindow::loadLayoutDelayed(const QString& filename)
{
  try
  {
    // Load layout file delayed - does not apply state
    dockHandler->loadWindowState(filename, OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK), layoutWarnText);
    QTimer::singleShot(200, dockHandler, &atools::gui::DockWidgetHandler::currentStateToWindow);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(this).handleException(e);
  }
}

void MainWindow::mainWindowShownDelayed()
{
  qDebug() << Q_FUNC_INFO << "enter";

  NavApp::closeSplashScreen();

  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_LAYOUT) && !layoutFileHistory->isEmpty() && !NavApp::isSafeMode())
    loadLayoutDelayed(layoutFileHistory->getTopFile());
  // else layout was already loaded from settings earlier

  // Apply layout again to avoid issues with formatting
  if(!dockHandler->isDelayedFullscreen())
    dockHandler->normalStateToWindow();
  else
  {
    // Started with normal screen layout but was saved as fullscreen - restore full now
    // Switch to fullscreen now after applying normal layout to avoid a distorted layout
    dockHandler->fullscreenStateToWindow();

    if(centralWidget() != ui->dockWidgetMap)
      // Hide the map window title bar if map is undockable
      ui->dockWidgetMap->setTitleBarWidget(new QWidget(ui->dockWidgetMap));

    // Update action
    ui->actionShowFullscreenMap->blockSignals(true);
    ui->actionShowFullscreenMap->setChecked(dockHandler->isFullScreen());
    ui->actionShowFullscreenMap->blockSignals(false);

    mapWidget->addFullScreenExitButton();
    mapWidget->setFocus();
  }

  // Center flight plan after loading - do this delayed to consider window size changes
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_SHOW_ROUTE) && !NavApp::isRouteEmpty())
    NavApp::getMapPaintWidgetGui()->showRect(routeController->getBoundingRect(), false);

  // Set window flag
  stayOnTop();

  // Raise all floating docks and focus map widget
  raiseFloatingWindows();

  if(migrate::getOptionsVersion().isValid() &&
     migrate::getOptionsVersion() <= atools::util::Version("2.6.14") &&
     atools::util::Version(QApplication::applicationVersion()) == atools::util::Version("2.6.15"))
  {
    qDebug() << Q_FUNC_INFO << "Fixing status bar visibility";
    ui->statusBar->setVisible(true);
  }

  ui->actionShowStatusbar->blockSignals(true);
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
  ui->actionShowStatusbar->blockSignals(false);

  // Attempt to restore splitter after first start
  profileWidget->restoreSplitter();

  NavApp::setMainWindowVisible();

  // Draw map ============================================================================
  // Map widget draws gray rectangle until main window is visible
  mapWidget->update();

  // Check for missing simulators and databases ====================================================
  DatabaseManager *databaseManager = NavApp::getDatabaseManager();
  if(!databaseManager->hasSimulatorDatabases() && !databaseManager->hasInstalledSimulators())
  {
    // No databases and no simulators - show a message dialog
    QString message = tr("<p>Could not find a simulator installation on this computer. Also, no scenery library databases were found.</p>"
                           "<p>You can copy a Little Navmap scenery library database from another computer "
                             "if you wish to run this Little Navmap instance on a remote across a network.</p>");

    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineUrl % "NETWORK.html", lnm::helpLanguageOnline());
    message.append(tr("<p><a href=\"%1\">Click here for more information in the Little Navmap online manual</a></p>").arg(url.toString()));

    dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_MISSING_SIMULATORS, message, tr("Do not &show this dialog again."));
  } // else have databases do nothing

  // First installation actions ====================================================

  Settings& settings = Settings::instance();
  if(settings.valueBool(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, true))
  {
    settings.setValue(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, false);

    QString text = tr("<html>"
                        "<head><style>body { font-size: large; white-space: pre; }</style></head>"
                          "<body>"
                            "<h2>Welcome to Little Navmap</h2>"
                              "<p>This seems to be the first time you are installing the program.</p>"
                                "<p>In the following several dialog windows and a web page will<br/>"
                                "open to guide you through the first steps:</p>"
                                "<ol>"
                                  "<li>Web page in the online user manual showing<br/>"
                                  "important information for first time users.</li>"
                                  "<li>A dialog window which allows to create a<br/>"
                                  "directory structure to save your files.<br/>"
                                  "You can do this later in menu \"Tools\" -> \"Create Directory Structure\".<br/>"
                                  "This step is optional.</li>"
                                  "<li>The dialog window \"Load Scenery Library\" opens to load the<br/>"
                                  "simulator scenery into the Little Navmap database.<br/>"
                                  "This process runs in the background.<br/>"
                                  "You can start this manually in the menu<br/>"
                                  "\"Scenery Library\" -> \"Load Scenery Library\".</li>"
                                  "<li>The connection dialog window opens allowing to attach Little Navmap<br/>"
                                  "to a simulator while flying.<br/>"
                                  "Do this manually in menu \"Tools\" -> \"Connect to Flight Simulator\".</li>"
                                "</ol>"
                                "<p>You can also skip all these steps and run them later.</p>"
                                  "<p>See the help menu to access the online user manual and tutorials.</p>"
                                  "</body>"
                                "</html>");

    int retval = QMessageBox::information(this, tr("%1 - Introduction").arg(QApplication::applicationName()), text,
                                          QMessageBox::Ok, QMessageBox::Cancel);

    if(retval == QMessageBox::Ok)
    {
      // Open a start page in the web browser ============================
      HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineStartUrl, lnm::helpLanguageOnline());

      // Create recommended folder structure if user confirms =========================================
      runDirTool(false /* manual */);

      // Show the scenery database dialog on first start
      if(databaseManager->hasInstalledSimulators())
      {
        // Found simulators let the user create new databases
        databaseManager->loadScenery();

        // Open connection dialog ============================
        NavApp::getConnectClient()->connectToServerDialog();
      }
      // else warning was already shown above
    }
  }
  else if(databasesErased)
  {
    databasesErased = false;
    // Databases were removed - show dialog
    if(databaseManager->hasInstalledSimulators())
      databaseManager->loadScenery();
  }

  // Checks if version of database is smaller than application database version and shows a warning dialog if it is
  databaseManager->checkDatabaseVersion();

  // Check for updates once main window is visible
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(), false /* manual */, true /* startup */, false /* forceDebug */);

  // Update the information display later delayed to avoid long loading times due to weather timeout
  QTimer::singleShot(50, infoController, &InfoController::restoreInformation);

#ifdef DEBUG_INFORMATION
  qDebug() << "mapDistanceLabel->size()" << mapDistanceLabel->size();
  qDebug() << "mapPositionLabel->size()" << mapPositionLabel->size();
  qDebug() << "mapMagvarLabel->size()" << mapMagvarLabel->size();
  qDebug() << "mapRenderStatusLabel->size()" << mapRenderStatusLabel->size();
  qDebug() << "mapDetailLabel->size()" << mapDetailLabel->size();
  qDebug() << "mapVisibleLabel->size()" << mapVisibleLabel->size();
  qDebug() << "connectStatusLabel->size()" << connectStatusLabel->size();
  qDebug() << "timeLabel->size()" << timeLabel->size();
#endif
  qDebug() << Q_FUNC_INFO << "leave";
}

void MainWindow::runDirToolManual()
{
  runDirTool(true /* manual */);
}

void MainWindow::runDirTool(bool manual)
{
  bool alreadyComplete, created;
  DirTool dirTool(this, atools::documentsDir(), QApplication::applicationName(), lnm::ACTIONS_SHOW_INSTALL_DIRS);
  dirTool.runIfMissing(manual, alreadyComplete, created);

  qDebug() << Q_FUNC_INFO << "alreadyComplete" << alreadyComplete << "manual" << manual << "created" << created;

  if(alreadyComplete && manual && !created)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Directory structure for Little Navmap files is already complete.</p>"
                                  "<p>Base directory is<br/>\"%1\"</p>").arg(dirTool.getApplicationDir()));
}

void MainWindow::exitFullScreenPressed()
{
  qDebug() << Q_FUNC_INFO;

  // Let the action pass a signal to toggle fs off
  ui->actionShowFullscreenMap->setChecked(false);
}

bool MainWindow::isFullScreen() const
{
  return dockHandler->isFullScreen();
}

void MainWindow::fullScreenOn()
{
  qDebug() << Q_FUNC_INFO << "setFullScreenOn";

  // Hide toolbars and docks initially - user can open again if needed and state is saved then
  dockHandler->setFullScreenOn(atools::gui::HIDE_TOOLBARS | atools::gui::HIDE_DOCKS);

  if(centralWidget() != ui->dockWidgetMap)
    // Add a dummy widget to erase the title bar if map is inside a dock widget
    ui->dockWidgetMap->setTitleBarWidget(new QWidget(ui->dockWidgetMap));

  mapWidget->addFullScreenExitButton();
  mapWidget->setFocus();
  ui->actionShowStatusbar->blockSignals(true);
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
  ui->actionShowStatusbar->blockSignals(false);
}

void MainWindow::fullScreenOff()
{
  qDebug() << Q_FUNC_INFO << "setFullScreenOff";
  mapWidget->removeFullScreenExitButton();

  dockHandler->setFullScreenOff();

  if(centralWidget() != ui->dockWidgetMap)
  {
    // Delete dummy widget to restore title bar
    QWidget *oldTitleBar = ui->dockWidgetMap->titleBarWidget();
    ui->dockWidgetMap->setTitleBarWidget(nullptr);
    delete oldTitleBar;
  }
  ui->actionShowStatusbar->blockSignals(true);
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
  ui->actionShowStatusbar->blockSignals(false);
}

void MainWindow::fullScreenMapToggle()
{
  if(ui->actionShowFullscreenMap->isChecked())
    fullScreenOn();
  else
    fullScreenOff();
}

void MainWindow::setStayOnTop(QWidget *widget)
{
  dockHandler->setStayOnTop(widget, ui->actionWindowStayOnTop->isChecked());
}

void MainWindow::setLnmplnExportDir(const QString& dir)
{
  routeExport->setLnmplnExportDir(dir);
}

void MainWindow::stayOnTop()
{
  qDebug() << Q_FUNC_INFO;

  bool onTop = ui->actionWindowStayOnTop->isChecked();
  dockHandler->setStayOnTop(marbleAboutDialog, onTop);
  dockHandler->setStayOnTop(optionsDialog, onTop);
  dockHandler->setStayOnTopDialogWidgets(onTop);
  dockHandler->setStayOnTopMain(onTop);
}

void MainWindow::allowMovingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setMovingAllowed(ui->actionShowAllowMoving->isChecked());

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Undockable map widget is not registered in handler
    DockWidgetHandler::setMovingAllowed(ui->dockWidgetMap, ui->actionShowAllowMoving->isChecked());
}

void MainWindow::allowDockingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setDockingAllowed(ui->actionShowAllowDocking->isChecked());

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Undockable map widget is not registered in handler
    DockWidgetHandler::setDockingAllowed(ui->dockWidgetMap, ui->actionShowAllowDocking->isChecked());
}

void MainWindow::hideTitleBar()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setHideTitleBar(!ui->actionShowWindowTitleBar->isChecked());

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Undockable map widget is not registered in handler
    DockWidgetHandler::setHideTitleBar(ui->dockWidgetMap, !ui->actionShowWindowTitleBar->isChecked());
}

void MainWindow::mapDockTopLevelChanged(bool topLevel)
{
  // Map widget changed floating state
  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    DockWidgetHandler::setHideTitleBar(ui->dockWidgetMap, dockHandler->getHideTitle() && !topLevel);
}

void MainWindow::raiseFloatingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->raiseWindows();

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Map window is not registered in dockHandler
    DockWidgetHandler::raiseFloatingDockWidget(ui->dockWidgetMap);

  // Avoid having random widget focus
  mapWidget->setFocus();
}

/* Enable or disable actions related to online networks */
void MainWindow::updateOnlineActionStates()
{
  ui->actionViewAirspaceSrcOnline->setEnabled(NavApp::isOnlineNetworkActive());
}

/* Enable or disable actions */
void MainWindow::updateMarkActionStates()
{
  ui->actionMapHideAllDistanceMarkers->setEnabled(!mapWidget->getDistanceMarks().isEmpty());
  ui->actionMapHideAllRangeRings->setEnabled(!mapWidget->getRangeMarks().isEmpty());
  ui->actionMapHideAllPatterns->setEnabled(!mapWidget->getPatternsMarks().isEmpty());
  ui->actionMapHideAllMsa->setEnabled(!mapWidget->getMsaMarks().isEmpty());
  ui->actionMapHideAllHoldings->setEnabled(!mapWidget->getHoldingMarks().isEmpty());
}

/* Enable or disable actions */
void MainWindow::updateHighlightActionStates()
{
  ui->actionMapClearAllHighlights->setEnabled(mapWidget->hasHighlights() || searchController->hasSelection() ||
                                              routeController->hasTableSelection());
}

/* Enable or disable actions */
void MainWindow::updateActionStates()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(NavApp::isShuttingDown())
    return;

  ui->actionClearKml->setEnabled(!mapWidget->getKmlFiles().isEmpty());

  // Enable MORA button depending on available data
  ui->actionMapShowMinimumAltitude->setEnabled(NavApp::isMoraAvailable());
  ui->actionMapShowGls->setEnabled(NavApp::isGlsAvailable());
  ui->actionMapShowHolding->setEnabled(NavApp::isHoldingsAvailable());
  ui->actionMapShowAirportMsa->setEnabled(NavApp::isAirportMsaAvailable());

  const Route& route = NavApp::getRouteConst();
  bool hasFlightplan = !route.isFlightplanEmpty();
  bool hasTrack = !NavApp::isAircraftTrailEmpty();
  ui->actionRouteAppend->setEnabled(hasFlightplan);
  ui->actionRouteSave->setEnabled(hasFlightplan /* && routeController->hasChanged()*/);
  ui->actionRouteSaveAs->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsFlightGear->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsFms11->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsPln->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsPlnMsfs->setEnabled(hasFlightplan);
  ui->actionSaveAircraftTrailToGPX->setEnabled(hasFlightplan || hasTrack);
  ui->actionRouteSaveAsHtml->setEnabled(hasFlightplan);
  ui->actionRouteSaveAll->setEnabled(hasFlightplan && routeExport->hasSelected());
  ui->actionRouteSendToSimBrief->setEnabled(hasFlightplan);

  ui->actionRouteSaveAsVfp->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsIvap->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsXIvap->setEnabled(hasFlightplan);
  ui->actionRouteShowSkyVector->setEnabled(hasFlightplan);

  ui->actionRouteCenter->setEnabled(hasFlightplan);
  ui->actionRouteSelectParking->setEnabled(route.hasValidDeparture());
  ui->actionMapShowRoute->setEnabled(true);
  ui->actionMapShowTocTod->setEnabled(true);
  ui->actionMapShowAlternate->setEnabled(true);
  ui->actionInfoApproachShowMissedAppr->setEnabled(true);
  ui->actionRouteEditMode->setEnabled(hasFlightplan);
  ui->actionPrintFlightplan->setEnabled(hasFlightplan);
  ui->actionRouteCopyString->setEnabled(hasFlightplan);
  ui->actionRouteAdjustAltitude->setEnabled(hasFlightplan);

  bool hasTracks = NavApp::hasTracks();
  ui->actionRouteDeleteTracks->setEnabled(hasTracks);
  ui->actionMapShowTracks->setEnabled(hasTracks);

  bool hasTracksEnabled = NavApp::hasTracksEnabled();
  ui->actionRouteDownloadTracksNow->setEnabled(hasTracksEnabled);

#ifdef DEBUG_MOVING_AIRPLANE
  ui->actionMapShowAircraft->setEnabled(true);
  ui->actionMapAircraftCenter->setEnabled(true);
  ui->actionMapAircraftCenterNow->setEnabled(true);
  ui->actionMapShowAircraftAi->setEnabled(true);
  ui->actionMapShowAircraftOnline->setEnabled(true);
  ui->actionMapShowAircraftAiBoat->setEnabled(true);
#else
  ui->actionMapShowAircraft->setEnabled(NavApp::isConnected());
  ui->actionMapAircraftCenter->setEnabled(NavApp::isConnected());
  ui->actionMapAircraftCenterNow->setEnabled(NavApp::isConnectedAndAircraft());

  // AI, multiplayer or online clients
  ui->actionMapShowAircraftAi->setEnabled(NavApp::isConnected() && NavApp::isFetchAiAircraft());
  ui->actionMapShowAircraftOnline->setEnabled(NavApp::getOnlinedataController()->isNetworkActive());

  ui->actionMapShowAircraftAiBoat->setEnabled(NavApp::isConnected() && NavApp::isFetchAiShip());
#endif

  ui->actionConnectSimulatorToggle->blockSignals(true);
  ui->actionConnectSimulatorToggle->setChecked(NavApp::isConnected());
  ui->actionConnectSimulatorToggle->blockSignals(false);

  ui->actionMapShowAircraftTrack->setEnabled(true);
  ui->actionMapDeleteAircraftTrack->setEnabled(mapWidget->getAircraftTrailSize() > 0 || profileWidget->hasTrailPoints());

  bool canCalcRoute = route.canCalcRoute();
  ui->actionRouteCalcDirect->setEnabled(canCalcRoute && NavApp::getRouteConst().hasEntries());
  ui->actionRouteReverse->setEnabled(canCalcRoute);

  ui->actionMapShowAirportWeather->setEnabled(NavApp::getAirportWeatherSource() != map::WEATHER_SOURCE_DISABLED);

  ui->actionMapShowHome->setEnabled(mapWidget->getHomePos().isValid());
  ui->actionMapShowMark->setEnabled(mapWidget->getSearchMarkPos().isValid());

  ui->actionShowApproachCustom->setEnabled(route.hasValidDestinationAndRunways());
  ui->actionShowDepartureCustom->setEnabled(route.hasValidDepartureAndRunways());
}

void MainWindow::resetAllSettings()
{
  qDebug() << Q_FUNC_INFO;
  QString settingFile = Settings::getFilename();
  QString settingPath = Settings::getPath();

  QMessageBox::StandardButton retval =
    QMessageBox::warning(this, QApplication::applicationName() % tr("Reset all Settings "),
                         tr("<b>This will reset all options, window layout, dialog layout, "
                              "aircraft trail, map position history and file histories "
                              "back to default and restart %1.</b><br/><br/>"
                              "User features like range rings or patterns as well as "
                              "scenery, logbook and userpoint databases are not affected.<br/><br/>"
                              "A copy of the settings file<br/><br/>"
                              "\"%2\"<br/><br/>"
                              "will be created in the folder<br/><br/>"
                              "\"%3\"<br/><br/>"
                              "which allows you to undo this change."
                            ).arg(QApplication::applicationName()).arg(settingFile).arg(settingPath),
                         QMessageBox::Ok | QMessageBox::Cancel | QMessageBox::Help, QMessageBox::Cancel);

  if(retval == QMessageBox::Ok)
  {
    NavApp::setRestartProcess(true);
    close();
  }
  else if(retval == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl % "MENUS.html#reset-and-restart", lnm::helpLanguageOnline());
}

void MainWindow::createIssueReport()
{
  qDebug() << Q_FUNC_INFO;

  // Build report and get file path
  QString crashReportFile = NavApp::buildCrashReportNavAppManual();

  QFileInfo crashReportFileinfo(crashReportFile);
  QUrl crashReportUrl = QUrl::fromLocalFile(crashReportFileinfo.absoluteFilePath());

  QString message = tr("<p style=\"white-space:pre\">An issue report was generated and saved with all related files in a Zip archive.</p>"
                         "<p style=\"white-space:pre\"><a href=\"%1\"><b>Click here to open the directory containing the report \"%2\"</b></a></p>"
                           "<p style=\"white-space:pre\">You can send this file to the author of %3 to investigate a problem.</p>"
                             "<p style=\"white-space:pre\">%4</p>").
                    arg(crashReportUrl.toString()).arg(crashReportFileinfo.fileName()).
                    arg(QApplication::applicationName()).arg(NavApp::getContactHtml());

  atools::gui::MessageBox box(this, QApplication::applicationName(), "ISSUEREPORT.html");
  box.setHelpOnlineUrl(lnm::helpOnlineUrl);
  box.setHelpLanguageOnline(lnm::helpLanguageOnline());

  box.addAcceptButton(QDialogButtonBox::Ok);
  box.addButton(QDialogButtonBox::Help);
  box.setText(message);
  box.setIcon(QMessageBox::Information);

  connect(&box, &atools::gui::MessageBox::linkActivated, [&box](const QString& link) {
    if(link.startsWith("https://") || link.startsWith("http://"))
      atools::gui::HelpHandler::openUrl(&box, QUrl(link));
    else
      atools::gui::showInFileManager(link, &box);
  });

  box.exec();
}

void MainWindow::resetWindowLayout()
{
  qDebug() << Q_FUNC_INFO;

  mapWidget->removeFullScreenExitButton();

  bool allowUndockMap = OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK);
  dockHandler->resetWindowState(lnm::DEFAULT_MAINWINDOW_SIZE,
                                QString(":/littlenavmap/resources/config/mainwindow_state_%1.bin").
                                arg(allowUndockMap ? "dock" : "nodock"));

  ui->dockWidgetMap->setVisible(allowUndockMap);

  ui->actionShowFullscreenMap->blockSignals(true);
  ui->actionShowFullscreenMap->setChecked(false);
  ui->actionShowFullscreenMap->blockSignals(false);

  ui->actionShowStatusbar->setChecked(true);
  ui->statusBar->setVisible(true);
  ui->menuBar->setVisible(true);
}

void MainWindow::resetTabLayout()
{
  qDebug() << Q_FUNC_INFO;
  searchController->resetTabLayout();
  infoController->resetTabLayout();
  routeController->resetTabLayout();
}

/* Read settings for all windows, docks, controller and manager classes */
void MainWindow::restoreStateMain()
{
  qDebug() << Q_FUNC_INFO << "enter";

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);

  Settings& settings = Settings::instance();

  applyToolBarSize();

  if(!NavApp::isSafeMode() && settings.contains(lnm::MAINWINDOW_WIDGET_DOCKHANDLER))
  {
    dockHandler->restoreState(settings.valueVar(lnm::MAINWINDOW_WIDGET_DOCKHANDLER).toByteArray());

    // Start with normal state - apply fullscreen later to avoid layout mess up
    dockHandler->normalStateToWindow();
    ui->actionShowFullscreenMap->blockSignals(true);
    ui->actionShowFullscreenMap->setChecked(dockHandler->isFullScreen());
    ui->actionShowFullscreenMap->blockSignals(false);

    ui->actionShowStatusbar->blockSignals(true);
    ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
    ui->actionShowStatusbar->blockSignals(false);
  }
  else
    // Use default state saved in application resources
    resetWindowLayout();

  // Need to be loaded in constructor first since it reads all options
  // optionsDialog->restoreState();

  // Initalize early to allow altitude adaption when loading flight plans
  // Errors are show later in MainWindow::mainWindowShown()
  NavApp::initElevationProvider();

  qDebug() << "kmlFileHistory";
  kmlFileHistory->restoreState();

  qDebug() << "layoutFileHistory";
  layoutFileHistory->restoreState();

  qDebug() << "searchController";
  routeFileHistory->restoreState();

  qDebug() << "mapWidget";
  mapWidget->restoreState();

  qDebug() << "searchController";
  searchController->restoreState();

  qDebug() << "logdataController";
  NavApp::getLogdataController()->restoreState();

  qDebug() << "mapMarkHandler";
  NavApp::getMapMarkHandler()->restoreState();

  qDebug() << "mapAirportHandler";
  NavApp::getMapAirportHandler()->restoreState();

  qDebug() << "mapDetailHandler";
  NavApp::getMapDetailHandler()->restoreState();

  qDebug() << "userdataController";
  NavApp::getUserdataController()->restoreState();

  qDebug() << "windReporter";
  NavApp::getWindReporter()->restoreState();

  qDebug() << "airspaceController";
  NavApp::getAirspaceController()->restoreState();

  qDebug() << "aircraftPerfController";
  NavApp::getAircraftPerfController()->restoreState();

  qDebug() << "routeController";
  routeController->restoreState();

  qDebug() << "profileWidget";
  profileWidget->restoreState();

  qDebug() << "connectClient";
  NavApp::getConnectClient()->restoreState();

  qDebug() << "trackController";
  NavApp::getTrackController()->restoreState();

  qDebug() << "infoController";
  infoController->restoreState();

  qDebug() << "printSupport";
  printSupport->restoreState();

  qDebug() << "routeExport";
  routeExport->restoreState();

  mapThemeHandler->restoreState();
  updateMapKeys();

  widgetState.setBlockSignals(true);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    // Restore map settings if requested by the user
    widgetState.restore({ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp,
                         ui->actionMapShowIls, ui->actionMapShowGls, ui->actionMapShowHolding, ui->actionMapShowAirportMsa,
                         ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways, ui->actionMapShowTracks, ui->actionShowAirspaces,
                         ui->actionMapShowRoute, ui->actionMapShowTocTod, ui->actionMapShowAlternate, ui->actionMapShowAircraft,
                         ui->actionMapShowCompassRose, ui->actionMapShowCompassRoseAttach, ui->actionMapShowEndurance,
                         ui->actionMapShowSelectedAltRange, ui->actionMapShowTurnPath, ui->actionMapAircraftCenter,
                         ui->actionMapShowAircraftAi, ui->actionMapShowAircraftOnline, ui->actionMapShowAircraftAiBoat,
                         ui->actionMapShowAircraftTrack, ui->actionInfoApproachShowMissedAppr, ui->actionSearchLogdataShowDirect,
                         ui->actionSearchLogdataShowRoute, ui->actionSearchLogdataShowTrack});
  }
  else
    mapWidget->resetSettingActionsToDefault();

  // Map settings that are always loaded
  widgetState.restore({ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionRouteEditMode, ui->actionRouteSaveSidStarWaypointsOpt,
                       ui->actionRouteSaveApprWaypointsOpt, ui->actionRouteSaveAirwayWaypointsOpt, ui->actionLogdataCreateLogbook,
                       ui->actionAircraftPerformanceWarnMismatch, ui->actionMapShowSunShading, ui->actionMapShowAirportWeather,
                       ui->actionMapShowMinimumAltitude, ui->actionRunWebserver, ui->actionShowAllowDocking, ui->actionShowAllowMoving,
                       ui->actionShowWindowTitleBar, ui->actionWindowStayOnTop});

  widgetState.setBlockSignals(false);

  // Already loaded in constructor early to allow database creations
  // databaseLoader->restoreState();

  if(!(OptionData::instance().getFlags2() & opts2::MAP_ALLOW_UNDOCK))
    ui->dockWidgetMap->hide();
  else
    ui->dockWidgetMap->show();

  qDebug() << Q_FUNC_INFO << "leave";
}

void MainWindow::applyToolBarSize()
{
  if(OptionData::instance().getFlags2().testFlag(opts2::OVERRIDE_TOOLBAR_SIZE))
    setIconSize(OptionData::instance().getGuiToolbarSize());
  else
    setIconSize(defaultToolbarIconSize);
}

void MainWindow::optionsChanged()
{
  applyToolBarSize();

  dockHandler->setAutoRaiseWindows(OptionData::instance().getFlags2().testFlag(opts2::RAISE_DOCK_WINDOWS));
  dockHandler->setAutoRaiseMainWindow(OptionData::instance().getFlags2().testFlag(opts2::RAISE_MAIN_WINDOW));

  if(mapThemeHandler != nullptr)
    mapThemeHandler->optionsChanged();
}

void MainWindow::fontChanged(const QFont& font)
{
  atools::gui::updateAllFonts(this, font);
}

void MainWindow::updateMapKeys()
{
  if(mapThemeHandler != nullptr)
  {
    // Is null on startup
    if(mapWidget != nullptr)
      mapWidget->setKeys(mapThemeHandler->getMapThemeKeysHash());

    // Might be null if not started
    if(NavApp::getMapPaintWidgetWeb() != nullptr)
      NavApp::getMapPaintWidgetWeb()->setKeys(mapThemeHandler->getMapThemeKeysHash());
  }
}

void MainWindow::saveStateNow()
{
  saveStateMain();
}

/* Write settings for all windows, docks, controller and manager classes */
void MainWindow::saveStateMain()
{
  qDebug() << Q_FUNC_INFO;

  try
  {
#ifdef DEBUG_CREATE_WINDOW_STATE
    // Save the state into a binary file to be used for reset window layout
    // One state is needed with undockable map window and one without
    QFile stateFile(QString("mainwindow_state_%1.bin").
                    arg(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK) ? "dock" : "nodock"));
    if(stateFile.open(QFile::WriteOnly))
    {
      stateFile.write(saveState());
      stateFile.close();
    }

#endif

#ifdef DEBUG_DUMP_SHORTCUTS
    printShortcuts();
#endif

    // About to reset all settings and restart application
    if(NavApp::isRestartProcess())
    {
      deleteAircraftTrail(true /* quiet */);
      mapWidget->clearHistory();
    }

    saveMainWindowStates();

    qDebug() << Q_FUNC_INFO << "searchController";
    if(searchController != nullptr)
      searchController->saveState();

    qDebug() << Q_FUNC_INFO << "mapWidget";
    if(mapWidget != nullptr)
      mapWidget->saveState();

    qDebug() << Q_FUNC_INFO << "userDataController";
    if(mapThemeHandler != nullptr)
      mapThemeHandler->saveState();

    qDebug() << Q_FUNC_INFO << "userDataController";
    if(NavApp::getUserdataController() != nullptr)
      NavApp::getUserdataController()->saveState();

    qDebug() << Q_FUNC_INFO << "mapMarkHandler";
    if(NavApp::getMapMarkHandler() != nullptr)
      NavApp::getMapMarkHandler()->saveState();

    qDebug() << Q_FUNC_INFO << "mapAirportHandler";
    if(NavApp::getMapAirportHandler() != nullptr)
      NavApp::getMapAirportHandler()->saveState();

    qDebug() << Q_FUNC_INFO << "mapDetailHandler";
    if(NavApp::getMapDetailHandler() != nullptr)
      NavApp::getMapDetailHandler()->saveState();

    qDebug() << Q_FUNC_INFO << "logdataController";
    if(NavApp::getLogdataController() != nullptr)
      NavApp::getLogdataController()->saveState();

    qDebug() << Q_FUNC_INFO << "windReporter";
    if(NavApp::getWindReporter() != nullptr)
      NavApp::getWindReporter()->saveState();

    qDebug() << Q_FUNC_INFO << "aircraftPerfController";
    if(NavApp::getAircraftPerfController() != nullptr)
      NavApp::getAircraftPerfController()->saveState();

    qDebug() << Q_FUNC_INFO << "airspaceController";
    if(NavApp::getAirspaceController() != nullptr)
      NavApp::getAirspaceController()->saveState();

    qDebug() << Q_FUNC_INFO << "routeController";
    if(routeController != nullptr)
      routeController->saveState();

    qDebug() << Q_FUNC_INFO << "profileWidget";
    if(profileWidget != nullptr)
      profileWidget->saveState();

    qDebug() << Q_FUNC_INFO << "connectClient";
    if(NavApp::getConnectClient() != nullptr)
      NavApp::getConnectClient()->saveState();

    qDebug() << Q_FUNC_INFO << "trackController";
    if(NavApp::getTrackController() != nullptr)
      NavApp::getTrackController()->saveState();

    qDebug() << Q_FUNC_INFO << "infoController";
    if(infoController != nullptr)
      infoController->saveState();

    qDebug() << Q_FUNC_INFO << "routeStringDialog";
    if(routeStringDialog != nullptr)
      routeStringDialog->saveState();

    saveFileHistoryStates();

    qDebug() << Q_FUNC_INFO << "printSupport";
    if(printSupport != nullptr)
      printSupport->saveState();

    qDebug() << Q_FUNC_INFO << "routeExport";
    if(routeExport != nullptr)
      routeExport->saveState();

    qDebug() << Q_FUNC_INFO << "optionsDialog";
    if(optionsDialog != nullptr)
      optionsDialog->saveState();

    qDebug() << Q_FUNC_INFO << "styleHandler";
    if(NavApp::getStyleHandler() != nullptr)
      NavApp::getStyleHandler()->saveState();

    saveActionStates();

    qDebug() << Q_FUNC_INFO << "databaseManager";
    if(NavApp::getDatabaseManager() != nullptr)
      NavApp::getDatabaseManager()->saveState();

    qDebug() << Q_FUNC_INFO << "syncSettings";
    Settings::syncSettings();
    qDebug() << Q_FUNC_INFO << "save state done";
  }
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

void MainWindow::saveFileHistoryStates()
{
  qDebug() << Q_FUNC_INFO;

  qDebug() << Q_FUNC_INFO << "routeFileHistory";
  if(routeFileHistory != nullptr)
    routeFileHistory->saveState();

  qDebug() << Q_FUNC_INFO << "kmlFileHistory";
  if(kmlFileHistory != nullptr)
    kmlFileHistory->saveState();

  qDebug() << Q_FUNC_INFO << "layoutFileHistory";
  if(layoutFileHistory != nullptr)
    layoutFileHistory->saveState();

  Settings::syncSettings();
}

void MainWindow::saveMainWindowStates()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);

  Settings& settings = Settings::instance();
  settings.setValueVar(lnm::MAINWINDOW_WIDGET_DOCKHANDLER, dockHandler->saveState());

  // Save profile dock size separately since it is sometimes resized by other docks
  // settings.setValue(lnm::MAINWINDOW_WIDGET_STATE + "ProfileDockHeight", ui->dockWidgetContentsProfile->height());
  Settings::syncSettings();
}

void MainWindow::saveActionStates()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);
  widgetState.save({ui->actionMapShowVor, ui->actionMapShowNdb,
                    ui->actionMapShowWp, ui->actionMapShowIls, ui->actionMapShowGls, ui->actionMapShowHolding, ui->actionMapShowAirportMsa,
                    ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways, ui->actionMapShowTracks, ui->actionShowAirspaces,
                    ui->actionMapShowRoute, ui->actionMapShowTocTod, ui->actionMapShowAlternate, ui->actionMapShowAircraft,
                    ui->actionMapShowCompassRose, ui->actionMapShowCompassRoseAttach, ui->actionMapShowEndurance,
                    ui->actionMapShowSelectedAltRange, ui->actionMapShowTurnPath, ui->actionMapAircraftCenter, ui->actionMapShowAircraftAi,
                    ui->actionMapShowAircraftOnline, ui->actionMapShowAircraftAiBoat, ui->actionMapShowAircraftTrack,
                    ui->actionInfoApproachShowMissedAppr, ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowSunShading,
                    ui->actionMapShowAirportWeather, ui->actionMapShowMinimumAltitude, ui->actionRouteEditMode,
                    ui->actionRouteSaveSidStarWaypointsOpt, ui->actionAircraftPerformanceWarnMismatch, ui->actionRouteSaveApprWaypointsOpt,
                    ui->actionRouteSaveAirwayWaypointsOpt, ui->actionLogdataCreateLogbook, ui->actionRunWebserver,
                    ui->actionSearchLogdataShowDirect, ui->actionSearchLogdataShowRoute, ui->actionSearchLogdataShowTrack,
                    ui->actionShowAllowDocking, ui->actionShowAllowMoving, ui->actionShowWindowTitleBar, ui->actionWindowStayOnTop});

  Settings::syncSettings();
}

QList<QAction *> MainWindow::getMainWindowActions()
{
  QList<QAction *> actions;
  const QList<QAction *> menuBarActions = ui->menuBar->actions();
  for(QAction *menuBarAction : menuBarActions)
  {
    if(menuBarAction->menu() != nullptr)
    {
      const QList<QAction *> menuActions = menuBarAction->menu()->actions();
      for(QAction *menuAction : menuActions)
      {
        if(menuAction->menu() != nullptr)
        {
          const QList<QAction *> subMenuActions = menuAction->menu()->actions();
          for(QAction *subMenuAction : subMenuActions)
          {
            if(!subMenuAction->text().isEmpty() && !subMenuAction->shortcut().isEmpty())
              actions.append(subMenuAction);
          }
        }
        else
        {
          if(!menuAction->text().isEmpty() && !menuAction->shortcut().isEmpty())
            actions.append(menuAction);
        }
      }
    }
  }
  return actions;
}

#ifdef DEBUG_DUMP_SHORTCUTS
void MainWindow::printShortcuts()
{
  // Print all main menu and sub menu shortcuts ==============================================
  qDebug() << "===============================================================================";

  QString outShortcuts, outItems;
  QTextStream streamShortcuts(&outShortcuts, QIODevice::WriteOnly);
  QTextStream streamItems(&outItems, QIODevice::WriteOnly);
  QSet<QKeySequence> keys;
  QStringList warnings;

  int c1 = 80, c2 = 25;

  const QList<QAction *> actions = ui->menuBar->actions();
  for(const QAction *mainmenus : actions)
  {
    if(mainmenus->menu() != nullptr)
    {
      QString text = mainmenus->menu()->menuAction()->text().remove(QChar('&'));

      streamShortcuts << endl << ".. _shortcuts-main-" << text.toLower() << ":" << endl << endl;

      streamShortcuts << text << endl;
      streamShortcuts << QString("^").repeated(text.size()) << endl << endl;

      streamShortcuts << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
      streamShortcuts << "| " << QString("Menu").leftJustified(c1 - 1)
                      << "| " << QString("Shortcut").leftJustified(c2 - 1) << "|" << endl;
      streamShortcuts << "+" << QString("=").repeated(c1) << "+" << QString("=").repeated(c2) << "+" << endl;

      QString mainmenu = mainmenus->text().remove(QChar('&'));
      const QList<QAction *> menuActions = mainmenus->menu()->actions();
      for(const QAction *mainAction : menuActions)
      {
        if(mainAction->menu() != nullptr)
        {
          QString submenu = mainAction->text().remove(QChar('&'));
          const QList<QAction *> subMenuActions = mainAction->menu()->actions();
          for(const QAction *subAction : subMenuActions)
          {
            if(!subAction->text().isEmpty() && !subAction->shortcut().isEmpty())
            {
              if(keys.contains(subAction->shortcut()))
                warnings.append(QString("Duplicate shortcut \"%1\"").arg(subAction->shortcut().toString()));

              streamShortcuts << "| "
                              << QString(mainmenu + " -> " + submenu + " -> " + subAction->text().remove(QChar('&'))).leftJustified(c1 - 1)
                              << "| "
                              << ("``" + subAction->shortcut().toString() + "``").leftJustified(c2 - 1)
                              << "|" << endl;
              streamShortcuts << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
              keys.insert(subAction->shortcut());
            }

            if(!submenu.startsWith("Recent") && !subAction->text().isEmpty())
            {
              QString actionText = subAction->text().remove(QChar('&')).simplified().remove(" ...").remove("...");
              QString imageName = subAction->icon().name().isEmpty() ? QString() : QFileInfo(subAction->icon().name()).baseName() % ".png";

              streamItems << (".. ===== " % mainmenu % " -> " % submenu % " -> " % subAction->text().remove(QChar('&'))) << endl;

              // .. _show-empty-airports:
              streamItems << (".. _" % actionText.toLower().replace(" ", "-") % ":") << endl;

              // if(imageName.isEmpty())
              // {
              //// Show empty Airports
              //// '''''''''''''''''''''''''''''''''''''''''
              // streamItems << actionText << endl;
              // streamItems << QString("'").repeated(80) << endl << endl;
              // }
              // else
              {
                // |Show empty Airports| Show empty Airports
                // '''''''''''''''''''''''''''''''''''''''''
                streamItems << ("|" % actionText % "| " % actionText) << endl;
                streamItems << QString("'").repeated(80) << endl << endl;

                // .. |Show empty Airports| image:: ../images/icon_airportempty.png
                streamItems << (".. |" % actionText % "| image:: ../images/icon_" % imageName) << endl << endl;
              }
            }
          }
          submenu.clear();
        }
        else
        {
          if(!mainAction->text().isEmpty() && !mainAction->shortcut().isEmpty())
          {
            if(keys.contains(mainAction->shortcut()))
              warnings.append(QString("Duplicate shortcut \"%1\"").arg(mainAction->shortcut().toString()));

            streamShortcuts << "| "
                            << QString(mainmenu + " -> " + mainAction->text().remove(QChar('&'))).leftJustified(c1 - 1)
                            << "| "
                            << ("``" + mainAction->shortcut().toString() + "``").leftJustified(c2 - 1)
                            << "|" << endl;
            streamShortcuts << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
            keys.insert(mainAction->shortcut());
          }

          if(!mainAction->text().isEmpty())
          {
            streamItems << (".. ===== " % mainmenu % " -> " % mainAction->text().remove(QChar('&'))) << endl;
            QString actionText = mainAction->text().remove(QChar('&')).simplified().remove(" ...").remove("...");
            QString imageName = mainAction->icon().name().isEmpty() ? QString() : QFileInfo(mainAction->icon().name()).baseName() % ".png";

            // if(imageName.isEmpty())
            // {
            //// Show empty Airports
            //// '''''''''''''''''''''''''''''''''''''''''
            // streamItems << actionText << endl;
            // streamItems << QString("'").repeated(80) << endl << endl;
            // }
            // else
            {
              // .. _show-empty-airports:
              streamItems << (".. _" % actionText.toLower().replace(" ", "-") % ":") << endl;

              // |Show empty Airports| Show empty Airports
              // '''''''''''''''''''''''''''''''''''''''''
              streamItems << ("|" % actionText % "| " % actionText) << endl;
              streamItems << QString("'").repeated(80) << endl << endl;

              // .. |Show empty Airports| image:: ../images/icon_airportempty.png
              streamItems << (".. |" % actionText % "| image:: ../images/icon_" % imageName) << endl << endl;
            }
          }
        }
      }
    }
  }
  qDebug() << "Shortcuts ===============================================================================";
  qDebug().nospace().noquote() << endl << outShortcuts;
  qDebug() << "===============================================================================";
  qDebug().nospace().noquote() << endl << outItems;
  qDebug() << "Items ===============================================================================";
  for(const QString& warning : warnings)
    qWarning().nospace().noquote() << Q_FUNC_INFO << " " << warning;

  qDebug() << "===============================================================================";

  QStringList keysStr;
  for(const QKeySequence& key: keys)
    keysStr.append(key.toString());
  std::sort(keysStr.begin(), keysStr.end());

  qInfo().nospace().noquote() << Q_FUNC_INFO << " " << keysStr.join("\n") << endl;

  qDebug() << "===============================================================================";
}

#endif

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Catch all close events like Ctrl-Q or Menu/Exit or clicking on the
  // close button on the window frame
  qDebug() << Q_FUNC_INFO;

  // Save to default file in options or delete the default file if plan is empty
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_ROUTE))
    // Save automatically
    routeController->saveFlightplanLnmDefault();
  else if(routeController->hasChanged())
  {
    // Changed flight plan found - ask for confirmation
    if(!routeCheckForChanges())
    {
      // Do not exit
      event->ignore();

      // Do not restart process after settings reset
      NavApp::setRestartProcess(false);
      return;
    }
  }

  if(NavApp::getAircraftPerfController()->hasChanged())
  {
    if(!NavApp::getAircraftPerfController()->checkForChanges())
    {
      // Do not exit
      event->ignore();

      // Do not restart process after settings reset
      NavApp::setRestartProcess(false);
      return;
    }
  }

  bool quit = false;
  // Do not ask if user did a reset settings
  if(!NavApp::isRestartProcess())
  {
    if(NavApp::getDatabaseManager()->isLoadingProgress())
    {
      // Database compiling in background ==========================
      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_QUIT_LOADING,
                                              tr("%1 is loading the scenery library database in the background.\n"
                                                 "Really quit and cancel the loading process?").arg(QApplication::applicationName()),
                                              tr("Do not &show this dialog again and cancel loading."),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::Yes);

      if(result != QMessageBox::Yes)
      {
        // Do not exit
        event->ignore();
        NavApp::getDatabaseManager()->showProgressWindow();
      }
      else
        quit = true;
    }
    else
    {
      // Normal as quit dialog ======================================
      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_QUIT, tr("Really quit?"),
                                              tr("Do not &show this dialog again and quit."),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::Yes);

      if(result != QMessageBox::Yes)
        // Do not exit
        event->ignore();
      else
        quit = true;
    }
  }

  if(quit)
  {
    NavApp::setCloseCalled();
    NavApp::getDatabaseManager()->loadSceneryStop();

    // Close all registerd non-modal dialogs to allow application to close
    dockHandler->closeAllDialogWidgets();
  }

  saveStateMain();
}

void MainWindow::showEvent(QShowEvent *event)
{
  if(firstStart)
  {
    emit windowShown();
    firstStart = false;
  }

  event->ignore();
}

/* Call other other classes to close queries and clear caches */
void MainWindow::preDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  if(!hasDatabaseLoadStatus)
  {
    hasDatabaseLoadStatus = true;

    searchController->preDatabaseLoad();
    routeController->preDatabaseLoad();

    mapWidget->preDatabaseLoad();
    NavApp::getWebController()->postDatabaseLoad();

    profileWidget->preDatabaseLoad();
    infoController->preDatabaseLoad();
    weatherReporter->preDatabaseLoad();
    windReporter->preDatabaseLoad();

    NavApp::preDatabaseLoad();

    weatherContextHandler->clearWeatherContext();
  }
  else
    qWarning() << "Already in database loading status";
}

/* Call other other classes to reopen queries */
void MainWindow::postDatabaseLoad(atools::fs::FsPaths::SimulatorType type)
{
  qDebug() << Q_FUNC_INFO;

  if(hasDatabaseLoadStatus)
  {
    NavApp::getWebController()->postDatabaseLoad();
    mapWidget->postDatabaseLoad(); // Init map widget dependent queries first
    NavApp::postDatabaseLoad();
    searchController->postDatabaseLoad();
    routeController->postDatabaseLoad();
    profileWidget->postDatabaseLoad();
    infoController->postDatabaseLoad();
    weatherReporter->postDatabaseLoad(type);
    windReporter->postDatabaseLoad(type);
    routeExport->postDatabaseLoad();

    // U actions for flight simulator database switch in main menu
    NavApp::getDatabaseManager()->insertSimSwitchActions();

    hasDatabaseLoadStatus = false;
  }
  else
    qWarning() << "Not in database loading status";

  // Database title might have changed
  updateWindowTitle();

  // Update toolbar buttons
  updateActionStates();

  NavApp::getAirspaceController()->updateButtonsAndActions();

  // Need to clear caches again and redraw after enabling queries
  updateMapObjectsShown();

  NavApp::logDatabaseMeta();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  qDebug() << Q_FUNC_INFO;
  // Accept only one file
  if(event->mimeData() != nullptr && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
  {
    QList<QUrl> urls = event->mimeData()->urls();

    if(!urls.isEmpty())
    {
      // Has to be a file
      QUrl url = urls.constFirst();
      if(url.isLocalFile())
      {
        // accept if file exists, is readable and matches the supported extensions or content
        QFileInfo file(url.toLocalFile());

        QString flightplan, perf, layout, gpx;
        fc::checkFileType(file.filePath(), &flightplan, &perf, &layout, &gpx);

        if(!flightplan.isEmpty() || !perf.isEmpty() || !layout.isEmpty() || !gpx.isEmpty())
        {
          qDebug() << Q_FUNC_INFO << "accepting" << url;
          event->acceptProposedAction();
          return;
        }
      }
      qDebug() << Q_FUNC_INFO << "not accepting" << url;
    }
  }
}

void MainWindow::dropEvent(QDropEvent *event)
{
  if(event->mimeData()->urls().size() == 1)
  {
    QString filepath = event->mimeData()->urls().constFirst().toLocalFile();
    qDebug() << Q_FUNC_INFO << "Dropped file:" << filepath;

    if(atools::gui::DockWidgetHandler::isWindowLayoutFile(filepath))
      // Open window layout file
      layoutOpenDrag(filepath);
    else if(AircraftPerfController::isPerformanceFile(filepath))
      // Load aircraft performance file
      NavApp::getAircraftPerfController()->loadFile(filepath);
    else if(atools::fs::gpx::GpxIO::isGpxFile(filepath))
      // Load GPX trail
      trailLoadGpxFile(filepath);
    else
      // Load flight plan
      routeOpenFile(filepath);
  }
}

#ifdef DEBUG_SIZE_INFORMATION

void MainWindow::resizeEvent(QResizeEvent *event)
{
  qDebug() << event->size();
}

#endif

void MainWindow::showFlightPlan()
{
  if(NavApp::isMainWindowVisible() && OptionData::instance().getFlags2() & opts2::RAISE_WINDOWS)
    actionShortcutFlightPlanTriggered();
}

void MainWindow::showAircraftPerformance()
{
  if(NavApp::isMainWindowVisible() && OptionData::instance().getFlags2() & opts2::RAISE_WINDOWS)
    actionShortcutAircraftPerformanceTriggered();
}

void MainWindow::showLogbookSearch()
{
  if(NavApp::isMainWindowVisible() && OptionData::instance().getFlags2() & opts2::RAISE_WINDOWS)
    actionShortcutLogbookSearchTriggered();
}

void MainWindow::showUserpointSearch()
{
  if(NavApp::isMainWindowVisible() && OptionData::instance().getFlags2() & opts2::RAISE_WINDOWS)
    actionShortcutUserpointSearchTriggered();
}

void MainWindow::webserverStatusChanged(bool running)
{
  ui->actionRunWebserver->setChecked(running);
  ui->actionOpenWebserver->setEnabled(running);
}

void MainWindow::toggleWebserver(bool checked)
{
  if(checked)
  {
    NavApp::getWebController()->startServer();
    updateMapKeys();
  }
  else
    NavApp::getWebController()->stopServer();
}

void MainWindow::openWebserver()
{
  NavApp::getWebController()->openPage();
}

void MainWindow::debugDumpContainerSizes() const
{
  qDebug() << Q_FUNC_INFO << "======================================";
  if(windReporter != nullptr)
    windReporter->debugDumpContainerSizes();
  if(weatherReporter != nullptr)
    weatherReporter->debugDumpContainerSizes();
  if(NavApp::getOnlinedataController() != nullptr)
    NavApp::getOnlinedataController()->debugDumpContainerSizes();
  if(NavApp::getConnectClient() != nullptr)
    NavApp::getConnectClient()->debugDumpContainerSizes();
  qDebug() << Q_FUNC_INFO << "======================================";
}
