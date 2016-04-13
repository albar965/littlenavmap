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

#include "db/databaseloader.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "sql/sqldatabase.h"
#include "settings/settings.h"
#include "logging/loggingdefs.h"
#include "logging/logginghandler.h"
#include "gui/translator.h"
#include "fs/fspaths.h"
#include "table/search.h"
#include "mapgui/navmapwidget.h"
#include "common/formatter.h"
#include "table/airportsearch.h"
#include "table/navsearch.h"
#include "mapgui/mapquery.h"
#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/MarbleAboutDialog.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/LegendWidget.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/RenderPlugin.h>
#include <marble/MarbleDirs.h>
#include <marble/QtMarbleConfigDialog.h>
#include <marble/MarbleDebug.h>
#include "mapgui/maplayersettings.h"

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QSettings>

#include "settings/settings.h"
#include "fs/bglreaderoptions.h"
#include "table/controller.h"
#include "gui/widgetstate.h"
#include "gui/tablezoomhandler.h"
#include "fs/bglreaderprogressinfo.h"
#include "fs/navdatabase.h"
#include "table/searchcontroller.h"
#include "gui/helphandler.h"

#include "sql/sqlutil.h"

#include "ui_mainwindow.h"

#include <route/routecontroller.h>

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

  dialog = new atools::gui::Dialog(this);
  errorHandler = new atools::gui::ErrorHandler(this);
  helpHandler = new atools::gui::HelpHandler(this, aboutMessage, GIT_REVISION);
  marbleAbout = new Marble::MarbleAboutDialog(this);

  ui->setupUi(this);
  setupUi();

  openDatabase();

  databaseLoader = new DatabaseLoader(this, &db);

  mapQuery = new MapQuery(&db);
  mapQuery->initQueries();

  routeController = new RouteController(this, mapQuery, ui->tableViewRoute);

  createNavMap();

  legendWidget = new Marble::LegendWidget(this);
  legendWidget->setMarbleModel(navMapWidget->model());
  ui->verticalLayoutMapLegendInfo->addWidget(legendWidget);

  // Have to create searches in the same order as the tabs
  searchController = new SearchController(this, mapQuery, ui->tabWidgetSearch);
  searchController->createAirportSearch(ui->tableViewAirportSearch);
  searchController->createNavSearch(ui->tableViewNavSearch);

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
}

MainWindow::~MainWindow()
{
  delete routeController;
  delete searchController;
  delete mapQuery;
  delete legendWidget;
  delete marbleAbout;
  delete ui;

  qDebug() << "MainWindow destructor";

  delete dialog;
  delete errorHandler;
  delete databaseLoader;

  atools::settings::Settings::shutdown();
  atools::gui::Translator::unload();

  closeDatabase();

  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();

}

