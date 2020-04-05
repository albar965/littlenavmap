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

#ifndef LITTLENAVMAP_WAYPOINTQUERY_H
#define LITTLENAVMAP_WAYPOINTQUERY_H

#include "query/querytypes.h"

#include <QCache>

class MapTypesFactory;
class CoordinateConverter;

/*
 * Provides map related database queries.
 *
 * All ids are database ids.
 */
class WaypointQuery
{
public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  WaypointQuery(atools::sql::SqlDatabase *sqlDbNav, bool trackDatabaseParam);
  ~WaypointQuery();

  /* Get one by database id */
  map::MapWaypoint getWaypointById(int id);

  /* Get a list of matching points for ident and optionally region. */
  void getWaypointByByIdent(QList<map::MapWaypoint>& waypoints, const QString& ident,
                            const QString& region = QString());

  /* Get nearest waypoint by screen coordinates for types and given map layer. */
  void getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, map::MapObjectTypes types,
                               int xs, int ys, int screenDistance, map::MapSearchResult& result);

  /* Get nearest waypoint - slow */
  void getWaypointNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos);

  /* Get waypoints in rectangle by point and radius for maximum distance. Results are sorted by distance closest first.
   * Does not interfere with the cache. */
  void getWaypointsRect(QVector<map::MapWaypoint>& waypoints, const atools::geo::Pos& pos, float distanceNm);
  void getWaypointRectNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos, float distanceNm);

  /* Similar to getAirports */
  const QList<map::MapWaypoint> *getWaypoints(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                              bool lazy);

  /* Get record for joined tables waypoint, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  atools::sql::SqlQuery *getWaypointsByRectQuery() const
  {
    return waypointsByRectQuery;
  }

  void clearCache();

private:
  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbNav;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapWaypoint> waypointCache;
  QCache<int, atools::sql::SqlRecord> waypointInfoCache;

  static int queryMaxRows;

  bool trackDatabase;

  /* Database queries */
  atools::sql::SqlQuery *waypointByIdQuery = nullptr, *waypointNearestQuery = nullptr, *waypointRectQuery = nullptr,
                        *waypointByIdentQuery = nullptr, *waypointsByRectQuery = nullptr, *waypointInfoQuery = nullptr;
};

#endif // LITTLENAVMAP_WAYPOINTQUERY_H
