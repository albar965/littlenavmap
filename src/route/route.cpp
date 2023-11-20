/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "common/maptools.h"
#include "common/unit.h"
#include "fs/db/databasemeta.h"
#include "fs/perf/aircraftperf.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "geo/calculations.h"
#include "app/navapp.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "route/flightplanentrybuilder.h"
#include "route/routealtitude.h"

#include <QBitArray>
#include <QRegularExpression>
#include <QStringBuilder>

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
const float MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM = 40.f;

// Invalid leg as default value
const static RouteLeg EMPTY_ROUTELEG;

const static QRegularExpression USER_WP_ID("^WP([0-9]+)$");

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

void Route::updateAircraftPerfMetadata()
{
  QHash<QString, QString>& properties = flightplan.getProperties();
  properties.insert(atools::fs::pln::AIRCRAFT_PERF_NAME, NavApp::getCurrentAircraftPerfName());
  properties.insert(atools::fs::pln::AIRCRAFT_PERF_TYPE, NavApp::getCurrentAircraftPerfAircraftType());
  properties.insert(atools::fs::pln::AIRCRAFT_PERF_FILE, NavApp::getCurrentAircraftPerfFilepath());
}

void Route::updateRouteCycleMetadata()
{
  QHash<QString, QString>& properties = flightplan.getProperties();
  // Add metadata for navdata reference =========================
  properties.insert(atools::fs::pln::SIMDATA, NavApp::getDatabaseMetaSim()->getDataSource());
  properties.insert(atools::fs::pln::SIMDATA_CYCLE, NavApp::getDatabaseAiracCycleSim());
  properties.insert(atools::fs::pln::NAVDATA, NavApp::getDatabaseMetaNav()->getDataSource());
  properties.insert(atools::fs::pln::NAVDATA_CYCLE, NavApp::getDatabaseAiracCycleNav());
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

  destRunwayIlsMap = other.destRunwayIlsMap;
  destRunwayIlsProfile = other.destRunwayIlsProfile;
  destRunwayIlsFlightPlanTable = other.destRunwayIlsFlightPlanTable;
  destRunwayEnd = other.destRunwayEnd;

  // Update flightplan pointers to this instance
  for(RouteLeg& routeLeg : *this)
    routeLeg.setFlightplan(&flightplan);

  delete altitude;
  altitude = new RouteAltitude(this);
  *altitude = other.altitude->copy(this);
}

void Route::clearAll()
{
  removeProcedureLegs();
  removeAlternateLegs();
  getFlightplan().clearAll();
  resetActive();
  clear();

  altitude->clearAll();
  destRunwayIlsMap.clear();
  destRunwayIlsProfile.clear();
  destRunwayIlsFlightPlanTable.clear();
  destRunwayEnd = map::MapRunwayEnd();
  objectIndex.clear();

  totalDistance = 0.f;
}

int Route::getNextUserWaypointNumber() const
{
  /* Get number from user waypoint from user defined waypoint in fs flight plan */
  int nextNum = 0;

  for(const FlightplanEntry& entry : flightplan)
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::USER && entry.getIdent().startsWith("WP"))
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
  sid = sidLegs.procedureFixIdent;
  sidTrans = sidLegs.transitionFixIdent;
  star = starLegs.procedureFixIdent;
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

const QString& Route::getSidRunwayName() const
{
  return sidLegs.runwayEnd.name;
}

const QString& Route::getStarRunwayName() const
{
  return starLegs.runwayEnd.name;
}

const QString& Route::getApproachRunwayName() const
{
  return approachLegs.runwayEnd.name;
}

void Route::getOutboundCourse(int index, float& magCourse, float& trueCourse) const
{
  magCourse = trueCourse = map::INVALID_COURSE_VALUE;

  const RouteLeg& curLeg = value(index);
  if(curLeg.isValid())
  {
    // Course for first segment in geometry
    trueCourse = curLeg.getCourseStartTrue();
    if(trueCourse < map::INVALID_COURSE_VALUE)
    {
      const RouteLeg& lastLeg = value(index - 1);
      if(lastLeg.isValid())
      {
        float magvar = lastLeg.isCalibratedVor() ? lastLeg.getVor().magvar : lastLeg.getMagvarEnd();
        magCourse = normalizeCourse(trueCourse - magvar);
      }
    }
  }
}

void Route::getInboundCourse(int index, float& magCourse, float& trueCourse) const
{
  magCourse = trueCourse = map::INVALID_COURSE_VALUE;

  const RouteLeg& curLeg = value(index);
  if(curLeg.isValid())
  {
    // Course for last segment in geometry
    trueCourse = curLeg.getCourseEndTrue();
    if(trueCourse < map::INVALID_COURSE_VALUE)
    {
      float magvar = curLeg.getVor().isCalibratedVor() ? curLeg.getVor().magvar : curLeg.getMagvarEnd();
      magCourse = normalizeCourse(trueCourse - magvar);
    }
  }
}

QString Route::getFullApproachName() const
{
  QString approach, approachArincName, approachTransition, approachSuffixDummy;
  getApproachNames(approachArincName, approachTransition, approachSuffixDummy);

  if(!approachTransition.isEmpty())
    approach = approachTransition % '.' % approachArincName;
  else
    approach = approachArincName;

  if(!approach.isEmpty())
    approach.prepend('/');
  return approach;
}

void Route::getApproachNames(QString& approachArincName, QString& approachTransition, QString& approachSuffix) const
{
  if(hasAnyApproachProcedure())
  {
    approachArincName = approachLegs.arincName;
    approachTransition = approachLegs.transitionFixIdent;
    approachSuffix = approachLegs.suffix;
  }
  else
  {
    approachArincName.clear();
    approachTransition.clear();
    approachSuffix.clear();
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
bool Route::isSmaller(const atools::geo::LineDistance& dist1, const atools::geo::LineDistance& dist2, float epsilon)
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
    activePos.pos.distanceMeterToLine(constFirst().getPosition(), constFirst().getPosition(), activeLegResult);
  }
  else
  {
    if(activeLegIndex == 0)
      // Reset from point route ====================================
      activeLegIndex = 1;

    const RouteLeg& actLeg = value(activeLegIndex);
    if(actLeg.getGeometry().size() > 2 && actLeg.isAnyProcedure())
      actLeg.getGeometry().distanceMeterToLineString(activePos.pos, activeLegResult);
    else
      activePos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex), activeLegResult);
  }

  if(isTooFarToFlightPlan())
  {
    // Too far away from plan - remove active leg
    activeLegIndex = map::INVALID_INDEX_VALUE;
    return;
  }

#ifdef DEBUG_ACTIVE_LEG
  qDebug() << Q_FUNC_INFO << "activeLegResult" << activeLegResult;
  qDebug() << Q_FUNC_INFO << "activeGeometry" << value(activeLegIndex).getGeometry();
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
      while(atools::contains(value(nextLeg).getProcedureLegType(), {proc::INITIAL_FIX, proc::CUSTOM_APP_START, proc::START_OF_PROCEDURE}) &&
            getPrevPositionAt(nextLeg) == getPositionAt(nextLeg) && nextLeg < size() - 2)
        nextLeg++;
    }
    else
    {
      // Jump all initial fixes for holds since the next line can probably not overlap
      while(atools::contains(value(nextLeg).getProcedureLegType(),
                             {proc::INITIAL_FIX, proc::CUSTOM_APP_START, proc::START_OF_PROCEDURE}) && nextLeg < size() - 2)
        nextLeg++;
    }
    Pos pos1 = getPrevPositionAt(nextLeg);
    Pos pos2 = getPositionAt(nextLeg);

    // Calculate course difference
    float legCrs = pos1.angleDegTo(pos2);
    courseDiff = atools::mod(pos.course - legCrs + 360.f, 360.f);
    if(courseDiff > 180.f)
      courseDiff = 360.f - courseDiff;

#ifdef DEBUG_ACTIVE_LEG
    qDebug() << Q_FUNC_INFO << "ACTIVE" << at(activeLegIndex);
    qDebug() << Q_FUNC_INFO << "NEXT" << at(nextLeg);
#endif

    // Test next leg
    atools::geo::LineDistance nextLegResult;
    activePos.pos.distanceMeterToLine(pos1, pos2, nextLegResult);
#ifdef DEBUG_ACTIVE_LEG
    qDebug() << Q_FUNC_INFO << "NEXT" << nextLeg << nextLegResult;
#endif

    bool switchToNextLeg = false;
    if(value(activeLegIndex).getProcedureLeg().isHold())
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << Q_FUNC_INFO << "ACTIVE HOLD";
#endif
      // Test next leg if we can exit a hold
      if(value(nextLeg).getProcedureLeg().line.getPos1() == value(activeLegIndex).getPosition())
      {
#ifdef DEBUG_ACTIVE_LEG
        qDebug() << Q_FUNC_INFO << "HOLD SAME";
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
        qDebug() << Q_FUNC_INFO << "NEXT HOLD" << nextLeg << resultHold;
        qDebug() << Q_FUNC_INFO << "HOLD DIFFER";
        qDebug() << Q_FUNC_INFO << "resultHold" << resultHold;
        qDebug() << Q_FUNC_INFO << "holdLine" << at(activeLegIndex).getProcedureLeg().holdLine;
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
    qDebug() << Q_FUNC_INFO << "Next leg" << at(nextLeg);
#endif

    // Check if next leg should be activated =================================================
    // if(isAirportAfterArrival(nextLeg) && !hasAlternates())
    if(isAirportAfterArrival(nextLeg) || value(nextLeg).isAlternate())
      // Do not sequence after arrival or after end of missed
      switchToNextLeg = false;

    if(switchToNextLeg)
    {
#ifdef DEBUG_ACTIVE_LEG
      qDebug() << Q_FUNC_INFO << "Switching to next leg";
#endif

      // Either left current leg or closer to next and on courses
      // Do not track on missed if legs are not displayed
      if(!(!(shownTypes & map::MISSED_APPROACH) && value(nextLeg).getProcedureLeg().isMissed()))
      {
        // Go to next leg and increase all values
        activeLegIndex = nextLeg;
        pos.pos.distanceMeterToLine(getPrevPositionAt(activeLegIndex), getPositionAt(activeLegIndex), activeLegResult);
      }
    }
  }
