/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "common/constants.h"
#include "gui/application.h"
#include "common/weatherreporter.h"
#include "connect/connectclient.h"
#include "db/databasemanager.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/helphandler.h"
#include "gui/tablezoomhandler.h"
#include "gui/translator.h"
#include "gui/widgetstate.h"
#include "info/infocontroller.h"
#include "info/infoquery.h"
#include "logging/logginghandler.h"
#include "mapgui/mapquery.h"
#include "mapgui/mapwidget.h"
#include "profile/profilewidget.h"
#include "route/routecontroller.h"
#include "gui/filehistoryhandler.h"
#include "search/airportsearch.h"
#include "search/navsearch.h"
#include "mapgui/maplayersettings.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"
#include "options/optionsdialog.h"
#include "print/printsupport.h"
#include "exception.h"
#include "route/routestringdialog.h"
#include "route/routestring.h"
#include "common/unit.h"

#include <marble/LegendWidget.h>
#include <marble/MarbleAboutDialog.h>
#include <marble/MarbleModel.h>

#include <QDebug>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QScreen>
#include <QWindow>
#include <QDesktopWidget>
#include <QDir>
#include <QFileInfoList>

#include "ui_mainwindow.h"

static const QString ABOUT_MESSAGE =
  QObject::tr("<p>is a free open source flight planner, navigation tool, moving map, "
                "airport search and airport information system for Flight Simulator X and Prepar3D.</p>"
                "<p>This software is licensed under "
                  "<a href=\"http://www.gnu.org/licenses/gpl-3.0\">GPL3</a> or any later version.</p>"
                    "<p>The source code for this application is available at "
                      "<a href=\"https://github.com/albar965\">Github</a>.</p>"
                        "<p>More about my projects at "
                          "<a href=\"https://albar965.github.io\">albar965.github.io</a>.</p>"
                            "<p><b>Copyright 2015-2016 Alexander Barthel</b></p>");

// All known map themes
static const QStringList STOCK_MAP_THEMES({"clouds", "hillshading", "openstreetmap", "openstreetmaproads",
                                           "openstreetmaproadshs", "opentopomap", "plain", "political",
                                           "srtm", "srtm2", "stamenterrain"});

using namespace Marble;
using atools::settings::Settings;
using atools::gui::FileHistoryHandler;
using atools::gui::MapPosHistory;
using atools::gui::HelpHandler;

MainWindow::MainWindow()
  : QMainWindow(nullptr), ui(new Ui::MainWindow)
{
  qDebug() << "MainWindow constructor";

  try
  {
    // Have to handle exceptions here since no message handler is active yet and no
    // atools::Application method can catch it

    ui->setupUi(this);

    dialog = new atools::gui::Dialog(this);
    errorHandler = new atools::gui::ErrorHandler(this);
    helpHandler = new atools::gui::HelpHandler(this, ABOUT_MESSAGE, GIT_REVISION);

    marbleAbout = new Marble::MarbleAboutDialog(this);
    marbleAbout->setApplicationTitle(QApplication::applicationName());

    setupUi();

    qDebug() << "MainWindow Creating OptionsDialog";
    optionsDialog = new OptionsDialog(this);
    // Has to load the state now to options are available for all controller and manager classes
    optionsDialog->restoreState();

    Unit::init();

    // Remember original title
    mainWindowTitle = windowTitle();

    // Prepare database and queries
    qDebug() << "MainWindow Creating DatabaseManager";
    databaseManager = new DatabaseManager(this);
    databaseManager->openDatabase();

    mapQuery = new MapQuery(this, databaseManager->getDatabase());
    mapQuery->initQueries();

    infoQuery = new InfoQuery(databaseManager->getDatabase());
    infoQuery->initQueries();

    // Add actions for flight simulator database switch in main menu
    databaseManager->insertSimSwitchActions(ui->actionDatabaseFiles, ui->menuDatabase);

    qDebug() << "MainWindow Creating WeatherReporter";
    weatherReporter = new WeatherReporter(this, databaseManager->getCurrentSimulator());

    qDebug() << "MainWindow Creating FileHistoryHandler for flight plans";
    routeFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMESRECENT, ui->menuRecentRoutes,
                                              ui->actionRecentRoutesClear);

    qDebug() << "MainWindow Creating RouteController";
    routeController = new RouteController(this, mapQuery, ui->tableViewRoute);

    qDebug() << "MainWindow Creating FileHistoryHandler for KML files";
    kmlFileHistory = new FileHistoryHandler(this, lnm::ROUTE_FILENAMESKMLRECENT, ui->menuRecentKml,
                                            ui->actionClearKmlMenu);

    // Create map widget and replace dummy widget in window
    qDebug() << "MainWindow Creating MapWidget";
    mapWidget = new MapWidget(this, mapQuery);
    ui->verticalLayout_10->replaceWidget(ui->mapWidgetDummy, mapWidget);

    // Create elevation profile widget and replace dummy widget in window
    qDebug() << "MainWindow Creating ProfileWidget";
    profileWidget = new ProfileWidget(this);
    ui->verticalLayout_12->replaceWidget(ui->elevationWidgetDummy, profileWidget);

    // Have to create searches in the same order as the tabs
    qDebug() << "MainWindow Creating SearchController";
    searchController = new SearchController(this, mapQuery, ui->tabWidgetSearch);
    searchController->createAirportSearch(ui->tableViewAirportSearch);
    searchController->createNavSearch(ui->tableViewNavSearch);

    qDebug() << "MainWindow Creating ConnectClient";
    connectClient = new ConnectClient(this);

    qDebug() << "MainWindow Creating InfoController";
    infoController = new InfoController(this, mapQuery);

    qDebug() << "MainWindow Creating PrintSupport";
    printSupport = new PrintSupport(this, mapQuery, infoQuery);

    qDebug() << "MainWindow Connecting slots";
    connectAllSlots();

    qDebug() << "MainWindow Reading settings";
    readSettings();

    updateActionStates();

    qDebug() << "MainWindow Setting theme";
    mapWidget->setTheme(mapThemeComboBox->currentData().toString(), mapThemeComboBox->currentIndex());

    qDebug() << "MainWindow Setting projection";
    mapWidget->setProjection(mapProjectionComboBox->currentData().toInt());

    // Wait until everything is set up and update map
    updateMapObjectsShown();

    profileWidget->updateProfileShowFeatures();

    loadNavmapLegend();
    updateLegend();

    qDebug() << "MainWindow Constructor done";
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
}

MainWindow::~MainWindow()
{
  qDebug() << "MainWindow destructor";

  weatherUpdateTimer.stop();

  // Close all queries
  preDatabaseLoad();

  delete connectClient;
  delete routeController;
  delete searchController;
  delete weatherReporter;
  delete mapQuery;
  delete infoQuery;
  delete profileWidget;
  delete marbleAbout;
  delete infoController;
  delete printSupport;
  delete routeFileHistory;
  delete kmlFileHistory;
  delete optionsDialog;
  delete mapWidget;
  delete ui;

  delete dialog;
  delete errorHandler;
  delete databaseManager;
  delete actionGroupMapProjection;
  delete actionGroupMapTheme;

  Unit::deInit();
  // Delete settings singleton
  Settings::shutdown();
  atools::gui::Translator::unload();

  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();
}

/* Show map legend and bring information dock to front */
void MainWindow::showNavmapLegend()
{
  if(legendUrl.isLocalFile() && legendUrl.host().isEmpty())
  {
    ui->dockWidgetInformation->show();
    ui->tabWidgetInformation->setCurrentIndex(ic::NAVMAP_LEGEND);
    setStatusMessage(tr("Opened navigation map legend."));
  }
  else
  {
    // URL is empty loading failed - show it in browser
    helpHandler->openHelpUrl(legendUrl);
    setStatusMessage(tr("Opened map legend in browser."));
  }
}

