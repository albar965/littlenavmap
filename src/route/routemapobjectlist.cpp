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
using atools::geo::LineString;
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
  activeLeg = maptypes::INVALID_INDEX_VALUE;
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
  approachStartIndex = maptypes::INVALID_INDEX_VALUE;

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

void RouteMapObjectList::updateActiveLegAndPos()
{
  updateActiveLegAndPos(activePos);
}

/* Compare crosstrack distance fuzzy */
bool RouteMapObjectList::isSmaller(const atools::geo::LineDistance& dist1, const atools::geo::LineDistance& dist2,
                                   float epsilon)
{
  return std::abs(dist1.distance) < std::abs(dist2.distance) + epsilon;
}

void RouteMapObjectList::updateActiveLegAndPos(const maptypes::PosCourse& pos)
{
  if(isEmpty() || !pos.isValid())
  {
    resetActive();
    return;
  }

  if(activeLeg == maptypes::INVALID_INDEX_VALUE)
  {
    // Start with nearest leg
    float crossDummy;
    nearestAllLegIndex(pos, crossDummy, activeLeg);
  }

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

    activePos.pos.distanceMeterToLine(getPositionAt(activeLeg - 1), getPositionAt(activeLeg), activeLegResult);
  }

  // Get potential next leg and course difference
  int nextLeg = activeLeg + 1;
  float courseDiff = 0.f;
  if(nextLeg < size())
  {
    while(at(nextLeg).isApproachPoint() && nextLeg < size() - 2)
      // Catch the case of initial fixes or others that are points instead of lines and try the next legs
      nextLeg++;

    Pos pos1 = getPositionAt(nextLeg - 1);
    Pos pos2 = getPositionAt(nextLeg);

    // Calculate course difference
    float legCrs = normalizeCourse(pos1.angleDegTo(pos2));
    courseDiff = atools::mod(pos.course - legCrs + 360.f, 360.f);
    if(courseDiff > 180.f)
      courseDiff = 360.f - courseDiff;

    // if(at(activeLeg).isAnyApproach())
    // qDebug() << "ACTIVE" << at(activeLeg).getApproachLeg();

    // if(at(nextLeg).isAnyApproach())
    // qDebug() << "NEXT" << at(nextLeg).getApproachLeg();

    // qDebug() << "ACTIVE" << activeLeg << activeLegResult;
    // qDebug() << "legCrs" << legCrs << "course diff" << courseDiff;
    // << "holdStarted" << holdStarted << "inHold" << inHold;

    // Test next leg
    atools::geo::LineDistance nextLegResult;
    activePos.pos.distanceMeterToLine(pos1, pos2, nextLegResult);
    // qDebug() << "NEXT" << nextLeg << nextLegResult;

    bool switchToNextLeg = false;
    if(at(activeLeg).isHold())
    {
      // Test next leg if we can exit a hold
      if(at(nextLeg).getApproachLeg().line.getPos1() == at(activeLeg).getPosition())
      {
        // qDebug() << "HOLD SAME";
        // hold point is the same as next leg starting point
        if(nextLegResult.status == atools::geo::ALONG_TRACK && // on track of next
           std::abs(nextLegResult.distance) < nmToMeter(0.5f) && // not too far away from start of next
           nextLegResult.distanceFrom1 > nmToMeter(0.75f) && // Travelled some distance into the new segment
           courseDiff < 25.f) // Keep course
          switchToNextLeg = true;
      }
      else
      {
        atools::geo::LineDistance resultHold;
        at(activeLeg).getApproachLeg().holdLine.distanceMeterToLine(activePos.pos, resultHold);
        // qDebug() << "NEXT HOLD" << nextLeg << resultHold;

        // qDebug() << "HOLD DIFFER";
        // Hold point differs from next leg start - use the helping line
        if(resultHold.status == atools::geo::ALONG_TRACK && // Check if we are outside of the hold
           resultHold.distance < nmToMeter(at(activeLeg).getApproachLeg().turnDirection == "R" ? -0.5f : 0.5f))
          switchToNextLeg = true;
      }
    }
    else
    {
      if(at(nextLeg).isHold())
      {
        // Ignore all other rulues and use distance to hold point to activate hold
        if(std::abs(nextLegResult.distance) < nmToMeter(0.5f))
          switchToNextLeg = true;
      }
      else if(at(activeLeg).getApproachLegType() == maptypes::PROCEDURE_TURN)
      {
        // Ignore the after end indication for current leg for procedure turns since turn can happen earlier
        if(isSmaller(nextLegResult, activeLegResult, 100.f /* meter */) && courseDiff < 45.f)
          switchToNextLeg = true;
      }
      else
      {
        // Check if we can advance to the next leg - if at end of current and distance to next is smaller and course similar
        if(activeLegResult.status == atools::geo::AFTER_END ||
           (isSmaller(nextLegResult, activeLegResult, 10.f /* meter */) && courseDiff < 90.f))
          switchToNextLeg = true;
      }
    }

    if(switchToNextLeg)
    {
      // qDebug() << "Switching to next leg";
      // Either left current leg or closer to next and on courses
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes & maptypes::APPROACH_MISSED) && at(nextLeg).isMissed()))
      {
        // Go to next leg and increase all values
        activeLeg = nextLeg;
        pos.pos.distanceMeterToLine(getPositionAt(activeLeg - 1), getPositionAt(activeLeg), activeLegResult);
      }
    }
  }
  // qDebug() << "active" << activeLeg << "size" << size() << "ident" << at(activeLeg).getIdent() <<
  // maptypes::approachLegTypeFullStr(at(activeLeg).getApproachLeg().type);
}

