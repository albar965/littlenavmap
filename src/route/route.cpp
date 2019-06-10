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

#include "route/route.h"

#include "geo/calculations.h"
#include "common/maptools.h"
#include "query/mapquery.h"
#include "common/unit.h"
#include "common/constants.h"
#include "route/flightplanentrybuilder.h"
#include "query/procedurequery.h"
#include "query/airportquery.h"
#include "weather/windreporter.h"
#include "navapp.h"
#include "fs/util/fsutil.h"
#include "route/routealtitude.h"
#include "perf/aircraftperfcontroller.h"
#include "fs/perf/aircraftperf.h"
#include "settings/settings.h"

#include <QRegularExpression>

#include <marble/GeoDataLineString.h>

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::LineString;
using atools::geo::nmToMeter;
using atools::geo::normalizeCourse;
using atools::geo::meterToNm;
using atools::geo::manhattanDistance;
using atools::fs::pln::FlightplanEntry;
using atools::fs::pln::Flightplan;

// Disable leg activation if distance is larger
const float MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM = 50.f;

Route::Route()
{
  resetActive();
  altitude = new RouteAltitude(this);
}

Route::Route(const Route& other)
  : QList<RouteLeg>(other)
{
  copy(other);
}

Route::~Route()
{
  delete altitude;
}

Route& Route::operator=(const Route& other)
{
  copy(other);

  return *this;
}

void Route::resetActive()
{
  activeLegResult.distanceFrom1 =
    activeLegResult.distanceFrom2 =
      activeLegResult.distance =
        map::INVALID_DISTANCE_VALUE;
  activeLegResult.status = atools::geo::INVALID;
  activePos = map::PosCourse();
  activeLegIndex = map::INVALID_INDEX_VALUE;
}

void Route::copy(const Route& other)
{
  clear();
  QList::append(other);

  totalDistance = other.totalDistance;
  flightplan = other.flightplan;
  shownTypes = other.shownTypes;
  boundingRect = other.boundingRect;
  activePos = other.activePos;

  arrivalLegs = other.arrivalLegs;
  starLegs = other.starLegs;
  departureLegs = other.departureLegs;

  departureLegsOffset = other.departureLegsOffset;
  starLegsOffset = other.starLegsOffset;
  arrivalLegsOffset = other.arrivalLegsOffset;

  alternateLegsOffset = other.alternateLegsOffset;
  numAlternateLegs = other.numAlternateLegs;

  activeLegIndex = other.activeLegIndex;
  activeLegResult = other.activeLegResult;

  // Update flightplan pointers to this instance
  for(RouteLeg& routeLeg : *this)
    routeLeg.setFlightplan(&flightplan);

  altitude = new RouteAltitude(this);
  *altitude = *other.altitude;
}

void Route::clearAll()
{
  removeProcedureLegs();
  removeAlternateLegs();
  getFlightplan().clear();
  getFlightplan().getProperties().clear();
  resetActive();
  clear();
  setTotalDistance(0.f);
}

int Route::getNextUserWaypointNumber() const
{
  /* Get number from user waypoint from user defined waypoint in fs flight plan */
  static const QRegularExpression USER_WP_ID("^WP([0-9]+)$");

  int nextNum = 0;

  for(const FlightplanEntry& entry : flightplan.getEntries())
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::USER)
      nextNum = std::max(QString(USER_WP_ID.match(entry.getWaypointId()).captured(1)).toInt(), nextNum);
  }
  return nextNum + 1;
}

bool Route::canEditLeg(int index) const
{
  // Do not allow any edits between the procedures

  if(hasAnyDepartureProcedure() && index < departureLegsOffset + departureLegs.size())
    return false;

  if(hasAnyStarProcedure() && index > starLegsOffset)
    return false;

  if(hasAnyArrivalProcedure() && index > arrivalLegsOffset)
    return false;

  if(hasAlternates() && index >= alternateLegsOffset)
    return false;

  return true;
}

bool Route::canEditPoint(int index) const
{
  return at(index).isRoute() || at(index).isAlternate();
}

void Route::getSidStarNames(QString& sid, QString& sidTrans, QString& star, QString& starTrans) const
{
  sid = departureLegs.approachFixIdent;
  sidTrans = departureLegs.transitionFixIdent;
  star = starLegs.approachFixIdent;
  starTrans = starLegs.transitionFixIdent;
}

void Route::getRunwayNames(QString& departure, QString& arrival) const
{
  departure = departureLegs.runwayEnd.name;

  if(hasAnyArrivalProcedure())
    arrival = arrivalLegs.runwayEnd.name;
  else if(hasAnyStarProcedure())
    arrival = starLegs.runwayEnd.name;
  else
    arrival.clear();
}

const QString& Route::getStarRunwayName() const
{
  return starLegs.runwayEnd.name;
}

const QString& Route::getApproachRunwayName() const
{
  return arrivalLegs.runwayEnd.name;
}

void Route::getArrivalNames(QString& arrivalArincName, QString& arrivalTransition) const
{
  if(hasAnyArrivalProcedure())
  {
    arrivalArincName = arrivalLegs.approachArincName;
    arrivalTransition = arrivalLegs.transitionFixIdent;
  }
  else
  {
    arrivalArincName.clear();
    arrivalTransition.clear();
  }
}

void Route::updateActiveLegAndPos(bool force)
{
  if(force)
  {
    activeLegResult.distanceFrom1 =
      activeLegResult.distanceFrom2 =
        activeLegResult.distance =
          map::INVALID_DISTANCE_VALUE;
    activeLegResult.status = atools::geo::INVALID;
    activeLegIndex = map::INVALID_INDEX_VALUE;
  }
  updateActiveLegAndPos(activePos);
}

/* Compare crosstrack distance fuzzy */
bool Route::isSmaller(const atools::geo::LineDistance& dist1, const atools::geo::LineDistance& dist2,
                      float epsilon)
{
  return std::abs(dist1.distance) < std::abs(dist2.distance) + epsilon;
}

void Route::updateActiveLegAndPos(const map::PosCourse& pos)
{
  if(isEmpty() || !pos.isValid())
  {
    resetActive();
    return;
  }

  // Nothing active so far - start with nearest leg ============================
  if(activeLegIndex == map::INVALID_INDEX_VALUE)
  {
    float crossDummy;
    nearestAllLegIndex(pos, crossDummy, activeLegIndex);
  }

  if(activeLegIndex >= size())
    activeLegIndex = size() - 1;

  // Update current position =================================
  activePos = pos;

  // Update activeLegResult with distance, cross track and more ===================================
  if(getSizeWithoutAlternates() == 1)
  {
    // Special case point route - remain on first and only leg ==========================
    activeLegIndex = 0;
    // Test if still nearby
    activePos.pos.distanceMeterToLine(
      first().getPosition(), first().getPosition(), activeLegResult);
  }
  else
  {
    if(activeLegIndex == 0)
      // Reset from point route ====================================
      activeLegIndex = 1;
    activePos.pos.distanceMeterToLine(
      getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex), activeLegResult);
  }

  if(getDistanceToFlightPlan() > MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM)
  {
    // Too far away from plan - remove active leg
    activeLegIndex = map::INVALID_INDEX_VALUE;
    return;
  }

#ifdef DEBUG_ACTIVE_LEG
  qDebug() << "activePos" << activeLegResult;
