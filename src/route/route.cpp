/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "route/flightplanentrybuilder.h"
#include "query/procedurequery.h"
#include "query/airportquery.h"
#include "navapp.h"
#include "fs/util/fsutil.h"

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

Route::Route()
{
  resetActive();
}

Route::Route(const Route& other)
  : QList<RouteLeg>(other)
{
  copy(other);
}

Route::~Route()
{

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
  activeLeg = map::INVALID_INDEX_VALUE;
}

void Route::copy(const Route& other)
{
  clear();
  append(other);

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

  activeLeg = other.activeLeg;
  activeLegResult = other.activeLegResult;

  // Update flightplan pointers to this instance
  for(RouteLeg& routeLeg : *this)
    routeLeg.setFlightplan(&flightplan);

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

  return true;
}

bool Route::canEditPoint(int index) const
{
  return at(index).isRoute();
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
    activeLeg = map::INVALID_INDEX_VALUE;
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

  if(activeLeg == map::INVALID_INDEX_VALUE)
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

  // qDebug() << "activePos" << activeLegResult;

  // Get potential next leg and course difference
  int nextLeg = activeLeg + 1;
  float courseDiff = 0.f;
  if(nextLeg < size())
  {
    // Catch the case of initial fixes or others that are points instead of lines and try the next legs
    if(!at(activeLeg).getProcedureLeg().isHold())
    {
      while(atools::contains(at(nextLeg).getProcedureLegType(),
                             {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) &&
            getPositionAt(nextLeg - 1) == getPositionAt(nextLeg) &&
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
    Pos pos1 = getPositionAt(nextLeg - 1);
    Pos pos2 = getPositionAt(nextLeg);

    // Calculate course difference
    float legCrs = normalizeCourse(pos1.angleDegTo(pos2));
    courseDiff = atools::mod(pos.course - legCrs + 360.f, 360.f);
    if(courseDiff > 180.f)
      courseDiff = 360.f - courseDiff;

    // qDebug() << "ACTIVE" << at(activeLeg);
    // qDebug() << "NEXT" << at(nextLeg);

    // Test next leg
    atools::geo::LineDistance nextLegResult;
    activePos.pos.distanceMeterToLine(pos1, pos2, nextLegResult);
    // qDebug() << "NEXT" << nextLeg << nextLegResult;

    bool switchToNextLeg = false;
    if(at(activeLeg).getProcedureLeg().isHold())
    {
      // qDebug() << "ACTIVE HOLD";
      // Test next leg if we can exit a hold
      if(at(nextLeg).getProcedureLeg().line.getPos1() == at(activeLeg).getPosition())
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
        at(activeLeg).getProcedureLeg().holdLine.distanceMeterToLine(activePos.pos, resultHold);
        // qDebug() << "NEXT HOLD" << nextLeg << resultHold;

        // qDebug() << "HOLD DIFFER";
        // qDebug() << "resultHold" << resultHold;
        // qDebug() << "holdLine" << at(activeLeg).getProcedureLeg().holdLine;
        // Hold point differs from next leg start - use the helping line
        if(resultHold.status == atools::geo::ALONG_TRACK) // Check if we are outside of the hold
        {
          if(at(activeLeg).getProcedureLeg().turnDirection == "R")
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
      else if(at(activeLeg).getProcedureLegType() == proc::PROCEDURE_TURN)
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

    if(isAirportAfterArrival(nextLeg))
      // Do not sequence after arrival or after end of missed
      switchToNextLeg = false;

    if(switchToNextLeg)
    {
      // qDebug() << "Switching to next leg";
      // Either left current leg or closer to next and on courses
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes& map::MISSED_APPROACH) && at(nextLeg).getProcedureLeg().isMissed()))
      {
        // Go to next leg and increase all values
        activeLeg = nextLeg;
        pos.pos.distanceMeterToLine(getPositionAt(activeLeg - 1), getPositionAt(activeLeg), activeLegResult);
      }
    }
  }
  // qDebug() << "active" << activeLeg << "size" << size() << "ident" << at(activeLeg).getIdent() <<
  // maptypes::approachLegTypeFullStr(at(activeLeg).getProcedureLeg().type);
}

bool Route::getRouteDistances(float *distFromStart, float *distToDest,
                              float *nextLegDistance, float *crossTrackDistance) const
{
  const proc::MapProcedureLeg *geometryLeg = nullptr;

  if(activeLeg == map::INVALID_INDEX_VALUE)
    return false;

  int active = adjustedActiveLeg();

  if(at(active).isAnyProcedure() && (at(active).getGeometry().size() > 2))
    // Use arc or intercept geometry to calculate distance
    geometryLeg = &at(active).getProcedureLeg();

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

    bool activeIsMissed = at(active).getProcedureLeg().isMissed();

    // Ignore missed approach legs until the active is a missed approach leg
    if(!at(routeIndex).getProcedureLeg().isMissed() || activeIsMissed)
    {
      if(geometryLeg != nullptr)
      {
        atools::geo::LineDistance result;
        geometryLeg->geometry.distanceMeterToLineString(activePos.pos, result);
        distToCurrent = meterToNm(result.distanceFrom2);
      }
      else
        distToCurrent = meterToNm(getPositionAt(routeIndex).distanceMeterTo(activePos.pos));
    }

    if(nextLegDistance != nullptr)
      *nextLegDistance = distToCurrent;

    // Sum up all distances along the legs
    // Ignore missed approach legs until the active is a missedd approach leg
    float fromstart = 0.f;
    for(int i = 0; i <= routeIndex; i++)
    {
      if(!at(i).getProcedureLeg().isMissed() || activeIsMissed)
        fromstart += at(i).getDistanceTo();
      else
        break;
    }
    fromstart -= distToCurrent;
    fromstart = std::abs(fromstart);

    if(distFromStart != nullptr)
      *distFromStart = std::max(fromstart, 0.f);

    if(distToDest != nullptr)
    {
      if(size() == 1)
        *distToDest = meterToNm(activePos.pos.distanceMeterTo(first().getPosition()));
      else
      {
        if(!activeIsMissed)
          *distToDest = std::max(totalDistance - fromstart, 0.f);
        else
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
      }
    }

    return true;
  }
  return false;
}

float Route::getDistanceFromStart(const atools::geo::Pos& pos) const
{
  atools::geo::LineDistance result;
  int leg = getNearestRouteLegResult(pos, result, false /* ignoreNotEditable */);
  float distFromStart = map::INVALID_DISTANCE_VALUE;

  if(leg < map::INVALID_INDEX_VALUE && result.status == atools::geo::ALONG_TRACK)
  {
    float fromstart = 0.f;
    for(int i = 1; i < leg; i++)
    {
      if(!at(i).getProcedureLeg().isMissed())
        fromstart += nmToMeter(at(i).getDistanceTo());
      else
        break;
    }
    fromstart += result.distanceFrom1;
    fromstart = std::abs(fromstart);

    distFromStart = std::max(fromstart, 0.f);
  }
  return meterToNm(distFromStart);
}

float Route::getTopOfDescentFromStart() const
{
  if(!isEmpty())
    return getTotalDistance() - getTopOfDescentFromDestination();

  return 0.f;
}

float Route::getDescentVerticalAltitude(float currentDistToDest) const
{
  if(hasValidDestination())
  {
    float destAlt = last().getAirport().position.getAltitude();
    return (getCruisingAltitudeFeet() - destAlt) *
           currentDistToDest /
           (getTopOfDescentFromDestination()) + destAlt;
  }

  return map::INVALID_ALTITUDE_VALUE;
}

float Route::getCruisingAltitudeFeet() const
{
  return Unit::rev(getFlightplan().getCruisingAltitude(), Unit::altFeetF);
}

float Route::getTopOfDescentFromDestination() const
{
  if(!isEmpty())
  {
    float cruisingAltitude = getCruisingAltitudeFeet();

    float diff = (cruisingAltitude - last().getPosition().getAltitude());

    // Either nm per 1000 something alt or km per 1000 something alt
    float distNm = Unit::rev(OptionData::instance().getRouteTodRule(), Unit::distNmF);
    float altFt = Unit::rev(1000.f, Unit::altFeetF);

    return std::min(diff / altFt * distNm, totalDistance);
  }
  return 0.f;
}

atools::geo::Pos Route::getTopOfDescent() const
{
  if(!isEmpty())
    return positionAtDistance(getTopOfDescentFromStart());

  return atools::geo::EMPTY_POS;
}

atools::geo::Pos Route::positionAtDistance(float distFromStartNm) const
{
  if(distFromStartNm < 0.f || distFromStartNm > totalDistance)
    return atools::geo::EMPTY_POS;

  atools::geo::Pos retval;

  // Find the leg that contains the given distance point
  float total = 0.f;
  int foundIndex = map::INVALID_INDEX_VALUE; // Found leg is from this index to index + 1
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
      retval = getPositionAt(foundIndex - 1).interpolate(getPositionAt(foundIndex), fraction);
    }
  }

  return retval;
}

