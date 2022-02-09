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

#include "gui/mainwindow.h"

#include "airspace/airspacecontroller.h"
#include "atools.h"
#include "common/constants.h"
#include "common/dirtool.h"
#include "common/elevationprovider.h"
#include "common/mapcolors.h"
#include "common/settingsmigrate.h"
#include "common/unit.h"
#include "connect/connectclient.h"
#include "db/databasemanager.h"
#include "exception.h"
#include "fs/common/morareader.h"
#include "fs/perf/aircraftperf.h"
#include "fs/pln/flightplanio.h"
#include "gui/application.h"
#include "gui/choicedialog.h"
#include "gui/clicktooltiphandler.h"
#include "gui/dialog.h"
#include "gui/dockwidgethandler.h"
#include "gui/errorhandler.h"
#include "gui/filehistoryhandler.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/statusbareventfilter.h"
#include "gui/stylehandler.h"
#include "gui/tabwidgethandler.h"
#include "gui/timedialog.h"
#include "gui/translator.h"
#include "gui/widgetstate.h"
#include "info/infocontroller.h"
#include "logbook/logdatacontroller.h"
#include "logging/loggingguiabort.h"
#include "logging/logginghandler.h"
#include "mapgui/aprongeometrycache.h"
#include "mapgui/imageexportdialog.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/maplayersettings.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapwidget.h"
#include "mapgui/mapthemehandler.h"
#include "navapp.h"
#include "online/onlinedatacontroller.h"
#include "options/optionsdialog.h"
#include "perf/aircraftperfcontroller.h"
#include "print/printsupport.h"
#include "profile/profilewidget.h"
#include "query/airportquery.h"
#include "query/procedurequery.h"
#include "route/routealtitude.h"
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
#include "weather/weatherreporter.h"
#include "weather/windreporter.h"
#include "web/webcontroller.h"
#include "web/webmapcontroller.h"

#include <marble/LegendWidget.h>
#include <marble/MarbleAboutDialog.h>
#include <marble/MarbleModel.h>
#include <marble/HttpDownloadManager.h>

#include <QDebug>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QScreen>
#include <QWindow>
#include <QDesktopWidget>
#include <QDir>
#include <QFileInfoList>
#include <QSslSocket>
#include <QEvent>
#include <QMimeData>
#include <QClipboard>
#include <QProgressDialog>
#include <QThread>
#include <QStringBuilder>

#include "ui_mainwindow.h"

static const int WEATHER_UPDATE_MS = 15000;

static const int MAX_STATUS_MESSAGES = 10;

using namespace Marble;
using atools::settings::Settings;
using atools::gui::FileHistoryHandler;
using atools::gui::MapPosHistory;
using atools::gui::HelpHandler;

MainWindow::MainWindow()
  : QMainWindow(nullptr), ui(new Ui::MainWindow)
{
  qDebug() << Q_FUNC_INFO << "constructor";

  aboutMessage =
    QObject::tr("<p style='white-space:pre'>is a free open source flight planner, navigation tool, moving map,<br/>"
                "airport search and airport information system<br/>"
                "for X-Plane 11, Flight Simulator X, Prepar3D and Microsoft Flight Simulator 2020.</p>"
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
                  "<a href=\"https://github.com/albar965\">Github"
                  "</a>."
                "</p>"
                "<p>More about my projects at "
                  "<a href=\"https://www.littlenavmap.org\">www.littlenavmap.org"
                  "</a>."
                "</p>"
                "<p>"
                  "<b>Copyright 2015-2021 Alexander Barthel"
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
    ui->dockWidgetRouteCalc->hide();
    ui->labelProfileInfo->hide();

    setAcceptDrops(true);
    defaultToolbarIconSize = iconSize();

    // Show tooltips also for inactive windows (e.g. if a floating window is active)
    setAttribute(Qt::WA_AlwaysShowToolTips);

    // Create misc GUI handlers ============================================
    dialog = new atools::gui::Dialog(this);
    errorHandler = new atools::gui::ErrorHandler(this);
    helpHandler = new atools::gui::HelpHandler(this, aboutMessage, GIT_REVISION);

    // Create dock and mainwindow handler ============================================
    atools::settings::Settings& settings = atools::settings::Settings::instance();
    dockHandler =
      new atools::gui::DockWidgetHandler(this,
                                         // Add all available dock widgets here ==========================
                                         {ui->dockWidgetLegend, ui->dockWidgetAircraft,
                                          ui->dockWidgetSearch, ui->dockWidgetProfile,
                                          ui->dockWidgetInformation, ui->dockWidgetRoute,
                                          ui->dockWidgetRouteCalc},
                                         // Add all available toolbars  here =============================
                                         {ui->toolBarMain, ui->toolBarMap, ui->toolbarMapOptions,
                                          ui->toolBarRoute, ui->toolBarView, ui->toolBarAirspaces,
                                          ui->toolBarMapThemeProjection, ui->toolBarTools},
                                         settings.getAndStoreValue(lnm::OPTIONS_DOCKHANDLER_DEBUG, false).toBool());

    marbleAboutDialog = new Marble::MarbleAboutDialog(this);
    marbleAboutDialog->setApplicationTitle(QApplication::applicationName());

    currentWeatherContext = new map::WeatherContext;

    routeExport = new RouteExport(this);
    simbriefHandler = new SimBriefHandler(this);

    qDebug() << Q_FUNC_INFO << "Creating OptionsDialog";
    optionsDialog = new OptionsDialog(this);
    // Has to load the state now so options are available for all controller and manager classes
    optionsDialog->restoreState();
    optionsChanged();

    // Dialog is opened with asynchronous open()
    connect(optionsDialog, &QDialog::finished, this, [ = ](int result) {
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
    mapcolors::syncColors();

    Unit::init();

    map::updateUnits();

    // Remember original title
    mainWindowTitle = windowTitle();

    // Prepare database and queries
    qDebug() << Q_FUNC_INFO << "Creating DatabaseManager";

    NavApp::init(this);

    NavApp::getStyleHandler()->insertMenuItems(ui->menuWindowStyle);
    NavApp::getStyleHandler()->restoreState();
    mapcolors::init();

    // Add actions for flight simulator database switch in main menu
    NavApp::getDatabaseManager()->insertSimSwitchActions();

    qDebug() << Q_FUNC_INFO << "Creating WeatherReporter";
    weatherReporter = new WeatherReporter(this, NavApp::getCurrentSimulatorDb());

    qDebug() << Q_FUNC_INFO << "Creating WindReporter";
    windReporter = new WindReporter(this, NavApp::getCurrentSimulatorDb());
    windReporter->addToolbarButton();

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for flight plans";
    routeFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMES_RECENT, ui->menuRecentRoutes,
                                              ui->actionRecentRoutesClear);

    qDebug() << Q_FUNC_INFO << "Creating RouteController";
    routeController = new RouteController(this, ui->tableViewRoute);

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for KML files";
    kmlFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMESKML_RECENT, ui->menuRecentKml,
                                            ui->actionClearKmlMenu);

    qDebug() << Q_FUNC_INFO << "Creating FileHistoryHandler for layout files";
    layoutFileHistory = new FileHistoryHandler(this, lnm::LAYOUT_RECENT, ui->menuWindowLayoutRecent,
                                               ui->actionWindowLayoutClearRecent);

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

    // Fill theme combo box and menus after setting up map widget
    setupMapThemesUi();

    // Init a few late objects since these depend on the map widget instance
    NavApp::initQueries();
    NavApp::initElevationProvider();

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

    // Add user defined points toolbar button and submenu items
    NavApp::getUserdataController()->addToolbarButton();

    qDebug() << Q_FUNC_INFO << "Reading settings";
    restoreStateMain();

    // Update window states based on actions
    allowDockingWindows();
    allowMovingWindows();

    updateActionStates();
    updateMarkActionStates();
    updateHighlightActionStates();

    NavApp::getAirspaceController()->updateButtonsAndActions();
    updateOnlineActionStates();

    qDebug() << Q_FUNC_INFO << "Setting theme";
    changeMapTheme();

    qDebug() << Q_FUNC_INFO << "Setting projection";
    mapWidget->setProjection(mapProjectionComboBox->currentData().toInt());

    // Wait until everything is set up and update map
    updateMapObjectsShown();

    profileWidget->updateProfileShowFeatures();

    loadNavmapLegend();
    updateLegend();
    updateWindowTitle();

    // Enable or disable tooltips - call later since it needs the map window
    optionsDialog->updateTooltipOption();

    // Update clock every second =====================
    clockTimer.setInterval(1000);
    connect(&clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer.start();

    // Reset render status - change to done after ten seconds =====================
    renderStatusTimer.setInterval(5000);
    renderStatusTimer.setSingleShot(true);
    connect(&renderStatusTimer, &QTimer::timeout, this, &MainWindow::renderStatusReset);

    qDebug() << Q_FUNC_INFO << "Constructor done";
  }
  // Exit application if something goes wrong
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
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

  ui->menuHelp->addSeparator();
  ui->menuHelp->addAction(debugAction1);
  ui->menuHelp->addAction(debugAction2);
  ui->menuHelp->addAction(debugAction3);
  ui->menuHelp->addAction(debugAction4);
  ui->menuHelp->addAction(debugAction5);
  ui->menuHelp->addAction(debugAction6);
  ui->menuHelp->addAction(debugAction7);

  connect(debugAction1, &QAction::triggered, this, &MainWindow::debugActionTriggered1);
  connect(debugAction2, &QAction::triggered, this, &MainWindow::debugActionTriggered2);
  connect(debugAction3, &QAction::triggered, this, &MainWindow::debugActionTriggered3);
  connect(debugAction4, &QAction::triggered, this, &MainWindow::debugActionTriggered4);
  connect(debugAction5, &QAction::triggered, this, &MainWindow::debugActionTriggered5);
  connect(debugAction6, &QAction::triggered, this, &MainWindow::debugActionTriggered6);
  connect(debugAction7, &QAction::triggered, this, &MainWindow::debugActionTriggered7);

#endif

}

MainWindow::~MainWindow()
{
  qDebug() << Q_FUNC_INFO;

  clockTimer.stop();

  NavApp::setShuttingDown(true);

  weatherUpdateTimer.stop();

  // Close all queries
  preDatabaseLoad();

  qDebug() << Q_FUNC_INFO << "delete routeController";
  delete routeController;
  qDebug() << Q_FUNC_INFO << "delete searchController";
  delete searchController;
  qDebug() << Q_FUNC_INFO << "delete weatherReporter";
  delete weatherReporter;
  qDebug() << Q_FUNC_INFO << "delete windReporter";
  delete windReporter;
  qDebug() << Q_FUNC_INFO << "delete profileWidget";
  delete profileWidget;
  qDebug() << Q_FUNC_INFO << "delete marbleAbout";
  delete marbleAboutDialog;
  qDebug() << Q_FUNC_INFO << "delete infoController";
  delete infoController;
  qDebug() << Q_FUNC_INFO << "delete printSupport";
  delete printSupport;
  qDebug() << Q_FUNC_INFO << "delete routeFileHistory";
  delete routeFileHistory;
  qDebug() << Q_FUNC_INFO << "delete kmlFileHistory";
  delete kmlFileHistory;
  qDebug() << Q_FUNC_INFO << "delete layoutFileHistory";
  delete layoutFileHistory;
  qDebug() << Q_FUNC_INFO << "delete optionsDialog";
  delete optionsDialog;
  qDebug() << Q_FUNC_INFO << "delete mapWidget";
  delete mapWidget;
  qDebug() << Q_FUNC_INFO << "delete ui";
  delete ui;
  qDebug() << Q_FUNC_INFO << "delete dialog";
  delete dialog;
  qDebug() << Q_FUNC_INFO << "delete errorHandler";
  delete errorHandler;
  qDebug() << Q_FUNC_INFO << "delete helpHandler";
  delete helpHandler;
  qDebug() << Q_FUNC_INFO << "delete dockHandler";
  delete dockHandler;
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapProjection";
  delete actionGroupMapProjection;
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapTheme";
  delete actionGroupMapTheme;
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapSunShading";
  delete actionGroupMapSunShading;
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapWeatherSource";
  delete actionGroupMapWeatherSource;
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapWeatherWindSource";
  delete actionGroupMapWeatherWindSource;
  qDebug() << Q_FUNC_INFO << "delete currentWeatherContext";
  delete currentWeatherContext;
  qDebug() << Q_FUNC_INFO << "delete routeExport";
  delete routeExport;
  qDebug() << Q_FUNC_INFO << "delete simbriefHandler";
  delete simbriefHandler;

  qDebug() << Q_FUNC_INFO << "NavApplication::deInit()";
  NavApp::deInit();

  qDebug() << Q_FUNC_INFO << "Unit::deInit()";
  Unit::deInit();

  // Delete settings singleton
  qDebug() << Q_FUNC_INFO << "Settings::shutdown()";

  if(NavApp::isRestartProcess())
    Settings::clearAndShutdown();
  else
    Settings::shutdown();

  // Free translations
  atools::gui::Translator::unload();

  atools::logging::LoggingGuiAbortHandler::resetGuiAbortFunction();

  qDebug() << Q_FUNC_INFO << "destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();
}

#ifdef DEBUG_INFORMATION

void MainWindow::debugActionTriggered1()
{
  qDebug() << "======================================================================================";
  qDebug() << Q_FUNC_INFO;
  qDebug() << NavApp::getRouteConst();
  qDebug() << "======================================================================================";

  qDebug() << Q_FUNC_INFO << ui->dockWidgetRouteCalc->windowFlags().testFlag(Qt::WindowStaysOnTopHint);

}

void MainWindow::debugActionTriggered2()
{
  qDebug() << "======================================================================================";
  qDebug() << Q_FUNC_INFO;
  qDebug() << NavApp::getRouteConst().getFlightplan();
  qDebug() << "======================================================================================";
}

void MainWindow::debugActionTriggered3()
{
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(), false /* manually triggered */,
                          true /* forceDebug */);
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

#endif

void MainWindow::updateMap() const
{
  mapWidget->update();
}

void MainWindow::updateClock() const
{
  timeLabel->setText(QDateTime::currentDateTimeUtc().toString("d   HH:mm:ss Z"));
  timeLabel->setToolTip(tr("Day of month and UTC time.\n%1\nLocal: %2")
                        .arg(QDateTime::currentDateTimeUtc().toString())
                        .arg(QDateTime::currentDateTime().toString()));
}

/* Show map legend and bring information dock to front */
void MainWindow::showNavmapLegend()
{
  if(!legendFile.isEmpty() && QFile::exists(legendFile))
  {
    dockHandler->activateWindow(ui->dockWidgetLegend);
    ui->tabWidgetLegend->setCurrentIndex(0);
    setStatusMessage(tr("Opened navigation map legend."));
  }
  else
  {
    // URL is empty loading failed - show it in browser
    helpHandler->openHelpUrlWeb(this, lnm::helpOnlineLegendUrl, lnm::helpLanguageOnline());
    setStatusMessage(tr("Opened map legend in browser."));
  }
}

/* Load the navmap legend into the text browser */
void MainWindow::loadNavmapLegend()
{
  qDebug() << Q_FUNC_INFO;

  legendFile = HelpHandler::getHelpFile(lnm::helpLegendLocalFile, OptionData::instance().getLanguage());
  qDebug() << "legendUrl" << legendFile;

  QFile legend(legendFile);
  if(legend.open(QIODevice::ReadOnly))
  {
    QTextStream stream(&legend);
    stream.setCodec("UTF-8");
    QString legendText = stream.readAll();

#ifdef DEBUG_INFORMATION_LEGEND
    qDebug() << Q_FUNC_INFO << "==========================================";
    qDebug().noquote().nospace() << legendText;
    qDebug() << Q_FUNC_INFO << "==========================================";
#endif

    QString searchPath = QCoreApplication::applicationDirPath() + QDir::separator() + "help";
    ui->textBrowserLegendNavInfo->setSearchPaths({searchPath});
    ui->textBrowserLegendNavInfo->setText(legendText);
  }
  else
  {
    qWarning() << "Error opening legend" << legendFile << legend.errorString();
    legendFile.clear();
  }
}

/* Check manually for updates as triggered by the action */
void MainWindow::checkForUpdates()
{
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(),
                          true /* manually triggered */, false /* forceDebug */);
}

