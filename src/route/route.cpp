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

#include "route/route.h"

#include "geo/calculations.h"
#include "common/maptools.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "common/unit.h"
#include "common/constants.h"
#include "route/flightplanentrybuilder.h"
#include "query/procedurequery.h"
#include "query/airportquery.h"
#include "weather/windreporter.h"
#include "navapp.h"
#include "fs/util/fsutil.h"
#include "route/routealtitude.h"
#include "fs/perf/aircraftperf.h"
#include "settings/settings.h"

#include <QBitArray>
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

const static RouteLeg EMPTY_ROUTELEG;

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

  approachLegs = other.approachLegs;
  starLegs = other.starLegs;
  sidLegs = other.sidLegs;

  sidLegsOffset = other.sidLegsOffset;
  starLegsOffset = other.starLegsOffset;
  approachLegsOffset = other.approachLegsOffset;

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
      nextNum = std::max(QString(USER_WP_ID.match(entry.getIdent()).captured(1)).toInt(), nextNum);
  }
  return nextNum + 1;
}

bool Route::canEditLeg(int index) const
{
  if(hasAnySidProcedure() && index < sidLegsOffset + sidLegs.size())
    return false;

  if(hasAnyStarProcedure() && index > starLegsOffset)
    return false;

  if(hasAnyApproachProcedure() && index > approachLegsOffset)
    return false;

  if(hasAlternates() && index >= alternateLegsOffset)
    return false;

  return true;
}

bool Route::canEditPoint(int index) const
{
  return value(index).isRoute() || value(index).isAlternate();
}

bool Route::canEditComment(int index) const
{
  return value(index).isRoute() && !value(index).isAlternate();
}

void Route::getSidStarNames(QString& sid, QString& sidTrans, QString& star, QString& starTrans) const
{
  sid = sidLegs.approachFixIdent;
  sidTrans = sidLegs.transitionFixIdent;
  star = starLegs.approachFixIdent;
  starTrans = starLegs.transitionFixIdent;
}

void Route::getRunwayNames(QString& departure, QString& arrival) const
{
  departure = sidLegs.runwayEnd.name;

  if(hasAnyApproachProcedure())
    arrival = approachLegs.runwayEnd.name;
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
  return approachLegs.runwayEnd.name;
}

void Route::getArrivalNames(QString& arrivalArincName, QString& arrivalTransition) const
{
  if(hasAnyApproachProcedure())
  {
    arrivalArincName = approachLegs.approachArincName;
    arrivalTransition = approachLegs.transitionFixIdent;
  }
  else
  {
    arrivalArincName.clear();
    arrivalTransition.clear();
  }
}

void Route::updateActiveLegAndPos(bool force, bool flying)
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

  if(flying)
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
    activePos.pos.distanceMeterToLine(first().getPosition(), first().getPosition(), activeLegResult);
  }
  else
  {
    if(activeLegIndex == 0)
      // Reset from point route ====================================
      activeLegIndex = 1;
    activePos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex),
                                      activeLegResult);
  }

  if(isTooFarToFlightPlan())
  {
    // Too far away from plan - remove active leg
    activeLegIndex = map::INVALID_INDEX_VALUE;
    return;
  }

#ifdef DEBUG_ACTIVE_LEG
  qDebug() << "activePos" << activeLegResult;
#endif

  if(activeLegIndex != map::INVALID_INDEX_VALUE && value(activeLegIndex).isAlternate())
    return;

  // =========================================================================
  // Get potential next leg and course difference
  int nextLeg = activeLegIndex + 1;
  float courseDiff = 0.f;
  if(nextLeg < size())
  {
    // Catch the case of initial fixes or others that are points instead of lines and try the next legs
    if(!value(activeLegIndex).getProcedureLeg().isHold())
    {
      while(atools::contains(value(nextLeg).getProcedureLegType(),
                             {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) &&
            getPrevPositionAt(nextLeg) == getPositionAt(nextLeg) &&
            nextLeg < size() - 2)
        nextLeg++;
    }
    else
    {
      // Jump all initial fixes for holds since the next line can probably not overlap
      while(atools::contains(value(nextLeg).getProcedureLegType(),
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
    if(value(activeLegIndex).getProcedureLeg().isHold())
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << "ACTIVE HOLD";
#endif
      // Test next leg if we can exit a hold
      if(value(nextLeg).getProcedureLeg().line.getPos1() == value(activeLegIndex).getPosition())
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
        value(activeLegIndex).getProcedureLeg().holdLine.distanceMeterToLine(activePos.pos, resultHold);
#ifdef DEBUG_ACTIVE_LEG
        qDebug() << "NEXT HOLD" << nextLeg << resultHold;
        qDebug() << "HOLD DIFFER";
        qDebug() << "resultHold" << resultHold;
        qDebug() << "holdLine" << at(activeLegIndex).getProcedureLeg().holdLine;
#endif
        // Hold point differs from next leg start - use the helping line
        if(resultHold.status == atools::geo::ALONG_TRACK) // Check if we are outside of the hold
        {
          if(value(activeLegIndex).getProcedureLeg().turnDirection == "R")
            switchToNextLeg = resultHold.distance < nmToMeter(-0.5f);
          else
            switchToNextLeg = resultHold.distance > nmToMeter(0.5f);
        }
      }
    }
    else
    {
      if(value(nextLeg).getProcedureLeg().isHold())
      {
        // Ignore all other rulues and use distance to hold point to activate hold
        if(std::abs(nextLegResult.distance) < nmToMeter(0.5f))
          switchToNextLeg = true;
      }
      else if(value(activeLegIndex).getProcedureLegType() == proc::PROCEDURE_TURN)
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
    if(isAirportAfterArrival(nextLeg) || value(nextLeg).isAlternate())
      // Do not sequence after arrival or after end of missed
      switchToNextLeg = false;

    if(switchToNextLeg)
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << "Switching to next leg";
#endif

      // Either left current leg or closer to next and on courses
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes& map::MISSED_APPROACH) && value(nextLeg).getProcedureLeg().isMissed()))
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
  const RouteLeg& activeLeg = value(active);

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
    if(!value(routeIndex).getProcedureLeg().isMissed() || activeIsMissed ||
       !value(routeIndex).isAlternate() || activeIsAlternate)
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
      if(!value(i).getProcedureLeg().isMissed() || activeIsMissed ||
         !value(i).isAlternate() || activeIsAlternate)
        fromStart += value(i).getDistanceTo();
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
            if(value(i).getProcedureLeg().isMissed())
              *distToDest += value(i).getDistanceTo();
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
            if(value(i).isAlternate())
              *distToDest += value(i).getDistanceTo();
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

float Route::getDistanceFromStart(const Pos& pos) const
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
        fromstart += value(i).getDistanceTo();

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
    projectionDistance = legFromStart - value(legIndex).getDistanceTo();

  if(value(legIndex).isAnyProcedure() && value(legIndex).getProcedureLeg().isMissed())
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
  return Unit::rev(flightplan.getCruisingAltitude(), Unit::altFeetF);
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

Pos Route::getTopOfDescentPos() const
{
  return altitude->getTopOfDescentPos();
}

Pos Route::getTopOfClimbPos() const
{
  return altitude->getTopOfClimbPos();
}

Pos Route::getPositionAtDistance(float distFromStartNm) const
{
  if(distFromStartNm < 0.f || distFromStartNm > totalDistance)
    return atools::geo::EMPTY_POS;

  Pos retval;

  // Find the leg that contains the given distance point
  float total = 0.f;
  int foundIndex = map::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
  for(int i = 0; i <= getDestinationAirportLegIndex(); i++)
  {
    total += value(i + 1).getDistanceTo();
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
    if(value(foundIndex).getGeometry().size() > 2)
    {
      // Use approach geometry to display
      float base = distFromStartNm - (total - value(foundIndex).getProcedureLeg().calculatedDistance);
      float fraction = base / value(foundIndex).getProcedureLeg().calculatedDistance;
      retval = value(foundIndex).getGeometry().interpolate(fraction);
    }
    else
    {
      float base = distFromStartNm - (total - value(foundIndex).getDistanceTo());
      float fraction = base / value(foundIndex).getDistanceTo();
      retval = getPrevPositionAt(foundIndex).interpolate(getPositionAt(foundIndex), fraction);
    }
  }

  if(!retval.isValid())
    qWarning() << Q_FUNC_INFO << "position not valid";
  return retval;
}

const RouteAltitudeLeg& Route::getAltitudeLegAt(int i) const
{
  return altitude->value(i);
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

  if(hasAnyApproachProcedure())
  {
    // Last leg before missed approach is destination runway
    for(int i = approachLegsOffset; i <= getDestinationAirportLegIndex(); i++)
    {
      const RouteLeg& leg = value(i);
      if((leg.isAnyProcedure() && leg.getProcedureLeg().isMissed()) || leg.isAlternate())
        return i - 1;
    }
    return getDestinationAirportLegIndex() - 1;
  }

  // Simple destination airport without approach
  return getDestinationAirportLegIndex();
}

const RouteLeg& Route::getDestinationLeg() const
{
  int idx = getDestinationLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? value(idx) : EMPTY_ROUTELEG;
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
  return idx != map::INVALID_INDEX_VALUE ? value(idx) : EMPTY_ROUTELEG;
}

int Route::getSidLegIndex() const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  if(hasAnySidProcedure())
    return 1;

  return 0;
}

