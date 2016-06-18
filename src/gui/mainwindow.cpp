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

#include "logging/loggingdefs.h"
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
#include "common/filehistoryhandler.h"
#include "search/airportsearch.h"
#include "search/navsearch.h"
#include "mapgui/maplayersettings.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"

#include <marble/LegendWidget.h>
#include <marble/MarbleAboutDialog.h>
#include <marble/MarbleModel.h>

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>

#include "ui_mainwindow.h"

using namespace Marble;
using atools::settings::Settings;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::MainWindow)
{
  qDebug() << "MainWindow constructor";

  QString aboutMessage =
    tr("<p>is a fast flight planner and airport search tool for FSX.</p>"
         "<p>This software is licensed under "
           "<a href=\"http://www.gnu.org/licenses/gpl-3.0\">GPL3</a> or any later version.</p>"
             "<p>The source code for this application is available at "
               "<a href=\"https://github.com/albar965\">Github</a>.</p>"
                 "<p><b>Copyright 2015-2016 Alexander Barthel (albar965@mailbox.org).</b></p>");

  ui->setupUi(this);

  dialog = new atools::gui::Dialog(this);
  errorHandler = new atools::gui::ErrorHandler(this);
  helpHandler = new atools::gui::HelpHandler(this, aboutMessage, GIT_REVISION);

  marbleAbout = new Marble::MarbleAboutDialog(this);
  marbleAbout->setApplicationTitle(QApplication::applicationName());

  setupUi();

  mainWindowTitle = windowTitle();

  databaseManager = new DatabaseManager(this);

  databaseManager->openDatabase();
  databaseManager->insertSimSwitchActions(ui->actionDatabaseFiles, ui->menuTools);
  // helpHandler->addDirLink("Database Files", databaseManager->getDatabaseDirectory());

  weatherReporter = new WeatherReporter(this);

  mapQuery = new MapQuery(this, databaseManager->getDatabase());
  mapQuery->initQueries();

  infoQuery = new InfoQuery(this, databaseManager->getDatabase());
  infoQuery->initQueries();

  routeFileHistory = new FileHistoryHandler(this, "Route/FilenamesRecent", ui->menuRecentRoutes,
                                            ui->actionRecentRoutesClear);
  routeController = new RouteController(this, mapQuery, ui->tableViewRoute);

  kmlFileHistory = new FileHistoryHandler(this, "Route/FilenamesKmlRecent", ui->menuRecentKml,
                                          ui->actionClearKmlMenu);

  createNavMap();

  profileWidget = new ProfileWidget(this);
  ui->verticalLayout_12->replaceWidget(ui->elevationWidgetDummy, profileWidget);

  legendWidget = new Marble::LegendWidget(this);
  legendWidget->setMarbleModel(navMapWidget->model());
  ui->verticalLayoutMapLegendInfo->addWidget(legendWidget);

  // Have to create searches in the same order as the tabs
  searchController = new SearchController(this, mapQuery, ui->tabWidgetSearch);
  searchController->createAirportSearch(ui->tableViewAirportSearch);
  searchController->createNavSearch(ui->tableViewNavSearch);

  connectClient = new ConnectClient(this);

  infoController = new InfoController(this, mapQuery, infoQuery);

  connectAllSlots();
  readSettings();
  updateActionStates();

  navMapWidget->setTheme(mapThemeComboBox->currentData().toString(), mapThemeComboBox->currentIndex());
  navMapWidget->setProjection(mapProjectionComboBox->currentData().toInt());
  setMapDetail(mapDetailFactor);

  // Wait until everything is set up
  updateMapShowFeatures();
  navMapWidget->showSavedPos();
  searchController->updateTableSelection();

  profileWidget->updateProfileShowFeatures();
  connectClient->tryConnect();
  loadNavmapLegend();
}

MainWindow::~MainWindow()
{
  qDebug() << "MainWindow destructor";

  delete connectClient;
  delete routeController;
  delete searchController;
  delete weatherReporter;
  delete mapQuery;
  delete infoQuery;
  delete profileWidget;
  delete legendWidget;
  delete marbleAbout;
  delete infoController;
  delete routeFileHistory;
  delete kmlFileHistory;
  delete ui;

  delete dialog;
  delete errorHandler;
  delete databaseManager;

  Settings::shutdown();
  atools::gui::Translator::unload();
  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();

}

void MainWindow::showNavmapLegend()
{
  if(legendUrl.isLocalFile() && legendUrl.host().isEmpty())
  {
    ui->dockWidgetInformation->show();
    ui->tabWidgetInformation->setCurrentIndex(NAVMAP_LEGEND);
  }
  else
    helpHandler->openHelpUrl(legendUrl);
}

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
      errorHandler->handleIOError(legendFile, "While opening Navmap Legend file:");
  }
  else
    helpHandler->openHelpUrl(legendUrl);
}

