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
    return static_cast<float>(geometry.last().x());
}

float RouteAltitudeLeg::getDistanceTo() const
{
  if(geometry.isEmpty())
    return map::INVALID_DISTANCE_VALUE;
  else
    return static_cast<float>(getDistanceFromStart() - geometry.first().x());
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

void RouteAltitudeLeg::getFuelFromDistToDestination(float& fuelToDist, float distFromStart) const
{
  // Fuel of all legs after this one
  fuelToDist = fuelToDest - getFuel();

  // Length of given distance to end of this leg
  float legRemainingLength = x2() - distFromStart;

  if(topOfClimb && topOfDescent)
  {
    // TOC and TOC leg =========================================================
    if(distFromStart < tocPos())
      // Before TOC
      fuelToDist += climbFuel / climbDist() * (tocPos() - distFromStart) + cruiseFuel + descentFuel;
    else if(distFromStart < todPos())
      // After TOC - part is cruise and all descent
      fuelToDist += cruiseFuel / cruiseDist() * (todPos() - distFromStart) + descentFuel;
    else
      // After TOD - part is descent
      fuelToDist += descentFuel / descentDist() * legRemainingLength;
  }
  else if(topOfClimb)
  {
    // TOC leg =========================================================
    if(distFromStart < tocPos())
      // Before TOC - part of climb and full cruise
      fuelToDist += climbFuel / climbDist() * (tocPos() - distFromStart) + cruiseFuel;
    else
      // After TOC - all is cruise
      fuelToDist += cruiseFuel / cruiseDist() * legRemainingLength;
  }
  else if(topOfDescent)
  {
    // TOD leg =========================================================
    if(distFromStart < todPos())
      // Part of cruise and full descent
      fuelToDist += cruiseFuel / cruiseDist() * (todPos() - distFromStart) + descentFuel;
    else
      // After TOD - all is descent
      fuelToDist += descentFuel / descentDist() * legRemainingLength;
  }
  else
    // Normal leg =========================================================
    // All is either climb, cruise, descent or alternate
    fuelToDist += getFuel() / getDistanceTo() * legRemainingLength;
}

void RouteAltitudeLeg::getTimeFromDistToDestination(float& timeToDist, float distFromStart) const
{
  // Time of all legs after this one
  timeToDist = timeToDest - getTime();

  // Length of given distance to end of this leg
  float legRemainingLength = x2() - distFromStart;

  if(topOfClimb && topOfDescent)
  {
    // TOC and TOC leg =========================================================
    if(distFromStart < tocPos())
      // Before TOC
      timeToDist += climbTime / climbDist() * (tocPos() - distFromStart) + cruiseTime + descentTime;
    else if(distFromStart < todPos())
      // After TOC - part is cruise and all descent
      timeToDist += cruiseTime / cruiseDist() * (todPos() - distFromStart) + descentTime;
    else
      // After TOD - part is descent
      timeToDist += descentTime / descentDist() * legRemainingLength;
  }
  else if(topOfClimb)
  {
    // TOC leg =========================================================
    if(distFromStart < tocPos())
      // Before TOC - part of climb and full cruise
      timeToDist += climbTime / climbDist() * (tocPos() - distFromStart) + cruiseTime;
    else
      // After TOC - all is cruise
      timeToDist += cruiseTime / cruiseDist() * legRemainingLength;
  }
  else if(topOfDescent)
  {
    // TOD leg =========================================================
    if(distFromStart < todPos())
      // Part of cruise and full descent
      timeToDist += cruiseTime / cruiseDist() * (todPos() - distFromStart) + descentTime;
    else
      // After TOD - all is descent
      timeToDist += descentTime / descentDist() * legRemainingLength;
  }
  else
    // Normal leg =========================================================
    // All is either climb, cruise, descent or alternate
    timeToDist += getTime() / getDistanceTo() * legRemainingLength;
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
    return static_cast<float>(geometry.last().x() - geometry.first().x());
  else
    return map::INVALID_DISTANCE_VALUE;
}

QDebug operator<<(QDebug out, const RouteAltitudeLeg& obj)
{
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
      << "geometry" << obj.geometry << "NM/ft";
  return out;
}