/* Show marble legend */
void MainWindow::showMapLegend()
{
  ui->dockWidgetInformation->show();
  ui->tabWidgetInformation->setCurrentIndex(ic::MAP_LEGEND);
  setStatusMessage(tr("Opened map legend."));
}

/* Load the navmap legend into the text browser */
void MainWindow::loadNavmapLegend()
{
  legendUrl = helpHandler->getHelpUrl("help", "legend_inline.html");
  if(legendUrl.isLocalFile() && legendUrl.host().isEmpty())
  {
    QString legend;
    QFile legendFile(legendUrl.toLocalFile());
    if(legendFile.open(QIODevice::ReadOnly))
    {
      QTextStream stream(&legendFile);
      legend.append(stream.readAll());

      QString searchPath = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
      searchPath += tr("/help/en");
      ui->textBrowserNavmapLegendInfo->setSearchPaths({searchPath});
      ui->textBrowserNavmapLegendInfo->setText(legend);
    }
    else
      errorHandler->handleIOError(legendFile, tr("While opening Navmap Legend file:"));
  }
}

/* User clicked "show in browser" in legend */
void MainWindow::legendAnchorClicked(const QUrl& url)
{
  qDebug() << "MainWindow::legendAnchorClicked" << url;

  QUrl newUrl(url);
  if(url.isLocalFile())
    newUrl = helpHandler->getHelpUrl("help", url.fileName(QUrl::FullyDecoded));

  if(!QDesktopServices::openUrl(newUrl))
    QMessageBox::warning(this, QApplication::applicationName(), QString(
                           tr("Error opening URL \"%1\"")).arg(url.toDisplayString()));
  else
    setStatusMessage(tr("Opened legend link in browser."));
}

void MainWindow::scaleToolbar(QToolBar *toolbar, float scale)
{
  QSizeF size = toolbar->iconSize();
  size *= scale;
  toolbar->setIconSize(size.toSize());
}

/* Set up own UI elements that cannot be created in designer */
void MainWindow::setupUi()
{
  // Reduce large icons on mac
#if defined(Q_OS_MAC)
  scaleToolbar(ui->mainToolBar, 0.72f);
  scaleToolbar(ui->mapToolBar, 0.72f);
  scaleToolbar(ui->mapToolBarOptions, 0.72f);
  scaleToolbar(ui->routeToolBar, 0.72f);
  scaleToolbar(ui->viewToolBar, 0.72f);
#endif

  ui->mapToolBarOptions->addSeparator();

  // Projection combo box
  mapProjectionComboBox = new QComboBox(this);
  mapProjectionComboBox->setObjectName("mapProjectionComboBox");
  QString helpText = tr("Select map projection");
  mapProjectionComboBox->setToolTip(helpText);
  mapProjectionComboBox->setStatusTip(helpText);
  mapProjectionComboBox->addItem(tr("Mercator"), Marble::Mercator);
  mapProjectionComboBox->addItem(tr("Spherical"), Marble::Spherical);
  ui->mapToolBarOptions->addWidget(mapProjectionComboBox);

  // Projection menu items
  actionGroupMapProjection = new QActionGroup(ui->menuMapProjection);
  ui->actionMapProjectionMercator->setActionGroup(actionGroupMapProjection);
  ui->actionMapProjectionSpherical->setActionGroup(actionGroupMapProjection);

  // Theme combo box
  mapThemeComboBox = new QComboBox(this);
  mapThemeComboBox->setObjectName("mapThemeComboBox");
  helpText = tr("Select map theme");
  mapThemeComboBox->setToolTip(helpText);
  mapThemeComboBox->setStatusTip(helpText);
  // Item order has to match MapWidget::MapThemeComboIndex
  mapThemeComboBox->addItem(tr("OpenStreetMap"), "earth/openstreetmap/openstreetmap.dgml");
  mapThemeComboBox->addItem(tr("OpenMapSurfer"), "earth/openstreetmaproads/openstreetmaproads.dgml");
  mapThemeComboBox->addItem(tr("OpenTopoMap"), "earth/opentopomap/opentopomap.dgml");
  mapThemeComboBox->addItem(tr("Stamen Terrain"), "earth/stamenterrain/stamenterrain.dgml");
  mapThemeComboBox->addItem(tr("Simple (Offline)"), "earth/political/political.dgml");
  mapThemeComboBox->addItem(tr("Plain (Offline)"), "earth/plain/plain.dgml");
  mapThemeComboBox->addItem(tr("Atlas (Offline)"), "earth/srtm/srtm.dgml");
  ui->mapToolBarOptions->addWidget(mapThemeComboBox);

  // Theme menu items
  actionGroupMapTheme = new QActionGroup(ui->menuMapTheme);
  ui->actionMapThemeOpenStreetMap->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeOpenStreetMap->setData(MapWidget::OPENSTREETMAP);

  ui->actionMapThemeOpenStreetMapRoads->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeOpenStreetMapRoads->setData(MapWidget::OPENSTREETMAPROADS);

  ui->actionMapThemeOpenTopoMap->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeOpenTopoMap->setData(MapWidget::OPENTOPOMAP);

  ui->actionMapThemeStamenTerrain->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeStamenTerrain->setData(MapWidget::STAMENTERRAIN);

  ui->actionMapThemeSimple->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeSimple->setData(MapWidget::SIMPLE);

  ui->actionMapThemePlain->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemePlain->setData(MapWidget::PLAIN);

  ui->actionMapThemeAtlas->setActionGroup(actionGroupMapTheme);
  ui->actionMapThemeAtlas->setData(MapWidget::ATLAS);

  // Add any custom map themes that were found besides the stock themes
  QFileInfoList customDgmlFiles;
  findCustomMaps(customDgmlFiles);

  for(int i = 0; i < customDgmlFiles.size(); i++)
  {
    const QFileInfo& dgmlFile = customDgmlFiles.at(i);
    QString name = tr("Custom (%1)").arg(dgmlFile.baseName());
    QString helptext = tr("Custom map theme (%1)").arg(dgmlFile.baseName());

    // Add item to combo box in toolbar
    mapThemeComboBox->addItem(name, dgmlFile.filePath());

    // Create action for map/theme submenu
    QAction *action = ui->menuMapTheme->addAction(name);
    action->setCheckable(true);
    action->setToolTip(helptext);
    action->setStatusTip(helptext);
    action->setActionGroup(actionGroupMapTheme);
    action->setData(MapWidget::CUSTOM + i);

    // Add to list for connect signal etc.
    customMapThemeMenuActions.append(action);
  }

  // Update dock widget actions with tooltip, status tip and keypress
  ui->dockWidgetSearch->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/searchdock.svg"));
  ui->dockWidgetSearch->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+1")));
  ui->dockWidgetSearch->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                       arg(ui->dockWidgetSearch->windowTitle().toLower()));
  ui->dockWidgetSearch->toggleViewAction()->setStatusTip(ui->dockWidgetSearch->toggleViewAction()->toolTip());

  ui->dockWidgetRoute->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/routedock.svg"));
  ui->dockWidgetRoute->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+2")));
  ui->dockWidgetRoute->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                      arg(ui->dockWidgetRoute->windowTitle().toLower()));
  ui->dockWidgetRoute->toggleViewAction()->setStatusTip(ui->dockWidgetRoute->toggleViewAction()->toolTip());

  ui->dockWidgetInformation->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/infodock.svg"));
  ui->dockWidgetInformation->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+3")));
  ui->dockWidgetInformation->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                            arg(ui->dockWidgetInformation->windowTitle().
                                                                toLower()));
  ui->dockWidgetInformation->toggleViewAction()->setStatusTip(
    ui->dockWidgetInformation->toggleViewAction()->toolTip());

  ui->dockWidgetElevation->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/profiledock.svg"));
  ui->dockWidgetElevation->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+4")));
  ui->dockWidgetElevation->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                          arg(ui->dockWidgetElevation->windowTitle().toLower()));
  ui->dockWidgetElevation->toggleViewAction()->setStatusTip(
    ui->dockWidgetElevation->toggleViewAction()->toolTip());

  ui->dockWidgetAircraft->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/aircraftdock.svg"));
  ui->dockWidgetAircraft->toggleViewAction()->setShortcut(QKeySequence(tr("Alt+5")));
  ui->dockWidgetAircraft->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                         arg(ui->dockWidgetAircraft->windowTitle().toLower()));
  ui->dockWidgetAircraft->toggleViewAction()->setStatusTip(
    ui->dockWidgetAircraft->toggleViewAction()->toolTip());

  // Add dock actions to main menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->dockWidgetSearch->toggleViewAction(),
                               ui->dockWidgetRoute->toggleViewAction(),
                               ui->dockWidgetInformation->toggleViewAction(),
                               ui->dockWidgetElevation->toggleViewAction(),
                               ui->dockWidgetAircraft->toggleViewAction()});

  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to menu
  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->mainToolBar->toggleViewAction(),
                               ui->mapToolBar->toggleViewAction(),
                               ui->mapToolBarOptions->toggleViewAction(),
                               ui->routeToolBar->toggleViewAction(),
                               ui->viewToolBar->toggleViewAction()});
  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  // Add toobar actions to toolbar
  ui->viewToolBar->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetInformation->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetElevation->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetAircraft->toggleViewAction());

  // Create labels for the statusbar
  connectStatusLabel = new QLabel();
  connectStatusLabel->setAlignment(Qt::AlignCenter);
  connectStatusLabel->setMinimumWidth(100);
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
  renderStatusLabel->setMinimumWidth(120);
  renderStatusLabel->setToolTip(tr("Map rendering and download status."));
  ui->statusBar->addPermanentWidget(renderStatusLabel);

  mapDistanceLabel = new QLabel();
  mapDistanceLabel->setAlignment(Qt::AlignCenter);
  mapDistanceLabel->setMinimumWidth(100);
  mapDistanceLabel->setToolTip(tr("Map view distance to ground."));
  ui->statusBar->addPermanentWidget(mapDistanceLabel);

  mapPosLabel = new QLabel();
  mapPosLabel->setAlignment(Qt::AlignCenter);
  mapPosLabel->setMinimumWidth(220);
  mapPosLabel->setToolTip(tr("Cursor position on map."));
  ui->statusBar->addPermanentWidget(mapPosLabel);
}