void MainWindow::legendAnchorClicked(const QUrl& url)
{
  qDebug() << "MainWindow::legendAnchorClicked" << url;

  QUrl newUrl(url);
  if(url.isLocalFile())
    newUrl = helpHandler->getHelpUrl("help", url.fileName(QUrl::FullyDecoded));

  if(!QDesktopServices::openUrl(newUrl))
    QMessageBox::warning(this, QApplication::applicationName(), QString(
                           tr("Error opening URL <i>%1</i>")).arg(url.toDisplayString()));
}

void MainWindow::createNavMap()
{
  // Create a Marble QWidget without a parent
  navMapWidget = new MapWidget(this, mapQuery);

  navMapWidget->setVolatileTileCacheLimit(512 * 1024);

  // mapWidget->setShowSunShading(true);

  // navMapWidget->model()->addGeoDataFile("/home/alex/Dokumente/Google Earth/BC Airports.kmz");
  // mapWidget->model()->addGeoDataFile( "/home/alex/Downloads/map.osm" );

  ui->verticalLayout_10->replaceWidget(ui->mapWidgetDummy, navMapWidget);

  // QSet<QString> pluginEnable;
  // pluginEnable << "Compass" << "Coordinate Grid" << "License" << "Scale Bar" << "Navigation"
  // << "Overview Map" << "Position Marker" << "Elevation Profile" << "Elevation Profile Marker"
  // << "Download Progress Indicator";

  // pluginDisable << "Annotation" << "Amateur Radio Aprs Plugin" << "Atmosphere" << "Compass" <<
  // "Crosshairs" << "Earthquakes" << "Eclipses" << "Elevation Profile" << "Elevation Profile Marker" <<
  // "Places" << "GpsInfo" << "Coordinate Grid" << "License" << "Scale Bar" << "Measure Tool" << "Navigation" <<
  // "OpenCaching.Com" << "OpenDesktop Items" << "Overview Map" << "Photos" << "Position Marker" <<
  // "Postal Codes" << "Download Progress Indicator" << "Routing" << "Satellites" << "Speedometer" << "Stars" <<
  // "Sun" << "Weather" << "Wikipedia Articles";

  // QList<RenderPlugin *> localRenderPlugins = mapWidget->renderPlugins();
  // for(RenderPlugin *p : localRenderPlugins)
  // if(!pluginEnable.contains(p->name()))
  // {
  // qDebug() << "Disabled plugin" << p->name();
  // p->setEnabled(false);
  // }
  // else
  // qDebug() << "Found plugin" << p->name();

}

