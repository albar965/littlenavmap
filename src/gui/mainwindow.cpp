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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "sql/sqldatabase.h"
#include "settings/settings.h"
#include "logging/loggingdefs.h"
#include "logging/logginghandler.h"
#include "gui/translator.h"
#include "fs/fspaths.h"

#include "map/navmapwidget.h"
#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/RenderPlugin.h>
#include <QCloseEvent>
#include <QProgressDialog>
#include <QSettings>
#include <settings/settings.h>
#include <fs/bglreaderoptions.h>
#include <table/controller.h>
#include "fs/bglreaderprogressinfo.h"
#include <fs/navdatabase.h>

using namespace Marble;
using atools::settings::Settings;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::MainWindow)
{
  qDebug() << "MainWindow constructor";
  dialog = new atools::gui::Dialog(this);
  errorHandler = new atools::gui::ErrorHandler(this);

  ui->setupUi(this);
  setupUi();

  openDatabase();

  airportController = new Controller(this, &db, ui->tableViewAirportSearch);

  readSettings();

  // Create a Marble QWidget without a parent
  mapWidget = new NavMapWidget(this);

  // Load the OpenStreetMap map
  mapWidget->setMapThemeId("earth/openstreetmap/openstreetmap.dgml");
  // mapWidget->setMapThemeId("earth/bluemarble/bluemarble.dgml");
  // mapWidget->setShowBorders( true );
  // mapWidget->setShowClouds( true );
  // mapWidget->setProjection( Marble::Mercator );
  mapWidget->setShowCrosshairs(false);
  mapWidget->setShowBackground(false);
  mapWidget->setShowAtmosphere(false);
  mapWidget->setShowGrid(true);
  mapWidget->setShowTerrain(true);
  mapWidget->setShowRelief(true);
  // mapWidget->setShowSunShading(true);

  // mapWidget->model()->addGeoDataFile("/home/alex/ownCloud/Flight Simulator/FSX/Airports KML/NA Blue.kml");
  // mapWidget->model()->addGeoDataFile( "/home/alex/Downloads/map.osm" );
  connectAllSlots();
  updateActionStates();

  GeoDataIconStyle *style = new GeoDataIconStyle;
  // style->setIconPath(":/littlenavmap/resources/icons/checkmark.svg");
  GeoDataStyle *style2 = new GeoDataStyle;
  style2->setIconStyle(*style);

  GeoDataPlacemark *place = new GeoDataPlacemark("Bad Camberg");
  place->setCoordinate(8.26589, 50.29824, 0.0, GeoDataCoordinates::Degree);
  place->setDescription("Test place");
  place->setPopulation(15000);
  place->setCountryCode("Germany");
  // place->setStyle(style2);

  GeoDataDocument *document = new GeoDataDocument;
  document->append(place);

  // Add the document to MarbleWidget's tree model
  mapWidget->model()->treeModel()->addDocument(document);

  MarbleWidgetInputHandler *inputHandler = mapWidget->inputHandler();
  inputHandler->setMouseButtonPopupEnabled(Qt::RightButton, false);

  ui->verticalLayout_10->replaceWidget(ui->mapWidgetDummy, mapWidget);

  mapWidget->setContextMenuPolicy(Qt::CustomContextMenu);

  QSet<QString> pluginEnable;
  pluginEnable << "Compass" << "Coordinate Grid" << "License" << "Scale Bar" << "Navigation" <<
  "Overview Map" << "Position Marker";

  // pluginDisable << "Annotation" << "Amateur Radio Aprs Plugin" << "Atmosphere" << "Compass" <<
  // "Crosshairs" << "Earthquakes" << "Eclipses" << "Elevation Profile" << "Elevation Profile Marker" <<
  // "Places" << "GpsInfo" << "Coordinate Grid" << "License" << "Scale Bar" << "Measure Tool" << "Navigation" <<
  // "OpenCaching.Com" << "OpenDesktop Items" << "Overview Map" << "Photos" << "Position Marker" <<
  // "Postal Codes" << "Download Progress Indicator" << "Routing" << "Satellites" << "Speedometer" << "Stars" <<
  // "Sun" << "Weather" << "Wikipedia Articles";

  QList<RenderPlugin *> localRenderPlugins = mapWidget->renderPlugins();
  for(RenderPlugin *p : localRenderPlugins)
  {
    qInfo() << p->name();
    if(!pluginEnable.contains(p->name()))
      p->setEnabled(false);
  }

  // MarbleWidgetPopupMenu *menu = mapWidget->popupMenu();
  // QAction tst(QString("Menu"), mapWidget);
  // menu->addAction(Qt::RightButton, &tst);

  // mapWidget->setFocusPoint(GeoDataCoordinates(8.26589, 50.29824, 0.0, GeoDataCoordinates::Degree));
  mapWidget->centerOn(8.26589, 50.29824, false);
}

