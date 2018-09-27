/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

  /* First point is equal to last point of previous leg. Last point is position of the waypoint for this leg.
   * Can contain more than two points for TOC and/or TOD legs.
   * x = distance from start and y = altitude
   */
  const QPolygonF& getGeometry() const
  {
    return geometry;
  }

private:
  friend class RouteAltitude;

  /* Set altitude for all geometry */
  void setAlt(float alt);

  /* Waypoint of last leg */
  float y1() const
  {
    return static_cast<float>(geometry.first().y());
  }

  void setY1(float y)
  {
    geometry.first().setY(y);
  }

  /* Waypoint of this  leg */
  float y2() const
  {
    return static_cast<float>(geometry.last().y());
  }

  void setY2(float y)
  {
    geometry.last().setY(y);
  }

  QPolygonF geometry;
  proc::MapAltRestriction restriction;
  bool procedure = false, topOfClimb = false, topOfDescent = false;

};

#endif // LNM_ROUTEALTITUDELEG_H
