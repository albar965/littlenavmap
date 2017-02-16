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

#include "route/routemapobjectlist.h"

#include "geo/calculations.h"
#include "common/maptools.h"
#include "common/unit.h"

#include <QRegularExpression>

RouteMapObjectList::RouteMapObjectList()
{

}

RouteMapObjectList::RouteMapObjectList(const RouteMapObjectList& other)
  : QList<RouteMapObject>(other)
{
  copy(other);
}

RouteMapObjectList::~RouteMapObjectList()
{

}

RouteMapObjectList& RouteMapObjectList::operator=(const RouteMapObjectList& other)
{
  clear();
  append(other);
  copy(other);

  return *this;
}

void RouteMapObjectList::copy(const RouteMapObjectList& other)
{
  totalDistance = other.totalDistance;
  flightplan = other.flightplan;
  approachLegs = other.approachLegs;

  // Update flightplan pointers to this instance
  for(RouteMapObject& rmo : *this)
    rmo.setFlightplan(&flightplan);
}

int RouteMapObjectList::getNextUserWaypointNumber() const
{
  /* Get number from user waypoint from user defined waypoint in fs flight plan */
  static const QRegularExpression USER_WP_ID("^WP([0-9]+)$");

  int nextNum = 0;

  for(const atools::fs::pln::FlightplanEntry& entry : flightplan.getEntries())
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::USER)
      nextNum = std::max(QString(USER_WP_ID.match(entry.getWaypointId()).captured(1)).toInt(), nextNum);
  }
  return nextNum + 1;
}

int RouteMapObjectList::getNearestLegOrPointIndex(const atools::geo::Pos& pos) const
{
  float crossDist;
  int legIndex, apprLegIndex;
  getNearestLegIndex(pos, crossDist, legIndex, apprLegIndex);

  float pointDistance;
  int pointIndex = nearestPointIndex(pos, pointDistance);
  if(pointDistance < std::abs(crossDist))
    return -pointIndex;
  else
    return legIndex;
}

int RouteMapObjectList::nearestPointIndex(const atools::geo::Pos& pos, float& pointDistanceNm) const
{
  int nearest = maptypes::INVALID_INDEX_VALUE;
  float minDistance = maptypes::INVALID_DISTANCE_VALUE;

  pointDistanceNm = maptypes::INVALID_DISTANCE_VALUE;

  for(int i = 0; i < size(); i++)
  {
    float distance = at(i).getPosition().distanceMeterTo(pos);
    if(distance < minDistance)
    {
      minDistance = distance;
      nearest = i + 1;
    }
  }

  if(minDistance < maptypes::INVALID_DISTANCE_VALUE)
    pointDistanceNm = atools::geo::meterToNm(minDistance);
  return nearest;
}

void RouteMapObjectList::getNearestLegIndex(const atools::geo::Pos& pos, float& crossTrackDistanceNm,
                                            int& routeIndex, int& approachIndex) const
{
  nearestLegIndex(pos, crossTrackDistanceNm, routeIndex, approachIndex);
}

int RouteMapObjectList::getNearestRouteLegIndex(const atools::geo::Pos& pos) const
{
  int routeIndex, approachIndex;
  getNearestLegIndex(pos, routeIndex, approachIndex);
  return routeIndex;
}

int RouteMapObjectList::getNearestApproachLegIndex(const atools::geo::Pos& pos) const
{
  int routeIndex, approachIndex;
  getNearestLegIndex(pos, routeIndex, approachIndex);
  return approachIndex;
}

void RouteMapObjectList::getNearestLegIndex(const atools::geo::Pos& pos, int& routeIndex, int& approachIndex) const
{
  float dummy;

  nearestLegIndex(pos, dummy, routeIndex, approachIndex);
}

