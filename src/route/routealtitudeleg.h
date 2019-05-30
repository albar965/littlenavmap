/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
 * Contains all calculated altitude information for a route leg. This includes distance from start and
 * altitudes for leg start and end.
 *
 * Contains fuel and time information calculated from aircraft performance too.
 *
 * Geometry might contain more than two points for TOC and/or TOD legs.
 */
class RouteAltitudeLeg
{
public:
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

  float getTravelTimeHours() const
  {
    return travelTimeHours;
  }

  /* TAS */
  float getAverageSpeedKts() const
  {
    return averageSpeedKts;
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

  /* Mostly for  debugging purposes */
  const QString& getIdent() const
  {
    return ident;
  }

  /* Fuel consumption for this leg. Volume or weight (gal/lbs) depending on performance */
  float getFuel() const
  {
    return fuel;
  }

  /* First and last point of this leg. Pos 2 in the line is position of this leg waypoint.
   * Can contain more than two points for TOC and/or TOD legs.
   * Altitude in Pos is set */
  const atools::geo::LineString& getLineString() const
  {
    return line;
  }

  /* Average wind direction for leg degrees true */
  float getAverageWindDirection() const
  {
    return avgWindDirection;
  }

  /* Average wind speed for this leg in knots */
  float getAverageWindSpeed() const
  {
    return avgWindSpeed;
  }

  /* Average head wind speed for this leg in knots. Negative values are tailwind. */
  float getAverageHeadWind() const
  {
    return avgWindHead;
  }

  /* Average crosswind for this leg. If cross wind is < 0 wind is from left */
  float getAverageCrossWind() const
  {
    return avgWindCross;
  }

  float getWindSpeed() const
  {
    return windSpeed;
  }

  float getWindDirection() const
  {
    return windDirection;
  }

private:
  friend class RouteAltitude;

  /* Set altitude for all geometry */
  void setAlt(float alt);

  /* Waypoint of last leg */
  float y1() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.first().y());
  }

  void setY1(float y);

  /* Waypoint of this  leg */
  float y2() const
  {
    return geometry.isEmpty() ? 0.f : static_cast<float>(geometry.last().y());
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

  atools::geo::LineString line;
  QString ident;
  QPolygonF geometry;
  proc::MapAltRestriction restriction;
  bool procedure = false, missed = false, topOfClimb = false, topOfDescent = false;
  float travelTimeHours = 0.f;
  float fuel = 0.f;
  float averageSpeedKts = 0.f;

  // Average wind values for this leg
  float avgWindDirection = 0.f, avgWindSpeed = 0.f, avgWindHead = 0.f, avgWindCross = 0.f;

  // Wind at the waypoint (y2)
  float windSpeed = 0.f, windDirection = 0.f;
};

QDebug operator<<(QDebug out, const RouteAltitudeLeg& obj);

#endif // LNM_ROUTEALTITUDELEG_H