#endif

  if(activeLegIndex != map::INVALID_INDEX_VALUE && at(activeLegIndex).isAlternate())
    return;

  // =========================================================================
  // Get potential next leg and course difference
  int nextLeg = activeLegIndex + 1;
  float courseDiff = 0.f;
  if(nextLeg < size())
  {
    // Catch the case of initial fixes or others that are points instead of lines and try the next legs
    if(!at(activeLegIndex).getProcedureLeg().isHold())
    {
      while(atools::contains(at(nextLeg).getProcedureLegType(),
                             {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) &&
            getPrevPositionAt(nextLeg) == getPositionAt(nextLeg) &&
            nextLeg < size() - 2)
        nextLeg++;
    }
    else
    {
      // Jump all initial fixes for holds since the next line can probably not overlap
      while(atools::contains(at(nextLeg).getProcedureLegType(),
                             {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) && nextLeg < size() - 2)
        nextLeg++;
    }
    Pos pos1 = getPrevPositionAt(nextLeg);
    Pos pos2 = getPositionAt(nextLeg);

    // Calculate course difference
    float legCrs = normalizeCourse(pos1.angleDegTo(pos2));
    courseDiff = atools::mod(pos.course - legCrs + 360.f, 360.f);
    if(courseDiff > 180.f)
      courseDiff = 360.f - courseDiff;

#ifdef DEBUG_ACTIVE_LEG
    qDebug() << "ACTIVE" << at(activeLegIndex);
    qDebug() << "NEXT" << at(nextLeg);
#endif

    // Test next leg
    atools::geo::LineDistance nextLegResult;
    activePos.pos.distanceMeterToLine(pos1, pos2, nextLegResult);
#ifdef DEBUG_ACTIVE_LEG
    qDebug() << "NEXT" << nextLeg << nextLegResult;
#endif

    bool switchToNextLeg = false;
    if(at(activeLegIndex).getProcedureLeg().isHold())
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << "ACTIVE HOLD";
#endif
      // Test next leg if we can exit a hold
      if(at(nextLeg).getProcedureLeg().line.getPos1() == at(activeLegIndex).getPosition())
      {
#ifdef DEBUG_ACTIVE_LEG
        qDebug() << "HOLD SAME";
#endif

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
        at(activeLegIndex).getProcedureLeg().holdLine.distanceMeterToLine(activePos.pos, resultHold);
#ifdef DEBUG_ACTIVE_LEG
        qDebug() << "NEXT HOLD" << nextLeg << resultHold;
        qDebug() << "HOLD DIFFER";
        qDebug() << "resultHold" << resultHold;
        qDebug() << "holdLine" << at(activeLegIndex).getProcedureLeg().holdLine;
#endif
        // Hold point differs from next leg start - use the helping line
        if(resultHold.status == atools::geo::ALONG_TRACK) // Check if we are outside of the hold
        {
          if(at(activeLegIndex).getProcedureLeg().turnDirection == "R")
            switchToNextLeg = resultHold.distance < nmToMeter(-0.5f);
          else
            switchToNextLeg = resultHold.distance > nmToMeter(0.5f);
        }
      }
    }
    else
    {
      if(at(nextLeg).getProcedureLeg().isHold())
      {
        // Ignore all other rulues and use distance to hold point to activate hold
        if(std::abs(nextLegResult.distance) < nmToMeter(0.5f))
          switchToNextLeg = true;
      }
      else if(at(activeLegIndex).getProcedureLegType() == proc::PROCEDURE_TURN)
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

#ifdef DEBUG_ACTIVE_LEG
    qDebug() << "Next leg" << at(nextLeg);
#endif

    // Check if next leg should be activated =================================================
    // if(isAirportAfterArrival(nextLeg) && !hasAlternates())
    if(isAirportAfterArrival(nextLeg) || at(nextLeg).isAlternate())
      // Do not sequence after arrival or after end of missed
      switchToNextLeg = false;

    if(switchToNextLeg)
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << "Switching to next leg";
#endif

      // Either left current leg or closer to next and on courses
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes& map::MISSED_APPROACH) && at(nextLeg).getProcedureLeg().isMissed()))
      {
        // Go to next leg and increase all values
        activeLegIndex = nextLeg;
        pos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex), activeLegResult);
      }
    }
  }
#ifdef DEBUG_ACTIVE_LEG
  qDebug() << "active" << activeLegIndex << "size" << size()
           << "ident" << at(activeLegIndex).getIdent()
           << "alternate" << at(activeLegIndex).isAlternate()
           << proc::procedureLegTypeStr(at(activeLegIndex).getProcedureLeg().type);
#endif
}

bool Route::getRouteDistances(float *distFromStart, float *distToDest,
                              float *nextLegDistance, float *crossTrackDistance, float *projectionDistance) const
{
  const proc::MapProcedureLeg *geometryLeg = nullptr;

  if(activeLegIndex == map::INVALID_INDEX_VALUE)
    return false;

  int active = adjustedActiveLeg();
  const RouteLeg& activeLeg = at(active);

  if(activeLeg.isAnyProcedure() && (activeLeg.getGeometry().size() > 2))
    // Use arc or intercept geometry to calculate distance
    geometryLeg = &activeLeg.getProcedureLeg();

#ifdef DEBUG_INFORMATION_ROUTE_LEG
  qDebug() << Q_FUNC_INFO << activeLegResult;
#endif

  if(crossTrackDistance != nullptr)
  {
    if(geometryLeg != nullptr)
    {
      atools::geo::LineDistance lineDist;
      geometryLeg->geometry.distanceMeterToLineString(activePos.pos, lineDist);
      if(lineDist.status == atools::geo::ALONG_TRACK)
        *crossTrackDistance = meterToNm(lineDist.distance);
      else
        *crossTrackDistance = map::INVALID_DISTANCE_VALUE;
    }
    else if(activeLegResult.status == atools::geo::ALONG_TRACK)
      *crossTrackDistance = meterToNm(activeLegResult.distance);
    else
      *crossTrackDistance = map::INVALID_DISTANCE_VALUE;
  }

  int routeIndex = active;
  if(routeIndex != map::INVALID_INDEX_VALUE)
  {
    if(routeIndex >= size())
      routeIndex = size() - 1;

    float distToCurrent = 0.f;

    bool activeIsMissed = activeLeg.getProcedureLeg().isMissed();
    bool activeIsAlternate = activeLeg.isAlternate();

    // Ignore missed approach legs until the active is a missed approach leg - same for alternate
    if(!at(routeIndex).getProcedureLeg().isMissed() || activeIsMissed ||
       !at(routeIndex).isAlternate() || activeIsAlternate)
    {
      atools::geo::LineDistance result;
      if(geometryLeg != nullptr)
      {
        geometryLeg->geometry.distanceMeterToLineString(activePos.pos, result);
        distToCurrent = meterToNm(result.distanceFrom2);
      }
      else
        distToCurrent = meterToNm(getPositionAt(routeIndex).distanceMeterTo(activePos.pos));
    }

#ifdef DEBUG_INFORMATION_ROUTE_LEG
    qDebug() << Q_FUNC_INFO << distToCurrent;
#endif

    if(nextLegDistance != nullptr)
      *nextLegDistance = distToCurrent;

    // Sum up all distances along the legs
    // Ignore missed approach legs until the active is a missedd approach leg
    float fromStart = 0.f;
    for(int i = 0; i <= routeIndex; i++)
    {
      if(!at(i).getProcedureLeg().isMissed() || activeIsMissed ||
         !at(i).isAlternate() || activeIsAlternate)
        fromStart += at(i).getDistanceTo();
      else
        break;
    }
    float fromStartLegs = fromStart;
    fromStart -= distToCurrent;
    fromStart = std::abs(fromStart);

    if(distFromStart != nullptr)
      *distFromStart = std::max(fromStart, 0.f);

    if(projectionDistance != nullptr)
      *projectionDistance = projectedDistance(activeLegResult, fromStartLegs, activeLegIndex);

    if(distToDest != nullptr)
    {
      if(getSizeWithoutAlternates() == 1)
        *distToDest = meterToNm(activePos.pos.distanceMeterTo(first().getPosition()));
      else
      {
        if(activeIsMissed)
        {
          // Summarize remaining missed leg distance if on missed
          *distToDest = 0.f;
          for(int i = routeIndex + 1; i < size(); i++)
          {
            if(at(i).getProcedureLeg().isMissed())
              *distToDest += at(i).getDistanceTo();
          }
          *distToDest += distToCurrent;
          *distToDest = std::abs(*distToDest);
        }
        else if(activeIsAlternate)
        {
          // Summarize remaining leg distance to alternate if on alternate
          *distToDest = 0.f;
          for(int i = routeIndex + 1; i < size(); i++)
          {
            if(at(i).isAlternate())
              *distToDest += at(i).getDistanceTo();
          }
          *distToDest += distToCurrent;
          *distToDest = std::abs(*distToDest);
        }
        else
          *distToDest = std::max(totalDistance - fromStart, 0.f);
      }
    }

    return true;
  }
  return false;
}