void MainWindow::connectAllSlots()
{
  // Get "show in browser"  click
  connect(ui->textBrowserNavmapLegendInfo, &QTextBrowser::anchorClicked, this,
          &MainWindow::legendAnchorClicked);

  // Notify others of options change
  // The units need to be called before all others
  connect(optionsDialog, &OptionsDialog::optionsChanged, &Unit::optionsChanged);

  connect(optionsDialog, &OptionsDialog::optionsChanged,
          mapWidget, static_cast<void (MapWidget::*)(void)>(&MapWidget::update));
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateMapObjectsShown);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::updateActionStates);
  connect(optionsDialog, &OptionsDialog::optionsChanged, this, &MainWindow::distanceChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, weatherReporter, &WeatherReporter::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, searchController, &SearchController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, routeController, &RouteController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, infoController, &InfoController::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, mapWidget, &MapWidget::optionsChanged);
  connect(optionsDialog, &OptionsDialog::optionsChanged, profileWidget, &ProfileWidget::optionsChanged);

  connect(ui->actionMapSetHome, &QAction::triggered, mapWidget, &MapWidget::changeHome);

  connect(routeController, &RouteController::showRect, mapWidget, &MapWidget::showRect);
  connect(routeController, &RouteController::showPos, mapWidget, &MapWidget::showPos);
  connect(routeController, &RouteController::changeMark, mapWidget, &MapWidget::changeSearchMark);
  connect(routeController, &RouteController::routeChanged, mapWidget, &MapWidget::routeChanged);
  connect(routeController, &RouteController::preRouteCalc, profileWidget, &ProfileWidget::preRouteCalc);
  connect(routeController, &RouteController::showInformation, infoController,
          &InfoController::showInformation);

  // Update rubber band in map window if user hovers over profile
  connect(profileWidget, &ProfileWidget::highlightProfilePoint,
          mapWidget, &MapWidget::highlightProfilePoint);

  connect(routeController, &RouteController::routeChanged, profileWidget, &ProfileWidget::routeChanged);
  connect(routeController, &RouteController::routeChanged, this, &MainWindow::updateActionStates);

  connect(searchController->getAirportSearch(), &AirportSearch::showRect, mapWidget, &MapWidget::showRect);
  connect(searchController->getAirportSearch(), &AirportSearch::showPos, mapWidget, &MapWidget::showPos);
  connect(
    searchController->getAirportSearch(), &AirportSearch::changeSearchMark, mapWidget,
    &MapWidget::changeSearchMark);
  connect(searchController->getAirportSearch(), &AirportSearch::showInformation,
          infoController, &InfoController::showInformation);

  connect(searchController->getNavSearch(), &NavSearch::showPos, mapWidget, &MapWidget::showPos);
  connect(
    searchController->getNavSearch(), &NavSearch::changeSearchMark, mapWidget, &MapWidget::changeSearchMark);
  connect(searchController->getNavSearch(), &NavSearch::showInformation,
          infoController, &InfoController::showInformation);

  connect(infoController, &InfoController::showPos, mapWidget, &MapWidget::showPos);
  connect(infoController, &InfoController::showRect, mapWidget, &MapWidget::showRect);

  // Use this event to show scenery library dialog on first start after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);
  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
  connect(ui->actionReloadScenery, &QAction::triggered, databaseManager, &DatabaseManager::run);
  connect(ui->actionDatabaseFiles, &QAction::triggered, this, &MainWindow::showDatabaseFiles);

  connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::options);
  connect(ui->actionResetMessages, &QAction::triggered, this, &MainWindow::resetMessages);

  // Flight plan file actions
  connect(ui->actionRouteCenter, &QAction::triggered, this, &MainWindow::routeCenter);
  connect(ui->actionRouteNew, &QAction::triggered, this, &MainWindow::routeNew);
  connect(ui->actionRouteNewFromString, &QAction::triggered, this, &MainWindow::routeNewFromString);
  connect(ui->actionRouteOpen, &QAction::triggered, this, &MainWindow::routeOpen);
  connect(ui->actionRouteAppend, &QAction::triggered, this, &MainWindow::routeAppend);
  connect(ui->actionRouteSave, &QAction::triggered, this, &MainWindow::routeSave);
  connect(ui->actionRouteSaveAs, &QAction::triggered, this, &MainWindow::routeSaveAs);
  connect(ui->actionRouteSaveAsGfp, &QAction::triggered, this, &MainWindow::routeSaveAsGfp);
  connect(routeFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::routeOpenRecent);

  connect(ui->actionPrintMap, &QAction::triggered, printSupport, &PrintSupport::printMap);
  connect(ui->actionPrintFlightplan, &QAction::triggered, printSupport, &PrintSupport::printFlightplan);
  connect(ui->actionSaveMapAsImage, &QAction::triggered, this, &MainWindow::mapSaveImage);

  // KML actions
  connect(ui->actionLoadKml, &QAction::triggered, this, &MainWindow::kmlOpen);
  connect(ui->actionClearKml, &QAction::triggered, this, &MainWindow::kmlClear);
  connect(kmlFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::kmlOpenRecent);

  // Flight plan calculation
  connect(ui->actionRouteCalcDirect, &QAction::triggered,
          routeController, &RouteController::calculateDirect);
  connect(ui->actionRouteCalcRadionav, &QAction::triggered,
          routeController, &RouteController::calculateRadionav);
  connect(ui->actionRouteCalcHighAlt, &QAction::triggered,
          routeController, &RouteController::calculateHighAlt);
  connect(ui->actionRouteCalcLowAlt, &QAction::triggered,
          routeController, &RouteController::calculateLowAlt);
  connect(ui->actionRouteCalcSetAlt, &QAction::triggered,
          routeController, &RouteController::calculateSetAlt);
  connect(ui->actionRouteReverse, &QAction::triggered,
          routeController, &RouteController::reverseRoute);

  connect(ui->actionRouteCopyString, &QAction::triggered,
          routeController, &RouteController::routeStringToClipboard);

  connect(ui->actionRouteAdjustAltitude, &QAction::triggered,
          routeController, &RouteController::adjustFlightplanAltitude);

  // Help menu
  connect(ui->actionHelpContents, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::help);
  connect(ui->actionHelpAbout, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::about);
  connect(ui->actionHelpAboutQt, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::aboutQt);

  // Map widget related connections
  connect(mapWidget, &MapWidget::showInSearch, searchController, &SearchController::showInSearch);
  // Connect the map widget to the position label.
  connect(mapWidget, &MapWidget::distanceChanged, this, &MainWindow::distanceChanged);
  connect(mapWidget, &MapWidget::renderStatusChanged, this, &MainWindow::renderStatusChanged);
  connect(mapWidget, &MapWidget::updateActionStates, this, &MainWindow::updateActionStates);
  connect(mapWidget, &MapWidget::showInformation, infoController, &InfoController::showInformation);

  // Connect toolbar combo boxes
  void (QComboBox::*indexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  connect(mapProjectionComboBox, indexChangedPtr, this, &MainWindow::changeMapProjection);
  connect(mapThemeComboBox, indexChangedPtr, this, &MainWindow::changeMapTheme);

  // Let projection menus update combo boxes
  connect(ui->actionMapProjectionMercator, &QAction::triggered, [ = ](bool checked)
          {
            if(checked)
              mapProjectionComboBox->setCurrentIndex(0);
          });

  connect(ui->actionMapProjectionSpherical, &QAction::triggered, [ = ](bool checked)
          {
            if(checked)
              mapProjectionComboBox->setCurrentIndex(1);
          });

  // Let theme menus update combo boxes
  connect(ui->actionMapThemeOpenStreetMap, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemeOpenStreetMapRoads, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemeOpenTopoMap, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemeStamenTerrain, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemeSimple, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemePlain, &QAction::triggered, this, &MainWindow::themeMenuTriggered);
  connect(ui->actionMapThemeAtlas, &QAction::triggered, this, &MainWindow::themeMenuTriggered);

  for(QAction *action : customMapThemeMenuActions)
    connect(action, &QAction::triggered, this, &MainWindow::themeMenuTriggered);

  // Map object/feature display
  connect(ui->actionMapShowCities, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowGrid, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowHillshading, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAirports, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowSoftAirports, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowEmptyAirports, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAddonAirports, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVor, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowNdb, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowWp, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowIls, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowVictorAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowJetAirways, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowRoute, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);

  connect(ui->actionMapShowAircraft, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftAi, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, this, &MainWindow::updateMapObjectsShown);
  connect(ui->actionMapShowAircraft, &QAction::toggled, profileWidget,
          &ProfileWidget::updateProfileShowFeatures);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, profileWidget,
          &ProfileWidget::updateProfileShowFeatures);

  // Order is important here. First let the mapwidget delete the track then notify the profile
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered, mapWidget,
          &MapWidget::deleteAircraftTrack);
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered, profileWidget,
          &ProfileWidget::deleteAircraftTrack);

  connect(ui->actionMapShowMark, &QAction::triggered, mapWidget, &MapWidget::showSearchMark);
  connect(ui->actionMapShowHome, &QAction::triggered, mapWidget, &MapWidget::showHome);
  connect(ui->actionMapAircraftCenter, &QAction::toggled, mapWidget, &MapWidget::showAircraft);

  connect(ui->actionMapBack, &QAction::triggered, mapWidget, &MapWidget::historyBack);
  connect(ui->actionMapNext, &QAction::triggered, mapWidget, &MapWidget::historyNext);
  connect(ui->actionWorkOffline, &QAction::toggled, mapWidget, &MapWidget::workOffline);

  connect(ui->actionMapMoreDetails, &QAction::triggered, mapWidget, &MapWidget::increaseMapDetail);
  connect(ui->actionMapLessDetails, &QAction::triggered, mapWidget, &MapWidget::decreaseMapDetail);
  connect(ui->actionMapDefaultDetails, &QAction::triggered, mapWidget, &MapWidget::defaultMapDetail);

  connect(mapWidget->getHistory(), &MapPosHistory::historyChanged, this, &MainWindow::updateMapHistoryActions);

  connect(routeController, &RouteController::routeSelectionChanged,
          this, &MainWindow::routeSelectionChanged);
  connect(searchController->getAirportSearch(), &SearchBase::selectionChanged,
          this, &MainWindow::searchSelectionChanged);
  connect(searchController->getNavSearch(), &SearchBase::selectionChanged,
          this, &MainWindow::searchSelectionChanged);

  connect(ui->actionRouteSelectParking, &QAction::triggered,
          routeController, &RouteController::selectDepartureParking);

  // Route editing
  connect(mapWidget, &MapWidget::routeSetStart,
          routeController, &RouteController::routeSetDeparture);
  connect(mapWidget, &MapWidget::routeSetParkingStart,
          routeController, &RouteController::routeSetParking);
  connect(mapWidget, &MapWidget::routeSetDest,
          routeController, &RouteController::routeSetDestination);
  connect(mapWidget, &MapWidget::routeAdd,
          routeController, &RouteController::routeAdd);
  connect(mapWidget, &MapWidget::routeReplace,
          routeController, &RouteController::routeReplace);
  connect(mapWidget, &MapWidget::routeDelete,
          routeController, &RouteController::routeDelete);

  connect(searchController->getAirportSearch(), &SearchBase::routeSetDeparture,
          routeController, &RouteController::routeSetDeparture);
  connect(searchController->getAirportSearch(), &SearchBase::routeSetDestination,
          routeController, &RouteController::routeSetDestination);
  connect(searchController->getAirportSearch(), &SearchBase::routeAdd,
          routeController, &RouteController::routeAdd);

  connect(searchController->getNavSearch(), &SearchBase::routeAdd,
          routeController, &RouteController::routeAdd);

  // Messages about database query result status
  connect(mapQuery, &MapQuery::resultTruncated, this, &MainWindow::resultTruncated);

  connect(databaseManager, &DatabaseManager::preDatabaseLoad, this, &MainWindow::preDatabaseLoad);
  connect(databaseManager, &DatabaseManager::postDatabaseLoad, this, &MainWindow::postDatabaseLoad);

  // Not needed. All properties removed from legend since they are not persistent
  // connect(legendWidget, &Marble::LegendWidget::propertyValueChanged,
  // mapWidget, &MapWidget::setPropertyValue);

  connect(ui->actionAboutMarble, &QAction::triggered,
          marbleAbout, &Marble::MarbleAboutDialog::exec);

  connect(ui->actionConnectSimulator, &QAction::triggered,
          connectClient, &ConnectClient::connectToServerDialog);

  connect(connectClient, &ConnectClient::dataPacketReceived,
          mapWidget, &MapWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived,
          profileWidget, &ProfileWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived,
          infoController, &InfoController::simulatorDataReceived);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          this, &MainWindow::updateActionStates);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          this, &MainWindow::updateActionStates);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          infoController, &InfoController::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          infoController, &InfoController::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          mapWidget, &MapWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          mapWidget, &MapWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          profileWidget, &ProfileWidget::connectedToSimulator);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          profileWidget, &ProfileWidget::disconnectedFromSimulator);

  connect(mapWidget, &MapWidget::aircraftTrackPruned, profileWidget, &ProfileWidget::aircraftTrackPruned);

  connect(weatherReporter, &WeatherReporter::weatherUpdated,
          mapWidget, &MapWidget::updateTooltip);
  connect(weatherReporter, &WeatherReporter::weatherUpdated,
          infoController, &InfoController::updateAirport);

  connect(connectClient, &ConnectClient::weatherUpdated,
          mapWidget, &MapWidget::updateTooltip);
  connect(connectClient, &ConnectClient::weatherUpdated,
          infoController, &InfoController::updateAirport);

  connect(ui->actionHelpNavmapLegend, &QAction::triggered, this, &MainWindow::showNavmapLegend);
  connect(ui->actionHelpMapLegend, &QAction::triggered, this, &MainWindow::showMapLegend);

  connect(&weatherUpdateTimer, &QTimer::timeout, this, &MainWindow::weatherUpdateTimeout);
}