void MainWindow::setupUi()
{
  ui->mapToolBar->addSeparator();

  mapProjectionComboBox = new QComboBox(this);
  mapProjectionComboBox->setObjectName("mapProjectionComboBox");
  QString helpText = tr("Select map projection");
  mapProjectionComboBox->setToolTip(helpText);
  mapProjectionComboBox->setStatusTip(helpText);
  mapProjectionComboBox->addItem(tr("Mercator"), Marble::Mercator);
  mapProjectionComboBox->addItem(tr("Spherical"), Marble::Spherical);
  ui->mapToolBar->addWidget(mapProjectionComboBox);

  mapThemeComboBox = new QComboBox(this);
  mapThemeComboBox->setObjectName("mapThemeComboBox");
  helpText = tr("Select map theme");
  mapThemeComboBox->setToolTip(helpText);
  mapThemeComboBox->setStatusTip(helpText);
  mapThemeComboBox->addItem(tr("OpenStreetMap"),
                            "earth/openstreetmap/openstreetmap.dgml");
  mapThemeComboBox->addItem(tr("OpenTopoMap"),
                            "earth/opentopomap/opentopomap.dgml");
  mapThemeComboBox->addItem(tr("Simple"),
                            "earth/political/political.dgml");
  mapThemeComboBox->addItem(tr("Plain"),
                            "earth/plain/plain.dgml");
  ui->mapToolBar->addWidget(mapThemeComboBox);

  ui->dockWidgetSearch->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/searchdock.svg"));
  ui->dockWidgetSearch->toggleViewAction()->setShortcut(QKeySequence("Alt+1"));
  ui->dockWidgetSearch->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                       arg(ui->dockWidgetSearch->windowTitle().toLower()));
  ui->dockWidgetSearch->toggleViewAction()->setStatusTip(ui->dockWidgetSearch->toggleViewAction()->toolTip());

  ui->dockWidgetRoute->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/routedock.svg"));
  ui->dockWidgetRoute->toggleViewAction()->setShortcut(QKeySequence("Alt+2"));
  ui->dockWidgetRoute->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                      arg(ui->dockWidgetRoute->windowTitle().toLower()));
  ui->dockWidgetRoute->toggleViewAction()->setStatusTip(ui->dockWidgetRoute->toggleViewAction()->toolTip());

  ui->dockWidgetInformation->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/infodock.svg"));
  ui->dockWidgetInformation->toggleViewAction()->setShortcut(QKeySequence("Alt+3"));
  ui->dockWidgetInformation->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                            arg(ui->dockWidgetInformation->windowTitle().
                                                                toLower()));
  ui->dockWidgetInformation->toggleViewAction()->setStatusTip(
    ui->dockWidgetInformation->toggleViewAction()->toolTip());

  ui->dockWidgetElevation->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/profiledock.svg"));
  ui->dockWidgetElevation->toggleViewAction()->setShortcut(QKeySequence("Alt+4"));
  ui->dockWidgetElevation->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                          arg(ui->dockWidgetElevation->windowTitle().toLower()));
  ui->dockWidgetElevation->toggleViewAction()->setStatusTip(
    ui->dockWidgetElevation->toggleViewAction()->toolTip());

  ui->dockWidgetAircraft->toggleViewAction()->setIcon(QIcon(":/littlenavmap/resources/icons/aircraftdock.svg"));
  ui->dockWidgetAircraft->toggleViewAction()->setShortcut(QKeySequence("Alt+5"));
  ui->dockWidgetAircraft->toggleViewAction()->setToolTip(tr("Open or show the %1 dock window").
                                                         arg(ui->dockWidgetAircraft->windowTitle().toLower()));
  ui->dockWidgetAircraft->toggleViewAction()->setStatusTip(
    ui->dockWidgetAircraft->toggleViewAction()->toolTip());

  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->dockWidgetSearch->toggleViewAction(),
                               ui->dockWidgetRoute->toggleViewAction(),
                               ui->dockWidgetInformation->toggleViewAction(),
                               ui->dockWidgetElevation->toggleViewAction(),
                               ui->dockWidgetAircraft->toggleViewAction()});

  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  ui->menuView->insertActions(ui->actionShowStatusbar,
                              {ui->mainToolBar->toggleViewAction(),
                               ui->mapToolBar->toggleViewAction(),
                               ui->routeToolBar->toggleViewAction(),
                               ui->viewToolBar->toggleViewAction()});
  ui->menuView->insertSeparator(ui->actionShowStatusbar);

  ui->viewToolBar->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetInformation->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetElevation->toggleViewAction());
  ui->viewToolBar->addAction(ui->dockWidgetAircraft->toggleViewAction());

  // Create labels for the statusbar
  messageLabel = new QLabel();
  messageLabel->setMinimumWidth(100);
  ui->statusBar->addPermanentWidget(messageLabel);

  detailLabel = new QLabel();
  detailLabel->setMinimumWidth(100);
  ui->statusBar->addPermanentWidget(detailLabel);

  renderStatusLabel = new QLabel();
  renderStatusLabel->setMinimumWidth(120);
  ui->statusBar->addPermanentWidget(renderStatusLabel);

  mapDistanceLabel = new QLabel();
  mapDistanceLabel->setMinimumWidth(80);
  ui->statusBar->addPermanentWidget(mapDistanceLabel);

  mapPosLabel = new QLabel();
  mapPosLabel->setMinimumWidth(200);
  ui->statusBar->addPermanentWidget(mapPosLabel);
}