const RouteLeg& Route::getSidLeg() const
{
  int idx = getSidLegIndex();
  return idx != map::INVALID_INDEX_VALUE ? value(idx) : EMPTY_ROUTELEG;
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
  return idx != map::INVALID_INDEX_VALUE ? value(idx) : EMPTY_ROUTELEG;
}

void Route::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                       map::MapSearchResult& mapobjects, map::MapObjectQueryTypes types) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < size(); i++)
  {
    const RouteLeg& leg = value(i);
    if(!(types& map::QUERY_PROCEDURES) && leg.isAnyProcedure())
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
        up.ident = leg.getIdent() + tr(" (not found)");
        up.region = leg.getRegion();
        up.name = leg.getName();
        up.comment = leg.getComment();
        up.position = leg.getPosition();
        up.magvar = NavApp::getMagVar(leg.getPosition());
        mapobjects.userpointsRoute.append(up);
      }

      if(leg.getMapObjectType() == map::USERPOINTROUTE)
      {
        map::MapUserpointRoute up;
        up.id = i;
        up.routeIndex = i;
        up.ident = leg.getIdent();
        up.region = leg.getRegion();
        up.name = leg.getName();
        up.comment = leg.getComment();
        up.position = leg.getPosition();
        up.magvar = NavApp::getMagVar(leg.getPosition());
        mapobjects.userpointsRoute.append(up);
      }

      if(types & map::QUERY_PROC_POINTS && leg.isAnyProcedure())
        mapobjects.procPoints.append(proc::MapProcedurePoint(leg.getProcedureLeg(), false /* preview */));
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
    const RouteLeg& routeLeg = value(i);
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
    sidLegs.clearApproach();
  if(type & proc::PROCEDURE_SID_TRANSITION)
    sidLegs.clearTransition();

  if(type & proc::PROCEDURE_STAR_TRANSITION)
    starLegs.clearTransition();
  if(type & proc::PROCEDURE_STAR)
    starLegs.clearApproach();

  if(type & proc::PROCEDURE_TRANSITION)
    approachLegs.clearTransition();
  if(type & proc::PROCEDURE_APPROACH)
    approachLegs.clearApproach();
}

void Route::reloadProcedures(proc::MapProcedureTypes procs)
{
  ProcedureQuery *procQuery = NavApp::getProcedureQuery();
  AirportQuery *apQuery = NavApp::getAirportQueryNav();

  // Check which transitions are deleted where procedures are not touched
  // Reload procedures to get a clean version which is not modified by the transition
  if(procs & proc::PROCEDURE_SID_TRANSITION && !(procs & proc::PROCEDURE_SID))
    sidLegs = *procQuery->getApproachLegs(apQuery->getAirportById(sidLegs.ref.airportId), sidLegs.ref.approachId);

  if(procs & proc::PROCEDURE_STAR_TRANSITION && !(procs & proc::PROCEDURE_STAR))
    starLegs = *procQuery->getApproachLegs(apQuery->getAirportById(starLegs.ref.airportId), starLegs.ref.approachId);

  if(procs & proc::PROCEDURE_TRANSITION && !(procs & proc::PROCEDURE_APPROACH))
    approachLegs = *procQuery->getApproachLegs(apQuery->getAirportById(approachLegs.ref.airportId),
                                               approachLegs.ref.approachId);
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
    QStringList alternates = getAlternateIdents();
    if(!alternates.isEmpty())
      getFlightplan().getProperties().insert(atools::fs::pln::ALTERNATES, alternates.join("#"));
    else
      getFlightplan().getProperties().remove(atools::fs::pln::ALTERNATES);
  }
  else
    getFlightplan().getProperties().remove(atools::fs::pln::ALTERNATES);
}

QStringList Route::getAlternateIdents() const
{
  QStringList alternates;
  int offset = getAlternateLegsOffset();
  if(offset != map::INVALID_INDEX_VALUE)
  {
    for(int idx = offset; idx < offset + getNumAlternateLegs(); idx++)
      alternates.append(value(idx).getIdent());
  }
  return alternates;
}

