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

#include "route/routealtitude.h"

#include "route/route.h"
#include "atools.h"

#include <QLineF>

RouteAltitude::RouteAltitude(const Route *routeParam)
  : route(routeParam)
{

}

RouteAltitude::~RouteAltitude()
{

}

float RouteAltitude::getAltitudeForDistance(float distanceToDest) const
{
  if(isEmpty())
    return map::INVALID_ALTITUDE_VALUE;

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
  {
    // Now search through the geometry to find a matching line (if more than one)
    const RouteAltitudeLeg& leg = *it;

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

float RouteAltitude::getTopOfDescentFromDestination() const
{
  if(isEmpty())
    return map::INVALID_DISTANCE_VALUE;
  else
    return route->getTotalDistance() - distanceTopOfDescent;
}

atools::geo::Pos RouteAltitude::getTopOfDescentPos() const
{
  // Avoid any invalid points near destination
  if(isEmpty() || !(distanceTopOfDescent < map::INVALID_DISTANCE_VALUE) ||
     distanceTopOfDescent > route->getTotalDistance() - 0.2f)
    return atools::geo::EMPTY_POS;
  else
    return route->getPositionAtDistance(distanceTopOfDescent);
}

atools::geo::Pos RouteAltitude::getTopOfClimbPos() const
{
  // Avoid any invalid points near departure
  if(isEmpty() || !(distanceTopOfClimb < map::INVALID_DISTANCE_VALUE) || distanceTopOfClimb < 0.2f)
    return atools::geo::EMPTY_POS;
  else
    return route->getPositionAtDistance(distanceTopOfClimb);
}

void RouteAltitude::clearAll()
{
  clear();
  distanceTopOfClimb = map::INVALID_DISTANCE_VALUE;
  distanceTopOfDescent = map::INVALID_DISTANCE_VALUE;
  violatesRestrictions = false;
  destRunwayIls.clear();
  destRunwayEnd = map::MapRunwayEnd();
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
      altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_ABOVE:
      if(altitude < restriction.alt1)
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::AT_OR_BELOW:
      if(altitude > restriction.alt1)
        altitude = restriction.alt1;
      break;

    case proc::MapAltRestriction::BETWEEN:
      if(altitude > restriction.alt1)
        altitude = restriction.alt1;

      if(altitude < restriction.alt2)
        altitude = restriction.alt2;
      break;
  }
  return altitude;
}

bool RouteAltitude::violatesAltitudeRestriction(const RouteAltitudeLeg& leg) const
{
  if(!leg.isEmpty())
  {
    switch(leg.restriction.descriptor)
    {
      case proc::MapAltRestriction::NONE:
        return false;

      case proc::MapAltRestriction::AT:
        return atools::almostNotEqual(leg.y2(), leg.restriction.alt1, 10.f);

      case proc::MapAltRestriction::AT_OR_ABOVE:
        return leg.y2() < leg.restriction.alt1;

      case proc::MapAltRestriction::AT_OR_BELOW:
        return leg.y2() > leg.restriction.alt1;

      case proc::MapAltRestriction::BETWEEN:
        return leg.y2() > leg.restriction.alt1 || leg.y2() < leg.restriction.alt2;
    }
  }
  return false;
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
        const RouteLeg& leg = route->at(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyArrival() && leg.getProcedureLegAltRestr().isValid())
        {
          const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();
          if(r.descriptor == proc::MapAltRestriction::AT || r.descriptor == proc::MapAltRestriction::AT_OR_BELOW ||
             r.descriptor == proc::MapAltRestriction::BETWEEN)
            return r.alt1;
        }
      }
    }
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
    int end = fixRange(route->getDepartureLegsOffset() + route->getDepartureLegs().size());
    if(end == map::INVALID_INDEX_VALUE)
      // Search through the whole route
      end = route->size() - 1;

    for(int i = index; i < end; i++)
    {
      const RouteLeg& leg = route->at(i);

      if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyDeparture() && leg.getProcedureLegAltRestr().isValid())
      {
        const proc::MapAltRestriction& r = leg.getProcedureLegAltRestr();
        if(r.descriptor == proc::MapAltRestriction::AT || r.descriptor == proc::MapAltRestriction::AT_OR_BELOW ||
           r.descriptor == proc::MapAltRestriction::BETWEEN)
          return r.alt1;
      }
    }
  }
  return map::INVALID_ALTITUDE_VALUE;
}