void MainWindow::showOnlineHelp()
{
  HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl, lnm::helpLanguageOnline());
}

void MainWindow::showOnlineTutorials()
{
  HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineTutorialsUrl, lnm::helpLanguageOnline());
}

void MainWindow::showOnlineDownloads()
{
  HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineDownloadsUrl, lnm::helpLanguageOnline());
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

/* Show marble legend */
void MainWindow::showMapLegend()
{
  dockHandler->activateWindow(ui->dockWidgetLegend);
  ui->tabWidgetLegend->setCurrentIndex(1);
  setStatusMessage(tr("Opened map legend."));
}

/* User clicked "show in browser" in legend */
void MainWindow::legendAnchorClicked(const QUrl& url)
{
  if(url.scheme() == "lnm" && url.host() == "legend")
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "LEGEND.html", lnm::helpLanguageOnline());
  else
    HelpHandler::openUrl(this, url);

  setStatusMessage(tr("Opened legend link in browser."));
}

void MainWindow::scaleToolbar(QToolBar *toolbar, float scale)
{
  QSizeF size = toolbar->iconSize();
  size *= scale;
  toolbar->setIconSize(size.toSize());
}

void MainWindow::setupMapThemesUi()
{
  // Theme combo box ============================
  comboBoxMapTheme = new QComboBox(this);
  comboBoxMapTheme->setObjectName("comboBoxMapTheme");
  comboBoxMapTheme->setToolTip(tr("Select map theme"));
  comboBoxMapTheme->setStatusTip(comboBoxMapTheme->toolTip());
  ui->toolBarMapThemeProjection->addWidget(comboBoxMapTheme);

  // Theme menu items ===============================
  actionGroupMapTheme = new QActionGroup(ui->menuViewTheme);
  actionGroupMapTheme->setObjectName("actionGroupMapTheme");

  bool online = true;
  int index = 0;
  // Sort order is always online/offline and then alphabetical
  for(const MapTheme& theme : mapWidget->getMapThemeHandler()->getThemes())
  {
    // Check if offline map come after online and add separators
    if(!theme.isOnline() && online)
    {
      // Add separator between online and offline maps
      ui->menuViewTheme->addSeparator();
      comboBoxMapTheme->insertSeparator(comboBoxMapTheme->count());
    }

    // Add item to combo box in toolbar
    QString name = theme.isOnline() ? theme.getName() : tr("%1 (offline)").arg(theme.getName());
    if(theme.hasKeys())
      // Add star to maps which require an API key or token
      name += tr(" *");

    // Build tooltip for entries
    QStringList tip;
    tip.append(theme.getName());
    tip.append(theme.isOnline() ? tr("online") : tr("offline"));
    tip.append(theme.hasKeys() ? tr("* requires registration") : tr("free"));

    // Add item and attach index for theme in MapThemeHandler
    comboBoxMapTheme->addItem(name, index);
    comboBoxMapTheme->setItemData(index, tip.join(tr(", ")), Qt::ToolTipRole);

    // Create action for map/theme submenu
    QAction *action = ui->menuViewTheme->addAction(name);
    action->setCheckable(true);
    action->setToolTip(tip.join(tr(", ")));
    action->setStatusTip(action->toolTip());
    action->setActionGroup(actionGroupMapTheme);

    // Add keyboard shortcut for top 10 themes
    if(index < 10)
      action->setShortcut(tr("Ctrl+Alt+%1").arg(index));

    // Attach index for theme in MapThemeHandler
    action->setData(index);

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << name << index;
#endif

    index++;

    // Remember theme online status
    online = theme.isOnline();
  }
}

void MainWindow::setupUi()
{
  // Reduce large icons on mac
#if defined(Q_OS_MACOS)
  scaleToolbar(ui->toolBarMain, 0.72f);
  scaleToolbar(ui->toolBarMap, 0.72f);
  scaleToolbar(ui->toolbarMapOptions, 0.72f);
  scaleToolbar(ui->toolBarMapThemeProjection, 0.72f);
  scaleToolbar(ui->toolBarRoute, 0.72f);
  scaleToolbar(ui->toolBarAirspaces, 0.72f);
  scaleToolbar(ui->toolBarView, 0.72f);
#endif

  // Projection combo box
  mapProjectionComboBox = new QComboBox(this);
  mapProjectionComboBox->setObjectName("mapProjectionComboBox");
  QString helpText = tr("Select map projection");
  mapProjectionComboBox->setToolTip(helpText);
  mapProjectionComboBox->setStatusTip(helpText);
  mapProjectionComboBox->addItem(tr("Mercator"), Marble::Mercator);
  mapProjectionComboBox->addItem(tr("Spherical"), Marble::Spherical);
  mapProjectionComboBox->setItemData(0, tr("Flat map projection"), Qt::ToolTipRole);
  mapProjectionComboBox->setItemData(1, tr("Map projection showing a globe"), Qt::ToolTipRole);

  ui->toolBarMapThemeProjection->addWidget(mapProjectionComboBox);

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
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindNOAA);
  actionGroupMapWeatherWindSource->addAction(ui->actionMapShowWindSimulator);

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

  ui->dockWidgetRouteCalc->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/routecalcdock.svg"));
  ui->dockWidgetRouteCalc->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+3")));
  ui->dockWidgetRouteCalc->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                          arg(ui->dockWidgetRouteCalc->windowTitle()));
  ui->dockWidgetRouteCalc->toggleViewAction()->setStatusTip(ui->dockWidgetRouteCalc->toggleViewAction()->toolTip());

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

  ui->dockWidgetLegend->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/legenddock.svg"));
  ui->dockWidgetLegend->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+7")));
  ui->dockWidgetLegend->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                       arg(ui->dockWidgetLegend->windowTitle()));
  ui->dockWidgetLegend->toggleViewAction()->setStatusTip(ui->dockWidgetLegend->toggleViewAction()->toolTip());

  // Connect to methods internally
  dockHandler->connectDockWindows();

  // Add dock actions to main menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->dockWidgetSearch->toggleViewAction(),
                               ui->dockWidgetRoute->toggleViewAction(),
                               ui->dockWidgetRouteCalc->toggleViewAction(),
                               ui->dockWidgetInformation->toggleViewAction(),
                               ui->dockWidgetProfile->toggleViewAction(),
                               ui->dockWidgetAircraft->toggleViewAction(),
                               ui->dockWidgetLegend->toggleViewAction()});

  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->toolBarMain->toggleViewAction(),
                               ui->toolBarMap->toggleViewAction(),
                               ui->toolbarMapOptions->toggleViewAction(),
                               ui->toolBarMapThemeProjection->toggleViewAction(),
                               ui->toolBarRoute->toggleViewAction(),
                               ui->toolBarAirspaces->toggleViewAction(),
                               ui->toolBarView->toggleViewAction(),
                               ui->toolBarTools->toggleViewAction()});
  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to toolbar
  ui->toolBarView->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetRouteCalc->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetInformation->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetProfile->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetAircraft->toggleViewAction());
  ui->toolBarView->addAction(ui->dockWidgetLegend->toggleViewAction());

  // ==============================================================
  // Create labels for the statusbar
  connectStatusLabel = new QLabel();
  connectStatusLabel->setAlignment(Qt::AlignCenter);
  connectStatusLabel->setMinimumWidth(140);
  connectStatusLabel->setText(tr("Not connected."));
  connectStatusLabel->setToolTip(tr("Simulator connection status."));
  ui->statusBar->addPermanentWidget(connectStatusLabel);

  messageLabel = new QLabel();
  messageLabel->setAlignment(Qt::AlignCenter);
  messageLabel->setMinimumWidth(150);
  ui->statusBar->addPermanentWidget(messageLabel);

  detailLabel = new QLabel();
  detailLabel->setAlignment(Qt::AlignCenter);
  detailLabel->setMinimumWidth(120);
  detailLabel->setToolTip(tr("Map detail level."));
  ui->statusBar->addPermanentWidget(detailLabel);

  renderStatusLabel = new QLabel();
  renderStatusLabel->setAlignment(Qt::AlignCenter);
  renderStatusLabel->setMinimumWidth(140);
  renderStatusLabel->setToolTip(tr("Map rendering and download status."));
  ui->statusBar->addPermanentWidget(renderStatusLabel);

  mapDistanceLabel = new QLabel();
  mapDistanceLabel->setAlignment(Qt::AlignCenter);
  mapDistanceLabel->setMinimumWidth(60);
  mapDistanceLabel->setToolTip(tr("Map view distance to ground."));
  ui->statusBar->addPermanentWidget(mapDistanceLabel);

  mapPosLabel = new QLabel();
  mapPosLabel->setAlignment(Qt::AlignCenter);
  mapPosLabel->setMinimumWidth(240);
  mapPosLabel->setToolTip(tr("Coordinates and elevation at cursor position."));
  ui->statusBar->addPermanentWidget(mapPosLabel);

  magvarLabel = new QLabel();
  magvarLabel->setAlignment(Qt::AlignCenter);
  magvarLabel->setMinimumWidth(40);
  magvarLabel->setToolTip(tr("Magnetic declination at cursor position."));
  ui->statusBar->addPermanentWidget(magvarLabel);

  timeLabel = new QLabel();
  timeLabel->setAlignment(Qt::AlignCenter);
#ifdef Q_OS_MACOS
  timeLabel->setMinimumWidth(60);
#else
  timeLabel->setMinimumWidth(55);
#endif
  timeLabel->setToolTip(tr("Day of month and UTC time."));
  ui->statusBar->addPermanentWidget(timeLabel);

  // Status bar takes ownership of filter which handles tooltip on click
  ui->statusBar->installEventFilter(new StatusBarEventFilter(ui->statusBar, connectStatusLabel));

  connect(ui->statusBar, &QStatusBar::messageChanged, this, &MainWindow::statusMessageChanged);

  // Show error messages in tooltip on click ========================================
  ui->labelRouteError->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelRouteError));
}

void MainWindow::clearProcedureCache()
{
  NavApp::getProcedureQuery()->clearCache();
}