float Route::getProjectionDistance() const
{
  float distance = 0.f;
  if(getRouteDistances(nullptr, nullptr, nullptr, nullptr, &distance))
    return distance;
  else
    return map::INVALID_DISTANCE_VALUE;
}

float Route::getDistanceFromStart(const atools::geo::Pos& pos) const
{
  atools::geo::LineDistance result;
  int legIndex = getNearestRouteLegResult(pos, result, false /* ignoreNotEditable */);

#ifdef DEBUG_INFORMATION_ROUTE
  qDebug() << Q_FUNC_INFO << "leg" << leg << "result" << result;
#endif

  if(legIndex < map::INVALID_INDEX_VALUE)
  {
    if(result.status != atools::geo::INVALID)
    {
      float fromstart = 0.f;
      for(int i = 1; i <= legIndex; i++)
        fromstart += at(i).getDistanceTo();

      return projectedDistance(result, fromstart, legIndex);
    }
  }
  return map::INVALID_DISTANCE_VALUE;
}

float Route::projectedDistance(const atools::geo::LineDistance& result, float legFromStart, int legIndex) const
{
  float projectionDistance = map::INVALID_DISTANCE_VALUE;
  // Use along track distance only for projection to avoid aircraft travelling backwards
  if(result.status == atools::geo::ALONG_TRACK || // Middle of flight plan
     (result.status == atools::geo::BEFORE_START && legIndex == 1)) // Before first leg
    projectionDistance = legFromStart - meterToNm(result.distanceFrom2);
  else if(result.status == atools::geo::AFTER_END && legIndex >= getDestinationLegIndex())
    // After destination leg
    projectionDistance = legFromStart + meterToNm(std::abs(result.distanceFrom2));
  else
    // Middle of flight plan between legs
    projectionDistance = legFromStart - at(legIndex).getDistanceTo();

  if(at(legIndex).isAnyProcedure() && at(legIndex).getProcedureLeg().isMissed())
    // Hide aircraft if missed is active
    projectionDistance = map::INVALID_DISTANCE_VALUE;

  return projectionDistance;
}

float Route::getTopOfDescentDistance() const
{
  return altitude->getTopOfDescentDistance();
}

int Route::getTopOfDescentLegIndex() const
{
  return altitude->getTopOfDescentLegIndex();
}

float Route::getCruisingAltitudeFeet() const
{
  return Unit::rev(getFlightplan().getCruisingAltitude(), Unit::altFeetF);
}

float Route::getAltitudeForDistance(float currentDistToDest) const
{
  return altitude->getAltitudeForDistance(currentDistToDest);
}

float Route::getTopOfDescentFromDestination() const
{
  return altitude->getTopOfDescentFromDestination();
}

float Route::getTopOfClimbDistance() const
{
  return altitude->getTopOfClimbDistance();
}

int Route::getTopOfClimbLegIndex() const
{
  return altitude->getTopOfClimbLegIndex();
}

atools::geo::Pos Route::getTopOfDescentPos() const
{
  return altitude->getTopOfDescentPos();
}

atools::geo::Pos Route::getTopOfClimbPos() const
{
  return altitude->getTopOfClimbPos();
}

atools::geo::Pos Route::getPositionAtDistance(float distFromStartNm) const
{
  if(distFromStartNm < 0.f || distFromStartNm > totalDistance)
    return atools::geo::EMPTY_POS;

  atools::geo::Pos retval;

  // Find the leg that contains the given distance point
  float total = 0.f;
  int foundIndex = map::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
  for(int i = 0; i <= getDestinationAirportLegIndex(); i++)
  {
    total += at(i + 1).getDistanceTo();
    if(total >= distFromStartNm)
    {
      // Distance already within leg
      foundIndex = i;
      break;
    }
  }

  if(foundIndex < size() - 1)
  {
    foundIndex++;
    if(at(foundIndex).getGeometry().size() > 2)
    {
      // Use approach geometry to display
      float base = distFromStartNm - (total - at(foundIndex).getProcedureLeg().calculatedDistance);
      float fraction = base / at(foundIndex).getProcedureLeg().calculatedDistance;
      retval = at(foundIndex).getGeometry().interpolate(fraction);
    }
    else
    {
      float base = distFromStartNm - (total - at(foundIndex).getDistanceTo());
      float fraction = base / at(foundIndex).getDistanceTo();
      retval = getPrevPositionAt(foundIndex).interpolate(getPositionAt(foundIndex), fraction);
    }
  }

  if(!retval.isValid())
    qWarning() << Q_FUNC_INFO << "position not valid";
  return retval;
}

const RouteAltitudeLeg& Route::getAltitudeLegAt(int i) const
{
  return altitude->at(i);
}

bool Route::hasAltitudeLegs() const
{
  return !altitude->isEmpty();
}

int Route::getNumAltitudeLegs() const
{
  return altitude->size();
}

bool Route::hasValidProfile() const
{
  return altitude->isValidProfile();
}

int Route::getDestinationLegIndex() const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  if(hasAnyArrivalProcedure())
  {
    // Last leg before missed approach is destination runway
    for(int i = arrivalLegsOffset; i <= getDestinationAirportLegIndex(); i++)
    {
      if(at(i).isAnyProcedure() && at(i).getProcedureType() & proc::PROCEDURE_MISSED)
        return i - 1;
    }
  }
  // Simple destination airport without approach
  return getDestinationAirportLegIndex();
}

const RouteLeg& Route::getDestinationLeg() const
{
  int idx = getDestinationLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? at(idx) : RouteLeg::EMPTY_ROUTELEG;
}

int Route::getDestinationAirportLegIndex() const
{
  if(getSizeWithoutAlternates() == 0)
    return map::INVALID_INDEX_VALUE;

  // Either airport after missed and before the alternates or last one
  return alternateLegsOffset != map::INVALID_INDEX_VALUE ? alternateLegsOffset - 1 : size() - 1;
}

const RouteLeg& Route::getDestinationAirportLeg() const
{
  int idx = getDestinationAirportLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? at(idx) : RouteLeg::EMPTY_ROUTELEG;
}

int Route::getDepartureLegIndex() const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  if(hasAnyDepartureProcedure())
    return 1;

  return 0;
}

const RouteLeg& Route::getDepartureLeg() const
{
  int idx = getDepartureLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? at(idx) : RouteLeg::EMPTY_ROUTELEG;
}

int Route::getDepartureAirportLegIndex() const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  return 0;
}

const RouteLeg& Route::getDepartureAirportLeg() const
{
  int idx = getDepartureAirportLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? at(idx) : RouteLeg::EMPTY_ROUTELEG;
}

