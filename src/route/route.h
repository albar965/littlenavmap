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

#ifndef LITTLENAVMAP_ROUTE_H
#define LITTLENAVMAP_ROUTE_H

#include "route/routeleg.h"
#include "route/routeflags.h"

#include "fs/pln/flightplan.h"

class CoordinateConverter;
class FlightplanEntryBuilder;
class RouteAltitude;
class RouteAltitudeLeg;

/*
 * Aggregates the flight plan and is a list of all route map objects. Also contains and stores information
 * about the active route leg and current aircraft position.
 *
 * The flight plan and the route is kept in sync and contains dummy entries for all procedure legs.
 * Destination airport is added after any arrival procedures. Alternates after that.
 *
 * Procedure information is kept in FlightPlan properties and will be reloaded on demand.
 *
 * Index functions return map::INVALID_INDEX_VALUE if not valid.
 * Leg methods return invalid legs if unusable index.
 *
 * Example layout of the list:
 *  0	DEPARTURE (AIRPORT), distanceTo 0
 *  1	SID Leg 1 (RW)
 *  2	SID Leg 2
 *  3	WPT 1
 *  4	WPT 2
 *  4	WPT 3
 *  5	STAR Leg 1
 *  6	STAR Leg 2
 *  7	APPR Leg 1
 *  8	APPR Leg 2 (RW), distanceTo = totalDistance
 *  9	MISSED Leg 1, (excluded from total distance), distanceTo = distance to end of missed
 * 10	MISSED Leg 2,              "
 * 11	DESTINATION (AIRPORT), distanceTo = distance from "APPR Leg 2 (RW)"
 * 12	ALTERNATE 1 (distance calculated from dest airport)
 * 13	ALTERNATE 2               "
 */