void MainWindow::connectAllSlots()
{

  // Get "show in browser"  click
  connect(ui->textBrowserLegendNavInfo, &QTextBrowser::anchorClicked, this,
          &MainWindow::legendAnchorClicked);

  // Options dialog ===================================================================
  // Notify others of options change
  // The units need to be called before all others
  connect(optionsDialog, &OptionsDialog::optionsChanged, &Unit::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, &NavApp::optionsChanged);

  // Need to clean cache to regenerate some text if units have changed
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::clearProcedureCache);

  // Reset weather context first
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::clearWeatherContext);

  connect(optionsDialog, &OptionsDialog::optionsChanged,
          NavApp::getAirspaceController(), &AirspaceController::optionsChanged);

  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateMapObjectsShown);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateActionStates);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::distanceChanged);

  connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getMapAirportHandler(), &MapAirportHandler::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, weatherReporter, &WeatherReporter::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, windReporter, &WindReporter::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, searchController, &SearchController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, map::updateUnits);
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

  // Updated manually in dialog
  // connect(optionsDialog, &OptionsDialog::optionsChanged, NavApp::getWebController(), &WebController::optionsChanged);

  // Style handler ===================================================================
  // Save complete state due to crashes in Qt
  AircraftPerfController *perfController = NavApp::getAircraftPerfController();

  connect(NavApp::getStyleHandler(), &StyleHandler::preStyleChange, this, &MainWindow::saveStateNow);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, mapcolors::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, infoController, &InfoController::optionsChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, routeController, &RouteController::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, searchController, &SearchController::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, mapWidget, &MapPaintWidget::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, profileWidget, &ProfileWidget::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, perfController, &AircraftPerfController::optionsChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, NavApp::getInfoController(), &InfoController::styleChanged);
  connect(NavApp::getStyleHandler(), &StyleHandler::styleChanged, optionsDialog, &OptionsDialog::styleChanged);

  // WindReporter ===================================================================================
  // Wind has to be calculated first - receive routeChanged signal first
  connect(routeController, &RouteController::routeChanged, windReporter, &WindReporter::updateManualRouteWinds);
  connect(routeController, &RouteController::routeChanged, windReporter, &WindReporter::updateToolButtonState);
  connect(perfController, &AircraftPerfController::aircraftPerformanceChanged,
          windReporter, &WindReporter::updateToolButtonState);

  // Aircraft performance signals =======================================================
  connect(perfController, &AircraftPerfController::aircraftPerformanceChanged, routeController,
          &RouteController::aircraftPerformanceChanged);
  connect(perfController, &AircraftPerfController::windChanged, routeController, &RouteController::windUpdated);
  connect(perfController, &AircraftPerfController::windChanged, profileWidget, &ProfileWidget::windUpdated);
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

  connect(routeController, &RouteController::showProcedures, searchController->getProcedureSearch(),
          &ProcedureSearch::showProcedures);

  // Update rubber band in map window if user hovers over profile
  connect(profileWidget, &ProfileWidget::highlightProfilePoint, mapWidget, &MapPaintWidget::changeProfileHighlight);
  connect(profileWidget, &ProfileWidget::showPos, mapWidget, &MapPaintWidget::showPos);

  connect(routeController, &RouteController::routeChanged, profileWidget, &ProfileWidget::routeChanged);
  connect(routeController, &RouteController::routeAltitudeChanged, profileWidget, &ProfileWidget::routeAltitudeChanged);
  connect(routeController, &RouteController::routeChanged, this, &MainWindow::updateActionStates);
  connect(routeController, &RouteController::routeInsert, this, &MainWindow::routeInsert);
  connect(routeController, &RouteController::addAirportMsa, mapWidget, &MapWidget::addMsaMark);

  connect(routeController, &RouteController::routeChanged, NavApp::updateErrorLabels);
  connect(routeController, &RouteController::routeChanged, NavApp::updateWindowTitle);
  connect(routeController, &RouteController::routeAltitudeChanged, NavApp::updateErrorLabels);

  // Add departure and dest runway actions separately to windows since their shortcuts overlap with context menu shortcuts
  QList<QAction *> actions({ui->actionShowDepartureCustom, ui->actionShowApproachCustom});
  mapWidget->addActions(actions);
  ui->dockWidgetInformation->addActions(actions);
  ui->dockWidgetAircraft->addActions(actions);
  ui->dockWidgetProfile->addActions(actions);
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
  connect(airportSearch, &SearchBaseTable::showProcedures,
          searchController->getProcedureSearch(), &ProcedureSearch::showProcedures);
  connect(airportSearch, &SearchBaseTable::showCustomApproach, routeController, &RouteController::showCustomApproach);
  connect(airportSearch, &SearchBaseTable::showCustomDeparture, routeController, &RouteController::showCustomDeparture);
  connect(airportSearch, &SearchBaseTable::routeSetDeparture, routeController, &RouteController::routeSetDeparture);
  connect(airportSearch, &SearchBaseTable::routeSetDestination, routeController, &RouteController::routeSetDestination);
  connect(airportSearch, &SearchBaseTable::routeAddAlternate, routeController, &RouteController::routeAddAlternate);
  connect(airportSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);
  connect(airportSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);
  connect(airportSearch, &SearchBaseTable::addAirportMsa, mapWidget, &MapWidget::addMsaMark);

  // Nav search ===================================================================================
  NavSearch *navSearch = searchController->getNavSearch();
  connect(navSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(navSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(navSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(navSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);
  connect(navSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);
  connect(navSearch, &SearchBaseTable::addAirportMsa, mapWidget, &MapWidget::addMsaMark);

  // Userdata search ===================================================================================
  UserdataSearch *userSearch = searchController->getUserdataSearch();
  connect(userSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(userSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(userSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(userSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);
  connect(userSearch, &SearchBaseTable::routeAdd, routeController, &RouteController::routeAdd);

  // Logbook search ===================================================================================
  LogdataSearch *logSearch = searchController->getLogdataSearch();
  connect(logSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(logSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(logSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(logSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);
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
  connect(logdataController, &LogdataController::logDataChanged, infoController,
          &InfoController::updateAllInformation);

  connect(mapWidget, &MapWidget::aircraftTakeoff, logdataController, &LogdataController::aircraftTakeoff);
  connect(mapWidget, &MapWidget::aircraftLanding, logdataController, &LogdataController::aircraftLanding);

  connect(logdataController, &LogdataController::showInSearch, searchController, &SearchController::showInSearch);

  connect(ui->actionLogdataShowStatistics, &QAction::triggered, logdataController, &LogdataController::showStatistics);
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
  connect(clientSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);

  // Online center search ===================================================================================
  connect(centerSearch, &SearchBaseTable::showRect, mapWidget, &MapPaintWidget::showRect);
  connect(centerSearch, &SearchBaseTable::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(centerSearch, &SearchBaseTable::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(centerSearch, &SearchBaseTable::showInformation, infoController, &InfoController::showInformation);
  connect(centerSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);

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
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated,
          mapWidget, &MapPaintWidget::onlineClientAndAtcUpdated);
  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged,
          mapWidget, &MapPaintWidget::onlineNetworkChanged);

  // Update info
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated,
          infoController, &InfoController::onlineClientAndAtcUpdated);
  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged,
          infoController, &InfoController::onlineNetworkChanged);

  connect(onlinedataController, &OnlinedataController::onlineNetworkChanged,
          NavApp::getAirspaceController(), &AirspaceController::updateButtonsAndActions);
  connect(onlinedataController, &OnlinedataController::onlineClientAndAtcUpdated,
          NavApp::getAirspaceController(), &AirspaceController::updateButtonsAndActions);

  connect(clientSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);
  connect(centerSearch, &SearchBaseTable::selectionChanged, this, &MainWindow::searchSelectionChanged);

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
  connect(ui->actionResetAllSettings, &QAction::triggered, this, &MainWindow::resetAllSettings);

  connect(infoController, &InfoController::showPos, mapWidget, &MapPaintWidget::showPos);
  connect(infoController, &InfoController::showRect, mapWidget, &MapPaintWidget::showRect);

  // Use this event to show scenery library dialog on first start after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);

  // Scenery library menu ============================================================
  connect(ui->actionLoadAirspaces, &QAction::triggered, NavApp::getAirspaceController(), &AirspaceController::loadAirspaces);
  connect(ui->actionReloadScenery, &QAction::triggered, NavApp::getDatabaseManager(), &DatabaseManager::run);
  connect(ui->actionDatabaseFiles, &QAction::triggered, this, &MainWindow::showDatabaseFiles);

  connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::openOptionsDialog);
  connect(ui->actionResetMessages, &QAction::triggered, this, &MainWindow::resetMessages);
  connect(ui->actionSaveAllNow, &QAction::triggered, this, &MainWindow::saveStateNow);

  // Windows menu ============================================================
  connect(ui->actionShowFloatingWindows, &QAction::triggered, this, &MainWindow::raiseFloatingWindows);
  connect(ui->actionWindowStayOnTop, &QAction::toggled, this, &MainWindow::stayOnTop);
  connect(ui->actionShowAllowDocking, &QAction::toggled, this, &MainWindow::allowDockingWindows);
  connect(ui->actionShowAllowMoving, &QAction::toggled, this, &MainWindow::allowMovingWindows);
  connect(ui->actionShowFullscreenMap, &QAction::toggled, this, &MainWindow::fullScreenMapToggle);

  // File menu ============================================================
  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);

  // Flight plan file actions ============================================================
  connect(ui->actionRouteResetAll, &QAction::triggered, this, &MainWindow::routeResetAll);

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
  connect(ui->actionRouteSaveAsPln, &QAction::triggered, this, [ = ]() { routeExport->routeExportPlnMan(); });
  connect(ui->actionRouteSaveAsPlnMsfs, &QAction::triggered, this, [ = ]() { routeExport->routeExportPlnMsfsMan(); });
  connect(ui->actionRouteSaveAsFms11, &QAction::triggered, this, [ = ]() { routeExport->routeExportFms11Man(); });
  connect(ui->actionRouteSaveAsFlightGear, &QAction::triggered, this, [ = ]() { routeExport->routeExportFlightgearMan(); });
  connect(ui->actionRouteSaveAll, &QAction::triggered, this, [ = ]() { routeExport->routeMultiExport(); });
  connect(ui->actionRouteSaveAllOptions, &QAction::triggered, this, [ = ]() { routeExport->routeMultiExportOptions(); });

  connect(ui->actionRouteSaveAsGpx, &QAction::triggered, this, [ = ]() { routeExport->routeExportGpxMan(); });
  connect(ui->actionRouteSaveAsHtml, &QAction::triggered, this, [ = ]() { routeExport->routeExportHtmlMan(); });

  // Online export options
  connect(ui->actionRouteSaveAsVfp, &QAction::triggered, this, [ = ]() { routeExport->routeExportVfpMan(); });
  connect(ui->actionRouteSaveAsIvap, &QAction::triggered, this, [ = ]() { routeExport->routeExportIvapMan(); });
  connect(ui->actionRouteSaveAsXIvap, &QAction::triggered, this, [ = ]() { routeExport->routeExportXIvapMan(); });
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
  connect(ui->actionRouteCalc, &QAction::triggered, routeController, &RouteController::calculateRouteWindowFull);
  connect(ui->actionRouteReverse, &QAction::triggered, routeController, &RouteController::reverseRoute);
  connect(ui->actionRouteCopyString, &QAction::triggered, routeController, &RouteController::routeStringToClipboard);
  connect(ui->actionRouteAdjustAltitude, &QAction::triggered, routeController, &RouteController::adjustFlightplanAltitude);

  // Help menu ========================================================================
  connect(ui->actionHelpContents, &QAction::triggered, this, &MainWindow::showOnlineHelp);
  connect(ui->actionHelpTutorials, &QAction::triggered, this, &MainWindow::showOnlineTutorials);
  connect(ui->actionHelpContentsOffline, &QAction::triggered, this, &MainWindow::showOfflineHelp);
  connect(ui->actionHelpDownloads, &QAction::triggered, this, &MainWindow::showOnlineDownloads);
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
  connect(mapWidget, &MapWidget::addUserpointFromMap, NavApp::getUserdataController(), &UserdataController::addUserpointFromMap);
  connect(mapWidget, &MapWidget::editUserpointFromMap, NavApp::getUserdataController(), &UserdataController::editUserpointFromMap);
  connect(mapWidget, &MapWidget::deleteUserpointFromMap, NavApp::getUserdataController(), &UserdataController::deleteUserpointFromMap);
  connect(mapWidget, &MapWidget::moveUserpointFromMap, NavApp::getUserdataController(), &UserdataController::moveUserpointFromMap);

  connect(mapWidget, &MapWidget::editLogEntryFromMap, NavApp::getLogdataController(), &LogdataController::editLogEntryFromMap);
  connect(mapWidget, &MapWidget::exitFullScreenPressed, this, &MainWindow::exitFullScreenPressed);

  connect(mapWidget, &MapWidget::routeInsertProcedure, routeController, &RouteController::routeAddProcedure);

  // Connect toolbar combo boxes
  connect(mapProjectionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeMapProjection);
  connect(comboBoxMapTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::changeMapTheme);

  // Let projection menus update combo boxes
  connect(ui->actionMapProjectionMercator, &QAction::triggered, this, [ = ](bool checked)
  {
    if(checked)
      mapProjectionComboBox->setCurrentIndex(0);
  });

  connect(ui->actionMapProjectionSpherical, &QAction::triggered, this, [ = ](bool checked)
  {
    if(checked)
      mapProjectionComboBox->setCurrentIndex(1);
  });

  // Window menu ======================================
  connect(layoutFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::layoutOpenRecent);
  connect(ui->actionWindowLayoutOpen, &QAction::triggered, this, &MainWindow::layoutOpen);
  connect(ui->actionWindowLayoutSaveAs, &QAction::triggered, this, &MainWindow::layoutSaveAs);

  // Let theme menus update combo boxes
  for(QAction *action : actionGroupMapTheme->actions())
    connect(action, &QAction::triggered, this, &MainWindow::themeMenuTriggered);

  // Map object/feature display
  connect(ui->actionMapShowMinimumAltitude, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAirportWeather, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowCities, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowGrid, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVor, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowNdb, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWp, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowIls, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowGls, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowHolding, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAirportMsa, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVictorAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowJetAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowTracks, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowRoute, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowTocTod, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapHideRangeRings, &QAction::triggered, this, &MainWindow::clearRangeRingsAndDistanceMarkers);

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
  connect(ui->actionMapShowAircraft, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftAi, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftAiBoat, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionShowAirspaces, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapResetSettings, &QAction::triggered, this, &MainWindow::resetMapObjectsShown);

  // Airspace sources ======
  AirspaceController *airspaceController = NavApp::getAirspaceController();
  connect(airspaceController, &AirspaceController::updateAirspaceSources, this, &MainWindow::updateMapObjectsShown);
  connect(airspaceController, &AirspaceController::updateAirspaceSources,
          NavApp::getAirspaceController(), &AirspaceController::updateButtonsAndActions);
  connect(airspaceController, &AirspaceController::updateAirspaceSources, this, &MainWindow::updateAirspaceSources);

  // Other airspace signals ======
  connect(airspaceController, &AirspaceController::updateAirspaceTypes, this, &MainWindow::updateAirspaceTypes);
  connect(airspaceController, &AirspaceController::userAirspacesUpdated,
          NavApp::getOnlinedataController(), &OnlinedataController::userAirspacesUpdated);

  // Connect airspace manger signals to database manager signals
  connect(airspaceController, &AirspaceController::preDatabaseLoadAirspaces,
          NavApp::getDatabaseManager(), &DatabaseManager::preDatabaseLoad);
  connect(airspaceController, &AirspaceController::postDatabaseLoadAirspaces,
          NavApp::getDatabaseManager(), &DatabaseManager::postDatabaseLoad);

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
  connect(ui->actionMapShowWeatherSimulator, &QAction::toggled, weatherReporter,
          &WeatherReporter::updateAirportWeather);
  connect(ui->actionMapShowWeatherActiveSky, &QAction::toggled, weatherReporter,
          &WeatherReporter::updateAirportWeather);
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
  connect(ui->actionMapShowAircraftAiBoat, &QAction::toggled, infoController, &InfoController::updateAllInformation);

  // Order is important here. First let the mapwidget delete the track then notify the profile
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered, this, &MainWindow::deleteAircraftTrack);

  connect(ui->actionMapShowMark, &QAction::triggered, mapWidget, &MapWidget::showSearchMark);
  connect(ui->actionMapShowHome, &QAction::triggered, mapWidget, &MapWidget::showHome);
  connect(ui->actionMapJumpCoordinatesMain, &QAction::triggered, mapWidget, &MapWidget::jumpCoordinates);
  connect(ui->actionMapAircraftCenter, &QAction::toggled, mapWidget, &MapPaintWidget::showAircraft);
  connect(ui->actionMapAircraftCenterNow, &QAction::triggered, mapWidget, &MapPaintWidget::showAircraftNow);

  // Update jump back
  connect(ui->actionMapAircraftCenter, &QAction::toggled, mapWidget, &MapPaintWidget::jumpBackToAircraftCancel);

  connect(ui->actionMapBack, &QAction::triggered, mapWidget, &MapWidget::historyBack);
  connect(ui->actionMapNext, &QAction::triggered, mapWidget, &MapWidget::historyNext);

  connect(ui->actionMapMoreDetails, &QAction::triggered, mapWidget, &MapWidget::increaseMapDetail);
  connect(ui->actionMapLessDetails, &QAction::triggered, mapWidget, &MapWidget::decreaseMapDetail);
  connect(ui->actionMapDefaultDetails, &QAction::triggered, mapWidget, &MapWidget::defaultMapDetail);

  connect(mapWidget->getHistory(), &MapPosHistory::historyChanged, this, &MainWindow::updateMapHistoryActions);

  connect(routeController, &RouteController::routeSelectionChanged, this, &MainWindow::routeSelectionChanged);

  connect(ui->actionRouteSelectParking, &QAction::triggered, routeController, &RouteController::selectDepartureParking);

  // Route editing
  connect(mapWidget, &MapWidget::routeSetStart, routeController, &RouteController::routeSetDeparture);
  connect(mapWidget, &MapWidget::routeSetParkingStart, routeController, &RouteController::routeSetParking);
  connect(mapWidget, &MapWidget::routeSetHelipadStart, routeController, &RouteController::routeSetHelipad);
  connect(mapWidget, &MapWidget::routeSetDest, routeController, &RouteController::routeSetDestination);
  connect(mapWidget, &MapWidget::routeAddAlternate, routeController, &RouteController::routeAddAlternate);
  connect(mapWidget, &MapWidget::routeAdd, routeController, &RouteController::routeAdd);
  connect(mapWidget, &MapWidget::routeReplace, routeController, &RouteController::routeReplace);

  // Messages about database query result status
  connect(mapWidget, &MapPaintWidget::resultTruncated, this, &MainWindow::resultTruncated);

  connect(NavApp::getDatabaseManager(), &DatabaseManager::preDatabaseLoad, this, &MainWindow::preDatabaseLoad);
  connect(NavApp::getDatabaseManager(), &DatabaseManager::postDatabaseLoad, this, &MainWindow::postDatabaseLoad);

  // Not needed. All properties removed from legend since they are not persistent
  // connect(legendWidget, &Marble::LegendWidget::propertyValueChanged,
  // mapWidget, &MapWidget::setPropertyValue);

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

  connect(connectClient, &ConnectClient::dataPacketReceived,
          NavApp::getAircraftPerfController(), &AircraftPerfController::simDataChanged);
  connect(connectClient, &ConnectClient::connectedToSimulator,
          NavApp::getAircraftPerfController(), &AircraftPerfController::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          NavApp::getAircraftPerfController(), &AircraftPerfController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator, routeController,
          &RouteController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator, this, &MainWindow::sunShadingTimeChanged);

  // Map widget needs to clear track first
  connect(connectClient, &ConnectClient::connectedToSimulator, mapWidget, &MapPaintWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, mapWidget,
          &MapPaintWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, this, &MainWindow::updateActionStates);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, this, &MainWindow::updateActionStates);

  connect(connectClient, &ConnectClient::connectedToSimulator, infoController, &InfoController::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, infoController,
          &InfoController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, profileWidget, &ProfileWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, profileWidget,
          &ProfileWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator, NavApp::updateErrorLabels);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator, NavApp::updateErrorLabels);

  connect(connectClient, &ConnectClient::aiFetchOptionsChanged, this, &MainWindow::updateActionStates);

  connect(mapWidget, &MapPaintWidget::aircraftTrackPruned, profileWidget, &ProfileWidget::aircraftTrackPruned);

  // Weather update ===================================================
  connect(weatherReporter, &WeatherReporter::weatherUpdated, mapWidget, &MapWidget::updateTooltip);
  connect(weatherReporter, &WeatherReporter::weatherUpdated, infoController, &InfoController::updateAirportWeather);
  connect(weatherReporter, &WeatherReporter::weatherUpdated, mapWidget, &MapPaintWidget::weatherUpdated);

  connect(connectClient, &ConnectClient::weatherUpdated, mapWidget, &MapPaintWidget::weatherUpdated);
  connect(connectClient, &ConnectClient::weatherUpdated, mapWidget, &MapWidget::updateTooltip);
  connect(connectClient, &ConnectClient::weatherUpdated, infoController, &InfoController::updateAirportWeather);

  // Wind update ===================================================
  connect(windReporter, &WindReporter::windUpdated, routeController, &RouteController::windUpdated);
  connect(windReporter, &WindReporter::windUpdated, profileWidget, &ProfileWidget::windUpdated);
  connect(windReporter, &WindReporter::windUpdated, perfController, &AircraftPerfController::updateReports);
  connect(windReporter, &WindReporter::windUpdated, this, &MainWindow::updateMapObjectsShown);
  connect(windReporter, &WindReporter::windUpdated, this, &MainWindow::updateActionStates);

  // Legend ===============================================
  connect(ui->actionHelpNavmapLegend, &QAction::triggered, this, &MainWindow::showNavmapLegend);
  connect(ui->actionHelpMapLegend, &QAction::triggered, this, &MainWindow::showMapLegend);

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
          NavApp::getDatabaseManager(), &DatabaseManager::checkForChangedNavAndSimDatabases);
}

