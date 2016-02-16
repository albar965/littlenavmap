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

using namespace Marble;

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::MainWindow)
{
  qDebug() << "MainWindow constructor";
  dialog = new atools::gui::Dialog(this);
  errorHandler = new atools::gui::ErrorHandler(this);

  ui->setupUi(this);

  openDatabase();

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
  // mapWidget->setShowTerrain(true);
  // mapWidget->setShowRelief(true);
  // mapWidget->setShowSunShading(true);

  // mapWidget->model()->addGeoDataFile("/home/alex/ownCloud/Flight Simulator/FSX/Airports KML/NA Blue.kml");
  // mapWidget->model()->addGeoDataFile( "/home/alex/Downloads/map.osm" );

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

  connect(mapWidget, &NavMapWidget::customContextMenuRequested, this, &MainWindow::tableContextMenu);

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

  delete dialog;
  delete errorHandler;

  atools::settings::Settings::shutdown();
  atools::gui::Translator::unload();

  closeDatabase();

  qDebug() << "MainWindow destructor about to shut down logging";
  atools::logging::LoggingHandler::shutdown();

}

void MainWindow::tableContextMenu(const QPoint& pos)
{
  qInfo() << "tableContextMenu";
  MarbleWidgetPopupMenu *menu = mapWidget->popupMenu();

  QMenu m;
  m.addAction("Menu");

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
    db.open();
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
