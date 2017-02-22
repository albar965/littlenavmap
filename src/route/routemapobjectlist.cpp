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

#include <marble/GeoDataLineString.h>

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::nmToMeter;
using atools::geo::normalizeCourse;
using atools::geo::meterToNm;
using atools::geo::manhattanDistance;

RouteMapObjectList::RouteMapObjectList()
{
  resetActive();
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
  copy(other);

  return *this;
}

void RouteMapObjectList::resetActive()
{
  activeLegResult.distanceFrom1 = activeLegResult.distanceFrom2 = activeLegResult.distance =
                                                                    maptypes::INVALID_DISTANCE_VALUE;
  activeLegResult.status = atools::geo::INVALID;
  activePos = maptypes::PosCourse();
}

void RouteMapObjectList::copy(const RouteMapObjectList& other)
{
  copyNoLegs(other);
  approachLegs = other.approachLegs;
  approachStartIndex = other.approachStartIndex;
}

void RouteMapObjectList::copyNoLegs(const RouteMapObjectList& other)
{
  clear();
  append(other);

  totalDistance = other.totalDistance;
  flightplan = other.flightplan;
  shownTypes = other.shownTypes;
  boundingRect = other.boundingRect;
  activePos = other.activePos;
  trueCourse = other.trueCourse;
  approachStartIndex = size();

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

int RouteMapObjectList::getNearestRouteLegOrPointIndex(const atools::geo::Pos& pos) const
{
  float crossDist;
  int legIndex, apprLegIndex;
  nearestLegIndex(maptypes::PosCourse(pos), crossDist, legIndex, apprLegIndex);

  float pointDistance;
  int pointIndex = nearestPointIndex(pos, pointDistance);
  if(pointDistance < std::abs(crossDist))
    return -pointIndex;
  else
    return legIndex;
}

void RouteMapObjectList::updateActiveLegAndPos(const maptypes::PosCourse& pos)
{
  if(isEmpty() || !pos.isValid())
  {
    activeLeg = maptypes::INVALID_INDEX_VALUE;
    return;
  }

  if(activeLeg == maptypes::INVALID_INDEX_VALUE)
    // Start with first leg
    activeLeg = 1;

  if(activeLeg >= size())
    activeLeg = size() - 1;

  activePos = pos;

  if(size() == 1)
  {
    // Special case point route
    activeLeg = 0;
    // Test if still nearby
    activePos.pos.distanceMeterToLine(first().getPosition(), first().getPosition(), activeLegResult);
  }
  else
  {
    if(activeLeg == 0)
      // Reset from point route
      activeLeg = 1;
    activePos.pos.distanceMeterToLine(at(activeLeg - 1).getPosition(), at(activeLeg).getPosition(), activeLegResult);
  }

  int newLeg = activeLeg + 1;
  float courseDiff = 0.f;
  atools::geo::LineDistance nextLegResult;
  if(newLeg < size())
  {
    Pos pos1 = at(newLeg - 1).getPosition();
    Pos pos2 = at(newLeg).getPosition();
    if(pos1.almostEqual(pos2, 0.f) && newLeg < size() - 2)
    {
      // Catch the case of initial fixes that are points instead of lines and try the second next leg
      newLeg++;
      pos1 = at(newLeg - 1).getPosition();
      pos2 = at(newLeg).getPosition();
    }

    // Test next leg
    activePos.pos.distanceMeterToLine(pos1, pos2, nextLegResult);

    // Calculate course difference
    float legCrs = normalizeCourse(pos1.angleDegTo(pos2));
    courseDiff = atools::mod(pos.course - legCrs + 360.f, 360.f);
    if(courseDiff > 180.f)
      courseDiff = 360.f - courseDiff;
  }

  if(activeLegResult.status == atools::geo::AFTER_END ||
     (std::abs(nextLegResult.distance) < std::abs(activeLegResult.distance) && courseDiff < 90.f))
  {
    // Either left current leg or closer to next and on courses
    if(newLeg < size())
    {
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes & maptypes::APPROACH_MISSED) && at(newLeg).isMissed()))
      {
        // Go to next leg and increase all values
        activeLeg = newLeg;
        pos.pos.distanceMeterToLine(at(activeLeg - 1).getPosition(), at(activeLeg).getPosition(), activeLegResult);
      }
    }
  }
  // qDebug() << "active" << activeLeg << "size" << size();
}

