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

#include "route/routealtitudeleg.h"

RouteAltitudeLeg::RouteAltitudeLeg()
{

}

float RouteAltitudeLeg::getMaxAltitude() const
{
  if(geometry.isEmpty())
    return map::INVALID_ALTITUDE_VALUE;
  else
    // Rectangle bottom and top are revered since this is no screen coordinate system
    return static_cast<float>(geometry.boundingRect().bottom());
}

float RouteAltitudeLeg::getDistanceFromStart() const
{
  if(geometry.isEmpty())
    return map::INVALID_DISTANCE_VALUE;
  else
    return static_cast<float>(geometry.constLast().x());
}

float RouteAltitudeLeg::getDistanceTo() const
{
  if(geometry.isEmpty())
    return map::INVALID_DISTANCE_VALUE;
  else
    return static_cast<float>(getDistanceFromStart() - geometry.constFirst().x());
}

float RouteAltitudeLeg::climbDist() const
{
  return static_cast<float>(topOfClimb ? geometry.at(1).x() - geometry.at(0).x() : 0.);
}

float RouteAltitudeLeg::tocPos() const
{
  return static_cast<float>(topOfClimb ? geometry.at(1).x() : map::INVALID_DISTANCE_VALUE);
}

float RouteAltitudeLeg::todPos() const
{
  return static_cast<float>(topOfDescent ? geometry.at(topOfClimb ? 2 : 1).x() : map::INVALID_DISTANCE_VALUE);
}

float RouteAltitudeLeg::descentDist() const
{
  return static_cast<float>(topOfDescent ?
                            geometry.at(topOfClimb ? 3 : 2).x() - geometry.at(topOfClimb ? 2 : 1).x() : 0.);
}

float RouteAltitudeLeg::cruiseDist() const
{
  return getDistanceTo() - descentDist() - climbDist();
}

float RouteAltitudeLeg::getFuelFromDistToEnd(float distFromDeparture) const
{
  // Length of given distance to end of this leg
  float legRemainingLength = x2() - distFromDeparture;

  float fuelToEnd = 0.f;
  if(topOfClimb && topOfDescent)
  {
    // TOC and TOC leg =========================================================
    if(distFromDeparture < tocPos())
      // Before TOC
      fuelToEnd = climbFuel / climbDist() * (tocPos() - distFromDeparture) + cruiseFuel + descentFuel;
    else if(distFromDeparture < todPos())
      // After TOC - part is cruise and all descent
      fuelToEnd = cruiseFuel / cruiseDist() * (todPos() - distFromDeparture) + descentFuel;
    else
      // After TOD - part is descent
      fuelToEnd = descentFuel / descentDist() * legRemainingLength;
  }
  else if(topOfClimb)
  {
    // TOC leg =========================================================
    if(distFromDeparture < tocPos())
      // Before TOC - part of climb and full cruise
      fuelToEnd = climbFuel / climbDist() * (tocPos() - distFromDeparture) + cruiseFuel;
    else
      // After TOC - all is cruise
      fuelToEnd = cruiseFuel / cruiseDist() * legRemainingLength;
  }
  else if(topOfDescent)
  {
    // TOD leg =========================================================
    if(distFromDeparture < todPos())
      // Part of cruise and full descent
      fuelToEnd = cruiseFuel / cruiseDist() * (todPos() - distFromDeparture) + descentFuel;
    else
      // After TOD - all is descent
      fuelToEnd = descentFuel / descentDist() * legRemainingLength;
  }
  else
    // Normal leg =========================================================
    // All is either climb, cruise, descent or alternate
    fuelToEnd = getFuel() / getDistanceTo() * legRemainingLength;

  return fuelToEnd;
}

float RouteAltitudeLeg::getFuelFromDistToDestination(float distFromDeparture) const
{
  return fuelToDest - getFuel() + getFuelFromDistToEnd(distFromDeparture);
}