bool RouteMapObjectList::getRouteDistances(float *distFromStart, float *distToDest,
                                           float *nextLegDistance, float *crossTrackDistance) const
{
  const maptypes::MapApproachLeg *geometryLeg = nullptr;

  if(activeLeg == maptypes::INVALID_INDEX_VALUE)
    return false;

  if(at(activeLeg).isAnyApproach() && (at(activeLeg).getGeometry().size() > 2))
    geometryLeg = &at(activeLeg).getApproachLeg();

  if(crossTrackDistance != nullptr)
  {
    if(geometryLeg != nullptr)
    {
      atools::geo::LineDistance lineDist;
      geometryLeg->geometry.distanceMeterToLineString(activePos.pos, lineDist);
      if(lineDist.status == atools::geo::ALONG_TRACK)
        *crossTrackDistance = meterToNm(lineDist.distance);
      else
        *crossTrackDistance = maptypes::INVALID_DISTANCE_VALUE;
    }
    else if(activeLegResult.status == atools::geo::ALONG_TRACK)
      *crossTrackDistance = meterToNm(activeLegResult.distance);
    else
      *crossTrackDistance = maptypes::INVALID_DISTANCE_VALUE;
  }

  int routeIndex = activeLeg;
  if(routeIndex != maptypes::INVALID_INDEX_VALUE)
  {
    if(routeIndex >= size())
      routeIndex = size() - 1;

    float distToCur = 0.f;

    bool activeIsMissed = at(activeLeg).isMissed();

    // Ignore missed approach legs until the active is a missedd approach leg
    if(!at(routeIndex).isMissed() || activeIsMissed)
    {
      if(geometryLeg != nullptr)
      {
        atools::geo::LineDistance result;
        geometryLeg->geometry.distanceMeterToLineString(activePos.pos, result);
        distToCur = meterToNm(result.distanceFrom2);
      }
      else
        distToCur = meterToNm(getPositionAt(routeIndex).distanceMeterTo(activePos.pos));
    }

    if(nextLegDistance != nullptr)
      *nextLegDistance = distToCur;

    // Sum up all distances along the legs
    // Ignore missed approach legs until the active is a missedd approach leg
    if(distFromStart != nullptr)
    {
      *distFromStart = 0.f;
      for(int i = 0; i <= routeIndex; i++)
      {
        if(!at(i).isMissed() || activeIsMissed)
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
        if(!at(i).isMissed() || activeIsMissed)
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

  // Find the leg that contains the given distance point
  float total = 0.f;
  int foundIndex = maptypes::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
  for(int i = 0; i < size() - 1; i++)
  {
    total += at(i + 1).getDistanceTo();
    if(total > distFromStartNm)
    {
      // Distance already within leg
      foundIndex = i;
      break;
    }
  }

  if(foundIndex < size() - 1)
  {
    if(!at(foundIndex).isAnyApproach())
    {
      float base = distFromStartNm - (total - at(foundIndex + 1).getDistanceTo());
      float fraction = base / at(foundIndex + 1).getDistanceTo();

      // qDebug() << "idx" << foundIndex << "total" << total << "rem"
      // << at(foundIndex + 1).getDistanceTo() << "distFromStartNm" << distFromStartNm << "base"
      // << base << "frac" << fraction;

      retval = getPositionAt(foundIndex).interpolate(getPositionAt(foundIndex + 1), fraction);
    }
    else
    {
      // Skip all points like initial fixes or any other intercepted/collapsed legs
      foundIndex++;
      while(at(foundIndex).isApproachPoint() && foundIndex < size())
        foundIndex++;

      float base = distFromStartNm - (total - at(foundIndex).getApproachLeg().calculatedDistance);
      float fraction = base / at(foundIndex).getApproachLeg().calculatedDistance;

      // qDebug() << "idx appr" << foundIndex << "total" << total << "rem"
      // << at(foundIndex).getApproachLeg().calculatedDistance << "distFromStartNm" << distFromStartNm << "base"
      // << base << "frac" << fraction;
      // qDebug() << "foundIndex" << at(foundIndex).getApproachLeg();
      // qDebug() << "foundIndex + 1" << at(foundIndex + 1).getApproachLeg();
      // qDebug() << "foundIndex + 2" << at(foundIndex + 2).getApproachLeg();

      retval = at(foundIndex).getGeometry().interpolate(fraction);
    }
  }

  return retval;
}

void RouteMapObjectList::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                                    maptypes::MapSearchResult& mapobjects) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < getApproachStartIndex(); i++)
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
  Pos initialPos;

  if(!approachLegs.isEmpty())
  {
    const maptypes::MapApproachLeg& initialFix = approachLegs.first();

    if(initialFix.type == maptypes::COURSE_TO_FIX)
      // The only legs that starts before the fix
      initialPos = initialFix.line.getPos1();
    else if(initialFix.navaids.hasWaypoints())
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

  if(initialFixRef.id != -1)
  {
    // Check if the route contains the initial fix and return the index to it
    for(int i = 0; i < size(); i++)
    {
      const RouteMapObject& obj = at(i);
      if(obj.getId() == initialFixRef.id && obj.getMapObjectType() == initialFixRef.type)
      {
        approachStartIndex = i + 1;
        break;
      }
    }
  }
  else if(initialPos.isValid())
  {
    // Check if the route contains a user waypoint overlapping with the intial fix and return the index to it
    for(int i = 0; i < size(); i++)
    {
      if(initialPos.almostEqual(at(i).getPosition(), atools::geo::Pos::POS_EPSILON_10M))
      {
        approachStartIndex = i + 1;
        break;
      }
    }
  }

  if(approachStartIndex >= size())
  {
    // No approach legs - erase all in the current object
    QList<RouteMapObject>::iterator it = std::remove_if(begin(), end(),
                                                        [] (const RouteMapObject &p)->bool
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

int RouteMapObjectList::activeRouteLegInternal(int leg) const
{
  if(leg < maptypes::INVALID_INDEX_VALUE && size() > 1)
    // Last leg before approach is part of route and approach
    return leg <= getApproachStartIndex() ? leg : maptypes::INVALID_INDEX_VALUE;
  else
    return maptypes::INVALID_INDEX_VALUE;
}

int RouteMapObjectList::activeApproachLegInternal(int leg) const
{
  if(leg < maptypes::INVALID_INDEX_VALUE && size() > 1)
    return at(leg).isAnyApproach() ? leg - (size() - approachLegs.size()) : maptypes::INVALID_INDEX_VALUE;
  else
    return maptypes::INVALID_INDEX_VALUE;
}

int RouteMapObjectList::getActiveRouteLegIndex() const
{
  return activeRouteLegInternal(activeLeg);
}

int RouteMapObjectList::getActiveApproachLegIndex() const
{
  return activeApproachLegInternal(activeLeg);
}

const RouteMapObject *RouteMapObjectList::getActiveLeg() const
{
  if(activeLeg != maptypes::INVALID_INDEX_VALUE)
    return &at(activeLeg);
  else
    return nullptr;
}

const RouteMapObject *RouteMapObjectList::getActiveLegCorrected(bool *corrected) const
{
  int idx = getActiveLegIndexCorrected(corrected);

  if(idx != maptypes::INVALID_INDEX_VALUE)
    return &at(idx);
  else
    return nullptr;
}

bool RouteMapObjectList::isActiveMissed() const
{
  const RouteMapObject *leg = getActiveLeg();
  if(leg != nullptr)
    return leg->isMissed();
  else
    return false;
}

int RouteMapObjectList::getActiveRouteLegIndexCorrected() const
{
  return activeRouteLegInternal(getActiveLegIndexCorrected());
}

int RouteMapObjectList::getActiveApproachLegIndexCorrected() const
{
  return activeApproachLegInternal(getActiveLegIndexCorrected());
}

int RouteMapObjectList::getActiveLegIndexCorrected(bool *corrected) const
{
  if(activeLeg == maptypes::INVALID_INDEX_VALUE)
    return maptypes::INVALID_INDEX_VALUE;

  int nextLeg = activeLeg + 1;
  if(nextLeg < size() && nextLeg == getApproachStartIndex() &&
     at(nextLeg).isAnyApproach() /* && (at(nextLeg).getApproachLeg().type == maptypes::INITIAL_FIX ||
                                  *    at(nextLeg).isHold())*/)
  {
    if(corrected != nullptr)
      *corrected = true;
    return activeLeg + 1;
  }
  else
  {
    if(corrected != nullptr)
      *corrected = false;
    return activeLeg;
  }
}

bool RouteMapObjectList::isPassedLastLeg() const
{
  if((activeLeg >= size() - 1 || (activeLeg + 1 < size() && at(activeLeg + 1).isMissed())) &&
     activeLegResult.status == atools::geo::AFTER_END)
    return true;
  else
    return false;
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
    pos.pos.distanceMeterToLine(getPositionAt(i - 1), getPositionAt(i), result);
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

int RouteMapObjectList::getNearestLegResult(const atools::geo::Pos& pos,
                                            atools::geo::LineDistance& lineDistanceResult) const
{
  int index;
  nearestLegResult(pos, lineDistanceResult, index);
  return index;
}

void RouteMapObjectList::nearestLegResult(const atools::geo::Pos& pos,
                                          atools::geo::LineDistance& lineDistanceResult, int& index) const
{
  index = maptypes::INVALID_INDEX_VALUE;
  lineDistanceResult.status = atools::geo::INVALID;
  lineDistanceResult.distance = maptypes::INVALID_DISTANCE_VALUE;

  if(!pos.isValid())
    return;

  // Check only until the approach starts if required
  atools::geo::LineDistance result, minResult;
  minResult.status = atools::geo::INVALID;
  minResult.distance = maptypes::INVALID_DISTANCE_VALUE;

  for(int i = 1; i < size(); i++)
  {
    pos.distanceMeterToLine(getPositionAt(i - 1), getPositionAt(i), result);

    if(result.status != atools::geo::INVALID && std::abs(result.distance) < std::abs(minResult.distance))
    {
      minResult = result;
      index = i;
    }
  }

  if(index != maptypes::INVALID_INDEX_VALUE)
    lineDistanceResult = minResult;
}