bool RouteMapObjectList::getRouteDistances(float *distFromStart, float *distToDest,
                                           float *nextLegDistance, float *crossTrackDistance,
                                           int *nextRouteLegIndex) const
{
  if(crossTrackDistance != nullptr)
  {
    if(activeLegResult.status == atools::geo::ALONG_TRACK)
      *crossTrackDistance = meterToNm(activeLegResult.distance);
    else
      *crossTrackDistance = maptypes::INVALID_DISTANCE_VALUE;
  }

  int routeIndex = activeLeg;
  if(routeIndex != maptypes::INVALID_INDEX_VALUE)
  {
    if(routeIndex >= size())
      routeIndex = size() - 1;

    if(nextRouteLegIndex != nullptr)
      *nextRouteLegIndex = routeIndex;

    float distToCur = 0.f;

    if(!at(routeIndex).isMissed())
      distToCur = meterToNm(at(routeIndex).getPosition().distanceMeterTo(activePos.pos));

    if(nextLegDistance != nullptr)
      *nextLegDistance = distToCur;

    // Sum up all distances along the legs except missed approaches
    if(distFromStart != nullptr)
    {
      *distFromStart = 0.f;
      for(int i = 0; i <= routeIndex; i++)
      {
        if(!at(i).isMissed())
          *distFromStart += at(i).getDistanceTo();
        else
          break;
      }
      *distFromStart -= distToCur;
      *distFromStart = std::abs(*distFromStart);
    }

    if(distToDest != nullptr)
    {
      *distToDest = 0.f;
      for(int i = routeIndex + 1; i < size(); i++)
      {
        if(!at(i).isMissed())
          *distToDest += at(i).getDistanceTo();
      }
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

float RouteMapObjectList::getTotalDistance() const
{
  return totalDistance;
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

atools::geo::Pos RouteMapObjectList::positionAtDistance(float distFromStartNm) const
{
  if(distFromStartNm < 0.f || distFromStartNm > totalDistance)
    return atools::geo::EMPTY_POS;

  atools::geo::Pos retval;

  float total = 0.f;
  int foundIndex = maptypes::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
  for(int i = 1; i < size(); i++)
  {
    if(total + at(i).getDistanceTo() > distFromStartNm)
    {
      // Distance already within leg
      foundIndex = i - 1;
      break;
    }
    total += at(i).getDistanceTo();
  }

  if(foundIndex != maptypes::INVALID_INDEX_VALUE)
  {
    float fraction = (distFromStartNm - total) / at(foundIndex + 1).getDistanceTo();
    retval = at(foundIndex).getPosition().interpolate(at(foundIndex + 1).getPosition(), fraction);
  }
  return retval;
}

void RouteMapObjectList::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                                    maptypes::MapSearchResult& mapobjects) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < approachStartIndex; i++)
  {
    const RouteMapObject& obj = at(i);
    if(conv.wToS(obj.getPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)
    {
      switch(obj.getMapObjectType())
      {
        case maptypes::VOR:
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
  }
}

bool RouteMapObjectList::hasDepartureParking() const
{
  if(hasValidDeparture())
    return first().getDepartureParking().isValid();

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
    return first().getDepartureStart().isValid();

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

void RouteMapObjectList::setApproachLegs(const maptypes::MapApproachLegs& legs)
{
  approachLegs = legs;
  updateFromApproachLegs();
}

void RouteMapObjectList::updateAll()
{
  updateIndices();
  updateMagvar();
  updateDistancesAndCourse();
  updateBoundingRect();
}

void RouteMapObjectList::updateFromApproachLegs()
{
  /* Calculate the index in the route where an approach or transition starts - size of route if not found */
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

  approachStartIndex = size();

  // Check if the route contains the initial fix and return the index to it
  for(int i = 0; i < size(); i++)
  {
    const RouteMapObject& obj = at(i);

    if(obj.getId() == initialFixRef.id && obj.getMapObjectType() == initialFixRef.type)
    {
      approachStartIndex = i;
      break;
    }
  }

  if(approachStartIndex >= size())
  {
    // No approach legs - erase all in the current object
    QList<RouteMapObject>::iterator it = std::remove_if(begin(), end(),
                                                        [ = ](const RouteMapObject &p)->bool
                                                        {
                                                          return p.isAnyApproach();
                                                        });

    if(it != end())
      erase(it, end());
  }
  else
  {
    // Remove all segments overlapping with route
    erase(begin() + approachStartIndex, end());

    // Add the approach legs
    for(int i = 0; i < approachLegs.size(); ++i)
    {
      RouteMapObject obj(&flightplan);
      obj.createFromApproachLeg(i, approachLegs, i > 0 ? &at(i - 1) : nullptr);
      append(obj);
    }
  }

  updateAll();
  updateActiveLegAndPos(activePos);
}

void RouteMapObjectList::updateIndices()
{
  // Update indices
  for(int i = 0; i < size(); i++)
    (*this)[i].setFlightplanEntryIndex(i);
}

int RouteMapObjectList::getActiveRouteLeg() const
{
  if(activeLeg < maptypes::INVALID_INDEX_VALUE && size() > 1)
    // Last leg before approach is part of route and approach
    return activeLeg <= approachStartIndex ? activeLeg : maptypes::INVALID_INDEX_VALUE;
  else
    return maptypes::INVALID_INDEX_VALUE;
}

int RouteMapObjectList::getActiveApproachLeg() const
{
  if(activeLeg < maptypes::INVALID_INDEX_VALUE && size() > 1)
    return at(activeLeg).isAnyApproach() ? activeLeg - (size() - approachLegs.size()) : maptypes::INVALID_INDEX_VALUE;
  else
    return maptypes::INVALID_INDEX_VALUE;
}

void RouteMapObjectList::setActiveLeg(int value)
{
  if(value > 0 && value < size())
    activeLeg = value;
  else
    activeLeg = 1;

  activePos.pos.distanceMeterToLine(at(activeLeg - 1).getPosition(), at(activeLeg).getPosition(), activeLegResult);
}

void RouteMapObjectList::updateDistancesAndCourse()
{
  totalDistance = 0.f;
  RouteMapObject *last = nullptr;
  for(int i = 0; i < size(); i++)
  {
    RouteMapObject& mapobj = (*this)[i];
    mapobj.updateDistanceAndCourse(i, last);
    if(!mapobj.isMissed())
      totalDistance += mapobj.getDistanceTo();
    last = &mapobj;
  }
}

void RouteMapObjectList::updateMagvar()
{
  // get magvar from internal database objects
  for(int i = 0; i < size(); i++)
    (*this)[i].updateMagvar();

  // Update missing magvar values using neighbour entries
  for(int i = 0; i < size(); i++)
    (*this)[i].updateInvalidMagvar(i, this);

  trueCourse = true;
  // Check if there is any magnetic variance on the route
  // If not (all user waypoints) use true heading
  for(const RouteMapObject& obj : *this)
  {
    // Route contains correct magvar if any of these objects were found
    if(obj.getMapObjectType() & maptypes::NAV_MAGVAR)
    {
      trueCourse = false;
      break;
    }
  }
}

/* Update the bounding rect using marble functions to catch anti meridian overlap */
void RouteMapObjectList::updateBoundingRect()
{
  Marble::GeoDataLineString line;

  for(const RouteMapObject& rmo : *this)
    line.append(Marble::GeoDataCoordinates(rmo.getPosition().getLonX(),
                                           rmo.getPosition().getLatY(), 0., Marble::GeoDataCoordinates::Degree));

  Marble::GeoDataLatLonBox box = Marble::GeoDataLatLonBox::fromLineString(line);
  boundingRect = atools::geo::Rect(box.west(), box.north(), box.east(), box.south());
  boundingRect.toDeg();
}

void RouteMapObjectList::nearestLegIndex(const maptypes::PosCourse& pos, float& crossTrackDistanceMeter,
                                         int& routeIndex, int& approachIndex) const
{
  approachIndex = maptypes::INVALID_INDEX_VALUE;
  routeIndex = maptypes::INVALID_INDEX_VALUE;
  crossTrackDistanceMeter = maptypes::INVALID_DISTANCE_VALUE;

  if(!pos.isValid())
    return;

  int index;
  nearestAllLegIndex(pos, crossTrackDistanceMeter, index);

  if(index != maptypes::INVALID_INDEX_VALUE)
  {
    if(at(index).isAnyApproach())
      approachIndex = index - (size() - approachLegs.size());
    else
      routeIndex = index;
  }
}

int RouteMapObjectList::nearestPointIndex(const atools::geo::Pos& pos, float& pointDistanceMeter) const
{
  int nearest = maptypes::INVALID_INDEX_VALUE;
  float minDistance = maptypes::INVALID_DISTANCE_VALUE;

  pointDistanceMeter = maptypes::INVALID_DISTANCE_VALUE;

  for(int i = 0; i < approachStartIndex; i++)
  {
    float distance = at(i).getPosition().distanceMeterTo(pos);
    if(distance < minDistance)
    {
      minDistance = distance;
      nearest = i + 1;
    }
  }

  if(minDistance < maptypes::INVALID_DISTANCE_VALUE)
    pointDistanceMeter = minDistance;
  return nearest;
}

void RouteMapObjectList::nearestAllLegIndex(const maptypes::PosCourse& pos, float& crossTrackDistanceMeter,
                                            int& index) const
{
  crossTrackDistanceMeter = maptypes::INVALID_DISTANCE_VALUE;
  index = maptypes::INVALID_INDEX_VALUE;

  if(!pos.isValid())
    return;

  float minDistance = maptypes::INVALID_DISTANCE_VALUE;

  // Check only until the approach starts if required
  atools::geo::LineDistance result;

  for(int i = 1; i < size(); i++)
  {
    pos.pos.distanceMeterToLine(at(i - 1).getPosition(), at(i).getPosition(), result);
    float distance = std::abs(result.distance);

    if(result.status != atools::geo::INVALID && distance < minDistance)
    {
      minDistance = distance;
      crossTrackDistanceMeter = result.distance;
      index = i;
    }
  }

  if(crossTrackDistanceMeter < maptypes::INVALID_DISTANCE_VALUE)
  {
    if(std::abs(crossTrackDistanceMeter) > atools::geo::nmToMeter(100.f))
    {
      // Too far away from any segment or point
      crossTrackDistanceMeter = maptypes::INVALID_DISTANCE_VALUE;
      index = maptypes::INVALID_INDEX_VALUE;
    }
  }
}
