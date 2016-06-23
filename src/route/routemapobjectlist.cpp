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

#include "route/routemapobjectlist.h"

#include "geo/calculations.h"
#include "common/maptools.h"

const float RouteMapObjectList::INVALID_DISTANCE_VALUE = std::numeric_limits<float>::max();

RouteMapObjectList::RouteMapObjectList()
{

}

RouteMapObjectList::~RouteMapObjectList()
{

}

int RouteMapObjectList::getNearestLegOrPointIndex(const atools::geo::Pos& pos) const
{
  float crossDist;
  int legIndex = getNearestLegIndex(pos, crossDist);

  float pointDistance;
  int pointIndex = getNearestPointIndex(pos, pointDistance);
  if(pointDistance < std::abs(crossDist))
    return pointIndex;
  else
    return legIndex;
}

int RouteMapObjectList::getNearestPointIndex(const atools::geo::Pos& pos, float& pointDistance) const
{
  int nearest = -1;
  float minDistance = INVALID_DISTANCE_VALUE;

  pointDistance = INVALID_DISTANCE_VALUE;

  for(int i = 0; i < size(); i++)
  {
    float distance = at(i).getPosition().distanceMeterTo(pos);
    if(distance < minDistance)
    {
      minDistance = distance;
      nearest = i + 1;
    }
  }

  if(minDistance != INVALID_DISTANCE_VALUE)
    pointDistance = atools::geo::meterToNm(minDistance);
  return nearest;
}

int RouteMapObjectList::getNearestLegIndex(const atools::geo::Pos& pos, float& crossTrackDistance) const
{
  int nearestLeg = -1;

  float minDistance = std::numeric_limits<float>::max();

  crossTrackDistance = INVALID_DISTANCE_VALUE;

  for(int i = 1; i < size(); i++)
  {
    bool valid;
    float crossTrack = pos.distanceMeterToLine(at(i - 1).getPosition(), at(i).getPosition(), valid);
    float distance = std::abs(crossTrack);

    if(valid && distance < minDistance)
    {
      minDistance = distance;
      crossTrackDistance = crossTrack;
      nearestLeg = i;
    }
  }

  if(crossTrackDistance != INVALID_DISTANCE_VALUE)
    crossTrackDistance = atools::geo::meterToNm(crossTrackDistance);

  return nearestLeg;
}

bool RouteMapObjectList::getRouteDistances(const atools::geo::Pos& pos,
                                           float *distFromStartNm, float *distToDestNm,
                                           float *nearestLegDistance, float *crossTrackDistance,
                                           int *nearestLegIndex) const
{
  float crossDist;
  int legIndex = getNearestLegIndex(pos, crossDist);

  float pointDistance;
  int pointIndex = getNearestPointIndex(pos, pointDistance);
  if(pointDistance < std::abs(crossDist))
  {
    legIndex = pointIndex;
    if(crossTrackDistance != nullptr)
      *crossTrackDistance = INVALID_DISTANCE_VALUE;
  }
  else if(crossTrackDistance != nullptr)
    *crossTrackDistance = crossDist;

  if(legIndex != -1)
  {
    if(legIndex >= size())
      legIndex = size() - 1;

    if(nearestLegIndex != nullptr)
      *nearestLegIndex = legIndex;

    const atools::geo::Pos& position = at(legIndex).getPosition();
    float distToCur = atools::geo::meterToNm(position.distanceMeterTo(pos));

    if(nearestLegDistance != nullptr)
      *nearestLegDistance = distToCur;

    if(distFromStartNm != nullptr)
    {
      *distFromStartNm = 0.f;
      for(int i = 0; i <= legIndex; i++)
        *distFromStartNm += at(i).getDistanceTo();
      *distFromStartNm -= distToCur;
      *distFromStartNm = std::abs(*distFromStartNm);
    }

    if(distToDestNm != nullptr)
    {
      *distToDestNm = 0.f;
      for(int i = legIndex + 1; i < size(); i++)
        *distToDestNm += at(i).getDistanceTo();
      *distToDestNm += distToCur;
      *distToDestNm = std::abs(*distToDestNm);
    }

    return true;
  }
  return false;
}

void RouteMapObjectList::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                                    maptypes::MapSearchResult& mapobjects) const
{
  using maptools::insertSortedByDistance;

  int x, y;
  int i = 0;
  for(const RouteMapObject& obj : *this)
  {
    if(conv.wToS(obj.getPosition(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
      {
        switch(obj.getMapObjectType())
        {
          case maptypes::VOR :
            {
              maptypes::MapVor vor = obj.getVor();
              vor.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, vor);
            }
            break;
          case maptypes::WAYPOINT:
            {
              maptypes::MapWaypoint wp = obj.getWaypoint();
              wp.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, wp);
            }
            break;
          case maptypes::NDB:
            {
              maptypes::MapNdb ndb = obj.getNdb();
              ndb.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, ndb);
            }
            break;
          case maptypes::AIRPORT:
            {
              maptypes::MapAirport ap = obj.getAirport();
              ap.routeIndex = i;
              insertSortedByDistance(conv, mapobjects.airports, &mapobjects.airportIds, xs, ys, ap);
            }
            break;
          case maptypes::INVALID:
            {
              maptypes::MapUserpoint up;
              up.routeIndex = i;
              up.name = obj.getIdent() + " (not found)";
              up.position = obj.getPosition();
              mapobjects.userPoints.append(up);
            }
            break;
          case maptypes::USER:
            {
              maptypes::MapUserpoint up;
              up.id = i;
              up.routeIndex = i;
              up.name = obj.getIdent();
              up.position = obj.getPosition();
              mapobjects.userPoints.append(up);
            }
            break;
        }
      }
    i++;
  }
}
