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

#ifndef LNM_ROUTEALTITUDE_H
#define LNM_ROUTEALTITUDE_H

#include "route/routealtitudeleg.h"

#include <QApplication>

namespace atools {
namespace grib {
struct Wind;
}
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

/* Result package of fuel and time calculation or estimate */
struct FuelTimeResult
{
  float
  /* To destination */
    fuelLbsToDest = map::INVALID_WEIGHT_VALUE,
    fuelGalToDest = map::INVALID_VOLUME_VALUE,
    timeToDest = map::INVALID_TIME_VALUE,

  /* To top of descent */
    fuelLbsToTod = map::INVALID_WEIGHT_VALUE,
    fuelGalToTod = map::INVALID_VOLUME_VALUE,
    timeToTod = map::INVALID_TIME_VALUE,

  /* To next waypoint */
    fuelLbsToNext = map::INVALID_WEIGHT_VALUE,
    fuelGalToNext = map::INVALID_VOLUME_VALUE,
    timeToNext = map::INVALID_TIME_VALUE;

  bool estimatedFuel = false, estimatedTime = false;

  bool isTimeToDestValid() const
  {
    return timeToDest < map::INVALID_TIME_VALUE;
  }

  bool isTimeToTodValid() const
  {
    return timeToTod < map::INVALID_TIME_VALUE;
  }

  bool isTimeToNextValid() const
  {
    return timeToNext < map::INVALID_TIME_VALUE;
  }

  bool isFuelToDestValid() const
  {
    return fuelLbsToDest < map::INVALID_WEIGHT_VALUE && fuelGalToDest < map::INVALID_VOLUME_VALUE;
  }

  bool isFuelToTodValid() const
  {
    return fuelLbsToTod < map::INVALID_WEIGHT_VALUE && fuelGalToTod < map::INVALID_VOLUME_VALUE;
  }

  bool isFuelToNextValid() const
  {
    return fuelLbsToNext < map::INVALID_WEIGHT_VALUE && fuelGalToNext < map::INVALID_VOLUME_VALUE;
  }

};

QDebug operator<<(QDebug out, const FuelTimeResult& obj);

/*
 * This class calculates altitudes for all route legs. This covers top of climb/descent
 * and sticks to all altitude restrictions of procedures while calculating.
 *
 * Also contains methods to calculate trip time, wind and fuel consumption based on AircraftPerf information.
 *
 * Fuel units (weight or volume) are based on what is used in the given AircraftPerf object. Uses always either gallon
 * or lbs.
 *
 * Uses the route object for calculation and caches all values.
 *
 * First leg is departure and has length 0. Destination leg is leg to destination airport.
 * Order is same as in class Route.
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
   * Fuel units (weight or volume) are based on what is used in the given AircraftPerf object.
   * Calculate travelling time and fuel consumption based on given performance object and wind
   * value in feet. */
  void calculateAll(const atools::fs::perf::AircraftPerf& perf, float cruiseAltitudeFt);

  /* Get interpolated altitude value in ft for the given distance to destination in NM.
   *  Not for missed and alternate legs. */
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

  /* Distance for cruise phase only in NM */
  float getCruiseDistance() const
  {
    return getTotalDistance() - distanceTopOfClimb - getTopOfDescentFromDestination();
  }

  /* Destination altitude. Either airport or runway if approach used. */
  float getDestinationAltitude() const;

  /* Distance to destination leg either airport or runway end in NM */
  float getDestinationDistance() const;

  /* Departure altitude. Either airport or runway. */
  float getDepartureAltitude() const;

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

  /* Returns empty object if index is invalid */
  const RouteAltitudeLeg& value(int i) const;

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

  /* Route distance in NM excluding alternates */
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

  /* True if result is not valid and error messages exist */
  bool hasErrors() const;
  QString getErrorStrings(QStringList& toolTip) const;

  /* Get an array for all altitudes in feet. Includes procedure points. */
  QVector<float> getAltitudes() const;

  /* Average wind direction for route degrees true */
  float getWindDirection() const
  {
    return windDirectionAvg;
  }

  /* Average wind speed for this route in knots */
  float getWindSpeedAverage() const
  {
    return windSpeedAvg;
  }

  /* Average head wind speed for this route in knots. Negative values are tailwind. */
  float getHeadWindAverage() const
  {
    return windHeadAvg;
  }

  /* true if result of calculation was valid */
  bool isValidProfile() const
  {
    return validProfile;
  }

  /* Average head wind for climb phase */
  float getClimbHeadWind() const
  {
    return windHeadClimb;
  }

  /* Average head wind for cruise phase */
  float getCruiseHeadWind() const
  {
    return windHeadCruise;
  }

  /* Average head wind for descent phase */
  float getDescentHeadWind() const
  {
    return windHeadDescent;
  }

  /* Equal to GS */
  float getClimbSpeedWindCorrected() const
  {
    return climbSpeedWindCorrected;
  }

  /* Equal to GS */
  float getDescentSpeedWindCorrected() const
  {
    return descentSpeedWindCorrected;
  }