void MainWindow::connectAllSlots()
{
  qDebug() << "Connecting slots";

  connect(ui->textBrowserNavmapLegendInfo, &QTextBrowser::anchorClicked, this,
          &MainWindow::legendAnchorClicked);

  connect(ui->actionMapSetHome, &QAction::triggered, navMapWidget, &MapWidget::changeHome);

  connect(routeController, &RouteController::showRect, navMapWidget, &MapWidget::showRect);
  connect(routeController, &RouteController::showPos, navMapWidget, &MapWidget::showPos);
  connect(routeController, &RouteController::changeMark, navMapWidget, &MapWidget::changeMark);
  connect(routeController, &RouteController::routeChanged, navMapWidget, &MapWidget::routeChanged);
  connect(routeController, &RouteController::showInformation,
          infoController, &InfoController::showInformation);

  connect(profileWidget, &ProfileWidget::highlightProfilePoint,
          navMapWidget, &MapWidget::highlightProfilePoint);

  connect(routeController, &RouteController::routeChanged, profileWidget, &ProfileWidget::routeChanged);
  connect(routeController, &RouteController::routeChanged, this, &MainWindow::updateActionStates);

  connect(searchController->getAirportSearch(), &AirportSearch::showRect,
          navMapWidget, &MapWidget::showRect);
  connect(searchController->getAirportSearch(), &AirportSearch::showPos,
          navMapWidget, &MapWidget::showPos);
  connect(searchController->getAirportSearch(), &AirportSearch::changeMark,
          navMapWidget, &MapWidget::changeMark);
  connect(searchController->getAirportSearch(), &AirportSearch::showInformation,
          infoController, &InfoController::showInformation);

  connect(searchController->getNavSearch(), &NavSearch::showPos, navMapWidget, &MapWidget::showPos);
  connect(searchController->getNavSearch(), &NavSearch::changeMark, navMapWidget, &MapWidget::changeMark);
  connect(searchController->getNavSearch(), &NavSearch::showInformation,
          infoController, &InfoController::showInformation);

  // Use this event to show path dialog after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);
  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
  connect(ui->actionReloadScenery, &QAction::triggered, databaseManager, &DatabaseManager::run);
  connect(ui->actionDatabaseFiles, &QAction::triggered, this, &MainWindow::showDatabaseFiles);

  connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::options);
  connect(ui->actionResetMessages, &QAction::triggered, this, &MainWindow::resetMessages);

  connect(ui->actionRouteCenter, &QAction::triggered, this, &MainWindow::routeCenter);
  connect(ui->actionRouteNew, &QAction::triggered, this, &MainWindow::routeNew);
  connect(ui->actionRouteOpen, &QAction::triggered, this, &MainWindow::routeOpen);
  connect(ui->actionRouteSave, &QAction::triggered, this, &MainWindow::routeSave);
  connect(ui->actionRouteSaveAs, &QAction::triggered, this, &MainWindow::routeSaveAs);
  connect(routeFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::routeOpenRecent);

  connect(ui->actionLoadKml, &QAction::triggered, this, &MainWindow::kmlOpen);
  connect(ui->actionClearKml, &QAction::triggered, this, &MainWindow::kmlClear);
  connect(kmlFileHistory, &FileHistoryHandler::fileSelected, this, &MainWindow::kmlOpenRecent);

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
          routeController, &RouteController::reverse);

  connect(ui->actionHelpContents, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::help);
  connect(ui->actionHelpAbout, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::about);
  connect(ui->actionHelpAboutQt, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::aboutQt);

  // Map widget related connections
  connect(navMapWidget, &MapWidget::objectSelected, searchController, &SearchController::objectSelected);
  // Connect the map widget to the position label.
  connect(navMapWidget, &MapWidget::mouseMoveGeoPosition, mapPosLabel, &QLabel::setText);
  connect(navMapWidget, &MapWidget::distanceChanged, mapDistanceLabel, &QLabel::setText);
  connect(navMapWidget, &MapWidget::renderStatusChanged, this, &MainWindow::renderStatusChanged);
  connect(navMapWidget, &MapWidget::updateActionStates, this, &MainWindow::updateActionStates);
  connect(navMapWidget, &MapWidget::showInformation, infoController, &InfoController::showInformation);

  void (QComboBox::*indexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  connect(mapProjectionComboBox, indexChangedPtr, [ = ](int)
          {
            Marble::Projection proj =
              static_cast<Marble::Projection>(mapProjectionComboBox->currentData().toInt());
            qDebug() << "Changing projection to" << proj;
            navMapWidget->setProjection(proj);
          });

  connect(mapThemeComboBox, indexChangedPtr, navMapWidget, [ = ](int index)
          {
            QString theme = mapThemeComboBox->currentData().toString();
            qDebug() << "Changing theme to" << theme << "index" << index;
            navMapWidget->setTheme(theme, index);
          });

  connect(ui->actionMapShowCities, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowGrid, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowHillshading, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowSoftAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowEmptyAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowAddonAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowVor, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowNdb, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowWp, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowIls, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowVictorAirways, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowJetAirways, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowRoute, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);

  connect(ui->actionMapShowAircraft, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowAircraft, &QAction::toggled, profileWidget,
          &ProfileWidget::updateProfileShowFeatures);
  connect(ui->actionMapShowAircraftTrack, &QAction::toggled, profileWidget,
          &ProfileWidget::updateProfileShowFeatures);

  // Order is important. First let the mapwidget delete the track then notify the profile
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered, navMapWidget,
          &MapWidget::deleteAircraftTrack);
  connect(ui->actionMapDeleteAircraftTrack, &QAction::triggered, profileWidget,
          &ProfileWidget::deleteAircraftTrack);

  connect(ui->actionMapShowMark, &QAction::triggered, navMapWidget, &MapWidget::showMark);
  connect(ui->actionMapShowHome, &QAction::triggered, navMapWidget, &MapWidget::showHome);
  connect(ui->actionMapAircraftCenter, &QAction::toggled, navMapWidget, &MapWidget::showAircraft);

  connect(ui->actionMapBack, &QAction::triggered, navMapWidget, &MapWidget::historyBack);
  connect(ui->actionMapNext, &QAction::triggered, navMapWidget, &MapWidget::historyNext);
  connect(ui->actionWorkOffline, &QAction::toggled, navMapWidget, &MapWidget::workOffline);

  connect(ui->actionMapMoreDetails, &QAction::triggered, this, &MainWindow::increaseMapDetail);
  connect(ui->actionMapLessDetails, &QAction::triggered, this, &MainWindow::decreaseMapDetail);
  connect(ui->actionMapDefaultDetails, &QAction::triggered, this, &MainWindow::defaultMapDetail);

  connect(navMapWidget->getHistory(), &MapPosHistory::historyChanged, this, &MainWindow::updateHistActions);

  connect(routeController, &RouteController::routeSelectionChanged,
          this, &MainWindow::routeSelectionChanged);
  connect(searchController->getAirportSearch(), &Search::selectionChanged,
          this, &MainWindow::selectionChanged);
  connect(searchController->getNavSearch(), &Search::selectionChanged,
          this, &MainWindow::selectionChanged);

  connect(ui->actionRouteSelectParking, &QAction::triggered,
          routeController, &RouteController::selectDepartureParking);

  connect(navMapWidget, &MapWidget::routeSetStart,
          routeController, &RouteController::routeSetStart);
  connect(navMapWidget, &MapWidget::routeSetParkingStart,
          routeController, &RouteController::routeSetParking);
  connect(navMapWidget, &MapWidget::routeSetDest,
          routeController, &RouteController::routeSetDest);
  connect(navMapWidget, &MapWidget::routeAdd,
          routeController, &RouteController::routeAdd);
  connect(navMapWidget, &MapWidget::routeReplace,
          routeController, &RouteController::routeReplace);
  connect(navMapWidget, &MapWidget::routeDelete,
          routeController, &RouteController::routeDelete);

  connect(searchController->getAirportSearch(), &Search::routeSetStart,
          routeController, &RouteController::routeSetStart);
  connect(searchController->getAirportSearch(), &Search::routeSetDest,
          routeController, &RouteController::routeSetDest);
  connect(searchController->getAirportSearch(), &Search::routeAdd,
          routeController, &RouteController::routeAdd);

  connect(searchController->getNavSearch(), &Search::routeAdd,
          routeController, &RouteController::routeAdd);

  connect(mapQuery, &MapQuery::resultTruncated, this, &MainWindow::resultTruncated);

  connect(databaseManager, &DatabaseManager::preDatabaseLoad, this, &MainWindow::preDatabaseLoad);
  connect(databaseManager, &DatabaseManager::postDatabaseLoad, this, &MainWindow::postDatabaseLoad);

  connect(legendWidget, &Marble::LegendWidget::propertyValueChanged,
          navMapWidget, &MapWidget::setPropertyValue);
  connect(ui->actionAboutMarble, &QAction::triggered,
          marbleAbout, &Marble::MarbleAboutDialog::exec);

  connect(ui->actionConnectSimulator, &QAction::triggered,
          connectClient, &ConnectClient::connectToServer);

  connect(connectClient, &ConnectClient::dataPacketReceived,
          navMapWidget, &MapWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived,
          profileWidget, &ProfileWidget::simDataChanged);
  connect(connectClient, &ConnectClient::dataPacketReceived,
          infoController, &InfoController::dataPacketReceived);

  connect(connectClient, &ConnectClient::connectedToSimulator,
          this, &MainWindow::updateActionStates);
  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          this, &MainWindow::updateActionStates);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          infoController, &InfoController::disconnectedFromSimulator);
  connect(connectClient, &ConnectClient::connectedToSimulator,
          infoController, &InfoController::connectedToSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          navMapWidget, &MapWidget::disconnectedFromSimulator);

  connect(connectClient, &ConnectClient::disconnectedFromSimulator,
          profileWidget, &ProfileWidget::disconnectedFromSimulator);

  connect(weatherReporter, &WeatherReporter::weatherUpdated,
          navMapWidget, &MapWidget::updateTooltip);
  connect(weatherReporter, &WeatherReporter::weatherUpdated,
          infoController, &InfoController::updateAirport);

  connect(ui->actionHelpNavmapLegend, &QAction::triggered, this, &MainWindow::showNavmapLegend);
}