void MainWindow::actionShortcutMapTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(OptionData::instance().getFlags2() & opts2::MAP_ALLOW_UNDOCK)
  {
    ui->dockWidgetMap->show();
    ui->dockWidgetMap->activateWindow();
    dockHandler->raiseFloatingWindow(ui->dockWidgetMap);
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
  ui->lineEditAirportIcaoSearch->setFocus();
  ui->lineEditAirportIcaoSearch->selectAll();
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
  qDebug() << Q_FUNC_INFO;
  dockHandler->activateWindow(ui->dockWidgetRouteCalc);

  // Place window near cursor for first time show to avoid Qt positioning it randomly elsewhere
  if(!Settings::instance().valueBool(lnm::MAINWINDOW_PLACE_ROUTE_CALC, false))
  {
    Settings::instance().setValue(lnm::MAINWINDOW_PLACE_ROUTE_CALC, true);
    ui->dockWidgetRouteCalc->move(QCursor::pos() + QPoint(20, 20));
  }
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

void MainWindow::themeMenuTriggered(bool checked)
{
  QAction *action = dynamic_cast<QAction *>(sender());
  if(action != nullptr && checked)
  {
    // Get index in MapThemeHandler
    int index = action->data().toInt();

    for(int i = 0; i < comboBoxMapTheme->count(); i++)
    {
      if(comboBoxMapTheme->itemData(i).isValid() && comboBoxMapTheme->itemData(i).toInt() == index)
      {
        // Use signal from combo box to activate theme
        comboBoxMapTheme->setCurrentIndex(i);
        break;
      }
    }
  }
}

/* Called by the toolbar combo box */
void MainWindow::changeMapProjection(int index)
{
  Q_UNUSED(index)

  mapWidget->cancelDragAll();

  Marble::Projection proj = static_cast<Marble::Projection>(mapProjectionComboBox->currentData().toInt());
  qDebug() << "Changing projection to" << proj;
  mapWidget->setProjection(proj);

  // Update menu items
  ui->actionMapProjectionMercator->setChecked(proj == Marble::Mercator);
  ui->actionMapProjectionSpherical->setChecked(proj == Marble::Spherical);

  setStatusMessage(tr("Map projection changed to %1.").arg(mapProjectionComboBox->currentText()));
}

/* Called by the toolbar combo box */
void MainWindow::changeMapTheme()
{
  mapWidget->cancelDragAll();
  const MapThemeHandler *mapThemeHandler = mapWidget->getMapThemeHandler();

  int index = comboBoxMapTheme->currentData().toInt();
  MapTheme theme = mapThemeHandler->getTheme(index);

  if(!theme.isValid())
  {
    qDebug() << Q_FUNC_INFO << "Falling back to default theme due to invalid index" << index;
    // No theme for index found - use default OSM
    theme = mapThemeHandler->getDefaultTheme();
    index = theme.getIndex();

    // Search for combo box entry with index for MapThemeHandler
    for(int i = 0; i < comboBoxMapTheme->count(); i++)
    {
      if(comboBoxMapTheme->itemData(i).isValid() && comboBoxMapTheme->itemData(i).toInt() == index)
      {
        // Avoid recursion by blocking signals
        comboBoxMapTheme->blockSignals(true);
        comboBoxMapTheme->setCurrentIndex(i);
        comboBoxMapTheme->blockSignals(false);
        break;
      }
    }
  }

  // Check if theme needs API keys, usernames or tokens ======================================
  if(theme.hasKeys())
  {
    // Get all available key/value pairs from handler
    QMap<QString, QString> mapThemeKeys = mapThemeHandler->getMapThemeKeys();

    // Check if all required values are set
    bool allValid = true;
    for(const QString& key : theme.getKeys())
    {
      if(mapThemeKeys.value(key).isEmpty())
      {
        allValid = false;
        break;
      }
    }

    if(!allValid)
    {
      // One or more keys are not present or empty - show info dialog =================================
      NavApp::closeSplashScreen();

      // Fetch all keys for map theme
      QString url;
      if(!theme.getUrlRef().isEmpty())
        url = tr("<p>Click here to create an account: <a href=\"%1\">%2</a></p>").
              arg(theme.getUrlRef()).arg(theme.getUrlName().isEmpty() ? tr("Link") : theme.getUrlName());

      dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_MAPTHEME_REQUIRES_KEY,
                             tr("<p>The map theme \"%1\" requires additional information.</p>"
                                  "<p>You have to create an user account at the related website and then create an username, an access key or a token.<br/>"
                                  "Most of these services offer a free plan for hobbyists.</p>"
                                  "<p>Then go to menu \"Tools\" -> \"Options\" and to page \"Map Display Keys\" in Little Navmap and "
                                    "enter the information for the key(s) below:</p>"
                                    "<ul><li>%2</li></ul>"
                                      "<p>The map will not show correctly until this is done.</p>%3").
                             arg(theme.getName()).arg(theme.getKeys().join(tr("</li><li>"))).arg(url),
                             tr("Do not &show this dialog again."));
    }
  }

  qDebug() << Q_FUNC_INFO << "index" << index << theme;

  mapWidget->setTheme(theme.getId(), index);

  // Update menu items in group
  for(QAction *action : actionGroupMapTheme->actions())
  {
    action->blockSignals(true);
    action->setChecked(action->data() == index);
    action->blockSignals(false);
  }

  updateLegend();

  setStatusMessage(tr("Map theme changed to %1.").arg(comboBoxMapTheme->currentText()));
}

void MainWindow::updateLegend()
{
  Marble::LegendWidget *legendWidget = new Marble::LegendWidget(this);
  legendWidget->setMarbleModel(mapWidget->model());
  QString basePath;
  QString html = legendWidget->getHtml(basePath);
  ui->textBrowserLegendInfo->setSearchPaths({basePath});
  ui->textBrowserLegendInfo->setText(html);
  delete legendWidget;
}

/* Menu item */
void MainWindow::showDatabaseFiles()
{
  QUrl url = QUrl::fromLocalFile(NavApp::getDatabaseManager()->getDatabaseDirectory());

  if(!QDesktopServices::openUrl(url))
    atools::gui::Dialog::warning(this, tr("Error opening help URL \"%1\"").arg(url.toDisplayString()));
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
}

/* Updates label and tooltip for objects shown on map */
void MainWindow::setMapObjectsShownMessageText(const QString& text, const QString& tooltipText)
{
  messageLabel->setText(text);
  messageLabel->setToolTip(tooltipText);
}

const ElevationModel *MainWindow::getElevationModel()
{
  return mapWidget->model()->elevationModel();
}

/* Called after each query */
void MainWindow::resultTruncated()
{
  messageLabel->setText(atools::util::HtmlBuilder::errorMessage(tr("Too many objects")));
  messageLabel->setToolTip(tr("Too many objects to show on map.\n"
                              "Display might be incomplete.\n"
                              "Reduce map details in the \"View\" menu.",
                              "Keep menu item in sync with menu translation"));
}

void MainWindow::distanceChanged()
{
  // #ifdef DEBUG_INFORMATION
  // qDebug() << Q_FUNC_INFO << "minimumZoom" << mapWidget->minimumZoom() << "maximumZoom" << mapWidget->maximumZoom()
  // << "step" << mapWidget->zoomStep() << "distance" << mapWidget->distance() << "zoom" << mapWidget->zoom();
  // #endif
  float dist = Unit::distMeterF(static_cast<float>(mapWidget->distance() * 1000.f));
  QString distStr = QLocale().toString(dist, 'f', dist < 20.f ? (dist < 0.2f ? 2 : 1) : 0);
  if(distStr.endsWith(QString(QLocale().decimalPoint()) + "0"))
    distStr.chop(2);

  QString text = distStr + " " + Unit::getUnitDistStr();

#ifdef DEBUG_INFORMATION
  text += QString("[%1km][%2z]").arg(mapWidget->distance(), 0, 'f', 2).arg(mapWidget->zoom());
#endif

  mapDistanceLabel->setText(text);
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
        renderStatusLabel->setText(tr("Done"));
        break;
      case Marble::WaitingForUpdate:
        renderStatusLabel->setText(tr("Updating"));
        break;
      case Marble::WaitingForData:
        renderStatusLabel->setText(tr("Loading"));
        break;
      case Marble::Incomplete:
        renderStatusLabel->setText(tr("Incomplete"));
        break;
    }
    lastRenderStatus = status;
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