class Route :
  private QList<RouteLeg>
{
  Q_DECLARE_TR_FUNCTIONS(Route)

public:
  Route();
  Route(const Route& other);
  virtual ~Route();

  Route& operator=(const Route& other);

  /* Update navdata properties in flightplan properties for export and save */
  void updateRouteCycleMetadata();

  /* Insert properties for aircraft performance */
  void updateAircraftPerfMetadata();

  /* Update positions, distances and try to select next leg*/
  void updateActiveLegAndPos(const map::PosCourse& pos);
  void updateActiveLegAndPos(bool force, bool flying);

  /*
   * Get multiple flight plan distances for the given position. If value pointers are null they will be ignored.
   * All distances in nautical miles. Considers the active leg.
   * @param pos
   * @param distFromStartNm Distance from departure
   * @param distToDestNm Distance to destination
   * @param nextLegDistance Distance to next leg
   * @param crossTrackDistance Cross track distance to current leg. Positive and
   * negative values indicate left or right of track.
   * @return false if no current/next leg was found
   */
  bool getRouteDistances(float *distFromStart, float *distToDest = nullptr,
                         float *nextLegDistance = nullptr, float *crossTrackDistance = nullptr,
                         float *projectionDistance = nullptr) const;

  /* Calculated distance for aircraft projection in profile */
  float getProjectionDistance() const;

  /* Start from distance but values do not decrease if aircraft is leaving route.
   *  Ignores active and looks for all legs.
   *
   * Ignores missed approach legs.*/
  float getDistanceFromStart(const atools::geo::Pos& pos) const;

  /* Ignores approach objects if ignoreNotEditable is true.
   *  Checks course if not INVALID_COURSE_VALUE */
  int getNearestRouteLegResult(const atools::geo::Pos& pos, atools::geo::LineDistance& lineDistanceResult,
                               bool ignoreNotEditable, bool ignoreMissed) const;

  /* First route leg after departure procedure or 0 for departure airport */
  int getStartIndexAfterProcedure() const;
  const RouteLeg& getStartAfterProcedure() const;

  /* Last leg of departure procedure or 0 for departure airport */
  int getLastIndexOfDepartureProcedure() const;
  const RouteLeg& getLastLegOfDepartureProcedure() const;

  /* Last route leg before STAR, transition or approach */
  int getDestinationIndexBeforeProcedure() const;
  const RouteLeg& getDestinationBeforeProcedure() const;

  /* Index for first alternate airport or invalid if none */
  int getAlternateLegsOffset() const
  {
    return alternateLegsOffset;
  }

  int getNumAlternateLegs() const
  {
    return numAlternateLegs;
  }

  /* map::INVALID_INDEX_VALUE if no active.
   * 1 for first leg to route.size() - 1 for active legs.
   * 0 is special case for plans consisting of only one airport */
  int getActiveLegIndex() const
  {
    return activeLegIndex;
  }

  /* true if active leg is valid. false for special one airport case */
  bool isActiveValid() const
  {
    return activeLegIndex > 0 && activeLegIndex < size();
  }

  /* true if active leg is an alternate leg */
  bool isActiveAlternate() const
  {
    return alternateLegsOffset != map::INVALID_INDEX_VALUE && activeLegIndex != map::INVALID_INDEX_VALUE &&
           activeLegIndex >= alternateLegsOffset;
  }

  /* true if active leg is destination airport. false if not or any arrival procedure is used. */
  bool isActiveDestinationAirport() const;

  /* Set departure parking information. Parking clears start and vice versa. */
  void setDepartureParking(const map::MapParking& departureParking);
  void setDepartureStart(const map::MapStart& departureStart);
  map::MapParking getDepartureParking() const;
  map::MapStart getDepartureStart() const;

  /* Create copies of first and last to ease tracking */
  const RouteLeg& getLastLeg() const
  {
    return constLast();
  }

  const RouteLeg& getFirstLeg() const
  {
    return constFirst();
  }

  /* Returns an empty leg if the index is not valid */
  const RouteLeg& getLegAt(int index) const;

  /* First leg of departure procedure. 1 if SID used otherwise 0. */
  int getSidLegIndex() const;
  const RouteLeg& getSidLeg() const;

  /* First leg. Always 0 if not empty. */
  int getDepartureAirportLegIndex() const;
  const RouteLeg& getDepartureAirportLeg() const;

  /* Either destination airport or last leg of approach procedure (usually runway) before missed.
   * Not necessarily an airport or runway. */
  int getDestinationLegIndex() const;
  const RouteLeg& getDestinationLeg() const;

  /* Always destination airport after missed (if any) and one before the alternate if any.
   * Not necessarily an airport. */
  int getDestinationAirportLegIndex() const;
  const RouteLeg& getDestinationAirportLeg() const;

  /* true if flight plan is not empty and airport is departure or destination */
  bool isAirportDeparture(const QString& ident) const;
  bool isAirportDestination(const QString& ident) const;
  bool isAirportAlternate(const QString& ident) const;

  /* true if ident is same for departure and destination */
  bool isAirportRoundTrip(const QString& ident) const;

  /* Get flight plan dependent flags for airport procedures and the given airport. Used to select procedure filter */
  void getAirportProcedureFlags(const map::MapAirport& airport, int index, bool& departureFilter,
                                bool& arrivalFilter, bool& hasDeparture, bool& hasAnyArrival,
                                bool& airportDeparture, bool& airportDestination, bool& airportRoundTrip) const;
  void getAirportProcedureFlags(const map::MapAirport& airport, int index, bool& departureFilter,
                                bool& arrivalFilter) const;

  /* Get active leg or null if there is none */
  const RouteLeg *getActiveLeg() const;

  /* Give next procedure leg instead of route leg if approaching one */
  const RouteLeg *getActiveLegCorrected(bool *corrected = nullptr) const;

  /* Currently flying missed approach */
  bool isActiveMissed() const;

  /* Currently flying procedure including missed */
  bool isActiveProcedure() const;

  /* Corrected methods replace the current leg with the initial fix
   * if one follows between route and transition/approach.  */
  int getActiveLegIndexCorrected(bool *corrected = nullptr) const;

  /* Get top of descent or climb position based on the option setting (default is 3 nm per 1000 ft) */
  atools::geo::Pos getTopOfDescentPos() const;
  atools::geo::Pos getTopOfClimbPos() const;

  /* Distance from TOD to destination in nm */
  float getTopOfDescentFromDestination() const;

  /* Distance TOD from departure in NM or INVALID_DISTANCE_VALUE if it could not be calculated. */
  float getTopOfDescentDistance() const;
  int getTopOfDescentLegIndex() const;

  /* Distance TOC from departure in NM or INVALID_DISTANCE_VALUE if it could not be calculated. */
  float getTopOfClimbDistance() const;
  int getTopOfClimbLegIndex() const;

  /* Get interpolated altitude value in ft for the given distance to destination in NM.
   *  Not for missed and alternate legs. */
  float getAltitudeForDistance(float currentDistToDest) const;

  /* Get either calculated or required by procedure vertical angle. */
  float getVerticalAngleAtDistance(float distanceToDest, bool *required = nullptr) const;

  /* Vertical angle to reach next waypoint at calculated altitude depending on user aircraft altitude. */
  float getVerticalAngleToNext(float nearestLegDistance) const;

  /* Same as above for TAS knots from performance profile */
  float getSpeedForDistance(float currentDistToDest) const;

  /* Total route distance in nautical miles */
  float getTotalDistance() const
  {
    return totalDistance;
  }

  /* The flight plan has dummy entries for procedure points that are flagged as no save */
  const atools::fs::pln::Flightplan& getFlightplanConst() const
  {
    return flightplan;
  }

  atools::fs::pln::Flightplan& getFlightplan()
  {
    return flightplan;
  }

  bool isTypeVfr() const
  {
    return flightplan.getFlightplanType() == atools::fs::pln::VFR;
  }

  bool isTypeIfr() const
  {
    return flightplan.getFlightplanType() == atools::fs::pln::IFR;
  }

  /* Value in flight plan is stored in local unit */
  float getCruiseAltitudeFt() const;

  void setFlightplan(const atools::fs::pln::Flightplan& value)
  {
    flightplan = value;
  }

  /* Get nearest flight plan leg to given screen position xs/ys. */
  void getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance, map::MapResult& mapobjects,
                  map::MapObjectQueryTypes types, const QVector<map::MapRef>& routeDrawnNavaids) const;

  /* Get nearest recommended navaids to given screen position xs/ys. */
  void getNearestRecommended(const CoordinateConverter& conv, int xs, int ys, int screenDistance, map::MapResult& mapobjects,
                             map::MapObjectQueryTypes types, const QVector<map::MapRef>& routeDrawnNavaids) const;

  void eraseAirway(int row);

  /* @return true if any leg has an airway name */
  bool hasAirways() const;

  bool hasUserWaypoints() const;

  /* @return true if departure is an airport and parking is set */
  bool hasDepartureParking() const;

  /* @return true if departure is an airport and start position is a runway */
  bool hasDepartureRunway() const;

  /* @return true if departure is an airport and a start position is set */
  bool hasDepartureStart() const;

  /* @return true if flight plan has no departure, no destination and no intermediate waypoints */
  bool isFlightplanEmpty() const;

  /* @return true if departure is an airport */
  bool hasValidDeparture() const;
  bool hasValidDepartureAndRunways() const;

  /* @return true if destination is an airport */
  bool hasValidDestination() const;
  bool hasValidDestinationAndRunways() const;

  /* @return true if departure start position is a helipad */
  bool hasDepartureHelipad() const;

  /* @return true if has intermediate waypoints */
  bool hasEntries() const;

  /* @return true if route has at lease one alternate */
  bool hasAlternates() const
  {
    return alternateLegsOffset != map::INVALID_INDEX_VALUE;
  }

  /* @return true if it has at least two waypoints */
  bool canCalcRoute() const;

  /* returns true if a route can be calculated between the selected legs */
  bool canCalcSelection(int firstIndex, int lastIndex) const;

  /* returns true if a route snippet can be saved between the selected legs */
  bool canSaveSelection(int firstIndex, int lastIndex) const;

  /* Get a new number for a user waypoint for automatic naming */
  int getNextUserWaypointNumber() const;

  /* Index from 0 (departure) to size() -1 */
  bool canEditLeg(int index) const;

  /* Index from 0 (departure) to size() -1. true if point can be moved around or deleted in the flight plan */
  bool canEditPoint(int index) const;

  /* Index from 0 (departure) to size() -1. true if a comment can be attached to the waypoint of the leg */
  bool canEditComment(int index) const;

  bool hasAnyProcedure() const
  {
    return hasAnyApproachProcedure() || hasAnySidProcedure() || hasAnyStarProcedure();
  }

  bool hasCustomApproach() const
  {
    return approachLegs.isCustomApproach();
  }

  bool hasCustomDeparture() const
  {
    return sidLegs.isCustomDeparture();
  }

  /* Final approach */
  bool hasAnyApproachProcedure() const
  {
    return !approachLegs.isEmpty();
  }

  /* STAR or final approach */
  bool hasAnyArrivalProcedure() const
  {
    return hasAnyApproachProcedure() || hasAnyStarProcedure();
  }

  /* Final approach transition */
  bool hasTransitionProcedure() const
  {
    return !approachLegs.transitionLegs.isEmpty();
  }

  bool hasAnyStarProcedure() const
  {
    return !starLegs.isEmpty();
  }

  bool hasAnySidProcedure() const
  {
    return !sidLegs.isEmpty();
  }

  /* Get the various procedure names */
  void getSidStarNames(QString& sid, QString& sidTrans, QString& star, QString& starTrans) const;

  /* Arrival rw is either STAR or approach */
  void getRunwayNames(QString& departure, QString& arrival) const;

  /* Get approach ARINC name including suffix and suffix separately */
  void getApproachNames(QString& approachArincName, QString& approachTransition, QString& approachSuffix) const;

  /* Full name plus forward slash  separator like "/LMA.I05R-Z". Transition plus ARINC name. */
  QString getFullApproachName() const;

  const QString& getSidRunwayName() const;
  const QString& getStarRunwayName() const;
  const QString& getApproachRunwayName() const;

  /* Assign and update internal indexes for approach legs. Depending if legs are type SID, STAR,
   * transition or approach they are added at the end of start of the route
   *  call updateProcedureLegs after setting */
  void setArrivalProcedureLegs(const proc::MapProcedureLegs& legs)
  {
    approachLegs = legs;
  }

  void setStarProcedureLegs(const proc::MapProcedureLegs& legs)
  {
    starLegs = legs;
  }

  void setSidProcedureLegs(const proc::MapProcedureLegs& legs)
  {
    sidLegs = legs;
  }

  /* Outbound course from the waypoint of the previous leg, i.e. course at the start of the given leg. Uses VOR declination if present. */
  void getOutboundCourse(int index, float& magCourse, float& trueCourse) const;

  /* Inbound course to the waypoint of the given leg, i.e. course at the end of the given leg.  Uses VOR declination if present.*/
  void getInboundCourse(int index, float& magCourse, float& trueCourse) const;

  /* Insert legs of procedures into flight plan and update all offsets and indexes */
  void updateProcedureLegs(FlightplanEntryBuilder *entryBuilder, bool clearOldProcedureProperties, bool cleanupRoute);

  /* Remove all intermediate legs between departure and destination. Procedures and alternates are not touched. */
  void removeRouteLegs();

  /* Does not delete flight plan properties. Clears the MapProcedure structures. */
  void clearProcedures(proc::MapProcedureTypes type);

  /* Removes legs that match the given procedures from the route and/or flightplan */
  void clearProcedureLegs(proc::MapProcedureTypes type, bool clearRoute = true, bool clearFlightplan = true);

  /* Remove alternate airport(s) from route and flightplan */
  void removeAlternateLegs();

  /* Remove missed approach legs from route and flightplan */
  void removeMissedLegs();

  /* Deletes flight plan properties too */
  void removeProcedureLegs();
  void removeProcedureLegs(proc::MapProcedureTypes type);

  /* Needed to activate missed approach sequencing or not depending on visibility state */
  void setShownMapFeatures(map::MapTypes types)
  {
    shownTypes = types;
  }

  const atools::geo::Rect& getBoundingRect() const
  {
    return boundingRect;
  }

  /* Leg end position - i.e. the waypoint at the end of the leg */
  const atools::geo::Pos getPositionAt(int i) const;

  /* Leg start position - i.e. the waypoint at the end of the previous leg */
  const atools::geo::Pos getPrevPositionAt(int i) const;

  /* Update distance, course, bounding rect and total distance for route map objects.
   *  Also calculates maximum number of user points. */
  void updateAll();

  /* Use an expensive heuristic to update the missing regions in all airports
   * before export for formats which need it. */
  void updateAirportRegions();

  /* Set active leg and update all internal distances */
  void setActiveLeg(int value);

  /* Set to no active leg */
  void resetActive();

  /* true if type is airport at the given index and is after an arrival procedure (approach and transition) */
  bool isAirportAfterArrival(int index);

  /* Get approach and transition in one legs struct */
  const proc::MapProcedureLegs& getApproachLegs() const
  {
    return approachLegs;
  }

  /* Get STAR legs only */
  const proc::MapProcedureLegs& getStarLegs() const
  {
    return starLegs;
  }

  /* Get SID legs only */
  const proc::MapProcedureLegs& getSidLegs() const
  {
    return sidLegs;
  }

  /* Index of first transition and/or approach leg in the route */
  int getApproachLegsOffset() const
  {
    return approachLegsOffset;
  }

  /* Index of first transition and/or approach/STAR leg in the route or INVALID_INDEX_VALUE if no procedures */
  int getArrivaLegsOffset() const;

  /* Index of first SID leg in the route */
  int getSidLegsOffset() const
  {
    return sidLegsOffset;
  }

  /* Index of first STAR leg in the route */
  int getStarLegsOffset() const
  {
    return starLegsOffset;
  }

  /* Create copies of list methods to ease tracking of usage */

  /* Returns empty object if index is invalid */
  const RouteLeg& value(int i) const;

  using QList::isEmpty;
  using QList::append;
  using QList::prepend;
  using QList::insert;
  using QList::replace;
  using QList::move;
  using QList::clear;
  using QList::size;

  int getSizeWithoutAlternates() const;

  /* Removes the shadowed flight plan entry too */
  void removeAllAt(int i)
  {
    QList::removeAt(i);
    flightplan.removeAt(i);
  }

  void removeLegAt(int i)
  {
    QList::removeAt(i);
  }

  /* Removes all entries in route and flightplan except the ones in the range (including) */
  void removeAllExceptRange(int from, int to);

  /* Removes all legs, procedure information and flight plan legs */
  void clearAll();

  /* Removes approaches and SID/STAR depending on save options, deletes duplicates and returns a copy.
   * All procedure legs are converted to normal flight plan (user) legs if requested.
   * Used for flight plan export.
   *
   * Does not adapt RouteAltitude legs. */
  Route adjustedToOptions(rf::RouteAdjustOptions options) const;
  static Route adjustedToOptions(const Route& origRoute, rf::RouteAdjustOptions options);

  /* Copy flight plan profile altitudes into entries for FMS and other formats
   *  All following functions have to use setCoords instead of setPosition to avoid overwriting.
   *
   * This has to be called before adjustedToOptions() since the RouteAltitude legs
   * are not adapted and might have a different size*/
  Route updatedAltitudes() const;
  static Route updatedAltitudes(const Route& routeParam);

  /* As above but sets all altitudes to zero */
  Route zeroedAltitudes() const;
  static Route zeroedAltitudes(const Route& routeParam);

  /* Loads navaids from database and create all route map objects from flight plan.
   * Flight plan will be corrected if needed. */
  void createRouteLegsFromFlightplan();

  /* @return true if departure is valid and departure airport has no parking or departure of flight plan
   *  has parking or helipad as start position */
  bool hasValidParking() const;

  /* Fetch airways by waypoint and name and adjust route altititude if needed */
  /* Uses airway by name cache in query which is called often. */
  void updateAirwaysAndAltitude(bool adjustRouteAltitude);

  /* Apply simplified east/west or north/south rule. Return in local units */
  float getAdjustedAltitude(float altitudeLocal) const;

  /* Get a position along the route. Pos is invalid if not along. distFromStart in nm */
  atools::geo::Pos getPositionAtDistance(float distFromStartNm) const;

  const RouteAltitude& getAltitudeLegs() const
  {
    return *altitude;
  }

  /* Get ILS which are referenced from the recommended fix of the approach procedure for display in the flight plan table. */
  const QVector<map::MapIls>& getDestRunwayIlsFlightPlanTable() const
  {
    return destRunwayIlsFlightPlanTable;
  }

  /* Get a list of matching ILS/LOC which are not too far away from runway (in case of CTL) */
  const QVector<map::MapIls>& getDestRunwayIlsMap() const
  {
    return destRunwayIlsMap;
  }

  /* As above but filtered out for elevation profile only having slope  */
  const QVector<map::MapIls>& getDestRunwayIlsProfile() const
  {
    return destRunwayIlsProfile;
  }

  const map::MapRunwayEnd& getDestRunwayEnd() const
  {
    return destRunwayEnd;
  }

  /* Get ILS (for ILS and LOC approaches) and VASI pitch if approach is available */
  void updateApproachIls();

  const RouteAltitudeLeg& getAltitudeLegAt(int i) const;
  bool hasAltitudeLegs() const;
  int getNumAltitudeLegs() const;
  bool isValidProfile() const;

  /* Calculate route leg altitudes that are needed for the elevation profile */
  void updateLegAltitudes();

  /* general distance in NM which is either cross track, previous or next waypoint */
  float getDistanceToFlightPlan() const;
  bool isTooFarToFlightPlan() const;

  /* SID RAMY6, Approach ILS 12, etc.
   * @param includeRunway Include runway information.
   * @param missedAsApproach Show "Approach" for missed legs instead of "Missed".
   */
  QString getProcedureLegText(proc::MapProcedureTypes mapType, bool includeRunway, bool missedAsApproach, bool transitionAsProcedure) const;

  /* Get texts for deviation and descent angles after passing TOD. Pointers can be nullptr if not required. */
  void getVerticalPathDeviationTexts(QString *descentDeviation, QString *verticalAngle, bool *verticalRequired,
                                     QString *verticalAngleNext) const;

  /* Procedure leg is not valid if en-route leg */
  const proc::MapProcedureLeg& getProcedureLegAt(int i) const
  {
    return getLegAt(i).getProcedureLeg();
  }

  /* Assign index and pointer to flight plan for all objects and also update all procedure and alternate offsets */
  void updateIndicesAndOffsets();

  /* Get idents of all alternates */
  QStringList getAlternateIdents() const;

  /* Get display idents (ICAO, IATA, FAA or local) of all alternates */
  QStringList getAlternateDisplayIdents() const;

  const QVector<map::MapAirport> getAlternateAirports() const;

  /* Get a bit array which indicates high/low airways - needed for some export formats.
   *  True indicates high airway used towards waypoint at the same index. */
  QBitArray getJetAirwayFlags() const;

  /* Update current position only */
  void updateActivePos(const map::PosCourse& pos)
  {
    activePos = pos;
  }

  /* Reload procedures from the database after deleting a transition.
   * This is needed since attached transitions can change procedures. */
  void reloadProcedures(proc::MapProcedureTypes procs);

  /* Copies departure and destination names and positions from Route to Flightplan */
  void updateDepartureAndDestination(bool clearInvalidStart);

  /* Get file name pattern based on route values */
  QString buildDefaultFilename(const QString& suffix) const;
  QString buildDefaultFilenameShort(const QString& separator, const QString& suffix) const;

  /* Uses pattern from options if empty */
  QString buildDefaultFilename(QString pattern, QString suffix) const;

  /* Get all missing (i.e. not loaded) procedures where property values are present
   * but procedure structs are not loaded/resolved */
  proc::MapProcedureTypes getMissingProcedures();

  /* Check if selected rows affect procedures. Used to disable move and other actions */
  void selectionFlagsAlternate(const QList<int>& rows, bool& containsAlternate, bool& moveDownTouchesAlt, bool& moveUpTouchesAlt,
                               bool& moveDownLeavesAlt, bool& moveUpLeavesAlt) const;

  /* Check if selected rows affect alternate airports. Used to disable move and other actions. */
  void selectionFlagsProcedures(const QList<int>& rows, bool& containsProc, bool& moveDownTouchesProc, bool& moveUpTouchesProc) const;

  /* Override current positon and course for aircraft */
  void setActivePos(const atools::geo::Pos& pos, float course);

  /* Get index for ref or -1 if not found. Procedures are stored with flag PROCEDURE in the index. */
  int getLegIndexForRef(const map::MapRef& ref) const;

  int getLegIndexForIdAndType(int id, map::MapTypes type) const
  {
    return getLegIndexForRef(map::MapRef(id, type));
  }

  /* Fill route index for all airports in the result */
  void updateAirportRouteIndex(map::MapResult& result) const;

  /* Clear route index for all flight plan related objects in result */
  void clearAirportRouteIndex(map::MapResult& result) const;