/* Update the info weather */
void MainWindow::weatherUpdateTimeout()
{
  if(connectClient != nullptr && connectClient->isConnected() && infoController != nullptr)
    infoController->updateAirport();
}

void MainWindow::themeMenuTriggered(bool checked)
{
  QAction *action = dynamic_cast<QAction *>(sender());
  if(action != nullptr && checked)
    mapThemeComboBox->setCurrentIndex(action->data().toInt());
}

/* Look for new directories with a valid DGML file in the earth dir */
void MainWindow::findCustomMaps(QFileInfoList& customDgmlFiles)
{
  QDir dir("data/maps/earth");
  QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  // Look for new map themes
  for(const QFileInfo& info : entries)
  {
    if(!STOCK_MAP_THEMES.contains(info.baseName(), Qt::CaseInsensitive))
    {
      // Found a new directory
      qDebug() << "Custom theme" << info.absoluteFilePath();
      QFileInfo dgml(info.filePath() + QDir::separator() + info.baseName() + ".dgml");
      // Check if DGML exists
      if(dgml.exists() && dgml.isFile() && dgml.isReadable())
      {
        // Create a new entry with path relative to "earth"
        QString dgmlFileRelative(QString("earth") +
                                 QDir::separator() + dir.relativeFilePath(info.absoluteFilePath()) +
                                 QDir::separator() + info.baseName() + ".dgml");
        qDebug() << "Custom theme DGML" << dgml.absoluteFilePath() << "relative path" << dgmlFileRelative;
        customDgmlFiles.append(dgmlFileRelative);
      }
      else
        qWarning() << "No DGML found for custom theme" << info.absoluteFilePath();
    }
    else
      qDebug() << "Found stock theme" << info.baseName();
  }
}

