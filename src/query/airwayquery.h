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

#ifndef LITTLENAVMAP_AIRWAYQUERY_H
#define LITTLENAVMAP_AIRWAYQUERY_H

#include "query/querytypes.h"

#include <QCache>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class CoordinateConverter;
class MapTypesFactory;
class MapLayer;

/*
 * Provides map related database queries for airways.
 * All ids are database ids.
 */
class AirwayQuery
{
public:
  /*
   * @param sqlDbNav for updated navaids
   */
  AirwayQuery(atools::sql::SqlDatabase *sqlDbNav);
  ~AirwayQuery();

  /* Get all airways that are attached to a waypoint */
  void getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId);

  /* Get all waypoints of an airway */
  void getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                             const QString& waypointIdent = QString());
  void getAirwayByNameAndWaypoint(map::MapAirway& airway, const QString& airwayName, const QString& waypoint1,
                                  const QString& waypoint2);

  /* Get all airway segments by name */
  void getAirwaysByName(QList<map::MapAirway>& airways, const QString& name);

  /* Get all waypoints or an airway ordered by fragment an sequence number */
  void getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName);

  /* Get all airway segments for name and fragment - not cached */
  void getAirwayFull(QList<map::MapAirway>& airways, atools::geo::Rect& bounding, const QString& airwayName,
                     int fragment);

  void getAirwayById(map::MapAirway& airway, int airwayId);
  map::MapAirway getAirwayById(int airwayId);

  /* Fill objects of the maptypes namespace and maintains a cache.
   * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
   * if they have to be kept between event loop calls. */
  const QList<map::MapAirway> *getAirways(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

private:
  map::MapWaypoint waypointById(int id);

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbNav;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapAirway> airwayCache;

  /* ID/object caches */
  QCache<query::NearestCacheKeyNavaid, map::MapSearchResultIndex> nearestNavaidCache;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *airwayByRectQuery = nullptr;

  atools::sql::SqlQuery *airwayByWaypointIdQuery = nullptr, *airwayByNameAndWaypointQuery = nullptr,
                        *airwayByIdQuery = nullptr, *airwayWaypointByIdentQuery = nullptr, *waypointByIdQuery = nullptr,
                        *airwayWaypointsQuery = nullptr, *airwayByNameQuery = nullptr, *airwayFullQuery = nullptr;
};

#endif // LITTLENAVMAP_AIRWAYQUERY_H
