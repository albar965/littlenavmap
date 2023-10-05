/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "route/routealtitude.h"

#include "atools.h"
#include "common/unit.h"
#include "fs/perf/aircraftperf.h"
#include "geo/calculations.h"
#include "app/navapp.h"
#include "route/route.h"
#include "weather/windreporter.h"

#include <QLineF>

// Altitude resuling from vertical angle is adjusted to restriction if close with this limit
const float VERT_ANGLE_ALT_ADJUST_LIMIT_FT = 100.f;

// Tolerance when checking vertical angle with restrictions
const float VERT_ANGLE_ALT_TOLERANCE_FT = 50.f;

// Tolerance when comparing altitude to restrictions
const float AT_RESTRICTION_TOLERANCE_FT = 10.f;

// Minimums for flight plan to avoid errors
const float MIN_CRUISE_ALTITUDE_FT = 100.f;
const float MIN_FLIGHTPLAN_DIST_NM = 0.5f;

using atools::interpolate;
namespace ageo = atools::geo;

RouteAltitude::RouteAltitude(const Route *routeParam)
  : route(routeParam)
{

}

RouteAltitude RouteAltitude::copy(const Route *routeParam)
{
  RouteAltitude retval(*this);
  retval.route = routeParam;
  return retval;
}

int RouteAltitude::indexForDistance(float distanceToDest) const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  float distFromStart = route->getTotalDistance() - distanceToDest;

  for(int index = 0; index < size(); index++)
  {
    const RouteAltitudeLeg& leg = at(index);

    if(leg.isMissed() || leg.isAlternate())
      break;

    if(distFromStart < leg.getDistanceFromStart())
      return index;
  }

  return map::INVALID_INDEX_VALUE;
}

float RouteAltitude::getSpeedForDistance(float distanceToDest, const atools::fs::perf::AircraftPerf& perf) const
{
  if(isValidProfile())
  {
    float distFromStart = route->getTotalDistance() - distanceToDest;
    if(distFromStart < distanceTopOfClimb)
      return perf.getClimbSpeed();
    else if(distFromStart < distanceTopOfDescent)
      return perf.getCruiseSpeed();
    else
      return perf.getDescentSpeed();
  }

  return map::INVALID_SPEED_VALUE;
}

float RouteAltitude::getAltitudeForDistance(float distanceToDest) const
{
  if(isEmpty())
    return map::INVALID_ALTITUDE_VALUE;

  float distFromStart = route->getTotalDistance() - distanceToDest;

  // Search through all legs to find one that overlaps with distanceToDest
  int idx = indexForDistance(distanceToDest);

  if(idx != map::INVALID_INDEX_VALUE)
  {
    // Now search through the geometry to find a matching line (if more than one)
    const RouteAltitudeLeg& leg = value(idx);

    auto itGeo = std::lower_bound(leg.geometry.constBegin(), leg.geometry.constEnd(), distFromStart,
                                  [](const QPointF& pt, float dist) -> bool {
      // true if first is less than second, i.e. ordered before
      return pt.x() < dist;
    });

    if(itGeo != leg.geometry.end())
    {
      QPointF pt2 = *itGeo;
      QPointF pt1 = *(--itGeo);

      // Interpolate and try to find point and altitude for given distance
      QLineF line(pt1, pt2);
      return static_cast<float>(line.pointAt((distFromStart - pt1.x()) / line.dx()).y());
    }
  }
  return map::INVALID_ALTITUDE_VALUE;
}

float RouteAltitude::getVerticalAngleAtDistance(float distanceToDest, bool *required) const
{
  if(isEmpty())
    return map::INVALID_ANGLE_VALUE;

  if(required != nullptr)
    *required = false;

  // Search through all legs to find one that overlaps with distanceToDest
  int idx = indexForDistance(distanceToDest);

  if(idx != map::INVALID_INDEX_VALUE)
  {
    // Now search through the geometry to find a matching line (if more than one)
    const RouteAltitudeLeg& leg = value(idx);

    if(leg.isVerticalProcAngleValid())
    {
      if(required != nullptr)
        *required = true;
      return leg.getVerticalProcAngle();
    }

    auto itGeo = std::lower_bound(leg.geometry.constBegin(), leg.geometry.constEnd(), route->getTotalDistance() - distanceToDest,
                                  [](const QPointF& pt, float dist) -> bool {
      // true if first is less than second, i.e. ordered before
      return pt.x() < dist;
    });

    if(itGeo != leg.geometry.end())
      return leg.getVerticalGeoAngles().value(static_cast<int>(itGeo - leg.geometry.begin()) - 1, map::INVALID_ANGLE_VALUE);
  }
  return map::INVALID_ANGLE_VALUE;
}

float RouteAltitude::getTravelTimeHours() const
{
  return travelTime;
}

float RouteAltitude::getTopOfDescentFromDestination() const
{
  if(isEmpty() || !(distanceTopOfDescent < map::INVALID_DISTANCE_VALUE))
    return map::INVALID_DISTANCE_VALUE;
  else
    return route->getTotalDistance() - distanceTopOfDescent;
}

atools::geo::Pos RouteAltitude::getTopOfDescentPos() const
{
  // Avoid any invalid points near destination
  if(isEmpty() || !(distanceTopOfDescent < map::INVALID_DISTANCE_VALUE) ||
     distanceTopOfDescent > route->getTotalDistance() ||
     legIndexTopOfDescent > map::INVALID_INDEX_VALUE / 2)
    return ageo::EMPTY_POS;
  else
  {
    const ageo::LineString& line = value(legIndexTopOfDescent).getLineString();

    if(!line.isEmpty())
      return line.value(line.size() - 2);
    else
      return ageo::EMPTY_POS;
  }
}

atools::geo::Pos RouteAltitude::getTopOfClimbPos() const
{
  // Avoid any invalid points near departure
  if(isEmpty() || !(distanceTopOfClimb < map::INVALID_DISTANCE_VALUE) || distanceTopOfClimb < 0.f ||
     legIndexTopOfClimb > map::INVALID_INDEX_VALUE / 2)
    return ageo::EMPTY_POS;
  else
    return value(legIndexTopOfClimb).getLineString().value(1);
}

void RouteAltitude::clearAll()
{
  clear();
  distanceTopOfClimb = map::INVALID_DISTANCE_VALUE;
  distanceTopOfDescent = map::INVALID_DISTANCE_VALUE;
  legIndexTopOfClimb = map::INVALID_INDEX_VALUE;
  legIndexTopOfDescent = map::INVALID_INDEX_VALUE;
  travelTime = 0.f;
  averageGroundSpeed = 0.f;
  unflyableLegs = false;
  validProfile = false;
}

const RouteAltitudeLeg& RouteAltitude::value(int i) const
{
  const static RouteAltitudeLeg EMPTY_ROUTE_ALT_LEG;
  if(!atools::inRange(*this, i))
  {
#ifdef DEBUG_INFORMATION
    qWarning() << Q_FUNC_INFO << "Invalid index" << i;
#endif
    return EMPTY_ROUTE_ALT_LEG;
  }
  else
    return QVector::at(i);
}

float RouteAltitude::getTotalDistance() const
{
  return route->getTotalDistance();
}

float RouteAltitude::getBlockFuel(const atools::fs::perf::AircraftPerf& perf) const
{
  return (tripFuel * perf.getContingencyFuelFactor()) + alternateFuel +
         perf.getTaxiFuel() + perf.getExtraFuel() + perf.getReserveFuel();
}

float RouteAltitude::getDestinationFuel(const atools::fs::perf::AircraftPerf& perf) const
{
  float destFuel = getBlockFuel(perf) - tripFuel - perf.getTaxiFuel();

  if(atools::almostEqual(destFuel, 0.f, 0.1f))
    // Avoid -0 case
    destFuel = 0.f;
  return destFuel;
}

float RouteAltitude::getContingencyFuel(const atools::fs::perf::AircraftPerf& perf) const
{
  return tripFuel * (perf.getContingencyFuelFactor() - 1.f);
}

bool RouteAltitude::hasErrors() const
{
  return !errors.isEmpty() || !(getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE &&
                                getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE);
}

QStringList RouteAltitude::getErrorStrings() const
{
#ifdef DEBUG_INFORMATION
  if(!errors.isEmpty())
    qDebug() << Q_FUNC_INFO << errors;
#endif

  return errors;
}