/* Called by the toolbar combo box */
void MainWindow::changeMapProjection(int index)
{
  Q_UNUSED(index);

  mapWidget->cancelDragAll();

  Marble::Projection proj = static_cast<Marble::Projection>(mapProjectionComboBox->currentData().toInt());
  qDebug() << "Changing projection to" << proj;
  mapWidget->setProjection(proj);

  // Update menu items
  ui->actionMapProjectionMercator->setChecked(proj == Marble::Mercator);
  ui->actionMapProjectionSpherical->setChecked(proj == Marble::Spherical);

  setStatusMessage(tr("Map projection changed to %1").arg(mapProjectionComboBox->currentText()));
}

/* Called by the toolbar combo box */
void MainWindow::changeMapTheme(int index)
{
  Q_UNUSED(index);

  mapWidget->cancelDragAll();

  QString theme = mapThemeComboBox->currentData().toString();
  qDebug() << "Changing theme to" << theme << "index" << index;
  mapWidget->setTheme(theme, index);

  ui->actionMapThemeOpenStreetMap->setChecked(index == MapWidget::OPENSTREETMAP);
  ui->actionMapThemeOpenStreetMapRoads->setChecked(index == MapWidget::OPENSTREETMAPROADS);
  ui->actionMapThemeOpenTopoMap->setChecked(index == MapWidget::OPENTOPOMAP);
  ui->actionMapThemeStamenTerrain->setChecked(index == MapWidget::STAMENTERRAIN);
  ui->actionMapThemeSimple->setChecked(index == MapWidget::SIMPLE);
  ui->actionMapThemePlain->setChecked(index == MapWidget::PLAIN);
  ui->actionMapThemeAtlas->setChecked(index == MapWidget::ATLAS);

  for(int i = 0; i < customMapThemeMenuActions.size(); i++)
    customMapThemeMenuActions.at(i)->setChecked(index == MapWidget::CUSTOM + i);

  updateLegend();

  setStatusMessage(tr("Map theme changed to %1").arg(mapThemeComboBox->currentText()));
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

  QUrl url = QUrl::fromLocalFile(databaseManager->getDatabaseDirectory());

  if(!QDesktopServices::openUrl(url))
    QMessageBox::warning(this, QApplication::applicationName(), QString(
                           tr("Error opening help URL \"%1\"")).arg(url.toDisplayString()));
}

/* Updates label and tooltip for connection status */
void MainWindow::setConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  connectStatusLabel->setText(text);
  connectStatusLabel->setToolTip(tooltipText);
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
void MainWindow::resultTruncated(maptypes::MapObjectTypes type, int truncatedTo)
{
  if(truncatedTo > 0)
  {
    qDebug() << "resultTruncated" << type << "num" << truncatedTo;
    messageLabel->setText(tr("Too many objects."));
  }
}

void MainWindow::distanceChanged()
{
  mapDistanceLabel->setText(Unit::distMeter(static_cast<float>(mapWidget->distance() * 1000.f)));
}

/* Render status from marble widget */
void MainWindow::renderStatusChanged(RenderStatus status)
{
  switch(status)
  {
    case Marble::Complete:
      renderStatusLabel->setText(tr("Done."));
      break;
    case Marble::WaitingForUpdate:
      renderStatusLabel->setText(tr("Waiting for Update ..."));
      break;
    case Marble::WaitingForData:
      renderStatusLabel->setText(tr("Waiting for Data ..."));
      break;
    case Marble::Incomplete:
      renderStatusLabel->setText(tr("Incomplete."));
      break;
  }
}

/* Route center action */
void MainWindow::routeCenter()
{
  if(!routeController->isFlightplanEmpty())
  {
    mapWidget->showRect(routeController->getBoundingRect(), false);
    setStatusMessage(tr("Flight plan shown on map."));
  }
}

/* Check if route has valid departure  and destination and departure parking.
 *  @return true if route can be saved anyway */