QBitArray Route::getJetAirwayFlags() const
{
  QBitArray flags(size());

  for(int i = 0; i < size(); i++)
  {
    const map::MapAirway& airway = value(i).getAirway();
    flags.setBit(i, airway.isValid() && (airway.type == map::AIRWAY_JET || airway.type == map::AIRWAY_BOTH));
  }
  return flags;
}

int Route::legIndexForPosition(const Pos& pos, bool reverse)
{
  if(pos.isValid())
  {
    if(reverse)
    {
      // Search flight plan from end to start
      for(int i = size() - 1; i >= 0; i--)
      {
        if(value(i).getPosition().almostEqual(pos, Pos::POS_EPSILON_100M))
          return i;
      }
    }
    else
    {
      // Search flight plan from start to end
      for(int i = 0; i < size(); i++)
      {
        if(value(i).getPosition().almostEqual(pos, Pos::POS_EPSILON_100M))
          return i;
      }
    }
  }
  return -1;
}

int Route::legIndexForPositions(const LineString& line, bool reverse)
{
  if(line.isValid())
  {
    for(const Pos& pos : line)
    {
      int idx = legIndexForPosition(pos, reverse);
      if(idx >= 0)
        return idx;
    }
  }
  return -1;
}

void Route::cleanupFlightPlanForProcedures()
{
  updateIndices();

  // like removeDuplicateRouteLegs
  // Remove any legs overlapping with SID ======================================================
  int fromIdx = -1, toIdx = -1;
  if(!sidLegs.isEmpty())
  {
    // Departure - start at last SID point and look for overlap with route traversing route from end to start
    toIdx = legIndexForPositions(sidLegs.geometry().reversed(), true /* not reverse */);
    fromIdx = 1;
  }

  if(fromIdx > 0 && toIdx > 0)
    removeLegs(fromIdx, toIdx);

  fromIdx = -1;
  toIdx = -1;

  // Remove any legs overlapping with STAR or approach ======================================================
  if(!starLegs.isEmpty() || !approachLegs.isEmpty())
  {
    // Arrivals - start at first point and look for overlap with route traversing route from start to end
    updateIndices();
    updateAlternateIndicesAndOffsets();
    toIdx = getDestinationAirportLegIndex() - 1;

    if(!starLegs.isEmpty())
      fromIdx = legIndexForPositions(starLegs.geometry(), true /* reverse */);
    else if(!approachLegs.isEmpty())
      fromIdx = legIndexForPositions(approachLegs.geometry(), true /* reverse */);
  }

  if(fromIdx > 0 && toIdx > 0)
    removeLegs(fromIdx, toIdx);
}