QVector<float> RouteAltitude::getAltitudes() const
{
  QVector<float> retval;

  if(!isEmpty())
  {
    // Have valid altitude legs ==========================
    for(const RouteAltitudeLeg& leg: (*this))
      retval.append(leg.y2());

    if(!route->isEmpty())
    {
      // Fix departure altitude if airport is valid
      const RouteLeg& first = route->getDepartureAirportLeg();
      if(first.isRoute())
        retval.replace(0, first.getAltitude());

      // Replace the zero altitude of the last dummy segment with the airport altitude
      const RouteLeg& last = route->getDestinationAirportLeg();
      if(last.isRoute())
        retval.replace(retval.size() - 1, last.getAltitude());
    }
  }
  else
  {
    int destinationAirportLegIndex = route->getDestinationAirportLegIndex();
    // No altitude legs - copy airport and cruise altitude ==========================
    for(int i = 0; i < route->size(); i++)
    {
      const RouteLeg& leg = route->value(i);

      if(i == 0)
        retval.append(leg.getAltitude());
      else if(i == destinationAirportLegIndex)
        retval.append(leg.getAltitude());
      else
        retval.append(route->getCruiseAltitudeFt());
    }
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << retval;
#endif

  return retval;
}

void RouteAltitude::calculateFuelAndTimeTo(FuelTimeResult& result, float distanceToDest, float distanceToNext,
                                           const atools::fs::perf::AircraftPerf& perf,
                                           float aircraftFuelFlowLbs, float aircraftFuelFlowGal,
                                           float aircraftGroundSpeed, int activeLegIdx) const
{
  // otherwise estimated using aircraft fuel flow

  bool alternate = route->isActiveAlternate();
  bool missed = route->isActiveMissed();

  // Need a valid profile and valid active leg
  if(isValidProfile() && activeLegIdx != map::INVALID_INDEX_VALUE)
  {
    const RouteAltitudeLeg& activeLeg = value(activeLegIdx);
    float fuelToDest = 0.f;

    // Copy values from next into destination later if flying an alternate
    // Alternate also has no TOD
    if(!alternate)
    {
      // Calculate values based on profile data ======================================

      // Calculate time and fuel to destination ============================================
      // Fuel from end of leg to destination - missed will be estimated later
      if(!missed && distanceToDest >= 0.f && distanceToDest < map::INVALID_DISTANCE_VALUE)
      {
        float distFromDeparture = getTotalDistance() - distanceToDest;

        if(perf.isFuelFlowValid())
        {
          // calculate fuel and time from this leg
          fuelToDest = activeLeg.getFuelFromDistToDestination(distFromDeparture);

          // Convert units
          if(perf.useFuelAsVolume())
          {
            result.fuelLbsToDest = ageo::fromGalToLbs(perf.isJetFuel(), fuelToDest);
            result.fuelGalToDest = fuelToDest;
          }
          else
          {
            result.fuelLbsToDest = fuelToDest;
            result.fuelGalToDest = ageo::fromLbsToGal(perf.isJetFuel(), fuelToDest);
          }
        }

        if(perf.isSpeedValid())
          result.timeToDest = activeLeg.getTimeFromDistToDestination(distFromDeparture);

        // Calculate time and fuel to top of descent ===================================================
        int todIdx = getTopOfDescentLegIndex();
        float todDistanceFromDeparture = getTopOfDescentDistance();

        if(todDistanceFromDeparture >= 0.f && todDistanceFromDeparture < map::INVALID_DISTANCE_VALUE &&
           todIdx != map::INVALID_INDEX_VALUE)
        {
          const RouteAltitudeLeg& todLeg = value(todIdx);

          if(perf.isFuelFlowValid())
          {
            // Calculate fuel and time from TOD to destination
            float fuelTodToDist = todLeg.getFuelFromDistToDestination(todDistanceFromDeparture);

            // Calculate fuel and time from aircraft to TOD
            float fuelToTod = fuelToDest - fuelTodToDist;

            if(perf.useFuelAsVolume())
            {
              result.fuelLbsToTod = ageo::fromGalToLbs(perf.isJetFuel(), fuelToTod);
              result.fuelGalToTod = fuelToTod;
            }
            else
            {
              result.fuelLbsToTod = fuelToTod;
              result.fuelGalToTod = ageo::fromLbsToGal(perf.isJetFuel(), fuelToTod);
            }
          }

          if(perf.isSpeedValid())
            result.timeToTod = result.timeToDest - todLeg.getTimeFromDistToDestination(todDistanceFromDeparture);
        } // if(todDistanceFromDeparture >= 0.f && todDistanceFromDeparture ...

        // Calculate time and fuel to top of climb  ===================================================
        int tocIdx = getTopOfClimbLegIndex();
        float tocDistanceFromDeparture = getTopOfClimbDistance();

        if(tocDistanceFromDeparture >= 0.f && tocDistanceFromDeparture < map::INVALID_DISTANCE_VALUE &&
           tocIdx != map::INVALID_INDEX_VALUE)
        {
          const RouteAltitudeLeg& tocLeg = value(tocIdx);

          if(perf.isFuelFlowValid())
          {
            // Calculate fuel and time from TOC to destination
            float fuelTocToDist = tocLeg.getFuelFromDistToDestination(tocDistanceFromDeparture);

            // Calculate fuel and time from aircraft to TOC
            float fuelToToc = fuelToDest - fuelTocToDist;

            if(perf.useFuelAsVolume())
            {
              result.fuelLbsToToc = ageo::fromGalToLbs(perf.isJetFuel(), fuelToToc);
              result.fuelGalToToc = fuelToToc;
            }
            else
            {
              result.fuelLbsToToc = fuelToToc;
              result.fuelGalToToc = ageo::fromLbsToGal(perf.isJetFuel(), fuelToToc);
            }
          }

          if(perf.isSpeedValid())
            result.timeToToc = result.timeToDest - tocLeg.getTimeFromDistToDestination(tocDistanceFromDeparture);
        } // if(tocDistanceFromDeparture >= 0.f && tocDistanceFromDeparture ...
      } // if(distanceToDest > 0.f && distanceToDest < map::INVALID_DISTANCE_VALUE)
    } // if(!alternate)

    // Calculate time and fuel to Next waypoint ============================================
    if(distanceToNext >= 0.f && distanceToNext < map::INVALID_DISTANCE_VALUE)
    {
      float distFromStart = activeLeg.getDistanceFromStart() - distanceToNext;

      if(perf.isFuelFlowValid())
      {
        float fuelToNext = activeLeg.getFuelFromDistToEnd(distFromStart);

        if(perf.useFuelAsVolume())
        {
          result.fuelLbsToNext = ageo::fromGalToLbs(perf.isJetFuel(), fuelToNext);
          result.fuelGalToNext = fuelToNext;
        }
        else
        {
          result.fuelLbsToNext = fuelToNext;
          result.fuelGalToNext = ageo::fromLbsToGal(perf.isJetFuel(), fuelToNext);
        }
      }

      if(perf.isSpeedValid())
        result.timeToNext = activeLeg.getTimeFromDistToEnd(distFromStart);
    } // if(distanceToNext < map::INVALID_DISTANCE_VALUE)
  } // if(isValidProfile())

  // Fill missing values with estimates ====================================================
  // Need aircraft flying but no active leg
  if(aircraftFuelFlowLbs > 0.01f && aircraftGroundSpeed > map::MIN_GROUND_SPEED)
  {
    result.estimatedFuel = !perf.isFuelFlowValid() || !isValidProfile();
    result.estimatedTime = !perf.isSpeedValid() || !isValidProfile();

    // No valid profile - calculate values based on aircraft ======================================
    // Use classic calculation based on actual consumption values ==================

    if(!alternate)
    {
      // Estimate time and fuel to Destination =============================================================
      if(distanceToDest > 0.f && distanceToDest < map::INVALID_DISTANCE_VALUE)
      {
        if(!result.isFuelToDestValid())
        {
          result.fuelLbsToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowLbs;
          result.fuelGalToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowGal;
        }
        if(!result.isTimeToDestValid())
          result.timeToDest = distanceToDest / aircraftGroundSpeed;
      }

      // Estimate time and fuel to TOD =============================================================
      float distanceToTod = distanceToDest - getTopOfDescentFromDestination();
      if(distanceToTod > 0.f && distanceToTod < map::INVALID_DISTANCE_VALUE)
      {
        if(!result.isFuelToTodValid())
        {
          result.fuelLbsToTod = distanceToTod / aircraftGroundSpeed * aircraftFuelFlowLbs;
          result.fuelGalToTod = distanceToTod / aircraftGroundSpeed * aircraftFuelFlowGal;
        }

        if(!result.isTimeToTodValid())
          result.timeToTod = distanceToTod / aircraftGroundSpeed;
      }
    }

    // Estimate time and fuel to next waypoint ====================================================
    if(distanceToNext > 0.f && distanceToNext < map::INVALID_DISTANCE_VALUE)
    {
      if(!result.isFuelToNextValid())
      {
        result.fuelLbsToNext = distanceToNext / aircraftGroundSpeed * aircraftFuelFlowLbs;
        result.fuelGalToNext = distanceToNext / aircraftGroundSpeed * aircraftFuelFlowGal;
      }

      if(!result.isTimeToNextValid())
        result.timeToNext = distanceToNext / aircraftGroundSpeed;
    }
  } // if(aircraftFuelFlowLbs > 0.01f && aircraftGroundSpeed > map::MIN_GROUND_SPEED)

  if(alternate)
  {
    // Destination is the same as next for alternate legs
    result.fuelLbsToDest = result.fuelLbsToNext;
    result.fuelGalToDest = result.fuelGalToNext;
    result.timeToDest = result.timeToNext;
  }

  if(missed)
  {
    // RouteAltitude legs does not provide values for missed - calculate them based on aircraft
    result.fuelLbsToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowLbs;
    result.fuelGalToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowGal;
    result.timeToDest = distanceToDest / aircraftGroundSpeed;
  }
}

float RouteAltitude::adjustAltitudeForRestriction(float altitude, const proc::MapAltRestriction& restriction) const
{
  bool forceFinal = restriction.forceFinal;

  if(restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
  {
    // Take altitude calculated from vertical angle and check it against restriction
    altitude = restriction.verticalAngleAlt;

    // Ignore altitude forced down to minimum at FACF or FAF
    forceFinal = false;
  }

  switch(restriction.descriptor)
  {
    case proc::MapAltRestriction::NO_ALT_RESTR:
      break;

    case proc::MapAltRestriction::AT:
      altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::ILS_AT:
      // Do not consider ILS_AT a restriction if overridden by vertical path
      if(!(restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE))
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
      // Do not consider ILS_AT_OR_ABOVE a restriction if overridden by vertical path
      if(!(restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE) && (forceFinal || altitude < restriction.alt1))
        // Stick to lowest altitude on FAF or FACF
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_ABOVE:
      if(forceFinal || altitude < restriction.alt1)
        // Stick to lowest altitude on FAF or FACF
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_BELOW:
      if(forceFinal || altitude > restriction.alt1)
        // Stick to lowest altitude on FAF or FACF
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::BETWEEN:
      if(forceFinal)
        // Stick to lowest altitude on FAF or FACF
        altitude = restriction.alt2;
      else
      {
        if(altitude > restriction.alt1)
          altitude = restriction.alt1;

        if(altitude < restriction.alt2)
          altitude = restriction.alt2;
      }
      break;
  }
  return altitude;
}

void RouteAltitude::adjustVertAngleAltForRestriction(proc::MapAltRestriction& restriction) const
{
  if(!(restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE))
    return;

  switch(restriction.descriptor)
  {
    case proc::MapAltRestriction::NO_ALT_RESTR:
      break;

    case proc::MapAltRestriction::AT:
    case proc::MapAltRestriction::ILS_AT:
    case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
    case proc::MapAltRestriction::AT_OR_ABOVE:
    case proc::MapAltRestriction::AT_OR_BELOW:
      if(atools::almostEqual(restriction.verticalAngleAlt, restriction.alt1, VERT_ANGLE_ALT_ADJUST_LIMIT_FT))
        restriction.verticalAngleAlt = restriction.alt1;
      break;

    case proc::MapAltRestriction::BETWEEN:
      if(atools::almostEqual(restriction.verticalAngleAlt, restriction.alt1, VERT_ANGLE_ALT_ADJUST_LIMIT_FT))
        restriction.verticalAngleAlt = restriction.alt1;
      if(atools::almostEqual(restriction.verticalAngleAlt, restriction.alt2, VERT_ANGLE_ALT_ADJUST_LIMIT_FT))
        restriction.verticalAngleAlt = restriction.alt2;
      break;
  }
}

bool RouteAltitude::violatesAltitudeRestriction(QString& errorMessage, int legIndex) const
{
  const RouteAltitudeLeg& leg = value(legIndex);
  float legAlt = leg.y2();
  bool retval = false;

  if(!leg.isEmpty())
  {
    switch(leg.restriction.descriptor)
    {
      case proc::MapAltRestriction::NO_ALT_RESTR:
      case proc::MapAltRestriction::ILS_AT:
      case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
        break;

      case proc::MapAltRestriction::AT:
        retval = atools::almostNotEqual(legAlt, leg.restriction.alt1, AT_RESTRICTION_TOLERANCE_FT);
        break;

      case proc::MapAltRestriction::AT_OR_ABOVE:
        retval = legAlt < leg.restriction.alt1;
        break;

      case proc::MapAltRestriction::AT_OR_BELOW:
        retval = legAlt > leg.restriction.alt1;
        break;

      case proc::MapAltRestriction::BETWEEN:
        retval = legAlt > leg.restriction.alt1 || legAlt < leg.restriction.alt2;
        break;
    }

    if(!retval && leg.restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
      // Take altitude calculated from vertical angle and check it against restriction
      retval = atools::almostNotEqual(legAlt, leg.restriction.verticalAngleAlt, VERT_ANGLE_ALT_TOLERANCE_FT);

    if(retval)
      errorMessage = tr("Leg number %1, %2 (%3) at %4 violates altitude restriction \"%5\".").
                     arg(legIndex + 1).arg(leg.getIdent()).arg(leg.getProcedureType()).arg(Unit::altFeet(legAlt)).
                     arg(proc::altRestrictionText(leg.restriction));
  }

  return retval;
}

float RouteAltitude::findApproachMaxAltitude(int index) const
{
  float maxAlt = map::INVALID_ALTITUDE_VALUE;

  if(index > 1)
  {
    // Avoid crashes
    index = fixRange(index);

    if(index < map::INVALID_INDEX_VALUE)
    {
      int stopIndex = route->getDestinationIndexBeforeProcedure();

      // Check backwards from index for a arrival/STAR leg that limits the maximum altitude
      for(int i = index - 1; i >= 0; i--)
      {
        if(i < stopIndex)
          break;

        const RouteLeg& leg = route->value(i);
        const RouteLeg& next = route->value(i + 1);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyArrival() && leg.getProcedureLegAltRestr().isValid())
        {
          const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();

          if(r.descriptor == proc::MapAltRestriction::ILS_AT && next.isAnyProcedure() && next.getProcedureLeg().isVerticalAngleValid())
            // Ignore ILS restriction if next (right) procedure has a vertical angle - continue lookup
            continue;

          if(r.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
            // Altitude required by flight path angle
            maxAlt = std::min(maxAlt, r.verticalAngleAlt);
          else if(r.forceFinal || atools::contains(r.descriptor, {proc::MapAltRestriction::AT, proc::MapAltRestriction::AT_OR_BELOW,
                                                                  proc::MapAltRestriction::BETWEEN, proc::MapAltRestriction::ILS_AT}))
            // Either altitude is forced to lowest restriction value or altitude is limited by a "below", "at" or "between" restriction
            // Forced by FAF, FCAF
            maxAlt = std::min(maxAlt, r.alt1);
        }
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }
  return maxAlt;
}

float RouteAltitude::findDepartureMaxAltitude(int index) const
{
  float maxAlt = map::INVALID_ALTITUDE_VALUE;

  if(index > 1)
  {
    // Avoid crashes
    index = fixRange(index);

    // Check forward from index to end of SID for leg that limits the maximum altitude
    int end = fixRange(route->getSidLegsOffset() + route->getSidLegs().size());
    if(end == map::INVALID_INDEX_VALUE)
      // Search through the whole route
      end = route->size() - 1;

    int stopIndex = route->getLastIndexOfDepartureProcedure();

    if(index < map::INVALID_INDEX_VALUE && end < map::INVALID_INDEX_VALUE)
    {
      for(int i = index; i < end; i++)
      {
        if(i > stopIndex)
          break;

        const RouteLeg& leg = route->value(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyDeparture() && leg.getProcedureLegAltRestr().isValid())
        {
          const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();
          if(r.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
            // Altitude required by flight path angle - might occur when iterating into approach procedure
            maxAlt = std::min(maxAlt, r.verticalAngleAlt);
          else if(r.forceFinal || atools::contains(r.descriptor, {proc::MapAltRestriction::AT, proc::MapAltRestriction::AT_OR_BELOW,
                                                                  proc::MapAltRestriction::BETWEEN}))
            // Either altitude is forced to lowest restriction value or altitude is limited by a "below", "at" or "between" restriction
            // Forced by FAF, FCAF
            maxAlt = std::min(maxAlt, r.alt1);
        }
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }

  return maxAlt;
}

int RouteAltitude::findApproachFirstRestricion() const
{
  if(route->hasAnyApproachProcedure() || route->hasAnyStarProcedure())
  {
    // Search for first restriction in an arrival or STAR
    int start = route->getStarLegsOffset();
    if(!(start < map::INVALID_INDEX_VALUE))
      // No STAR - use arrival
      start = route->getApproachLegsOffset();

    if(start < map::INVALID_INDEX_VALUE)
    {
      for(int i = start; i < route->size(); i++)
      {
        const RouteLeg& leg = route->value(i);
        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyArrival() && leg.getProcedureLegAltRestr().isValid())
          return i;
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }
  return map::INVALID_INDEX_VALUE;
}

int RouteAltitude::findDepartureLastRestricion() const
{
  if(route->hasAnySidProcedure())
  {
    // Search for first restriction in a SID
    int start = fixRange(route->getSidLegsOffset() + route->getSidLegs().size());

    if(start < map::INVALID_INDEX_VALUE)
    {
      for(int i = start; i > 0; i--)
      {
        const RouteLeg& leg = route->value(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyDeparture() && leg.getProcedureLegAltRestr().isValid())
          return i;
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }
  return map::INVALID_INDEX_VALUE;
}

int RouteAltitude::fixRange(int index) const
{
  if(index < map::INVALID_INDEX_VALUE)
    return std::min(std::max(index, 0), size() - 1);

  return index;
}

void RouteAltitude::simplyfyRouteAltitudes()
{
  // Flatten descent legs starting from TOD to destination
  if(legIndexTopOfDescent < map::INVALID_INDEX_VALUE && legIndexTopOfDescent >= 0)
  {
    // Use several iterations
    for(int i = 0; i < 16; i++)
    {
      for(int j = legIndexTopOfDescent; j < route->getDestinationAirportLegIndex(); j++)
        simplifyRouteAltitude(j, false /* departure */);
    }
  }
  else
    qWarning() << Q_FUNC_INFO;

  // Flatten departure legs starting from departure to TOC
  if(legIndexTopOfClimb < map::INVALID_INDEX_VALUE && legIndexTopOfClimb >= 0)
  {
    // Use several iterations
    for(int i = 0; i < 8; i++)
    {
      for(int j = 1; j < legIndexTopOfClimb; j++)
        simplifyRouteAltitude(j, true /* departure */);
    }
  }
  else
    qWarning() << Q_FUNC_INFO;
}

void RouteAltitude::simplifyRouteAltitude(int index, bool departure)
{
  // #ifdef DEBUG_INFORMATION
  // qDebug() << Q_FUNC_INFO << index;
  // #endif

  if(index <= 0 || index >= size() - 1)
  {
    qWarning() << Q_FUNC_INFO << "index <= 0 || index >= size() - 1";
    return;
  }

  RouteAltitudeLeg *midAlt = &(*this)[index];
  RouteAltitudeLeg *leftAlt = &(*this)[index - 1];
  RouteAltitudeLeg *rightAlt = &(*this)[index + 1];
  RouteAltitudeLeg *rightSkippedAlt = nullptr, *leftSkippedAlt = nullptr;

  // Check if left neighbor has zero distance to this one, i.e. left.x2 = mid.x1 = mid.x2
  if(midAlt->isPoint() && index >= 2)
  {
    if(leftAlt->geometry.size() <= 2)
    {
      // This duplicate leg with the same position (IAF, etc.) will be skipped
      leftSkippedAlt = &(*this)[index - 1];

      // Adjust leg if two equal legs (IAF) are after each other - otherwise no change will be done
      leftAlt = &(*this)[index - 2];
    }
  }

  // Check if the right neighbor has zero distance to this one, i.e. mid.x2 = right.x1 = right.x2
  if(rightAlt->isPoint() && index < size() - 2)
  {
    if(rightAlt->geometry.size() <= 2)
    {
      // This duplicate leg with the same position (IAF, etc.) will be skipped
      rightSkippedAlt = &(*this)[index + 1];

      // Adjust leg if two equal legs (IAF) are after each other - otherwise no change will be done
      rightAlt = &(*this)[index + 2];

      // Right leg is too far after destination - quit here
      if(rightAlt->isAlternate() || rightAlt->isMissed())
        return;
    }
  }

#ifdef DEBUG_INFORMATION_ROUTE_ALT_SIMPLIFY
  qDebug() << Q_FUNC_INFO
           << leftAlt->ident
           << leftAlt->procedureType
           << QString("(%1)").arg(leftSkippedAlt != nullptr ? leftSkippedAlt->ident : QString("-"))
           << midAlt->ident
           << QString("(%1)").arg(rightSkippedAlt != nullptr ? rightSkippedAlt->ident : QString("-"))
           << rightAlt->ident
           << rightAlt->procedureType
           << "departure" << departure;
#endif

  // Avoid dummy legs (e.g. missed approach)
  if(!leftAlt->isEmpty() && !rightAlt->isEmpty() && !midAlt->isEmpty())
  {
    QPointF leftPt = leftAlt->asPoint();
    QPointF rightPt = rightAlt->asPoint();

    if(midAlt->geometry.size() >= 3)
    {
      // This leg contains a TOD or/and TOC
      if(departure)
        // Use TOD/TOC position from this instead of the one from the right leg
        rightPt = midAlt->geometry.at(1);
      else
        // Arrival
        // Use TOD/TOC position from this instead of the one from the left leg
        leftPt = midAlt->geometry.at(midAlt->geometry.size() - 2);
    }

    if(departure && rightAlt->geometry.size() >= 3)
      // Right one is a TOD or TOC take the geometry at the bend
      rightPt = rightAlt->geometry.at(1);

    // Create a direct line from left to right ignoring middle leg
    QLineF line(leftPt, rightPt);

    // Calculate altitude for the middle leg using the direct line
    QPointF midPt = midAlt->asPoint();
    double t = (midPt.x() - leftPt.x()) / line.dx();

    // 0 = left, 1 = right
    QPointF mid = line.pointAt(t);

#ifdef DEBUG_INFORMATION_ROUTE_ALT_SIMPLIFY
    qDebug() << Q_FUNC_INFO << "old" << midAlt->y2() << "new" << mid.y();
#endif

    // Apply limitations for skipped (close) waypoints
    float newAlt = adjustAltitudeForRestriction(static_cast<float>(mid.y()), midAlt->restriction);
    if(rightSkippedAlt != nullptr)
      newAlt = adjustAltitudeForRestriction(newAlt, rightSkippedAlt->restriction);
    if(leftSkippedAlt != nullptr)
      newAlt = adjustAltitudeForRestriction(newAlt, leftSkippedAlt->restriction);

    if(!departure)
      // Make new altitude never below next or right one for approaches. Avoid climb when forcing a FAF or FACF before a vertical path
      newAlt = std::max(newAlt, static_cast<float>(rightPt.y()));

#ifdef DEBUG_INFORMATION_ROUTE_ALT_SIMPLIFY
    qDebug() << Q_FUNC_INFO << "after adjust" << newAlt;
#endif

    // Change middle leg and adjust altitude
    midAlt->setY2(newAlt);

    // Also change skipped right neighbor
    if(rightSkippedAlt != nullptr)
      rightSkippedAlt->setAlt(newAlt);

    rightAlt->setY1(newAlt);

    // Also change skipped left neighbor
    if(leftSkippedAlt != nullptr)
      leftSkippedAlt->setY2(newAlt);
  }
}

void RouteAltitude::collectErrors(const QStringList& altRestrErrors)
{
  if(!altRestrErrors.isEmpty())
  {
    errors.append(tr("Check the cruise altitude and procedures of the flight plan."));
    errors.append(altRestrErrors);
  }
  else if(!(getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE && getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE))
    errors.append(tr("Cannot calculate top of climb or top of descent. "
                     "Possible reasons:<ul>"
                     "<li>The flight plan is too short or the cruise altitude is too high.</li>"
                       "<li>Climb and descent speeds in the aircraft performance data are too low.</li>"
                         "<li>Departure or destination airport elevation is above cruise altitude.</li>"
                           "<li>Cruise altitude violates one or more procedure altitude restrictions.</li></ul>"));
}

void RouteAltitude::calculateAll(const atools::fs::perf::AircraftPerf& perf, float cruiseAltitudeFt)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << perf.getAircraftType() << cruiseAltitudeFt;
#endif

  // Get default climb speed
  climbSpeedWindCorrected = perf.getClimbSpeed();
  cruiseSpeedWindCorrected = perf.getCruiseSpeed();
  descentSpeedWindCorrected = perf.getDescentSpeed();

  // Calculate default climb rate
  climbRateWindFtPerNm = perf.getClimbVertSpeed() * 60.f / climbSpeedWindCorrected;
  descentRateWindFtPerNm = perf.getDescentVertSpeed() * 60.f / descentSpeedWindCorrected;

  cruiseAltitude = cruiseAltitudeFt;

  errors.clear();
  clearAll();
  bool invalid = false;

  // Collect basic issues ================================================
  if(route->isEmpty())
    invalid = true;
  else if(route->getSizeWithoutAlternates() == 1)
    invalid = true;

  // Check again if there is a plan ===============================================
  if(!invalid)
  {
    if(route->getTotalDistance() < MIN_FLIGHTPLAN_DIST_NM)
    {
      errors.append(tr("Flight plan is too short."));
      qWarning() << Q_FUNC_INFO << "Flight plan too short";
      invalid = true;
    }

    if(cruiseAltitude < MIN_CRUISE_ALTITUDE_FT)
    {
      errors.append(tr("Cruise altitude is too low."));
      qWarning() << Q_FUNC_INFO << "Cruise altitude is too low";
      invalid = true;
    }

    const RouteLeg destinationLeg = route->getDestinationAirportLeg();
    if(!destinationLeg.isValidWaypoint())
    {
      errors.append(tr("Destination is not valid."));
      qWarning() << Q_FUNC_INFO << "Destination is not valid or neither airport nor runway";
      invalid = true;
    }

    const RouteLeg departureLeg = route->getDepartureAirportLeg();
    if(!departureLeg.isValidWaypoint())
    {
      errors.append(tr("Departure is not valid."));
      qWarning() << Q_FUNC_INFO << "Departure is not valid or neither airport nor runway";
      invalid = true;
    }
  }

  if(!invalid)
  {
    // Flight plan is valid so far s===============================================
    QStringList altRestrErrors;
    calculate(altRestrErrors);
    collectErrors(altRestrErrors);

    if(validProfile)
    {
      calculateTrip(perf);

      // Recalculate more iterations to get more accuracy.
      // Wind related path changes can get the profile into different wind conditions changing the profile again.
      for(int i = 0; i < 3; i++)
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "iteration" << i
                 << "windHeadClimb" << windHeadClimb
                 << "windHeadCruise" << windHeadCruise
                 << "climbSpeedWindCorrected" << climbSpeedWindCorrected
                 << "descentSpeedWindCorrected" << descentSpeedWindCorrected
                 << "perf.getClimbSpeed()" << perf.getClimbSpeed()
                 << "perf.getDescentSpeed()" << perf.getDescentSpeed();

        qDebug() << Q_FUNC_INFO << "error climb" << climbSpeedWindCorrected - perf.getClimbSpeed();
        qDebug() << Q_FUNC_INFO << "error descent" << descentSpeedWindCorrected - perf.getDescentSpeed();
#endif

        climbRateWindFtPerNm = perf.getClimbVertSpeed() * 60.f / climbSpeedWindCorrected;
        descentRateWindFtPerNm = perf.getDescentVertSpeed() * 60.f / descentSpeedWindCorrected;

#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "iteration" << i << "climbRateWindFtPerNm" << climbRateWindFtPerNm
                 << "descentRateWindFtPerNm" << descentRateWindFtPerNm;
#endif

        clearAll();
        calculate(altRestrErrors);
        collectErrors(altRestrErrors);

        if(validProfile)
          calculateTrip(perf);
        else
          break;
      }
    }
  } // if(!invalid)

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
  qDebug() << "windDirection" << windDirectionAvg << "windSpeed" << windSpeedAvg << "windHead" << windHeadAvg
           << "windHeadClimb" << windHeadClimb << "windHeadCruise" << windHeadCruise
           << "windHeadDescent" << windHeadDescent;
  qDebug() << "distanceTopOfClimb" << distanceTopOfClimb << "distanceTopOfDescent" << distanceTopOfDescent;
  qDebug() << "tripFuel" << tripFuel << "alternateFuel" << alternateFuel << "travelTime" << travelTime
           << "averageGroundSpeed" << averageGroundSpeed;
  qDebug() << "climbFuel" << climbFuel << "cruiseFuel" << cruiseFuel << "descentFuel" << descentFuel;
  qDebug() << "climbTime" << climbTime << "cruiseTime" << cruiseTime << "descentTime" << descentTime;
  qDebug() << "legIndexTopOfClimb" << legIndexTopOfClimb << "legIndexTopOfDescent" << legIndexTopOfDescent;
  qDebug() << "validProfile" << validProfile << "unflyableLegs" << unflyableLegs;
  qDebug() << "climbRateWindFtPerNm" << climbRateWindFtPerNm << "descentRateWindFtPerNm" << descentRateWindFtPerNm
           << "cruiseAltitide" << cruiseAltitude;

  qDebug() << Q_FUNC_INFO << "exit";
#endif
}

void RouteAltitude::calculate(QStringList& altRestErrors)
{
  altRestErrors.clear();

  if(route->getSizeWithoutAlternates() <= 1)
    return;

  // Prefill all legs with distance and cruise altitude
  calculateDistances();

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "After calculateDistances() ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif

  // Calculate arrival first to get vertical path altitudes right which are needed by calculateDeparture()
  if(calcTopOfDescent)
    calculateArrival();

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "After calculateArrival() ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif

  if(calcTopOfClimb)
    calculateDeparture();

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "After calculateDeparture() ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif

  // Check for violations because of too low cruise but only if TOD and TOC calculation succeeded
  if(getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE && getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
  {
    for(int i = 0; i < size(); i++)
    {
      const RouteAltitudeLeg& leg = value(i);

      if(!leg.isMissed() && !leg.isAlternate())
      {
        QString errorMessage;
        bool err = violatesAltitudeRestriction(errorMessage, i);

        if(err)
        {
#ifdef DEBUG_INFORMATION
          qWarning() << Q_FUNC_INFO << "violating messge" << errorMessage << "leg" << leg;
#endif
          altRestErrors.append(errorMessage);
        }
      }
    }

    if(!altRestErrors.isEmpty() && route->hasAnyStarProcedure() && route->hasAnyArrivalProcedure())
      altRestErrors.prepend(tr("STAR might not fit to the approach procedure due to altitude or vertical path restrictions."));
  }

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "Before cleanup ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif

  if(!altRestErrors.isEmpty() || distanceTopOfClimb > distanceTopOfDescent ||
     (calcTopOfClimb && !(distanceTopOfClimb < map::INVALID_DISTANCE_VALUE)) ||
     (calcTopOfDescent && !(distanceTopOfDescent < map::INVALID_DISTANCE_VALUE)))
  {
    // TOD and TOC overlap or are invalid or restrictions violated  - cruise altitude is too high
    clearAll();

    // Reset all to cruise level - profile will print a message
    calculateDistances();

    validProfile = false;
  }
  else
  {
    // Success - flatten legs
    if(simplify && (calcTopOfClimb || calcTopOfDescent))
      simplyfyRouteAltitudes();

    validProfile = true;
  }

  // Fill vertical angles
  calculateGeoAngles();

  // Set coordinates into legs
  fillGeometry();

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "Finished ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif
}

void RouteAltitude::calculateDistances()
{
  float distanceToLeg = 0.f;
  int destinationLegIdx = route->getDestinationLegIndex();
  int destinationAirportLegIdx = route->getDestinationAirportLegIndex();
  const RouteLeg& destinationAirportLeg = route->getDestinationAirportLeg();

  if(destinationLegIdx == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO;
    return;
  }

  // Fill all legs with distance and cruise altitude and add them to the vector
  for(int i = 0; i < route->size(); i++)
  {
    const RouteLeg& leg = route->value(i);

    RouteAltitudeLeg alt;

    alt.ident = leg.getIdent();
    alt.procedureType = proc::procedureTypeText(leg.getProcedureType());

    if(i <= destinationLegIdx || i == destinationAirportLegIdx)
    {
      // Not a dummy (missed)
      alt.restriction = leg.getProcedureLegAltRestr();
      alt.verticalAngle = leg.getProcedureLeg().verticalAngle;
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitude));
      distanceToLeg += leg.getDistanceTo();
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitude));
    }
    // else ignore missed approach dummy legs after destination runway

    if(leg.isAlternate())
    {
      alt.geometry.append(QPointF(0., destinationAirportLeg.getAltitude()));
      alt.geometry.append(QPointF(leg.getDistanceTo(), leg.getAltitude()));
    }

    append(alt);
  }

  // Set the flags which are needed for drawing
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->value(i);
    const RouteLeg& last = route->value(i - 1);
    RouteAltitudeLeg& altLeg = (*this)[i];
    const RouteAltitudeLeg& lastAltLeg = value(i - 1);

    if(leg.getProcedureLeg().isAnyArrival() && altLeg.isPoint() && lastAltLeg.restriction.isValid())
    {
      // If this is a point like an IF leg, copy restriction from last leg but save force flag
      bool forceFinal = altLeg.restriction.forceFinal;
      altLeg.restriction = lastAltLeg.restriction;
      altLeg.restriction.forceFinal = forceFinal;
    }

    altLeg.missed = leg.isAnyProcedure() && leg.getProcedureLeg().isMissed();
    altLeg.alternate = leg.isAlternate();

    if(last.isRoute() || leg.isRoute() || // Any is route - also covers STAR to airport
       (last.getProcedureLeg().isAnyDeparture() && leg.getProcedureLeg().isAnyArrival()) || // empty space from SID to STAR, transition or approach
       (last.getProcedureLeg().isStar() && leg.getProcedureLeg().isArrival())) // empty space from STAR to transition or approach
      altLeg.procedure = false;
    else
      altLeg.procedure = true;
  }
}

void RouteAltitude::calculateDeparture()
{
  // First leg of departure procedure. 1 if SID used otherwise 0.
  int departureLegIdx = route->getSidLegIndex();

  if(departureLegIdx == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO << "departureLegIdx" << departureLegIdx;
    return;
  }

  if(climbRateWindFtPerNm < 1.f)
  {
    qWarning() << Q_FUNC_INFO << "climbRateWindFtPerNm " << climbRateWindFtPerNm;
    return;
  }

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "climbRateWindFtPerNm" << climbRateWindFtPerNm;
#endif

  float departAlt = getDepartureAltitude();

  if(departureLegIdx > 0) // Assign altitude to dummy for departure airport too
    first().setAlt(departAlt);

  // Start from departure forward until hitting cruise altitude (TOC) or destination airport or runway
  for(int i = departureLegIdx; i <= route->getDestinationLegIndex(); i++)
  {
    // Calculate altitude for this leg based on altitude of the leg to the right
    RouteAltitudeLeg& alt = (*this)[i];
    if(alt.isEmpty())
      // Dummy leg
      continue;

    // Altitude of last leg
    float lastAlt = i > departureLegIdx ? value(i - 1).y2() : departAlt;
    float newAltitude = lastAlt;

    if(i <= departureLegIdx)
      // Departure leg - assign altitude to airport and RW too if available
      alt.setAlt(departAlt);
    else
    {
      // Set geometry for climbing
      alt.setY1(lastAlt);

      // Use a default value of 3 nm per 1000 ft if performance is not available
      newAltitude = lastAlt + alt.getDistanceTo() * climbRateWindFtPerNm;
    }

    if(!alt.isEmpty())
    {
      // Remember geometry which is not changed by altitude restrictions for calculation of cruise intersection
      float uncorrectedAltitude = newAltitude;

      // Get new altitude corrected by restrictions
      newAltitude = adjustAltitudeForRestriction(newAltitude, alt.restriction);

      // Avoid climbing above any below/at/between restrictions
      float maxAlt = findDepartureMaxAltitude(i);
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
        newAltitude = std::min(newAltitude, maxAlt);

      bool altitudeRestricts = false;
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
      {
        newAltitude = std::min(newAltitude, maxAlt);

        // Avoid climbing above next limiting restriction - no TOC yet
        altitudeRestricts = maxAlt < cruiseAltitude;
      }

      // Never lower than last leg
      newAltitude = std::max(newAltitude, lastAlt);

      if(!altitudeRestricts && i > 0 && !(distanceTopOfClimb < map::INVALID_ALTITUDE_VALUE) && uncorrectedAltitude > cruiseAltitude)
      {
        // Reached TOC - calculate distance
        distanceTopOfClimb = distanceForAltitude(alt.geometry.constFirst(), QPointF(alt.geometry.constLast().x(), uncorrectedAltitude),
                                                 cruiseAltitude);
        legIndexTopOfClimb = i;

        // Adjust this leg
        newAltitude = std::min(newAltitude, cruiseAltitude);
        newAltitude = std::max(newAltitude, lastAlt);

        // Add point to allow drawing the bend at TOC
        alt.geometry.insert(1, QPointF(distanceTopOfClimb, cruiseAltitude));

        // Done here =================================================
        alt.topOfClimb = true;
        break;
      }
    }

    // Adjust altitude to be above last and below cruise
    alt.setY2(std::min(newAltitude, cruiseAltitude));
    alt.setY2(std::max(newAltitude, lastAlt));
  }
}

void RouteAltitude::calculateArrival()
{
  // Either destination airport or last leg of approach procedure (usually runway) before missed.
  int destinationLegIdx = route->getDestinationLegIndex();

  // Destination airport after missed (if any) and one before the alternate if any.
  int destinationAirportLegIndex = route->getDestinationAirportLegIndex();

  int departureLegIndex = route->getSidLegIndex();
  float lastAlt = getDestinationAltitude();

  if(departureLegIndex == map::INVALID_INDEX_VALUE || destinationLegIdx == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO << "departureLegIdx" << departureLegIndex << "destinationLegIdx" << destinationLegIdx;
    return;
  }

  if(descentRateWindFtPerNm < 1.f)
  {
    qWarning() << Q_FUNC_INFO << "descentRateWindFtPerNm " << descentRateWindFtPerNm;
    return;
  }

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << Q_FUNC_INFO << "descentRateWindFtPerNm" << descentRateWindFtPerNm;
#endif

  // Calculate from last leg down until we hit the cruise altitude (TOD)
  // Need to step from procedure over missed so that airport is caught too
  for(int i = destinationAirportLegIndex; i >= 0; i--)
  {
    RouteAltitudeLeg& alt = (*this)[i];

    if(alt.isMissed())
      continue;

    RouteAltitudeLeg *lastAltLeg = i < destinationLegIdx ? &(*this)[i + 1] : nullptr;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
    qDebug() << Q_FUNC_INFO << "alt.getIdent()" << alt.getIdent();

    if(lastAltLeg != nullptr)
      qDebug() << Q_FUNC_INFO << "lastAltLeg->getIdent()" << lastAltLeg->getIdent();
    else
      qDebug() << Q_FUNC_INFO << "lastAltLeg is null";
#endif

    // Use altitude of last leg
    float lastLegAlt = lastAltLeg != nullptr ? lastAltLeg->y2() : 0.;

    // Use altitude of airport if destination
    if(i == destinationAirportLegIndex)
      lastLegAlt = route->value(i).getAltitude();

    // Start with altitude of the right leg (close to dest)
    float newAltitude = lastLegAlt;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
    qDebug() << Q_FUNC_INFO << "lastLegAlt" << lastLegAlt;
#endif

    if(i <= destinationLegIdx)
    {
      // Leg leading to this point (right to this)
      float distFromRight = lastAltLeg != nullptr ? lastAltLeg->getDistanceTo() : 0.f;

      // Point of this leg
      if(lastAltLeg != nullptr && lastAltLeg->isVerticalProcAngleValid())
      {
        // Calculate next altitude from vertical angle and use this as new altitude
        // tan(alpha)=a/b - looking for b
        alt.restriction.verticalAngleAlt = lastAlt + ageo::nmToFeet(ageo::tanDeg(-lastAltLeg->verticalAngle) * distFromRight);

        // Correct calculated verticalAngleAlt since there might be small differences causing validation errors
        adjustVertAngleAltForRestriction(alt.restriction);
        newAltitude = alt.restriction.verticalAngleAlt;
      }
      else
      {
        // Use a default value of 3 nm per 1000 ft if performance is not available
        newAltitude = lastAlt + distFromRight * descentRateWindFtPerNm;
        alt.restriction.verticalAngleAlt = map::INVALID_ALTITUDE_VALUE;
      }
    }

#ifdef DEBUG_INFORMATION_ROUTE_ALT
    qDebug() << Q_FUNC_INFO << "newAltitude" << newAltitude;
#endif

    if(!alt.isEmpty())
    {
      // Remember geometry which is not changed by altitude restrictions for calculation of cruise intersection
      float uncorrectedAltitude = newAltitude;

      // Get new altitude corrected by restrictions
      newAltitude = adjustAltitudeForRestriction(newAltitude, alt.restriction);

#ifdef DEBUG_INFORMATION_ROUTE_ALT
      qDebug() << Q_FUNC_INFO << "newAltitude adjusted" << newAltitude;
#endif

      // Correct calculated altitude restriction if the result from the path angle is not accurate
      if(alt.restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
        alt.restriction.verticalAngleAlt = newAltitude;

      // Avoid climbing (up the descent slope) above any below/at/between restrictions
      float maxAlt = findApproachMaxAltitude(i + 1);
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
      {
        // Adjust only if there is no vertical angle restriction on the next/right leg
        if(lastAltLeg == nullptr || !lastAltLeg->isVerticalProcAngleValid())
          newAltitude = std::min(newAltitude, maxAlt);
      }

#ifdef DEBUG_INFORMATION_ROUTE_ALT
      qDebug() << Q_FUNC_INFO << "newAltitude adjusted to maxAlt" << newAltitude;
#endif

      // Never lower than right leg
      newAltitude = std::max(newAltitude, lastAlt);

#ifdef DEBUG_INFORMATION_ROUTE_ALT
      qDebug() << Q_FUNC_INFO << "newAltitude adjusted to lastAlt" << newAltitude;
#endif

      // Check the limits to the left - either cruise or an altitude restriction
      // Set to true if an altitude restriction is limiting and postone calculation of the TOD
      bool altitudeRestricts = maxAlt < map::INVALID_ALTITUDE_VALUE && maxAlt <= cruiseAltitude;

      if(!altitudeRestricts && !(distanceTopOfDescent < map::INVALID_ALTITUDE_VALUE) &&
         uncorrectedAltitude > cruiseAltitude && i + 1 < size())
      {
        if(value(i + 1).isEmpty())
        {
          // Stepped into the dummies after arrival runway - bail out
#ifdef DEBUG_INFORMATION_ROUTE_ALT
          qDebug() << Q_FUNC_INFO << "Stepped into dummies";
#endif
          break;
        }

        // Reached TOD - calculate distance
        distanceTopOfDescent = distanceForAltitude(value(i + 1).getGeometry().constLast(),
                                                   QPointF(alt.getDistanceFromStart(), uncorrectedAltitude), cruiseAltitude);
        legIndexTopOfDescent = i + 1;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
        qDebug() << Q_FUNC_INFO << "distanceTopOfDescent" << distanceTopOfDescent << "uncorrectedAltitude" << uncorrectedAltitude;
#endif

        if(lastAltLeg != nullptr)
        {
          if(!lastAltLeg->topOfClimb)
            // Adjust only if not modified by TOC calculation
            alt.setY2(std::min(newAltitude, cruiseAltitude));

          // Adjust neighbor too
          lastAltLeg->setY1(alt.y2());

          // Append point to allow drawing the bend at TOD - TOC position might be already included in leg
          lastAltLeg->geometry.insert(lastAltLeg->geometry.size() - 1, QPointF(distanceTopOfDescent, cruiseAltitude));
        }

        if(legIndexTopOfDescent < size())
          (*this)[legIndexTopOfDescent].topOfDescent = true;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
        qDebug() << Q_FUNC_INFO << "Done";
#endif
        // Done here  =================================================
        break;
      }

      // Adjust altitude to be above last and below cruise
      alt.setY2(newAltitude);
      alt.setY2(std::min(alt.y2(), cruiseAltitude));

      if(lastAltLeg != nullptr)
        alt.setY2(std::max(alt.y2(), lastLegAlt));
      if(i == destinationLegIdx && i != departureLegIndex && !alt.topOfClimb)
        alt.setY1(alt.y2());

      if(lastAltLeg != nullptr)
        lastAltLeg->setY1(alt.y2());

      lastAlt = alt.y2();
    }
  }
}

void RouteAltitude::calculateGeoAngles()
{
  for(RouteAltitudeLeg& leg : *this)
  {
    for(int i = 1; i < leg.geometry.size(); i++)
    {
      // Leg length
      double xdiff = ageo::nmToFeet(leg.geometry.at(i).x() - leg.geometry.at(i - 1).x()); // Always positive

      // Altitude difference
      double ydiff = leg.geometry.at(i).y() - leg.geometry.at(i - 1).y();

      if(std::abs(xdiff) > 0.f && std::abs(ydiff) > 0.f)
        leg.angles.append(static_cast<float>(ageo::atan2Deg(ydiff, xdiff)));
      else
        leg.angles.append(0.f);
    }
  }
}

void RouteAltitude::fillGeometry()
{
  Q_ASSERT(route->size() == size());

  const RouteLeg& destinationAirportLeg = route->getDestinationAirportLeg();

  for(int i = 0; i < route->size(); i++)
  {
    RouteAltitudeLeg& altLeg = (*this)[i];
    const RouteLeg& routeLeg = route->value(i);

    altLeg.line.clear();
    altLeg.geoLine.clear();

    if(altLeg.isAlternate())
    {
      altLeg.line.append(destinationAirportLeg.getPosition());
      altLeg.line.append(routeLeg.getPosition());
      altLeg.geoLine = altLeg.line;
    }
    else
    {
      if(altLeg.isPoint())
      {
        altLeg.line.append(routeLeg.getPosition().alt(altLeg.y1()));
        altLeg.geoLine = altLeg.line;
      }
      else
      {
        if(altLeg.isAnyProcedure())
        {
          // Get full procedure geometry
          for(const ageo::Pos& pos : routeLeg.getProcedureLeg().geometry)
            altLeg.geoLine.append(pos.alt(altLeg.y1()));
        }

        // Build simple line consisting of start and end - might receive TOD and/or TOC position later
        if(i > 0)
        {
          const RouteLeg& prevLeg = route->value(i - 1);
          if(i == 1 && prevLeg.getDeparturePosition().isValid() && routeLeg.getProcedureLeg().isAnyDeparture())
            // Use departure parking spot as start position for "proceeed to runway" if this is the start of an SID
            altLeg.line.append(prevLeg.getDeparturePosition().alt(altLeg.y1()));
          else
            altLeg.line.append(route->value(i - 1).getPosition().alt(altLeg.y1()));
        }

        altLeg.line.append(routeLeg.getPosition().alt(altLeg.y2()));

        if(!altLeg.isAnyProcedure())
          // Not a procedure - use simple line
          altLeg.geoLine = altLeg.line;
      }

      // Add TOD and TOC =======================
      if(altLeg.topOfClimb)
        altLeg.line.insert(1, route->getPositionAtDistance(distanceTopOfClimb).alt(cruiseAltitude));

      if(altLeg.topOfDescent)
        altLeg.line.insert(altLeg.line.size() - 1, route->getPositionAtDistance(distanceTopOfDescent).alt(cruiseAltitude));
    }

    if(!altLeg.line.hasAllValidPoints())
      qWarning() << Q_FUNC_INFO << "Invalid points";
  }
}

float RouteAltitude::getDestinationAltitude() const
{
  const RouteLeg& destLeg = route->getDestinationLeg();

  if(!destLeg.isValidWaypoint())
    qWarning() << Q_FUNC_INFO << "dest leg not valid";
  else
  {
    if(destLeg.isAnyProcedure() && destLeg.getProcedureLegAltRestr().isValid())
    {
      if(destLeg.getRunwayEnd().isValid())
        // Use restriction from procedure - field elevation + 50 ft
        return destLeg.getProcedureLegAltRestr().alt1;
      else
        // Not valid - start at cruise at a waypoint
        return adjustAltitudeForRestriction(cruiseAltitude, destLeg.getProcedureLegAltRestr());
    }
    else
      // Airport altitude
      return destLeg.getAltitude();
  }

  // Other leg types (waypoint, VOR, etc.) remain on cruise
  return cruiseAltitude;
}

float RouteAltitude::getDestinationDistance() const
{
  int idx = route->getDestinationLegIndex();

  if(idx < map::INVALID_INDEX_VALUE)
    return value(idx).getDistanceFromStart();
  else
    return map::INVALID_DISTANCE_VALUE;
}

float RouteAltitude::getDepartureAltitude() const
{
  const RouteLeg& startLeg = route->value(route->getSidLegIndex());
  if(startLeg.isAnyProcedure() && startLeg.getProcedureLegAltRestr().isValid())
  {
    if(startLeg.getRunwayEnd().isValid())
      // Use restriction from procedure - airport dummy or runway - field elevation + 50 ft
      return startLeg.getProcedureLegAltRestr().alt1;
    else
      // Not valid - start at cruise at a waypoint
      return adjustAltitudeForRestriction(cruiseAltitude, startLeg.getProcedureLegAltRestr());
  }
  else
    // Airport altitude
    return startLeg.getAltitude();
}

float RouteAltitude::distanceForAltitude(const QPointF& leg1, const QPointF& leg2, float altitude)
{
  QLineF curLeg(leg1, leg2);

  // 0 = left/low 1 = this/right/high
  double t = (altitude - curLeg.y1()) / curLeg.dy();

  if(t < 0. || t > 1.)
    // Straight does not touch given altitude
    return map::INVALID_DISTANCE_VALUE;
  else
    return static_cast<float>(curLeg.pointAt(t).x());
}

float RouteAltitude::distanceForAltitude(const RouteAltitudeLeg& leg, float altitude)
{
  return distanceForAltitude(leg.geometry.constFirst(), leg.geometry.constLast(), altitude);
}

void RouteAltitude::calculateTrip(const atools::fs::perf::AircraftPerf& perf)
{
  if(isEmpty())
    return;

  WindReporter *windReporter = NavApp::getWindReporter();

  climbFuel = cruiseFuel = descentFuel = climbTime = cruiseTime = descentTime = tripFuel = alternateFuel = 0.f;

  travelTime = 0.f;
  unflyableLegs = false;
  averageGroundSpeed = windDirectionAvg = windSpeedAvg =
    windDirectionCruiseAvg = windSpeedCruiseAvg =
      windHeadAvg = windHeadClimb = windHeadCruise = windHeadDescent =
        climbSpeedWindCorrected = cruiseSpeedWindCorrected = descentSpeedWindCorrected = 0.f;

  float tocDist = getTopOfClimbDistance();
  float todDist = getTopOfDescentDistance();

  if(!(tocDist < map::INVALID_DISTANCE_VALUE) || !(todDist < map::INVALID_DISTANCE_VALUE))
  {
    qWarning() << Q_FUNC_INFO << "tocDist" << tocDist << "todDist" << todDist;
    return;
  }

  for(int i = 0; i < size(); i++)
  {
    RouteAltitudeLeg& leg = (*this)[i];
    float legDist = leg.getDistanceTo();

#ifdef DEBUG_INFORMATION_ROUTE_ALT
    qDebug() << Q_FUNC_INFO << "leg.getIdent()" << leg.getIdent();
#endif

    if(atools::almostEqual(legDist, 0.f))
      // same as last one - no travel time and no fuel
      continue;

    if(leg.isAlternate())
    {
      // Alternate legs are calculated from destination airport - use cruise if alternate speed is not set
      float averageSpeedKts = perf.getAlternateSpeed() > 1.f ? perf.getAlternateSpeed() : perf.getCruiseSpeed();
      leg.cruiseTime = legDist / averageSpeedKts;
      leg.cruiseFuel = (perf.getAlternateFuelFlow() > 0.f ?
                        perf.getAlternateFuelFlow() :
                        perf.getCruiseFuelFlow()) * leg.cruiseTime;

      // No wind data here since altitude is unknown
    }
    else
    {
      // Beginning and end of this leg
      float startDistLeg = leg.getDistanceFromStart() - leg.getDistanceTo();
      float endDistLeg = leg.getDistanceFromStart();
      const ageo::LineString& legLine = leg.getLineString();

      // Reset all variables
      float climbDist = 0.f, cruiseDist = 0.f, descentDist = 0.f;
      float climbSpeed = 0.f, cruiseSpeed = 0.f, descentSpeed = 0.f;
      atools::grib::Wind climbWind, cruiseWind, descentWind;

      // Check if leg covers TOC and/or TOD =================================================
      // Calculate wind, distance and averate speed (TAS) for this leg
      // Wind is interpolated by altitude
      // Need to use smaller/greater *or equal* to catch special cases of exactly matching distances
      if(endDistLeg <= tocDist)
      {
        // All climb before TOC ==========================
        climbDist = legDist;
        climbWind = windReporter->getWindForLineStringRoute(legLine);
        climbSpeed = perf.getClimbSpeed();
      }
      else if(startDistLeg >= todDist)
      {
        // All descent after TOD ==========================
        descentDist = legDist;
        descentWind = windReporter->getWindForLineStringRoute(legLine);
        descentSpeed = perf.getDescentSpeed();
      }
      else if(startDistLeg <= tocDist && endDistLeg >= todDist)
      {
        // Crosses TOC *and* TOD  - phases climb, cruise and descent ==========================
        // Climb to TOC ===================
        climbDist = tocDist - startDistLeg;
        climbWind = windReporter->getWindForLineStringRoute(legLine.left(2));
        climbSpeed = perf.getClimbSpeed();

        // cruise - TOC to TOD ===================
        cruiseDist = todDist - tocDist;
        cruiseWind = windReporter->getWindForLineStringRoute(legLine.mid(1, 2));
        cruiseSpeed = perf.getCruiseSpeed();

        // TOD to destination ===================
        descentDist = endDistLeg - todDist;
        descentWind = windReporter->getWindForLineStringRoute(legLine.right(2));
        descentSpeed = perf.getDescentSpeed();
      }
      else if(startDistLeg <= tocDist && endDistLeg <= todDist)
      {
        // Crosses TOC and goes into cruise ==========================
        climbDist = tocDist - startDistLeg;
        climbWind = windReporter->getWindForLineStringRoute(legLine.left(2));
        climbSpeed = perf.getClimbSpeed();

        // Cruise to TOD ==========================
        cruiseDist = endDistLeg - tocDist;
        cruiseWind = windReporter->getWindForLineStringRoute(legLine.right(2));
        cruiseSpeed = perf.getCruiseSpeed();
      }
      else if(startDistLeg >= tocDist && endDistLeg >= todDist)
      {
        // Goes from cruise to and after TOD ==========================
        // Cruise to TOD ==========================
        cruiseDist = todDist - startDistLeg;
        cruiseWind = windReporter->getWindForLineStringRoute(legLine.left(2));
        cruiseSpeed = perf.getCruiseSpeed();

        // TOD to destination ===================
        descentDist = endDistLeg - todDist;
        descentWind = windReporter->getWindForLineStringRoute(legLine.right(2));
        descentSpeed = perf.getDescentSpeed();
      }
      else
      {
        // Cruise only ==========================
        cruiseDist = legDist;
        cruiseWind = windReporter->getWindForLineStringRoute(legLine);
        cruiseSpeed = perf.getCruiseSpeed();
      }

      // Calculate ground speed for each phase (climb, cruise, descent) of this leg - 0 is phase is not touched
      float course = route->value(i).getCourseEndTrue();

      float climbHeadWind = 0.f, cruiseHeadWind = 0.f, descentHeadWind = 0.f;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
      qDebug() << Q_FUNC_INFO << "=========== leg #" << i
               << "wind: climb" << climbWind << "cruise" << cruiseWind << "descent" << descentWind;
#endif

      // Skip wind calculation for circular legs which have no course
      if(course < map::INVALID_COURSE_VALUE)
      {
        // Calculate ground speed if legs for phase found - Calculate head and cross wind for leg
        if(climbSpeed > 0.f)
        {
          climbSpeed = windCorrectedGroundSpeed(climbWind, course, climbSpeed);
          climbHeadWind = ageo::headWindForCourse(climbWind.speed, climbWind.dir, course);
        }
        if(cruiseSpeed > 0.f)
        {
          cruiseSpeed = windCorrectedGroundSpeed(cruiseWind, course, cruiseSpeed);
          cruiseHeadWind = ageo::headWindForCourse(cruiseWind.speed, cruiseWind.dir, course);
        }

        if(descentSpeed > 0.f)
        {
          descentSpeed = windCorrectedGroundSpeed(descentWind, course, descentSpeed);
          descentHeadWind = ageo::headWindForCourse(descentWind.speed, descentWind.dir, course);
        }
      }

      // Check if wind is too strong =====================================
      if(!(climbSpeed < map::INVALID_SPEED_VALUE))
      {
        unflyableLegs = true;
        climbSpeed = perf.getClimbSpeed();
      }
      if(!(cruiseSpeed < map::INVALID_SPEED_VALUE))
      {
        unflyableLegs = true;
        cruiseSpeed = perf.getCruiseSpeed();
      }
      if(!(descentSpeed < map::INVALID_SPEED_VALUE))
      {
        unflyableLegs = true;
        descentSpeed = perf.getDescentSpeed();
      }

      if(atools::almostNotEqual(climbDist + cruiseDist + descentDist, legDist, 1.f))
        qWarning() << Q_FUNC_INFO << "Distance differs" << (climbDist + cruiseDist + descentDist) << legDist;

      // Calculate leg time ================================================================
      leg.climbTime = climbSpeed > 0.f ? (climbDist / climbSpeed) : 0.f;
      leg.cruiseTime = cruiseSpeed > 0.f ? (cruiseDist / cruiseSpeed) : 0.f;
      leg.descentTime = descentSpeed > 0.f ? (descentDist / descentSpeed) : 0.f;

      // Assign values to leg =========================================================
      if(!leg.isMissed() && !leg.isAlternate() && legDist < map::INVALID_DISTANCE_VALUE)
      {
        leg.climbWindHead = climbHeadWind;
        leg.cruiseWindHead = cruiseHeadWind;
        leg.descentWindHead = descentHeadWind;

        // Calculate head wind for climb and descent separately =================================
        windHeadClimb += climbHeadWind * leg.climbTime;
        windHeadCruise += cruiseHeadWind * leg.cruiseTime;
        windHeadDescent += descentHeadWind * leg.descentTime;

        // Wind corrected speeds for all phases (equal to GS) ========
        climbSpeedWindCorrected += climbSpeed * leg.climbTime;
        cruiseSpeedWindCorrected += cruiseSpeed * leg.cruiseTime;
        descentSpeedWindCorrected += descentSpeed * leg.descentTime;

        // Wind ===========
        leg.climbWindSpeed = climbWind.speed;
        leg.climbWindDir = climbWind.dir;
        leg.cruiseWindSpeed = cruiseWind.speed;
        leg.cruiseWindDir = cruiseWind.dir;
        leg.descentWindSpeed = descentWind.speed;
        leg.descentWindDir = descentWind.dir;

        // Sum up fuel for phases for this leg ============
        leg.climbFuel = perf.getClimbFuelFlow() * leg.climbTime;
        leg.cruiseFuel = perf.getCruiseFuelFlow() * leg.cruiseTime;
        leg.descentFuel = perf.getDescentFuelFlow() * leg.descentTime;

        atools::grib::Wind wind = windReporter->getWindForPosRoute(legLine.getPos2());
        leg.windSpeed = wind.speed;
        leg.windDirection = wind.dir;

        // Summarize trip values ====================
        travelTime += leg.getTime();
        tripFuel += leg.getFuel();

        // Summarize fuel for each phase ====================
        climbFuel += perf.getClimbFuelFlow() * leg.climbTime;
        cruiseFuel += perf.getCruiseFuelFlow() * leg.cruiseTime;
        descentFuel += perf.getDescentFuelFlow() * leg.descentTime;

        // Summarize time for each phase ====================
        climbTime += leg.climbTime;
        cruiseTime += leg.cruiseTime;
        descentTime += leg.descentTime;
      } // if(!leg.isMissed() && !leg.isAlternate() && legDist < map::INVALID_DISTANCE_VALUE)
    } // if(leg.isAlternate()) ... else
  } // for(int i = 0; i < size(); i++)

  // Calculate average for summarized values ==============================
  windHeadClimb /= climbTime;
  windHeadCruise /= cruiseTime;
  windHeadDescent /= descentTime;

  climbSpeedWindCorrected /= climbTime;
  cruiseSpeedWindCorrected /= cruiseTime;
  descentSpeedWindCorrected /= descentTime;

  // Calculate alternate fuel and time =====================================
  alternateFuel = 0.f;
  int offset = route->getAlternateLegsOffset();
  if(offset != map::INVALID_INDEX_VALUE)
  {
    for(int idx = offset; idx < offset + route->getNumAlternateLegs(); idx++)
    {
      // Fuel to the farthest alternate
      alternateFuel = std::max(value(idx).getFuel(), alternateFuel);

      RouteAltitudeLeg& leg = (*this)[idx];
      leg.fuelToDest = tripFuel + leg.getFuel();
      leg.timeToDest = travelTime + leg.getTime();
    }
  }

  // Calculate fuel and time to destination for each leg ===================================
  float fuelToDest = tripFuel;
  float timeToDest = travelTime;
  for(int i = 0; i < size(); i++)
  {
    RouteAltitudeLeg& leg = (*this)[i];
    if(leg.isMissed() || leg.isAlternate())
      break;

    // Fuel and time to destination from start point
    leg.fuelToDest = fuelToDest;
    fuelToDest -= leg.getFuel();

    // Round down and correct negative values
    leg.timeToDest = timeToDest > 0.000001f ? timeToDest : 0.f;
    timeToDest -= leg.getTime();
  }

  // Calculate average values for all =====================================
  float uAverageAll = 0.f, vAverageAll = 0.f;
  float uAverageCruise = 0.f, vAverageCruise = 0.f;
  for(const RouteAltitudeLeg& leg : qAsConst(*this))
  {
    if(leg.isMissed() || leg.isAlternate())
      break;

    float legTime = leg.getTime();
    if(atools::almostEqual(legTime, 0.f) || !(legTime < map::INVALID_DISTANCE_VALUE))
      continue;

    // Speed ===================

    // Wind - sum up U/V components =================
    float uCruise = ageo::windUComponent(leg.cruiseWindSpeed, leg.cruiseWindDir) * leg.cruiseTime;
    float vCruise = ageo::windVComponent(leg.cruiseWindSpeed, leg.cruiseWindDir) * leg.cruiseTime;
    uAverageCruise += uCruise;
    vAverageCruise += vCruise;

    uAverageAll += ageo::windUComponent(leg.climbWindSpeed, leg.climbWindDir) * leg.climbTime + uCruise +
                   ageo::windUComponent(leg.descentWindSpeed, leg.descentWindDir) * leg.descentTime;

    vAverageAll += ageo::windVComponent(leg.climbWindSpeed, leg.climbWindDir) * leg.climbTime + vCruise +
                   ageo::windVComponent(leg.descentWindSpeed, leg.descentWindDir) * leg.descentTime;

    windHeadAvg += leg.climbWindHead * leg.climbTime +
                   leg.cruiseWindHead * leg.cruiseTime +
                   leg.descentWindHead * leg.descentTime;
  }
  uAverageAll /= travelTime;
  vAverageAll /= travelTime;
  uAverageCruise /= cruiseTime;
  vAverageCruise /= cruiseTime;

  // get back to direction/speed from U/V components
  // Average for whole route ========================================
  windDirectionAvg = ageo::windDirectionFromUV(uAverageAll, vAverageAll);
  windSpeedAvg = ageo::windSpeedFromUV(uAverageAll, vAverageAll);
  windHeadAvg /= travelTime;

  // Average for cruise phase only ========================================
  windDirectionCruiseAvg = ageo::windDirectionFromUV(uAverageCruise, vAverageCruise);
  windSpeedCruiseAvg = ageo::windSpeedFromUV(uAverageCruise, vAverageCruise);

  averageGroundSpeed = getTotalDistance() / travelTime;

#ifdef DEBUG_INFORMATION_ROUTE_ALT
  qDebug() << "================================================================================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif
}

float RouteAltitude::windCorrectedGroundSpeed(atools::grib::Wind& wind, float course, float speed)
{
  float gs = ageo::windCorrectedGroundSpeed(wind.speed, wind.dir, course, speed);
  return gs < 1.f ? map::INVALID_SPEED_VALUE : gs;
}

QDebug operator<<(QDebug out, const FuelTimeResult& obj)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace()
    << "FuelTimeResult["
    << "fuelLbsToDest" << (obj.isFuelToDestValid() ? obj.fuelLbsToDest : -1.f)
    << "fuelGalToDest" << (obj.isFuelToDestValid() ? obj.fuelGalToDest : -1.f)
    << "timeToDest" << (obj.isTimeToDestValid() ? obj.timeToDest : -1.f)
    << "fuelLbsToTod" << (obj.isFuelToTodValid() ? obj.fuelLbsToTod : -1.f)
    << "fuelGalToTod" << (obj.isFuelToTodValid() ? obj.fuelGalToTod : -1.f)
    << "timeToTod" << (obj.isTimeToTodValid() ? obj.timeToTod : -1.f)
    << "fuelLbsToNext" << (obj.isFuelToNextValid() ? obj.fuelLbsToNext : -1.f)
    << "fuelGalToNext" << (obj.isFuelToNextValid() ? obj.fuelGalToNext : -1.f)
    << "timeToNext" << (obj.isTimeToNextValid() ? obj.timeToNext : -1.f)
    << "estimatedFuel" << obj.estimatedFuel
    << "estimatedTime" << obj.estimatedTime
    << "]";
  return out;
}

QDebug operator<<(QDebug out, const RouteAltitude& obj)
{
  QDebugStateSaver saver(out);
  out << "TOC dist" << obj.getTopOfClimbDistance()
      << "index" << obj.getTopOfClimbLegIndex()
      << "TOD dist" << obj.getTopOfDescentDistance()
      << "index" << obj.getTopOfDescentLegIndex()
      << "travelTime " << obj.getTravelTimeHours()
      << "averageSpeed" << obj.getAverageGroundSpeed()
      << "tripFuel" << obj.getTripFuel()
      << "alternateFuel" << obj.getAlternateFuel()
      << "totalDistance" << obj.route->getTotalDistance()
      << endl;

  for(int i = 0; i < obj.size(); i++)
    out << "++++++++++++++++++++++" << endl << i << obj.value(i) << endl;
  return out;
}