bool MainWindow::routeValidate(bool validateParking)
{
  if(!routeController->hasValidDeparture() || !routeController->hasValidDestination())
  {
    int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOWROUTEWARNING,
                                            tr("Flight Plan must have a valid airport as "
                                               "start and destination and "
                                               "will not be usable by the Simulator."),
                                            tr("Do not show this dialog again and"
                                               " save Flight Plan in the future."),
                                            QMessageBox::Cancel | QMessageBox::Save,
                                            QMessageBox::Cancel, QMessageBox::Save);

    if(result == QMessageBox::Save)
      // Save anyway
      return true;
    else if(result == QMessageBox::Cancel)
      return false;
  }
  else
  {
    if(validateParking)
    {
      if(!routeController->hasValidParking())
      {
        // Airport has parking but no one is selected
        atools::gui::DialogButtonList buttons({
                                                {QString(), QMessageBox::Cancel},
                                                {tr("Select Start Position"), QMessageBox::Yes},
                                                {QString(), QMessageBox::Save}
                                              });

        int result = dialog->showQuestionMsgBox(
          lnm::ACTIONS_SHOWROUTEPARKINGWARNING,
          tr("The start airport has parking spots but no parking was selected for this Flight Plan"),
          tr("Do not show this dialog again and save Flight Plan in the future."),
          buttons, QMessageBox::Yes, QMessageBox::Save);

        if(result == QMessageBox::Yes)
          // saving depends if user selects parking or  cancels out of the dialog
          return routeController->selectDepartureParking();
        else if(result == QMessageBox::Save)
          // Save right away
          return true;
        else if(result == QMessageBox::Cancel)
          return false;
      }
    }
  }
  return true;
}

/* Check if route has valid departure parking.
 *  @return true if route can be saved anyway */
bool RouteController::hasValidParking() const
{
  if(hasValidDeparture())
  {
    const QList<maptypes::MapParking> *parkingCache = query->getParkingsForAirport(route.first().getId());

    if(!parkingCache->isEmpty())
      return route.hasDepartureParking() || route.hasDepartureHelipad();
    else
      // No parking available - so no parking selection is ok
      return true;
  }
  else
    return false;
}

void MainWindow::updateMapPosLabel(const atools::geo::Pos& pos)
{
  if(pos.isValid())
    mapPosLabel->setText(Unit::coords(pos));
  else
    mapPosLabel->setText(tr("No position"));
}

/* Updates main window title with simulator type, flight plan name and change status */
void MainWindow::updateWindowTitle()
{
  QString newTitle = mainWindowTitle;
  newTitle += " - " + databaseManager->getSimulatorShortName();

  if(!routeController->getCurrentRouteFilename().isEmpty())
    newTitle += " - " + QFileInfo(routeController->getCurrentRouteFilename()).fileName() +
                (routeController->hasChanged() ? tr(" *") : QString());
  else if(routeController->hasChanged())
    newTitle += tr(" - *");

  setWindowTitle(newTitle);
}

/* Ask user if flight plan can be deleted when quitting.
 * @return true continue with new flight plan, exit, etc. */
bool MainWindow::routeCheckForChanges()
{
  if(!routeController->hasChanged())
    return true;

  QMessageBox msgBox;
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText(tr("Flight Plan has been changed."));
  msgBox.setInformativeText(tr("Save changes?"));
  msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel);

  int retval = msgBox.exec();

  switch(retval)
  {
    case QMessageBox::Save:
      if(routeController->getCurrentRouteFilename().isEmpty())
        return routeSaveAs();
      else
        return routeSave();

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
void MainWindow::routeNewFromString()
{
  RouteStringDialog routeStringDialog(this, routeController);
  routeStringDialog.restoreState();

  if(routeStringDialog.exec() == QDialog::Accepted)
  {
    if(!routeStringDialog.getFlightplan().isEmpty())
    {
      if(routeCheckForChanges())
      {
        routeController->loadFlightplan(routeStringDialog.getFlightplan(), QString(),
                                        true /*quiet*/, true /*changed*/);
        if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
          routeCenter();
        setStatusMessage(tr("Created new flight plan."));
      }
    }
    else
      qWarning() << "Flight plan is null";
  }
  routeStringDialog.saveState();
}

/* Called from menu or toolbar by action */
void MainWindow::routeNew()
{
  if(routeCheckForChanges())
  {
    routeController->newFlightplan();
    mapWidget->update();
    setStatusMessage(tr("Created new empty flight plan."));
  }
}

/* Called from menu or toolbar by action */
void MainWindow::routeOpen()
{
  if(routeCheckForChanges())
  {
    QString routeFile = dialog->openFileDialog(
      tr("Open Flightplan"),
      tr("Flightplan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN),
      "Route/",
      atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX));

    if(!routeFile.isEmpty())
    {
      if(routeController->loadFlightplan(routeFile))
      {
        routeFileHistory->addFile(routeFile);
        if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
          routeCenter();
        setStatusMessage(tr("Flight plan opened."));
      }
    }
  }
}

/* Called from menu or toolbar by action */
void MainWindow::routeAppend()
{
  QString routeFile = dialog->openFileDialog(
    tr("Append Flightplan"),
    tr("Flightplan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN),
    "Route/",
    atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX));

  if(!routeFile.isEmpty())
  {
    if(routeController->appendFlightplan(routeFile))
    {
      routeFileHistory->addFile(routeFile);
      if(OptionData::instance().getFlags() & opts::GUI_CENTER_ROUTE)
        routeCenter();
      setStatusMessage(tr("Flight plan appended."));
    }
  }
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
        setStatusMessage(tr("Flight plan opened."));
      }
    }
    else
    {
      // File not valid remove from history
      QMessageBox::warning(this, QApplication::applicationName(),
                           tr("File \"%1\" does not exist").arg(routeFile));
      routeFileHistory->removeFile(routeFile);
    }
  }
}