void MainWindow::showDatabaseFiles()
{

  QUrl url = QUrl::fromLocalFile(databaseManager->getDatabaseDirectory());

  if(!QDesktopServices::openUrl(url))
    QMessageBox::warning(this, QApplication::applicationName(), QString(
                           tr("Error opening help URL <i>%1</i>")).arg(url.toDisplayString()));
}

void MainWindow::setMessageText(const QString& text, const QString& tooltipText)
{
  messageLabel->setText(text);
  messageLabel->setToolTip(tooltipText);
}

const ElevationModel *MainWindow::getElevationModel()
{
  return navMapWidget->model()->elevationModel();
}

void MainWindow::resultTruncated(maptypes::MapObjectTypes type, int truncatedTo)
{
  if(truncatedTo > 0)
    qDebug() << "resultTruncated" << type << "num" << truncatedTo;
  if(truncatedTo > 0)
    messageLabel->setText(tr("Too many objects."));
}

void MainWindow::renderStatusChanged(RenderStatus status)
{
  switch(status)
  {
    case Marble::Complete:
      renderStatusLabel->setText("Done.");
      break;
    case Marble::WaitingForUpdate:
      renderStatusLabel->setText("Waiting for Update ...");
      break;
    case Marble::WaitingForData:
      renderStatusLabel->setText("Waiting for Data ...");
      break;
    case Marble::Incomplete:
      renderStatusLabel->setText("Incomplete.");
      break;

  }
}

