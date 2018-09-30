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

#ifndef LNM_ROUTEALTITUDE_H
#define LNM_ROUTEALTITUDE_H

#include "route/routealtitudeleg.h"

class Route;

/*
 * This class calculates altitudes for all route legs. This covers top of climb/descent
 * and sticks to all altitude restrictions of procedures while calculating.
 *
 * Uses the route object for calculation and caches all values.
 */
class RouteAltitude
  : private QVector<RouteAltitudeLeg>
{
public:
  /* route used for calculation */
  RouteAltitude(const Route *routeParam);
  ~RouteAltitude();

  /* Calculate altitudes for all legs. TOD and TOC are INVALID_DISTANCE_VALUE if these could not be calculated which
   * can happen for short routes with too high cruise altitude. */
  void calculate();

  /* Get interpolated altitude value for the given distance to destination in NM */
  float getAltitudeForDistance(float distanceToDest) const;

  /* Position on the route. EMPTY_POS if it could not be calculated. */
  atools::geo::Pos getTopOfDescentPos() const;
  atools::geo::Pos getTopOfClimbPos() const;

  /* Distance TOD to destination in NM or INVALID_DISTANCE_VALUE if it could not be calculated. */
  float getTopOfDescentFromDestination() const;

  /* Distance TOC from departure in NM or INVALID_DISTANCE_VALUE if it could not be calculated. */
  float getTopOfClimbDistance() const
  {
    return distanceTopOfClimb;
  }

  /* Distance TOD from departure in NM or INVALID_DISTANCE_VALUE if it could not be calculated. */
  float getTopOfDescentDistance() const
  {
    return distanceTopOfDescent;
  }

  /* Destination altitude. Either airport or runway if approach used. */
  float getDestinationAltitude() const;

  /* Distance to destination leg either airport or runway end in NM */
  float getDestinationDistance() const;

  /* value in feet. Require to set before compilation. */
  void setCruiseAltitide(float value)
  {
    cruiseAltitide = value;
  }

  /* Straighten out climb and descent segments which will also remove artifacts using
   * constant altitude before descending. */
  void setSimplify(bool value)
  {
    simplify = value;
  }

  /* Reset all except route pointer */
  void clearAll();

  /* Calculate TOD ramp if true. Otherwise uses cruise altitude */
  void setCalcTopOfDescent(bool value)
  {
    calcTopOfDescent = value;
  }

  /* Calculate TOC ramp if true. Otherwise uses cruise altitude */
  void setCalcTopOfClimb(bool value)
  {
    calcTopOfClimb = value;
  }

  /* true if the calculation result violates altitude restriction which can happen if the cruise altitude is too low */
  bool altRestrictionsViolated() const
  {
    return violatesRestrictions;
  }

  /* value is climb in feet per NM.Default is 333. */
  void setAltitudePerNmClimb(float value)
  {
    altitudePerNmClimb = value;
  }

  /* value is climb in feet per NM. Default is 333. */
  void setAltitudePerNmDescent(float value)
  {
    altitudePerNmDescent = value;
  }

  /* Pull only the needed methods in public space to have control over it and allow only read access */
  using QVector<RouteAltitudeLeg>::const_iterator;
  using QVector<RouteAltitudeLeg>::begin;
  using QVector<RouteAltitudeLeg>::end;
  using QVector<RouteAltitudeLeg>::at;
  using QVector<RouteAltitudeLeg>::size;
  using QVector<RouteAltitudeLeg>::isEmpty;

  const QVector<map::MapIls>& getDestRunwayIls() const
  {
    return destRunwayIls;
  }

  const map::MapRunwayEnd& getDestRunwayEnd() const
  {
    return destRunwayEnd;
  }

private:
  /* Adjust the altitude to fit into the restriction. I.e. raise if it is below an at or above restriction */
  float adjustAltitudeForRestriction(float altitude, const proc::MapAltRestriction& restriction) const;
  void adjustAltitudeForRestriction(RouteAltitudeLeg& leg) const;

  /* Find the maximum allowed altitide for approach/STAR and SID beginning from index */
  float findApproachMaxAltitude(int index) const;
  float findDepartureMaxAltitude(int index) const;

  /* find first and last altitude restriction for approach/STAR and SID  */
  int findApproachFirstRestricion() const;
  int findDepartureLastRestricion() const;

  /* Departure altitude. Either airport or runway. */
  float departureAltitude() const;

  /* interpolate distance where the given leg intersects the given altitude */
  float distanceForAltitude(const RouteAltitudeLeg& leg, float altitude);
  float distanceForAltitude(const QPointF& leg1, const QPointF& leg2, float altitude);

  /* Check if leg violates any altitude restricton */
  bool violatesAltitudeRestriction(const RouteAltitudeLeg& leg) const;

  /* Prefill all legs with distances and cruise altitude. Also mark the procedure flag for painting. */
  void calculateDistances();

  /* Calculate altitude and TOD for approach/STAR or no procedures */
  void calculateArrival();

  /* Calculate altitude and TOC for SID  or no procedures */
  void calculateDeparture();

  /* Get ILS (for ILS and LOC approaches) and VASI pitch if approach is available */
  void calculateApproachIlsAndSlopes();

  /* Flatten altitude legs to avoid bends and flats when climbing/descending */
  void simplyfyRouteAltitudes();
  void simplifyRouteAltitude(int index, bool departure);

  /* Adjust range for vector size */
  int fixRange(int index) const;

  /* NM from start */
  float distanceTopOfClimb = map::INVALID_DISTANCE_VALUE,
        distanceTopOfDescent = map::INVALID_DISTANCE_VALUE;

  const Route *route;

  /* Configuration options */
  bool simplify = false;
  bool calcTopOfDescent = true;
  bool calcTopOfClimb = true;

  /* Set by calculate */
  bool violatesRestrictions = false;

  /* Climb or descent speed in ft per NM */
  float altitudePerNmClimb = 333.f;
  float altitudePerNmDescent = 333.f;

  float cruiseAltitide = 1000.f;

  QVector<map::MapIls> destRunwayIls;
  map::MapRunwayEnd destRunwayEnd;
};

#endif // LNM_ROUTEALTITUDE_H
