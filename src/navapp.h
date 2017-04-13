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

#ifndef NAVAPPLICATION_H
#define NAVAPPLICATION_H

#include "gui/application.h"

#include "common/mapflags.h"
#include "fs/fspaths.h"

class MapQuery;
class InfoQuery;
class ProcedureQuery;
class Route;
class MainWindow;
class ConnectClient;
class DatabaseManager;
class QMainWindow;
class RouteController;
class MapWidget;
class WeatherReporter;
class ElevationProvider;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

namespace Ui {
class MainWindow;
}

/* Keeps most important handler, window and query classes for static access.
 * Initialized and deinitialized in main window.
 * Not all getters refer to aggregated values but are rather delegates that help to minimized dependencies. */
class NavApp :
  public atools::gui::Application
{
public:
  NavApp(int& argc, char **argv, int flags = ApplicationFlags);
  virtual ~NavApp();

  /* Creates all aggregated objects */
  static void init(MainWindow *mainWindowParam);

  /* Needs map widget first */
  static void initElevationProvider();

  /* Deletes all aggregated objects */
  static void deInit();

  static void optionsChanged();
  static void preDatabaseLoad();
  static void postDatabaseLoad();

  static Ui::MainWindow *getMainUi();

  static bool isConnected();

  static map::MapObjectTypes getShownMapFeatures();
  static map::MapAirspaceTypes getShownMapAirspaces();

  static MapQuery *getMapQuery();
  static InfoQuery *getInfoQuery();
  static ProcedureQuery *getProcedureQuery();
  static const Route& getRoute();
  static float getSpeedKts();

  static atools::fs::FsPaths::SimulatorType getCurrentSimulator();

  /* Get full path to language dependent "Flight Simulator X Files" or "Flight Simulator X-Dateien",
   * etc. Returns the documents path if FS files cannot be found. */
  static QString getCurrentSimulatorFilesPath();

  /* Get the short name (FSX, FSXSE, P3DV3, P3DV2) of the currently selected simulator. */
  static QString getCurrentSimulatorShortName();
  static bool hasCurrentSimulatorSidStarSupport();

  static atools::sql::SqlDatabase *getDatabase();

  static ElevationProvider *getElevationProvider();

  static WeatherReporter *getWeatherReporter();

  static void updateWindowTitle();
  static void setStatusMessage(const QString& message);

  /* Get main window in different variations to avoid including it */
  static QWidget *getQMainWidget();
  static QMainWindow *getQMainWindow();
  static MainWindow *getMainWindow();

  static MapWidget *getMapWidget();
  static RouteController *getRouteController();

  static DatabaseManager *getDatabaseManager();

  static ConnectClient *getConnectClient();

private:
  /* Database query helpers and caches */
  static MapQuery *mapQuery;
  static InfoQuery *infoQuery;
  static ProcedureQuery *procedureQuery;
  static ElevationProvider *elevationProvider;
  /* Most important handlers */
  static ConnectClient *connectClient;
  static DatabaseManager *databaseManager;

  /* Main window is not aggregated */
  static MainWindow *mainWindow;
};

#endif // NAVAPPLICATION_H