void Route::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                       map::MapSearchResult& mapobjects, QList<proc::MapProcedurePoint> *procPoints,
                       bool includeProcedure) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < size(); i++)
  {
    const RouteLeg& leg = at(i);
    if(!includeProcedure && leg.isAnyProcedure())
      // Do not edit procedures
      continue;

    if(conv.wToS(leg.getPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)
    {
      if(leg.getVor().isValid())
      {
        map::MapVor vor = leg.getVor();
        vor.routeIndex = i;
        insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, vor);
      }

      if(leg.getWaypoint().isValid())
      {
        map::MapWaypoint wp = leg.getWaypoint();
        wp.routeIndex = i;
        insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, wp);
      }

      if(leg.getNdb().isValid())
      {
        map::MapNdb ndb = leg.getNdb();
        ndb.routeIndex = i;
        insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, ndb);
      }

      if(leg.getAirport().isValid())
      {
        map::MapAirport ap = leg.getAirport();
        ap.routeIndex = i;
        insertSortedByDistance(conv, mapobjects.airports, &mapobjects.airportIds, xs, ys, ap);
      }

      if(leg.getMapObjectType() == map::INVALID)
      {
        map::MapUserpointRoute up;
        up.routeIndex = i;
        up.name = leg.getIdent() + " (not found)";
        up.position = leg.getPosition();
        up.magvar = NavApp::getMagVar(leg.getPosition());
        mapobjects.userPointsRoute.append(up);
      }

      if(leg.getMapObjectType() == map::USERPOINTROUTE)
      {
        map::MapUserpointRoute up;
        up.id = i;
        up.routeIndex = i;
        up.name = leg.getIdent();
        up.position = leg.getPosition();
        up.magvar = NavApp::getMagVar(leg.getPosition());
        mapobjects.userPointsRoute.append(up);
      }

      if(procPoints != nullptr && leg.isAnyProcedure())
        procPoints->append(proc::MapProcedurePoint(leg.getProcedureLeg(), false /* preview */));
    }
  }
}

bool Route::hasDepartureParking() const
{
  if(hasValidDeparture())
    return first().getDepartureParking().isValid();

  return false;
}

bool Route::hasDepartureHelipad() const
{
  if(hasDepartureStart())
    return first().getDepartureStart().helipadNumber > 0;

  return false;
}

bool Route::hasDepartureStart() const
{
  if(hasValidDeparture())
    return first().getDepartureStart().isValid();

  return false;
}

bool Route::isFlightplanEmpty() const
{
  return getFlightplan().isEmpty();
}

bool Route::hasAirways() const
{
  for(const RouteLeg& leg : *this)
  {
    if(!leg.getAirwayName().isEmpty())
      return true;
  }
  return false;
}

bool Route::hasUserWaypoints() const
{
  for(const RouteLeg& leg : *this)
  {
    if(leg.getMapObjectType() == map::USERPOINTROUTE)
      return true;
  }
  return false;
}

bool Route::hasValidDeparture() const
{
  return !getFlightplan().isEmpty() &&
         getFlightplan().getEntries().first().getWaypointType() == atools::fs::pln::entry::AIRPORT &&
         first().isValid();
}

bool Route::hasValidDestination() const
{
  int destIdx = getDestinationAirportLegIndex();
  return destIdx != map::INVALID_INDEX_VALUE &&
         flightplan.getEntries().at(destIdx).getWaypointType() == atools::fs::pln::entry::AIRPORT;
}

bool Route::hasEntries() const
{
  return getFlightplan().getEntries().size() > 2;
}

bool Route::canCalcRoute() const
{
  return getSizeWithoutAlternates() >= 2;
}

void Route::removeAlternateLegs()
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = size() - 1; i >= 0; i--)
  {
    const RouteLeg& routeLeg = at(i);
    if(routeLeg.isAlternate())
      indexes.append(i);
  }

  // Delete in route legs and flight plan from the end
  for(int i = 0; i < indexes.size(); i++)
  {
    removeAt(indexes.at(i));
    flightplan.getEntries().removeAt(indexes.at(i));
  }
  alternateLegsOffset = map::INVALID_INDEX_VALUE;
  numAlternateLegs = 0;
}

void Route::clearProcedures(proc::MapProcedureTypes type)
{
  // Clear procedure legs
  if(type & proc::PROCEDURE_SID)
    departureLegs.clearApproach();
  if(type & proc::PROCEDURE_SID_TRANSITION)
    departureLegs.clearTransition();

  if(type & proc::PROCEDURE_STAR_TRANSITION)
    starLegs.clearTransition();
  if(type & proc::PROCEDURE_STAR)
    starLegs.clearApproach();

  if(type & proc::PROCEDURE_TRANSITION)
    arrivalLegs.clearTransition();
  if(type & proc::PROCEDURE_APPROACH)
    arrivalLegs.clearApproach();
}

void Route::removeProcedureLegs()
{
  removeProcedureLegs(proc::PROCEDURE_ALL);
}

void Route::removeProcedureLegs(proc::MapProcedureTypes type)
{
  clearProcedures(type);
  clearProcedureLegs(type);

  // Remove properties from flight plan
  clearFlightplanProcedureProperties(type);

  updateAll();
  updateLegAltitudes();
}

void Route::clearFlightplanProcedureProperties(proc::MapProcedureTypes type)
{
  ProcedureQuery::clearFlightplanProcedureProperties(flightplan.getProperties(), type);
}

void Route::clearFlightplanAlternateProperties()
{
  flightplan.getProperties().remove(atools::fs::pln::ALTERNATES);
}

void Route::updateAlternateProperties()
{
  int offset = getAlternateLegsOffset();
  if(offset != map::INVALID_INDEX_VALUE)
  {
    QStringList alternates;
    for(int idx = offset; idx < offset + getNumAlternateLegs(); idx++)
      alternates.append(at(idx).getIdent());

    if(!alternates.isEmpty())
      getFlightplan().getProperties().insert(atools::fs::pln::ALTERNATES, alternates.join("#"));
    else
      getFlightplan().getProperties().remove(atools::fs::pln::ALTERNATES);
  }
  else
    getFlightplan().getProperties().remove(atools::fs::pln::ALTERNATES);
}

void Route::updateProcedureLegs(FlightplanEntryBuilder *entryBuilder, bool clearOldProcedureProperties)
{
  clearProcedureLegs(proc::PROCEDURE_ALL);

  departureLegsOffset = map::INVALID_INDEX_VALUE;
  starLegsOffset = map::INVALID_INDEX_VALUE;
  arrivalLegsOffset = map::INVALID_INDEX_VALUE;

  // Create route legs and flight plan entries from departure
  if(!departureLegs.isEmpty())
    // Starts always after departure airport
    departureLegsOffset = 1;

  QList<FlightplanEntry>& entries = flightplan.getEntries();
  for(int i = 0; i < departureLegs.size(); i++)
  {
    int insertIndex = 1 + i;
    RouteLeg obj(&flightplan);
    obj.createFromProcedureLeg(i, departureLegs, &at(i));
    insert(insertIndex, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departureLegs.at(insertIndex - 1), entry, true);
    entries.insert(insertIndex, entry);
  }

  // Create route legs and flight plan entries from STAR
  int insertOffset = numAlternateLegs + 1;
  if(!starLegs.isEmpty())
    starLegsOffset = size() - insertOffset;

  for(int i = 0; i < starLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &at(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromProcedureLeg(i, starLegs, prev);
    insert(size() - insertOffset, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(starLegs.at(i), entry, true);
    entries.insert(entries.size() - insertOffset, entry);
  }

  // Create route legs and flight plan entries from arrival
  if(!arrivalLegs.isEmpty())
    arrivalLegsOffset = size() - insertOffset;

  for(int i = 0; i < arrivalLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &at(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromProcedureLeg(i, arrivalLegs, prev);
    insert(size() - insertOffset, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(arrivalLegs.at(i), entry, true);
    entries.insert(entries.size() - insertOffset, entry);
  }

  // Leave procedure information in the PLN file
  if(clearOldProcedureProperties)
    clearFlightplanProcedureProperties(proc::PROCEDURE_ALL);

  ProcedureQuery::fillFlightplanProcedureProperties(flightplan.getProperties(), arrivalLegs, starLegs, departureLegs);
}

void Route::removeRouteLegs()
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = getSizeWithoutAlternates() - 2; i > 0; i--)
  {
    const RouteLeg& routeLeg = at(i);
    if(routeLeg.isRoute()) // also excludes alternates
      indexes.append(i);
  }

  // Delete in route legs and flight plan from the end
  for(int i = 0; i < indexes.size(); i++)
  {
    removeAt(indexes.at(i));
    flightplan.getEntries().removeAt(indexes.at(i));
  }
}

void Route::clearProcedureLegs(proc::MapProcedureTypes type, bool clearRoute, bool clearFlightplan)
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = size() - 1; i >= 0; i--)
  {
    const RouteLeg& routeLeg = at(i);
    if(type & routeLeg.getProcedureLeg().mapType) // Check if any bits/flags overlap
      indexes.append(i);
  }

  // Delete in route legs and flight plan from the end
  for(int i = 0; i < indexes.size(); i++)
  {
    if(clearRoute)
      removeAt(indexes.at(i));
    if(clearFlightplan)
      flightplan.getEntries().removeAt(indexes.at(i));
  }
}