void MainWindow::routeResetAll()
{
  enum Choice
  {
    EMPTY_FLIGHT_PLAN,
    DELETE_TRAIL,
    DELETE_ACTIVE_LEG,
    RESTART_PERF,
    RESTART_LOGBOOK,
    REMOVE_MARKS
  };

  qDebug() << Q_FUNC_INFO;

  // Create a dialog with four checkboxes
  atools::gui::ChoiceDialog choiceDialog(this, QApplication::applicationName() + tr(" - Reset for new Flight"),
                                         tr("Select items to reset for a new flight"), lnm::RESET_FOR_NEW_FLIGHT_DIALOG, "RESET.html");
  choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

  choiceDialog.addCheckBox(EMPTY_FLIGHT_PLAN, tr("&Create a new and empty flight plan"), QString(), true);
  choiceDialog.addCheckBox(DELETE_TRAIL, tr("&Delete aircraft trail"),
                           tr("Delete simulator aircraft trail from map and elevation profile"), true);
  choiceDialog.addCheckBox(DELETE_ACTIVE_LEG, tr("&Reset active flight plan leg"),
                           tr("Remove the active (magenta) flight plan leg"), true);
  choiceDialog.addCheckBox(RESTART_PERF, tr("Restart Aircraft &Performance Collection"),
                           tr("Restarts the background aircraft performance collection"), true);
  choiceDialog.addCheckBox(RESTART_LOGBOOK, tr("Reset flight detection in &logbook"),
                           tr("Reset the logbook to detect takeoff and landing for new logbook entries"), true);
  choiceDialog.addCheckBox(REMOVE_MARKS, tr("&Remove all Ranges, Measurements, Patterns, Holdings and MSA Diagrams"),
                           tr("Remove all range rings, measurements, traffic patterns, holdings and "
                              "airport MSA diagrams from the map"), false);
  choiceDialog.addSpacer();

  choiceDialog.restoreState();

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    if(choiceDialog.isChecked(EMPTY_FLIGHT_PLAN))
      routeNew();
    if(choiceDialog.isChecked(DELETE_TRAIL))
      deleteAircraftTrack(true /* do not ask questions */);
    if(choiceDialog.isChecked(DELETE_ACTIVE_LEG))
      NavApp::getRouteController()->resetActiveLeg();
    if(choiceDialog.isChecked(RESTART_PERF))
      NavApp::getAircraftPerfController()->restartCollection(true /* do not ask questions */);
    if(choiceDialog.isChecked(RESTART_LOGBOOK))
    {
      NavApp::getLogdataController()->resetTakeoffLandingDetection();
      mapWidget->resetTakeoffLandingDetection();
    }
    if(choiceDialog.isChecked(REMOVE_MARKS))
      clearRangeRingsAndDistanceMarkers(true /* quiet */);
  }
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