void Route::updateProcedureLegs(FlightplanEntryBuilder *entryBuilder, bool clearOldProcedureProperties,
                                bool cleanupRoute)
{
  // Change STAR to connect manual legs to the arrival/approach or airport
  // This can only be done with a copy of the procedures in the route
  NavApp::getProcedureQuery()->postProcessLegsForRoute(starLegs, approachLegs, getDestinationAirportLeg().getAirport());

  // Remove all procedure / dummy legs from flight plan and route
  clearProcedureLegs(proc::PROCEDURE_ALL);

  if(cleanupRoute)
    cleanupFlightPlanForProcedures();

  sidLegsOffset = map::INVALID_INDEX_VALUE;
  starLegsOffset = map::INVALID_INDEX_VALUE;
  approachLegsOffset = map::INVALID_INDEX_VALUE;

  // Create route legs and flight plan entries from departure
  if(!sidLegs.isEmpty())
    // Starts always after departure airport
    sidLegsOffset = 1;

  QList<FlightplanEntry>& entries = flightplan.getEntries();
  for(int i = 0; i < sidLegs.size(); i++)
  {
    int insertIndex = 1 + i;
    RouteLeg obj(&flightplan);
    obj.createFromProcedureLegs(i, sidLegs, &value(i));
    insert(insertIndex, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(sidLegs.at(insertIndex - 1), entry, true);
    entries.insert(insertIndex, entry);
  }

  // Create route legs and flight plan entries from STAR
  int insertOffset = numAlternateLegs + 1;
  if(!starLegs.isEmpty())
    starLegsOffset = size() - insertOffset;

  for(int i = 0; i < starLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &value(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromProcedureLegs(i, starLegs, prev);
    insert(size() - insertOffset, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(starLegs.at(i), entry, true);
    entries.insert(entries.size() - insertOffset, entry);
  }

  // Create route legs and flight plan entries from arrival
  if(!approachLegs.isEmpty())
    approachLegsOffset = size() - insertOffset;

  for(int i = 0; i < approachLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &value(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromProcedureLegs(i, approachLegs, prev);
    insert(size() - insertOffset, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(approachLegs.at(i), entry, true);
    entries.insert(entries.size() - insertOffset, entry);
  }

  // Leave procedure information in the PLN file
  if(clearOldProcedureProperties)
    clearFlightplanProcedureProperties(proc::PROCEDURE_ALL);

  ProcedureQuery::fillFlightplanProcedureProperties(flightplan.getProperties(), approachLegs, starLegs, sidLegs);
}

void Route::removeLegs(int from, int to)
{
  qDebug() << Q_FUNC_INFO << "removing from" << from << "to" << to;

  // Delete in route legs and flight plan
  for(int i = to; i >= from; i--)
  {
    removeAt(i);
    flightplan.getEntries().removeAt(i);
  }
}

void Route::removeRouteLegs()
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = getSizeWithoutAlternates() - 2; i > 0; i--)
  {
    const RouteLeg& routeLeg = value(i);
    if(routeLeg.isRoute()) // excludes alternates and procedures
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
    const RouteLeg& routeLeg = value(i);
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
  removeDuplicateRouteLegs();
  validateAirways();
  updateAlternateProperties();
  updateMagvar();
  updateDistancesAndCourse();
  updateBoundingRect();

  if(!isEmpty())
  {
    // Correct departure and destination values if missing - can happen after import of FLP or FMS plans
    flightplan.setDepartureIdent(getDepartureAirportLeg().getIdent());
    if(flightplan.getDepartureName().isEmpty())
      flightplan.setDepartureName(getDepartureAirportLeg().getName());
    if(!flightplan.getDeparturePosition().isValid())
      flightplan.setDeparturePosition(first().getPosition());

    flightplan.setDestinationIdent(getDestinationAirportLeg().getIdent());
    if(flightplan.getDestinationName().isEmpty())
      flightplan.setDestinationName(getDestinationAirportLeg().getName());
    if(!flightplan.getDestinationPosition().isValid())
      flightplan.setDestinationPosition(first().getPosition());
  }
}

void Route::updateAirportRegions()
{
  int i = 0;
  for(RouteLeg& leg : *this)
  {
    if(leg.getMapObjectType() == map::AIRPORT)
    {
      NavApp::getAirportQuerySim()->getAirportRegion(leg.getAirport());
      flightplan.getEntries()[i].setRegion(leg.getAirport().region);
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

void Route::updateIndices()
{
  // Update internal indices pointing to flight plan legs
  for(int i = 0; i < size(); i++)
    (*this)[i].setFlightplanEntryIndex(i);
}

void Route::updateIndicesAndOffsets()
{
  updateIndices();

  // Update offsets
  activeLegIndex = adjustedActiveLeg();
  sidLegsOffset = map::INVALID_INDEX_VALUE;
  starLegsOffset = map::INVALID_INDEX_VALUE;
  approachLegsOffset = map::INVALID_INDEX_VALUE;

  // Update offsets
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];

    if(leg.getProcedureLeg().isAnyDeparture() && sidLegsOffset == map::INVALID_INDEX_VALUE)
      sidLegsOffset = i;

    if(leg.getProcedureLeg().isAnyStar() && starLegsOffset == map::INVALID_INDEX_VALUE)
      starLegsOffset = i;

    if(leg.getProcedureLeg().isArrival() && approachLegsOffset == map::INVALID_INDEX_VALUE)
      approachLegsOffset = i;
  }
  updateAlternateIndicesAndOffsets();
}

void Route::updateAlternateIndicesAndOffsets()
{
  alternateLegsOffset = map::INVALID_INDEX_VALUE;

  // Update offsets
  for(int i = 0; i < size(); i++)
  {
    if(value(i).isAlternate() && alternateLegsOffset == map::INVALID_INDEX_VALUE)
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
    return &value(idx);
  else
    return nullptr;
}

const RouteLeg *Route::getActiveLeg() const
{
  if(activeLegIndex != map::INVALID_INDEX_VALUE)
    return &value(activeLegIndex);
  else
    return nullptr;
}

int Route::getActiveLegIndexCorrected(bool *corrected) const
{
  if(activeLegIndex == map::INVALID_INDEX_VALUE)
    return map::INVALID_INDEX_VALUE;

  int nextLeg = activeLegIndex + 1;
  if(nextLeg < size() && nextLeg == size() && value(nextLeg).isAnyProcedure()
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
  if(size() > 1)
  {
    if(value > 0 && value < size())
      activeLegIndex = value;
    else
      activeLegIndex = 1;

    activePos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex),
                                      activeLegResult);
  }
}

bool Route::isAirportAfterArrival(int index)
{
  return (hasAnyApproachProcedure() /*|| hasStarProcedure()*/) &&
         index == getDestinationAirportLegIndex() && value(index).getMapObjectType() == map::AIRPORT;
}

int Route::getArrivaLegsOffset() const
{
  if(hasAnyStarProcedure())
    return getStarLegsOffset();
  else if(hasAnyApproachProcedure())
    return getApproachLegsOffset();

  return map::INVALID_INDEX_VALUE;
}

const RouteLeg& Route::value(int i) const
{
  const static RouteLeg EMPTY_ROUTE_LEG;
  if(!atools::inRange(*this, i))
  {
    qWarning() << Q_FUNC_INFO << "Invalid index" << i;
    return EMPTY_ROUTE_LEG;
  }
  else
    return QList::at(i);
}

int Route::getSizeWithoutAlternates() const
{
  return QList::size() - getNumAlternateLegs();
}

void Route::updateDistancesAndCourse()
{
  totalDistance = 0.f;
  RouteLeg *last = nullptr, *beforeDestAirport = nullptr;
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];

    if(leg.isAlternate())
      // Update all alternate distances from destination airport
      leg.updateDistanceAndCourse(i, &getDestinationAirportLeg());
    else
    {
      if(isAirportAfterArrival(i))
      {
        // Update with distance from previous or last procedure leg like runway
        leg.updateDistanceAndCourse(i, beforeDestAirport != nullptr ? beforeDestAirport : last);
        continue;
      }

      leg.updateDistanceAndCourse(i, last);

      if(!leg.getProcedureLeg().isMissed())
        // Do not sum up missed legs
        totalDistance += leg.getDistanceTo();
      else if(beforeDestAirport == nullptr)
        // Remember leg before airport (usually runway if procedure is used)
        beforeDestAirport = last;
    }
    last = &leg;
  }
}

void Route::updateMagvar()
{
  // get magvar from internal database objects (waypoints, VOR and others)
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];
    leg.updateMagvar();
  }

  // Update variance for to VOR legs and for legs which are outbound from VOR to other waypoint type
  for(int i = 1; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];
    if(!leg.isRoute())
      continue;

    const map::MapVor& vor = leg.getVor();
    const map::MapVor& prevVor = value(i - 1).getVor();
    if(vor.isCalibratedVor())
      // This is a VOR at the end of the leg - user variance from calibration
      leg.setMagvar(vor.magvar);
    else if(prevVor.isCalibratedVor())
      // No VOR at the end of the leg but previous one is a VOR - use previous variance
      leg.setMagvar(prevVor.magvar);
  }
}