void MainWindow::createNavMap()
{
  // Create a Marble QWidget without a parent
  navMapWidget = new NavMapWidget(this, mapQuery);

  navMapWidget->setVolatileTileCacheLimit(512 * 1024);

  // mapWidget->setShowSunShading(true);

  // mapWidget->model()->addGeoDataFile("/home/alex/ownCloud/Flight Simulator/FSX/Airports KML/NA Blue.kml");
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
  QString helpText = tr("Select Map Theme");
  mapProjectionComboBox->setToolTip(helpText);
  mapProjectionComboBox->setStatusTip(helpText);
  mapProjectionComboBox->addItem(tr("Mercator"), Marble::Mercator);
  mapProjectionComboBox->addItem(tr("Spherical"), Marble::Spherical);
  ui->mapToolBar->addWidget(mapProjectionComboBox);

  mapThemeComboBox = new QComboBox(this);
  mapThemeComboBox->setObjectName("mapThemeComboBox");
  helpText = tr("Select Map Theme");
  mapThemeComboBox->setToolTip(helpText);
  mapThemeComboBox->setStatusTip(helpText);
  mapThemeComboBox->addItem(tr("OpenStreetMap"),
                            "earth/openstreetmap/openstreetmap.dgml");
  mapThemeComboBox->addItem(tr("OpenTopoMap"),
                            "earth/opentopomap/opentopomap.dgml");
  mapThemeComboBox->addItem(tr("Simple"),
                            "earth/political/political.dgml");
  ui->mapToolBar->addWidget(mapThemeComboBox);

  ui->menuView->addAction(ui->mainToolBar->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetInformation->toggleViewAction());

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

  connect(ui->actionMapSetHome, &QAction::triggered, navMapWidget, &NavMapWidget::changeHome);

  connect(routeController, &RouteController::showRect, navMapWidget, &NavMapWidget::showRect);
  connect(routeController, &RouteController::showPos, navMapWidget, &NavMapWidget::showPos);
  connect(routeController, &RouteController::changeMark, navMapWidget, &NavMapWidget::changeMark);
  connect(routeController, &RouteController::routeChanged, navMapWidget, &NavMapWidget::routeChanged);

  connect(searchController->getAirportSearch(), &AirportSearch::showRect,
          navMapWidget, &NavMapWidget::showRect);
  connect(searchController->getAirportSearch(), &AirportSearch::showPos,
          navMapWidget, &NavMapWidget::showPos);
  connect(searchController->getAirportSearch(), &AirportSearch::changeMark,
          navMapWidget, &NavMapWidget::changeMark);

  connect(searchController->getNavSearch(), &NavSearch::showPos, navMapWidget, &NavMapWidget::showPos);
  connect(searchController->getNavSearch(), &NavSearch::changeMark, navMapWidget, &NavMapWidget::changeMark);

  // Use this event to show path dialog after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);
  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
  connect(ui->actionReloadScenery, &QAction::triggered, databaseLoader, &DatabaseLoader::exec);
  connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::options);

  connect(ui->actionRouteCenter, &QAction::triggered, this, &MainWindow::routeCenter);
  connect(ui->actionRouteNew, &QAction::triggered, this, &MainWindow::routeNew);
  connect(ui->actionRouteOpen, &QAction::triggered, this, &MainWindow::routeOpen);
  connect(ui->actionRouteSave, &QAction::triggered, this, &MainWindow::routeSave);
  connect(ui->actionRouteSaveAs, &QAction::triggered, this, &MainWindow::routeSaveAs);

  connect(ui->actionContents, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::help);
  connect(ui->actionAbout, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::about);
  connect(ui->actionAbout_Qt, &QAction::triggered, helpHandler, &atools::gui::HelpHandler::aboutQt);

  // Map widget related connections
  connect(navMapWidget, &NavMapWidget::objectSelected, searchController, &SearchController::objectSelected);
  // Connect the map widget to the position label.
  QObject::connect(navMapWidget, &NavMapWidget::mouseMoveGeoPosition, mapPosLabel, &QLabel::setText);
  QObject::connect(navMapWidget, &NavMapWidget::distanceChanged, mapDistanceLabel, &QLabel::setText);
  QObject::connect(navMapWidget, &NavMapWidget::renderStatusChanged, this, &MainWindow::renderStatusChanged);

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
  connect(ui->actionMapShowHillshading, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowSoftAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowEmptyAirports, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowVor, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowNdb, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowWp, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowIls, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowVictorAirways, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowJetAirways, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);
  connect(ui->actionMapShowRoute, &QAction::toggled, this, &MainWindow::updateMapShowFeatures);

  connect(ui->actionMapShowMark, &QAction::triggered, navMapWidget, &NavMapWidget::showMark);
  connect(ui->actionMapShowHome, &QAction::triggered, navMapWidget, &NavMapWidget::showHome);
  connect(ui->actionMapBack, &QAction::triggered, navMapWidget, &NavMapWidget::historyBack);
  connect(ui->actionMapNext, &QAction::triggered, navMapWidget, &NavMapWidget::historyNext);
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

  connect(navMapWidget, &NavMapWidget::routeSetStart,
          routeController, &RouteController::routeSetStart);
  connect(navMapWidget, &NavMapWidget::routeSetParkingStart,
          routeController, &RouteController::routeSetParking);
  connect(navMapWidget, &NavMapWidget::routeSetDest,
          routeController, &RouteController::routeSetDest);
  connect(navMapWidget, &NavMapWidget::routeAdd,
          routeController, &RouteController::routeAdd);
  connect(navMapWidget, &NavMapWidget::routeReplace,
          routeController, &RouteController::routeReplace);
  connect(navMapWidget, &NavMapWidget::routeDelete,
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

  connect(databaseLoader, &DatabaseLoader::preDatabaseLoad, this, &MainWindow::preDatabaseLoad);
  connect(databaseLoader, &DatabaseLoader::postDatabaseLoad, this, &MainWindow::postDatabaseLoad);

  connect(legendWidget, &Marble::LegendWidget::propertyValueChanged,
          navMapWidget, &NavMapWidget::setPropertyValue);
  connect(ui->actionAboutMarble, &QAction::triggered,
          marbleAbout, &Marble::MarbleAboutDialog::exec);
}

void MainWindow::clearMessageText()
{
  messageLabel->clear();
}