void Route::updateAll()
{
  updateIndicesAndOffsets();
  updateAlternateProperties();
  updateMagvar();
  updateDistancesAndCourse();
  updateBoundingRect();
}

void Route::updateAirportRegions()
{
  int i = 0;
  for(RouteLeg& leg : *this)
  {
    if(leg.getMapObjectType() == map::AIRPORT)
    {
      NavApp::getAirportQuerySim()->getAirportRegion(leg.getAirport());
      flightplan.getEntries()[i].setIcaoRegion(leg.getAirport().region);
    }
    i++;
  }
}

int Route::adjustedActiveLeg() const
{
  int retval = activeLegIndex;
  if(retval < map::INVALID_INDEX_VALUE)
  {
    // Put the active back into bounds
    retval = std::min(retval, size() - 1);
    retval = std::max(retval, 0);
  }
  return retval;
}

void Route::updateIndicesAndOffsets()
{
  activeLegIndex = adjustedActiveLeg();
  departureLegsOffset = map::INVALID_INDEX_VALUE;
  starLegsOffset = map::INVALID_INDEX_VALUE;
  arrivalLegsOffset = map::INVALID_INDEX_VALUE;

  // Update offsets
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];
    leg.setFlightplanEntryIndex(i);

    if(leg.getProcedureLeg().isAnyDeparture() && departureLegsOffset == map::INVALID_INDEX_VALUE)
      departureLegsOffset = i;

    if(leg.getProcedureLeg().isAnyStar() && starLegsOffset == map::INVALID_INDEX_VALUE)
      starLegsOffset = i;

    if(leg.getProcedureLeg().isArrival() && arrivalLegsOffset == map::INVALID_INDEX_VALUE)
      arrivalLegsOffset = i;
  }
  updateAlternateIndicesAndOffsets();
}

void Route::updateAlternateIndicesAndOffsets()
{
  alternateLegsOffset = map::INVALID_INDEX_VALUE;

  // Update offsets
  for(int i = 0; i < size(); i++)
  {
    if(at(i).isAlternate() && alternateLegsOffset == map::INVALID_INDEX_VALUE)
    {
      alternateLegsOffset = i;
      break;
    }
  }
  numAlternateLegs = alternateLegsOffset != map::INVALID_INDEX_VALUE ? size() - getAlternateLegsOffset() : 0;
}

const RouteLeg *Route::getActiveLegCorrected(bool *corrected) const
{
  int idx = getActiveLegIndexCorrected(corrected);

  if(idx != map::INVALID_INDEX_VALUE)
    return &at(idx);
  else
    return nullptr;
}

const RouteLeg *Route::getActiveLeg() const
{
  if(activeLegIndex != map::INVALID_INDEX_VALUE)
    return &at(activeLegIndex);
  else
    return nullptr;
}

int Route::getActiveLegIndexCorrected(bool *corrected) const
{
  if(activeLegIndex == map::INVALID_INDEX_VALUE)
    return map::INVALID_INDEX_VALUE;

  int nextLeg = activeLegIndex + 1;
  if(nextLeg < size() && nextLeg == size() &&
     at(nextLeg).isAnyProcedure()
     /*&&
      *  (at(nextLeg).getProcedureLegType() == proc::INITIAL_FIX || at(nextLeg).getProcedureLeg().isHold())*/)
  {
    if(corrected != nullptr)
      *corrected = true;
    return activeLegIndex + 1;
  }
  else
  {
    if(corrected != nullptr)
      *corrected = false;
    return activeLegIndex;
  }
}

bool Route::isActiveMissed() const
{
  const RouteLeg *leg = getActiveLeg();
  if(leg != nullptr)
    return leg->getProcedureLeg().isMissed();
  else
    return false;
}

void Route::setActiveLeg(int value)
{
  if(value > 0 && value < size())
    activeLegIndex = value;
  else
    activeLegIndex = 1;

  activePos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex), activeLegResult);
}

bool Route::isAirportAfterArrival(int index)
{
  return (hasAnyArrivalProcedure() /*|| hasStarProcedure()*/) &&
         index == getDestinationAirportLegIndex() && at(index).getMapObjectType() == map::AIRPORT;
}

const RouteLeg& Route::at(int i) const
{
  if(i < 0 || i > size() - 1)
    qWarning() << Q_FUNC_INFO << "Invalid index" << i;

  return QList::at(i);
}

int Route::getSizeWithoutAlternates() const
{
  return QList::size() - getNumAlternateLegs();
}

void Route::updateDistancesAndCourse()
{
  totalDistance = 0.f;
  RouteLeg *last = nullptr;
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];

    if(leg.isAlternate())
      // Update all alternate distances from destination airport
      leg.updateDistanceAndCourse(i, &getDestinationAirportLeg());
    else
    {
      if(isAirportAfterArrival(i))
        // Airport after arrival legs does not contain a valid distance - loop ends here anyway
        continue;

      leg.updateDistanceAndCourse(i, last);

      if(!leg.getProcedureLeg().isMissed())
        // Do not sum up missed legs
        totalDistance += leg.getDistanceTo();
    }
    last = &leg;
  }
}

void Route::updateMagvar()
{
  // get magvar from internal database objects (waypoints, VOR and others)
  for(int i = 0; i < size(); i++)
    (*this)[i].updateMagvar();
}

void Route::updateLegAltitudes()
{
  AircraftPerfController *aircraftPerfController = NavApp::getAircraftPerfController();

  bool collecting = aircraftPerfController->isCollecting();
  // Calculate if values are valid or collecting data
  altitude->setCalcTopOfClimb(collecting || aircraftPerfController->isClimbValid());
  altitude->setCalcTopOfDescent(collecting || aircraftPerfController->isDescentValid());

  // Use default values if invalid values or collecting data
  const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerformance();
  altitude->setClimbRateFtPerNm(collecting ? 333.f : perf.getClimbRateFtPerNm());
  altitude->setDesentRateFtPerNm(collecting ? 333.f : perf.getDescentRateFtPerNm());

  altitude->setSimplify(
    atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_PROFILE_SIMPLYFY, true).toBool());

  altitude->setCruiseAltitude(getCruisingAltitudeFeet());

  // Need to update the wind data for manual wind setting
  NavApp::getWindReporter()->updateManualRouteWinds();
  altitude->calculate();

  altitude->calculateTrip(NavApp::getAircraftPerformance());
}

/* Update the bounding rect using marble functions to catch anti meridian overlap */
void Route::updateBoundingRect()
{
  Marble::GeoDataLineString line;

  for(const RouteLeg& routeLeg : *this)
    if(routeLeg.getPosition().isValid())
      line.append(Marble::GeoDataCoordinates(routeLeg.getPosition().getLonX(),
                                             routeLeg.getPosition().getLatY(), 0., Marble::GeoDataCoordinates::Degree));


  Marble::GeoDataLatLonBox box = Marble::GeoDataLatLonBox::fromLineString(line);
  boundingRect = atools::geo::Rect(box.west(), box.north(), box.east(), box.south());
  boundingRect.toDeg();
}