void Route::updateLegAltitudes()
{
  if(isEmpty())
    return;

  // Uses default values if invalid values or collecting data
  altitude->setSimplify(atools::settings::Settings::instance().
                        getAndStoreValue(lnm::OPTIONS_PROFILE_SIMPLYFY, true).toBool());

  // Need to update the wind data for manual wind setting
  NavApp::getWindReporter()->updateManualRouteWinds();
  altitude->calculateAll(NavApp::getAircraftPerformance(), getCruisingAltitudeFeet());
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

int Route::getNearestRouteLegResult(const Pos& pos,
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
  return value(getStartIndexAfterProcedure());
}

int Route::getLastIndexOfDepartureProcedure() const
{
  if(hasAnySidProcedure())
    return sidLegsOffset + sidLegs.size() - 1;
  else
    return 0;
}

const RouteLeg& Route::getLastLegOfDepartureProcedure() const
{
  return value(getLastIndexOfDepartureProcedure());
}

const RouteLeg& Route::getDestinationBeforeProcedure() const
{
  return value(getDestinationIndexBeforeProcedure());
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
    (*this)[idx].setDepartureParking(departureParking);
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << idx;
}

void Route::setDepartureStart(const map::MapStart& departureStart)
{
  int idx = getDepartureAirportLegIndex();

  if(idx != map::INVALID_INDEX_VALUE)
    (*this)[idx].setDepartureStart(departureStart);
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
  if(hasAnySidProcedure())
    return sidLegsOffset + sidLegs.size();
  else
    return 0;
}

int Route::getDestinationIndexBeforeProcedure() const
{
  if(hasAnyStarProcedure())
    return starLegsOffset;
  else if(hasAnyApproachProcedure())
    return approachLegsOffset;
  else
    return size() - numAlternateLegs - 1;
}

bool Route::arrivalRouteToProcLegs(int& arrivaLegsOffset) const
{
  if(hasAnyArrivalProcedure())
  {
    arrivaLegsOffset = getArrivaLegsOffset();
    if(arrivaLegsOffset > 0 && arrivaLegsOffset < size() - 1)
    {
      const RouteLeg& routeLeg = value(arrivaLegsOffset - 1);
      const RouteLeg& arrivalLeg = value(arrivaLegsOffset);
      return arrivalLeg.isAnyProcedure() && arrivalLeg.isValid() && routeLeg.isValid() && routeLeg.isRoute();
    }
  }
  return false;
}

bool Route::departureProcToRouteLegs(int& startIndexAfterProcedure) const
{
  if(hasAnySidProcedure())
  {
    // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
    startIndexAfterProcedure = getStartIndexAfterProcedure();
    if(startIndexAfterProcedure > 0 && startIndexAfterProcedure < size() - 1)
    {
      const RouteLeg& departureLeg = value(startIndexAfterProcedure - 1);
      const RouteLeg& routeLeg = value(startIndexAfterProcedure);
      return departureLeg.isAnyProcedure() && departureLeg.isValid() && routeLeg.isRoute() && routeLeg.isValid();
    }
  }
  return false;
}

bool Route::canCalcSelection(int firstIndex, int lastIndex) const
{
  // Check validity of indexes first
  if(!atools::inRange(*this, firstIndex) || !atools::inRange(*this, lastIndex))
    return false;

  if(firstIndex >= lastIndex)
    return false;

  if(value(firstIndex).isAlternate() || value(lastIndex).isAlternate())
    // First or last are alternate - cannot calculate
    return false;

  bool ok = true;

  int startIndexAfterProcedure = 0;
  if(departureProcToRouteLegs(startIndexAfterProcedure) && firstIndex == startIndexAfterProcedure - 1)
    // Index is on the last departure procedure leg - check for correct leg type
    ok &= proc::procedureLegFixAtEnd(value(startIndexAfterProcedure - 1).getProcedureLegType());
  else
    // Test if index is at or after first route leg or departure airport
    ok &= firstIndex >= getStartIndexAfterProcedure();

  int arrivaLegsOffset = 0;
  if(arrivalRouteToProcLegs(arrivaLegsOffset) && lastIndex == arrivaLegsOffset)
    // Index on first STAR or approach (transition) leg - check for correct leg type
    ok &= proc::procedureLegFixAtStart(value(arrivaLegsOffset).getProcedureLegType());
  else
    // Test if index is at or before first route leg
    ok &= lastIndex < getArrivaLegsOffset();

  return ok;
}

// Needs updateIndex called before
void Route::validateAirways()
{
  if(isEmpty())
    return;

  first().getFlightplanEntry()->setAirway(QString());
  first().setAirway(map::MapAirway());

  // Check arrival airway ===========================================================================
  int arrivaLegsOffset = map::INVALID_INDEX_VALUE;
  if(arrivalRouteToProcLegs(arrivaLegsOffset))
  {
    const RouteLeg& routeLeg = value(arrivaLegsOffset - 1);
    RouteLeg& arrivalLeg = (*this)[arrivaLegsOffset];

    QString airway = arrivalLeg.getAirwayName();
    if(airway.isEmpty())
      airway = flightplan.getProperties().value(atools::fs::pln::PROCAIRWAY);
    if(NavApp::getAirwayTrackQuery()->hasAirwayForNameAndWaypoint(airway, routeLeg.getIdent(), arrivalLeg.getIdent()))
    {
      // Airway is valid - set into procedure leg and property
      atools::fs::pln::FlightplanEntry *entry = arrivalLeg.getFlightplanEntry();
      if(entry != nullptr)
      {
        entry->setAirway(airway);
        if(airway.isEmpty())
          entry->setFlag(atools::fs::pln::entry::TRACK, false);
      }
      else
        qDebug() << Q_FUNC_INFO << "Entry is null";
      flightplan.getProperties().insert(atools::fs::pln::PROCAIRWAY, airway);
    }
    else
    {
      // Airway not valid between legs - erase
      atools::fs::pln::FlightplanEntry *entry = arrivalLeg.getFlightplanEntry();
      if(entry != nullptr)
      {
        entry->setAirway(QString());
        entry->setFlag(atools::fs::pln::entry::TRACK, false);
      }
      else
        qDebug() << Q_FUNC_INFO << "Entry is null";
      flightplan.getProperties().remove(atools::fs::pln::PROCAIRWAY);
    }
  }

  // Check departure airway ===========================================================================
  int startIndexAfterProcedure = map::INVALID_INDEX_VALUE;
  if(departureProcToRouteLegs(startIndexAfterProcedure))
  {
    // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
    const RouteLeg& departureLeg = value(startIndexAfterProcedure - 1);
    const RouteLeg& routeLeg = value(startIndexAfterProcedure);

    if(!NavApp::getAirwayTrackQuery()->hasAirwayForNameAndWaypoint(routeLeg.getAirwayName(), departureLeg.getIdent(),
                                                                   routeLeg.getIdent()))
      // Airway not valid for changed waypoints - erase
      flightplan.getEntries()[startIndexAfterProcedure].setAirway(QString());
  }
}

// Called after route calculation
void Route::removeDuplicateRouteLegs()
{
  if(isEmpty())
    return;

  // Check arrival ===========================================================================
  // remove duplicates between start of STAR, approach or transition and
  // route: only legs that have the navaid at the start of the leg - currently only initial fix
  int arrivaLegsOffset = map::INVALID_INDEX_VALUE;
  bool changed = false;
  if(arrivalRouteToProcLegs(arrivaLegsOffset))
  {
    const RouteLeg& routeLeg = value(arrivaLegsOffset - 1);
    RouteLeg& arrivalLeg = (*this)[arrivaLegsOffset];

    if(proc::procedureLegFixAtStart(arrivalLeg.getProcedureLegType()) && arrivalLeg.isNavaidEqualTo(routeLeg))
    {
      qDebug() << "removing duplicate leg at" << (arrivaLegsOffset - 1) << routeLeg;

      // Copy airway name from the route leg to be deleted into the first procedure leg
      flightplan.getEntries()[arrivaLegsOffset].setAirway(flightplan.getEntries().at(arrivaLegsOffset - 1).getAirway());

      // Remove the route leg before the procedure
      removeAllAt(arrivaLegsOffset - 1);
      changed = true;
    }
  }

  if(changed)
    updateIndicesAndOffsets();
  changed = false;

  // Check departure  ===========================================================================
  int startIndexAfterProcedure = map::INVALID_INDEX_VALUE;
  if(departureProcToRouteLegs(startIndexAfterProcedure))
  {
    // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
    const RouteLeg& departureLeg = value(startIndexAfterProcedure - 1);
    const RouteLeg& routeLeg = value(startIndexAfterProcedure);

    if(proc::procedureLegFixAtEnd(departureLeg.getProcedureLegType()) && departureLeg.isNavaidEqualTo(routeLeg))
    {
      qDebug() << "removing duplicate leg at" << startIndexAfterProcedure << routeLeg;

      // Remove the route leg after the procedure
      removeAllAt(startIndexAfterProcedure);
      changed = true;
    }
  }

  if(changed)
    updateIndicesAndOffsets();
  changed = false;

  // Remove all other duplicates from end to begin ============================
  for(int i = size() - 1; i > 0; i--)
  {
    const RouteLeg& leg1 = value(i - 1);
    const RouteLeg& leg2 = value(i);
    if(leg1.isRoute() && leg2.isRoute() && leg1.isNavaidEqualTo(leg2))
    {
      removeAllAt(i);
      changed = true;
    }
  }

  if(changed)
    updateIndicesAndOffsets();
}

const Pos& Route::getPositionAt(int i) const
{
  return value(i).getPosition();
}

const Pos& Route::getPrevPositionAt(int i) const
{
  if(i > 0)
  {
    if(value(i).isAlternate())
      return getDestinationAirportLeg().getPosition();
    else
      return value(i - 1).getPosition();
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
      qWarning() << "Entry for ident" << flightplan.at(i).getIdent()
                 << "region" << flightplan.at(i).getRegion() << "is not valid";

    append(leg);
    lastLeg = &last();
  }
}

void Route::assignAltitudes()
{
  QVector<float> altVector = altitude->getAltitudes();
  for(int i = 0; i < size(); i++)
    flightplan.getEntries()[i].setAltitude(altVector.at(i));
}

Route Route::adjustedToOptions(rf::RouteAdjustOptions options) const
{
  return adjustedToOptions(*this, options);
}

Route Route::adjustedToOptions(const Route& routeParam, rf::RouteAdjustOptions options)
{
  qDebug() << Q_FUNC_INFO << "options" << options;
  bool saveApproachWp = options.testFlag(rf::SAVE_APPROACH_WP), saveSidStarWp = options.testFlag(rf::SAVE_SIDSTAR_WP),
       replaceCustomWp = options.testFlag(rf::REPLACE_CUSTOM_WP);

  // Create copy which allows to modify the plan ==============
  Route route(routeParam);
  atools::fs::pln::Flightplan& plan = route.getFlightplan();
  atools::fs::pln::FlightplanEntryListType& entries = plan.getEntries();
  FlightplanEntryBuilder entryBuilder;

  // Copy flight plan profile altitudes into entries for FMS and other formats
  // All following functions have to use setCoords instead of setPosition to avoid overwriting
  route.assignAltitudes();

  // Restore duplicate waypoints at route/procedure entry/exits which were removed after route calculation
  if(options.testFlag(rf::FIX_PROC_ENTRY_EXIT))
  {
    // Check arrival airway ===========================================================================
    int arrivaLegsOffset = map::INVALID_INDEX_VALUE;
    if(route.arrivalRouteToProcLegs(arrivaLegsOffset))
    {
      const RouteLeg& routeLeg = route.value(arrivaLegsOffset - 1);
      RouteLeg& arrivalLeg = route[arrivaLegsOffset];

      QString airway = arrivalLeg.getAirwayName();
      if(airway.isEmpty())
        airway = plan.getProperties().value(atools::fs::pln::PROCAIRWAY);

      if(!airway.isEmpty() && proc::procedureLegFixAtStart(arrivalLeg.getProcedureLegType()) &&
         !arrivalLeg.isNavaidEqualTo(routeLeg))
      {
        FlightplanEntry entry;
        entryBuilder.buildFlightplanEntry(arrivalLeg.getProcedureLeg(), entry,
                                          true /* resolve waypoints to VOR and others*/);
        entry.setAirway(arrivalLeg.getAirwayName());
        entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);

        RouteLeg newLeg = RouteLeg(&route.flightplan);
        newLeg.createCopyFromProcedureLeg(arrivaLegsOffset, arrivalLeg, &routeLeg);
        newLeg.setAirway(arrivalLeg.getAirway());

        route.insert(arrivaLegsOffset, newLeg);
        entries.insert(arrivaLegsOffset, entry);
        plan.getProperties().remove(atools::fs::pln::PROCAIRWAY);
        route.updateIndicesAndOffsets();
      }
    }

    // Check departure airway ===========================================================================
    int startIndexAfterProcedure = map::INVALID_INDEX_VALUE;
    if(route.departureProcToRouteLegs(startIndexAfterProcedure))
    {
      // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
      const RouteLeg& departureLeg = route.value(startIndexAfterProcedure - 1);
      const RouteLeg& routeLeg = route.value(startIndexAfterProcedure);

      if(!routeLeg.getAirwayName().isEmpty() && proc::procedureLegFixAtEnd(departureLeg.getProcedureLegType()) &&
         !departureLeg.isNavaidEqualTo(routeLeg))
      {
        FlightplanEntry entry;
        entryBuilder.buildFlightplanEntry(departureLeg.getProcedureLeg(), entry,
                                          true /* resolve waypoints to VOR and others*/);
        entry.setAirway(QString());
        entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);

        RouteLeg newLeg = RouteLeg(&route.flightplan);
        newLeg.createCopyFromProcedureLeg(startIndexAfterProcedure, departureLeg, &routeLeg);
        newLeg.setAirway(map::MapAirway());

        route.insert(startIndexAfterProcedure, newLeg);
        entries.insert(startIndexAfterProcedure, entry);
        route.updateIndicesAndOffsets();
      }
    }
  }

  if(options.testFlag(rf::REMOVE_ALTERNATE))
  {
    route.removeAlternateLegs();
    route.updateIndicesAndOffsets();
  }

  if(route.getApproachLegs().isCustom() && replaceCustomWp)
    saveApproachWp = true;

  // First remove properties and procedure structures if needed ====================================
  if(saveApproachWp)
  {
    route.clearProcedures(proc::PROCEDURE_ARRIVAL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_ARRIVAL);
    route.clearProcedureLegs(proc::PROCEDURE_MISSED);
    route.updateIndicesAndOffsets();
  }

  if(saveSidStarWp)
  {
    route.clearProcedures(proc::PROCEDURE_SID_STAR_ALL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_SID_STAR_ALL);
    route.updateIndicesAndOffsets();
  }

  // Remove track names from airway field but keep waypoints =========================================
  if(options.testFlag(rf::REMOVE_TRACKS))
  {
    for(int i = 0; i < entries.size(); i++)
    {
      if(route.value(i).isTrack())
      {
        route[i].clearAirwayOrTrack();
        entries[i].setAirway(QString());
      }
    }
  }

  // Remove airway names but keep waypoints =========================================
  if(options.testFlag(rf::SAVE_AIRWAY_WP))
  {
    for(int i = 0; i < entries.size(); i++)
    {
      if(route.value(i).isAirway())
      {
        route[i].clearAirwayOrTrack();
        entries[i].setAirway(QString());
      }
    }
    plan.getProperties().remove(atools::fs::pln::PROCAIRWAY);
  }

  if(saveApproachWp || saveSidStarWp)
  {
    // Replace procedures with waypoints =======================================================================
    // Now replace all entries with either valid waypoints or user defined waypoints
    for(int i = 0; i < entries.size(); i++)
    {
      FlightplanEntry& entry = entries[i];
      const RouteLeg& leg = route.value(i);

      if((saveApproachWp && (leg.getProcedureType() & proc::PROCEDURE_ARRIVAL)) ||
         (saveSidStarWp && (leg.getProcedureType() & proc::PROCEDURE_SID_STAR_ALL)))
      {
        // Entry is one of the category which have to be replaced
        entry.setIdent(QString());
        entry.setRegion(QString());
        entry.setCoords(leg.getPosition());
        entry.setAirway(QString());
        entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);
        entry.setFlag(atools::fs::pln::entry::TRACK, false);

        if(leg.getWaypoint().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::WAYPOINT);
          entry.setIdent(leg.getWaypoint().ident);
          entry.setRegion(leg.getWaypoint().region);
        }
        else if(leg.getVor().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::VOR);
          entry.setIdent(leg.getVor().ident);
          entry.setRegion(leg.getVor().region);
        }
        else if(leg.getNdb().isValid())
        {
          entry.setWaypointType(atools::fs::pln::entry::NDB);
          entry.setIdent(leg.getNdb().ident);
          entry.setRegion(leg.getNdb().region);
        }
        else if(leg.getRunwayEnd().isValid())
        {
          // Make a user defined waypoint from a runway end
          entry.setWaypointType(atools::fs::pln::entry::USER);
          entry.setIdent("RW" + leg.getRunwayEnd().name);
        }
        else
        {
          // Make a user defined waypoint from manual, altitude or other points
          entry.setWaypointType(atools::fs::pln::entry::USER);

          if((replaceCustomWp || saveApproachWp) &&
             route.getApproachLegs().isCustom() && leg.getProcedureType() & proc::PROCEDURE_APPROACH)
            entry.setIdent(leg.getProcedureLeg().fixIdent);
          else
            entry.setIdent(atools::fs::util::adjustFsxUserWpName(leg.getProcedureLeg().displayText.join(" ")));
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

      if(entry.getIdent() == prev.getIdent() &&
         entry.getRegion() == prev.getRegion() &&
         entry.getPosition().almostEqual(prev.getPosition(), Pos::POS_EPSILON_100M))
        entries.removeAt(i);
    }

    // Copy flight plan entries to route legs - will also add coordinates
    route.createRouteLegsFromFlightplan();
    route.updateAll();

    // Airways are updated in route controller
  }

  if(options.testFlag(rf::FIX_CIRCLETOLAND))
  {
    // ========================================================================
    // Check for a circle-to-land approach without runway - add a random (best) runway from the airport to
    // satisfy the X-Plane GPS/FMC/G1000
    if(route.getDestinationAirportLeg().getAirport().isValid() &&
       plan.getProperties().value(atools::fs::pln::APPROACHRW).isEmpty() &&
       (!plan.getProperties().value(atools::fs::pln::APPROACH).isEmpty() ||
        !plan.getProperties().value(atools::fs::pln::APPROACH_ARINC).isEmpty()))
    {
      // Get best runway - longest with probably hard surface
      const QList<map::MapRunway> *runways = NavApp::getAirportQuerySim()->
                                             getRunways(route.getDestinationAirportLeg().getId());
      if(runways != nullptr && !runways->isEmpty())
        plan.getProperties().insert(atools::fs::pln::APPROACHRW, runways->last().primaryName);
    }
  }

  return route;
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
void Route::updateAirwaysAndAltitude(bool adjustRouteAltitude)
{
  if(isEmpty())
    return;

  bool hasAirway = false;
  int minAltitude = 0, maxAltitude = 100000;
  for(int i = 1; i < size(); i++)
  {
    RouteLeg& routeLeg = (*this)[i];
    const RouteLeg& prevLeg = value(i - 1);

    if(!routeLeg.getAirwayName().isEmpty())
    {
      if(i == 1 && prevLeg.getMapObjectType() == map::AIRPORT)
      {
        routeLeg.setAirway(map::MapAirway());
        routeLeg.getFlightplanEntry()->setAirway(QString());
        routeLeg.getFlightplanEntry()->setFlag(atools::fs::pln::entry::TRACK, false);
        continue;
      }

      QList<map::MapAirway> airways;
      NavApp::getAirwayTrackQuery()->getAirwaysByNameAndWaypoint(airways, routeLeg.getAirwayName(), prevLeg.getIdent(),
                                                                 routeLeg.getIdent());
      map::MapAirway airway = airways.value(0);
      routeLeg.setAirway(airway);
      minAltitude = std::max(airway.minAltitude, minAltitude);
      if(airway.maxAltitude > 0)
        maxAltitude = std::min(airway.maxAltitude, maxAltitude);

      hasAirway |= !routeLeg.getAirwayName().isEmpty();
    }
    else
      routeLeg.setAirway(map::MapAirway());
  }

  // Convert feet to local unit
  minAltitude = Unit::altFeetI(minAltitude);
  maxAltitude = Unit::altFeetI(maxAltitude);

  if(adjustRouteAltitude)
  {
    // Check airway limits after calculation ===========================

    // Add 500 ft/m for VFR
    float offset = flightplan.getFlightplanType() == atools::fs::pln::IFR ? 0.f : 500.f;
    int cruisingAltitude = flightplan.getCruisingAltitude();

    // First round up to next rule adhering altitude independent of flight direction
    // cruisingAltitude = static_cast<int>(std::ceil((cruisingAltitude - offset) / 1000.f) * 1000.f + offset);

    if(cruisingAltitude < minAltitude)
      // Below min altitude - use min altitude and round up to next valid level
      cruisingAltitude = static_cast<int>(std::ceil((minAltitude - offset) / 1000.f) * 1000.f + offset);

    if(cruisingAltitude > maxAltitude)
      // Above max altitude - use max altitude and round down
      cruisingAltitude = static_cast<int>(std::floor((maxAltitude - offset) / 1000.f) * 1000.f + offset);

    // Adjust altitude after route calculation ===========================
    if(OptionData::instance().getFlags() & opts::ROUTE_ALTITUDE_RULE)
    {
      // Apply simplified east/west or other rule - always rounds up =============================
      int adjusted = getAdjustedAltitude(cruisingAltitude);

      if(adjusted > maxAltitude)
        adjusted = getAdjustedAltitude(cruisingAltitude - 1000);
      cruisingAltitude = adjusted;
    }

    qDebug() << Q_FUNC_INFO << "Updating flight plan altitude"
             << "minAltitude" << minAltitude << "maxAltitude" << maxAltitude << "cruisingAltitude" << cruisingAltitude;

    flightplan.setCruisingAltitude(cruisingAltitude);
  }
}