#ifdef DEBUG_ACTIVE_LEG
  qDebug() << Q_FUNC_INFO << "active" << activeLegIndex << "size" << size()
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

    double distToCurrent = 0.;

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
      *nextLegDistance = static_cast<float>(distToCurrent);

    // Sum up all distances along the legs
    // Ignore missed approach legs until the active is a missedd approach leg
    double fromStart = 0.;
    for(int i = 0; i <= routeIndex; i++)
    {
      if(!value(i).getProcedureLeg().isMissed() || activeIsMissed || !value(i).isAlternate() || activeIsAlternate)
        fromStart += value(i).getDistanceTo();
      else
        break;
    }

    double fromStartLegs = fromStart;
    fromStart -= distToCurrent;
    fromStart = std::abs(fromStart);

    if(distFromStart != nullptr)
      *distFromStart = atools::minmax(0.f, totalDistance, static_cast<float>(fromStart));

    if(projectionDistance != nullptr)
      *projectionDistance = projectedDistance(activeLegResult, static_cast<float>(fromStartLegs), activeLegIndex);

    if(distToDest != nullptr)
    {
      double toDest = 0.;
      if(getSizeWithoutAlternates() == 1)
        // Single airport in plan only - calculate direct distance to
        toDest = meterToNm(activePos.pos.distanceMeterTo(constFirst().getPosition()));
      else
      {
        if(activeIsMissed)
        {
          // Summarize remaining missed leg distance if on missed
          for(int i = routeIndex + 1; i < size(); i++)
          {
            if(value(i).getProcedureLeg().isMissed())
              toDest += value(i).getDistanceTo();
          }
          toDest += distToCurrent;
        }
        else if(activeIsAlternate)
        {
          // Summarize remaining leg distance to alternate if on alternate
          for(int i = routeIndex + 1; i < size(); i++)
          {
            if(value(i).isAlternate())
              toDest += value(i).getDistanceTo();
          }
          toDest += distToCurrent;
        }
        else
        {
          int destLegIdx = getDestinationLegIndex();
          for(int i = routeIndex; i <= destLegIdx; i++)
          {
            if(!value(i).getProcedureLeg().isMissed() || activeIsMissed || !value(i).isAlternate() || activeIsAlternate)
              toDest += value(i).getDistanceTo();
            else
              break;
          }
          toDest = toDest - value(routeIndex).getDistanceTo() + distToCurrent;
        }
      }
      if(getSizeWithoutAlternates() > 1)
        // Real plan with legs - limit to total length
        *distToDest = atools::minmax(0.f, totalDistance, static_cast<float>(toDest));
      else
        // Single point/airport plan for pattern work or other
        *distToDest = std::max(0.f, static_cast<float>(toDest));
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
  int legIndex = getNearestRouteLegResult(pos, result, false /* ignoreNotEditable */, true /* ignoreMissed */);

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

float Route::getCruiseAltitudeFt() const
{
  return flightplan.getCruiseAltitudeFt();
}

float Route::getAltitudeForDistance(float currentDistToDest) const
{
  return altitude->getAltitudeForDistance(currentDistToDest);
}

float Route::getVerticalAngleAtDistance(float distanceToDest, bool *required) const
{
  return altitude->getVerticalAngleAtDistance(distanceToDest, required);
}

float Route::getVerticalAngleToNext(float nearestLegDistance) const
{
  if(NavApp::isConnectedAndAircraftFlying())
  {
    int active = getActiveLegIndexCorrected();

    if(active != map::INVALID_INDEX_VALUE)
    {
      float indicatedAltitudeFt = NavApp::getUserAircraft().getIndicatedAltitudeFt();
      float waypointAltitudeFt = altitude->value(active).getWaypointAltitude();

      // Need to be above next waypoint altitude
      if(indicatedAltitudeFt > waypointAltitudeFt)
      {
        float angle = atools::geo::atan2Deg(indicatedAltitudeFt - waypointAltitudeFt, atools::geo::nmToFeet(nearestLegDistance));
        if(angle > 0.1f && angle < 45.f)
          return -angle;
      }
    }
  }

  return map::INVALID_ANGLE_VALUE;
}

float Route::getSpeedForDistance(float currentDistToDest) const
{
  return altitude->getSpeedForDistance(currentDistToDest, NavApp::getAircraftPerformance());
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
    const RouteLeg& leg = value(foundIndex);
    if(leg.getGeometry().size() > 2)
    {
      float calculatedDistance = leg.getProcedureLeg().calculatedDistance;

      // Use approach geometry to display
      float base = distFromStartNm - (total - calculatedDistance);
      float fraction = base / calculatedDistance;
      retval = leg.getGeometry().interpolate(fraction);
    }
    else
    {
      float base = distFromStartNm - (total - leg.getDistanceTo());
      float fraction = base / leg.getDistanceTo();
      retval = getPrevPositionAt(foundIndex).interpolate(leg.getPosition(), fraction);
    }
  }

  if(!retval.isValid())
    qWarning() << Q_FUNC_INFO << "position not valid";
  return retval;
}

void Route::updateAirportRouteIndex(map::MapResult& result) const
{
  for(map::MapAirport& airport : result.airports)
  {
    if(airport.routeIndex == -1)
      airport.routeIndex = getLegIndexForIdAndType(airport.id, map::AIRPORT);
  }
}

void Route::clearAirportRouteIndex(map::MapResult& result) const
{
  result.clearRouteIndex();
}

void Route::updateApproachIls()
{
  if(isEmpty())
    return;

  // Get recommended for flight plan table
  destRunwayEnd = map::MapRunwayEnd();
  destRunwayIlsFlightPlanTable.clear();
  updateApproachRunwayEndAndIls(destRunwayIlsFlightPlanTable, &destRunwayEnd, true /* recommended */, false /* map */, false /* profile */);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "destRunwayEnd" << destRunwayEnd;
  qDebug() << Q_FUNC_INFO << "destRunwayIlsFlightPlanTable" << destRunwayIlsFlightPlanTable;
#endif

  // Get ILS and runway from route for map
  destRunwayIlsMap.clear();
  updateApproachRunwayEndAndIls(destRunwayIlsMap, &destRunwayEnd, false /* recommended */, true /* map */, false /* profile */);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "destRunwayIlsMap" << destRunwayIlsMap;
#endif

  // ILS for profile - has to match runway heading
  destRunwayIlsProfile.clear();
  updateApproachRunwayEndAndIls(destRunwayIlsProfile, &destRunwayEnd, false /* recommended */, false /* map */, true /* profile */);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "destRunwayIlsProfile" << destRunwayIlsProfile;
#endif
}

void Route::updateApproachRunwayEndAndIls(QVector<map::MapIls>& ilsVector, map::MapRunwayEnd *runwayEnd, bool recommended, bool map,
                                          bool profile) const
{
  QString destAirportIdent = getDestinationAirportLeg().getIdent();
  MapQuery *mapQuery = NavApp::getMapQueryGui();

  if(approachLegs.runwayEnd.isFullyValid())
  {
    // Runway name given in approach procedure ========================
    QList<map::MapRunwayEnd> runwayEnds;
    mapQuery->getRunwayEndByNameFuzzy(runwayEnds, approachLegs.runwayEnd.name, getDestinationAirportLeg().getAirport(),
                                      false /* nav data */);

    if(runwayEnd != nullptr && !runwayEnds.isEmpty())
      *runwayEnd = runwayEnds.constFirst();

    if(approachLegs.runwayEnd.isFullyValid())
    {
      // Have runway database object for approach ============================================

      // Get one or more ILS from flight plan leg as is
      QHash<int, map::MapIls> ilsMapAll =
        maptools::toHash(mapQuery->getIlsByAirportAndRunway(destAirportIdent, approachLegs.runwayEnd.name));

      if(ilsMapAll.isEmpty())
      {
        // ILS does not even match runway - try fuzzy to consider renamed runways
        QStringList variants = atools::fs::util::runwayNameVariants(approachLegs.runwayEnd.name);
        for(const QString& runwayVariant : qAsConst(variants))
          maptools::insert(ilsMapAll, mapQuery->getIlsByAirportAndRunway(destAirportIdent, runwayVariant));
      }

      // =========================================================================
      // Look for recommended fix reference in approach legs independent of approach type
      // Iterate backwards to catch the recommended fix closest to the runway
      QHash<int, map::MapIls> ilsMapRecommended;
      for(int i = approachLegs.procedureLegs.size() - 1; i >= 0; i--)
      {
        const proc::MapProcedureLeg& leg = approachLegs.procedureLegs.at(i);

        // Do not look for NDB and waypoints - try VOR (V) and ILS/localizer (L)
        if(!leg.isMissed() && !leg.recFixIdent.isEmpty() && leg.recFixType != "N" && leg.recFixType != "TW" &&
           leg.recFixType != "TN" && leg.recFixType != "W")
        {
          map::MapIls foundIls;
          for(const map::MapIls& ils : qAsConst(ilsMapAll))
          {
            if(leg.recFixIdent == ils.ident)
            {
              // Recommended matches ILS in the list
              ilsMapRecommended.insert(ils.id, ils);
              break;
            }
          }
        }
      } // for(int i = approachLegs.approachLegs.size() - 1; i >= 0; i--)

      // Remove all approach aids which do not fit into the approach by type ============================================

      // Make a copy before clearing out not applicable navaids
      QHash<int, map::MapIls> ilsMapComplete(ilsMapAll);

      auto it = ilsMapAll.begin();
      while(it != ilsMapAll.end())
      {
        if(it->isIls() && approachLegs.isNonPrecision())
        {
          it = ilsMapAll.erase(it); // Remove ILS from non precision approaches
          continue;
        }

        if((it->isRnp() && approachLegs.isNonPrecision() && profile) || // Show glidepath for all non precision approaches in profile only
           (it->isRnp() && approachLegs.isRnavGps()) || // Show glidepath always for RNAV and GPS
           (it->isGls() && approachLegs.isGls()) || // Show GLS for all GLS approaches
           (!it->isAnyGlsRnp() && approachLegs.hasFrequency())) // Show ILS, LOC, etc for all ILS, LOC, LDA, etc. approaches
        {
          ++it; // Keep ILS
          continue;
        }

        it = ilsMapAll.erase(it);
      }

      // ===================================================================================
      // Fill vector
      if(approachLegs.isRnavGps())
      {
        // Keep other navaids (RNP - ILS are filtered out already)
        maptools::insert(ilsMapAll, ilsMapRecommended); // Merge and deduplicate

        // Check if there is a RNP navaid matching the approach ==========
        auto found = std::find_if(ilsMapAll.constBegin(), ilsMapAll.constEnd(), [this](const map::MapIls& ils) {
          return ils.ident == approachLegs.arincName;
        });

        // Keep only the matching RNP navaid if one matches
        if(found != ilsMapAll.constEnd())
        {
          map::MapIls ils = *found; // Create copy before clearing list
          ilsMapAll.clear();
          ilsMapAll.insert(ils.id, ils); // Keep only the matching one
        }
      }
      else
        // For ILS approaches use the recommended
        ilsMapAll = ilsMapRecommended;

      // Force display of applicable navaids (by runway) if enabled for map display
      if(profile)
      {
        if(NavApp::getShownMapTypes().testFlag(map::ILS))
        {
          // Add all ILS again
          for(auto ilsIter = ilsMapComplete.begin(); ilsIter != ilsMapComplete.end(); ++ilsIter)
          {
            if(!ilsIter->isAnyGlsRnp())
              ilsMapAll.insert(ilsIter->id, *ilsIter);
          }
        }

        if(NavApp::getShownMapDisplayTypes().testFlag(map::GLS))
        {
          // Add GLS/RNV again
          for(auto glsRnvIter = ilsMapComplete.begin(); glsRnvIter != ilsMapComplete.end(); ++glsRnvIter)
          {
            if(glsRnvIter->isAnyGlsRnp())
              ilsMapAll.insert(glsRnvIter->id, *glsRnvIter);
          }
        }
      }

      ilsVector = QVector<map::MapIls>::fromList(ilsMapAll.values());

    } // if(approachLegs.runwayEnd.isFullyValid())
  } // if(approachLegs.runwayEnd.isFullyValid())

  // No runway for approach found - circling - collect recommended independent of runway ============================================
  if(ilsVector.isEmpty())
  {
    // Get all frequencies for recommended and for map only for ILS, LOC, etc. approach
    if(recommended || (map && approachLegs.hasFrequency()))
    {
      // Iterate backwards to catch the recommended fix closest to the runway
      for(int i = approachLegs.procedureLegs.size() - 1; i >= 0; i--)
      {
        const proc::MapProcedureLeg& leg = approachLegs.procedureLegs.at(i);

        // Do not look for NDB and waypoints - try VOR (V) and ILS/localizer (L)
        if(!leg.isMissed() && !leg.recFixIdent.isEmpty() && leg.recFixType != "N" && leg.recFixType != "TN")
          // Get ILS referenced in the recommended fix
          // Do not exclude waypoints since these are sometimes referenced instead of localizers
          ilsVector = mapQuery->getIlsByAirportAndIdent(destAirportIdent, leg.recFixIdent);

        if(!ilsVector.isEmpty())
          break;
      }
    }
  } // if(ilsVector.isEmpty())

  // Filter out unusable ILS for profile display ========================================================
  if(profile)
  {
    ilsVector.erase(std::remove_if(ilsVector.begin(), ilsVector.end(), [this](const map::MapIls& ils) -> bool {
      // Needs to have GS, not farther away from runway end than 4NM and not more than 20 degree difference
      return !ils.hasGlideslope() || destRunwayEnd.position.distanceMeterTo(ils.position) > atools::geo::nmToMeter(4.) ||
      atools::geo::angleAbsDiff(destRunwayEnd.heading, ils.heading) > 20.f;
    }), ilsVector.end());
  }
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

bool Route::isValidProfile() const
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

void Route::getNearestRecommended(const CoordinateConverter& conv, int xs, int ys, int screenDistance, map::MapResult& mapobjects,
                                  map::MapObjectQueryTypes types, const QVector<map::MapRef>& routeDrawnNavaids) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < size(); i++)
  {
    const RouteLeg& leg = value(i);

    if(leg.isAnyProcedure())
    {
      if(!types.testFlag(map::QUERY_PROCEDURES))
        // Do not edit procedures
        continue;

      if(leg.getProcedureLeg().isMissed() && !types.testFlag(map::QUERY_PROCEDURES_MISSED))
        // Do not edit missed procedures
        continue;

      if(conv.wToS(leg.getRecommendedFixPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)
      {
        const map::MapResult& result = leg.getProcedureLeg().recNavaids;
        if(result.hasVor())
        {
          map::MapVor vor = result.vors.constFirst();

          // Do not search if it was not drawn (i.e. excluded by passed route legs)
          if(routeDrawnNavaids.contains(vor.getRef()))
          {
            vor.routeIndex = i;
            vor.recommended = true;
            insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, vor);
          }
        }

        if(result.hasNdb())
        {
          map::MapNdb ndb = result.ndbs.constFirst();

          // Do not search if it was not drawn (i.e. excluded by passed route legs)
          if(routeDrawnNavaids.contains(ndb.getRef()))
          {
            ndb.routeIndex = i;
            ndb.recommended = true;
            insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, ndb);
          }
        }

        if(result.hasWaypoints())
        {
          map::MapWaypoint waypoint = result.waypoints.constFirst();

          // Do not search if it was not drawn (i.e. excluded by passed route legs)
          if(routeDrawnNavaids.contains(waypoint.getRef()))
          {
            waypoint.routeIndex = i;
            waypoint.recommended = true;
            insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, waypoint);
          }
        }
      }
    }
  }
}