void Route::nearestAllLegIndex(const map::PosCourse& pos, float& crossTrackDistanceMeter,
                               int& index) const
{
  crossTrackDistanceMeter = map::INVALID_DISTANCE_VALUE;
  index = map::INVALID_INDEX_VALUE;

  if(!pos.isValid())
    return;

  float minDistance = map::INVALID_DISTANCE_VALUE;

  // Check only until the approach starts if required
  atools::geo::LineDistance result;

  for(int i = 1; i < size(); i++)
  {
    pos.pos.distanceMeterToLine(getPrevPositionAt(i), getPositionAt(i), result);
    float distance = std::abs(result.distance);

    if(result.status != atools::geo::INVALID && distance < minDistance)
    {
      minDistance = distance;
      crossTrackDistanceMeter = result.distance;
      index = i;
    }
  }

  if(crossTrackDistanceMeter < map::INVALID_DISTANCE_VALUE)
  {
    if(std::abs(crossTrackDistanceMeter) > atools::geo::nmToMeter(100.f))
    {
      // Too far away from any segment or point
      crossTrackDistanceMeter = map::INVALID_DISTANCE_VALUE;
      index = map::INVALID_INDEX_VALUE;
    }
  }
}

int Route::getNearestRouteLegResult(const atools::geo::Pos& pos,
                                    atools::geo::LineDistance& lineDistanceResult, bool ignoreNotEditable) const
{
  int index = map::INVALID_INDEX_VALUE;
  lineDistanceResult.status = atools::geo::INVALID;
  lineDistanceResult.distance = map::INVALID_DISTANCE_VALUE;

  if(!pos.isValid())
    return index;

  // Check only until the approach starts if required
  atools::geo::LineDistance result, minResult;
  minResult.status = atools::geo::INVALID;
  minResult.distance = map::INVALID_DISTANCE_VALUE;

  for(int i = 1; i < size(); i++)
  {
    if(ignoreNotEditable && !canEditLeg(i))
      continue;

    pos.distanceMeterToLine(getPrevPositionAt(i), getPositionAt(i), result);

    if(result.status != atools::geo::INVALID && std::abs(result.distance) < std::abs(minResult.distance))
    {
      minResult = result;
      index = i;
    }
  }

  if(index != map::INVALID_INDEX_VALUE)
    lineDistanceResult = minResult;

  return index;
}

const RouteLeg& Route::getStartAfterProcedure() const
{
  return at(getStartIndexAfterProcedure());
}

const RouteLeg& Route::getDestinationBeforeProcedure() const
{
  return at(getDestinationIndexBeforeProcedure());
}

bool Route::isActiveValid() const
{
  return activeLegIndex > 0 && activeLegIndex < size();
}

bool Route::isActiveAlternate() const
{
  return alternateLegsOffset != map::INVALID_INDEX_VALUE && activeLegIndex != map::INVALID_INDEX_VALUE &&
         activeLegIndex >= alternateLegsOffset;
}

void Route::setDepartureParking(const map::MapParking& departureParking)
{
  int idx = getDepartureAirportLegIndex();

  if(idx != map::INVALID_INDEX_VALUE)
  {
    RouteLeg& leg = (*this)[idx];
    leg.setDepartureParking(departureParking);
    leg.setDepartureStart(map::MapStart());
  }
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << idx;
}

void Route::setDepartureStart(const map::MapStart& departureStart)
{
  int idx = getDepartureAirportLegIndex();

  if(idx != map::INVALID_INDEX_VALUE)
  {
    RouteLeg& leg = (*this)[idx];
    leg.setDepartureParking(map::MapParking());
    leg.setDepartureStart(departureStart);
  }
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << idx;
}

bool Route::isAirportDeparture(const QString& ident) const
{
  return !isEmpty() && first().getAirport().isValid() && first().getAirport().ident == ident;
}

bool Route::isAirportDestination(const QString& ident) const
{
  return !isEmpty() && getDestinationAirportLeg().getAirport().isValid() &&
         getDestinationAirportLeg().getAirport().ident == ident;
}

int Route::getStartIndexAfterProcedure() const
{
  if(hasAnyDepartureProcedure())
    return departureLegsOffset + departureLegs.size() - 1;
  else
    return 0;
}

int Route::getDestinationIndexBeforeProcedure() const
{
  if(hasAnyStarProcedure())
    return starLegsOffset;
  else if(hasAnyArrivalProcedure())
    return arrivalLegsOffset;
  else
    return size() - numAlternateLegs - 1;
}

void Route::removeDuplicateRouteLegs()
{
  if(hasAnyStarProcedure() || hasAnyArrivalProcedure())
  {
    // remove duplicates between start of STAR, approach or transition and
    // route: only legs that have the navaid at the start of the leg - currently only initial fix
    int offset = -1;
    if(hasAnyStarProcedure())
      offset = getStarLegsOffset();
    else if(hasAnyArrivalProcedure())
      offset = getArrivalLegsOffset();

    if(offset != -1 && offset < size() - 1)
    {
      const RouteLeg& routeLeg = at(offset - 1);
      const RouteLeg& arrivalLeg = at(offset);

      if(arrivalLeg.isAnyProcedure() &&
         routeLeg.isRoute() &&
         atools::contains(arrivalLeg.getProcedureLegType(),
                          {proc::INITIAL_FIX, proc::START_OF_PROCEDURE, proc::TRACK_FROM_FIX_FROM_DISTANCE,
                           proc::TRACK_FROM_FIX_TO_DME_DISTANCE, proc::FROM_FIX_TO_MANUAL_TERMINATION}) &&
         arrivalLeg.isNavaidEqualTo(routeLeg))
      {
        qDebug() << "removing duplicate leg at" << (offset - 1) << at(offset - 1);
        removeAt(offset - 1);
        flightplan.getEntries().removeAt(offset - 1);
      }
    }
  }

  if(hasAnyDepartureProcedure())
  {
    // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
    int offset = getDepartureLegsOffset() + getDepartureLegs().size() - 1;
    if(offset < size() - 1)
    {
      const RouteLeg& departureLeg = at(offset);
      const RouteLeg& routeLeg = at(offset + 1);

      if(departureLeg.isAnyProcedure() &&
         routeLeg.isRoute() &&
         atools::contains(departureLeg.getProcedureLegType(),
                          {proc::TRACK_TO_FIX, proc::COURSE_TO_FIX, proc::ARC_TO_FIX, proc::DIRECT_TO_FIX}) &&
         departureLeg.isNavaidEqualTo(routeLeg))
      {
        // Remove duplicate leg and erase airway of following
        qDebug() << "removing duplicate leg at" << (offset + 1) << at(offset + 1);
        removeAt(offset + 1);
        (*this)[offset + 1].setAirway(map::MapAirway());

        flightplan.getEntries().removeAt(offset + 1);
        flightplan.getEntries()[offset + 1].setAirway(QString());
      }
    }
  }

  for(int i = size() - 1; i > 0; i--)
  {
    const RouteLeg& leg1 = at(i - 1);
    const RouteLeg& leg2 = at(i);
    if(leg1.isRoute() && leg2.isRoute() && leg1.isNavaidEqualTo(leg2))
    {
      removeAt(i);
      flightplan.getEntries().removeAt(i);
    }
  }
}

const atools::geo::Pos& Route::getPositionAt(int i) const
{
  return at(i).getPosition();
}

const atools::geo::Pos& Route::getPrevPositionAt(int i) const
{
  if(i > 0)
  {
    if(at(i).isAlternate())
      return getDestinationAirportLeg().getPosition();
    else
      return at(i - 1).getPosition();
  }
  else
    return atools::geo::EMPTY_POS;
}