MainWindow::~MainWindow()
{
  delete ui;

  qDebug() << "MainWindow destructor";

  delete airportController;
  delete dialog;
  delete errorHandler;
  delete progressDialog;

  atools::settings::Settings::shutdown();
  atools::gui::Translator::unload();

  closeDatabase();

  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();

}

void MainWindow::setupUi()
{
ui->checkBoxAirportAddonSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportApprSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportApprSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportClosedSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportIlsSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportLightSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportMilSearch->setCheckState(Qt::Unchecked);
ui->checkBoxAirportTowerSearch->setCheckState(Qt::Unchecked);

  ui->menuView->addAction(ui->mainToolBar->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetSearch->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetRoute->toggleViewAction());
  ui->menuView->addAction(ui->dockWidgetAirportInfo->toggleViewAction());

  ui->toolButtonAirportSearch->addActions({ui->actionAirportSearchShowExtOptions,
                                           ui->actionAirportSearchShowFuelParkOptions,
                                           ui->actionAirportSearchShowRunwayOptions,
                                           ui->actionAirportSearchShowAltOptions,
                                           ui->actionAirportSearchShowDistOptions,
                                           ui->actionAirportSearchShowSceneryOptions});
  ui->toolButtonAirportSearch->setArrowType(Qt::NoArrow);

  showHideLayoutElements(ui->gridLayoutAirportExtSearch, false, {ui->lineAirportExtSearch});
  showHideLayoutElements(ui->horizontalLayoutAirportFuelParkSearch, false, {ui->lineAirportFuelParkSearch});
  showHideLayoutElements(ui->horizontalLayoutAirportRunwaySearch, false, {ui->lineAirportRunwaySearch});
  showHideLayoutElements(ui->horizontalLayoutAirportAltitudeSearch, false, {ui->lineAirportAltSearch});
  showHideLayoutElements(ui->horizontalLayoutAirportDistanceSearch, false, {ui->lineAirportDistSearch});
  showHideLayoutElements(ui->horizontalLayoutAirportScenerySearch, false, {ui->lineAirportScenerySearch});

}

void MainWindow::loadScenery()
{
  using atools::fs::BglReaderOptions;
  QString config = Settings::getOverloadedPath(":/littlenavmap/resources/config/navdatareader.cfg");
  QSettings settings(config, QSettings::IniFormat);

  BglReaderOptions opts;
  opts.loadFromSettings(settings);

  progressDialog = new QProgressDialog(this);
  QLabel *label = new QLabel(progressDialog);
  label->setAlignment(Qt::AlignLeft);

  progressDialog->setWindowModality(Qt::ApplicationModal);
  progressDialog->setLabel(label);
  progressDialog->show();

  atools::fs::fstype::SimulatorType type = atools::fs::fstype::FSX;
  QString sceneryFile = atools::fs::FsPaths::getSceneryLibraryPath(type);
  QString basepath = atools::fs::FsPaths::getBasePath(type);

  opts.setSceneryFile(sceneryFile);
  opts.setBasepath(basepath);

  opts.setProgressCallback(std::bind(&MainWindow::progressCallback, this, std::placeholders::_1));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  atools::fs::Navdatabase nd(&opts, &db);
  nd.create();

  delete progressDialog;
  progressDialog = nullptr;
}