void MainWindow::updateMapPosLabel(const atools::geo::Pos& pos, int x, int y)
{
  Q_UNUSED(x)
  Q_UNUSED(y)

  if(pos.isValid())
  {
    QString text(Unit::coords(pos));

    if(NavApp::getElevationProvider()->isGlobeOfflineProvider() && pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      text += tr(" / ") + Unit::altMeter(pos.getAltitude());

    float magVar = NavApp::getMagVar(pos);
    QString magVarText = map::magvarText(magVar, true /* short text */);
#ifdef DEBUG_INFORMATION
    magVarText = QString("%1 [%2]").arg(magVarText).arg(magVar, 0, 'f', 2);
#endif

    magvarLabel->setText(magVarText);

#ifdef DEBUG_INFORMATION
    text.append(QString(" [%1,%2]").arg(x).arg(y));
#endif

    mapPosLabel->setText(text);
  }
  else
  {
    mapPosLabel->setText(tr("No position"));
    magvarLabel->clear();
  }
}

/* Updates main window title with simulator type, flight plan name and change status */
void MainWindow::updateWindowTitle()
{
  QString newTitle = mainWindowTitle;
  dm::NavdatabaseStatus navDbStatus = NavApp::getDatabaseManager()->getNavDatabaseStatus();

  atools::util::Version version(NavApp::applicationVersion());

  // Program version and revision ==========================================
  if(version.isStable() || version.isReleaseCandidate() || version.isBeta())
    newTitle += tr(" %1").arg(version.getVersionString());
  else
    newTitle += tr(" %1 (%2)").arg(version.getVersionString()).arg(GIT_REVISION);

  // Database information  ==========================================
  // Simulator database =========
  if(navDbStatus == dm::NAVDATABASE_ALL)
    newTitle += tr(" - (%1)").arg(NavApp::getCurrentSimulatorShortName());
  else
  {
    newTitle += tr(" - %1").arg(NavApp::getCurrentSimulatorShortName());

    if(!NavApp::getDatabaseAiracCycleSim().isEmpty())
      newTitle += tr(" %1").arg(NavApp::getDatabaseAiracCycleSim());
  }

  // Nav database =========
  if(navDbStatus == dm::NAVDATABASE_ALL)
    newTitle += tr(" / N");
  else if(navDbStatus == dm::NAVDATABASE_MIXED)
    newTitle += tr(" / N");
  else if(navDbStatus == dm::NAVDATABASE_OFF)
    newTitle += tr(" / (N)");

  if((navDbStatus == dm::NAVDATABASE_ALL || navDbStatus == dm::NAVDATABASE_MIXED) &&
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

#ifndef QT_NO_DEBUG
  newTitle += " - DEBUG";
#endif

  // Add a star to the flight plan tab if changed
  routeController->updateRouteTabChangedStatus();

  setWindowTitle(newTitle);
}

void MainWindow::deleteAircraftTrack(bool quiet)
{
  int result = QMessageBox::Yes;

  if(!quiet)
    result = atools::gui::Dialog(this).
             showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_TRAIL,
                                tr("Delete aircraft trail?"),
                                tr("Do not &show this dialog again."),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    mapWidget->deleteAircraftTrack();
    profileWidget->deleteAircraftTrack();
    updateActionStates();
    setStatusMessage(QString(tr("Aircraft track removed from map.")));
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
  msgBox.setText(routeController->getRoute().isEmpty() ?
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

/* Open a dialog that allows to create a new route from a string */
void MainWindow::routeFromStringCurrent()
{
  routeFromStringInternal(QString());
}

void MainWindow::routeFromString(const QString& routeString)
{
  routeFromStringInternal(routeString);
}

void MainWindow::routeFromStringInternal(const QString& routeString)
{
  qDebug() << Q_FUNC_INFO;
  RouteStringDialog routeStringDialog(this, routeString);
  routeStringDialog.restoreState();

  if(routeStringDialog.exec() == QDialog::Accepted)
  {
    if(!routeStringDialog.getFlightplan().isEmpty())
      routeFromFlightplan(routeStringDialog.getFlightplan(), !routeStringDialog.isAltitudeIncluded() /* adjustAltitude */);
    else
      qWarning() << "Flight plan is null";
  }
  routeStringDialog.saveState();
}

void MainWindow::routeFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude)
{
  qDebug() << Q_FUNC_INFO;

  if(routeCheckForChanges())
  {
    routeController->loadFlightplan(flightplan, atools::fs::pln::LNM_PLN, QString(), true /*quiet*/, true /*changed*/, adjustAltitude);
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
  return dialog->openFileDialog(
    tr("Open Flight Plan"),
    tr("Flight Plan Files %1;;All Files (*)").
    arg(lnm::FILE_PATTERN_FLIGHTPLAN_LOAD),
    "Route/LnmPln", atools::documentsDir());
}

void MainWindow::routeOpenFile(QString filepath)
{
  if(routeCheckForChanges())
  {
    if(filepath.isEmpty())
      filepath = routeOpenFileDialog();

    if(!filepath.isEmpty())
    {
      if(routeController->loadFlightplan(filepath))
      {
        routeFileHistory->addFile(filepath);
        if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
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
      if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
        routeCenter();
      showFlightPlan();
      setStatusMessage(tr("Flight plan opened."));
    }
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
                                         routeController->getRoute().getDestinationAirportLegIndex() + 1 /* append */))
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
  if(atools::almostEqual(NavApp::getRouteConst().getCruisingAltitudeFeet(), 0.f, 10.f))
    atools::gui::Dialog(this).showInfoMsgBox(lnm::ACTIONS_SHOW_CRUISE_ZERO_WARNING,
                                             tr("Flight plan cruise altitude is zero.\n"
                                                "A simulator might not be able to load the flight plan."),
                                             QObject::tr("Do not &show this dialog again."));

  if(!routeController->isLnmFormatFlightplan())
  {
    // Forbid saving of other formats than LNMPLN directly =========================================
    atools::gui::DialogButtonList buttonList =
    {
      {QString(), QMessageBox::Cancel},
      {tr("Save &as LNMPLN ..."), QMessageBox::Save},
      {QString(), QMessageBox::Help}
    };

    // Ask before saving file
    int result =
      dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_SAVE_WARNING,
                                 tr("<p>You cannot save this file directly.<br/>"
                                    "Use the export function instead.</p>"
                                    "<p>Save using the LNMPLN format now?</p>"),
                                 tr("Do not &show this dialog again and save as LNMPLN."),
                                 buttonList, QMessageBox::Cancel, QMessageBox::Save);

    if(result == QMessageBox::Cancel)
      return false;
    else if(result == QMessageBox::Help)
    {
      atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "FLIGHTPLANFMT.html", lnm::helpLanguageOnline());
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

  HelpHandler::openUrlWeb(this, "https://skyvector.com/?fpl=" + route);
  return true;
}

void MainWindow::clearRangeRingsAndDistanceMarkers(bool quiet)
{
  if(!quiet)
  {
    int result = atools::gui::Dialog(this).
                 showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_MARKS,
                                    tr("Delete all range rings, measurement lines, traffic patterns and holds from map?"),
                                    tr("Do not &show this dialog again."),
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No, QMessageBox::Yes);

    if(result == QMessageBox::Yes)
      mapWidget->clearAllMarkers();
  }
  else
    mapWidget->clearAllMarkers();
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
        progress.setLabelText(label.arg(numSeconds) + tr("%1 downloads active and %2 downloads queued.").
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

        if(routeController->getRoute().isEmpty())
          defaultFileName = tr("LittleNavmap_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmm"));
        else
          defaultFileName = NavApp::getRouteConst().buildDefaultFilenameShort("_", ".jpg");

        int filterIndex = -1;

        QString imageFile = dialog->saveFileDialog(
          tr("Save Map as Image for AviTab"),
          tr("JPG Image Files (*.jpg *.jpeg);;PNG Image Files (*.png);;All Files (*)"),
          "jpg", "MainWindow/AviTab",
          NavApp::getCurrentSimulatorBasePath() +
          QDir::separator() + "Resources" + QDir::separator() + "plugins" + QDir::separator() + "AviTab" +
          QDir::separator() + "MapTiles" + QDir::separator() + "Mercator", defaultFileName,
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
            QFile jsonFile(imageFile + ".json");
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

/* Selection in one of the search result tables has changed */
void MainWindow::searchSelectionChanged(const SearchBaseTable *source, int selected, int visible, int total)
{
  bool updateAirspace = false, updateLogEntries = false;
  QString selectionLabelText = tr("%1 of %2 %3 selected, %4 visible.%5");
  QString type;
  if(source->getTabIndex() == si::SEARCH_AIRPORT)
  {
    type = tr("Airports");
    ui->labelAirportSearchStatus->setText(selectionLabelText.
                                          arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_NAV)
  {
    type = tr("Navaids");
    ui->labelNavSearchStatus->setText(selectionLabelText.
                                      arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_USER)
  {
    type = tr("Userpoints");
    ui->labelUserdata->setText(selectionLabelText.
                               arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_LOG)
  {
    updateLogEntries = true;
    type = tr("Logbook Entries");
    ui->labelLogdata->setText(selectionLabelText.
                              arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_ONLINE_CLIENT)
  {
    type = tr("Clients");
    QString lastUpdate = tr(" Last Update: %1").
                         arg(NavApp::getOnlinedataController()->getLastUpdateTime().toString(Qt::DefaultLocaleShortDate));
    ui->labelOnlineClientSearchStatus->setText(selectionLabelText.
                                               arg(selected).arg(total).arg(type).arg(visible).
                                               arg(lastUpdate));
  }
  else if(source->getTabIndex() == si::SEARCH_ONLINE_CENTER)
  {
    updateAirspace = true;
    type = tr("Centers");
    QString lastUpdate = tr(" Last Update: %1").
                         arg(NavApp::getOnlinedataController()->getLastUpdateTime().toString(Qt::DefaultLocaleShortDate));
    ui->labelOnlineCenterSearchStatus->setText(selectionLabelText.
                                               arg(selected).arg(total).arg(type).arg(visible).
                                               arg(lastUpdate));
  }

  map::MapResult result;
  searchController->getSelectedMapObjects(result);
  mapWidget->changeSearchHighlights(result, updateAirspace, updateLogEntries);
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
    qDebug() << Q_FUNC_INFO << "approachId" << ref.approachId << "transitionId" << ref.transitionId << "legId" << ref.legId;
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
      if(ref.hasApproachAndTransitionIds())
      {
        // Load transition including approach
        const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(airport, ref.transitionId);
        if(legs != nullptr)
          procedures.append(*legs);
        else
          qWarning() << "Transition not found" << ref.transitionId;
      }
      else if(ref.hasApproachOnlyIds())
      {
        proc::MapProcedureRef curRef;
        if(!previewAll)
          // Only one procedure in preview - try to keep transition if user moved selection to approach leg
          curRef = mapWidget->getProcedureHighlight().ref;

        if(ref.airportId == curRef.airportId && ref.approachId == curRef.approachId &&
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
          const proc::MapProcedureLegs *legs = procedureQuery->getApproachLegs(airport, ref.approachId);
          if(legs != nullptr)
            procedures.append(*legs);
          else
            qWarning() << "Approach not found" << ref.approachId;
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
  qDebug() << Q_FUNC_INFO << "approachId" << ref.approachId << "transitionId" << ref.transitionId << "legId" << ref.legId;
#endif

  if(ref.legId != -1)
  {
    map::MapAirport airport = NavApp::getAirportQueryNav()->getAirportById(ref.airportId);
    if(ref.transitionId != -1)
      leg = procedureQuery->getTransitionLeg(airport, ref.legId);
    else
      leg = procedureQuery->getApproachLeg(airport, ref.approachId, ref.legId);
  }

  if(leg != nullptr)
    mapWidget->changeProcedureLegHighlight(*leg);
  else
    mapWidget->changeProcedureLegHighlight(proc::MapProcedureLeg());

  updateHighlightActionStates();
}

void MainWindow::updateAirspaceTypes(map::MapAirspaceFilter types)
{
  mapWidget->setShowMapAirspaces(types);
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

  mapWidget->updateMapObjectsShown();

  mapWidget->update();
  profileWidget->update();

  setStatusMessage(tr("Map settings reset."));
}

/* A button like airport, vor, ndb, etc. was pressed - update the map */
void MainWindow::updateMapObjectsShown()
{
  // Save to configuration
  // saveActionStates();

  mapWidget->updateMapObjectsShown();
  profileWidget->update();
  updateActionStates();
  // setStatusMessage(tr("Map settings changed."));
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
  optionsDialog->open();
}

/* Reset all "do not show this again" message box status values */
void MainWindow::resetMessages()
{
  qDebug() << "resetMessages";
  Settings& settings = Settings::instance();

  // Show all message dialogs again
  settings.setValue(lnm::ACTIONS_SHOW_DISCONNECT_INFO, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOAD_FLP_WARN, true);
  settings.setValue(lnm::ACTIONS_SHOW_QUIT, true);
  settings.setValue(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_RESET_VIEW, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_PARKING_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_WARNING_MULTI, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_ERROR, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_ALTERNATE_ERROR, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_START_CHANGED, true);
  settings.setValue(lnm::OPTIONS_DIALOG_WARN_STYLE, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOAD_FMS_ALT_WARN, true);
  settings.setValue(lnm::ACTIONS_SHOW_SAVE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_SAVE_LNMPLN_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_ZOOM_WARNING, true);
  settings.setValue(lnm::ACTIONS_OFFLINE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_UPDATE_FAILED, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_FAILED, true);
  settings.setValue(lnm::ACTIONS_SHOW_OVERWRITE_DATABASE, true);
  settings.setValue(lnm::ACTIONS_SHOW_NAVDATA_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_CRUISE_ZERO_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOWROUTE_NO_CYCLE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_INSTALL_GLOBE, true);
  settings.setValue(lnm::ACTIONS_SHOW_INSTALL_DIRS, true);
  settings.setValue(lnm::ACTIONS_SHOW_START_PERF_COLLECTION, true);
  settings.setValue(lnm::ACTIONS_SHOW_DELETE_TRAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_DELETE_MARKS, true);
  settings.setValue(lnm::ACTIONS_SHOW_RESET_PERF, true);
  settings.setValue(lnm::ACTIONS_SHOW_SEARCH_CENTER_NULL, true);
  settings.setValue(lnm::ACTIONS_SHOW_WEATHER_DOWNLOAD_FAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_SUCCESS, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOGBOOK_CONVERSION, true);
  settings.setValue(lnm::ACTIONS_SHOW_USER_AIRSPACE_NOTE, true);
  settings.setValue(lnm::ACTIONS_SHOW_SEND_SIMBRIEF, true);
  settings.setValue(lnm::ACTIONS_SHOW_MAPTHEME_REQUIRES_KEY, true);
  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_OLD, true);

  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_ONLINE, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_WIND, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_TRACK, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_WEATHER, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_SIMBRIEF, true);

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
    ui->statusBar->setToolTip(msg.join(tr("<br/>")) + tr("</p>"));
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
  detailLabel->setText(text);
}

/* Called by window shown event when the main window is visible the first time */
void MainWindow::mainWindowShown()
{
  qDebug() << Q_FUNC_INFO << "enter";

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

    QString message = QObject::tr("<p>Error initializing SSL subsystem.</p>"
                                    "<p>The program will not be able to use encrypted network connections<br/>"
                                    "(i.e. HTTPS) that are needed to check for updates or<br/>"
                                    "to load online maps.</p>");

#if defined(Q_OS_WIN32)
    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineInstallRedistUrl, lnm::helpLanguageOnline());
    QString message2 = QObject::tr("<p><b>Click the link below for more information:<br/><br/>"
                                   "<a href=\"%1\">Online Manual - Installation</a></b><br/></p>").
                       arg(url.toString());
    message += message2;
#endif

    atools::gui::Dialog(this).showWarnMsgBox(lnm::ACTIONS_SHOW_SSL_FAILED, message,
                                             QObject::tr("Do not &show this dialog again."));
  }

  if(!NavApp::getElevationProvider()->isGlobeOfflineProvider())
  {
    NavApp::closeSplashScreen();

    // Text from options
    // <p><a href="https://www.ngdc.noaa.gov/mgg/topo/gltiles.html"><b>Click here to open the download page for the GLOBE data in your browser</b></a><br/>
    // Download the file <b><i>All Tiles in One .zip (all10g.zip)</i></b> from the page and extract
    // the archive to an arbitrary place, e.g in \"Documents\". Then click \"Select GLOBE Directory ...\"
    // above and choose the directory with the extracted files.</p>
    // <p><a href="%1"><b>Click here for more information in the <i>Little Navmap</i> online manual</b></a></p>

    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineInstallGlobeUrl, lnm::helpLanguageOnline());
    QString message = QObject::tr(
      "<p>The online elevation data which is used by default for the elevation profile "
        "is limited and has a lot of errors.<br/>"
        "Therefore, it is recommended to download and use the offline GLOBE elevation data "
        "which provides world wide coverage.</p>"
        "<p><b>Go to the main menu -&gt; \"Tools\" -&gt; \"Options\" and "
          "then to page \"Cache and files\" to add the GLOBE data.</b></p>"
          "<p><a href=\"%1\"><b>Click here for more information in the "
            "<i>Little Navmap</i> online manual</b></a></p>",
      "Keep instructions in sync with translated menus")
                      .arg(url.toString());

    atools::gui::Dialog(this).showInfoMsgBox(lnm::ACTIONS_SHOW_INSTALL_GLOBE, message,
                                             QObject::tr("Do not &show this dialog again."));
  }

  // Create recommended folder structure if user confirms
  DirTool dirTool(this, atools::documentsDir(), QApplication::applicationName(), lnm::ACTIONS_SHOW_INSTALL_DIRS);
  if(!dirTool.hasAllDirs())
  {
    NavApp::closeSplashScreen();
    dirTool.run();
  }

  DatabaseManager *databaseManager = NavApp::getDatabaseManager();
  if(firstApplicationStart)
  {
    firstApplicationStart = false;

    // Open a start page in the web browser ============================
    helpHandler->openHelpUrlWeb(this, lnm::helpOnlineStartUrl, lnm::helpLanguageOnline());

    if(!databaseManager->hasSimulatorDatabases())
    {
      NavApp::closeSplashScreen();

      // Show the scenery database dialog on first start
#ifdef Q_OS_WIN32
      if(databaseManager->hasInstalledSimulators())
        // No databases but simulators let the user create new databases
        databaseManager->run();
      else
      {
        // No databases and no simulators - show a message dialog
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QApplication::applicationName());
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(
          tr("Could not find a<ul>"
             "<li>Microsoft Flight Simulator X,</li>"
               "<li>Microsoft Flight Simulator - Steam Edition,</li>"
                 "<li>Prepar3D or</li></ul>"
                   "<li>Microsoft Flight Simulator 2020 installation</li>"
                     "on this computer. Also, no scenery library databases were found.<br/><br/>"
                     "You can copy a Little Navmap scenery library database from another computer.<br/>"
                     "Press the help button for more information on this.<br/><br/>"
                     "If you have X-Plane 11 installed you can go to the scenery library "
                     "loading dialog by clicking the X-Plane button below.<br/><br/>"
             )
          );
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Open | QMessageBox::Help);
        msgBox.setButtonText(QMessageBox::Open, tr("X-Plane"));
        msgBox.setDefaultButton(QMessageBox::Ok);

        int result = msgBox.exec();
        if(result == QMessageBox::Help)
          HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "RUNNOSIM.html", lnm::helpLanguageOnline());
        else if(result == QMessageBox::Open)
          databaseManager->run();
      }
#else
      // Show always on non Windows systems ========================
      databaseManager->run();
#endif
    } // else have databases do nothing

    // Open connection dialog ============================
    NavApp::closeSplashScreen();
    NavApp::getConnectClient()->connectToServerDialog();
  }
  else if(databasesErased)
  {
    databasesErased = false;
    // Databases were removed - show dialog
    NavApp::closeSplashScreen();
    databaseManager->run();
  }

  // Checks if version of database is smaller than application database version and shows a warning dialog if it is
  databaseManager->checkDatabaseVersion();

  NavApp::logDatabaseMeta();

  // If enabled connect to simulator without showing dialog
  NavApp::getConnectClient()->tryConnectOnStartup();

  // Start weather downloads
  weatherUpdateTimeout();

  // Update the weather every 15 seconds if connected
  weatherUpdateTimer.setInterval(WEATHER_UPDATE_MS);
  weatherUpdateTimer.start();

  // Check for updates once main window is visible
  NavApp::checkForUpdates(OptionData::instance().getUpdateChannels(), false /* manually triggered */, false /* forceDebug */);

  optionsDialog->checkOfficialOnlineUrls();

  // Start regular download of online network files
  NavApp::getOnlinedataController()->startProcessing();

  // Start webserver
  if(ui->actionRunWebserver->isChecked())
    NavApp::getWebController()->startServer();
  webserverStatusChanged(NavApp::getWebController()->isRunning());

  renderStatusUpdateLabel(Marble::Complete, true /* forceUpdate */);

  // Do delayed dock window formatting and fullscreen state after widget layout is done
  QTimer::singleShot(100, this, &MainWindow::mainWindowShownDelayed);

  if(ui->actionRouteDownloadTracks->isChecked())
    QTimer::singleShot(1000, NavApp::getTrackController(), &TrackController::startDownloadStartup);

  // Log screen information ==============
  for(QScreen *screen : QGuiApplication::screens())
    qDebug() << Q_FUNC_INFO
             << (screen == QGuiApplication::primaryScreen() ? "Primary Screen" : "Screen")
             << "name" << screen->name() << "model" << screen->model() << "manufacturer" << screen->manufacturer()
             << "geo" << screen->geometry() << "available geo" << screen->availableGeometry()
             << "available virtual geo" << screen->availableVirtualGeometry();

  qDebug() << Q_FUNC_INFO << "leave";
}

void MainWindow::mainWindowShownDelayed()
{
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_LAYOUT) && !layoutFileHistory->isEmpty())
  {
    try
    {
      // Reload last layout file - does not apply state
      dockHandler->loadWindowState(layoutFileHistory->getTopFile(),
                                   OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK),
                                   layoutWarnText);
      QTimer::singleShot(200, dockHandler, &atools::gui::DockWidgetHandler::currentStateToWindow);
    }
    catch(atools::Exception& e)
    {
      NavApp::closeSplashScreen();
      atools::gui::ErrorHandler(this).handleException(e);
    }
  }
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

void MainWindow::stayOnTop()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setStayOnTop(marbleAboutDialog, ui->actionWindowStayOnTop->isChecked());
  dockHandler->setStayOnTop(optionsDialog, ui->actionWindowStayOnTop->isChecked());
  dockHandler->setStayOnTopMain(ui->actionWindowStayOnTop->isChecked());
}

void MainWindow::allowMovingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setMovingAllowed(ui->actionShowAllowMoving->isChecked());

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Undockable map widget is not registered in handler
    dockHandler->setMovingAllowed(ui->dockWidgetMap, ui->actionShowAllowMoving->isChecked());
}

void MainWindow::allowDockingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->setDockingAllowed(ui->actionShowAllowDocking->isChecked());

  if(OptionData::instance().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK))
    // Undockable map widget is not registered in handler
    dockHandler->setDockingAllowed(ui->actionShowAllowDocking->isChecked());
}

void MainWindow::raiseFloatingWindows()
{
  qDebug() << Q_FUNC_INFO;
  dockHandler->raiseFloatingWindows();

  // Map window is not registered in dockHandler
  dockHandler->raiseFloatingWindow(ui->dockWidgetMap);

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
  ui->actionMapHideRangeRings->setEnabled(!mapWidget->getDistanceMarks().isEmpty() ||
                                          !mapWidget->getRangeMarks().isEmpty() ||
                                          !mapWidget->getPatternsMarks().isEmpty() ||
                                          !mapWidget->getMsaMarks().isEmpty() ||
                                          !mapWidget->getHoldingMarks().isEmpty());
}

/* Enable or disable actions */
void MainWindow::updateHighlightActionStates()
{
  ui->actionMapClearAllHighlights->setEnabled(
    mapWidget->hasHighlights() || searchController->hasSelection() || routeController->hasTableSelection());
}