void Route::createRouteLegsFromFlightplan()
{
  clear();

  const RouteLeg *lastLeg = nullptr;

  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    RouteLeg leg(&flightplan);
    leg.createFromDatabaseByEntry(i, lastLeg);

    if(leg.getMapObjectType() == map::INVALID)
      // Not found in database
      qWarning() << "Entry for ident" << flightplan.at(i).getIcaoIdent()
                 << "region" << flightplan.at(i).getIcaoRegion() << "is not valid";

    append(leg);
    lastLeg = &last();
  }

  if(!isEmpty())
  {
    // Correct departure and destination values if missing - can happen after import of FLP or FMS plans
    if(flightplan.getDepartureAiportName().isEmpty())
      flightplan.setDepartureAiportName(first().getName());
    if(!flightplan.getDeparturePosition().isValid())
      flightplan.setDeparturePosition(first().getPosition());

    if(flightplan.getDestinationAiportName().isEmpty())
      flightplan.setDestinationAiportName(getDestinationAirportLeg().getName());
    if(!flightplan.getDestinationPosition().isValid())
      flightplan.setDestinationPosition(first().getPosition());
  }
}

Route Route::adjustedToProcedureOptions(bool saveApproachWp, bool saveSidStarWp) const
{
  qDebug() << Q_FUNC_INFO << "saveApproachWp" << saveApproachWp << "saveSidStarWp" << saveSidStarWp;

  Route route(*this);

  // Copy flight plan profile altitudes into entries for FMS and other formats
  // All following functions have to use setCoords instead of setPosition to avoid overwriting
  QVector<float> altVector = altitude->getAltitudes();
  for(int i = 0; i < route.size(); i++)
    route.getFlightplan().getEntries()[i].setAltitude(altVector.at(i));

  // First remove properties and procedure structures
  if(saveApproachWp)
  {
    route.clearProcedures(proc::PROCEDURE_ARRIVAL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_ARRIVAL);
    route.clearProcedureLegs(proc::PROCEDURE_MISSED);
  }

  if(saveSidStarWp)
  {
    route.clearProcedures(proc::PROCEDURE_SID_STAR_ALL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_SID_STAR_ALL);
  }

  if(saveApproachWp || saveSidStarWp)
  {
    Flightplan& fp = route.getFlightplan();
    auto& entries = fp.getEntries();

    // Now replace all entries with either valid waypoints or user defined waypoints
    for(int i = 0; i < entries.size(); i++)
    {
      FlightplanEntry& entry = entries[i];
      const RouteLeg& leg = route.at(i);

      if((saveApproachWp && (leg.getProcedureType() & proc::PROCEDURE_ARRIVAL)) ||
         (saveSidStarWp && (leg.getProcedureType() & proc::PROCEDURE_SID_STAR_ALL)))
      {
        // Entry is one of the category which have to be replaced
        entry.setIcaoIdent(QString());
        entry.setIcaoRegion(QString());
        entry.setCoords(leg.getPosition());
        entry.setAirway(QString());
        entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);

        if(leg.getWaypoint().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::INTERSECTION);
          entry.setIcaoIdent(leg.getWaypoint().ident);
          entry.setIcaoRegion(leg.getWaypoint().region);
          entry.setWaypointId(leg.getWaypoint().ident);
        }
        else if(leg.getVor().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::VOR);
          entry.setIcaoIdent(leg.getVor().ident);
          entry.setIcaoRegion(leg.getVor().region);
          entry.setWaypointId(leg.getVor().ident);
        }
        else if(leg.getNdb().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::NDB);
          entry.setIcaoIdent(leg.getNdb().ident);
          entry.setIcaoRegion(leg.getNdb().region);
          entry.setWaypointId(leg.getNdb().ident);
        }
        else if(leg.getRunwayEnd().isValid())
        {
          // Make a user defined waypoint from a runway end
          entry.setWaypointType(atools::fs::pln::entry::USER);
          entry.setWaypointId("RW" + leg.getRunwayEnd().name);
        }
        else
        {
          // Make a user defined waypoint from manual, altitude or other points
          entry.setWaypointType(atools::fs::pln::entry::USER);
          entry.setWaypointId(atools::fs::util::adjustFsxUserWpName(leg.getProcedureLeg().displayText.join(" ")));
        }
      }
    }

    // Now get rid of all procedure legs in the flight plan
    auto it = std::remove_if(entries.begin(), entries.end(),
                             [](const FlightplanEntry& type) -> bool
    {
      return type.getFlags() & atools::fs::pln::entry::PROCEDURE;
    });

    if(it != entries.end())
      entries.erase(it, entries.end());

    // Delete consecutive duplicates
    for(int i = entries.size() - 1; i > 0; i--)
    {
      const FlightplanEntry& entry = entries.at(i);
      const FlightplanEntry& prev = entries.at(i - 1);

      if(entry.getWaypointId() == prev.getWaypointId() &&
         entry.getIcaoIdent() == prev.getIcaoIdent() &&
         entry.getIcaoRegion() == prev.getIcaoRegion() &&
         entry.getPosition().almostEqual(prev.getPosition(), Pos::POS_EPSILON_100M))
        entries.removeAt(i);
    }

    // Copy flight plan entries to route legs - will also add coordinates
    route.createRouteLegsFromFlightplan();
    route.updateAll();

    // Airways are updated in route controller
  }
  return route;
}

void Route::changeUserAndPosition(int index, const QString& name, const atools::geo::Pos& pos)
{
  (*this)[index].updateUserName(name);
  (*this)[index].updateUserPosition(pos);
}

/* Check if route has valid departure parking.
 *  @return true if route can be saved anyway */
bool Route::hasValidParking() const
{
  if(hasValidDeparture())
  {
    const QList<map::MapParking> *parkingCache = NavApp::getAirportQuerySim()->getParkingsForAirport(first().getId());

    if(!parkingCache->isEmpty())
      return hasDepartureParking() || hasDepartureHelipad();
    else
      // No parking available - so no parking selection is ok
      return true;
  }
  else
    return false;
}

/* Fetch airways by waypoint and name and adjust route altititude if needed */
void Route::updateAirwaysAndAltitude(bool adjustRouteAltitude, bool adjustRouteType)
{
  if(isEmpty())
    return;

  bool hasAirway = false;
  int minAltitude = 0;
  for(int i = 1; i < size(); i++)
  {
    RouteLeg& routeLeg = (*this)[i];
    const RouteLeg& prevLeg = at(i - 1);

    if(!routeLeg.getAirwayName().isEmpty())
    {
      map::MapAirway airway;
      NavApp::getMapQuery()->getAirwayByNameAndWaypoint(airway, routeLeg.getAirwayName(), prevLeg.getIdent(),
                                                        routeLeg.getIdent());
      routeLeg.setAirway(airway);
      minAltitude = std::max(airway.minAltitude, minAltitude);

      hasAirway |= !routeLeg.getAirwayName().isEmpty();
      // qDebug() << "min" << airway.minAltitude << "max" << airway.maxAltitude;
    }
    else
      routeLeg.setAirway(map::MapAirway());
  }

  if(minAltitude > 0 && adjustRouteAltitude)
  {
    if(OptionData::instance().getFlags() & opts::ROUTE_ALTITUDE_RULE)
      // Apply simplified east/west rule
      minAltitude = adjustAltitude(minAltitude);
    getFlightplan().setCruisingAltitude(minAltitude);

    qDebug() << Q_FUNC_INFO << "Updating flight plan altitude to" << minAltitude;
  }

  if(adjustRouteType)
  {
    if(hasAirway)
    {
      if(getFlightplan().getCruisingAltitude() >= Unit::altFeetF(20000))
        getFlightplan().setRouteType(atools::fs::pln::HIGH_ALTITUDE);
      else
        getFlightplan().setRouteType(atools::fs::pln::LOW_ALTITUDE);
    }
    else
      getFlightplan().setRouteType(atools::fs::pln::DIRECT);

    qDebug() << Q_FUNC_INFO << "Updating flight plan route type to" << getFlightplan().getRouteType();
  }
}