void Route::getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance, map::MapResult& mapobjects,
                       map::MapObjectQueryTypes types, const QVector<map::MapRef>& routeDrawnNavaids) const
{
  using maptools::insertSortedByDistance;

  int x, y;

  for(int i = 0; i < size(); i++)
  {
    const RouteLeg& leg = value(i);

    if(leg.isAnyProcedure())
    {
      if(!types.testFlag(map::QUERY_PROCEDURES))
        // Do not edit procedures
        continue;

      if(leg.getProcedureLeg().isMissed() && !types.testFlag(map::QUERY_PROCEDURES_MISSED))
        // Do not edit missed procedures
        continue;
    }

    if(leg.isAlternate() && !types.testFlag(map::QUERY_ALTERNATE))
      // Do not edit alternate airports
      continue;

    // ==========================================================================================
    // Use fix position to get real navaids from procedures instead of projected or otherwise modified positions
    if(conv.wToS(leg.getFixPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)
    {
      if(leg.getVor().isValid())
      {
        map::MapVor vor = leg.getVor();

        // Do not search if it was not drawn (i.e. excluded by passed route legs)
        if(routeDrawnNavaids.contains(vor.getRef()))
        {
          vor.routeIndex = i;
          insertSortedByDistance(conv, mapobjects.vors, &mapobjects.vorIds, xs, ys, vor);
        }
      }

      if(leg.getWaypoint().isValid())
      {
        map::MapWaypoint wp = leg.getWaypoint();

        // Do not search if it was not drawn (i.e. excluded by passed route legs)
        if(routeDrawnNavaids.contains(wp.getRef()))
        {
          wp.routeIndex = i;
          insertSortedByDistance(conv, mapobjects.waypoints, &mapobjects.waypointIds, xs, ys, wp);
        }
      }

      if(leg.getNdb().isValid())
      {
        map::MapNdb ndb = leg.getNdb();

        // Do not search if it was not drawn (i.e. excluded by passed route legs)
        if(routeDrawnNavaids.contains(ndb.getRef()))
        {
          ndb.routeIndex = i;
          insertSortedByDistance(conv, mapobjects.ndbs, &mapobjects.ndbIds, xs, ys, ndb);
        }
      }

      if(leg.isAirport())
      {
        map::MapAirport ap = leg.getAirport();

        // Route airports are always shown - even if legs are passed
        ap.routeIndex = i;
        insertSortedByDistance(conv, mapobjects.airports, &mapobjects.airportIds, xs, ys, ap);
      }

      if(leg.getMapType() == map::INVALID)
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

      if(leg.getMapType() == map::USERPOINTROUTE)
      {
        map::MapUserpointRoute up;
        up.id = i;
        up.routeIndex = i;

        // Do not search if it was not drawn (i.e. excluded by passed route legs)
        if(routeDrawnNavaids.contains(up.getRef()))
        {
          up.ident = leg.getIdent();
          up.region = leg.getRegion();
          up.name = leg.getName();
          up.comment = leg.getComment();
          up.position = leg.getPosition();
          up.magvar = NavApp::getMagVar(leg.getPosition());
          mapobjects.userpointsRoute.append(up);
        }
      }
    } // if(conv.wToS(leg.getFixPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)

    // ==========================================================================================
    // Get procedure information by calculated position
    if(leg.isAnyProcedure() && types.testFlag(map::QUERY_PROC_POINTS) &&
       conv.wToS(leg.getPosition(), x, y) && manhattanDistance(x, y, xs, ys) < screenDistance)
    {
      const proc::MapProcedureLeg& procLeg = leg.getProcedureLeg();

      // Select owning procedure legs struct
      const proc::MapProcedureLegs *procLegs = nullptr;
      if(procLeg.isAnyDeparture())
        procLegs = &sidLegs;
      else if(procLeg.isAnyStar())
        procLegs = &starLegs;
      else if(procLeg.isAnyApproach())
        procLegs = &approachLegs;

      if(procLegs != nullptr)
      {
        int procIndex = procLegs->indexForLeg(procLeg);

        if(procIndex != -1)
        {
          if(leg.getProcedureLeg().isMissed())
          {
            // Add missed legs only if requested by query flags
            if(types.testFlag(map::QUERY_PROC_MISSED_POINTS))
              mapobjects.procPoints.append(map::MapProcedurePoint(*procLegs, procIndex, i,
                                                                  false /* previewParam */, false /* previewParamAll */));
          }
          else
            mapobjects.procPoints.append(map::MapProcedurePoint(*procLegs, procIndex, i,
                                                                false /* previewParam */, false /* previewParamAll */));
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs for index found";
      }
      else
        qWarning() << Q_FUNC_INFO << "no legs found";
    }
  }
}

bool Route::hasDepartureParking() const
{
  if(hasValidDeparture())
    return constFirst().getDepartureParking().isValid();

  return false;
}

bool Route::hasDepartureRunway() const
{
  if(hasDepartureStart())
    return !constFirst().getDepartureStart().runwayName.isEmpty();

  return false;
}

bool Route::hasDepartureHelipad() const
{
  if(hasDepartureStart())
    return constFirst().getDepartureStart().helipadNumber > 0;

  return false;
}

bool Route::hasDepartureStart() const
{
  if(hasValidDeparture())
    return constFirst().getDepartureStart().isValid();

  return false;
}

bool Route::isFlightplanEmpty() const
{
  return getFlightplanConst().isEmpty();
}

void Route::eraseAirway(int row)
{
  if(0 <= row && row < getFlightplanConst().size())
  {
    getFlightplan()[row].setAirway(QString());
    getFlightplan()[row].setFlag(atools::fs::pln::entry::TRACK, false);
  }
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
    if(leg.getMapType() == map::USERPOINTROUTE)
      return true;
  }
  return false;
}

bool Route::hasValidDeparture() const
{
  return getDepartureAirportLeg().isAirport();
}

bool Route::hasValidDestination() const
{
  return getDestinationAirportLeg().isAirport();
}

bool Route::hasValidDepartureAndRunways() const
{
  const RouteLeg& leg = getDepartureAirportLeg();
  return leg.isAirport() && !leg.getAirport().noRunways();
}

bool Route::hasValidDestinationAndRunways() const
{
  const RouteLeg& leg = getDestinationAirportLeg();
  return leg.isAirport() && !leg.getAirport().noRunways();
}

bool Route::hasEntries() const
{
  return getFlightplanConst().size() > 2;
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
    flightplan.removeAt(indexes.at(i));
  }
  alternateLegsOffset = map::INVALID_INDEX_VALUE;
  numAlternateLegs = 0;
}

void Route::removeMissedLegs()
{
  QVector<int> indexes;

  // Collect indexes to delete in reverse order
  for(int i = size() - 1; i >= 0; i--)
  {
    const RouteLeg& routeLeg = value(i);
    if(routeLeg.getProcedureLeg().isMissed())
      indexes.append(i);
  }

  // Delete in route legs and flight plan from the end
  for(int i = 0; i < indexes.size(); i++)
  {
    removeAt(indexes.at(i));
    flightplan.removeAt(indexes.at(i));
  }
  alternateLegsOffset = map::INVALID_INDEX_VALUE;
  numAlternateLegs = 0;
}

void Route::clearProcedures(proc::MapProcedureTypes type)
{
  // Clear procedure legs
  if(type & proc::PROCEDURE_SID)
    sidLegs.clearProcedure();
  if(type & proc::PROCEDURE_SID_TRANSITION)
    sidLegs.clearTransition();

  if(type & proc::PROCEDURE_STAR_TRANSITION)
    starLegs.clearTransition();
  if(type & proc::PROCEDURE_STAR)
    starLegs.clearProcedure();

  if(type & proc::PROCEDURE_TRANSITION)
    approachLegs.clearTransition();
  if(type & proc::PROCEDURE_APPROACH)
    approachLegs.clearProcedure();
}