int RouteAltitude::findApproachFirstRestricion() const
{
  if(route->hasAnyArrivalProcedure() || route->hasAnyStarProcedure())
  {
    // Search for first restriction in an arrival or STAR
    int start = route->getStarLegsOffset();
    if(!(start < map::INVALID_INDEX_VALUE))
      // No STAR - use arrival
      start = route->getArrivalLegsOffset();

    if(start < map::INVALID_INDEX_VALUE)
    {
      for(int i = start; i < route->size(); i++)
      {
        const RouteLeg& leg = route->at(i);
        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyArrival() && leg.getProcedureLegAltRestr().isValid())
          return i;
      }
    }
  }
  return map::INVALID_INDEX_VALUE;
}

int RouteAltitude::findDepartureLastRestricion() const
{
  if(route->hasAnyDepartureProcedure())
  {
    // Search for first restriction in a SID
    int start = fixRange(route->getDepartureLegsOffset() + route->getDepartureLegs().size());

    if(start < map::INVALID_INDEX_VALUE)
    {
      for(int i = start; i > 0; i--)
      {
        const RouteLeg& leg = route->at(i);

        if(leg.isAnyProcedure() && leg.getProcedureLeg().isAnyDeparture() && leg.getProcedureLegAltRestr().isValid())
          return i;
      }
    }
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
  // Flatten descent legs starting from the first restriction to destination
  int firstRestr = findApproachFirstRestricion();
  if(firstRestr < map::INVALID_INDEX_VALUE && firstRestr >= 0)
  {
    // Use eight iterations
    for(int i = 0; i < 8; i++)
    {
      for(int j = firstRestr + 1; j < size() - 1; j++)
        simplifyRouteAltitude(j, false /* departure */);
    }
  }

  // Flatten climb legs starting from the departure to the last restriction
  int lastRestriction = findDepartureLastRestricion();
  if(lastRestriction < map::INVALID_INDEX_VALUE && lastRestriction >= 0)
  {
    // Use eight iterations
    for(int i = 0; i < 8; i++)
    {
      for(int j = 1; j <= lastRestriction; j++)
        simplifyRouteAltitude(j, true /* departure */);
    }
  }
}

void RouteAltitude::simplifyRouteAltitude(int index, bool departure)
{
  if(index <= 0 || index >= size() - 1)
    return;

  RouteAltitudeLeg& midAlt = (*this)[index];
  bool rightAdjusted = false;
  const RouteAltitudeLeg *leftAlt = &at(index - 1);
  RouteAltitudeLeg *rightAlt = &(*this)[index + 1];

  if(atools::almostEqual(midAlt.getDistanceFromStart(), leftAlt->getDistanceFromStart()) && index >= 2)
    // Adjust leg if two equal legs (IAF) are after each other - otherwise no change will be done
    leftAlt = &at(index - 2);

  if(atools::almostEqual(midAlt.getDistanceFromStart(), rightAlt->getDistanceFromStart()) && index < size() - 2)
  {
    // Adjust leg if two equal legs (IAF) are after each other - otherwise no change will be done
    rightAlt = &(*this)[index + 2];
    rightAdjusted = true;
  }

  // Avoid dummy legs (e.g. missed)
  if(!leftAlt->isEmpty() && !rightAlt->isEmpty() && !midAlt.isEmpty())
  {
    // Check if the center is inside range for departure or destination
    if((departure && midAlt.getDistanceFromStart() < distanceTopOfClimb) ||
       (!departure && midAlt.getDistanceFromStart() > distanceTopOfDescent))
    {
      // Create a direct line from left ot right ignoring middle leg
      QLineF line(leftAlt->asPoint(), rightAlt->asPoint());

      // Calculate altitude for the middle leg using the direct line
      float t = (midAlt.getDistanceFromStart() - leftAlt->getDistanceFromStart()) / static_cast<float>(line.dx());
      // 0 = left, 1 = right
      QPointF mid = line.pointAt(t);

      // Change middle leg and adjust altitude
      midAlt.setY2(static_cast<float>(mid.y()));
      adjustAltitudeForRestriction(midAlt);

      // Also change neighbors
      if(rightAdjusted)
        (*this)[index + 1].setY1(midAlt.y2());

      rightAlt->setY1(midAlt.y2());
    }
  }
}

void RouteAltitude::calculate()
{
  clearAll();

  if(route->size() > 1)
  {
    if(route->getTotalDistance() < 0.5f)
      return;

    // Prefill all legs with distance and cruise altitude
    calculateDistances();

    if(calcTopOfClimb)
      calculateDeparture();

    if(calcTopOfDescent)
      calculateArrival();

    if(distanceTopOfClimb > distanceTopOfDescent ||
       (calcTopOfClimb && !(distanceTopOfClimb < map::INVALID_INDEX_VALUE)) ||
       (calcTopOfDescent && !(distanceTopOfDescent < map::INVALID_INDEX_VALUE)))
    {
      // TOD and TOC overlap or are invalid - cruise altitude is too high
      clearAll();

      // Reset all to cruise level - profile will print a message
      calculateDistances();
    }
    else
    {
      // Success - flatten legs
      if(simplify && (calcTopOfClimb || calcTopOfDescent))
        simplyfyRouteAltitudes();

      // Check for violations because of too low cruise
      for(RouteAltitudeLeg& leg : *this)
        violatesRestrictions |= violatesAltitudeRestriction(leg);

      calculateApproachIlsAndSlopes();
    }

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "distanceTopOfDescent" << distanceTopOfDescent
             << "distanceTopOfClimb" << distanceTopOfClimb;
    for(int i = 0; i < size(); i++)
      qDebug() << Q_FUNC_INFO << i << route->at(i).getIdent()
               << "geometry" << at(i).geometry << "NM/ft"
               << "topOfClimb" << at(i).topOfClimb
               << "topOfDescent" << at(i).topOfDescent
               << "valid" << at(i).isEmpty();
#endif
  }
}

float RouteAltitude::getDestinationAltitude() const
{
  const RouteLeg& destLeg = route->at(route->getDestinationLegIndex());
  if(destLeg.isAnyProcedure() && destLeg.getProcedureLegAltRestr().isValid())
    // Use restriction from procedure - field elevation + 50 ft
    return destLeg.getProcedureLegAltRestr().alt1;
  else if(destLeg.getAirport().isValid())
    // Airport altitude
    return destLeg.getPosition().getAltitude();

  // Other leg types (waypoint, VOR, etc.) remain on cruise
  return cruiseAltitide;
}

float RouteAltitude::getDestinationDistance() const
{
  int idx = route->getDestinationLegIndex();

  if(idx < map::INVALID_INDEX_VALUE)
    return at(idx).getDistanceFromStart();
  else
    return map::INVALID_DISTANCE_VALUE;
}

float RouteAltitude::departureAltitude() const
{
  const RouteLeg& startLeg = route->at(route->getDepartureLegIndex());
  if(startLeg.isAnyProcedure() && startLeg.getProcedureLegAltRestr().isValid())
    // Use restriction from procedure - field elevation + 50 ft
    return startLeg.getProcedureLegAltRestr().alt1;
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

void RouteAltitude::calculateDeparture()
{
  int departureLegIdx = route->getDepartureLegIndex();

  if(departureLegIdx > 0) // Assign altitude to dummy for departure airport too
    first().setAlt(departureAltitude());

  // Start from departure forward until hitting cruise altitude (TOC)
  for(int i = departureLegIdx; i < route->size(); i++)
  {
    // Calculate altitude for this leg based on altitude of the leg to the right
    RouteAltitudeLeg& alt = (*this)[i];
    if(alt.isEmpty())
      // Dummy leg
      continue;

    // Altitude of last leg
    float lastLegAlt = i > departureLegIdx ? at(i - 1).y2() : 0.;

    if(i <= departureLegIdx)
      // Departure leg - assign altitude to airport and RW too if available
      alt.setAlt(departureAltitude());
    else
    {
      // Set geometry for climbing
      alt.setY1(lastLegAlt);
      alt.setY2(lastLegAlt + alt.getDistanceTo() * altitudePerNmClimb);
    }

    if(!alt.isEmpty())
    {
      adjustAltitudeForRestriction(alt);

      // Avoid climbing above any below/at/between restrictions
      float maxAlt = findDepartureMaxAltitude(i);
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
        alt.setY2(std::min(alt.y2(), maxAlt));

      // Never lower than last leg
      alt.setY2(std::max(alt.y2(), lastLegAlt));

      if(i > 0 && alt.y2() > cruiseAltitide && !(distanceTopOfClimb < map::INVALID_ALTITUDE_VALUE))
      {
        // Reached TOC - calculate distance
        distanceTopOfClimb = distanceForAltitude(alt, cruiseAltitide);

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
  int departureLegIndex = route->getDepartureLegIndex();
  float lastAlt = getDestinationAltitude();

  // Calculate from last leg down until we hit the cruise altitude (TOD)
  for(int i = route->size() - 1; i >= 0; i--)
  {
    RouteAltitudeLeg& alt = (*this)[i];
    RouteAltitudeLeg *lastAltLeg = i < destinationLegIdx ? &(*this)[i + 1] : nullptr;
    float lastLegAlt = lastAltLeg != nullptr ? at(i + 1).y2() : 0.;

    // Start with altitude of the right leg (close to dest)
    float newAltitude = lastLegAlt;

    if(i <= destinationLegIdx)
    {
      // Leg leading to this point (right to this)
      float distFromRight = lastAltLeg != nullptr ? lastAltLeg->getDistanceTo() : 0.f;

      // Point of this leg
      newAltitude = lastAlt + distFromRight * altitudePerNmDescent;
    }

    if(!alt.isEmpty())
    {
      // Not a dummy leg
      newAltitude = adjustAltitudeForRestriction(newAltitude, alt.restriction);

      // Avoid climbing (up the descent slope) above any below/at/between restrictions
      float maxAlt = findApproachMaxAltitude(i);
      if(maxAlt < map::INVALID_ALTITUDE_VALUE)
        newAltitude = std::min(newAltitude, maxAlt);

      // Never lower than right leg
      newAltitude = std::max(newAltitude, lastAlt);

      if(!(distanceTopOfDescent < map::INVALID_ALTITUDE_VALUE) && newAltitude > cruiseAltitide && i + 1 < size())
      {
        // Reached TOD - calculate distance
        distanceTopOfDescent = distanceForAltitude(at(i + 1).getGeometry().last(),
                                                   QPointF(alt.getDistanceFromStart(), newAltitude), cruiseAltitide);

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
        alt.topOfDescent = true;
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

void RouteAltitude::calculateDistances()
{
  float distanceToLeg = 0.f;
  int destinationLegIdx = route->getDestinationLegIndex();

  // Fill all legs with distance and cruise altitude and add them to the vector
  for(int i = 0; i < route->size(); i++)
  {
    const RouteLeg& leg = route->at(i);

    RouteAltitudeLeg alt;

    if(i <= destinationLegIdx)
    {
      // Not a dummy (missed)
      alt.restriction = leg.getProcedureLegAltRestr();
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitide));
      distanceToLeg += leg.getDistanceTo();
      alt.geometry.append(QPointF(distanceToLeg, cruiseAltitide));
    }
    // else ignore missed approach dummy legs after destination runway

    append(alt);
  }

  // Set the procedure flag which is needed for drawing
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->at(i);
    const RouteLeg& last = route->at(i - 1);

    if(last.isRoute() || leg.isRoute() || // Any is route - also covers STAR to airport
       (last.getProcedureLeg().isAnyDeparture() && leg.getProcedureLeg().isAnyArrival()) || // empty space from SID to STAR, transition or approach
       (last.getProcedureLeg().isStar() && leg.getProcedureLeg().isArrival())) // empty space from STAR to transition or approach
      (*this)[i].procedure = false;
    else
      (*this)[i].procedure = true;
  }
}

void RouteAltitude::calculateApproachIlsAndSlopes()
{
  route->getApproachRunwayEndAndIls(destRunwayIls, destRunwayEnd);
}
