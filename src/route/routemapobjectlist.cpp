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

#include "routemapobjectlist.h"
#include "geo/calculations.h"

RouteMapObjectList::RouteMapObjectList()
{

}

RouteMapObjectList::~RouteMapObjectList()
{

}

int RouteMapObjectList::nearestLegIndex(const atools::geo::Pos& pos) const
{
  int nearest = -1;
  float minDistance = std::numeric_limits<float>::max();
  for(int i = 1; i < size(); i++)
  {
    bool valid;
    float distance = std::abs(pos.distanceMeterToLine(at(i - 1).getPosition(),
                                                      at(i).getPosition(), valid));

    if(valid && distance < minDistance)
    {
      minDistance = distance;
      nearest = i;
    }
  }
  for(int i = 0; i < size(); i++)
  {
    float distance = at(i).getPosition().distanceMeterTo(pos);
    if(distance < minDistance)
    {
      minDistance = distance;
      nearest = i + 1;
    }
  }
  return nearest;
}

bool RouteMapObjectList::getRouteDistances(const atools::geo::Pos& pos, float& distFromStart,
                                           float& distToDest) const
{
  int index = nearestLegIndex(pos);
  if(index != -1)
  {
    if(index >= size())
      index = size() - 1;

    const atools::geo::Pos& position = at(index).getPosition();
    float distToCur = atools::geo::meterToNm(position.distanceMeterTo(pos));

    distFromStart = 0.f;
    for(int i = 0; i <= index; i++)
      distFromStart += at(i).getDistanceTo();
    distFromStart -= distToCur;

    distToDest = 0.f;
    for(int i = index + 1; i < size(); i++)
      distToDest += at(i).getDistanceTo();
    distToDest += distToCur;
    return true;
  }
  return false;
}