bool MainWindow::progressCallback(const atools::fs::BglReaderProgressInfo& progress)
{
  if(progress.isFirstCall())
  {
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(progress.getTotal());
  }
  progressDialog->setValue(progress.getCurrent());

  if(progress.isNewOther())
    progressDialog->setLabelText("<br/><b>" + progress.getOtherAction() + "</b>");

  if(progress.isNewSceneryArea() || progress.isNewFile())
    progressDialog->setLabelText("<br/><b>Scenery: " + progress.getSceneryTitle() + "</b> " +
                                 "(" + progress.getSceneryPath() + "). " +
                                 "File: " + progress.getBglFilepath() + ".");

  return progressDialog->wasCanceled();
}

void MainWindow::connectAllSlots()
{
  qDebug() << "Connecting slots";
  connect(mapWidget, &NavMapWidget::customContextMenuRequested, this, &MainWindow::tableContextMenu);

  // Use this event to show path dialog after main windows is shown
  connect(this, &MainWindow::windowShown, this, &MainWindow::mainWindowShown, Qt::QueuedConnection);

  connect(ui->actionShowStatusbar, &QAction::toggled, ui->statusBar, &QStatusBar::setVisible);

  connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);

  connect(ui->actionReloadScenery, &QAction::triggered, this, &MainWindow::loadScenery);

  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->gridLayoutAirportExtSearch, state, {ui->lineAirportExtSearch}); });

  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->horizontalLayoutAirportFuelParkSearch, state, {ui->lineAirportFuelParkSearch}); });

  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->horizontalLayoutAirportRunwaySearch, state, {ui->lineAirportRunwaySearch}); });

  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->horizontalLayoutAirportAltitudeSearch, state, {ui->lineAirportAltSearch}); });

  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->horizontalLayoutAirportDistanceSearch, state, {ui->lineAirportDistSearch}); });

  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled,
          [=](bool state) {showHideLayoutElements(ui->horizontalLayoutAirportScenerySearch, state, {ui->lineAirportScenerySearch}); });
  /* *INDENT-ON* */

}

void MainWindow::mainWindowShown()
{
  qDebug() << "MainWindow::mainWindowShown()";
}

void MainWindow::showHideLayoutElements(QLayout *layout, bool visible, const QList<QWidget *>& otherWidgets)
{
  for(QWidget *w : otherWidgets)
    w->setVisible(visible);

  for(int i = 0; i < layout->count(); i++)
    layout->itemAt(i)->widget()->setVisible(visible);
}

void MainWindow::updateActionStates()
{
  qDebug() << "Updating action states";
  ui->actionShowStatusbar->setChecked(!ui->statusBar->isHidden());
}

void MainWindow::tableContextMenu(const QPoint& pos)
{
  qInfo() << "tableContextMenu";
  MarbleWidgetPopupMenu *menu = mapWidget->popupMenu();

  QMenu m;
  m.addAction("Menu");
  m.addAction("Copy");
  m.addAction("Paste");

  m.exec(QCursor::pos());

  // menu->slotInfoDialog();
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

  Settings& s = Settings::instance();

  if(s->contains("MainWindow/Size"))
    resize(s->value("MainWindow/Size", sizeHint()).toSize());

  if(s->contains("MainWindow/State"))
    restoreState(s->value("MainWindow/State").toByteArray());

  ui->statusBar->setHidden(!s->value("MainWindow/ShowStatusbar", true).toBool());

  // showSearchBar(s->value(ll::constants::SETTINGS_SHOW_SEARCHOOL, true).toBool());

  // ui->actionOpenAfterExport->setChecked(s->value(ll::constants::SETTINGS_EXPORT_OPEN, true).toBool());

  // ui->actionFilterLogbookEntries->setChecked(s->value(ll::constants::SETTINGS_FILTER_ENTRIES, false).toBool());
}

void MainWindow::writeSettings()
{
  qDebug() << "writeSettings";

  Settings& s = Settings::instance();
  s->setValue("MainWindow/Size", size());
  s->setValue("MainWindow/State", saveState());
  s->setValue("MainWindow/ShowStatusbar", !ui->statusBar->isHidden());

  s.syncSettings();
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