/* Enable or disable actions */
void MainWindow::updateActionStates()
{
  qDebug() << Q_FUNC_INFO;

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
  bool hasTrack = !NavApp::isAircraftTrackEmpty();
  ui->actionRouteAppend->setEnabled(hasFlightplan);
  ui->actionRouteSave->setEnabled(hasFlightplan /* && routeController->hasChanged()*/);
  ui->actionRouteSaveAs->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsFlightGear->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsFms11->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsPln->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsPlnMsfs->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsGpx->setEnabled(hasFlightplan || hasTrack);
  ui->actionRouteSaveAsHtml->setEnabled(hasFlightplan);
  ui->actionRouteSaveAll->setEnabled(hasFlightplan && routeExport->hasSelected());
  ui->actionRouteSendToSimBrief->setEnabled(hasFlightplan);

  ui->actionRouteSaveAsVfp->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsIvap->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsXIvap->setEnabled(hasFlightplan);
  ui->actionRouteShowSkyVector->setEnabled(hasFlightplan);

  ui->actionRouteCenter->setEnabled(hasFlightplan);
  ui->actionRouteSelectParking->setEnabled(route.hasValidDeparture());
  ui->actionMapShowRoute->setEnabled(hasFlightplan);
  ui->actionMapShowTocTod->setEnabled(hasFlightplan && ui->actionMapShowRoute->isChecked());
  ui->actionInfoApproachShowMissedAppr->setEnabled(hasFlightplan && ui->actionMapShowRoute->isChecked());
  ui->actionRouteEditMode->setEnabled(hasFlightplan);
  ui->actionPrintFlightplan->setEnabled(hasFlightplan);
  ui->actionRouteCopyString->setEnabled(hasFlightplan);
  ui->actionRouteAdjustAltitude->setEnabled(hasFlightplan);

  bool hasTracks = NavApp::hasTracks();
  ui->actionRouteDeleteTracks->setEnabled(hasTracks);
  ui->actionMapShowTracks->setEnabled(hasTracks);

#ifdef DEBUG_MOVING_AIRPLANE
  ui->actionMapShowAircraft->setEnabled(true);
  ui->actionMapAircraftCenter->setEnabled(true);
  ui->actionMapAircraftCenterNow->setEnabled(true);
  ui->actionMapShowAircraftAi->setEnabled(true);
  ui->actionMapShowAircraftAiBoat->setEnabled(true);
#else
  ui->actionMapShowAircraft->setEnabled(NavApp::isConnected());
  ui->actionMapAircraftCenter->setEnabled(NavApp::isConnected());
  ui->actionMapAircraftCenterNow->setEnabled(NavApp::isConnected());

  ui->actionMapShowAircraftAi->setEnabled((NavApp::isConnected() && NavApp::isFetchAiAircraft()) ||
                                          NavApp::getOnlinedataController()->isNetworkActive());

  ui->actionMapShowAircraftAiBoat->setEnabled(NavApp::isConnected() && NavApp::isFetchAiShip());
#endif

  ui->actionConnectSimulatorToggle->blockSignals(true);
  ui->actionConnectSimulatorToggle->setChecked(NavApp::isConnected());
  ui->actionConnectSimulatorToggle->blockSignals(false);

  ui->actionMapShowAircraftTrack->setEnabled(true);
  ui->actionMapDeleteAircraftTrack->setEnabled(mapWidget->hasTrackPoints() || profileWidget->hasTrackPoints());

  bool canCalcRoute = route.canCalcRoute();
  ui->actionRouteCalcDirect->setEnabled(canCalcRoute && NavApp::getRouteConst().hasEntries());
  // ui->actionRouteCalc->setEnabled(canCalcRoute);
  ui->actionRouteReverse->setEnabled(canCalcRoute);

  ui->actionMapShowAirportWeather->setEnabled(NavApp::getAirportWeatherSource() != map::WEATHER_SOURCE_DISABLED);

  ui->actionMapShowHome->setEnabled(mapWidget->getHomePos().isValid());
  ui->actionMapShowMark->setEnabled(mapWidget->getSearchMarkPos().isValid());

  ui->actionShowApproachCustom->setEnabled(route.hasValidDestinationAndRunways());
  ui->actionShowDepartureCustom->setEnabled(route.hasValidDepartureAndRunways());
}

void MainWindow::resetAllSettings()
{
  QString settingFile = Settings::instance().getFilename();
  QString settingPath = Settings::instance().getPath();

  QMessageBox::StandardButton retval =
    QMessageBox::warning(this, QApplication::applicationName() + tr("Reset all Settings "),
                         tr("<b>This will reset all options, window layout, dialog layout, "
                              "aircraft trail, map position history and file histories "
                              "back to default and restart <i>%1</i>.</b><br/><br/>"
                              "User features like range rings or patterns as well as "
                              "scenery, logbook and userpoint databases are not affected.<br/><br/>"
                              "A copy of the settings file<br/><br/>"
                              "\"%2\"<br/><br/>"
                              "will be created in the folder<br/><br/>"
                              "\"%3\"<br/><br/>"
                              "which allows you to undo this change."
                            ).arg(QApplication::applicationName()).arg(settingFile).arg(settingPath)
                         , QMessageBox::Ok | QMessageBox::Cancel | QMessageBox::Help,
                         QMessageBox::Cancel);

  if(retval == QMessageBox::Ok)
  {
    NavApp::setRestartProcess(true);
    close();
  }
  else if(retval == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "MENUS.html#reset-and-restart",
                                             lnm::helpLanguageOnline());
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

  NavApp::getRouteTabHandler()->reset();
  infoController->resetWindowLayout();
  searchController->resetWindowLayout();

  ui->actionShowFullscreenMap->blockSignals(true);
  ui->actionShowFullscreenMap->setChecked(false);
  ui->actionShowFullscreenMap->blockSignals(false);

  ui->actionShowStatusbar->setChecked(true);
  ui->statusBar->setVisible(true);
  ui->menuBar->setVisible(true);
}

/* Read settings for all windows, docks, controller and manager classes */
void MainWindow::restoreStateMain()
{
  qDebug() << Q_FUNC_INFO << "enter";

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);

  Settings& settings = Settings::instance();

  if(OptionData::instance().getFlags2().testFlag(opts2::OVERRIDE_TOOLBAR_SIZE))
    setIconSize(OptionData::instance().getGuiToolbarSize());

  if(settings.contains(lnm::MAINWINDOW_WIDGET_DOCKHANDLER))
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
    // Use default state saved in application
    resetWindowLayout();

#ifdef DEBUG_MENU_TOOLTIPS
  // Enable tooltips for all menus
  QList<QAction *> stack;
  stack.append(ui->menuBar->actions());
  while(!stack.isEmpty())
  {
    QMenu *menu = stack.takeLast()->menu();
    if(menu != nullptr)
    {
      menu->setToolTipsVisible(true);
      for(QAction *sub : menu->actions())
      {
        if(sub->menu() != nullptr)
          stack.append(sub);
      }
    }
  }
#endif

  // Need to be loaded in constructor first since it reads all options
  // optionsDialog->restoreState();

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

  widgetState.setBlockSignals(true);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    // Restore map settings if desired by the user
    widgetState.restore({ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp,
                         ui->actionMapShowIls, ui->actionMapShowGls, ui->actionMapShowHolding, ui->actionMapShowAirportMsa,
                         ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways, ui->actionMapShowTracks, ui->actionShowAirspaces,
                         ui->actionMapShowRoute, ui->actionMapShowTocTod, ui->actionMapShowAircraft, ui->actionMapShowCompassRose,
                         ui->actionMapShowCompassRoseAttach, ui->actionMapShowEndurance, ui->actionMapShowSelectedAltRange,
                         ui->actionMapAircraftCenter, ui->actionMapShowAircraftAi, ui->actionMapShowAircraftAiBoat,
                         ui->actionMapShowAircraftTrack, ui->actionInfoApproachShowMissedAppr, ui->actionSearchLogdataShowDirect,
                         ui->actionSearchLogdataShowRoute, ui->actionSearchLogdataShowTrack});
  }
  else
    mapWidget->resetSettingActionsToDefault();

  // Map settings that are always loaded
  widgetState.restore({mapProjectionComboBox, ui->actionMapShowGrid, ui->actionMapShowCities,
                       ui->actionRouteEditMode, ui->actionRouteSaveSidStarWaypoints,
                       ui->actionRouteSaveApprWaypoints, ui->actionRouteSaveAirwayWaypoints, ui->actionLogdataCreateLogbook,
                       ui->actionMapShowSunShading, ui->actionMapShowAirportWeather, ui->actionMapShowMinimumAltitude,
                       ui->actionRunWebserver, ui->actionShowAllowDocking, ui->actionShowAllowMoving, ui->actionWindowStayOnTop});

  if(widgetState.contains(comboBoxMapTheme))
    // Restore map theme selection
    widgetState.restore(comboBoxMapTheme);
  else
  {
    // Load default theme OSM
    int defaultIndex = mapWidget->getMapThemeHandler()->getDefaultTheme().getIndex();
    for(int i = 0; i < comboBoxMapTheme->count(); i++)
    {
      if(comboBoxMapTheme->itemData(i).isValid() && comboBoxMapTheme->itemData(i).toInt() == defaultIndex)
      {
        comboBoxMapTheme->blockSignals(true);
        comboBoxMapTheme->setCurrentIndex(defaultIndex);
        comboBoxMapTheme->blockSignals(false);
        break;
      }
    }
  }

  widgetState.setBlockSignals(false);

  firstApplicationStart = settings.valueBool(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, true);

  // Already loaded in constructor early to allow database creations
  // databaseLoader->restoreState();

  if(!(OptionData::instance().getFlags2() & opts2::MAP_ALLOW_UNDOCK))
    ui->dockWidgetMap->hide();
  else
    ui->dockWidgetMap->show();

  qDebug() << Q_FUNC_INFO << "leave";
}

void MainWindow::optionsChanged()
{
  if(OptionData::instance().getFlags2().testFlag(opts2::OVERRIDE_TOOLBAR_SIZE))
    setIconSize(OptionData::instance().getGuiToolbarSize());
  else
    setIconSize(defaultToolbarIconSize);

  dockHandler->setAutoRaiseDockWindows(OptionData::instance().getFlags2().testFlag(opts2::RAISE_DOCK_WINDOWS));
  dockHandler->setAutoRaiseMainWindow(OptionData::instance().getFlags2().testFlag(opts2::RAISE_MAIN_WINDOW));
}

void MainWindow::saveStateNow()
{
  saveStateMain();
}

/* Write settings for all windows, docks, controller and manager classes */
void MainWindow::saveStateMain()
{
  qDebug() << Q_FUNC_INFO;

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
    deleteAircraftTrack(true /* do not ask questions */);
    mapWidget->clearHistory();
  }

  saveMainWindowStates();

  qDebug() << "searchController";
  if(searchController != nullptr)
    searchController->saveState();

  qDebug() << "mapWidget";
  if(mapWidget != nullptr)
    mapWidget->saveState();

  qDebug() << "userDataController";
  if(NavApp::getUserdataController() != nullptr)
    NavApp::getUserdataController()->saveState();

  qDebug() << "mapMarkHandler";
  if(NavApp::getMapMarkHandler() != nullptr)
    NavApp::getMapMarkHandler()->saveState();

  qDebug() << "mapAirportHandler";
  if(NavApp::getMapAirportHandler() != nullptr)
    NavApp::getMapAirportHandler()->saveState();

  qDebug() << "logdataController";
  if(NavApp::getLogdataController() != nullptr)
    NavApp::getLogdataController()->saveState();

  qDebug() << "windReporter";
  if(NavApp::getWindReporter() != nullptr)
    NavApp::getWindReporter()->saveState();

  qDebug() << "aircraftPerfController";
  if(NavApp::getAircraftPerfController() != nullptr)
    NavApp::getAircraftPerfController()->saveState();

  qDebug() << "airspaceController";
  if(NavApp::getAirspaceController() != nullptr)
    NavApp::getAirspaceController()->saveState();

  qDebug() << "routeController";
  if(routeController != nullptr)
    routeController->saveState();

  qDebug() << "profileWidget";
  if(profileWidget != nullptr)
    profileWidget->saveState();

  qDebug() << "connectClient";
  if(NavApp::getConnectClient() != nullptr)
    NavApp::getConnectClient()->saveState();

  qDebug() << "trackController";
  if(NavApp::getTrackController() != nullptr)
    NavApp::getTrackController()->saveState();

  qDebug() << "infoController";
  if(infoController != nullptr)
    infoController->saveState();

  saveFileHistoryStates();

  qDebug() << "printSupport";
  if(printSupport != nullptr)
    printSupport->saveState();

  qDebug() << "routeExport";
  if(routeExport != nullptr)
    routeExport->saveState();

  qDebug() << "optionsDialog";
  if(optionsDialog != nullptr)
    optionsDialog->saveState();

  qDebug() << "styleHandler";
  if(NavApp::getStyleHandler() != nullptr)
    NavApp::getStyleHandler()->saveState();

  saveActionStates();

  Settings& settings = Settings::instance();
  settings.setValue(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, firstApplicationStart);

  qDebug() << "databaseManager";
  if(NavApp::getDatabaseManager() != nullptr)
    NavApp::getDatabaseManager()->saveState();

  qDebug() << "syncSettings";
  settings.syncSettings();
  qDebug() << "save state done";
}

void MainWindow::saveFileHistoryStates()
{
  qDebug() << Q_FUNC_INFO;

  qDebug() << "routeFileHistory";
  if(routeFileHistory != nullptr)
    routeFileHistory->saveState();

  qDebug() << "kmlFileHistory";
  if(kmlFileHistory != nullptr)
    kmlFileHistory->saveState();

  qDebug() << "layoutFileHistory";
  if(layoutFileHistory != nullptr)
    layoutFileHistory->saveState();

  Settings::instance().syncSettings();
}

void MainWindow::saveMainWindowStates()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);

  Settings& settings = Settings::instance();
  settings.setValueVar(lnm::MAINWINDOW_WIDGET_DOCKHANDLER, dockHandler->saveState());

  // Save profile dock size separately since it is sometimes resized by other docks
  // settings.setValue(lnm::MAINWINDOW_WIDGET_STATE + "ProfileDockHeight", ui->dockWidgetContentsProfile->height());
  Settings::instance().syncSettings();
}

