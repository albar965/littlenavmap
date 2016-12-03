/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
#define LITTLENAVMAP_ROUTEMAPOBJECTLIST_H

#include "routemapobject.h"

#include "fs/pln/flightplan.h"

class CoordinateConverter;

/*
 * Aggregates the flight plan and is a list of all route map objects.
 */
class RouteMapObjectList :
  public QList<RouteMapObject>
{
public:
  RouteMapObjectList();
  RouteMapObjectList(const RouteMapObjectList& other);
  virtual ~RouteMapObjectList();

  RouteMapObjectList& operator=(const RouteMapObjectList& other);

  /* Get nearest leg or waypoint index, whatever is closer to the position.
   * @return positive leg or negative point index */
  int getNearestLegOrPointIndex(const atools::geo::Pos& pos) const;

  /* Get nearest leg index along the position and cross track distance in nautical miles to the leg. */
  int getNearestLegIndex(const atools::geo::Pos& pos, float& crossTrackDistanceNm) const;

  /* Get nearest waypoint index and distance in nautical miles to this point.  */
  int getNearestPointIndex(const atools::geo::Pos& pos, float& pointDistanceNm) const;

  /*
   * Get multiple flight plan distances for the given position. If value pointers are null they will be ignored.
   * All distances in nautical miles.
   * @param pos
   * @param distFromStartNm Distance from departure
   * @param distToDestNm Distance to destination
   * @param nextLegDistance Distance to next leg
   * @param crossTrackDistance Cross track distance to current leg. Positive and
   * negative values indicate left or right of track.
   * @param nextLegIndex Index of next leg
   * @return false if no current/next leg was found
   */
  bool getRouteDistances(const atools::geo::Pos& pos, float *distFromStart, float *distToDest,
                         float *nextLegDistance = nullptr, float *crossTrackDistance = nullptr,
                         int *nextLegIndex = nullptr) const;

  /* Get a position along the route. Pos is invalid if not along */
  atools::geo::Pos getPositionAtDistance(float distFromStart) const;
  atools::geo::Pos getTopOfDescent() const;

  /* Distance from TOD to destination in nm */
  float getTopOfDescentFromStart() const;

  /* Total route distance in nautical miles */
  float getTotalDistance() const
  {
    return totalDistance;
  }

  void setTotalDistance(float value)
  {
    totalDistance = value;
  }

  const atools::fs::pln::Flightplan& getFlightplan() const
  {
    return flightplan;
  }

  atools::fs::pln::Flightplan& getFlightplan()
  {
    return flightplan;
  }

  void setFlightplan(const atools::fs::pln::Flightplan& value)
  {
    flightplan = value;
  }

  /* Get nearest flight plan leg to given screen position xs/ys. */
  void getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                  maptypes::MapSearchResult& mapobjects) const;

  /* @return true if departure is an airport and parking is set */
  bool hasDepartureParking() const;

  /* @return true if departure is an airport and a start position is set */
  bool hasDepartureStart() const;

  /* @return true if flight plan has no departure, no destination and no intermediate waypoints */
  bool isFlightplanEmpty() const;

  /* @return true if departure is an airport */
  bool hasValidDeparture() const;

  /* @return true if destination is an airport */
  bool hasValidDestination() const;

  /* @return true if departure start position is a helipad */
  bool hasDepartureHelipad() const;

  /* @return true if has intermediate waypoints */
  bool hasEntries() const;

  /* @return true if it has at least two waypoints */
  bool canCalcRoute() const;

  /* Value for invalid/not found distances */
  const static float INVALID_DISTANCE_VALUE;

  void copy(const RouteMapObjectList& other);

  int getNextUserWaypointNumber() const;

private:
  float totalDistance = 0.f;
  atools::fs::pln::Flightplan flightplan;

};

#endif // LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
