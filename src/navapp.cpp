/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/infoquery.h"
#include "common/procedurequery.h"
#include "connect/connectclient.h"
#include "mapgui/mapquery.h"
#include "db/databasemanager.h"
#include "fs/db/databasemeta.h"
#include "mapgui/mapwidget.h"
#include "gui/mainwindow.h"
#include "route/routecontroller.h"
#include "common/elevationprovider.h"

#include "ui_mainwindow.h"

#include <marble/MarbleModel.h>

#include <QIcon>

MapQuery *NavApp::mapQuery = nullptr;
InfoQuery *NavApp::infoQuery = nullptr;
ProcedureQuery *NavApp::procedureQuery = nullptr;

ConnectClient *NavApp::connectClient = nullptr;
DatabaseManager *NavApp::databaseManager = nullptr;
MainWindow *NavApp::mainWindow = nullptr;
ElevationProvider *NavApp::elevationProvider = nullptr;

NavApp::NavApp(int& argc, char **argv, int flags)
  : atools::gui::Application(argc, argv, flags)
{
  setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  setApplicationName("Little Navmap");
  setOrganizationName("ABarthel");
  setOrganizationDomain("abarthel.org");
  setApplicationVersion("1.3.5.develop");
}

NavApp::~NavApp()
{
}

void NavApp::init(MainWindow *mainWindowParam)
{
  qDebug() << Q_FUNC_INFO;

  NavApp::mainWindow = mainWindowParam;
  databaseManager = new DatabaseManager(mainWindow);
  databaseManager->openDatabase();

  mapQuery = new MapQuery(mainWindow, databaseManager->getDatabase());
  mapQuery->initQueries();

  infoQuery = new InfoQuery(databaseManager->getDatabase());
  infoQuery->initQueries();

  procedureQuery = new ProcedureQuery(databaseManager->getDatabase(), mapQuery);
  procedureQuery->setCurrentSimulator(databaseManager->getCurrentSimulator());
  procedureQuery->initQueries();

  qDebug() << "MainWindow Creating ConnectClient";
  connectClient = new ConnectClient(mainWindow);
}

void NavApp::initElevationProvider()
{
  elevationProvider = new ElevationProvider(mainWindow, mainWindow->getElevationModel());
}

void NavApp::deInit()
{
  qDebug() << Q_FUNC_INFO;

  qDebug() << Q_FUNC_INFO << "delete connectClient";
  delete connectClient;

  qDebug() << Q_FUNC_INFO << "delete elevationProvider";
  delete elevationProvider;

  qDebug() << Q_FUNC_INFO << "delete mapQuery";
  delete NavApp::mapQuery;

  qDebug() << Q_FUNC_INFO << "delete infoQuery";
  delete NavApp::infoQuery;

  qDebug() << Q_FUNC_INFO << "delete approachQuery";
  delete NavApp::procedureQuery;

  qDebug() << Q_FUNC_INFO << "delete databaseManager";
  delete databaseManager;
}

void NavApp::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;
}

void NavApp::preDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  infoQuery->deInitQueries();
  mapQuery->deInitQueries();
  procedureQuery->deInitQueries();
}

void NavApp::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;

  procedureQuery->setCurrentSimulator(getCurrentSimulator());
  mapQuery->initQueries();
  infoQuery->initQueries();
  procedureQuery->initQueries();
}

Ui::MainWindow *NavApp::getMainUi()
{
  return mainWindow->getUi();
}

bool NavApp::isConnected()
{
  return NavApp::getConnectClient()->isConnected();
}

MapQuery *NavApp::getMapQuery()
{
  return mapQuery;
}

InfoQuery *NavApp::getInfoQuery()
{
  return infoQuery;
}

ProcedureQuery *NavApp::getProcedureQuery()
{
  return procedureQuery;
}

const Route& NavApp::getRoute()
{
  return mainWindow->getRouteController()->getRoute();
}

float NavApp::getSpeedKts()
{
  return mainWindow->getRouteController()->getSpeedKts();
}

atools::fs::FsPaths::SimulatorType NavApp::getCurrentSimulator()
{
  return getDatabaseManager()->getCurrentSimulator();
}

QString NavApp::getCurrentSimulatorFilesPath()
{
  return atools::fs::FsPaths::getFilesPath(getCurrentSimulator());
}

QString NavApp::getCurrentSimulatorShortName()
{
  return atools::fs::FsPaths::typeToShortName(getCurrentSimulator());
}

bool NavApp::hasSidStarInDatabase()
{
  return atools::fs::db::DatabaseMeta(getDatabase()).hasSidStar();
}

bool NavApp::hasDataInDatabase()
{
  return atools::fs::db::DatabaseMeta(getDatabase()).hasData();
}

atools::sql::SqlDatabase *NavApp::getDatabase()
{
  return getDatabaseManager()->getDatabase();
}

ElevationProvider *NavApp::getElevationProvider()
{
  return elevationProvider;
}

WeatherReporter *NavApp::getWeatherReporter()
{
  return mainWindow->getWeatherReporter();
}

void NavApp::updateWindowTitle()
{
  mainWindow->updateWindowTitle();
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

RouteController *NavApp::getRouteController()
{
  return mainWindow->getRouteController();
}

DatabaseManager *NavApp::getDatabaseManager()
{
  return databaseManager;
}

ConnectClient *NavApp::getConnectClient()
{
  return connectClient;
}

map::MapObjectTypes NavApp::getShownMapFeatures()
{
  return mainWindow->getMapWidget()->getShownMapFeatures();
}

map::MapAirspaceTypes NavApp::getShownMapAirspaces()
{
  return mainWindow->getMapWidget()->getShownAirspaces();
}
