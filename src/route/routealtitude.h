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

#ifndef LNM_ROUTEALTITUDE_H
#define LNM_ROUTEALTITUDE_H

#include "route/routealtitudeleg.h"

#include <QApplication>

namespace atools {
namespace geo {
class LineString;
}
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

namespace wind {
struct WindAverageRoute;
}

class Route;

/*
 * This class calculates altitudes for all route legs. This covers top of climb/descent
 * and sticks to all altitude restrictions of procedures while calculating.
 *
 * Also contains methods to calculate trip time, wind and fuel consumption base on AircraftPerf information.
 *
 * Uses the route object for calculation and caches all values.
 */
class RouteAltitude
  : private QVector<RouteAltitudeLeg>
{
  Q_DECLARE_TR_FUNCTIONS(RouteAltitude)

public:
  /* route used for calculation */
  RouteAltitude(const Route *routeParam);
  ~RouteAltitude();

  /* Calculate altitudes for all legs. TOD and TOC are INVALID_DISTANCE_VALUE if these could not be calculated which
   * can happen for short routes with too high cruise altitude.
   * Use perf to calculate climb and descent legs
   */
  void calculate();

  /* Calculate travelling time and fuel consumption based on given performance object and wind */
  void calculateTrip(const atools::fs::perf::AircraftPerf& perf);

  /* Get interpolated altitude value in ft for the given distance to destination in NM */
  float getAltitudeForDistance(float distanceToDest) const;

  /* 0 if invalid */
  float getTravelTimeHours() const;

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
  void setCruiseAltitude(float value)
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

  const RouteAltitudeLeg& at(int i) const
  {
    if(i < 0 || i > size() - 1)
      qWarning() << Q_FUNC_INFO << "Invalid index" << i;

    return QVector::at(i);
  }

  int size() const
  {
    return QVector::size();
  }

  bool isEmpty() const // OK
  {
    return QVector::isEmpty();
  }

  /* Get a list of matching ILS which have a slope and are not too far away from runway (in case of CTL) */
  const QVector<map::MapIls>& getDestRunwayIls() const
  {
    return destRunwayIls;
  }

  /* Get runway end at destination if any. Used to get the VASI information */
  const map::MapRunwayEnd& getDestRunwayEnd() const
  {
    return destRunwayEnd;
  }

  /* Leg index containing the TOC */
  int getTopOfClimbLegIndex() const
  {
    return legIndexTopOfClimb;
  }

  /* Leg index containing the TOD */
  int getTopOfDescentLegIndex() const
  {
    return legIndexTopOfDescent;
  }

  /* Average ground speed in knots */
  float getAverageGroundSpeed() const
  {
    return averageGroundSpeed;
  }

  bool isFuelUnitVolume() const;

  /* Route distance in NM */
  float getTotalDistance() const;

  /* Trip fuel. Does not include reserve, extra, taxi and contingency fuel, Unit depends on performance settings. */
  float getTripFuel() const
  {
    return tripFuel;
  }

  /* Fuel to the farthest alternate airport from the destination */
  float getAlternateFuel() const
  {
    return alternateFuel;
  }

  /* Get all fuel to load for flight */
  float getBlockFuel(const atools::fs::perf::AircraftPerf& perf) const;

  /* Get fuel amount at destination */
  float getDestinationFuel(const atools::fs::perf::AircraftPerf& perf) const;

  /* Get contingency fuel amount*/
  float getContingencyFuel(const atools::fs::perf::AircraftPerf& perf) const;

  /* true if there are legs where the headwind is higher than aircraft capabilities */
  bool hasUnflyableLegs() const
  {
    return unflyableLegs;
  }

  /* Cruising altitude in feet */
  float getCruiseAltitide() const
  {
    return cruiseAltitide;
  }

  bool hasErrors() const;
  QStringList getErrorStrings(QString& tooltip) const;

  void setClimbRateFtPerNm(float value)
  {
    climbRateFtPerNm = value;
  }

  void setDesentRateFtPerNm(float value)
  {
    descentRateFtPerNm = value;
  }

  /* Get an array for all altitudes in feet. Includes procedure points. */
  QVector<float> getAltitudes() const;

  /* Average wind direction for route degrees true */
  float getWindDirection() const
  {
    return windDirection;
  }

  /* Average wind speed for this route in knots */
  float getWindSpeed() const
  {
    return windSpeed;
  }

  /* Average head wind speed for this route in knots. Negative values are tailwind. */
  float getHeadWind() const
  {
    return windHead;
  }

  /* Average crosswind for this route. If cross wind is < 0 wind is from left */
  float getCrossWind() const
  {
    return windCross;
  }

  bool isValidProfile() const
  {
    return validProfile;
  }

private:
  friend QDebug operator<<(QDebug out, const RouteAltitude& obj);

  /* Adjust the altitude to fit into the restriction. I.e. raise if it is below an at or above restriction */
  float adjustAltitudeForRestriction(float altitude, const proc::MapAltRestriction& restriction) const;
  void adjustAltitudeForRestriction(RouteAltitudeLeg& leg) const;

  /* Find the maximum allowed altitude for approach/STAR and SID beginning from index */
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

  /* Fill line object in leg with geometry */
  void fillGeometry();

  /* NM from start */
  float distanceTopOfClimb = map::INVALID_DISTANCE_VALUE,
        distanceTopOfDescent = map::INVALID_DISTANCE_VALUE;

  /* Fuel and performance calculation results */
  float tripFuel = 0.f, alternateFuel = 0.f, travelTime = 0.f, averageGroundSpeed = 0.f;
  bool unflyableLegs = false;

  // Average wind values for the whole route
  float windDirection = 0.f, windSpeed = 0.f, windHead = 0.f, windCross = 0.f;

  /* index in altitude legs */
  int legIndexTopOfClimb = map::INVALID_INDEX_VALUE,
      legIndexTopOfDescent = map::INVALID_INDEX_VALUE;

  const Route *route;

  /* Configuration options */
  bool simplify = true;
  bool calcTopOfDescent = true;
  bool calcTopOfClimb = true;

  /* Set by calculate */
  bool violatesRestrictions = false;

  /* Has TOC and TOD */
  bool validProfile = false;

  float climbRateFtPerNm = 333.f, descentRateFtPerNm = 333.f;
  float cruiseAltitide = 0.f;

  QVector<map::MapIls> destRunwayIls;
  map::MapRunwayEnd destRunwayEnd;
};

QDebug operator<<(QDebug out, const RouteAltitude& obj);

#endif // LNM_ROUTEALTITUDE_H