void MainWindow::saveActionStates()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);
  widgetState.save({mapProjectionComboBox, comboBoxMapTheme, ui->actionMapShowVor, ui->actionMapShowNdb,
                    ui->actionMapShowWp, ui->actionMapShowIls, ui->actionMapShowGls, ui->actionMapShowHolding, ui->actionMapShowAirportMsa,
                    ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways, ui->actionMapShowTracks, ui->actionShowAirspaces,
                    ui->actionMapShowRoute, ui->actionMapShowTocTod, ui->actionMapShowAircraft, ui->actionMapShowCompassRose,
                    ui->actionMapShowCompassRoseAttach, ui->actionMapShowEndurance, ui->actionMapShowSelectedAltRange,
                    ui->actionMapAircraftCenter, ui->actionMapShowAircraftAi, ui->actionMapShowAircraftAiBoat,
                    ui->actionMapShowAircraftTrack, ui->actionInfoApproachShowMissedAppr, ui->actionMapShowGrid, ui->actionMapShowCities,
                    ui->actionMapShowSunShading, ui->actionMapShowAirportWeather,
                    ui->actionMapShowMinimumAltitude, ui->actionRouteEditMode, ui->actionRouteSaveSidStarWaypoints,
                    ui->actionRouteSaveApprWaypoints, ui->actionRouteSaveAirwayWaypoints, ui->actionLogdataCreateLogbook,
                    ui->actionRunWebserver, ui->actionSearchLogdataShowDirect, ui->actionSearchLogdataShowRoute,
                    ui->actionSearchLogdataShowTrack, ui->actionShowAllowDocking, ui->actionShowAllowMoving, ui->actionWindowStayOnTop});

  Settings::instance().syncSettings();
}

#ifdef DEBUG_DUMP_SHORTCUTS
void MainWindow::printShortcuts()
{
  // Print all main menu and sub menu shortcuts ==============================================
  qDebug() << "===============================================================================";

  QString out;
  QTextStream stream(&out, QIODevice::WriteOnly);
  QSet<QKeySequence> keys;
  QStringList warnings;

  int c1 = 80, c2 = 25;

  for(const QAction *mainmenus : ui->menuBar->actions())
  {
    if(mainmenus->menu() != nullptr)
    {
      QString text = mainmenus->menu()->menuAction()->text().remove(QChar('&'));

      stream << endl << ".. _shortcuts-main-" << text.toLower() << ":" << endl << endl;

      stream << text << endl;
      stream << QString("^").repeated(text.size()) << endl << endl;

      stream << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
      stream << "| " << QString("Menu").leftJustified(c1 - 1)
             << "| " << QString("Shortcut").leftJustified(c2 - 1) << "|" << endl;
      stream << "+" << QString("=").repeated(c1) << "+" << QString("=").repeated(c2) << "+" << endl;

      QString mainmenu = mainmenus->text().remove(QChar('&'));
      for(const QAction *mainAction : mainmenus->menu()->actions())
      {
        if(mainAction->menu() != nullptr)
        {
          QString submenu = mainAction->text().remove(QChar('&'));
          for(const QAction *subAction : mainAction->menu()->actions())
          {
            if(!subAction->text().isEmpty() && !subAction->shortcut().isEmpty())
            {
              if(keys.contains(subAction->shortcut()))
                warnings.append(QString("Duplicate shortcut \"%1\"").arg(subAction->shortcut().toString()));

              stream << "| "
                     << QString(mainmenu + " -> " + submenu + " -> " +
                         subAction->text().remove(QChar('&'))).leftJustified(c1 - 1)
                     << "| "
                     << ("``" + subAction->shortcut().toString() + "``").leftJustified(c2 - 1)
                     << "|" << endl;
              stream << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
              keys.insert(subAction->shortcut());
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

            stream << "| "
                   << QString(mainmenu + " -> " + mainAction->text().remove(QChar('&'))).leftJustified(c1 - 1)
                   << "| "
                   << ("``" + mainAction->shortcut().toString() + "``").leftJustified(c2 - 1)
                   << "|" << endl;
            stream << "+" << QString("-").repeated(c1) << "+" << QString("-").repeated(c2) << "+" << endl;
            keys.insert(mainAction->shortcut());
          }
        }
      }
    }
  }
  qDebug().nospace().noquote() << endl << out;

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

  if(routeController->hasChanged())
  {
    if(!routeCheckForChanges())
    {
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
      event->ignore();
      // Do not restart process after settings reset
      NavApp::setRestartProcess(false);
      return;
    }
  }

  // Do not ask if user did a reset settings
  if(!NavApp::isRestartProcess())
  {
    int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_QUIT,
                                            tr("Really Quit?"),
                                            tr("Do not &show this dialog again."),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No, QMessageBox::Yes);

    if(result != QMessageBox::Yes)
      event->ignore();
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

    clearWeatherContext();
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

  NavApp::logDatabaseMeta();
}

/* Update the current weather context for the information window. Returns true if any
 * weather has changed or an update is needed */
bool MainWindow::buildWeatherContextForInfo(map::WeatherContext& weatherContext, const map::MapAirport& airport)
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();
  bool changed = false;
  bool newAirport = currentWeatherContext->ident != airport.ident;

#ifdef DEBUG_INFORMATION_WEATHER
  qDebug() << Q_FUNC_INFO;
#endif

  currentWeatherContext->ident = airport.ident;

  if(flags & optsw::WEATHER_INFO_FS)
  {
    if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
    {
      currentWeatherContext->fsMetar = weatherReporter->getXplaneMetar(airport.ident, airport.position);
      changed = true;
    }
    else if(NavApp::isConnected())
    {
      // FSX/P3D - Flight simulator fetched weather
      atools::fs::weather::MetarResult metar =
        NavApp::getConnectClient()->requestWeather(airport.ident, airport.position, false /* station only */);

      if(newAirport || (!metar.isEmpty() && metar != currentWeatherContext->fsMetar))
      {
        // Airport has changed or METAR has changed
        currentWeatherContext->fsMetar = metar;
        changed = true;
      }
    }
    else
    {
      if(!currentWeatherContext->fsMetar.isEmpty())
      {
        // If there was a previous metar and the new one is empty we were being disconnected
        currentWeatherContext->fsMetar = atools::fs::weather::MetarResult();
        changed = true;
      }
    }
  }

  if(flags & optsw::WEATHER_INFO_ACTIVESKY)
  {
    fillActiveSkyType(*currentWeatherContext, airport.ident);

    QString metarStr = weatherReporter->getActiveSkyMetar(airport.ident);
    if(newAirport || (!metarStr.isEmpty() && metarStr != currentWeatherContext->asMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->asMetar = metarStr;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_NOAA)
  {
    atools::fs::weather::MetarResult noaaMetar = weatherReporter->getNoaaMetar(airport.ident, airport.position);
    if(newAirport || (!noaaMetar.isEmpty() && noaaMetar != currentWeatherContext->noaaMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->noaaMetar = noaaMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_VATSIM)
  {
    atools::fs::weather::MetarResult vatsimMetar = weatherReporter->getVatsimMetar(airport.ident, airport.position);
    if(newAirport || (!vatsimMetar.isEmpty() && vatsimMetar != currentWeatherContext->vatsimMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->vatsimMetar = vatsimMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_IVAO)
  {
    atools::fs::weather::MetarResult ivaoMetar = weatherReporter->getIvaoMetar(airport.ident, airport.position);
    if(newAirport || (!ivaoMetar.isEmpty() && ivaoMetar != currentWeatherContext->ivaoMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->ivaoMetar = ivaoMetar;
      changed = true;
    }
  }

  weatherContext = *currentWeatherContext;

#ifdef DEBUG_INFORMATION_WEATHER
  if(changed)
    qDebug() << Q_FUNC_INFO << "changed" << changed << weatherContext;
#endif

  return changed;
}

/* Build a normal weather context - used by printing */
void MainWindow::buildWeatherContext(map::WeatherContext& weatherContext, const map::MapAirport& airport) const
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();

  weatherContext.ident = airport.ident;

  if(flags & optsw::WEATHER_INFO_FS)
  {
    if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
      weatherContext.fsMetar = weatherReporter->getXplaneMetar(airport.ident, airport.position);
    else
      weatherContext.fsMetar =
        NavApp::getConnectClient()->requestWeather(airport.ident, airport.position, false /* station only */);
  }

  if(flags & optsw::WEATHER_INFO_ACTIVESKY)
  {
    weatherContext.asMetar = weatherReporter->getActiveSkyMetar(airport.ident);
    fillActiveSkyType(weatherContext, airport.ident);
  }

  if(flags & optsw::WEATHER_INFO_NOAA)
    weatherContext.noaaMetar = weatherReporter->getNoaaMetar(airport.ident, airport.position);

  if(flags & optsw::WEATHER_INFO_VATSIM)
    weatherContext.vatsimMetar = weatherReporter->getVatsimMetar(airport.ident, airport.position);

  if(flags & optsw::WEATHER_INFO_IVAO)
    weatherContext.ivaoMetar = weatherReporter->getIvaoMetar(airport.ident, airport.position);
}

/* Build a temporary weather context for the map tooltip */
void MainWindow::buildWeatherContextForTooltip(map::WeatherContext& weatherContext,
                                               const map::MapAirport& airport) const
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();

  weatherContext.ident = airport.ident;

  if(flags & optsw::WEATHER_TOOLTIP_FS)
  {
    if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
      weatherContext.fsMetar = weatherReporter->getXplaneMetar(airport.ident, airport.position);
    else
      weatherContext.fsMetar =
        NavApp::getConnectClient()->requestWeather(airport.ident, airport.position, false /* station only */);
  }

  if(flags & optsw::WEATHER_TOOLTIP_ACTIVESKY)
  {
    weatherContext.asMetar = weatherReporter->getActiveSkyMetar(airport.ident);
    fillActiveSkyType(weatherContext, airport.ident);
  }

  if(flags & optsw::WEATHER_TOOLTIP_NOAA)
    weatherContext.noaaMetar = weatherReporter->getNoaaMetar(airport.ident, airport.position);

  if(flags & optsw::WEATHER_TOOLTIP_VATSIM)
    weatherContext.vatsimMetar = weatherReporter->getVatsimMetar(airport.ident, airport.position);

  if(flags & optsw::WEATHER_TOOLTIP_IVAO)
    weatherContext.ivaoMetar = weatherReporter->getIvaoMetar(airport.ident, airport.position);
}

/* Fill active sky information into the weather context */
void MainWindow::fillActiveSkyType(map::WeatherContext& weatherContext, const QString& airportIdent) const
{
  weatherContext.asType = weatherReporter->getCurrentActiveSkyName();
  weatherContext.isAsDeparture = false;
  weatherContext.isAsDestination = false;

  if(weatherReporter->getActiveSkyDepartureIdent() == airportIdent)
    weatherContext.isAsDeparture = true;
  if(weatherReporter->getActiveSkyDestinationIdent() == airportIdent)
    weatherContext.isAsDestination = true;
}

void MainWindow::clearWeatherContext()
{
  qDebug() << Q_FUNC_INFO;

  // Clear all weather and fetch new
  *currentWeatherContext = map::WeatherContext();
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

        if(file.exists() && file.isReadable() && file.isFile() &&
           (atools::fs::pln::FlightplanIO::detectFormat(file.filePath()) ||
            AircraftPerfController::isPerformanceFile(file.filePath()) ||
            atools::gui::DockWidgetHandler::isWindowLayoutFile(file.filePath())))
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

void MainWindow::makeErrorLabel(QString& toolTipText, QStringList errors, const QString& header)
{
  if(!errors.isEmpty())
  {
    if(errors.size() > 5)
    {
      errors = errors.mid(0, 5);
      errors.append(tr("More ..."));
    }

    toolTipText.append(header);
    toolTipText.append("<ul>");
    for(const QString& str : errors)
      toolTipText.append("<li>" % str % "</li>");
    toolTipText.append("</ul>");
  }
}

void MainWindow::updateErrorLabels()
{
  using atools::util::HtmlBuilder;

  // Collect errors from all controllers =================================
  QString toolTipText;

  // Flight plan ============
  makeErrorLabel(toolTipText, routeController->getErrorStrings(),
                 tr("<nobr><b>Problems on tab \"Flight Plan\":</b></nobr>", "Synchronize name with tab name"));

  // Elevation profile ============
  makeErrorLabel(toolTipText, NavApp::getAltitudeLegs().getErrorStrings(),
                 tr("<nobr><b>Problems when calculating profile for window \"Flight Plan Elevation Profile\":</b></nobr>",
                    "Synchronize name with window name"));

  // Aircraft performance ============
  makeErrorLabel(toolTipText, NavApp::getAircraftPerfController()->getErrorStrings(),
                 tr("<nobr><b>Problems on tab \"Fuel Report\":</b></nobr>", "Synchronize name with tab name"));

  // Build tooltip message ====================================
  if(!toolTipText.isEmpty())
  {
    ui->labelRouteError->setVisible(true);
    ui->labelRouteError->setText(HtmlBuilder::errorMessage(tr("Found problems in flight plan. Click here for details.")));

    // Disallow text wrapping
    ui->labelRouteError->setToolTip(toolTipText);
    ui->labelRouteError->setStatusTip(tr("Found problems in flight plan."));
  }
  else
  {
    ui->labelRouteError->setVisible(false);
    ui->labelRouteError->setText(QString());
    ui->labelRouteError->setToolTip(QString());
    ui->labelRouteError->setStatusTip(QString());
  }
}

int MainWindow::getMapThemeIndex() const
{
  return comboBoxMapTheme->currentData().toInt();
}

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

void MainWindow::showRouteCalc()
{
  if(NavApp::isMainWindowVisible())
    actionShortcutCalcRouteTriggered();
}

void MainWindow::webserverStatusChanged(bool running)
{
  ui->actionRunWebserver->setChecked(running);
  ui->actionOpenWebserver->setEnabled(running);
}

void MainWindow::toggleWebserver(bool checked)
{
  if(checked)
    NavApp::getWebController()->startServer();
  else
    NavApp::getWebController()->stopServer();
}

void MainWindow::openWebserver()
{
  NavApp::getWebController()->openPage();
}