void Route::reloadProcedures(proc::MapProcedureTypes procs)
{
  ProcedureQuery *procQuery = NavApp::getProcedureQuery();
  AirportQuery *apQuery = NavApp::getAirportQueryNav();

  // Check which transitions are deleted where procedures are not touched
  // Reload procedures to get a clean version which is not modified by the transition
  if(procs & proc::PROCEDURE_SID_TRANSITION && !(procs & proc::PROCEDURE_SID))
    sidLegs = *procQuery->getProcedureLegs(apQuery->getAirportById(sidLegs.ref.airportId), sidLegs.ref.procedureId);

  if(procs & proc::PROCEDURE_STAR_TRANSITION && !(procs & proc::PROCEDURE_STAR))
    starLegs = *procQuery->getProcedureLegs(apQuery->getAirportById(starLegs.ref.airportId), starLegs.ref.procedureId);

  if(procs & proc::PROCEDURE_TRANSITION && !(procs & proc::PROCEDURE_APPROACH))
    approachLegs = *procQuery->getProcedureLegs(apQuery->getAirportById(approachLegs.ref.airportId),
                                                approachLegs.ref.procedureId);
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

QStringList Route::getAlternateIdents() const
{
  QStringList alternateIdents;
  for(const map::MapAirport& airport : getAlternateAirports())
    alternateIdents.append(airport.ident);
  return alternateIdents;
}

QStringList Route::getAlternateDisplayIdents() const
{
  QStringList alternateIdents;
  for(const map::MapAirport& airport : getAlternateAirports())
    alternateIdents.append(airport.displayIdent());
  return alternateIdents;
}

const QVector<map::MapAirport> Route::getAlternateAirports() const
{
  QVector<map::MapAirport> alternates;
  int offset = getAlternateLegsOffset();
  if(offset != map::INVALID_INDEX_VALUE)
  {
    for(int idx = offset; idx < offset + getNumAlternateLegs(); idx++)
    {
      if(value(idx).isAirport())
        alternates.append(value(idx).getAirport());
    }
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

void Route::cleanupFlightPlanForProcedures(map::MapAirway& starAirway)
{
  updateIndices();

  // like removeDuplicateRouteLegs
  // Remove any legs overlapping with SID ======================================================
  int fromIdx = -1, toIdx = -1;
  if(!sidLegs.isEmpty())
  {
    // Departure - start at last SID point and look for overlap with route traversing route from end to start
    toIdx = legIndexForPositions(sidLegs.buildGeometry().reversed(), true /* not reverse */);
    fromIdx = 1;
  }

  if(fromIdx > 0 && toIdx > 0)
    removeLegs(fromIdx, toIdx);

  // Remove any legs overlapping with STAR or approach ======================================================
  fromIdx = toIdx = -1;
  if(!starLegs.isEmpty() || !approachLegs.isEmpty())
  {
    // Arrivals - start at first point and look for overlap with route traversing route from start to end
    updateIndicesAndOffsets();
    toIdx = getDestinationAirportLegIndex() - 1;

    if(!starLegs.isEmpty())
      fromIdx = legIndexForPositions(starLegs.buildGeometry(), true /* reverse */);
    else if(!approachLegs.isEmpty())
      fromIdx = legIndexForPositions(approachLegs.buildGeometry(), true /* reverse */);
  }

  if(fromIdx > 0 && toIdx > 0)
  {
    starAirway = getLegAt(toIdx).getAirway();
    removeLegs(fromIdx, toIdx);
  }
}

void Route::updateProcedureLegs(FlightplanEntryBuilder *entryBuilder, bool clearOldProcedureProperties, bool cleanupRoute)
{
  // Change STAR to connect manual legs to the arrival/approach or airport
  // This can only be done with a copy of the procedures in the route
  NavApp::getProcedureQuery()->postProcessLegsForRoute(starLegs, approachLegs, getDestinationAirportLeg().getAirport());

  // Remove all procedure / dummy legs from flight plan and route
  clearProcedureLegs(proc::PROCEDURE_ALL);

  // Airway found on the first probably duplicate waypoint of a STAR
  map::MapAirway starAirway;
  if(cleanupRoute)
    cleanupFlightPlanForProcedures(starAirway);

  sidLegsOffset = map::INVALID_INDEX_VALUE;
  starLegsOffset = map::INVALID_INDEX_VALUE;
  approachLegsOffset = map::INVALID_INDEX_VALUE;

  // Create route legs and flight plan entries from departure
  if(!sidLegs.isEmpty())
    // Starts always after departure airport
    sidLegsOffset = 1;

  for(int i = 0; i < sidLegs.size(); i++)
  {
    int insertIndex = 1 + i;
    RouteLeg routeLeg(&flightplan);
    routeLeg.createFromProcedureLegs(i, sidLegs, &value(i));
    insert(insertIndex, routeLeg);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(sidLegs.at(insertIndex - 1), entry, true);
    flightplan.insert(insertIndex, entry);
  }

  // Create route legs and flight plan entries from STAR
  int insertOffset = numAlternateLegs + 1;
  if(!starLegs.isEmpty())
    starLegsOffset = size() - insertOffset;

  for(int i = 0; i < starLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &value(size() - 2) : nullptr;

    RouteLeg routeLeg(&flightplan);
    routeLeg.createFromProcedureLegs(i, starLegs, prev);
    if(i == 0)
      // Add airway of first waypoint again
      routeLeg.setAirway(starAirway);
    insert(size() - insertOffset, routeLeg);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(starLegs.at(i), entry, true);
    if(i == 0)
      // Add airway of first waypoint again
      entry.setAirway(starAirway.getIdent());
    flightplan.insert(flightplan.size() - insertOffset, entry);
  }

  // Create route legs and flight plan entries from arrival
  if(!approachLegs.isEmpty())
    approachLegsOffset = size() - insertOffset;

  for(int i = 0; i < approachLegs.size(); i++)
  {
    const RouteLeg *prev = size() >= 2 ? &value(size() - 2) : nullptr;

    RouteLeg routeLeg(&flightplan);
    routeLeg.createFromProcedureLegs(i, approachLegs, prev);
    insert(size() - insertOffset, routeLeg);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(approachLegs.at(i), entry, true);
    flightplan.insert(flightplan.size() - insertOffset, entry);
  }

  // Leave procedure information in the PLN file
  if(clearOldProcedureProperties)
    clearFlightplanProcedureProperties(proc::PROCEDURE_ALL);

  ProcedureQuery::fillFlightplanProcedureProperties(flightplan.getProperties(), approachLegs, starLegs, sidLegs);
  updateIndicesAndOffsets();
}

proc::MapProcedureTypes Route::getMissingProcedures()
{
  return ProcedureQuery::getMissingProcedures(flightplan.getProperties(), approachLegs, starLegs, sidLegs);
}

void Route::selectionFlagsAlternate(const QList<int>& rows, bool& containsAlternate, bool& moveDownTouchesAlt,
                                    bool& moveUpTouchesAlt, bool& moveDownLeavesAlt, bool& moveUpLeavesAlt) const
{
  containsAlternate = moveDownTouchesAlt = moveUpTouchesAlt = moveDownLeavesAlt = moveUpLeavesAlt = false;

  if(!rows.isEmpty())
  {
    for(int row : rows)
      containsAlternate |= value(row).isAlternate();

    moveUpTouchesAlt = rows.constFirst() > 0 && value(rows.constFirst() - 1).isAlternate();
    moveDownTouchesAlt = rows.constLast() < size() - 1 && value(rows.constLast() + 1).isAlternate();

    moveUpLeavesAlt = rows.constFirst() > 0 && !value(rows.constFirst() - 1).isAlternate();
    moveDownLeavesAlt = rows.constLast() >= size() - 1 || !value(rows.constLast() + 1).isAlternate();
  }
}

void Route::selectionFlagsProcedures(const QList<int>& rows, bool& containsProc, bool& moveDownTouchesProc, bool& moveUpTouchesProc) const
{
  containsProc = moveDownTouchesProc = moveUpTouchesProc = false;

  if(!rows.isEmpty())
  {
    for(int row : rows)
      containsProc |= value(row).isAnyProcedure();

    moveUpTouchesProc = rows.constFirst() > 0 && value(rows.constFirst() - 1).isAnyProcedure();
    moveDownTouchesProc = rows.constLast() < size() - 1 && value(rows.constLast() + 1).isAnyProcedure();
  }
}

void Route::setActivePos(const atools::geo::Pos& pos, float course)
{
  activePos.pos = pos;
  activePos.course = course;
}

int Route::getLegIndexForRef(const map::MapRef& ref) const
{
  return objectIndex.value(ref, -1);
}

void Route::removeLegs(int from, int to)
{
  qDebug() << Q_FUNC_INFO << "removing from" << from << "to" << to;

  // Delete in route legs and flight plan
  for(int i = to; i >= from; i--)
  {
    removeAt(i);
    flightplan.removeAt(i);
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
    flightplan.removeAt(indexes.at(i));
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
      flightplan.removeAt(indexes.at(i));
  }
}

void Route::updateDepartureAndDestination(bool clearInvalidStart)
{
  if(!isEmpty())
  {
    // Only the departure airport has parking positions
    for(int i = 1; i < size(); i++)
      (*this)[i].clearParkingAndStart();

    // Correct departure and destination values
    const RouteLeg& departure = getDepartureAirportLeg();
    flightplan.setDepartureIdent(departure.getIdent());
    flightplan.setDepartureName(departure.getName());
    flightplan.setDeparturePosition(departure.getPosition());

    if(hasDepartureParking())
    {
      // Get position from parking spot
      flightplan.setDepartureParkingName(map::parkingNameForFlightplan(departure.getDepartureParking()));
      flightplan.setDepartureParkingPosition(departure.getDepartureParking().position, departure.getAltitude(),
                                             departure.getDepartureParking().heading);
      flightplan.setDepartureParkingType(atools::fs::pln::PARKING);
    }
    else if(hasDepartureStart())
    {
      // Get position from start
      flightplan.setDepartureParkingName(departure.getDepartureStart().runwayName);
      flightplan.setDepartureParkingPosition(departure.getDepartureStart().position, departure.getAltitude(),
                                             departure.getDepartureStart().heading);

      // A start can be a runway or a helipad
      if(departure.getDepartureStart().helipadNumber != -1)
        flightplan.setDepartureParkingType(atools::fs::pln::HELIPAD);
      else if(!departure.getDepartureStart().runwayName.isEmpty())
        flightplan.setDepartureParkingType(atools::fs::pln::RUNWAY);
    }
    else if(clearInvalidStart) // Clear only if requested - otherwise leave parking and/or start intact
    {
      // No start position and no parking - use airport/navaid position
      flightplan.setDepartureParkingName(QString());
      flightplan.setDepartureParkingPosition(departure.getPosition(),
                                             atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);
      flightplan.setDepartureParkingType(atools::fs::pln::AIRPORT);
    }

    const RouteLeg& destination = getDestinationAirportLeg();
    flightplan.setDestinationIdent(destination.getIdent());
    flightplan.setDestinationName(destination.getName());
    flightplan.setDestinationPosition(destination.getPosition());
  }
  else
    flightplan.clearAll();
}

void Route::updateAll()
{
  updateIndicesAndOffsets();
  removeDuplicateRouteLegs();
  validateAirways();
  updateMagvar();
  updateDistancesAndCourse();
  updateBoundingRect();
  updateWaypointNames();
  updateDepartureAndDestination(false /* clearInvalidStart */);
  updateApproachIls();
}

void Route::updateWaypointNames()
{
  int num = 1;
  for(FlightplanEntry& entry : flightplan)
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::USER && entry.getIdent().startsWith("WP"))
    {
      QRegularExpressionMatch match = USER_WP_ID.match(entry.getIdent());
      if(match.hasMatch())
        entry.setIdent(QString("WP%1").arg(num++));
    }
  }
}

void Route::updateAirportRegions()
{
  int i = 0;
  for(RouteLeg& leg : *this)
  {
    if(leg.getMapType() == map::AIRPORT)
    {
      NavApp::getAirportQuerySim()->getAirportRegion(leg.getAirport());
      flightplan[i].setRegion(leg.getAirport().region);
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
  objectIndex.clear();

  // Update internal indices pointing to flight plan legs
  for(int i = 0; i < size(); i++)
  {
    RouteLeg& leg = (*this)[i];
    leg.setFlightplanEntryIndex(i);
    objectIndex.insert(map::MapRef(leg.getId(), leg.getMapType()), i); // types includes PROCEDURE
  }
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
    const proc::MapProcedureLeg& leg = at(i).getProcedureLeg();

    if(leg.isValid())
    {
      if(leg.isAnyDeparture() && sidLegsOffset == map::INVALID_INDEX_VALUE)
        sidLegsOffset = i;

      if(leg.isAnyStar() && starLegsOffset == map::INVALID_INDEX_VALUE)
        starLegsOffset = i;

      if(leg.isArrival() && approachLegsOffset == map::INVALID_INDEX_VALUE)
        approachLegsOffset = i;
    }
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

bool Route::isActiveProcedure() const
{
  const RouteLeg *leg = getActiveLeg();
  if(leg != nullptr)
    return leg->isAnyProcedure();
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
         index == getDestinationAirportLegIndex() && value(index).getMapType() == map::AIRPORT;
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
#ifdef DEBUG_INFORMATION
    qWarning() << Q_FUNC_INFO << "Invalid index" << i;
#endif
    return EMPTY_ROUTE_LEG;
  }
  else
    return QList::at(i);
}

int Route::getSizeWithoutAlternates() const
{
  return QList::size() - getNumAlternateLegs();
}

void Route::removeAllExceptRange(int from, int to)
{
  for(int row = size() - 1; row > to; row--)
    removeAllAt(row);
  for(int row = from - 1; row >= 0; row--)
    removeAllAt(row);
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

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "totalDistance" << totalDistance;
#endif
}

void Route::updateMagvar()
{
  // get magvar from internal database objects (waypoints, VOR and others)
  for(int i = 0; i < size(); i++)
    (*this)[i].updateMagvar(i > 0 ? &at(i - 1) : nullptr);
}

void Route::updateLegAltitudes()
{
  // Calculate also with empty route to allow updating of error messages
  altitude->calculateAll(NavApp::getAircraftPerformance(), getCruiseAltitudeFt());
}

/* Update the bounding rect using marble functions to catch anti meridian overlap */
void Route::updateBoundingRect()
{
  Marble::GeoDataLineString line;

  for(const RouteLeg& routeLeg : qAsConst(*this))
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

int Route::getNearestRouteLegResult(const Pos& pos, atools::geo::LineDistance& lineDistanceResult, bool ignoreNotEditable,
                                    bool ignoreMissed) const
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
    if(ignoreMissed && value(i).isAnyProcedure() && value(i).getProcedureLeg().isMissed())
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

bool Route::isActiveDestinationAirport() const
{
  if(hasAnyArrivalProcedure())
    return false;
  else
  {
    int destIdx = getDestinationLegIndex();
    return destIdx != map::INVALID_INDEX_VALUE && activeLegIndex != map::INVALID_INDEX_VALUE &&
           activeLegIndex == getDestinationLegIndex();
  }
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

map::MapParking Route::getDepartureParking() const
{
  int idx = getDepartureAirportLegIndex();

  if(idx != map::INVALID_INDEX_VALUE)
    return at(idx).getDepartureParking();
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << idx;
  return map::MapParking();
}

map::MapStart Route::getDepartureStart() const
{
  int idx = getDepartureAirportLegIndex();

  if(idx != map::INVALID_INDEX_VALUE)
    return at(idx).getDepartureStart();
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << idx;
  return map::MapStart();
}

const RouteLeg& Route::getLegAt(int index) const
{
  if(index >= 0 && index < size())
    return at(index);
  else
    return EMPTY_ROUTELEG;
}

bool Route::isAirportDeparture(const QString& ident) const
{
  return !isEmpty() && constFirst().isAirport() && constFirst().getAirport().ident == ident;
}

bool Route::isAirportDestination(const QString& ident) const
{
  return !isEmpty() && getDestinationAirportLeg().isAirport() && getDestinationAirportLeg().getAirport().ident == ident;
}

bool Route::isAirportAlternate(const QString& ident) const
{
  return !isEmpty() && (getAlternateIdents().contains(ident) || getAlternateDisplayIdents().contains(ident));
}

bool Route::isAirportRoundTrip(const QString& ident) const
{
  return isAirportDeparture(ident) && isAirportDestination(ident);
}

void Route::getAirportProcedureFlags(const map::MapAirport& airport, int index, bool& departureFilter,
                                     bool& arrivalFilter) const
{
  bool hasDeparture, hasAnyArrival, airportDeparture, airportDestination, airportRoundTrip;
  getAirportProcedureFlags(airport, index, departureFilter, arrivalFilter, hasDeparture,
                           hasAnyArrival, airportDeparture, airportDestination, airportRoundTrip);
}

void Route::getAirportProcedureFlags(const map::MapAirport& airport, int index, bool& departureFilter,
                                     bool& arrivalFilter, bool& hasDeparture, bool& hasAnyArrival,
                                     bool& airportDeparture, bool& airportDestination, bool& airportRoundTrip) const
{
  departureFilter = arrivalFilter = hasDeparture = hasAnyArrival = airportDeparture = airportDestination =
    airportRoundTrip = false;

  if(airport.isValid())
  {
    hasDeparture = NavApp::getMapQueryGui()->hasDepartureProcedures(airport);
    hasAnyArrival = NavApp::getMapQueryGui()->hasArrivalProcedures(airport);

    if(index == -1)
    {
      // No index given - select state by ident
      airportDeparture = isAirportDeparture(airport.ident);
      airportDestination = isAirportDestination(airport.ident);

      // Can be one airport entry or departure equal to dest
      airportRoundTrip = isAirportRoundTrip(airport.ident);
    }
    else
    {
      // Use index to detect state
      airportDeparture = index == getDepartureAirportLegIndex();
      airportDestination = index == getDestinationAirportLegIndex();

      // Is only round trip if airport matches and only one leg
      airportRoundTrip = isAirportRoundTrip(airport.ident) && getSizeWithoutAlternates() == 1;
    }

    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture && !airportRoundTrip)
        departureFilter = hasDeparture;
      else if(airportDestination && !airportRoundTrip)
        arrivalFilter = hasAnyArrival;
    }
  }
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

  if(hasAnyProcedure())
  {
    int lastDeparture = getLastIndexOfDepartureProcedure();
    if(lastDeparture > 0)
      ok &=
        // Need a fix at procedure end to calculate from end of SID
        (firstIndex == lastDeparture && proc::procedureLegFixAtEnd(value(lastDeparture).getProcedureLegType())) ||
        // Above procedure boundary
        firstIndex > lastDeparture;

    int firstArrival = getArrivaLegsOffset();
    if(firstArrival != map::INVALID_INDEX_VALUE)
      ok &=
        // Need a fix at procedure start to calculate to start of approach or STAR
        (lastIndex == firstArrival && proc::procedureLegFixAtStart(value(firstArrival).getProcedureLegType())) ||
        // Below procedure boundary
        lastIndex < firstArrival;
  }

  return ok;
}

bool Route::canSaveSelection(int firstIndex, int lastIndex) const
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

  if(hasAnyProcedure())
  {
    int lastDeparture = getLastIndexOfDepartureProcedure();
    if(lastDeparture > 0)
      ok &= firstIndex > lastDeparture;

    int firstArrival = getArrivaLegsOffset();
    if(firstArrival != map::INVALID_INDEX_VALUE)
      ok &= lastIndex < firstArrival;
  }

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
      airway = flightplan.getPropertiesConst().value(atools::fs::pln::PROCAIRWAY);
    if(NavApp::getAirwayTrackQueryGui()->hasAirwayForNameAndWaypoint(airway, routeLeg.getIdent(), arrivalLeg.getIdent()))
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

    if(!NavApp::getAirwayTrackQueryGui()->hasAirwayForNameAndWaypoint(routeLeg.getAirwayName(), departureLeg.getIdent(),
                                                                      routeLeg.getIdent()))
      // Airway not valid for changed waypoints - erase
      flightplan[startIndexAfterProcedure].setAirway(QString());
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
      flightplan[arrivaLegsOffset].setAirway(flightplan.at(arrivaLegsOffset - 1).getAirway());

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

const Pos Route::getPositionAt(int i) const
{
  return value(i).getPosition();
}

const Pos Route::getPrevPositionAt(int i) const
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
  for(int i = 0; i < flightplan.size(); i++)
  {
    RouteLeg leg(&flightplan);
    leg.createFromDatabaseByEntry(i, lastLeg);

    if(leg.getMapType() == map::INVALID)
      // Not found in database
      qWarning() << "Entry for ident" << flightplan.at(i).getIdent() << "region" << flightplan.at(i).getRegion() << "is not valid";

    append(leg);
    lastLeg = &constLast();
  }
}

void Route::assignAltitudes()
{
  QVector<float> altVector = altitude->getAltitudes();
  for(int i = 0; i < size(); i++)
    flightplan[i].setAltitude(altVector.at(i));
}

void Route::zeroAltitudes()
{
  for(int i = 0; i < size(); i++)
    flightplan[i].setAltitude(0.f);
}

Route Route::updatedAltitudes() const
{
  return updatedAltitudes(*this);
}

Route Route::updatedAltitudes(const Route& routeParam)
{
  Route route(routeParam);
  route.assignAltitudes();
  return route;
}

Route Route::zeroedAltitudes() const
{
  return zeroedAltitudes(*this);
}

Route Route::zeroedAltitudes(const Route& routeParam)
{
  Route route(routeParam);
  route.zeroAltitudes();
  return route;
}

Route Route::adjustedToOptions(rf::RouteAdjustOptions options) const
{
  return adjustedToOptions(*this, options);
}

Route Route::adjustedToOptions(const Route& origRoute, rf::RouteAdjustOptions options)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "options" << options;
#endif
  bool saveApproachWp = options.testFlag(rf::SAVE_APPROACH_WP),
       saveSidWp = options.testFlag(rf::SAVE_SIDSTAR_WP), saveStarWp = options.testFlag(rf::SAVE_SIDSTAR_WP),
       replaceCustomWp = options.testFlag(rf::REPLACE_CUSTOM_WP), msfs = options.testFlag(rf::SAVE_MSFS),
       removeCustom = options.testFlag(rf::REMOVE_RUNWAY_PROC);

  // Create copy which allows to modify the plan ==============
  Route route(origRoute);
  atools::fs::pln::Flightplan& plan = route.getFlightplan();
  FlightplanEntryBuilder entryBuilder;

  // Clear unresolved parking/start names depending on flag
  route.updateDepartureAndDestination(!options.testFlag(rf::SAVE_KEEP_INVALID_START));

  // Restore duplicate waypoints at route/procedure entry/exits which were removed after route calculation
  if(options.testFlag(rf::FIX_PROC_ENTRY_EXIT) || options.testFlag(rf::FIX_PROC_ENTRY_EXIT_ALWAYS))
  {
    QVector<float> altVector = route.getAltitudeLegs().getAltitudes();

    // Check arrival airway ===========================================================================
    int arrivaLegsOffset = map::INVALID_INDEX_VALUE;
    if(route.arrivalRouteToProcLegs(arrivaLegsOffset))
    {
      const RouteLeg& arrivalLeg = route[arrivaLegsOffset];

      if(!arrivalLeg.getProcedureLeg().isCustomApproach())
      {
        QString airway = arrivalLeg.getAirwayName();
        if(airway.isEmpty())
          airway = plan.getPropertiesConst().value(atools::fs::pln::PROCAIRWAY);

        const RouteLeg& routeLeg = route.value(arrivaLegsOffset - 1);
        if((!airway.isEmpty() || options.testFlag(rf::FIX_PROC_ENTRY_EXIT_ALWAYS)) &&
           proc::procedureLegFixAtStart(arrivalLeg.getProcedureLegType()) && !arrivalLeg.isNavaidEqualTo(routeLeg))
        {
          FlightplanEntry entry;
          entryBuilder.buildFlightplanEntry(arrivalLeg.getProcedureLeg(), entry,
                                            true /* resolve waypoints to VOR and others*/);
          entry.setAirway(arrivalLeg.getAirwayName());
          entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);
          entry.setAltitude(altVector.value(arrivaLegsOffset, 0.f));

          RouteLeg newLeg = RouteLeg(&route.flightplan);
          newLeg.createCopyFromProcedureLeg(arrivaLegsOffset, arrivalLeg, &routeLeg);
          newLeg.setAirway(arrivalLeg.getAirway());

          route.insert(arrivaLegsOffset, newLeg);
          plan.insert(arrivaLegsOffset, entry);
          plan.getProperties().remove(atools::fs::pln::PROCAIRWAY);
          route.updateIndicesAndOffsets();
        }
      }
    }

    // Check departure airway ===========================================================================
    int startIndexAfterProcedure = map::INVALID_INDEX_VALUE;
    if(route.departureProcToRouteLegs(startIndexAfterProcedure))
    {
      // remove duplicates between end of SID and route: only legs that have the navaid at the end of the leg
      const RouteLeg& departureLeg = route.value(startIndexAfterProcedure - 1);

      if(!departureLeg.getProcedureLeg().isCustomDeparture())
      {
        const RouteLeg& routeLeg = route.value(startIndexAfterProcedure);
        if((!routeLeg.getAirwayName().isEmpty() || options.testFlag(rf::FIX_PROC_ENTRY_EXIT_ALWAYS)) &&
           proc::procedureLegFixAtEnd(departureLeg.getProcedureLegType()) && !departureLeg.isNavaidEqualTo(routeLeg))
        {
          FlightplanEntry entry;
          entryBuilder.buildFlightplanEntry(departureLeg.getProcedureLeg(), entry,
                                            true /* resolve waypoints to VOR and others*/);
          entry.setAirway(QString());
          entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);
          entry.setAltitude(altVector.value(startIndexAfterProcedure - 1, 0.f));

          RouteLeg newLeg = RouteLeg(&route.flightplan);
          newLeg.createCopyFromProcedureLeg(startIndexAfterProcedure, departureLeg, &routeLeg);
          newLeg.setAirway(map::MapAirway());

          route.insert(startIndexAfterProcedure, newLeg);
          plan.insert(startIndexAfterProcedure, entry);
          route.updateIndicesAndOffsets();
        }
      }
    }
  }

  // Remove trailing alternates ====================================================
  if(options.testFlag(rf::REMOVE_ALTERNATE))
  {
    route.removeAlternateLegs();
    route.updateIndicesAndOffsets();
  }

  // Remove missed approach legs ====================================================
  if(options.testFlag(rf::REMOVE_MISSED))
  {
    route.removeMissedLegs();
    route.updateIndicesAndOffsets();
  }

  // Remove or clear custom ============================================================================
  // Remember value since type is cleared later
  bool customApproach = route.hasCustomApproach();
  bool customDeparture = route.hasCustomDeparture();

  if(!removeCustom)
  {
    // Replace custom approach with user defined waypoints ==============
    if(customApproach && replaceCustomWp)
      saveApproachWp = true;

    if(customDeparture && replaceCustomWp)
    {
      saveSidWp = true;

      // Remove RW waypoint to avoid confusing X-Plane FMS and GPS
      // DEPART -> RW -> RW+X -> DEST
      if(options.testFlag(rf::CLEAN_CUSTOM_DEPART) && route.size() > 3)
        route.removeAllAt(1);
    }
  }

  // First remove properties and procedure structures if needed ====================================
  if(removeCustom)
  {
    // Remove custom departures and approaches including legs
    proc::MapProcedureTypes removeTypes = proc::PROCEDURE_NONE;
    if(customApproach)
      removeTypes |= proc::PROCEDURE_APPROACH_ALL_MISSED;
    if(customDeparture)
      removeTypes |= proc::PROCEDURE_SID_ALL;

    route.clearProcedureLegs(removeTypes);
    route.clearFlightplanProcedureProperties(removeTypes);
    route.clearProcedureLegs(removeTypes);
    route.updateIndicesAndOffsets();
  }

  // Remove MSFS transitions ==================================================================
  if(msfs)
  {
    // Remove approach transitions and missed- these are not saved
    route.clearProcedureLegs(proc::PROCEDURE_MISSED | proc::PROCEDURE_TRANSITION);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_TRANSITION);
    route.updateIndicesAndOffsets();
  }

  // Save procedures as waypoints ==================================================
  if(saveApproachWp)
  {
    route.clearProcedures(proc::PROCEDURE_APPROACH_ALL_MISSED);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_APPROACH_ALL_MISSED);

    // Remove missed legs
    route.clearProcedureLegs(proc::PROCEDURE_MISSED);
    route.updateIndicesAndOffsets();
  }

  if(saveSidWp)
  {
    route.clearProcedures(proc::PROCEDURE_SID_ALL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_SID_ALL);
    route.updateIndicesAndOffsets();
  }

  if(saveStarWp)
  {
    route.clearProcedures(proc::PROCEDURE_STAR_ALL);
    route.clearFlightplanProcedureProperties(proc::PROCEDURE_STAR_ALL);
    route.updateIndicesAndOffsets();
  }

  // Remove track names from airway field but keep waypoints =========================================
  if(options.testFlag(rf::REMOVE_TRACKS))
  {
    for(int i = 0; i < plan.size(); i++)
    {
      if(route.value(i).isTrack())
      {
        route[i].clearAirwayOrTrack();
        plan[i].setAirway(QString());
      }
    }
  }

  // Remove airway names but keep waypoints =========================================
  if(options.testFlag(rf::SAVE_AIRWAY_WP))
  {
    for(int i = 0; i < plan.size(); i++)
    {
      if(route.value(i).isAirway())
      {
        route[i].clearAirwayOrTrack();
        plan[i].setAirway(QString());
      }
    }
    plan.getProperties().remove(atools::fs::pln::PROCAIRWAY);
  }

  if(saveApproachWp || saveSidWp || saveStarWp || msfs)
  {
    const proc::MapProcedureLegs& sid = route.getSidLegs();
    const proc::MapProcedureLegs& star = route.getStarLegs();

    // Replace procedures with waypoints =======================================================================
    // Now replace all entries with either valid waypoints or user defined waypoints
    int wpNum = 1;
    for(int i = 0; i < plan.size(); i++)
    {
      FlightplanEntry& entry = plan[i];
      const RouteLeg& leg = route.value(i);
      const proc::MapProcedureLeg& procedureLeg = leg.getProcedureLeg();

      bool isAppr = leg.getProcedureType() & proc::PROCEDURE_APPROACH_ALL;
      bool isSid = leg.getProcedureType() & proc::PROCEDURE_SID_ALL;
      bool isStar = leg.getProcedureType() & proc::PROCEDURE_STAR_ALL;

      bool legMatches = (saveApproachWp && isAppr) || (saveSidWp && isSid) || (saveStarWp && isStar);

      if( // Save approach or SID/STAR waypoints and this leg is part of a procedure
        legMatches ||
        // MSFS - save SID/STAR legs and this leg is one
        (msfs && (isSid || isStar)))
      {
        entry.setFlag(atools::fs::pln::entry::TRACK, false);

        // Entry is one of the category which have to be replaced
        // Information is updated further down - clear here
        /// Coordinates are already set ok
        entry.setIdent(QString());
        entry.setRegion(QString());
        entry.setAirway(QString());

        if(!msfs || legMatches)
          // Clear procedure flag to keep legs in plan
          entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);
        else
        {
          // Prepare MSFS SID and STAR legs ==========================
          int number = 0;
          QString rw, designator;

          // Keep SID and STAR waypoints but keep transition waypoints
          if(leg.getProcedureType() & proc::PROCEDURE_SID_ALL)
          {
            // Clear procedure flag to keep SID and transition legs in plan
            entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);

            // Set entry to SID but not transition
            entry.setSid(sid.procedureFixIdent);
            rw = sid.runway;
          }
          else if(leg.getProcedureType() & proc::PROCEDURE_STAR_ALL)
          {
            // Clear procedure flag to keep STAR and transition legs in plan
            entry.setFlag(atools::fs::pln::entry::PROCEDURE, false);

            // Set entry to STAR but not transition
            entry.setStar(star.procedureFixIdent);
            rw = star.runway;
          }

          if(!rw.isEmpty())
          {
            if(atools::fs::util::runwayNameSplit(rw, &number, &designator))
              entry.setRunway(QString::number(number), atools::fs::util::runwayDesignatorLong(designator));
          }
        }

        // Assign navaid to flight plan entry ================
        if(leg.isAirport())
        {
          entry.setWaypointType(atools::fs::pln::entry::AIRPORT);
          entry.setIdent(leg.getAirport().ident);
          entry.setRegion(leg.getAirport().region);
        }
        else if(leg.getWaypoint().isValid())
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

          if((replaceCustomWp || saveApproachWp) && customApproach && leg.getProcedureType() & proc::PROCEDURE_APPROACH)
            entry.setIdent(procedureLeg.fixIdent);
          else if((replaceCustomWp || saveSidWp) && customDeparture && leg.getProcedureType() & proc::PROCEDURE_SID)
            entry.setIdent(procedureLeg.fixIdent);
          else
          {
            QString legText = procedureLeg.displayText.join(" ");
            if(msfs)
              // More relaxed than FSX
              entry.setIdent(atools::fs::util::adjustMsfsUserWpName(legText, 10, &wpNum));
            else
              entry.setIdent(atools::fs::util::adjustFsxUserWpName(legText));
          }
        }

        if(leg.isAnyProcedure())
        {
          // Correct coordinates for all distance or otherwise terminated legs ============================
          if(atools::contains(procedureLeg.type, {
            proc::COURSE_TO_ALTITUDE, proc::COURSE_TO_DME_DISTANCE, proc::COURSE_TO_INTERCEPT, proc::COURSE_TO_RADIAL_TERMINATION,
            proc::FIX_TO_ALTITUDE, proc::TRACK_FROM_FIX_FROM_DISTANCE, proc::TRACK_FROM_FIX_TO_DME_DISTANCE,
            proc::FROM_FIX_TO_MANUAL_TERMINATION, proc::HEADING_TO_ALTITUDE_TERMINATION, proc::HEADING_TO_DME_DISTANCE_TERMINATION,
            proc::HEADING_TO_INTERCEPT, proc::HEADING_TO_MANUAL_TERMINATION, proc::HEADING_TO_RADIAL_TERMINATION}))
          {
            // Set user waypoint
            entry.setWaypointType(atools::fs::pln::entry::USER);
            entry.setIdent(proc::procedureLegFixStr(procedureLeg));

            // Leg does not carry altitude in case of procedure points. Take this from the flight plan entry
            entry.setPosition(leg.getPosition().alt(entry.getAltitude()));
          }
        } // if(leg.isAnyProcedure())
      } // if((saveApproachWp && (leg.getProcedureType() & proc::PROCEDURE_ARRIVAL)) || ...
    } // for(int i = 0; i < entries.size(); i++)

    // Now remove of all procedure legs in the flight plan =====================
    plan.erase(std::remove_if(plan.begin(), plan.end(), [](const FlightplanEntry& type) -> bool
    {
      return type.getFlags() & atools::fs::pln::entry::PROCEDURE;
    }), plan.end());

    if(!msfs)
    {
      // Delete same consecutive =======================
      for(int i = plan.size() - 1; i > 0; i--)
      {
        const FlightplanEntry& entry = plan.at(i);
        const FlightplanEntry& prev = plan.at(i - 1);

        if(entry.getIdent() == prev.getIdent() &&
           entry.getRegion() == prev.getRegion() &&
           entry.getPosition().almostEqual(prev.getPosition(), Pos::POS_EPSILON_100M))
          plan.removeAt(i);
      }
    }
    else
    {
      // MSFS: Remove airway information for STAR entry waypoints ==============================
      for(FlightplanEntry& entry : plan)
      {
        if(!entry.getStar().isEmpty())
          entry.setAirway(QString());
      }

      // MSFS: Add approach information to destination airport ==============================
      const proc::MapProcedureLegs& appr = route.getApproachLegs();

      if(!appr.isEmpty())
      {
        // Can use last since alternates are already stripped off
        FlightplanEntry& entry = plan.last();
        int number = 0;
        QString designator;

        if(!appr.runway.isEmpty())
        {
          if(atools::fs::util::runwayNameSplit(appr.runway, &number, &designator))
            entry.setRunway(QString::number(number), atools::fs::util::runwayDesignatorLong(designator));
        }
        entry.setApproach(appr.type, appr.suffix, appr.transitionFixIdent);
      }
    }

    // Copy flight plan entries to route legs - will also add coordinates
    route.createRouteLegsFromFlightplan();
    route.updateAll();

    // Assign airport idents to waypoints where available - for MSFS =======================================
    if(msfs)
    {
      MapQuery *mapQuery = NavApp::getMapQueryGui();
      bool found = false;
      for(FlightplanEntry& entry : plan)
      {
        // Get airport for every navaid
        QString airportIdent;
        const QString& ident = entry.getIdent(), region = entry.getRegion();
        const atools::geo::Pos& pos = entry.getPosition();

        switch(entry.getWaypointType())
        {
          case atools::fs::pln::entry::WAYPOINT:
            airportIdent = mapQuery->getAirportIdentFromWaypoint(ident, region, pos, found);
            break;

          case atools::fs::pln::entry::VOR:
            airportIdent = mapQuery->getAirportIdentFromVor(ident, region, pos, found);
            break;

          case atools::fs::pln::entry::NDB:
            airportIdent = mapQuery->getAirportIdentFromNdb(ident, region, pos, found);
            break;

          case atools::fs::pln::entry::UNKNOWN:
          case atools::fs::pln::entry::AIRPORT:
          case atools::fs::pln::entry::USER:
            break;
        }

        if(!airportIdent.isEmpty())
          entry.setAirport(airportIdent);
      } // for(FlightplanEntry& entry : entries)
    } // if(msfs)
      // Airways are updated in route controller
  } // if(saveApproachWp || saveSidStarWp || msfs)

  if(options.testFlag(rf::FIX_CIRCLETOLAND))
  {
    // ========================================================================
    // Check for a circle-to-land approach without runway - add a random (best) runway from the airport to
    // satisfy the X-Plane GPS/FMC/G1000
    if(route.getDestinationAirportLeg().isAirport() &&
       plan.getPropertiesConst().value(atools::fs::pln::APPROACH_RW).isEmpty() &&
       (!plan.getPropertiesConst().value(atools::fs::pln::APPROACH).isEmpty() ||
        !plan.getPropertiesConst().value(atools::fs::pln::APPROACH_ARINC).isEmpty()))
    {
      // Get best runway - longest with probably hard surface
      const QList<map::MapRunway> *runways = NavApp::getAirportQuerySim()->
                                             getRunways(route.getDestinationAirportLeg().getId());
      if(runways != nullptr && !runways->isEmpty())
        plan.getProperties().insert(atools::fs::pln::APPROACH_RW, runways->constLast().primaryName);
    }
  }

  if(plan.size() <= 2 || plan.getFlightplanType() == atools::fs::pln::VFR)
    // Use only real direct without procedures or VFR - MSFS accepts only VFR if direct
    plan.setRouteType(atools::fs::pln::DIRECT);
  else
  {
    // Use an estimated difference for high/low
    if(route.getCruiseAltitudeFt() < 18000.f)
      plan.setRouteType(atools::fs::pln::LOW_ALTITUDE);
    else
      plan.setRouteType(atools::fs::pln::HIGH_ALTITUDE);
  }

  // Update internal structures - remaining tasks do not change order or number
  route.updateIndicesAndOffsets();

  // Reset airport name to display name for LNMPLN (allow to exchange plans without the internal XP names) and X-Plane FMS
  if(options.testFlag(rf::SAVE_LNMPLN) || options.testFlag(rf::XPLANE_REPLACE_AIRPORT_IDENTS))
  {
    for(int i = 0; i < plan.size(); i++)
    {
      FlightplanEntry& entry = plan[i];
      if(entry.getWaypointType() == atools::fs::pln::entry::AIRPORT)
      {
        // Use display ident in legs to avoid user confusion
        QString displayIdent = route.getLegAt(i).getAirport().displayIdent();
        if(displayIdent.isEmpty())
          // Fall back to flight plan if RouteLeg is invalid or empty (not resolved against database)
          displayIdent = route.getFlightplanConst().at(i).getIdent();

        entry.setIdent(displayIdent);
      }
    }
  }

  if(options.testFlag(rf::XPLANE_REPLACE_AIRPORT_IDENTS))
  {
    // Replace X-Plane waypoint idents with official ones for airports
    // XP accepts only internal codes in departure and destination fields
    for(int i = 0; i < plan.size(); i++)
    {
      FlightplanEntry& entry = plan[i];
      const map::MapAirport& airport = route.getLegAt(i).getAirport();
      if(entry.getWaypointType() == atools::fs::pln::entry::AIRPORT)
      {
        // All airport idents will be truncated to six characters on export
        if(i == 0 || i == plan.size() - 1)
        {
          // Determine ident for departure or destination

          // Use DEP/DES keywords for long internal airport idents
          bool useAirportKeys = airport.ident.size() <= 4;

          // Check official idents - ignore IATA
          if(!airport.icao.isEmpty())
            // Ok to use ADEP/ADES keywords if ICAO is present and the same
            useAirportKeys = airport.icao == airport.ident;
          else if(!airport.faa.isEmpty())
            // No ICAO - ok to use ADEP/ADES keywords if FAA is present and the same
            useAirportKeys = airport.faa == airport.ident;
          else if(!airport.local.isEmpty())
            // No ICAO and no FAA - ok to use ADEP/ADES keywords if local code is present and the same
            useAirportKeys = airport.local == airport.ident;

          // ADEP and ADES cannot be used if an airport is not stock but only add-on in X-Plane
          if(useAirportKeys && NavApp::getInfoQuery()->isAirportXplaneCustomOnly(airport.ident))
            useAirportKeys = false;

          // Use display ident for DEP/DES since it does not matter in this configuration
          QString ident = useAirportKeys ? airport.ident : airport.displayIdent();

          if(ident.size() > 4)
            useAirportKeys = false;

          // Use ADEP/ADES if there are any procedures in the flight plan or airport has procedures in general - X-Plane can load this
          if(airport.procedure() || // By XP database
             NavApp::getMapQueryGui()->hasProcedures(airport) || // Checks navdatabase
             (i == 0 && route.hasAnySidProcedure()) || (i == plan.size() - 1 && route.hasAnyArrivalProcedure()))
            useAirportKeys = true;

          if(i == 0)
          {
            // Set for start
            plan.setDepartureIdent(ident);

            // Set properties to use ADEP/ADES or DEP/DES.  Read by FlightplanIO::saveFmsInternal()
            // Value does not matter - only presence is checked
            if(useAirportKeys)
              // ADEP
              plan.getProperties().remove(atools::fs::pln::AIRPORT_DEPARTURE_NO_AIRPORT);
            else
              // DEP
              plan.getProperties().insert(atools::fs::pln::AIRPORT_DEPARTURE_NO_AIRPORT, QString());
          }
          else if(i == plan.size() - 1)
          {
            // Set for destination
            plan.setDestinationIdent(ident);

            if(useAirportKeys)
              // ADES
              plan.getProperties().remove(atools::fs::pln::AIRPORT_DESTINATION_NO_AIRPORT);
            else
              // DES
              plan.getProperties().insert(atools::fs::pln::AIRPORT_DESTINATION_NO_AIRPORT, QString());
          }
        } // if(i == 0 || i == entries.size() - 1)
      } // if(entry.getWaypointType() == atools::fs::pln::entry::AIRPORT)
    } // for(int i = 0; i < entries.size(); i++)
  } // if(options.testFlag(rf::XPLANE_REPLACE_AIRPORT_IDENTS))

  if(options.testFlag(rf::ISG_USER_WP_NAMES))
  {
    for(int i = 0; i < plan.size(); i++)
    {
      // <ATCWaypoint id="3924N11657W">
      // <ATCWaypointType>User</ATCWaypointType>
      // <WorldPosition>N39° 24' 0.01",W116° 57' 0.00",+010000.00</WorldPosition>
      // </ATCWaypoint>

      FlightplanEntry& entry = plan[i];
      if(entry.getWaypointType() == atools::fs::pln::entry::USER)
        entry.setIdent(atools::fs::util::toDegMinFormat(entry.getPosition()));
    }
  }

  route.updateRouteCycleMetadata();
  route.updateAircraftPerfMetadata();

  return route;
}