  /* Equal to GS */
  float getCruiseSpeedWindCorrected() const
  {
    return cruiseSpeedWindCorrected;
  }

  /* Fuel consumption of climb part of the flight plan */
  float getClimbFuel() const
  {
    return climbFuel;
  }

  /* Fuel consumption of cruise part of the flight plan */
  float getCruiseFuel() const
  {
    return cruiseFuel;
  }

  /* Fuel consumption of descent part of the flight plan */
  float getDescentFuel() const
  {
    return descentFuel;
  }

  /* Time of climb part of the flight plan */
  float getClimbTime() const
  {
    return climbTime;
  }

  /* Time of cruise part of the flight plan */
  float getCruiseTime() const
  {
    return cruiseTime;
  }

  /* Time of descent part of the flight plan */
  float getDescentTime() const
  {
    return descentTime;
  }

  /* Calculates needed fuel to destination and TOD. Falls back to current aircraft consumption values if profile or
   * altitude legs are not valid. distanceToDest: Aircraft position distance to destination. */
  void calculateFuelAndTimeTo(FuelTimeResult& calculation, float distanceToDest, float distanceToNext,
                              const atools::fs::perf::AircraftPerf& perf, float aircraftFuelFlowLbs,
                              float aircraftFuelFlowGal, float aircraftGroundSpeed, int activeLeg) const;

private:
  friend QDebug operator<<(QDebug out, const RouteAltitude& obj);

  /* Calculate altitudes for all legs. Error list will be filled with altitude restriction violations. */
  void calculate(QStringList& altRestErrors);

  /* Calculate travelling time and fuel consumption based on given performance object and wind */
  void calculateTrip(const atools::fs::perf::AircraftPerf& perf);

  /* Adjust the altitude to fit into the restriction. I.e. raise if it is below an at or above restriction */
  float adjustAltitudeForRestriction(float altitude, const proc::MapAltRestriction& restriction) const;
  void adjustAltitudeForRestriction(RouteAltitudeLeg& leg) const;

  /* Find the maximum allowed altitude for approach/STAR and SID beginning from index */
  float findApproachMaxAltitude(int index) const;
  float findDepartureMaxAltitude(int index) const;

  /* find first and last altitude restriction for approach/STAR and SID  */
  int findApproachFirstRestricion() const;
  int findDepartureLastRestricion() const;

  /* interpolate distance where the given leg intersects the given altitude */
  float distanceForAltitude(const RouteAltitudeLeg& leg, float altitude);
  float distanceForAltitude(const QPointF& leg1, const QPointF& leg2, float altitude);

  /* Check if leg violates any altitude restricton. Returns true and error message if yes. */
  bool violatesAltitudeRestriction(QString& errorMessage, int legIndex) const;

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

  int indexForDistance(float distanceToDest) const;

  void collectErrors(const QStringList& altRestrErrors);

  float windCorrectedGroundSpeed(atools::grib::Wind& wind, float course, float speed);

  /* NM from start */
  float distanceTopOfClimb = map::INVALID_DISTANCE_VALUE,
        distanceTopOfDescent = map::INVALID_DISTANCE_VALUE;

  /* Fuel and performance calculation results */
  float travelTime = 0.f, averageGroundSpeed = 0.f;

  /* Accumulated time and fuel for each phase */
  float climbFuel = 0.f, cruiseFuel = 0.f, descentFuel = 0.f,
        climbTime = 0.f, cruiseTime = 0.f, descentTime = 0.f;

  float tripFuel = 0.f, alternateFuel = 0.f;
  bool unflyableLegs = false;

  /*  Average wind values for the whole route */
  float windDirectionAvg = 0.f, windSpeedAvg = 0.f, windHeadAvg = 0.f,
        windHeadClimb = 0.f, windHeadCruise = 0.f, windHeadDescent = 0.f;

  /* Wind corrected climb speeds for second iteration. Ground speed. */
  float climbSpeedWindCorrected = 0.f, cruiseSpeedWindCorrected = 0.f, descentSpeedWindCorrected = 0.f;

  /* index in altitude legs */
  int legIndexTopOfClimb = map::INVALID_INDEX_VALUE,
      legIndexTopOfDescent = map::INVALID_INDEX_VALUE;

  const Route *route;

  /* Configuration options */
  bool simplify = true, calcTopOfDescent = true, calcTopOfClimb = true;

  /* Has TOC and TOD  */
  bool validProfile = false;

  /* From aircraft performance */
  /* Climb and descent are corrected for tail/head wind duringfor second iteration in significant wind */
  float climbRateWindFtPerNm = 333.f, descentRateWindFtPerNm = 333.f, cruiseAltitide = 0.f;

  /* Set by calculate */
  /* Contains a list of messages if the calculation result violates altitude restrictions
   * which can happen if the cruise altitude is too low */
  QStringList errors;

  QVector<map::MapIls> destRunwayIls;
  map::MapRunwayEnd destRunwayEnd;
};

QDebug operator<<(QDebug out, const RouteAltitude& obj);

#endif // LNM_ROUTEALTITUDE_H