float RouteAltitudeLeg::getTimeFromDistToEnd(float distFromDeparture) const
{
  // Length of given distance to end of this leg
  float legRemainingLength = x2() - distFromDeparture;

  float timeToEnd = 0.f;
  if(topOfClimb && topOfDescent)
  {
    // TOC and TOC leg =========================================================
    if(distFromDeparture < tocPos())
      // Before TOC
      timeToEnd = climbTime / climbDist() * (tocPos() - distFromDeparture) + cruiseTime + descentTime;
    else if(distFromDeparture < todPos())
      // After TOC - part is cruise and all descent
      timeToEnd = cruiseTime / cruiseDist() * (todPos() - distFromDeparture) + descentTime;
    else
      // After TOD - part is descent
      timeToEnd = descentTime / descentDist() * legRemainingLength;
  }
  else if(topOfClimb)
  {
    // TOC leg =========================================================
    if(distFromDeparture < tocPos())
      // Before TOC - part of climb and full cruise
      timeToEnd = climbTime / climbDist() * (tocPos() - distFromDeparture) + cruiseTime;
    else
      // After TOC - all is cruise
      timeToEnd = cruiseTime / cruiseDist() * legRemainingLength;
  }
  else if(topOfDescent)
  {
    // TOD leg =========================================================
    if(distFromDeparture < todPos())
      // Part of cruise and full descent
      timeToEnd = cruiseTime / cruiseDist() * (todPos() - distFromDeparture) + descentTime;
    else
      // After TOD - all is descent
      timeToEnd = descentTime / descentDist() * legRemainingLength;
  }
  else
    // Normal leg =========================================================
    // All is either climb, cruise, descent or alternate
    timeToEnd = getTime() / getDistanceTo() * legRemainingLength;

  return timeToEnd;
}

float RouteAltitudeLeg::getTimeFromDistToDestination(float distFromStart) const
{
  return timeToDest - getTime() + getTimeFromDistToEnd(distFromStart);
}

void RouteAltitudeLeg::setAlt(float alt)
{
  // Set altitude for all points
  for(QPointF& pt : geometry)
    pt.setY(alt);
}

void RouteAltitudeLeg::setY1(float y)
{
  if(isPoint())
    setAlt(y);
  else if(!geometry.isEmpty())
    geometry.first().setY(y);
}

void RouteAltitudeLeg::setY2(float y)
{
  if(isPoint())
    setAlt(y);
  else if(!geometry.isEmpty())
    geometry.last().setY(y);
}

bool RouteAltitudeLeg::isPoint() const
{
  return dx() < atools::geo::Pos::POS_EPSILON_5M;
}

float RouteAltitudeLeg::dx() const
{
  if(!geometry.isEmpty())
    return static_cast<float>(geometry.constLast().x() - geometry.constFirst().x());
  else
    return map::INVALID_DISTANCE_VALUE;
}

QDebug operator<<(QDebug out, const RouteAltitudeLeg& obj)
{
  QDebugStateSaver saver(out);
  out << obj.ident
      << obj.line
      << "TOC" << obj.topOfClimb
      << "TOD" << obj.topOfDescent
      << "alternate" << obj.alternate
      << "missed" << obj.missed
      << "procedure" << obj.procedure
      << "climb time" << obj.climbTime
      << "cruise time" << obj.cruiseTime
      << "descent time" << obj.descentTime << endl
      << "climb fuel" << obj.climbFuel
      << "cruise fuel" << obj.cruiseFuel
      << "descent fuel" << obj.descentFuel
      << "fuel to dest" << obj.fuelToDest << endl
      << "time to dest" << obj.timeToDest << endl
      << "wind speed" << obj.windSpeed
      << "wind dir" << obj.windDirection << endl
      << "geometry" << obj.geometry << "NM/ft" << endl
      << "angles" << obj.angles << "°" << endl
      << "line" << obj.line << endl
      << "geoLine" << obj.geoLine;
  return out;
}