/* Check if route has valid departure parking.
 *  @return true if route can be saved anyway */
bool Route::hasValidParking() const
{
  if(hasValidDeparture())
  {
    const QList<map::MapParking> *parkingCache = NavApp::getAirportQuerySim()->getParkingsForAirport(constFirst().getId());

    if(!parkingCache->isEmpty())
      return hasDepartureParking() || hasDepartureHelipad();
    else
      // No parking available - so no parking selection is ok
      return true;
  }
  else
    return false;
}

void Route::updateAirways(float& minAltitudeFt, float& maxAltitudeFt, bool adjustRouteAltitude)
{
  minAltitudeFt = atools::fs::pln::FLIGHTPLAN_ALTITUDE_FT_MIN;
  maxAltitudeFt = atools::fs::pln::FLIGHTPLAN_ALTITUDE_FT_MAX;

  for(int i = 1; i < size(); i++)
  {
    RouteLeg& routeLeg = (*this)[i];
    const RouteLeg& prevLeg = value(i - 1);

    // Adjust min and max by airway or track restricitons ============================
    if(!routeLeg.getAirwayName().isEmpty())
    {
      if(i == 1 && prevLeg.getMapType() == map::AIRPORT)
      {
        routeLeg.setAirway(map::MapAirway());
        routeLeg.getFlightplanEntry()->setAirway(QString());
        routeLeg.getFlightplanEntry()->setFlag(atools::fs::pln::entry::TRACK, false);
        continue;
      }

      QList<map::MapAirway> airways;
      NavApp::getAirwayTrackQueryGui()->getAirwaysByNameAndWaypoint(airways, routeLeg.getAirwayName(), prevLeg.getIdent(),
                                                                    routeLeg.getIdent());

      if(!airways.isEmpty())
      {
        // There is only one airway with the given between waypoints
        const map::MapAirway& airway = airways.constFirst();

        if(airway.isValid())
        {
          routeLeg.setAirway(airway);

          if(adjustRouteAltitude)
          {
            minAltitudeFt = std::max(static_cast<float>(airway.minAltitude), minAltitudeFt);
            if(airway.maxAltitude > 0)
              maxAltitudeFt = std::min(static_cast<float>(airway.maxAltitude), maxAltitudeFt);
          }
        }
      }
    }
    else
      // No airway - clear it in the route leg
      routeLeg.setAirway(map::MapAirway());

    if(adjustRouteAltitude)
    {
      // Adjust min by procedure restrictions ===========================
      const proc::MapAltRestriction& altRestr = routeLeg.getProcedureLegAltRestr();
      if(altRestr.isValid())
      {
        // Cannot fly cruise lower than procedure restriciton
        switch(altRestr.descriptor)
        {
          case proc::MapAltRestriction::AT: // At alt1
          case proc::MapAltRestriction::AT_OR_ABOVE: // At or above alt1
            minAltitudeFt = std::max(altRestr.alt1, minAltitudeFt);
            break;

          case proc::MapAltRestriction::BETWEEN:
            // At or above alt2 and at or below alt1
            minAltitudeFt = std::max(altRestr.alt2, minAltitudeFt);
            break;

          case proc::MapAltRestriction::AT_OR_BELOW:
          case proc::MapAltRestriction::NO_ALT_RESTR:
          case proc::MapAltRestriction::ILS_AT:
          case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
            break;
        }
      }
    }
  }
}