void MainWindow::resultTruncated(maptypes::MapObjectTypes type, int truncatedTo)
{
  qDebug() << "resultTruncated" << type << "num" << truncatedTo;
  if(truncatedTo > 0)
    messageLabel->setText(tr("Too many objects."));
  else
    messageLabel->clear();
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

void MainWindow::routeNew()
{
  routeController->newFlightplan();
  navMapWidget->update();
}

void MainWindow::routeOpen()
{
  QString routeFile = dialog->openFileDialog(tr("Open Flightplan"),
                                             tr("Flightplan Files (*.pln *.PLN);;All Files (*)"),
                                             "Route/",
                                             atools::fs::FsPaths::getFilesPath(atools::fs::fstype::FSX));

  if(!routeFile.isEmpty())
  {
    routeController->loadFlightplan(routeFile);
    navMapWidget->update();
  }
}

void MainWindow::routeSave()
{
  routeController->saveFlightplan();
}

void MainWindow::routeSaveAs()
{
  QString routeFile = dialog->saveFileDialog(tr("Save Flightplan"),
                                             tr("Flightplan Files (*.pln *.PLN);;All Files (*)"),
                                             "pln", "Route/",
                                             atools::fs::FsPaths::getFilesPath(atools::fs::fstype::FSX));

  if(!routeFile.isEmpty())
    routeController->saveFlighplanAs(routeFile);
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
  QList<RouteMapObject> result;
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
}

void MainWindow::updateHistActions(int minIndex, int curIndex, int maxIndex)
{
  qDebug() << "History changed min" << minIndex << "cur" << curIndex << "max" << maxIndex;
  ui->actionMapBack->setEnabled(curIndex > minIndex);
  ui->actionMapNext->setEnabled(curIndex < maxIndex);
}

void MainWindow::createEmptySchema()
{
  if(!atools::sql::SqlUtil(&db).hasTable("airport"))
  {
    atools::fs::BglReaderOptions opts;
    atools::fs::Navdatabase nd(&opts, &db);
    nd.createSchema();
  }
}

void MainWindow::options()
{
  // QtMarbleConfigDialog dlg(mapWidget);
  // dlg.exec();

}

void MainWindow::mainWindowShown()
{
  qDebug() << "MainWindow::mainWindowShown()";
}

void MainWindow::updateActionStates()
{
  qDebug() << "Updating action states";
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
}

void MainWindow::openDatabase()
{
  try
  {
    using atools::sql::SqlDatabase;

    // Get a file in the organization specific directory with an application
    // specific name (i.e. Linux: ~/.config/ABarthel/little_logbook.sqlite)
    databaseFile = atools::settings::Settings::getConfigFilename(".sqlite");

    qDebug() << "Opening database" << databaseFile;
    db = SqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(databaseFile);
    db.open({"PRAGMA foreign_keys = ON"});

    createEmptySchema();
  }
  catch(std::exception& e)
  {
    errorHandler->handleException(e, "While opening database");
  }
  catch(...)
  {
    errorHandler->handleUnknownException("While opening database");
  }
}

void MainWindow::closeDatabase()
{
  try
  {
    using atools::sql::SqlDatabase;

    qDebug() << "Closing database" << databaseFile;
    db.close();
  }
  catch(std::exception& e)
  {
    errorHandler->handleException(e, "While closing database");
  }
  catch(...)
  {
    errorHandler->handleUnknownException("While closing database");
  }
}

void MainWindow::readSettings()
{
  qDebug() << "readSettings";

  atools::gui::WidgetState ws("MainWindow/Widget");
  ws.restore({this, ui->statusBar, ui->tabWidgetSearch});

  searchController->restoreState();
  navMapWidget->restoreState();
  routeController->restoreState();

  ws.restore({mapProjectionComboBox, mapThemeComboBox,
              ui->actionMapShowAirports, ui->actionMapShowSoftAirports, ui->actionMapShowEmptyAirports,
              ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp, ui->actionMapShowIls,
              ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
              ui->actionMapShowRoute,
              ui->actionMapShowCities, ui->actionMapShowHillshading});

  mapDetailFactor = atools::settings::Settings::instance()->value("Map/DetailFactor",
                                                                  MAP_DEFAULT_DETAIL_FACTOR).toInt();

  databaseLoader->restoreState();

}

void MainWindow::writeSettings()
{
  qDebug() << "writeSettings";

  atools::gui::WidgetState ws("MainWindow/Widget");
  ws.save({this, ui->statusBar, ui->tabWidgetSearch});

  searchController->saveState();
  navMapWidget->saveState();
  routeController->saveState();

  ws.save({mapProjectionComboBox, mapThemeComboBox,
           ui->actionMapShowAirports, ui->actionMapShowSoftAirports, ui->actionMapShowEmptyAirports,
           ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp, ui->actionMapShowIls,
           ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
           ui->actionMapShowRoute,
           ui->actionMapShowCities, ui->actionMapShowHillshading});

  atools::settings::Settings::instance()->setValue("Map/DetailFactor", mapDetailFactor);

  databaseLoader->saveState();

  ws.syncSettings();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  // Catch all close events like Ctrl-Q or Menu/Exit or clicking on the
  // close button on the window frame
  qDebug() << "closeEvent";
  int result = dialog->showQuestionMsgBox("Actions/ShowQuit",
                                          tr("Really Quit?"),
                                          tr("Do not &show this dialog again."),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No, QMessageBox::Yes);

  if(result != QMessageBox::Yes)
    event->ignore();
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
  if(!hasDatabaseLoadStatus)
  {
    hasDatabaseLoadStatus = true;

    searchController->preDatabaseLoad();
    navMapWidget->preDatabaseLoad();
    mapQuery->deInitQueries();
  }
  else
    qWarning() << "Already in database loading status";
}

void MainWindow::postDatabaseLoad()
{
  if(hasDatabaseLoadStatus)
  {
    mapQuery->initQueries();
    searchController->postDatabaseLoad();
    navMapWidget->postDatabaseLoad();
    hasDatabaseLoadStatus = false;
  }
  else
    qWarning() << "Not in database loading status";
}