/* Called from menu or toolbar by action */
bool MainWindow::routeSave()
{
  if(routeController->getCurrentRouteFilename().isEmpty() ||
     !routeController->doesFilenameMatchRoute())
    return routeSaveAs();
  else
  {
    if(routeValidate())
    {
      if(routeController->saveFlightplan())
      {
        routeFileHistory->addFile(routeController->getCurrentRouteFilename());
        updateActionStates();
        setStatusMessage(tr("Flight plan saved."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool MainWindow::routeSaveAs()
{
  if(routeValidate())
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flightplan"),
      tr("Flightplan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN),
      "pln", "Route/",
      atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX),
      routeController->buildDefaultFilename());

    if(!routeFile.isEmpty())
    {
      if(routeController->saveFlighplanAs(routeFile))
      {
        routeFileHistory->addFile(routeFile);
        updateActionStates();
        setStatusMessage(tr("Flight plan saved."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool MainWindow::routeSaveAsGfp()
{
  if(routeValidate(false /*valideParking*/))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flightplan as Garmin GFP Format"),
      tr("Garmin GFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GFP),
      "gfp", "Route/Gfp",
      atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX),
      routeController->buildDefaultFilenameGfp());

    if(!routeFile.isEmpty())
    {
      if(routeController->saveFlighplanAsGfp(routeFile))
      {
        setStatusMessage(tr("Flight plan saved as GFP."));
        return true;
      }
    }
  }
  return false;
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

  if(!kmlFile.isEmpty())
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
}

/* Called from menu or toolbar by action */
void MainWindow::kmlOpenRecent(const QString& kmlFile)
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

void MainWindow::mapSaveImage()
{
  QString imageFile = dialog->saveFileDialog(
    tr("Save Map as Image"), tr("Image Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_IMAGE),
    "jpg", "MainWindow/",
    atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX), tr("Little Navmap Screenshot.jpg"));

  if(!imageFile.isEmpty())
  {
    QPixmap pixmap;
    mapWidget->showOverlays(false);
    pixmap = mapWidget->mapScreenShot();
    mapWidget->showOverlays(true);

    PrintSupport::drawWatermark(QPoint(0, pixmap.height()), &pixmap);

    if(!pixmap.save(imageFile))
      QMessageBox::warning(this, QApplication::applicationName(), tr("Error saving image.\n"
                                                                     "Only JPG, PNG and BMP are allowed."));
  }
}

/* Selection in flight plan table has changed */
void MainWindow::routeSelectionChanged(int selected, int total)
{
  Q_UNUSED(selected);
  Q_UNUSED(total);
  QList<int> result;
  routeController->getSelectedRouteMapObjects(result);
  mapWidget->changeRouteHighlights(result);
}

/* Selection in one of the search result tables has changed */
void MainWindow::searchSelectionChanged(const SearchBase *source, int selected, int visible, int total)
{
  static QString selectionLabelText = tr("%1 of %2 %3 selected, %4 visible.");
  QString type;
  if(source == searchController->getAirportSearch())
  {
    type = tr("Airports");
    ui->labelAirportSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible));
  }
  else if(source == searchController->getNavSearch())
  {
    type = tr("Navaids");
    ui->labelNavSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible));
  }

  maptypes::MapSearchResult result;
  searchController->getSelectedMapObjects(result);
  mapWidget->changeSearchHighlights(result);
}

/* A button like airport, vor, ndb, etc. was pressed - update the map */
void MainWindow::updateMapObjectsShown()
{
  mapWidget->updateMapObjectsShown();
  profileWidget->update();
  setStatusMessage(tr("Map settigs changed."));
}

/* Map history has changed */
void MainWindow::updateMapHistoryActions(int minIndex, int curIndex, int maxIndex)
{
  ui->actionMapBack->setEnabled(curIndex > minIndex);
  ui->actionMapNext->setEnabled(curIndex < maxIndex);
}

/* Reset all "do not show this again" message box status values */
void MainWindow::resetMessages()
{
  qDebug() << "resetMessages";
  Settings& s = Settings::instance();

  // Show all message dialogs again
  s.setValue(lnm::ACTIONS_SHOWDISCONNECTINFO, true);
  s.setValue(lnm::ACTIONS_SHOWQUIT, true);
  s.setValue(lnm::ACTIONS_SHOWRESETVIEW, true);
  s.setValue(lnm::ACTIONS_SHOWROUTEPARKINGWARNING, true);
  s.setValue(lnm::ACTIONS_SHOWROUTEWARNING, true);
  s.setValue(lnm::ACTIONS_SHOWROUTEERROR, true);
  s.setValue(lnm::ACTIONS_SHOWROUTE_START_CHANGED, true);
  setStatusMessage(tr("All message dialogs reset."));
}

/* Set a general status message */
void MainWindow::setStatusMessage(const QString& message)
{
  if(statusMessages.isEmpty() || statusMessages.last() != message)
    statusMessages.append(message);

  if(statusMessages.size() > 1)
    statusMessages.removeAt(0);

  ui->statusBar->showMessage(statusMessages.join(" "));
}

void MainWindow::setDetailLabelText(const QString& text)
{
  detailLabel->setText(text);
}

atools::fs::FsPaths::SimulatorType MainWindow::getCurrentSimulator()
{
  return databaseManager->getCurrentSimulator();
}

atools::sql::SqlDatabase *MainWindow::getDatabase() const
{
  return databaseManager->getDatabase();
}

/* From menu: Show options dialog */
void MainWindow::options()
{
  int retval = optionsDialog->exec();
  optionsDialog->hide();

  // Dialog saves its own states
  if(retval == QDialog::Accepted)
    setStatusMessage(tr("Options changed."));
}

/* Called by window shown event when the main window is visible the first time */
void MainWindow::mainWindowShown()
{
  qDebug() << "MainWindow::mainWindowShown()";

  // Postpone loading of KML etc. until now when everything is set up
  mapWidget->mainWindowShown();

  // Focus map widget instead of a random widget
  mapWidget->setFocus();

  if(firstApplicationStart)
  {
    firstApplicationStart = false;
    if(!databaseManager->hasSimulatorDatabases())
    {
      if(databaseManager->hasInstalledSimulators())
        // No databases but simulators let the user create new databases
        databaseManager->run();
      else
      {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QApplication::applicationName());
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(
          tr("No Flight Simulator installations and no scenery library databases found.<br/>"
             "You can copy a Little Navmap scenery library database from another computer.<br/><br/>"
             "Press the help button for more information about this.")
          );
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Help);
        msgBox.setDefaultButton(QMessageBox::Ok);

        int result = msgBox.exec();
        if(result == QMessageBox::Help)
          HelpHandler::openHelpUrl(this, HelpHandler::getHelpUrl(this, "help", "indexnosim.html"));
      }
    }
    // else have databases do nothing
  }

  // If enabled connect to simulator without showing dialog
  connectClient->tryConnectOnStartup();

  weatherUpdateTimeout();

  // Update the weather every 30 seconds if connected
  weatherUpdateTimer.setInterval(30000);
  weatherUpdateTimer.start();

  setStatusMessage(tr("Ready."));
}

/* Enable or disable actions */
void MainWindow::updateActionStates()
{
  qDebug() << "Updating action states";
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());

  ui->actionClearKml->setEnabled(!mapWidget->getKmlFiles().isEmpty());

  ui->actionReloadScenery->setEnabled(databaseManager->hasInstalledSimulators());

  bool hasFlightplan = !routeController->isFlightplanEmpty();
  ui->actionRouteAppend->setEnabled(hasFlightplan);
  ui->actionRouteSave->setEnabled(hasFlightplan && routeController->hasChanged());
  ui->actionRouteSaveAs->setEnabled(hasFlightplan);
  ui->actionRouteSaveAsGfp->setEnabled(hasFlightplan);
  ui->actionRouteCenter->setEnabled(hasFlightplan);
  ui->actionRouteSelectParking->setEnabled(routeController->hasValidDeparture());
  ui->actionMapShowRoute->setEnabled(hasFlightplan);
  ui->actionRouteEditMode->setEnabled(hasFlightplan);
  ui->actionPrintFlightplan->setEnabled(hasFlightplan);
  ui->actionRouteCopyString->setEnabled(hasFlightplan);
  ui->actionRouteAdjustAltitude->setEnabled(hasFlightplan);

  // Remove or add empty airport action from menu and toolbar depending on option
  if(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS)
  {
    // ui->actionMapShowEmptyAirports->setChecked(true);
    ui->actionMapShowEmptyAirports->setEnabled(true);

    if(!ui->mapToolBarOptions->actions().contains(ui->actionMapShowEmptyAirports))
      ui->mapToolBarOptions->insertAction(ui->actionMapShowVor, ui->actionMapShowEmptyAirports);

    if(!ui->menuMap->actions().contains(ui->actionMapShowEmptyAirports))
      ui->menuMap->insertAction(ui->actionMapShowVor, ui->actionMapShowEmptyAirports);
  }
  else
  {
    // Force display - even if action is not visible
    // ui->actionMapShowEmptyAirports->setChecked(true);
    ui->actionMapShowEmptyAirports->setDisabled(true);

    if(ui->mapToolBarOptions->actions().contains(ui->actionMapShowEmptyAirports))
      ui->mapToolBarOptions->removeAction(ui->actionMapShowEmptyAirports);

    if(ui->menuMap->actions().contains(ui->actionMapShowEmptyAirports))
      ui->menuMap->removeAction(ui->actionMapShowEmptyAirports);
  }

  ui->actionMapShowAircraft->setEnabled(connectClient->isConnected());
  ui->actionMapShowAircraftAi->setEnabled(connectClient->isConnected());
  ui->actionMapShowAircraftTrack->setEnabled(!mapWidget->getAircraftTrack().isEmpty());
  ui->actionMapDeleteAircraftTrack->setEnabled(!mapWidget->getAircraftTrack().isEmpty());
  ui->actionMapAircraftCenter->setEnabled(connectClient->isConnected());

  bool canCalcRoute = routeController->canCalcRoute();
  ui->actionRouteCalcDirect->setEnabled(canCalcRoute && routeController->hasEntries());
  ui->actionRouteCalcRadionav->setEnabled(canCalcRoute);
  ui->actionRouteCalcHighAlt->setEnabled(canCalcRoute);
  ui->actionRouteCalcLowAlt->setEnabled(canCalcRoute);
  ui->actionRouteCalcSetAlt->setEnabled(canCalcRoute && ui->spinBoxRouteAlt->value() > 0);
  ui->actionRouteReverse->setEnabled(canCalcRoute);

  ui->actionMapShowHome->setEnabled(mapWidget->getHomePos().isValid());
  ui->actionMapShowMark->setEnabled(mapWidget->getSearchMarkPos().isValid());
}

/* Read settings for all windows, docks, controller and manager classes */
void MainWindow::readSettings()
{
  qDebug() << "MainWindow::readSettings";

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);
  widgetState.restore({this, ui->statusBar, ui->tabWidgetSearch});

  const QRect geo = QApplication::desktop()->availableGeometry(this);

  // Check if window if off screen
  qDebug() << "Screen geometry" << geo << "Win geometry" << frameGeometry();
  if(!geo.intersects(frameGeometry()))
  {
    qDebug() << "Getting window back on screen";
    move(geo.topLeft());
  }

  // Need to be loaded in constructor first since it reads all options
  // optionsDialog->restoreState();

  qDebug() << "MainWindow restoring state of kmlFileHistory";
  kmlFileHistory->restoreState();

  qDebug() << "MainWindow restoring state of searchController";
  routeFileHistory->restoreState();

  qDebug() << "MainWindow restoring state of searchController";
  searchController->restoreState();

  qDebug() << "MainWindow restoring state of mapWidget";
  mapWidget->restoreState();

  qDebug() << "MainWindow restoring state of routeController";
  routeController->restoreState();

  qDebug() << "MainWindow restoring state of connectClient";
  connectClient->restoreState();

  qDebug() << "MainWindow restoring state of infoController";
  infoController->restoreState();

  qDebug() << "MainWindow restoring state of printSupport";
  printSupport->restoreState();

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    widgetState.setBlockSignals(true);
    widgetState.restore({ui->actionMapShowAirports, ui->actionMapShowSoftAirports,
                         ui->actionMapShowEmptyAirports,
                         ui->actionMapShowAddonAirports,
                         ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp,
                         ui->actionMapShowIls,
                         ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
                         ui->actionMapShowRoute, ui->actionMapShowAircraft, ui->actionMapAircraftCenter,
                         ui->actionMapShowAircraftAi,
                         ui->actionMapShowAircraftTrack});
    widgetState.setBlockSignals(false);
  }

  widgetState.restore({mapProjectionComboBox, mapThemeComboBox, ui->actionMapShowGrid,
                       ui->actionMapShowCities,
                       ui->actionMapShowHillshading, ui->actionRouteEditMode,
                       ui->actionWorkOffline});

  firstApplicationStart = Settings::instance().valueBool(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, true);

  // Already loaded in constructor early to allow database creations
  // databaseLoader->restoreState();

  qDebug() << "MainWindow::readSettings done";
}

