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

#ifndef LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
#define LITTLENAVMAP_ROUTEMAPOBJECTLIST_H

#include "routemapobject.h"

#include "fs/pln/flightplan.h"

class CoordinateConverter;
class FlightplanEntryBuilder;

/*
 * Aggregates the flight plan and is a list of all route map objects. Also contains and stores information
 * about the active route leg and current aircraft position.
 */
class RouteMapObjectList :
  private QList<RouteMapObject>
{
public:
  RouteMapObjectList();
  RouteMapObjectList(const RouteMapObjectList& other);
  virtual ~RouteMapObjectList();

  RouteMapObjectList& operator=(const RouteMapObjectList& other);

  /* Update positions, distances and try to select next leg*/
  void updateActiveLegAndPos(const maptypes::PosCourse& pos);
  void updateActiveLegAndPos();

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
   * @param nextRouteLegIndex Index of next leg
   * @return false if no current/next leg was found
   */
  bool getRouteDistances(float *distFromStart, float *distToDest,
                         float *nextLegDistance = nullptr, float *crossTrackDistance = nullptr) const;

  /* Ignores approach objects */
  int getNearestLegResult(const atools::geo::Pos& pos, atools::geo::LineDistance& lineDistanceResult) const;

  int getActiveLegIndex() const
  {
    return activeLeg;
  }

  /* Get active leg or null if this is none */
  const RouteMapObject *getActiveLeg() const;
  const RouteMapObject *getActiveLegCorrected(bool *corrected = nullptr) const;

  bool isActiveMissed() const;

  /* Corrected methods replace the current leg with the initial fix
   * if one follows between route and transition/approach.  */
  int getActiveLegIndexCorrected(bool *corrected = nullptr) const;

  /* At the end of the route and beyond */
  bool isPassedLastLeg() const;

  /* Get top of descent position based on the option setting (default is 3 nm per 1000 ft) */
  atools::geo::Pos getTopOfDescent() const;

  /* Distance from TOD to destination in nm */
  float getTopOfDescentFromDestination() const;
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

  /* Get a new number for a user waypoint for automatic naming */
  int getNextUserWaypointNumber() const;

  bool hasDepartureProcedure() const
  {
    return !departureLegs.isEmpty();
  }

  bool hasArrivalProcedure() const
  {
    return !arrivalLegs.isEmpty();
  }

  bool hasStarProcedure() const
  {
    return !starLegs.isEmpty();
  }

  /* Assign and update internal indexes for approach legs. Depending if legs are type SID, STAR,
   * transition or approach they are added at the end of start of the route
   *  call updateProcedureLegs after setting */
  void setArrivalProcedureLegs(const maptypes::MapApproachLegs& legs)
  {
    arrivalLegs = legs;
  }

  void setStarProcedureLegs(const maptypes::MapApproachLegs& legs)
  {
    starLegs = legs;
  }

  void setDepartureProcedureLegs(const maptypes::MapApproachLegs& legs)
  {
    departureLegs = legs;
  }

  void updateProcedureLegs(FlightplanEntryBuilder *entryBuilder);

  void clearArrivalProcedures();
  void clearDepartureProcedures();
  void clearStarProcedures();

  void setShownMapFeatures(maptypes::MapObjectTypes types)
  {
    shownTypes = types;
  }

  const atools::geo::Rect& getBoundingRect() const
  {
    return boundingRect;
  }

  const atools::geo::Pos& getPositionAt(int i) const
  {
    return at(i).getPosition();
  }

  void updateAll();
  void clearFlightplanProcedureProperties(maptypes::MapObjectTypes type);

  /* Pull only the needed methods in public space */
  using QList<RouteMapObject>::const_iterator;
  using QList<RouteMapObject>::begin;
  using QList<RouteMapObject>::end;
  using QList<RouteMapObject>::at;
  using QList<RouteMapObject>::first;
  using QList<RouteMapObject>::last;
  using QList<RouteMapObject>::size;
  using QList<RouteMapObject>::isEmpty;
  using QList<RouteMapObject>::clear;
  using QList<RouteMapObject>::append;
  using QList<RouteMapObject>::prepend;
  using QList<RouteMapObject>::insert;
  using QList<RouteMapObject>::replace;
  using QList<RouteMapObject>::move;
  using QList<RouteMapObject>::removeAt;
  using QList<RouteMapObject>::removeLast;
  using QList<RouteMapObject>::operator[];

  /* Set active leg and update all internal distances */
  void setActiveLeg(int value);
  void resetActive();

  bool isTrueCourse() const
  {
    return trueCourse;
  }

  bool isAirportAfterArrival(int index);

  const maptypes::MapApproachLegs& getArrivalLegs() const
  {
    return arrivalLegs;
  }

  const maptypes::MapApproachLegs& getStarLegs() const
  {
    return starLegs;
  }

  const maptypes::MapApproachLegs& getDepartureLegs() const
  {
    return departureLegs;
  }

private:
  /* Calculate all distances and courses for route map objects */
  void updateDistancesAndCourse();
  void updateBoundingRect();

  /* Update and calculate magnetic variation for all route map objects */
  void updateMagvar();

  /* Assign index and pointer to flight plan for all objects */
  void updateIndices();

  /* Get a position along the route. Pos is invalid if not along. distFromStart in nm */
  atools::geo::Pos positionAtDistance(float distFromStartNm) const;

  /* Get indexes to nearest approach or route leg and cross track distance to the nearest ofthem in nm */
  void copy(const RouteMapObjectList& other);
  void nearestAllLegIndex(const maptypes::PosCourse& pos, float& crossTrackDistanceMeter, int& index) const;
  void nearestLegResult(const atools::geo::Pos& pos, atools::geo::LineDistance& lineDistanceResult,
                        int& index) const;
  bool isSmaller(const atools::geo::LineDistance& dist1, const atools::geo::LineDistance& dist2, float epsilon);
  void eraseProcedureLegs(maptypes::MapObjectTypes type);

  bool trueCourse = false;

  atools::geo::Rect boundingRect;
  /* Nautical miles not including missed approach */
  float totalDistance = 0.f;
  atools::fs::pln::Flightplan flightplan;
  maptypes::MapApproachLegs arrivalLegs, starLegs, departureLegs;
  maptypes::MapObjectTypes shownTypes;

  int activeLeg = maptypes::INVALID_INDEX_VALUE;
  atools::geo::LineDistance activeLegResult;
  maptypes::PosCourse activePos;

};

#endif // LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