void Route::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                       map::MapSearchResult& mapobjects, QList<proc::MapProcedurePoint>& procPoints,
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

      if(leg.isAnyProcedure())
        procPoints.append(proc::MapProcedurePoint(leg.getProcedureLeg()));
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
  return !getFlightplan().isEmpty() &&
         getFlightplan().getEntries().last().getWaypointType() == atools::fs::pln::entry::AIRPORT &&
         last().isValid();
}

bool Route::hasEntries() const
{
  return getFlightplan().getEntries().size() > 2;
}

bool Route::canCalcRoute() const
{
  return getFlightplan().getEntries().size() >= 2;
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
}

void Route::clearFlightplanProcedureProperties(proc::MapProcedureTypes type)
{
  ProcedureQuery::clearFlightplanProcedureProperties(flightplan.getProperties(), type);
}

void Route::updateProcedureLegs(FlightplanEntryBuilder *entryBuilder)
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
    obj.createFromApproachLeg(i, departureLegs, &at(i));
    insert(insertIndex, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departureLegs.at(insertIndex - 1), entry, true);
    entries.insert(insertIndex, entry);
  }

  // Create route legs and flight plan entries from STAR
  if(!starLegs.isEmpty())
    starLegsOffset = size() - 1;

  for(int i = 0; i < starLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &at(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromApproachLeg(i, starLegs, prev);
    insert(size() - 1, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(starLegs.at(i), entry, true);
    entries.insert(entries.size() - 1, entry);
  }

  // Create route legs and flight plan entries from arrival
  if(!arrivalLegs.isEmpty())
    arrivalLegsOffset = size() - 1;

  for(int i = 0; i < arrivalLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &at(size() - 2) : nullptr;

    RouteLeg obj(&flightplan);
    obj.createFromApproachLeg(i, arrivalLegs, prev);
    insert(size() - 1, obj);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(arrivalLegs.at(i), entry, true);
    entries.insert(entries.size() - 1, entry);
  }

  // Leave procedure information in the PLN file
  clearFlightplanProcedureProperties(proc::PROCEDURE_ALL);

  ProcedureQuery::fillFlightplanProcedureProperties(flightplan.getProperties(), arrivalLegs, starLegs, departureLegs);
}

void Route::removeRouteLegs()
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = size() - 2; i > 0; i--)
  {
    const RouteLeg& routeLeg = at(i);
    if(routeLeg.isRoute())
      indexes.append(i);
  }

  // Delete in route legs and flight plan from the end
  for(int i = 0; i < indexes.size(); i++)
  {
    removeAt(indexes.at(i));
    flightplan.getEntries().removeAt(indexes.at(i));
  }
}

