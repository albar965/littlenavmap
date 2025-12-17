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

#ifndef LNM_AIRSPACECONTROLLER_H
#define LNM_AIRSPACECONTROLLER_H

#include "common/mapflags.h"
#include "fs/fspaths.h"

#include <QObject>

class AirspaceQueries;
namespace atools {

namespace geo {
class Pos;
}
namespace fs {
namespace userdata {
class AirspaceReaderBase;
}

}
namespace sql {
class SqlDatabase;
class SqlRecord;
}
}

namespace Marble {
class GeoDataLatLonBox;
}

class AirspaceQuery;
class MapLayer;
class AirspaceToolBarHandler;
class MainWindow;

/*
 * Wraps the airspace queries for nav, sim, user and online airspaces.
 * Provides a method to import user airspaces recursively from a folder.
 */
class AirspaceController
  : public QObject
{
  Q_OBJECT

public:
  explicit AirspaceController(MainWindow *mainWindowParam);
  virtual ~AirspaceController() override;

  AirspaceController(const AirspaceController& other) = delete;
  AirspaceController& operator=(const AirspaceController& other) = delete;

  /* Read and write widget states, source and airspace selection */
  void saveState() const;
  void restoreState();

  void optionsChanged();

  /* Clears and deletes all queries */
  void preDatabaseLoad();

  /* Re-initializes all queries */
  void postDatabaseLoad();

  /* Update drop down buttons and actions */
  void updateButtonsAndActions();

  /* Opens folder selection dialog if base is not set and loads all airspaces */
  void loadAirspaces();

  void resetSettingsToDefault();

  void onlineClientAndAtcUpdated();

signals:
  /* Filter in drop down buttons have changed */
  void updateAirspaceTypes(const map::MapAirspaceFilter& filter);

  /* Source database selection has changed */
  void updateAirspaceSources(map::MapAirspaceSources sources);

  /* Re-routed to database manager signals for re-emitting */
  void preDatabaseLoadAirspaces();
  void postDatabaseLoadAirspaces(atools::fs::FsPaths::SimulatorType type);

  /* Airspaces reloaded - update online centers */
  void userAirspacesUpdated();

private:
  /* One of the source database actions was changed */
  void sourceToggled();

  /* Copy source selection to actons */
  void sourceToActions();

  void loadAirspace(atools::fs::userdata::AirspaceReaderBase& reader, const QString& file, int fileId, int& nextAirspaceId,
                    int& numReadFile);
  void collectErrors(QStringList& errors, const atools::fs::userdata::AirspaceReaderBase& reader, const QString& basePath);
  atools::geo::Pos fetchAirportCoordinates(const QString& airportIdent);

  AirspaceToolBarHandler *airspaceHandler = nullptr;
  MainWindow *mainWindow;
};

#endif // LNM_AIRSPACECONTROLLER_H