void Route::updateAirwaysAndAltitude(bool adjustRouteAltitude)
{
  if(isEmpty())
    return;

  float minAltitudeFt, maxAltitudeFt;
  updateAirways(minAltitudeFt, maxAltitudeFt, adjustRouteAltitude); // Returns ft

  // Local units are ft or meter depending on options
  // Convert ft to local unit
  float minAltitudeLocal = Unit::altFeetF(minAltitudeFt);
  float maxAltitudeLocal = Unit::altFeetF(maxAltitudeFt);

  // Adjust if needed but only if values do not exceed maximum ==================================
  // This is only used after loading or importing a flight plan
  if(adjustRouteAltitude)
  {
    if(minAltitudeLocal <= 0 && maxAltitudeLocal >= Unit::altFeetI(map::MapAirway::MAX_ALTITUDE_LIMIT_FT))
    {
      opts::UnitAlt unitAlt = Unit::getUnitAlt();
      // No valid information from airways or procedures - fall back to fixed values based on type
      if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
      {
        minAltitudeLocal = unitAlt == opts::ALT_FT ? 20000.f : 5000.f;
        maxAltitudeLocal = unitAlt == opts::ALT_FT ? 24000.f : 7000.f;
      }
      else
      {
        minAltitudeLocal = unitAlt == opts::ALT_FT ? 8000.f : 2500.f;
        maxAltitudeLocal = unitAlt == opts::ALT_FT ? 10000.f : 3000.f;
      }

      // Increase values for high elevation airports
      float airportAltFt = 0.f;
      if(getDestinationAirportLeg().getAirport().isValid())
        airportAltFt = getDestinationAirportLeg().getAirport().getAltitude();

      if(getDepartureAirportLeg().getAirport().isValid())
        airportAltFt = std::max(getDepartureAirportLeg().getAirport().getAltitude(), airportAltFt);

      float airportAltLocal = Unit::altFeetF(airportAltFt);
      if(airportAltLocal >= minAltitudeLocal)
      {
        minAltitudeLocal = static_cast<int>(std::ceil(airportAltLocal / 1000.f) * 1000.f) + 1000.f;
        maxAltitudeLocal = minAltitudeLocal + (unitAlt == opts::ALT_FT ? 2000.f : 500.f);
      }
    }

    if(minAltitudeLocal > maxAltitudeLocal)
      maxAltitudeLocal = minAltitudeLocal;

    // Convert feet to local unit
    float cruisingAltitudeLocal = Unit::altFeetF(flightplan.getCruiseAltitudeFt());

    // Check airway limits after calculation ===========================
    // First do basic aligment - round to next 1000 and add 500 for VFR
    // Add 500 ft/m for VFR
    float offset = flightplan.getFlightplanType() == atools::fs::pln::IFR ? 0.f : 500.f;

    if(cruisingAltitudeLocal < minAltitudeLocal)
      // Below min altitude - use min altitude and round up to next valid level
      cruisingAltitudeLocal = static_cast<int>(std::ceil((minAltitudeLocal - offset) / 1000.f) * 1000.f + offset);

    if(cruisingAltitudeLocal > maxAltitudeLocal)
      // Above max altitude - use max altitude and round down
      cruisingAltitudeLocal = static_cast<int>(std::floor((maxAltitudeLocal - offset) / 1000.f) * 1000.f + offset);

    // Adjust altitude after route calculation for East/West or other rules ===========================
    if(OptionData::instance().getFlags().testFlag(opts::ROUTE_ALTITUDE_RULE))
    {
      // Apply simplified east/west or other rule - always rounds up =============================
      float adjusted = getAdjustedAltitude(cruisingAltitudeLocal);

      if(adjusted > maxAltitudeLocal)
        adjusted = getAdjustedAltitude(cruisingAltitudeLocal) - 1000.f;
      cruisingAltitudeLocal = adjusted;
    }

    // Convert local unit back to feet
    flightplan.setCruiseAltitudeFt(Unit::rev(cruisingAltitudeLocal, Unit::altFeetF));

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "Updating flight plan altitude"
             << "minAltitudeLocal" << minAltitudeLocal << "maxAltitudeLocal" << maxAltitudeLocal
             << "cruisingAltitudeLocal" << cruisingAltitudeLocal;
#endif
  } // if(adjustRouteAltitude)
}

