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

#ifndef LNM_AIRSPACECONTROLLER_H
#define LNM_AIRSPACECONTROLLER_H

#include "common/maptypes.h"

#include <QObject>

namespace atools {
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

typedef  QHash<map::MapAirspaceSources, AirspaceQuery *> AirspaceQueryMapType;
typedef  QVector<const map::MapAirspace *> AirspaceVector;

/*
 * Wraps the airspace queries for nav, sim, user and online airspaces.
 * Provides a method to import user airspaces recursively from a folder.
 */
class AirspaceController
  : public QObject
{
  Q_OBJECT

public:
  explicit AirspaceController(MainWindow *mainWindowParam, atools::sql::SqlDatabase *dbSim,
                              atools::sql::SqlDatabase *dbNav, atools::sql::SqlDatabase *dbUser,
                              atools::sql::SqlDatabase *dbOnline);
  virtual ~AirspaceController() override;

  /* Get airspace or online center by id and source database. Airspace is not valid if not found. */
  void getAirspaceById(map::MapAirspace& airspace, map::MapAirspaceId id);
  map::MapAirspace getAirspaceById(map::MapAirspaceId id);

  bool hasAirspaceById(map::MapAirspaceId id);

  /* Get record with all rows from atc table (online center) */
  atools::sql::SqlRecord getOnlineAirspaceRecordById(int airspaceId);

  /* Get boundary merged with scenery and file metadata records. Not for online centers. */
  atools::sql::SqlRecord getAirspaceInfoRecordById(map::MapAirspaceId id);

  /* Get airspaces from all enabled sources for map display */
  void getAirspaces(AirspaceVector& airspaces, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                    map::MapAirspaceFilter filter, float flightPlanAltitude, bool lazy, map::MapAirspaceSources src);

  /* Get Geometry for any airspace and source database */
  const atools::geo::LineString *getAirspaceGeometry(map::MapAirspaceId id);

  /* Read and write widget states, source and airspace selection */
  void restoreState();
  void saveState();

  void optionsChanged();

  /* Clears and deletes all queries */
  void preDatabaseLoad();

  /* Re-initializes all queries */
  void postDatabaseLoad();

  /* Clears cache for online database changes */
  void onlineClientAndAtcUpdated();

  /* Resets all queries */
  void resetAirspaceOnlineScreenGeometry();

  /* Update drop down buttons and actions */
  void updateButtonsAndActions();

  /* Get currently enabled souces for map display */
  map::MapAirspaceSources getAirspaceSources() const
  {
    return sources;
  }

  bool hasAnyAirspaces() const;

  /* Get sources as string */
  QStringList getAirspaceSourcesStr() const;

  /* Opens folder selection dialog if base is not set and loads all airspaces */
  void loadAirspaces();

  /* Tries to fetch online airspace geometry by name and facility. */
  atools::geo::LineString *getOnlineAirspaceGeoByName(const QString& callsign, const QString& facilityType);

  /* Tries to fetch online airspace geometry by file name. */
  atools::geo::LineString *getOnlineAirspaceGeoByFile(const QString& callsign);

  void resetSettingsToDefault();

signals:
  /* Filter in drop down buttons have changed */
  void updateAirspaceTypes(map::MapAirspaceFilter types);

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

  void getAirspacesInternal(AirspaceVector& airspaceVector, const Marble::GeoDataLatLonBox& rect,
                            const MapLayer *mapLayer, map::MapAirspaceFilter filter, float flightPlanAltitude,
                            bool lazy, map::MapAirspaceSources src);
  void preLoadAirpaces();
  void postLoadAirpaces();

  AirspaceQueryMapType queries;
  map::MapAirspaceSources sources = map::AIRSPACE_SRC_NONE;
  AirspaceToolBarHandler *airspaceHandler = nullptr;
  MainWindow *mainWindow;
  bool loadingUserAirspaces = false;
};

#endif // LNM_AIRSPACECONTROLLER_H