void MainWindow::routeCenter()
{
  navMapWidget->showRect(routeController->getBoundingRect());
}

bool MainWindow::routeValidate()
{
  if(!routeController->hasValidStart() || !routeController->hasValidDestination())
    dialog->showInfoMsgBox("Actions/ShowRouteWarning",
                           tr("Flight Plan must have an airport as start and destination and "
                              "will not be usable by the Simulator."),
                           tr("Do not &show this dialog again."));
  else
  {
    if(!routeController->hasValidParking())
    {
      atools::gui::DialogButtonList buttons({
                                              {QString(), QMessageBox::Cancel},
                                              {"Select Parking", QMessageBox::Yes},
                                              {QString(), QMessageBox::Save}
                                            });

      int result = dialog->showQuestionMsgBox(
        "Actions/ShowRouteParkingWarning",
        "The start airport has parking spots but no parking was selected for this Flight Plan",
        tr("Do not show this dialog again and save Flight Plan in the future."),
        buttons, QMessageBox::Yes, QMessageBox::Save);

      if(result == QMessageBox::Yes)
        return routeController->selectDepartureParking();
      else if(result == QMessageBox::Save)
        return true;
      else if(result == QMessageBox::Cancel)
        return false;
    }
  }
  return true;
}

void MainWindow::updateWindowTitle()
{
  QString newTitle = mainWindowTitle;
  newTitle += " - " + databaseManager->getSimShortName();

  if(!routeController->getRouteFilename().isEmpty())
    newTitle += " - " + QFileInfo(routeController->getRouteFilename()).fileName() +
                (routeController->hasChanged() ? " *" : QString());
  else if(routeController->hasChanged())
    newTitle += " - *";

  setWindowTitle(newTitle);
}

bool MainWindow::routeCheckForChanges()
{
  if(!routeController->hasChanged())
    return true;

  QMessageBox msgBox;
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText("Flight Plan has been changed.");
  msgBox.setInformativeText("Save changes?");
  msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel);

  int retval = msgBox.exec();

  switch(retval)
  {
    case QMessageBox::Save:
      if(routeController->getRouteFilename().isEmpty())
        return routeSaveAs();
      else
        return routeSave();

    case QMessageBox::No:
      // ok to erase route
      return true;

    case QMessageBox::Cancel:
      // stop any route erasing actions
      return false;
  }
  return false;
}

void MainWindow::routeNew()
{
  if(routeCheckForChanges())
  {
    routeController->newFlightplan();
    navMapWidget->update();
  }
}

void MainWindow::routeOpen()
{
  if(routeCheckForChanges())
  {
    QString routeFile = dialog->openFileDialog(tr("Open Flightplan"),
                                               tr("Flightplan Files (*.pln *.PLN);;All Files (*)"),
                                               "Route/",
                                               atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX));

    if(!routeFile.isEmpty())
    {
      if(routeController->loadFlightplan(routeFile))
      {
        routeFileHistory->addFile(routeFile);
        routeCenter();
      }
    }
  }
}

void MainWindow::routeOpenRecent(const QString& routeFile)
{
  if(routeCheckForChanges())
  {
    if(QFile::exists(routeFile))
    {
      if(routeController->loadFlightplan(routeFile))
        routeCenter();
    }
    else
    {
      QMessageBox::warning(this, QApplication::applicationName(),
                           QString("File \"%1\" does not exist").arg(routeFile));
      routeFileHistory->removeFile(routeFile);
    }
  }
}

bool MainWindow::routeSave()
{
  if(routeController->getRouteFilename().isEmpty())
    return routeSaveAs();
  else
  {
    if(routeValidate())
    {
      if(routeController->saveFlightplan())
      {
        routeFileHistory->addFile(routeController->getRouteFilename());
        updateActionStates();
        return true;
      }
    }
  }
  return false;
}

bool MainWindow::routeSaveAs()
{
  if(routeValidate())
  {
    QString routeFile = dialog->saveFileDialog(tr("Save Flightplan"),
                                               tr("Flightplan Files (*.pln *.PLN);;All Files (*)"),
                                               "pln", "Route/",
                                               atools::fs::FsPaths::getFilesPath(atools::fs::FsPaths::FSX),
                                               routeController->getDefaultFilename());

    if(!routeFile.isEmpty())
    {
      if(routeController->saveFlighplanAs(routeFile))
      {
        routeFileHistory->addFile(routeFile);
        updateActionStates();
        return true;
      }
    }
  }
  return false;
}

void MainWindow::kmlClear()
{
  navMapWidget->clearKmlFiles();
}