void RouteMapObjectList::nearestLegIndex(const atools::geo::Pos& pos, float& crossTrackDistanceNm, int& routeIndex,
                                         int& approachIndex) const
{
  crossTrackDistanceNm = maptypes::INVALID_DISTANCE_VALUE;
  routeIndex = maptypes::INVALID_INDEX_VALUE;
  approachIndex = maptypes::INVALID_INDEX_VALUE;

  if(!pos.isValid())
    return;

  float minDistance = maptypes::INVALID_DISTANCE_VALUE;

  // Check only until the approach starts if required
  atools::geo::CrossTrackStatus status;

  for(int i = 1; i < calculateApproachIndex(); i++)
  {
    float crossTrack = pos.distanceMeterToLine(at(i - 1).getPosition(), at(i).getPosition(), status);
    float distance = std::abs(crossTrack);

    if(status != atools::geo::INVALID && distance < minDistance)
    {
      minDistance = distance;
      crossTrackDistanceNm = crossTrack;
      routeIndex = i;
    }
  }

  if(crossTrackDistanceNm < maptypes::INVALID_DISTANCE_VALUE)
  {
    if(crossTrackDistanceNm < atools::geo::nmToMeter(100.f) && crossTrackDistanceNm > atools::geo::nmToMeter(-100.f))
      crossTrackDistanceNm = atools::geo::meterToNm(crossTrackDistanceNm);
    else
    {
      // Too far away from any segment or point
      crossTrackDistanceNm = maptypes::INVALID_DISTANCE_VALUE;
      routeIndex = maptypes::INVALID_INDEX_VALUE;
    }
  }

  if(!approachLegs.isEmpty())
  {
    // Check if an approach leg is closer
    float crossTrackApprDistanceNm;

    int nearestApprLegIndex = approachLegs.getNearestLegIndex(pos, shownTypes, crossTrackApprDistanceNm);
    if(std::abs(crossTrackApprDistanceNm) < std::abs(crossTrackDistanceNm))
    {
      routeIndex = maptypes::INVALID_INDEX_VALUE;
      approachIndex = nearestApprLegIndex;
      crossTrackDistanceNm = crossTrackApprDistanceNm;
    }
  }
}

bool RouteMapObjectList::getRouteDistances(const atools::geo::Pos& pos,
                                           float *distFromStart, float *distToDest,
                                           float *nextLegDistance, float *crossTrackDistance,
                                           int *nextRouteLegIndex) const
{
  float crossDist;
  int legIndex, apprIndex;
  getNearestLegIndex(pos, crossDist, legIndex, apprIndex);

  float pointDistance;
  int pointIndex = nearestPointIndex(pos, pointDistance);
  if(pointDistance < std::abs(crossDist))
  {
    legIndex = pointIndex;
    if(crossTrackDistance != nullptr)
      *crossTrackDistance = maptypes::INVALID_DISTANCE_VALUE;
  }
  else if(crossTrackDistance != nullptr)
    *crossTrackDistance = crossDist;

  if(legIndex != maptypes::INVALID_INDEX_VALUE)
  {
    if(legIndex >= size())
      legIndex = size() - 1;

    if(nextRouteLegIndex != nullptr)
      *nextRouteLegIndex = legIndex;

    const atools::geo::Pos& position = at(legIndex).getPosition();
    float distToCur = atools::geo::meterToNm(position.distanceMeterTo(pos));

    if(nextLegDistance != nullptr)
      *nextLegDistance = distToCur;

    if(distFromStart != nullptr)
    {
      *distFromStart = 0.f;
      for(int i = 0; i <= legIndex; i++)
        *distFromStart += at(i).getDistanceTo();
      *distFromStart -= distToCur;
      *distFromStart = std::abs(*distFromStart);
    }

    if(distToDest != nullptr)
    {
      *distToDest = 0.f;
      for(int i = legIndex + 1; i < size(); i++)
        *distToDest += at(i).getDistanceTo();
      *distToDest += distToCur;
      *distToDest = std::abs(*distToDest);
    }

    return true;
  }
  return false;
}

float RouteMapObjectList::getTopOfDescentFromStart() const
{
  if(!isEmpty())
    return getTotalDistance() - getTopOfDescentFromDestination();

  return 0.f;
}