void Route::clearProcedureLegs(proc::MapProcedureTypes type)
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
    removeAt(indexes.at(i));
    flightplan.getEntries().removeAt(indexes.at(i));
  }
}

void Route::updateAll()
{
  updateIndicesAndOffsets();
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
  int retval = activeLeg;
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
  activeLeg = adjustedActiveLeg();
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
  if(activeLeg != map::INVALID_INDEX_VALUE)
    return &at(activeLeg);
  else
    return nullptr;
}

int Route::getActiveLegIndexCorrected(bool *corrected) const
{
  if(activeLeg == map::INVALID_INDEX_VALUE)
    return map::INVALID_INDEX_VALUE;

  int nextLeg = activeLeg + 1;
  if(nextLeg < size() && nextLeg == size() &&
     at(nextLeg).isAnyProcedure()
     /*&&
      *  (at(nextLeg).getProcedureLegType() == proc::INITIAL_FIX || at(nextLeg).getProcedureLeg().isHold())*/)
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

bool Route::isActiveMissed() const
{
  const RouteLeg *leg = getActiveLeg();
  if(leg != nullptr)
    return leg->getProcedureLeg().isMissed();
  else
    return false;
}

bool Route::isPassedLastLeg() const
{
  if((activeLeg >= size() - 1 || (activeLeg + 1 < size() && at(activeLeg + 1).getProcedureLeg().isMissed())) &&
     activeLegResult.status == atools::geo::AFTER_END)
    return true;

  return false;
}

void Route::setActiveLeg(int value)
{
  if(value > 0 && value < size())
    activeLeg = value;
  else
    activeLeg = 1;

  activePos.pos.distanceMeterToLine(at(activeLeg - 1).getPosition(), at(activeLeg).getPosition(), activeLegResult);
}

bool Route::isAirportAfterArrival(int index)
{
  return (hasAnyArrivalProcedure() /*|| hasStarProcedure()*/) &&
         index == size() - 1 && at(index).getMapObjectType() == map::AIRPORT;
}

void Route::updateDistancesAndCourse()
{
  totalDistance = 0.f;
  RouteLeg *last = nullptr;
  for(int i = 0; i < size(); i++)
  {
    if(isAirportAfterArrival(i))
      break;

    RouteLeg& leg = (*this)[i];
    leg.updateDistanceAndCourse(i, last);
    if(!leg.getProcedureLeg().isMissed())
      totalDistance += leg.getDistanceTo();
    last = &leg;
  }
}

void Route::updateMagvar()
{
  // get magvar from internal database objects (waypoints, VOR and others)
  for(int i = 0; i < size(); i++)
    (*this)[i].updateMagvar();
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
    pos.pos.distanceMeterToLine(getPositionAt(i - 1), getPositionAt(i), result);
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

    pos.distanceMeterToLine(getPositionAt(i - 1), getPositionAt(i), result);

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

bool Route::isAirportDeparture(const QString& ident) const
{
  return !isEmpty() && first().getAirport().isValid() && first().getAirport().ident == ident;
}

bool Route::isAirportDestination(const QString& ident) const
{
  return !isEmpty() && last().getAirport().isValid() && last().getAirport().ident == ident;
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
    return size() - 1;
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

void Route::createRouteLegsFromFlightplan()
{
  clear();

  Flightplan& flightplan = getFlightplan();

  const RouteLeg *lastLeg = nullptr;

  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    RouteLeg mapobj(&flightplan);
    mapobj.createFromDatabaseByEntry(i, lastLeg);

    if(mapobj.getMapObjectType() == map::INVALID)
      // Not found in database
      qWarning() << "Entry for ident" << flightplan.at(i).getIcaoIdent()
                 << "region" << flightplan.at(i).getIcaoRegion() << "is not valid";

    append(mapobj);
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
      flightplan.setDestinationAiportName(last().getName());
    if(!flightplan.getDestinationPosition().isValid())
      flightplan.setDestinationPosition(first().getPosition());
  }
}

Route Route::adjustedToProcedureOptions(bool saveApproachWp, bool saveSidStarWp) const
{
  qDebug() << Q_FUNC_INFO << "saveApproachWp" << saveApproachWp << "saveSidStarWp" << saveSidStarWp;

  Route route(*this);

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

    // Now replace all entries with either valid waypoints or user defined waypoints
    for(int i = 0; i < fp.getEntries().size(); i++)
    {
      FlightplanEntry& entry = fp.getEntries()[i];
      const RouteLeg& leg = route.at(i);

      if((saveApproachWp && (leg.getProcedureType() & proc::PROCEDURE_ARRIVAL)) ||
         (saveSidStarWp && (leg.getProcedureType() & proc::PROCEDURE_SID_STAR_ALL)))
      {
        // Entry is one of the category which have to be replaced
        entry.setIcaoIdent(QString());
        entry.setIcaoRegion(QString());
        entry.setPosition(leg.getPosition());
        entry.setAirway(QString());
        entry.setNoSave(false);

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
    fp.removeNoSaveEntries();

    // Delete duplicates
    for(int i = fp.getEntries().size() - 1; i > 0; i--)
    {
      const FlightplanEntry& entry = fp.getEntries().at(i);
      const FlightplanEntry& prev = fp.getEntries().at(i - 1);

      if(entry.getWaypointId() == prev.getWaypointId() &&
         entry.getIcaoIdent() == prev.getIcaoIdent() &&
         entry.getIcaoRegion() == prev.getIcaoRegion() &&
         entry.getPosition().almostEqual(prev.getPosition(), Pos::POS_EPSILON_100M))
        fp.getEntries().removeAt(i);
    }

    // Copy flight plan entries to route legs
    route.createRouteLegsFromFlightplan();
    route.updateAll();

    // Airways are updated in route controller
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

      // qDebug() << "min" << airway.minAltitude << "max" << airway.maxAltitude;
    }
    else
      routeLeg.setAirway(map::MapAirway());
  }

  if(adjustRouteAltitude && minAltitude > 0 && !isEmpty())
  {
    if(OptionData::instance().getFlags() & opts::ROUTE_ALTITUDE_RULE)
      // Apply simplified east/west rule
      minAltitude = adjustAltitude(minAltitude);
    getFlightplan().setCruisingAltitude(minAltitude);

    qDebug() << Q_FUNC_INFO << "Updating flight plan altitude to" << minAltitude;

    if(getFlightplan().getCruisingAltitude() > Unit::altFeetF(20000))
      getFlightplan().setRouteType(atools::fs::pln::HIGH_ALTITUDE);
    else
      getFlightplan().setRouteType(atools::fs::pln::LOW_ALTITUDE);
  }
}

/* Apply simplified east/west or north/south rule */
int Route::adjustAltitude(int minAltitude) const
{
  if(size() > 1)
  {
    const Pos& departurePos = first().getPosition();
    const Pos& destinationPos = last().getPosition();

    float magvar = (first().getMagvar() + last().getMagvar()) / 2;

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

QDebug operator<<(QDebug out, const Route& route)
{
  out << "Route ======================" << endl;
  out << route.getFlightplan().getProperties() << endl;
  for(int i = 0; i < route.size(); ++i)
    out << i << route.at(i) << endl;
  out << "Departure ===========" << endl;
  out << "offset" << route.getDepartureLegsOffset() << endl;
  out << route.getDepartureLegs() << endl;
  out << "STAR ===========" << endl;
  out << "offset" << route.getStarLegsOffset() << endl;
  out << route.getStarLegs() << endl;
  out << "Arrival ========" << endl;
  out << "offset" << route.getArrivalLegsOffset() << endl;
  out << route.getArrivalLegs() << endl;
  out << "======================" << endl;

  return out;
}
