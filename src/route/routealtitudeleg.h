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

#ifndef LNM_ROUTEALTITUDELEG_H
#define LNM_ROUTEALTITUDELEG_H

#include "common/proctypes.h"

#include <QPolygonF>

/*
 * Contains all calculated altitude information for a route leg. This includes distance from start (x) and
 * altitudes (y) for leg start and end.
 * Alternate leg distances and times are measured from destination airport.
 *
 * Contains fuel and time information calculated from aircraft performance too.
 *
 * Geometry might contain more than two points for TOC and/or TOD legs:
 * - Normal leg: 2 points;
 * - TOC or TOD leg: 3 points;
 * - TOC and TOD leg: 4 points;
 *
 */
class RouteAltitudeLeg
{
public:
  RouteAltitudeLeg();

  /* Maximum altitude for this leg in feet */
  float getMaxAltitude() const;

  /* Distance from departute in NM */
  float getDistanceFromStart() const;

  /* Distance from last leg to this one i.e. this leg's length */
  float getDistanceTo() const;

  /* Altitude restriction from procedures if available. Otherwise invalid. */
  const proc::MapAltRestriction& getRestriction() const
  {
    return restriction;
  }

  /* true if this is a dummy leg without geometry. */
  bool isEmpty() const
  {
    return geometry.isEmpty();
  }

  /* Position of the waypoint of this leg. x = distance from start and y = altitude */
  QPointF asPoint() const
  {
    return geometry.isEmpty() ? QPointF() : geometry.last();
  }

  /* true if this leg should be drawn as a procedure leg. Does not neccesarily mean
   * that this *is* a procedure leg. */
  bool isAnyProcedure() const
  {
    return procedure;
  }

  bool isMissed() const
  {
    return missed;
  }

  bool isAlternate() const
  {
    return alternate;
  }

  /* Traveling time along this leg in hours */
  float getTime() const
  {
    return climbTime + cruiseTime + descentTime;
  }

  /* First point is equal to last point of previous leg. Last point is position of the waypoint for this leg.
   * Can contain more than two points for TOC and/or TOD legs.
   * x = distance from start and y = altitude
   */
  const QPolygonF& getGeometry() const
  {
    return geometry;
  }

  bool isTopOfClimb() const
  {
    return topOfClimb;
  }

  bool isTopOfDescent() const
  {
    return topOfDescent;
  }

  /* For debugging purposes and error reporting */
  const QString& getIdent() const
  {
    return ident;
  }

  /* For debugging purposes and error reporting */
  const QString& getProcedureType() const
  {
    return procedureType;
  }

  /* Fuel consumption for this leg. Volume or weight (gal/lbs) depending on performance */
  float getFuel() const
  {
    return climbFuel + cruiseFuel + descentFuel;
  }

  /* Fuel from start of this leg to the destination or alternate */
  float getFuelToDestination() const
  {
    return fuelToDest;
  }

  /* Traveling time from start of leg to destination or alternate in hours */
  float getTimeToDest() const
  {
    return timeToDest;
  }

  /* First and last point of this leg. Pos 2 in the line is position of this leg waypoint.
   * Can contain more than two points for TOC and/or TOD legs.
   * Altitude in Pos is set */
  const atools::geo::LineString& getLineString() const
  {
    return line;
  }

  float getWindSpeed() const
  {
    return windSpeed;
  }

  float getWindDirection() const
  {
    return windDirection;
  }

  /* Calculate fuel and time to destination from this leg at position distFromStart
   * fuel in local units (gal or lbs) depending on performance and distFromStart in NM.
   * distFromStart has to match this leg. */
  void getFuelAndTimeFromDistToDestination(float& fuelToDist, float& timeToDist, float distFromStart) const;

private:
  friend class RouteAltitude;
  friend QDebug operator<<(QDebug out, const RouteAltitudeLeg& obj);

  /* Set altitude for all geometry */
  void setAlt(float alt);

  /* Waypoint of last leg - altitude */
  float y1() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.first().y());
  }

  /* Waypoint of last leg - distance from start */
  float x1() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.first().x());
  }

  void setY1(float y);

  /* Waypoint of this leg - altitude */
  float y2() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.last().y());
  }

  /* Waypoint of this leg - distance from start */
  float x2() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.last().x());
  }

  QPointF pt1() const
  {
    return geometry.isEmpty() ? QPointF() : geometry.first();
  }

  QPointF pt2() const
  {
    return geometry.isEmpty() ? QPointF() : geometry.last();
  }

  /* Sets all if leg is a point */
  void setY2(float y);

  /* Length of this leg */
  float dx() const;

  /* Length is 0 */
  bool isPoint() const;

  /* Distances for each phase in this leg. Sum is equal to leg length. */
  float climbDist() const;
  float descentDist() const;
  float cruiseDist() const;

  /* Distance to TOC and TOD from departure or invalid value if not TOC or TOD leg */
  float tocPos() const;
  float todPos() const;

  atools::geo::LineString line;
  QString ident, procedureType;
  QPolygonF geometry;
  proc::MapAltRestriction restriction;
  bool procedure = false, missed = false, alternate = false, topOfClimb = false, topOfDescent = false;

  /* Fuel and time for all phases for this leg */
  float climbTime = 0.f, cruiseTime = 0.f, descentTime = 0.f;
  float climbFuel = 0.f, cruiseFuel = 0.f, descentFuel = 0.f;

  /* Internal values used during calculation */
  float climbWindSpeed = 0.f, cruiseWindSpeed = 0.f, descentWindSpeed = 0.f;
  float climbWindDir = 0.f, cruiseWindDir = 0.f, descentWindDir = 0.f;
  float climbWindHead = 0.f, cruiseWindHead = 0.f, descentWindHead = 0.f;

  float fuelToDest = 0.f; /* Fuel from start of this leg to the destination or alternate */
  float timeToDest = 0.f; /* Time from start of this leg to the destination or alternate */

  // Wind at the waypoint (y2)
  float windSpeed = 0.f, windDirection = 0.f;

};

QDebug operator<<(QDebug out, const RouteAltitudeLeg& obj);

#endif // LNM_ROUTEALTITUDELEG_H
