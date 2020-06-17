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

#include "route/routealtitude.h"

#include "route/route.h"
#include "atools.h"
#include "geo/calculations.h"
#include "fs/perf/aircraftperf.h"
#include "grib/windquery.h"
#include "common/unit.h"
#include "navapp.h"
#include "weather/windreporter.h"

#include <QLineF>

using atools::interpolate;
namespace ageo = atools::geo;

RouteAltitude::RouteAltitude(const Route *routeParam)
  : route(routeParam)
{

}

RouteAltitude::~RouteAltitude()
{

}

int RouteAltitude::indexForDistance(float distanceToDest) const
{
  if(isEmpty())
    return map::INVALID_INDEX_VALUE;

  float distFromStart = route->getTotalDistance() - distanceToDest;

  // Search through all legs to find one that overlaps with distanceToDest
  QVector<RouteAltitudeLeg>::const_iterator it =
    std::lower_bound(begin(), end(), distFromStart,
                     [](const RouteAltitudeLeg& l1, float dist) -> bool
  {
    // binary predicate which returns ​true if the first argument is less than (i.e. is ordered before) the second.
    return l1.getDistanceFromStart() < dist;
  });

  if(it != end())
    return static_cast<int>(std::distance(begin(), it));

  return map::INVALID_INDEX_VALUE;
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

    QPolygonF::const_iterator itGeo =
      std::lower_bound(leg.geometry.begin(), leg.geometry.end(), distFromStart,
                       [](const QPointF& pt, float dist) -> bool
    {
      // binary predicate which returns ​true if the first argument is less than (i.e. is ordered before) the second.
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
     distanceTopOfDescent > route->getTotalDistance() - 0.2f ||
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
  if(isEmpty() || !(distanceTopOfClimb < map::INVALID_DISTANCE_VALUE) || distanceTopOfClimb < 0.2f ||
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
  destRunwayIls.clear();
  destRunwayEnd = map::MapRunwayEnd();
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
    qWarning() << Q_FUNC_INFO << "Invalid index" << i;
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

QString RouteAltitude::getErrorStrings(QString& toolTip, QString& statusTip) const
{
  if(!errors.isEmpty())
  {
    statusTip = tr("Cannot calculate profile.");
    toolTip = errors.join(tr("\n"));
    return tr("Cannot calculate profile. Hover mouse over this message for details.");
  }
  else
    return QString();
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
      if(first.isRoute() && first.getAirport().isValid())
        retval.replace(0, first.getPosition().getAltitude());

      // Replace the zero altitude of the last dummy segment with the airport altitude
      const RouteLeg& last = route->getDestinationAirportLeg();
      if(last.isRoute() && last.getAirport().isValid())
        retval.replace(retval.size() - 1, last.getPosition().getAltitude());
    }
  }
  else
  {
    int destinationAirportLegIndex = route->getDestinationAirportLegIndex();
    // No altitude legs - copy airport and cruise altitude ==========================
    for(int i = 0; i < route->size(); i++)
    {
      const RouteLeg& leg = route->value(i);

      if(i == 0 && leg.getAirport().isValid())
        retval.append(leg.getPosition().getAltitude());
      else if(i == destinationAirportLegIndex && leg.getAirport().isValid())
        retval.append(leg.getPosition().getAltitude());
      else
        retval.append(route->getCruisingAltitudeFeet());
    }
  }

  return retval;
}

bool RouteAltitude::calculateFuelAndTimeTo(float& fuelLbsToDest, float& fuelGalToDest,
                                           float& fuelLbsToTod, float& fuelGalToTod,
                                           float& timeToDest, float& timeToTod,
                                           float distanceToDest, const atools::fs::perf::AircraftPerf& perf,
                                           float aircraftFuelFlowLbs, float aircraftFuelFlowGal,
                                           float aircraftGroundSpeed, int activeLeg) const
{
  // otherwise estimated using aircraft fuel flow
  bool fuelCalculated = false;

  if(distanceToDest >= 0.f && distanceToDest < map::INVALID_DISTANCE_VALUE)
  {
    if(!(getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE))
      // Bail out if TOD is not valid - means whole profile is not valid
      return false;

    // Distance from aircraft to TOD
    float curDistToTod = distanceToDest - getTopOfDescentFromDestination();

    // Need fuel flow and speed to calculate fuel and time
    if(isValidProfile() && perf.isFuelFlowValid() && perf.isSpeedValid())
    {
      // Calculate values based on profile data ======================================
      if(activeLeg != map::INVALID_INDEX_VALUE)
      {
        const RouteAltitudeLeg& leg = value(activeLeg);

        // Calculate time and fuel to destination ============================================
        // Fuel from end of leg to destination
        float distFromDeparture = getTotalDistance() - distanceToDest;
        float fuelToDest = 0.f;
        // calulate fuel and time from this leg
        leg.getFuelAndTimeFromDistToDestination(fuelToDest, timeToDest, distFromDeparture);

        // Convert units
        if(perf.useFuelAsVolume())
        {
          fuelLbsToDest = atools::geo::fromGalToLbs(perf.isJetFuel(), fuelToDest);
          fuelGalToDest = fuelToDest;
        }
        else
        {
          fuelLbsToDest = fuelToDest;
          fuelGalToDest = atools::geo::fromLbsToGal(perf.isJetFuel(), fuelToDest);
        }

        // Calculate time and fuel to TOD ============================================
        int todIdx = getTopOfDescentLegIndex();
        float todDistanceFromDeparture = getTopOfDescentDistance();
        if(curDistToTod > 0.f && todIdx != map::INVALID_INDEX_VALUE &&
           todDistanceFromDeparture < map::INVALID_DISTANCE_VALUE)
        {
          float fuelToTod = 0.f;
          // Calculate fuel and time from TOD to destination
          value(todIdx).getFuelAndTimeFromDistToDestination(fuelToTod, timeToTod, todDistanceFromDeparture);

          // Calculate fuel and time from aircraft to TOD
          fuelToTod = fuelToDest - fuelToTod;
          timeToTod = timeToDest - timeToTod;

          if(perf.useFuelAsVolume())
          {
            fuelLbsToTod = atools::geo::fromGalToLbs(perf.isJetFuel(), fuelToTod);
            fuelGalToTod = fuelToTod;
          }
          else
          {
            fuelLbsToTod = fuelToTod;
            fuelGalToTod = atools::geo::fromLbsToGal(perf.isJetFuel(), fuelToTod);
          }
        }
        else
        {
          fuelLbsToTod = map::INVALID_WEIGHT_VALUE;
          fuelGalToTod = map::INVALID_VOLUME_VALUE;
          timeToTod = map::INVALID_TIME_VALUE;
        }
      }
      fuelCalculated = true;
    }
    else if(aircraftFuelFlowLbs > 0.01f && aircraftGroundSpeed > map::MIN_GROUND_SPEED)
    {
      // No valid profile - calculate values based on aircraft ======================================
      // Use classic calculation based on actual consumption values ==================
      fuelLbsToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowLbs;
      fuelGalToDest = distanceToDest / aircraftGroundSpeed * aircraftFuelFlowGal;
      timeToDest = distanceToDest / aircraftGroundSpeed;

      // Calculate time and fuel to TOD ============================================
      if(curDistToTod > 0.f)
      {
        timeToTod = curDistToTod / aircraftGroundSpeed;

        if(perf.useFuelAsVolume())
        {
          fuelLbsToTod = fuelLbsToDest - atools::geo::fromGalToLbs(perf.isJetFuel(), descentFuel);
          fuelGalToTod = fuelGalToDest - descentFuel;
        }
        else
        {
          fuelLbsToTod = fuelLbsToDest - descentFuel;
          fuelGalToTod = fuelGalToDest - atools::geo::fromLbsToGal(perf.isJetFuel(), descentFuel);
        }
      }
      fuelCalculated = false;
    }
  }
  return fuelCalculated;
}

void RouteAltitude::adjustAltitudeForRestriction(RouteAltitudeLeg& leg) const
{
  if(!leg.isEmpty())
    leg.setY2(adjustAltitudeForRestriction(leg.y2(), leg.restriction));
}

float RouteAltitude::adjustAltitudeForRestriction(float altitude, const proc::MapAltRestriction& restriction) const
{
  switch(restriction.descriptor)
  {
    case proc::MapAltRestriction::NONE:
      break;

    case proc::MapAltRestriction::AT:
    case proc::MapAltRestriction::ILS_AT:
      altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_ABOVE:
    case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
      if(restriction.forceFinal)
        // Stick to lowest altitude on FAF and FACF
        altitude = restriction.alt1;
      else if(altitude < restriction.alt1)
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_BELOW:
      if(restriction.forceFinal)
        // Stick to lowest altitude on FAF and FACF
        altitude = restriction.alt1;
      else if(altitude > restriction.alt1)
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::BETWEEN:
      if(restriction.forceFinal)
        // Stick to lowest altitude on FAF and FACF
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

bool RouteAltitude::violatesAltitudeRestriction(QString& errorMessage, int legIndex) const
{
  const RouteAltitudeLeg& leg = value(legIndex);
  float legAlt = leg.y2();
  bool retval = false;

  if(!leg.isEmpty())
  {
    switch(leg.restriction.descriptor)
    {
      case proc::MapAltRestriction::NONE:
      case proc::MapAltRestriction::ILS_AT:
      case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
        break;

      case proc::MapAltRestriction::AT:
        retval = atools::almostNotEqual(legAlt, leg.restriction.alt1, 10.f);
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
  }

  if(retval)
    errorMessage = tr("Leg number %1, %2 (%3) at %4 violates restriction \"%5\".").
                   arg(legIndex + 1).
                   arg(leg.getIdent()).
                   arg(leg.getProcedureType()).
                   arg(Unit::altFeet(legAlt)).
                   arg(proc::altRestrictionText(leg.restriction));

  return retval;
}

float RouteAltitude::findApproachMaxAltitude(int index) const
{
  if(index > 1)
  {
    // Avoid crashes
    index = fixRange(index);

    if(index < map::INVALID_INDEX_VALUE)
    {
      // Check backwards from index for a arrival/STAR leg that limits the maximum altitude
      for(int i = index - 1; i >= 0; i--)
      {
        const RouteLeg& leg = route->value(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyArrival() && leg.getProcedureLegAltRestr().isValid())
        {
          const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();
          if(r.forceFinal || atools::contains(r.descriptor,
                                              {proc::MapAltRestriction::AT,
                                               proc::MapAltRestriction::AT_OR_BELOW,
                                               proc::MapAltRestriction::BETWEEN,
                                               proc::MapAltRestriction::ILS_AT}))
            return r.alt1;
        }
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }
  return map::INVALID_ALTITUDE_VALUE;
}

float RouteAltitude::findDepartureMaxAltitude(int index) const
{
  if(index > 1)
  {
    // Avoid crashes
    index = fixRange(index);

    // Check forward from index to end of SID for leg that limits the maximum altitude
    int end = fixRange(route->getSidLegsOffset() + route->getSidLegs().size());
    if(end == map::INVALID_INDEX_VALUE)
      // Search through the whole route
      end = route->size() - 1;

    if(index < map::INVALID_INDEX_VALUE && end < map::INVALID_INDEX_VALUE)
    {
      for(int i = index; i < end; i++)
      {
        const RouteLeg& leg = route->value(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyDeparture() && leg.getProcedureLegAltRestr().isValid())
        {
          const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();
          if(r.forceFinal || atools::contains(r.descriptor,
                                              {proc::MapAltRestriction::AT, proc::MapAltRestriction::AT_OR_BELOW,
                                               proc::MapAltRestriction::BETWEEN}))
            return r.alt1;
        }
      }
    }
    else
      qWarning() << Q_FUNC_INFO;
  }
  return map::INVALID_ALTITUDE_VALUE;
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
    }
  }

#ifdef DEBUG_INFORMATION
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

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "old" << midAlt->y2() << "new" << mid.y();
#endif

    // Apply limitations for skipped (close) waypoints
    float newAlt = adjustAltitudeForRestriction(static_cast<float>(mid.y()), midAlt->restriction);
    if(rightSkippedAlt != nullptr)
      newAlt = adjustAltitudeForRestriction(newAlt, rightSkippedAlt->restriction);
    if(leftSkippedAlt != nullptr)
      newAlt = adjustAltitudeForRestriction(newAlt, leftSkippedAlt->restriction);

#ifdef DEBUG_INFORMATION
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
    errors.append(tr("Check the cruise altitude and procedures."));
    errors.append(altRestrErrors);
  }
  else if(!(getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE &&
            getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE))
    errors.append(tr("Cannot calculate top of climb or top of descent.\n"
                     "Check the flight plan cruise altitude and the\n"
                     "climb/descent speeds in the Aircraft Performance."));
}

void RouteAltitude::calculateAll(const atools::fs::perf::AircraftPerf& perf, float cruiseAltitudeFt)
{
  qDebug() << Q_FUNC_INFO;

  // Get default climb speed
  climbSpeedWindCorrected = perf.getClimbSpeed();
  cruiseSpeedWindCorrected = perf.getCruiseSpeed();
  descentSpeedWindCorrected = perf.getDescentSpeed();

  // Calculate default climb rate
  climbRateWindFtPerNm = perf.getClimbVertSpeed() * 60.f / climbSpeedWindCorrected;
  descentRateWindFtPerNm = perf.getDescentVertSpeed() * 60.f / descentSpeedWindCorrected;

  cruiseAltitide = cruiseAltitudeFt;

  errors.clear();
  clearAll();

  bool invalid = false;
  if(route->getTotalDistance() < 0.5f)
  {
    errors.append(tr("Flight plan is too short."));
    qWarning() << Q_FUNC_INFO << "Flight plan too short";
    invalid = true;
  }

  if(route->getCruisingAltitudeFeet() < 100.f)
  {
    errors.append(tr("Cruise altitude is too low."));
    qWarning() << Q_FUNC_INFO << "Cruise altitude is too low";
    invalid = true;
  }

  const RouteLeg destinationLeg = route->getDestinationLeg();
  if(!destinationLeg.isValidWaypoint() ||
     (destinationLeg.getMapObjectType() != map::AIRPORT && destinationLeg.getMapObjectType() != map::PROCEDURE))
  {
    errors.append(tr("Destination is not valid. Must be an airport."));
    qWarning() << Q_FUNC_INFO << "Destination is not valid or neither airport nor runway";
    invalid = true;
  }

  const RouteLeg departureLeg = route->getDepartureAirportLeg();
  if(!departureLeg.isValidWaypoint() ||
     (departureLeg.getMapObjectType() != map::AIRPORT && departureLeg.getMapObjectType() != map::PROCEDURE))
  {
    errors.append(tr("Departure is not valid. Must be an airport."));
    qWarning() << Q_FUNC_INFO << "Departure is not valid or neither airport nor runway";
    invalid = true;
  }

  if(!invalid)
  {
    QStringList altRestrErrors;
    calculate(altRestrErrors);
    collectErrors(altRestrErrors);

    if(validProfile)
    {
      calculateTrip(perf);

      // Do a second iteration if difference in average climb or descent exceeds 10 knots ============================
      if(atools::almostNotEqual(climbSpeedWindCorrected, perf.getClimbSpeed(), 10.f) ||
         atools::almostNotEqual(descentRateWindFtPerNm, perf.getDescentSpeed(), 10.f))
      {
        qDebug() << Q_FUNC_INFO << "Second iteration: windHeadClimb" << windHeadClimb << "windHeadCruise" <<
          windHeadCruise
                 << "climbSpeedWindCorrected" << climbSpeedWindCorrected
                 << "descentSpeedWindCorrected" << descentSpeedWindCorrected;

        climbRateWindFtPerNm = perf.getClimbVertSpeed() * 60.f / climbSpeedWindCorrected;
        descentRateWindFtPerNm = perf.getDescentVertSpeed() * 60.f / descentSpeedWindCorrected;

        qDebug() << Q_FUNC_INFO << "climbRateWindFtPerNm" << climbRateWindFtPerNm
                 << "descentRateWindFtPerNm" << descentRateWindFtPerNm;

        clearAll();
        calculate(altRestrErrors);
        collectErrors(altRestrErrors);

        if(validProfile)
        {
          calculateTrip(perf);

          // Do a third iteration if difference in average climb or descent exceeds 30 knots ============================
          if(atools::almostNotEqual(climbSpeedWindCorrected, perf.getClimbSpeed(), 30.f) ||
             atools::almostNotEqual(descentRateWindFtPerNm, perf.getDescentSpeed(), 30.f))
          {
            clearAll();
            calculate(altRestrErrors);
            collectErrors(altRestrErrors);

            if(validProfile)
              calculateTrip(perf);
          }
        }
      }
    }
  }

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
           << "cruiseAltitide" << cruiseAltitide;
#endif

  if(!errors.isEmpty())
    qWarning() << "errors" << errors;
  qDebug() << Q_FUNC_INFO;
}

void RouteAltitude::calculate(QStringList& altRestErrors)
{
  altRestErrors.clear();

  if(route->getSizeWithoutAlternates() <= 1)
    return;

  // Prefill all legs with distance and cruise altitude
  calculateDistances();

  if(calcTopOfClimb)
    calculateDeparture();

  if(calcTopOfDescent)
    calculateArrival();

  // Check for violations because of too low cruise
  for(int i = 0; i < size(); i++)
  {
    const RouteAltitudeLeg& leg = value(i);

    if(!leg.isMissed() && !leg.isAlternate())
    {
      QString errorMessage;
      bool err = violatesAltitudeRestriction(errorMessage, i);

      if(err)
      {
        qWarning() << Q_FUNC_INFO << "violating messge" << errorMessage << "leg" << leg;
        altRestErrors.append(errorMessage);
      }
    }
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "Before cleanup ==================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif

  if(!altRestErrors.isEmpty() || distanceTopOfClimb > distanceTopOfDescent ||
     (calcTopOfClimb && !(distanceTopOfClimb < map::INVALID_INDEX_VALUE)) ||
     (calcTopOfDescent && !(distanceTopOfDescent < map::INVALID_INDEX_VALUE)))
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

    // Fetch ILS and VASI at destination
    calculateApproachIlsAndSlopes();
    validProfile = true;
  }

  // Set coordinates into legs
  fillGeometry();

#ifdef DEBUG_INFORMATION
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
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitide));
      distanceToLeg += leg.getDistanceTo();
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitide));
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
      // If this is a point like an IF leg copy restriction from last leg but save force flag
      bool force = altLeg.restriction.forceFinal;
      altLeg.restriction = lastAltLeg.restriction;
      altLeg.restriction.forceFinal = force;
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

  float departAlt = getDepartureAltitude();

  if(departureLegIdx > 0) // Assign altitude to dummy for departure airport too
    first().setAlt(departAlt);

  // Start from departure forward until hitting cruise altitude (TOC)
  for(int i = departureLegIdx; i <= route->getDestinationAirportLegIndex(); i++)
  {
    // Calculate altitude for this leg based on altitude of the leg to the right
    RouteAltitudeLeg& alt = (*this)[i];
    if(alt.isEmpty())
      // Dummy leg
      continue;

    // Altitude of last leg
    float lastLegAlt = i > departureLegIdx ? value(i - 1).y2() : departAlt;

    if(i <= departureLegIdx)
      // Departure leg - assign altitude to airport and RW too if available
      alt.setAlt(departAlt);
    else
    {
      // Set geometry for climbing
      alt.setY1(lastLegAlt);
      // Use a default value of 3 nm per 1000 ft if performance is not available
      alt.setY2(lastLegAlt + alt.getDistanceTo() * climbRateWindFtPerNm);
    }

    if(!alt.isEmpty())
    {
      // Remember geometry which is not changed by altitude restrictions for calculation of cruise intersection
      float uncorrectedAlt = alt.y2();

      adjustAltitudeForRestriction(alt);

      // Avoid climbing above any below/at/between restrictions
      float maxAlt = findDepartureMaxAltitude(i);
      bool maxAltRestricts = false;
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
      {
        alt.setY2(std::min(alt.y2(), maxAlt));

        // Avoid climbing above next limiting restriction - no TOC yet
        maxAltRestricts = maxAlt < cruiseAltitide;
      }

      // Never lower than last leg
      alt.setY2(std::max(alt.y2(), lastLegAlt));

      if(i > 0 && uncorrectedAlt > cruiseAltitide && !(distanceTopOfClimb < map::INVALID_ALTITUDE_VALUE) &&
         !maxAltRestricts)
      {
        // Reached TOC - calculate distance
        distanceTopOfClimb = distanceForAltitude(alt.geometry.first(), QPointF(alt.geometry.last().x(), uncorrectedAlt),
                                                 cruiseAltitide);
        legIndexTopOfClimb = i;

        // Adjust this leg
        alt.setY2(std::min(alt.y2(), cruiseAltitide));
        alt.setY2(std::max(alt.y2(), lastLegAlt));

        // Add point to allow drawing the bend at TOC
        alt.geometry.insert(1, QPointF(distanceTopOfClimb, cruiseAltitide));

        // Done here
        alt.topOfClimb = true;
        break;
      }
    }

    // Adjust altitude to be above last and below cruise
    alt.setY2(std::min(alt.y2(), cruiseAltitide));
    alt.setY2(std::max(alt.y2(), lastLegAlt));
  }
}

