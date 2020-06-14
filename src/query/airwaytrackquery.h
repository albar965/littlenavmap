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

#ifndef LITTLENAVMAP_AIRWAYTRACKQUERY_H
#define LITTLENAVMAP_AIRWAYTRACKQUERY_H

#include "query/querytypes.h"

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
}
}

class MapLayer;
class AirwayQuery;

/*
 * Provides map related database queries for airways and tracks (NAT, PACOTS, ...).
 * Simple delegate combining the track and airway queries.
 *
 * All queries check the track database first and the nav database second.
 *
 * All ids are database ids. Some ids in the track database are for generated waypoints and use an offset.
 */
class AirwayTrackQuery
{
public:
  /*
   * @param sqlDbNav for updated navaids
   * @param sqlDbTrack for tracks. May be null.
   */
  AirwayTrackQuery(AirwayQuery *airwayQueryParam, AirwayQuery *trackQueryParam);
  ~AirwayTrackQuery();

  AirwayTrackQuery(const AirwayTrackQuery& other)
  {
    this->operator=(other);
  }

  /* Does a shallow copy. Query classes are not owned by this */
  AirwayTrackQuery& operator=(const AirwayTrackQuery& other)
  {
    airwayQuery = other.airwayQuery;
    trackQuery = other.trackQuery;
    useTracks = other.useTracks;
    return *this;
  }

  /* Get all airways that are attached to a waypoint */
  void getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId);

  /* Get an airway segment with given name having two waypoints. Direction and waypoint order is not relevant.
   * All airways are returned if name is empty.*/
  void getAirwaysForWaypoints(QList<map::MapAirway>& airways, int waypointId1, int waypointId2,
                              const QString& airwayName);
  void getAirwaysByNameAndWaypoint(QList<map::MapAirway>& airways, const QString& airwayName, const QString& waypoint1,
                                   const QString& waypoint2 = QString());
  bool hasAirwayForNameAndWaypoint(const QString& airwayName, const QString& waypoint1,
                                   const QString& waypoint2 = QString());

  /* Get all waypoints of an airway */
  void getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                             const QString& waypointIdent = QString());

  /* Get all airway segments by name */
  void getAirwaysByName(QList<map::MapAirway>& airways, const QString& name);

  /* Get all waypoints for and airway ordered by fragment and sequence number. Fragment is ignored if -1. */
  void getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName,
                                    int airwayFragment = -1);

  /* Get all airway segments for name and fragment - not cached */
  void getAirwayFull(QList<map::MapAirway>& airways, const QString& airwayName, int fragment);
  void getAirwayFull(QList<map::MapAirway>& airways, atools::geo::Rect& bounding, const QString& airwayName,
                     int fragment);

  void getAirwayById(map::MapAirway& airway, int airwayId);
  map::MapAirway getAirwayById(int airwayId);

  /* Fill objects of the maptypes namespace and maintains a cache.
   * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
   * if they have to be kept between event loop calls. */
  void getAirways(QList<map::MapAirway>& airways, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                  bool lazy);
  void getTracks(QList<map::MapAirway>& airways, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                 bool lazy);

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

private:
  AirwayQuery *airwayQuery = nullptr, *trackQuery = nullptr;
  bool useTracks = true;
};

#endif // LITTLENAVMAP_AIRWAYTRACKQUERY_H