void MainWindow::kmlOpen()
{
  QString kmlFile = dialog->openFileDialog(tr("Google Earth KML"),
                                           tr("Google Earth KML (*.kml *.KML *.kmz *.KMZ);;All Files (*)"),
                                           "Kml/", QString());

  if(!kmlFile.isEmpty())
  {
    kmlFileHistory->addFile(kmlFile);
    navMapWidget->addKmlFile(kmlFile);
  }
}

void MainWindow::kmlOpenRecent(const QString& kmlFile)
{
  if(QFile::exists(kmlFile))
    navMapWidget->addKmlFile(kmlFile);
  else
  {
    QMessageBox::warning(this, QApplication::applicationName(),
                         QString("File \"%1\" does not exist").arg(kmlFile));
    kmlFileHistory->removeFile(kmlFile);
  }
}

void MainWindow::defaultMapDetail()
{
  mapDetailFactor = MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailFactor);
}

void MainWindow::increaseMapDetail()
{
  if(mapDetailFactor < MAP_MAX_DETAIL_FACTOR)
  {
    mapDetailFactor++;
    setMapDetail(mapDetailFactor);
  }
}

void MainWindow::decreaseMapDetail()
{
  if(mapDetailFactor > MAP_MIN_DETAIL_FACTOR)
  {
    mapDetailFactor--;
    setMapDetail(mapDetailFactor);
  }
}

void MainWindow::setMapDetail(int factor)
{
  mapDetailFactor = factor;
  navMapWidget->setDetailFactor(mapDetailFactor);
  ui->actionMapMoreDetails->setEnabled(mapDetailFactor < MAP_MAX_DETAIL_FACTOR);
  ui->actionMapLessDetails->setEnabled(mapDetailFactor > MAP_MIN_DETAIL_FACTOR);
  ui->actionMapDefaultDetails->setEnabled(mapDetailFactor != MAP_DEFAULT_DETAIL_FACTOR);
  navMapWidget->update();

  int det = mapDetailFactor - MAP_DEFAULT_DETAIL_FACTOR;
  QString detStr;
  if(det == 0)
    detStr = tr("Normal");
  else if(det > 0)
    detStr = "+" + QString::number(det);
  else if(det < 0)
    detStr = QString::number(det);

  detailLabel->setText(tr("Detail %1").arg(detStr));
}

void MainWindow::routeSelectionChanged(int selected, int total)
{
  Q_UNUSED(selected);
  Q_UNUSED(total);
  RouteMapObjectList result;
  routeController->getSelectedRouteMapObjects(result);
  navMapWidget->changeRouteHighlight(result);
}

void MainWindow::selectionChanged(const Search *source, int selected, int visible, int total)
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
  navMapWidget->changeHighlight(result);
}

void MainWindow::updateMapShowFeatures()
{
  navMapWidget->updateMapShowFeatures();
  profileWidget->update();
}

void MainWindow::updateHistActions(int minIndex, int curIndex, int maxIndex)
{
  qDebug() << "History changed min" << minIndex << "cur" << curIndex << "max" << maxIndex;
  ui->actionMapBack->setEnabled(curIndex > minIndex);
  ui->actionMapNext->setEnabled(curIndex < maxIndex);
}

void MainWindow::resetMessages()
{
  // TODO reset messages
}

void MainWindow::options()
{
  // TODO create options dialog
  // QtMarbleConfigDialog dlg(mapWidget);
  // dlg.exec();

}

void MainWindow::mainWindowShown()
{
  qDebug() << "MainWindow::mainWindowShown()";

  if(firstApplicationStart)
  {
    firstApplicationStart = false;
    if(databaseManager->hasRegistrySims())
      databaseManager->run();
    else
      QMessageBox::information(this, QApplication::applicationName(), "No Simulators found.");
  }
}

void MainWindow::updateActionStates()
{
  qDebug() << "Updating action states";
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());

  bool hasFlightplan = !routeController->isFlightplanEmpty();
  bool hasStartAndDest = routeController->hasValidStart() && routeController->hasValidDestination();

  ui->actionReloadScenery->setEnabled(databaseManager->hasRegistrySims());

  ui->actionRouteSave->setEnabled(hasFlightplan && routeController->hasChanged());
  ui->actionRouteSaveAs->setEnabled(hasFlightplan);
  ui->actionRouteCenter->setEnabled(hasFlightplan);
  ui->actionRouteSelectParking->setEnabled(routeController->hasValidStart());
  ui->actionMapShowRoute->setEnabled(hasFlightplan);
  ui->actionRouteEditMode->setEnabled(hasFlightplan);

  ui->actionMapShowAircraft->setEnabled(connectClient->isConnected());
  ui->actionMapShowAircraftTrack->setEnabled(connectClient->isConnected());
  ui->actionMapDeleteAircraftTrack->setEnabled(!navMapWidget->getAircraftTrack().isEmpty());
  ui->actionMapAircraftCenter->setEnabled(connectClient->isConnected());

  ui->actionRouteCalcDirect->setEnabled(hasStartAndDest && routeController->hasEntries());
  ui->actionRouteCalcRadionav->setEnabled(hasStartAndDest);
  ui->actionRouteCalcHighAlt->setEnabled(hasStartAndDest);
  ui->actionRouteCalcLowAlt->setEnabled(hasStartAndDest);
  ui->actionRouteCalcSetAlt->setEnabled(hasStartAndDest && ui->spinBoxRouteAlt->value() > 0);
  ui->actionRouteReverse->setEnabled(hasFlightplan);

  ui->actionMapShowHome->setEnabled(navMapWidget->getHomePos().isValid());
  ui->actionMapShowMark->setEnabled(navMapWidget->getMarkPos().isValid());
}

