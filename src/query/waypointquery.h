/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_WAYPOINTQUERY_H
#define LITTLENAVMAP_WAYPOINTQUERY_H

#include "query/querytypes.h"

#include <QCache>

namespace map {
struct MapResult;
}

class MapTypesFactory;
class CoordinateConverter;

/*
 * Provides map related database queries.
 * This is one is hidden by the facade WaypointTrackQuery and used in two instances for track and normal airway queries.
 *
 * All ids are database ids.
 */
class WaypointQuery
{
public:
  ~WaypointQuery();

  WaypointQuery(const WaypointQuery& other) = delete;
  WaypointQuery& operator=(const WaypointQuery& other) = delete;

  /* Get one by database id */
  map::MapWaypoint getWaypointById(int id);

  /* By VOR or NDB id */
  map::MapWaypoint getWaypointByNavId(int navId, map::MapType type);

  /* Get a list of matching points for ident and optionally region. */
  void getWaypointByByIdent(QList<map::MapWaypoint>& waypoints, const QString& ident, const QString& region, const QString& airport);

  /* Get nearest waypoint by screen coordinates for types and given map layer. */
  void getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, map::MapTypes types,
                               int xs, int ys, int screenDistance, map::MapResult& result);

  /* Get nearest waypoint - slow */
  void getWaypointNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos);

  /* Get waypoints in rectangle by point and radius for maximum distance. Results are sorted by distance closest first.
   * Does not interfere with the cache. */
  void getWaypointsRect(QVector<map::MapWaypoint>& waypoints, const atools::geo::Pos& pos, float distanceNm);
  void getWaypointRectNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos, float distanceNm);

  /* Similar to getAirports for map display. Caches values. */
  const QList<map::MapWaypoint> *getWaypoints(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  /* As getWaypoints() but only for waypoints having airways */
  const QList<map::MapWaypoint> *getWaypointsAirway(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                                    bool& overflow);

  /* Get record for joined tables waypoint, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);

  atools::sql::SqlQuery *getWaypointsByRectQuery() const
  {
    return waypointsByRectQuery;
  }

private:
  friend class Queries;
  friend class WaypointTrackQuery;

  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  explicit WaypointQuery(atools::sql::SqlDatabase *sqlDbNav, bool trackDatabaseParam);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  void clearCache();

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbNav;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapWaypoint> waypointCache, waypointAirwayCache;
  QCache<int, atools::sql::SqlRecord> waypointInfoCache;

  static int queryMaxRowsWaypoints;

  bool trackDatabase;

  /* Database queries */
  atools::sql::SqlQuery *waypointByIdQuery = nullptr, *waypointByNavIdQuery = nullptr, *waypointNearestQuery = nullptr,
                        *waypointRectQuery = nullptr, *waypointByIdentQuery = nullptr, *waypointByIdentAndAirportQuery = nullptr,
                        *waypointsByRectQuery = nullptr, *waypointsAirwayByRectQuery = nullptr, *waypointInfoQuery = nullptr;
};

#endif // LITTLENAVMAP_WAYPOINTQUERY_H