/* Apply simplified east/west or north/south rule */
int Route::getAdjustedAltitude(int newAltitude) const
{
  if(getSizeWithoutAlternates() > 1)
  {
    const Pos& departurePos = first().getPosition();
    const Pos& destinationPos = getDestinationAirportLeg().getPosition();

    float magvar = (first().getMagvar() + getDestinationAirportLeg().getMagVarBySettings()) / 2;

    float fpDir = atools::geo::normalizeCourse(departurePos.angleDegToRhumb(destinationPos) - magvar);

    if(fpDir < Pos::INVALID_VALUE)
    {
      qDebug() << Q_FUNC_INFO << "minAltitude" << newAltitude << "fp dir" << fpDir;

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
        // IFR - 1000
        if(odd)
          // round up to the next odd value
          newAltitude = static_cast<int>(std::ceil((newAltitude - 1000.f) / 2000.f) * 2000.f + 1000.f);
        else
          // round up to the next even value
          newAltitude = static_cast<int>(std::ceil(newAltitude / 2000.f) * 2000.f);
      }
      else
      {
        // VFR - 500
        if(odd)
          // round up to the next odd value + 500
          newAltitude = static_cast<int>(std::ceil((newAltitude - 1500.f) / 2000.f) * 2000.f + 1500.f);
        else
          // round up to the next even value + 500
          newAltitude = static_cast<int>(std::ceil((newAltitude - 500.f) / 2000.f) * 2000.f + 500.f);
      }

      qDebug() << "corrected minAltitude" << newAltitude;
    }
  }
  return newAltitude;
}