private:
  /* Get a list of approach ILS (not localizer) and the used runway end. Only for approaches. */
  void updateApproachRunwayEndAndIls(QVector<map::MapIls>& ilsVector, map::MapRunwayEnd *runwayEnd,
                                     bool recommended, bool map, bool profile) const;

  /* Copy flight plan profile altitudes into entries for FMS and other formats
   *  All following functions have to use setCoords instead of setPosition to avoid overwriting*/
  void assignAltitudes();
  void zeroAltitudes();

  /* Assign index and pointer to flight plan for all objects */
  void updateIndices();
  void updateAlternateIndicesAndOffsets();

  /* Removes duplicate waypoints when transitioning from route to procedure and vice versa.
   * Used after route calculation. */
  void removeDuplicateRouteLegs();

  /* Corrects the airways at the procedure entry and exit points as well as first leg */
  void validateAirways();

  /* Remove any waypoints which positions overlap with procedures. Requires a flight plan that is cleaned up and contains
   * no procedure legs. CPU intense do not use often. */
  void cleanupFlightPlanForProcedures(map::MapAirway& starAirway);

  /* Removes related properies in the flight plan only */
  void clearFlightplanProcedureProperties(proc::MapProcedureTypes type);

  /* Calculate all distances and courses for route map objects */
  void updateDistancesAndCourse();
  void updateBoundingRect();

  /* Looks fuzzy for a waypoint at the given position from front to end or vice versa if reverse is true */
  int legIndexForPosition(const atools::geo::Pos& pos, bool reverse);

  /* Look for overlap with any of the points in the flight plan. Double loop. Use rarely. */
  int legIndexForPositions(const atools::geo::LineString& line, bool reverse);

  void removeLegs(int from, int to);

  /* Update and calculate magnetic variation for all route map objects */
  void updateMagvar();

  /* Get indexes to nearest approach or route leg and cross track distance to the nearest ofthem in nm */
  void copy(const Route& other);
  void nearestAllLegIndex(const map::PosCourse& pos, float& crossTrackDistanceMeter, int& index) const;
  bool isSmaller(const atools::geo::LineDistance& dist1, const atools::geo::LineDistance& dist2, float epsilon);
  int adjustedActiveLeg() const;

  /* Calculated distance for aircraft projection in profile */
  float projectedDistance(const atools::geo::LineDistance& result, float legFromStart, int legIndex) const;

  /* get offsets for procedure entry/exit points and return true if a valid one was found */
  bool arrivalRouteToProcLegs(int& arrivaLegsOffset) const;
  bool departureProcToRouteLegs(int& startIndexAfterProcedure) const;

  /* Update waypoint numbers with prefix "WP" automatically in order of plan */
  void updateWaypointNames();

  /* Updates airway objects in route legs and returns min and max altitude defined by airways and flight plan restrictions */
  void updateAirways(float& minAltitudeFt, float& maxAltitudeFt, bool adjustRouteAltitude);

  atools::geo::Rect boundingRect;

  /* Nautical miles not including missed approach and alternates */
  float totalDistance = 0.f;

  atools::fs::pln::Flightplan flightplan;
  proc::MapProcedureLegs approachLegs, starLegs, sidLegs;
  map::MapTypes shownTypes = map::NONE;

  int activeLegIndex = map::INVALID_INDEX_VALUE;
  atools::geo::LineDistance activeLegResult;
  map::PosCourse activePos;
  int sidLegsOffset = map::INVALID_INDEX_VALUE, /* First departure leg */
      starLegsOffset = map::INVALID_INDEX_VALUE, /* First STAR leg */
      approachLegsOffset = map::INVALID_INDEX_VALUE, /* First approach leg */
      alternateLegsOffset = map::INVALID_INDEX_VALUE; /* First alternate airport*/
  int numAlternateLegs = 0;

  QVector<map::MapIls>
  /* Get a list of matching ILS which have a slope and are not too far away from runway (in case of CTL).
   * These ones can be used for map display. */
  destRunwayIlsMap,

  /* Get a list of matching ILS which have a slope and are not too far away from runway (in case of CTL).
   * These ones can be used for elevation profile display. */
    destRunwayIlsProfile,
  /* Get ILS which are referenced from the recommended fix of the approach procedure */
    destRunwayIlsFlightPlanTable;

  /* Get runway end at destination if any. Used to get the VASI information */
  map::MapRunwayEnd destRunwayEnd;

  RouteAltitude *altitude = nullptr;

  /* Ref to flight plan leg  index map */
  QHash<map::MapRef, int> objectIndex;
};

QDebug operator<<(QDebug out, const Route& route);

#endif // LITTLENAVMAP_ROUTE_H