/* Apply simplified east/west or north/south rule */
float Route::getAdjustedAltitude(float altitudeLocal) const
{
  if(getSizeWithoutAlternates() > 1)
  {
    const Pos& departurePos = constFirst().getPosition();
    const Pos& destinationPos = getDestinationAirportLeg().getPosition();

    float magvarAverage = (getDepartureAirportLeg().getMagvarStart() + getDestinationAirportLeg().getMagvarEnd()) / 2.f;

    // Use constant course line to calculate
    float fpDir = atools::geo::normalizeCourse(departurePos.angleDegToRhumb(destinationPos) - magvarAverage);

    if(fpDir < Pos::INVALID_VALUE)
    {
      qDebug() << Q_FUNC_INFO << "minAltitude" << altitudeLocal << "fp dir" << fpDir;

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

      if(getFlightplanConst().getFlightplanType() == atools::fs::pln::IFR)
      {
        // IFR - 1000
        if(odd)
          // round up to the next odd value
          altitudeLocal = std::ceil((altitudeLocal - 1000.f) / 2000.f) * 2000.f + 1000.f;
        else
          // round up to the next even value
          altitudeLocal = std::ceil(altitudeLocal / 2000.f) * 2000.f;
      }
      else
      {
        // VFR - 500
        if(odd)
          // round up to the next odd value + 500
          altitudeLocal = std::ceil((altitudeLocal - 1500.f) / 2000.f) * 2000.f + 1500.f;
        else
          // round up to the next even value + 500
          altitudeLocal = std::ceil((altitudeLocal - 500.f) / 2000.f) * 2000.f + 500.f;
      }
    }
  }
  return altitudeLocal;
}