void Route::getApproachRunwayEndAndIls(QVector<map::MapIls>& ils, map::MapRunwayEnd *runwayEnd) const
{
  if(runwayEnd != nullptr)
    *runwayEnd = approachLegs.runwayEnd;

  ils.clear();
  if(approachLegs.runwayEnd.isValid())
  {
    QString destIdent = getDestinationAirportLeg().getIdent();
    // Get one or more ILS from flight plan leg as is
    ils = NavApp::getMapQuery()->getIlsByAirportAndRunway(destIdent, approachLegs.runwayEnd.name);

    if(ils.isEmpty())
    {
      // ILS does not even match runway - try fuzzy
      QStringList variants = map::runwayNameVariants(approachLegs.runwayEnd.name);
      for(const QString& runwayVariant : variants)
        ils.append(NavApp::getMapQuery()->getIlsByAirportAndRunway(destIdent, runwayVariant));
    }

    if(ils.size() > 1)
    {
      for(const proc::MapProcedureLeg& leg : approachLegs.approachLegs)
      {
        for(map::MapIls i : ils)
        {
          if(leg.recFixIdent == i.ident)
          {
            ils.clear();
            ils.append(i);
          }
        }
      }
    }
  }
}

bool Route::isTooFarToFlightPlan() const
{
  return getDistanceToFlightPlan() < map::INVALID_DISTANCE_VALUE &&
         getDistanceToFlightPlan() > MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM;
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
               arg(approachLegs.approachType).
               arg(approachLegs.approachFixIdent).
               arg(approachLegs.approachSuffix.isEmpty() ? QString() : (tr("-") + approachLegs.approachSuffix));
  }
  else if(mapType & proc::PROCEDURE_TRANSITION)
    procText = QObject::tr("Transition %1").arg(approachLegs.transitionFixIdent);
  else if(mapType & proc::PROCEDURE_STAR)
    procText = QObject::tr("STAR %1").arg(starLegs.approachFixIdent);
  else if(mapType & proc::PROCEDURE_SID)
    procText = QObject::tr("SID %1").arg(sidLegs.approachFixIdent);
  else if(mapType & proc::PROCEDURE_SID_TRANSITION)
    procText = QObject::tr("SID Transition %1").arg(sidLegs.transitionFixIdent);
  else if(mapType & proc::PROCEDURE_STAR_TRANSITION)
    procText = QObject::tr("STAR Transition %1").arg(starLegs.transitionFixIdent);

  return procText;
}