float RouteMapObjectList::getTopOfDescentFromDestination() const
{
  if(!isEmpty())
  {
    float cruisingAltitude = Unit::rev(getFlightplan().getCruisingAltitude(), Unit::altFeetF);
    float diff = (cruisingAltitude - last().getPosition().getAltitude());

    // Either nm per 1000 something alt or km per 1000 something alt
    float distNm = Unit::rev(OptionData::instance().getRouteTodRule(), Unit::distNmF);
    float altFt = Unit::rev(1000.f, Unit::altFeetF);

    return diff / altFt * distNm;
  }
  return 0.f;
}

atools::geo::Pos RouteMapObjectList::getTopOfDescent() const
{
  if(!isEmpty())
    return positionAtDistance(getTopOfDescentFromStart());

  return atools::geo::EMPTY_POS;
}

atools::geo::Pos RouteMapObjectList::positionAtDistance(float distFromStart) const
{
  if(distFromStart < 0.f || distFromStart > totalDistance)
    return atools::geo::EMPTY_POS;

  atools::geo::Pos retval;

  float total = 0.f;
  int foundIndex = maptypes::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
  for(int i = 1; i < size(); i++)
  {
    if(total + at(i).getDistanceTo() > distFromStart)
    {
      // Distance already within leg
      foundIndex = i - 1;
      break;
    }
    total += at(i).getDistanceTo();
  }

  if(foundIndex != maptypes::INVALID_INDEX_VALUE)
  {
    float fraction = (distFromStart - total) / at(foundIndex + 1).getDistanceTo();
    retval = at(foundIndex).getPosition().interpolate(at(foundIndex + 1).getPosition(), fraction);
  }
  return retval;
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

bool RouteMapObjectList::hasDepartureParking() const
{
  if(hasValidDeparture())
    return first().getDepartureParking().position.isValid();

  return false;
}

bool RouteMapObjectList::hasDepartureHelipad() const
{
  if(hasDepartureStart())
    return first().getDepartureStart().helipadNumber > 0;

  return false;
}

bool RouteMapObjectList::hasDepartureStart() const
{
  if(hasValidDeparture())
    return first().getDepartureStart().position.isValid();

  return false;
}

bool RouteMapObjectList::isFlightplanEmpty() const
{
  return getFlightplan().isEmpty();
}

bool RouteMapObjectList::hasValidDeparture() const
{
  return !getFlightplan().isEmpty() &&
         getFlightplan().getEntries().first().getWaypointType() == atools::fs::pln::entry::AIRPORT &&
         first().isValid();
}

bool RouteMapObjectList::hasValidDestination() const
{
  return !getFlightplan().isEmpty() &&
         getFlightplan().getEntries().last().getWaypointType() == atools::fs::pln::entry::AIRPORT &&
         last().isValid();
}

bool RouteMapObjectList::hasEntries() const
{
  return getFlightplan().getEntries().size() > 2;
}

bool RouteMapObjectList::canCalcRoute() const
{
  return getFlightplan().getEntries().size() >= 2;
}

int RouteMapObjectList::calculateApproachIndex() const
{
  // Get type and id of initial fix if there is an approach shown
  maptypes::MapObjectRef initialFixRef({-1, maptypes::NONE});

  if(!approachLegs.isEmpty())
  {
    const maptypes::MapApproachLeg& initialFix = approachLegs.at(0);
    if(initialFix.navaids.hasWaypoints())
    {
      initialFixRef.id = initialFix.navaids.waypoints.first().id;
      initialFixRef.type = maptypes::WAYPOINT;
    }
    else if(initialFix.navaids.hasVor())
    {
      initialFixRef.id = initialFix.navaids.vors.first().id;
      initialFixRef.type = maptypes::VOR;
    }
    else if(initialFix.navaids.hasNdb())
    {
      initialFixRef.id = initialFix.navaids.ndbs.first().id;
      initialFixRef.type = maptypes::NDB;
    }
  }

  int approachIndex = size();

  for(int i = 0; i < size(); i++)
  {
    const RouteMapObject& obj = at(i);

    if(obj.getId() == initialFixRef.id && obj.getMapObjectType() == initialFixRef.type)
    {
      approachIndex = i + 1;
      break;
    }
  }
  return approachIndex;
}

void RouteMapObjectList::setApproachLegs(const maptypes::MapApproachLegs& legs)
{
  qDebug() << Q_FUNC_INFO;

  approachLegs = legs;
}