void MainWindow::readSettings()
{
  qDebug() << "readSettings";

  atools::gui::WidgetState ws("MainWindow/Widget");
  ws.restore({this, ui->statusBar, ui->tabWidgetSearch});

  kmlFileHistory->restoreState();
  routeFileHistory->restoreState();
  searchController->restoreState();
  navMapWidget->restoreState();
  routeController->restoreState();
  connectClient->restoreState();
  infoController->restoreState();

  ws.restore({mapProjectionComboBox, mapThemeComboBox,
              ui->actionMapShowAirports, ui->actionMapShowSoftAirports, ui->actionMapShowEmptyAirports,
              ui->actionMapShowAddonAirports,
              ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp, ui->actionMapShowIls,
              ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
              ui->actionMapShowRoute, ui->actionMapShowAircraft, ui->actionMapAircraftCenter,
              ui->actionMapShowAircraftTrack,
              ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowHillshading,
              ui->actionRouteEditMode,
              ui->actionWorkOffline});

  mapDetailFactor = Settings::instance().valueInt("Map/DetailFactor",
                                                  MAP_DEFAULT_DETAIL_FACTOR);

  firstApplicationStart = Settings::instance().valueBool("MainWindow/FirstApplicationStart", true);

  // Already loaded in constructor early to allow database creations
  // databaseLoader->restoreState();
}

void MainWindow::writeSettings()
{
  qDebug() << "writeSettings";

  atools::gui::WidgetState ws("MainWindow/Widget");
  ws.save({this, ui->statusBar, ui->tabWidgetSearch});

  if(searchController != nullptr)
    searchController->saveState();
  if(navMapWidget != nullptr)
    navMapWidget->saveState();
  if(routeController != nullptr)
    routeController->saveState();
  if(connectClient != nullptr)
    connectClient->saveState();
  if(infoController != nullptr)
    infoController->saveState();
  if(routeFileHistory != nullptr)
    routeFileHistory->saveState();
  if(kmlFileHistory != nullptr)
    kmlFileHistory->saveState();

  ws.save({mapProjectionComboBox, mapThemeComboBox,
           ui->actionMapShowAirports, ui->actionMapShowSoftAirports, ui->actionMapShowEmptyAirports,
           ui->actionMapShowAddonAirports,
           ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp, ui->actionMapShowIls,
           ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
           ui->actionMapShowRoute, ui->actionMapShowAircraft, ui->actionMapAircraftCenter,
           ui->actionMapShowAircraftTrack,
           ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowHillshading,
           ui->actionRouteEditMode,
           ui->actionWorkOffline});

  Settings::instance().setValue("Map/DetailFactor", mapDetailFactor);
  Settings::instance().setValue("MainWindow/FirstApplicationStart", firstApplicationStart);

  if(databaseManager != nullptr)
    databaseManager->saveState();

  ws.syncSettings();
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
      int result = dialog->showQuestionMsgBox("Actions/ShowQuit",
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

void MainWindow::preDatabaseLoad()
{
  qDebug() << "MainWindow::preDatabaseLoad";
  if(!hasDatabaseLoadStatus)
  {
    hasDatabaseLoadStatus = true;

    searchController->preDatabaseLoad();
    routeController->preDatabaseLoad();
    navMapWidget->preDatabaseLoad();
    profileWidget->preDatabaseLoad();
    infoController->preDatabaseLoad();

    infoQuery->deInitQueries();
    mapQuery->deInitQueries();
  }
  else
    qWarning() << "Already in database loading status";
}

void MainWindow::postDatabaseLoad()
{
  qDebug() << "MainWindow::postDatabaseLoad";
  if(hasDatabaseLoadStatus)
  {
    mapQuery->initQueries();
    infoQuery->initQueries();
    searchController->postDatabaseLoad();
    routeController->postDatabaseLoad();
    navMapWidget->postDatabaseLoad();
    profileWidget->postDatabaseLoad();
    infoController->postDatabaseLoad();
    hasDatabaseLoadStatus = false;
  }
  else
    qWarning() << "Not in database loading status";

  // Database title might have changed
  updateWindowTitle();
}