QDebug operator<<(QDebug out, const Route& route)
{
  out << endl << "Route ==================================================================" << endl;
  out << route.getFlightplan().getProperties() << endl << endl;
  for(int i = 0; i < route.size(); ++i)
    out << "===" << i << route.value(i) << endl;
  out << endl << "Departure ===========" << endl;
  out << "offset" << route.getDepartureAirportLegIndex();
  out << route.getDepartureAirportLeg() << endl;
  out << endl << "Destination ===========" << endl;
  out << "offset" << route.getDestinationAirportLegIndex();
  out << route.getDestinationAirportLeg() << endl;
  out << endl << "Departure Procedure ===========" << endl;
  out << "offset" << route.getSidLegsOffset();
  out << route.getSidLegs() << endl;
  out << "STAR Procedure ===========" << endl;
  out << "offset" << route.getStarLegsOffset() << endl;
  out << route.getStarLegs() << endl;
  out << "Arrival Procedure ========" << endl;
  out << "offset" << route.getApproachLegsOffset() << endl;
  out << route.getApproachLegs() << endl;
  out << "Alternates ========" << endl;
  out << "offset" << route.getAlternateLegsOffset() << "num" << route.getNumAlternateLegs() << endl;
  out << "==================================================================" << endl;

  out << endl << "RouteAltitudeLegs ==================================================================" << endl;
  out << route.getAltitudeLegs();
  out << "==================================================================" << endl;

  return out;
}