bool Route::isTooFarToFlightPlan() const
{
  return getDistanceToFlightPlan() < map::INVALID_DISTANCE_VALUE && getDistanceToFlightPlan() > MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM;
}

float Route::getDistanceToFlightPlan() const
{
  if(activeLegResult.status != atools::geo::INVALID)
    return atools::geo::meterToNm(std::abs(activeLegResult.distance));
  else
    return map::INVALID_DISTANCE_VALUE;
}

QString Route::getProcedureLegText(proc::MapProcedureTypes mapType, bool includeRunway, bool missedAsApproach,
                                   bool transitionAsProcedure) const
{
  const proc::MapProcedureLegs *procLegs = nullptr;

  if(mapType & proc::PROCEDURE_SID_ALL)
    procLegs = &sidLegs;
  else if(mapType & proc::PROCEDURE_STAR_ALL)
    procLegs = &starLegs;
  else if(mapType & proc::PROCEDURE_APPROACH_ALL_MISSED)
    procLegs = &approachLegs;

  if(procLegs != nullptr)
    return proc::procedureLegsText(*procLegs, mapType, false /* narrow */, includeRunway, missedAsApproach, transitionAsProcedure);
  else
    return QString();
}

void Route::getVerticalPathDeviationTexts(QString *descentDeviation, QString *verticalAngle, bool *verticalRequired,
                                          QString *verticalAngleNext) const
{
  float distFromStartNm = map::INVALID_DISTANCE_VALUE, distToDestNm = map::INVALID_DISTANCE_VALUE,
        nextLegDistance = map::INVALID_DISTANCE_VALUE, distanceToTod = map::INVALID_DISTANCE_VALUE;
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = NavApp::getUserAircraft();

  // Vertical path deviation =======================================================
  if(getRouteDistances(&distFromStartNm, &distToDestNm, &nextLegDistance))
  {
    if(distFromStartNm < map::INVALID_DISTANCE_VALUE)
      distanceToTod = getTopOfDescentDistance() - distFromStartNm;

    if(distanceToTod <= 0 && userAircraft.isValid() && userAircraft.isFlying())
    {
      if(descentDeviation != nullptr)
      {
        // Display vertical path deviation when after TOD
        float vertAlt = getAltitudeForDistance(distToDestNm);

        if(vertAlt < map::INVALID_ALTITUDE_VALUE)
        {
          float diff = userAircraft.getActualAltitudeFt() - vertAlt;
          QString upDown;
          if(diff >= 100.f)
            upDown = tr(", above ▼");
          else if(diff <= -100)
            upDown = tr(", below ▲");

          *descentDeviation = Unit::altFeet(diff) % upDown;
        }
      }
    }
  }

  // Vertical speed and angle to destination =======================================================
  if(verticalAngle != nullptr)
  {
    bool required = false;
    float vertAngle = getVerticalAngleAtDistance(distToDestNm, &required);

    if(verticalRequired != nullptr)
      *verticalRequired = required;

    if(vertAngle < map::INVALID_ANGLE_VALUE)
    {
      QString speedText = Unit::speedVertFpm(-atools::geo::descentSpeedForPathAngle(userAircraft.getGroundSpeedKts(), vertAngle));
      *verticalAngle = tr("%L1°, %L2").arg(vertAngle, 0, 'g', required ? 3 : 2).arg(speedText % tr(" ▼"));
    }
  }

  // Vertical speed and angle to next =======================================================
  if(verticalAngleNext != nullptr)
  {
    float vertAngleToNext = getVerticalAngleToNext(nextLegDistance);
    if(vertAngleToNext < map::INVALID_ANGLE_VALUE)
    {
      QString speedText = Unit::speedVertFpm(-atools::geo::descentSpeedForPathAngle(userAircraft.getGroundSpeedKts(), vertAngleToNext));
      *verticalAngleNext = tr("%L1°, %L2").arg(vertAngleToNext, 0, 'g', 2).arg(speedText % tr(" ▼"));
    }
  }

}

QString Route::buildDefaultFilename(const QString& suffix) const
{
  return buildDefaultFilename(QString(), suffix);
}

QString Route::buildDefaultFilename(QString pattern, QString suffix) const
{
  if(isEmpty())
    return tr("Empty Flightplan") + suffix;

  QString type = flightplan.getFlightplanTypeStr();
  QString departName = getDepartureAirportLeg().getName(), departIdent = getDepartureAirportLeg().getDisplayIdent(),
          destName = getDestinationAirportLeg().getName(), destIdent = getDestinationAirportLeg().getDisplayIdent();

  if(pattern.isEmpty())
    // Use pattern from options if not given
    pattern = OptionData::instance().getFlightplanPattern();

  if(pattern.endsWith(suffix, Qt::CaseInsensitive))
    // Clear suffix if this is already a part of the filename
    suffix.clear();

  return Flightplan::getFilenamePattern(pattern, type, departName, departIdent, destName, destIdent, suffix,
                                        atools::roundToInt(Unit::altFeetF(flightplan.getCruiseAltitudeFt())));
}

QString Route::buildDefaultFilenameShort(const QString& separator, const QString& suffix) const
{
  if(isEmpty())
    return tr("EMPTY") + suffix;

  QString departIdent = getDepartureAirportLeg().getDisplayIdent(), destIdent = getDestinationAirportLeg().getDisplayIdent();

  return Flightplan::getFilenamePattern(atools::fs::pln::pattern::DEPARTIDENT % separator % atools::fs::pln::pattern::DESTIDENT,
                                        QString(), QString(), departIdent, QString(), destIdent, suffix,
                                        atools::roundToInt(Unit::altFeetF(flightplan.getCruiseAltitudeFt())));
}

QDebug operator<<(QDebug out, const Route& route)
{
  QDebugStateSaver saver(out);
  out << endl << "Route ==================================================================" << endl;
  out << route.getFlightplanConst().getProperties() << endl << endl;
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
