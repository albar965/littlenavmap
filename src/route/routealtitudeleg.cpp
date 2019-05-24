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

#include "route/routealtitudeleg.h"

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
  out << obj.getIdent()
      << obj.getLineString()
      << "TOC" << obj.isTopOfClimb()
      << "TOD" << obj.isTopOfDescent()
      << "speed" << obj.getAverageSpeedKts()
      << "travel" << obj.getTravelTimeHours()
      << "fuel" << obj.getFuel()
      << "missed" << obj.isMissed()
      << "procedure" << obj.isAnyProcedure()
      << "wind speed" << obj.getWindSpeed()
      << "wind dir" << obj.getWindDirection()
      << "geometry" << obj.getGeometry() << "NM/ft";
  return out;
}