/* Apply simplified east/west or north/south rule */
int Route::adjustAltitude(int minAltitude) const
{
  if(getSizeWithoutAlternates() > 1)
  {
    const Pos& departurePos = first().getPosition();
    const Pos& destinationPos = getDestinationAirportLeg().getPosition();

    float magvar = (first().getMagvar() + getDestinationAirportLeg().getMagvar()) / 2;

    float fpDir = atools::geo::normalizeCourse(departurePos.angleDegToRhumb(destinationPos) - magvar);

    if(fpDir < Pos::INVALID_VALUE)
    {
      qDebug() << Q_FUNC_INFO << "minAltitude" << minAltitude << "fp dir" << fpDir;

      // East / West: Rounds up  cruise altitude to nearest odd thousand feet for eastward flight plans
      // and nearest even thousand feet for westward flight plans.
      // North / South: Rounds up  cruise altitude to nearest odd thousand feet for southward flight plans
      // and nearest even thousand feet for northward flight plans.

      // In Italy, France and Portugal, for example, southbound traffic uses odd flight levels;
      // in New Zealand, southbound traffic uses even flight levels.

      bool odd = false;
      if(OptionData::instance().getAltitudeRuleType() == opts::EAST_WEST)
        odd = fpDir >= 0.f && fpDir <= 180.f;
      else if(OptionData::instance().getAltitudeRuleType() == opts::NORTH_SOUTH)
        odd = fpDir >= 90.f && fpDir <= 270.f;
      else if(OptionData::instance().getAltitudeRuleType() == opts::SOUTH_NORTH)
        odd = !(fpDir >= 90.f && fpDir <= 270.f);

      if(getFlightplan().getFlightplanType() == atools::fs::pln::IFR)
      {
        if(odd)
          // round up to the next odd value
          minAltitude = static_cast<int>(std::ceil((minAltitude - 1000.f) / 2000.f) * 2000.f + 1000.f);
        else
          // round up to the next even value
          minAltitude = static_cast<int>(std::ceil((minAltitude) / 2000.f) * 2000.f);
      }
      else
      {
        if(odd)
          // round up to the next odd value + 500
          minAltitude = static_cast<int>(std::ceil((minAltitude - 1500.f) / 2000.f) * 2000.f + 1500.f);
        else
          // round up to the next even value + 500
          minAltitude = static_cast<int>(std::ceil((minAltitude - 500.f) / 2000.f) * 2000.f + 500.f);
      }

      qDebug() << "corrected minAltitude" << minAltitude;
    }
  }
  return minAltitude;
}

void Route::getApproachRunwayEndAndIls(QVector<map::MapIls>& ils, map::MapRunwayEnd *runwayEnd) const
{
  int destinationLegIdx = getDestinationLegIndex();
  if(destinationLegIdx < map::INVALID_INDEX_VALUE)
  {
    const RouteLeg& leg = at(destinationLegIdx);

    const RouteLeg& destLeg = getDestinationAirportLeg();

    // Get the runway end for arrival
    QList<map::MapRunwayEnd> runwayEnds;
    if(arrivalLegs.runwayEnd.isValid())
      NavApp::getMapQuery()->getRunwayEndByNameFuzzy(runwayEnds, arrivalLegs.runwayEnd.name, destLeg.getAirport(),
                                                     false /* nav data */);

    ils.clear();
    if(leg.isAnyProcedure() && !(leg.getProcedureType() & proc::PROCEDURE_MISSED) && leg.getRunwayEnd().isValid())
    {
      // Get ILS from flight plan leg as is
      ils = NavApp::getMapQuery()->getIlsByAirportAndRunway(destLeg.getAirport().ident, leg.getRunwayEnd().name);

      if(ils.isEmpty())
        // Get all ILS for the runway end found in a fuzzy way
        ils = NavApp::getMapQuery()->getIlsByAirportAndRunway(destLeg.getAirport().ident, runwayEnds.first().name);

      if(ils.isEmpty())
      {
        // ILS does not even match runway - try again fuzzy
        QStringList variants = map::runwayNameVariants(runwayEnds.first().name);
        for(const QString& runwayVariant : variants)
          ils.append(NavApp::getMapQuery()->getIlsByAirportAndRunway(destLeg.getAirport().ident, runwayVariant));
      }
    }

    if(runwayEnd != nullptr)
      *runwayEnd = runwayEnds.isEmpty() ? map::MapRunwayEnd() : runwayEnds.first();
  }
}

float Route::getDistanceToFlightPlan() const
{
  if(activeLegResult.status != atools::geo::INVALID)
    return atools::geo::meterToNm(std::abs(activeLegResult.distance));
  else
    return map::INVALID_DISTANCE_VALUE;
}

QString Route::getProcedureLegText(proc::MapProcedureTypes mapType) const
{
  QString procText;

  if(mapType & proc::PROCEDURE_APPROACH || mapType & proc::PROCEDURE_MISSED)
  {
    procText = QObject::tr("%1 %2 %3%4").
               arg(mapType & proc::PROCEDURE_MISSED ? tr("Missed") : tr("Approach")).
               arg(arrivalLegs.approachType).
               arg(arrivalLegs.approachFixIdent).
               arg(arrivalLegs.approachSuffix.isEmpty() ? QString() : (tr("-") + arrivalLegs.approachSuffix));
  }
  else if(mapType & proc::PROCEDURE_TRANSITION)
    procText = QObject::tr("Transition %1").arg(arrivalLegs.transitionFixIdent);
  else if(mapType & proc::PROCEDURE_STAR)
    procText = QObject::tr("STAR %1").arg(starLegs.approachFixIdent);
  else if(mapType & proc::PROCEDURE_SID)
    procText = QObject::tr("SID %1").arg(departureLegs.approachFixIdent);
  else if(mapType & proc::PROCEDURE_SID_TRANSITION)
    procText = QObject::tr("SID Transition %1").arg(departureLegs.transitionFixIdent);
  else if(mapType & proc::PROCEDURE_STAR_TRANSITION)
    procText = QObject::tr("STAR Transition %1").arg(starLegs.transitionFixIdent);

  return procText;
}

QDebug operator<<(QDebug out, const Route& route)
{
  out << endl << "Route ==================================================================" << endl;
  out << route.getFlightplan().getProperties() << endl << endl;
  for(int i = 0; i < route.size(); ++i)
    out << "===" << i << route.at(i) << endl;
  out << endl << "Departure ===========" << endl;
  out << "offset" << route.getDepartureAirportLegIndex();
  out << route.getDepartureAirportLeg() << endl;
  out << endl << "Destination ===========" << endl;
  out << "offset" << route.getDestinationAirportLegIndex();
  out << route.getDestinationAirportLeg() << endl;
  out << endl << "Departure Procedure ===========" << endl;
  out << "offset" << route.getDepartureLegsOffset();
  out << route.getDepartureLegs() << endl;
  out << "STAR Procedure ===========" << endl;
  out << "offset" << route.getStarLegsOffset() << endl;
  out << route.getStarLegs() << endl;
  out << "Arrival Procedure ========" << endl;
  out << "offset" << route.getArrivalLegsOffset() << endl;
  out << route.getArrivalLegs() << endl;
  out << "Alternates ========" << endl;
  out << "offset" << route.getAlternateLegsOffset() << "num" << route.getNumAlternateLegs() << endl;
  out << "==================================================================" << endl;

  out << endl << "RouteAltitudeLegs ==================================================================" << endl;
  out << route.getAltitudeLegs();
  out << "==================================================================" << endl;

  return out;
}