/* Write settings for all windows, docks, controller and manager classes */
void MainWindow::writeSettings()
{
  qDebug() << "writeSettings";

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET);
  widgetState.save({this, ui->statusBar, ui->tabWidgetSearch});

  qDebug() << "searchController";
  if(searchController != nullptr)
    searchController->saveState();

  qDebug() << "mapWidget";
  if(mapWidget != nullptr)
    mapWidget->saveState();

  qDebug() << "routeController";
  if(routeController != nullptr)
    routeController->saveState();

  qDebug() << "connectClient";
  if(connectClient != nullptr)
    connectClient->saveState();

  qDebug() << "infoController";
  if(infoController != nullptr)
    infoController->saveState();

  qDebug() << "routeFileHistory";
  if(routeFileHistory != nullptr)
    routeFileHistory->saveState();

  qDebug() << "kmlFileHistory";
  if(kmlFileHistory != nullptr)
    kmlFileHistory->saveState();

  qDebug() << "printSupport";
  if(printSupport != nullptr)
    printSupport->saveState();

  qDebug() << "optionsDialog";
  if(optionsDialog != nullptr)
    optionsDialog->saveState();

  qDebug() << "widgetState";
  widgetState.save({mapProjectionComboBox, mapThemeComboBox,
                    ui->actionMapShowAirports, ui->actionMapShowSoftAirports, ui->actionMapShowEmptyAirports,
                    ui->actionMapShowAddonAirports,
                    ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp, ui->actionMapShowIls,
                    ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
                    ui->actionMapShowRoute, ui->actionMapShowAircraft, ui->actionMapAircraftCenter,
                    ui->actionMapShowAircraftAi,
                    ui->actionMapShowAircraftTrack,
                    ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowHillshading,
                    ui->actionRouteEditMode,
                    ui->actionWorkOffline});

  Settings::instance().setValue(lnm::MAINWINDOW_FIRSTAPPLICATIONSTART, firstApplicationStart);

  qDebug() << "databaseManager";
  if(databaseManager != nullptr)
    databaseManager->saveState();

  // qDebug() << "syncSettings";
  // widgetState.syncSettings();
  qDebug() << "save state done";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Catch all close events like Ctrl-Q or Menu/Exit or clicking on the
  // close button on the window frame
  qDebug() << "closeEvent";

  if(routeController != nullptr)
  {
    if(routeController->hasChanged())
    {
      if(!routeCheckForChanges())
        event->ignore();
    }
    else
    {
      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOWQUIT,
                                              tr("Really Quit?"),
                                              tr("Do not &show this dialog again."),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No, QMessageBox::Yes);

      if(result != QMessageBox::Yes)
        event->ignore();
    }
  }
  writeSettings();
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
  qDebug() << "MainWindow::preDatabaseLoad";
  if(!hasDatabaseLoadStatus)
  {
    hasDatabaseLoadStatus = true;

    searchController->preDatabaseLoad();
    routeController->preDatabaseLoad();
    mapWidget->preDatabaseLoad();
    profileWidget->preDatabaseLoad();
    infoController->preDatabaseLoad();
    weatherReporter->preDatabaseLoad();
    infoQuery->deInitQueries();
    mapQuery->deInitQueries();
  }
  else
    qWarning() << "Already in database loading status";
}

/* Call other other classes to reopen queries */
void MainWindow::postDatabaseLoad(atools::fs::FsPaths::SimulatorType type)
{
  qDebug() << "MainWindow::postDatabaseLoad";
  if(hasDatabaseLoadStatus)
  {
    mapQuery->initQueries();
    infoQuery->initQueries();
    searchController->postDatabaseLoad();
    routeController->postDatabaseLoad();
    mapWidget->postDatabaseLoad();
    profileWidget->postDatabaseLoad();
    infoController->postDatabaseLoad();
    weatherReporter->postDatabaseLoad(type);
    hasDatabaseLoadStatus = false;
  }
  else
    qWarning() << "Not in database loading status";

  // Database title might have changed
  updateWindowTitle();

  // Update toolbar buttons
  updateActionStates();
}
