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

namespace map {
struct MapSearchResult;
struct MapSearchResultIndex;
}

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
 * Only to be used through the delegate AirwayTrackQuery.
 *
 * Queries can use track database or normal nav database.
 *
 * All ids are database ids.
 */
class AirwayQuery
{
public:
  /*
   * @param sqlDbNav for updated navaids
   */
  AirwayQuery(atools::sql::SqlDatabase *sqlDbNav, bool trackDatabaseParam);
  ~AirwayQuery();

  /* Get all airways that are attached to a waypoint */
  void getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId);

  /* Get an airway with given name having two waypoints. Direction and waypoint order is not relevant.
   * All airways are returned if name is empty.*/
  void getAirwaysForWaypoints(QList<map::MapAirway>& airways, int waypointId1, int waypointId2,
                              const QString& airwayName);
  void getAirwaysByNameAndWaypoint(QList<map::MapAirway>& airways, const QString& airwayName, const QString& waypoint1,
                                   const QString& waypoint2 = QString());

  /* Get all waypoints of an airway */
  void getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                             const QString& waypointIdent);

  /* Get all airway segments by name */
  void getAirwaysByName(QList<map::MapAirway>& airways, const QString& name);

  /* Get all waypoints or and airways ordered by fragment and sequence number */
  void getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName,
                                    int airwayFragment);

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

  void clearCache();

private:
  map::MapWaypoint waypointById(int id);

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbNav;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapAirway> airwayCache;

  /* ID/object caches */
  QCache<query::NearestCacheKeyNavaid, map::MapSearchResultIndex> nearestNavaidCache;

  /* true if this uses the track database (PACOTS, NAT, etc.) */
  bool trackDatabase;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *airwayByRectQuery = nullptr;

  atools::sql::SqlQuery *airwayByWaypointIdQuery = nullptr, *airwayByNameAndWaypointQuery = nullptr,
                        *airwayByIdQuery = nullptr, *airwayWaypointByIdentQuery = nullptr, *waypointByIdQuery = nullptr,
                        *airwayWaypointsQuery = nullptr, *airwayByNameQuery = nullptr, *airwayFullQuery = nullptr;

  /* Table and id column names depending on database type */
  QString airwayIdCol, airwayNameCol, airwayTable, waypointIdCol, waypointTable, prefix;
};

#endif // LITTLENAVMAP_AIRWAYQUERY_H
