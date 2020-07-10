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

#ifndef LITTLENAVMAP_WAYPOINTTRACKQUERY_H
#define LITTLENAVMAP_WAYPOINTTRACKQUERY_H

#include "query/querytypes.h"

namespace map {
struct MapSearchResult;
}

class WaypointQuery;
class CoordinateConverter;

/*
 * Provides map related database queries for waypoints and trackpoints (NAT, PACOTS, ...).
 * Simple delegate combining the track and waypoint queries.
 *
 * All queries check the track database first and the nav database second.
 *
 * All ids are database ids. Some ids in the track database are for generated waypoints and use an offset.
 */
class WaypointTrackQuery
{
public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  WaypointTrackQuery(WaypointQuery *waypointQueryParam, WaypointQuery *trackQueryParam);
  ~WaypointTrackQuery();

  WaypointTrackQuery(const WaypointTrackQuery& other)
  {
    this->operator=(other);
  }

  /* Does a shallow copy. Query classes are not owned by this */
  WaypointTrackQuery& operator=(const WaypointTrackQuery& other)
  {
    waypointQuery = other.waypointQuery;
    trackQuery = other.trackQuery;
    useTracks = other.useTracks;
    return *this;
  }

  /* Get one by database id */
  map::MapWaypoint getWaypointById(int id);

  /* Get a list of matching points for ident and optionally region. */
  void getWaypointByIdent(QList<map::MapWaypoint>& waypoints, const QString& ident,
                          const QString& region = QString());

  /* Get nearest waypoint by screen coordinates for types and given map layer. */
  void getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, map::MapObjectTypes types,
                               int xs, int ys,
                               int screenDistance, map::MapSearchResult& result);

  /* Get nearest waypoint - slow */
  void getWaypointNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos);

  /* Get waypoints in rectangle by point and radius for maximum distance. Results are sorted by distance closest first.
   * Does not interfere with the cache. */
  void getWaypointsRect(QVector<map::MapWaypoint>& waypoints, const atools::geo::Pos& pos, float distanceNm);
  void getWaypointRectNearest(map::MapWaypoint& waypoint, const atools::geo::Pos& pos, float distanceNm);

  /* Similar to getAirports */
  void getWaypoints(QList<map::MapWaypoint>& waypoints, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                    bool lazy);

  /* Get record for joined tables waypoint, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Tracks loaded - clear caches */
  void postTrackLoad();

  /* Set to false to ignore track database. Create a copy of this before using this method. */
  void setUseTracks(bool value)
  {
    useTracks = value;
  }

  bool isUseTracks() const
  {
    return useTracks;
  }

  /* Delete the query classes. This is not done in the destructor. */
  void deleteChildren();

  /* Queries that are shared with map query. */
  atools::sql::SqlQuery *getWaypointsByRectQuery() const;
  atools::sql::SqlQuery *getWaypointsByRectQueryTrack() const;

private:
  /* Copies objects and avoids duplicates in the to list/vector. */
  void copy(const QList<map::MapWaypoint>& from, QList<map::MapWaypoint>& to);
  void copy(const QVector<map::MapWaypoint>& from, QVector<map::MapWaypoint>& to);

  WaypointQuery *waypointQuery = nullptr, *trackQuery = nullptr;
  bool useTracks = true;
};

#endif // LITTLENAVMAP_WAYPOINTTRACKQUERY_H