void RouteAltitude::calculateArrival()
{
  int destinationLegIdx = route->getDestinationLegIndex();
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

  // Calculate from last leg down until we hit the cruise altitude (TOD)
  for(int i = route->getDestinationAirportLegIndex(); i >= 0; i--)
  {
    RouteAltitudeLeg& alt = (*this)[i];
    RouteAltitudeLeg *lastAltLeg = i < destinationLegIdx ? &(*this)[i + 1] : nullptr;
    float lastLegAlt = lastAltLeg != nullptr ? lastAltLeg->y2() : 0.;

    // Start with altitude of the right leg (close to dest)
    float newAltitude = lastLegAlt;

    if(i <= destinationLegIdx)
    {
      // Leg leading to this point (right to this)
      float distFromRight = lastAltLeg != nullptr ? lastAltLeg->getDistanceTo() : 0.f;

      // Point of this leg
      // Use a default value of 3 nm per 1000 ft if performance is not available
      newAltitude = lastAlt + distFromRight * descentRateWindFtPerNm;
    }

    if(!alt.isEmpty())
    {
      // Remember geometry which is not changed by altitude restrictions for calculation of cruise intersection
      float uncorrectedAltitude = newAltitude;

      // Not a dummy leg
      newAltitude = adjustAltitudeForRestriction(newAltitude, alt.restriction);

      // Avoid climbing (up the descent slope) above any below/at/between restrictions
      float maxAlt = findApproachMaxAltitude(i + 1);
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
        newAltitude = std::min(newAltitude, maxAlt);

      // Never lower than right leg
      newAltitude = std::max(newAltitude, lastAlt);

      // Check the limits to the left - either cruise or an altitude restriction
      // Set to true if an altitude restriction is limiting and postone calculation of the TOD
      bool altitudeRestricts = maxAlt < map::INVALID_ALTITUDE_VALUE && maxAlt <= cruiseAltitide;

      if(!altitudeRestricts && !(distanceTopOfDescent < map::INVALID_ALTITUDE_VALUE) &&
         uncorrectedAltitude > cruiseAltitide && i + 1 < size())
      {
        if(value(i + 1).isEmpty())
          // Stepped into the dummies after arrival runway - bail out
          break;

        // Reached TOD - calculate distance
        distanceTopOfDescent = distanceForAltitude(value(i + 1).getGeometry().last(),
                                                   QPointF(
                                                     alt.getDistanceFromStart(), uncorrectedAltitude), cruiseAltitide);
        legIndexTopOfDescent = i + 1;

        if(!lastAltLeg->topOfClimb)
          // Adjust only if not modified by TOC calculation
          alt.setY2(std::min(newAltitude, cruiseAltitide));

        if(lastAltLeg != nullptr)
          // Adjust neighbor too
          lastAltLeg->setY1(alt.y2());

        // Append point to allow drawing the bend at TOD - TOC position might be already included in leg
        lastAltLeg->geometry.insert(lastAltLeg->geometry.size() - 1,
                                    QPointF(distanceTopOfDescent, cruiseAltitide));

        // Done here
        if(legIndexTopOfDescent < size())
          (*this)[legIndexTopOfDescent].topOfDescent = true;
        break;
      }

      // Adjust altitude to be above last and below cruise
      alt.setY2(newAltitude);
      alt.setY2(std::min(alt.y2(), cruiseAltitide));

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

void RouteAltitude::calculateApproachIlsAndSlopes()
{
  route->getApproachRunwayEndAndIls(destRunwayIls, &destRunwayEnd);

  // Filter out unusable ILS
  auto it = std::remove_if(destRunwayIls.begin(), destRunwayIls.end(),
                           [ = ](const map::MapIls& ils) -> bool
  {
    // Needs to have GS, not farther away from runway end than 4NM and not more than 20 degree difference
    return ils.slope < 0.1f || destRunwayEnd.position.distanceMeterTo(ils.position) > ageo::nmToMeter(4.) ||
    ageo::angleAbsDiff(destRunwayEnd.heading, ils.heading) > 20.f;
  });

  if(it != destRunwayIls.end())
    destRunwayIls.erase(it, destRunwayIls.end());
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

    if(altLeg.isAlternate())
    {
      altLeg.line.append(destinationAirportLeg.getPosition());
      altLeg.line.append(routeLeg.getPosition());
    }
    else
    {
      if(altLeg.isPoint())
        altLeg.line.append(routeLeg.getPosition().alt(altLeg.y1()));
      else
      {
        if(i > 0)
          altLeg.line.append(route->value(i - 1).getPosition().alt(altLeg.y1()));
        altLeg.line.append(routeLeg.getPosition().alt(altLeg.y2()));
      }

      if(altLeg.topOfClimb)
        altLeg.line.insert(1, route->getPositionAtDistance(distanceTopOfClimb).alt(route->getCruisingAltitudeFeet()));

      if(altLeg.topOfDescent)
        altLeg.line.insert(altLeg.line.size() - 1,
                           route->getPositionAtDistance(distanceTopOfDescent).alt(route->getCruisingAltitudeFeet()));
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
        return adjustAltitudeForRestriction(cruiseAltitide, destLeg.getProcedureLegAltRestr());
    }
    else if(destLeg.getAirport().isValid())
      // Airport altitude
      return destLeg.getPosition().getAltitude();
  }

  // Other leg types (waypoint, VOR, etc.) remain on cruise
  return cruiseAltitide;
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
      return adjustAltitudeForRestriction(cruiseAltitide, startLeg.getProcedureLegAltRestr());
  }
  else if(startLeg.getAirport().isValid())
    // Airport altitude
    return startLeg.getPosition().getAltitude();

  // Other leg types (waypoint, VOR, etc.) remain on cruise
  return cruiseAltitide;
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
  return distanceForAltitude(leg.geometry.first(), leg.geometry.last(), altitude);
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

      float climbDist = 0.f, cruiseDist = 0.f, descentDist = 0.f;
      float climbSpeed = 0.f, cruiseSpeed = 0.f, descentSpeed = 0.f;
      atools::grib::Wind climbWind = atools::grib::EMPTY_WIND,
                         cruiseWind = atools::grib::EMPTY_WIND,
                         descentWind = atools::grib::EMPTY_WIND;

      // Check if leg covers TOC and/or TOD =================================================
      // Calculate wind, distance and averate speed (TAS) for this leg
      // Wind is interpolated by altitude
      if(endDistLeg < tocDist)
      {
        // All climb before TOC ==========================
        climbDist = legDist;
        climbWind = windReporter->getWindForLineStringRoute(leg.getLineString());
        climbSpeed = perf.getClimbSpeed();
      }
      else if(startDistLeg > todDist)
      {
        // All descent after TOD ==========================
        descentDist = legDist;
        descentWind = windReporter->getWindForLineStringRoute(leg.getLineString());
        descentSpeed = perf.getDescentSpeed();
      }
      else if(startDistLeg < tocDist && endDistLeg > todDist)
      {
        // Crosses TOC *and* TOD  - phases climb, cruise and descent ==========================
        // Climb to TOC ===================
        climbDist = tocDist - startDistLeg;
        climbWind = windReporter->getWindForLineStringRoute(leg.getLineString().left(2));
        climbSpeed = perf.getClimbSpeed();

        // cruise - TOC to TOD ===================
        cruiseDist = todDist - tocDist;
        cruiseWind = windReporter->getWindForLineStringRoute(leg.getLineString().mid(1, 2));
        cruiseSpeed = perf.getCruiseSpeed();

        // TOD to destination ===================
        descentDist = endDistLeg - todDist;
        descentWind = windReporter->getWindForLineStringRoute(leg.getLineString().right(2));
        descentSpeed = perf.getDescentSpeed();
      }
      else if(startDistLeg < tocDist && endDistLeg < todDist)
      {
        // Crosses TOC and goes into cruise ==========================
        climbDist = tocDist - startDistLeg;
        climbWind = windReporter->getWindForLineStringRoute(leg.getLineString().left(2));
        climbSpeed = perf.getClimbSpeed();

        // Cruise to TOD ==========================
        cruiseDist = endDistLeg - tocDist;
        cruiseWind = windReporter->getWindForLineStringRoute(leg.getLineString().right(2));
        cruiseSpeed = perf.getCruiseSpeed();
      }
      else if(startDistLeg > tocDist && endDistLeg > todDist)
      {
        // Goes from cruise to and after TOD ==========================
        // Cruise to TOD ==========================
        cruiseDist = todDist - startDistLeg;
        cruiseWind = windReporter->getWindForLineStringRoute(leg.getLineString().left(2));
        cruiseSpeed = perf.getCruiseSpeed();

        // TOD to destination ===================
        descentDist = endDistLeg - todDist;
        descentWind = windReporter->getWindForLineStringRoute(leg.getLineString().right(2));
        descentSpeed = perf.getDescentSpeed();
      }
      else
      {
        // Cruise only ==========================
        cruiseDist = legDist;
        cruiseWind = windReporter->getWindForLineStringRoute(leg.getLineString());
        cruiseSpeed = perf.getCruiseSpeed();
      }

      // Calculate ground speed for each phase (climb, cruise, descent) of this leg - 0 is phase is not touched
      float course = route->value(i).getCourseToTrue();

      float climbHeadWind = 0.f, cruiseHeadWind = 0.f, descentHeadWind = 0.f;

#ifdef DEBUG_INFORMATION
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

        atools::grib::Wind wind = windReporter->getWindForPosRoute(leg.getLineString().getPos2());
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

    leg.timeToDest = timeToDest;
    timeToDest -= leg.getTime();
  }

  // Calculate average values for all =====================================
  float u = 0.f, v = 0.f;
  for(const RouteAltitudeLeg& leg : *this)
  {
    if(leg.isMissed() || leg.isAlternate())
      break;

    float legTime = leg.getTime();
    if(atools::almostEqual(legTime, 0.f) || !(legTime < map::INVALID_DISTANCE_VALUE))
      continue;

    // Speed ===================

    // Wind - sum up U/V components =================
    u += ageo::windUComponent(leg.climbWindSpeed, leg.climbWindDir) * leg.climbTime +
         ageo::windUComponent(leg.cruiseWindSpeed, leg.cruiseWindDir) * leg.cruiseTime +
         ageo::windUComponent(leg.descentWindSpeed, leg.descentWindDir) * leg.descentTime;

    v += ageo::windVComponent(leg.climbWindSpeed, leg.climbWindDir) * leg.climbTime +
         ageo::windVComponent(leg.cruiseWindSpeed, leg.cruiseWindDir) * leg.cruiseTime +
         ageo::windVComponent(leg.descentWindSpeed, leg.descentWindDir) * leg.descentTime;

    windHeadAvg += leg.climbWindHead * leg.climbTime +
                   leg.cruiseWindHead * leg.cruiseTime +
                   leg.descentWindHead * leg.descentTime;
  }
  u /= travelTime;
  v /= travelTime;

  // get back to direction/speed from U/V components
  windDirectionAvg = ageo::windDirectionFromUV(u, v);
  windSpeedAvg = ageo::windSpeedFromUV(u, v);

  windHeadAvg /= travelTime;
  averageGroundSpeed = getTotalDistance() / travelTime;

#ifdef DEBUG_INFORMATION
  qDebug() << "================================================================================================";
  qDebug() << Q_FUNC_INFO << *this;
#endif
}

float RouteAltitude::windCorrectedGroundSpeed(atools::grib::Wind& wind, float course, float speed)
{
  float gs = ageo::windCorrectedGroundSpeed(wind.speed, wind.dir, course, speed);
  return gs < 1.f ? map::INVALID_SPEED_VALUE : gs;
}

QDebug operator<<(QDebug out, const RouteAltitude& obj)
{
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
