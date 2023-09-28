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

#ifndef LITTLENAVMAP_ROUTELEG_H
#define LITTLENAVMAP_ROUTELEG_H

#include "common/proctypes.h"

#include <QCoreApplication>

namespace atools {
namespace fs {
namespace pln {
class FlightplanEntry;
class Flightplan;
}
}
}

class MapQuery;
class Route;

/*
 * A flight plan waypoint, departure or destination. Data is loaded from the database. Provides
 * methods to get route table information, like distance, course and more.
 */
class RouteLeg
{
  Q_DECLARE_TR_FUNCTIONS(RouteLeg)

public:
  RouteLeg()
  {

  }

  explicit RouteLeg(atools::fs::pln::Flightplan *parentFlightplan);

  /*
   * Creates a route map object from a flight plan entry. Queries the database for existing navaids and airports.
   * If the flight plan entry is incomplete it will be updated. If no database object can be found
   * the flight plan entry will remain unchanged. The flight plan entry will be assigned to this object.
   *
   * @param planEntry Flight plan entry used to query database objects. Valid data is written back to the entry.
   * @param query Database query object
   * @param predRouteMapObj Predecessor of this entry or null if this is the first waypoint in the list
   */
  void createFromDatabaseByEntry(int entryIndex, const RouteLeg *prevLeg);

  /*
   * Creates a route map object from an airport database object.
   * @param planEntry Complete flight plan entry. Assigned to this object but not updated.
   * @param newAirport
   * @param predRouteMapObj Predecessor of this entry or null if this is the first waypoint in the list
   */
  void createFromAirport(int entryIndex, const map::MapAirport& newAirport, const RouteLeg *prevLeg);

  /* Create dummy procedure legs and copy all waypoint, etc. information into the waypoint, vor and other structs */
  void createFromProcedureLeg(int entryIndex, const proc::MapProcedureLeg& leg, const RouteLeg *prevLeg);
  void createFromProcedureLegs(int entryIndex, const proc::MapProcedureLegs& legs, const RouteLeg *prevLeg);

  /* Creates a clone of waypoint type but does not assign type PROCEDURE */
  void createCopyFromProcedureLeg(int entryIndex, const RouteLeg& leg, const RouteLeg *prevLeg);

  /* Updates distance and course to this object if the predecessor is not null. Will reset values otherwise. */
  void updateDistanceAndCourse(int entryIndex, const RouteLeg *prevLeg);

  /* Get magvar from all known objects */
  void updateMagvar(const RouteLeg *prevLeg);

  /* Set parking and start position. Does not modify the flight plan entry.
   * Parking clears start and vice versa. */
  void setDepartureParking(const map::MapParking& departureParking);
  void setDepartureStart(const map::MapStart& departureStart);

  /* Set departure parking and start to invalid values which means airport is start position */
  void clearParkingAndStart();

  /* Get database id of airport or navaid. -1 for invalid or user position. */
  int getId() const;

  /* Compares only the navaids (waypoints, NDB and VOR) - this works also with route and approach legs*/
  bool isNavaidEqualTo(const RouteLeg& other) const;

  /* Get position of airport or navaid. Source can be flight plan entry or database. */
  atools::geo::Pos getPosition() const;
  float getAltitude() const;

  /* Get the real position of the procedure fix instead of the endpoint. Otherwise like getPosition() */
  const atools::geo::Pos getFixPosition() const;
  const atools::geo::Pos& getRecommendedFixPosition() const;

  /* Get ident of airport or navaid. Source can be flight plan entry or database. */
  QString getIdent() const;

  /* Ident, ICAO, IATA or local for airport. Otherwise same as above. */
  QString getDisplayIdent(bool useIata = true) const;

  /* Comment section from flight plan entry */
  const QString& getComment() const;

  const QString& getRegion() const;

  /* Get name of airport or navaid. Empty for waypoint or user. Source can be flight plan entry or database. */
  const QString& getName() const;

  /* Get airway  name from loaded flight plan. */
  const QString& getAirwayName() const;

  const map::MapAirway& getAirway() const
  {
    return airway;
  }

  void setAirway(const map::MapAirway& value)
  {
    airway = value;
  }

  /* Get frequency of radio navaid. 0 if not a radio navaid. Source is always database. */
  int getFrequency() const;
  const QString& getChannel() const;
  QString getFrequencyOrChannel() const;

  /* Get magnetic variation at leg start. Source is always database. Does NOT use VOR declination. */
  float getMagvarStart() const
  {
    return magvarStart;
  }

  /* Get magnetic variation at leg end (waypoint). Source is always database. Does NOT use VOR declination. */
  float getMagvarEnd() const
  {
    return magvarEnd;
  }

  /* Get range of radio navaid. -1 if not a radio navaid. Source is always database. */
  int getRange() const;

  map::MapTypes getMapType() const
  {
    return type;
  }

  /* Type like Airport, NDB (MH) or VOR (H) */
  QString getMapTypeName() const;

  /* Type like Airport, NDB or VOR */
  QString getMapTypeNameShort() const;

  /* Display text usable for menu items */
  QString getDisplayText(int elideName = 100) const;

  /* Get airport or empty airport object if not an airport. Use position.isValid to check for empty */
  const map::MapAirport& getAirport() const
  {
    return airport;
  }

  map::MapAirport& getAirport()
  {
    return airport;
  }

  /* Get parking or empty parking object if parking is not assigned. Use position.isValid to check for empty */
  const map::MapParking& getDepartureParking() const
  {
    return parking;
  }

  /* Get start position or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapStart& getDepartureStart() const
  {
    return start;
  }

  /* Return start position of gate/parking/runway otherwise invalid */
  const atools::geo::Pos& getDeparturePosition() const;

  /* Get VOR or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapVor& getVor() const
  {
    return vor;
  }

  /* true if VOR and leg are valid and this is valid and a real VOR with calibration (VOR, VORDME or VORTAC) */
  bool isCalibratedVor() const
  {
    return isValid() && vor.isCalibratedVor();
  }

  /* Get NDB or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapNdb& getNdb() const
  {
    return ndb;
  }

  /* Get Waypoint or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapWaypoint& getWaypoint() const
  {
    return waypoint;
  }

  /* Build object for userpoint. id is routeindex */
  map::MapUserpointRoute getUserpointRoute() const;

  /* Get Waypoint or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapRunwayEnd& getRunwayEnd() const
  {
    return runwayEnd;
  }

  /* Great circle distance to this route map object from the predecessor in nautical miles or 0 if first in route */
  float getDistanceTo() const
  {
    return distanceTo;
  }

  /* Great circle magnetic course at the start of this leg using start declination.
   * INVALID_COURSE_VALUE if true course is not valid. */
  float getCourseStartMag() const;

  /* Great circle magnetic course at the end of this leg (at waypoint) using start declination.
   * INVALID_COURSE_VALUE if true course is not valid. */
  float getCourseEndMag() const;

  /* Great circle start true course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseStartTrue() const
  {
    return courseStartTrue;
  }

  /* Great circle end (at waypoint) true course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseEndTrue() const
  {
    return courseEndTrue;
  }

  /* @return false if this waypoint was not found in the database */
  bool isValidWaypoint() const
  {
    return validWaypoint;
  }

  /* @return false if this is default constructed */
  bool isValid() const
  {
    return valid;
  }

  /* true if nav database otherwise simulator */
  bool isNavdata() const;

  /* Route leg - not part of procedure and not alternate */
  bool isRoute() const
  {
    return !isAnyProcedure() && !isAlternate();
  }

  /* Alterante airport */
  bool isAlternate() const
  {
    return alternate;
  }

  void setAlternate(bool value = true)
  {
    alternate = value;
  }

  /* SID, STAR or approach */
  bool isAnyProcedure() const
  {
    return type.testFlag(map::PROCEDURE);
  }

  /* User defined waypoint */
  bool isUser() const;

  void setFlightplan(atools::fs::pln::Flightplan *fp)
  {
    flightplan = fp;
  }

  void setFlightplanEntryIndex(int value)
  {
    index = value;
  }

  const proc::MapProcedureLeg& getProcedureLeg() const
  {
    return procedureLeg;
  }

  const proc::MapAltRestriction& getProcedureLegAltRestr() const
  {
    return procedureLeg.altRestriction;
  }

  const proc::MapSpeedRestriction& getProcedureLegSpeedRestr() const
  {
    return procedureLeg.speedRestriction;
  }

  /* invalid type if not an approach */
  proc::ProcedureLegType getProcedureLegType() const
  {
    return procedureLeg.type;
  }

  /* invalid type if not an approach */
  proc::MapProcedureTypes getProcedureType() const
  {
    return procedureLeg.mapType;
  }

  /* Do not display distance e.g. for course to altitude */
  bool noDistanceDisplay() const
  {
    return procedureLeg.isValid() && procedureLeg.noDistanceDisplay();
  }

  /* No course display for e.g. arc legs */
  bool noCalcCourseDisplay() const
  {
    return procedureLeg.isValid() && procedureLeg.noCalcCourseDisplay();
  }

  /* Get start course from leg or procedure calculated course if applicable.
   * Procedure is true if the values were taken from a procedure. */
  void getMagTrueRealCourse(float& courseMag, float& courseTrue, bool *procedure = nullptr) const;

  /* No ident at end of manual legs */
  bool noIdentDisplay() const
  {
    return procedureLeg.isValid() && procedureLeg.noIdentDisplay();
  }

  const atools::geo::LineString& getGeometry() const
  {
    return geometry;
  }

  /* true if approach and inital fix or any other point that should be skipped for certain calculations */
  bool isApproachPoint() const;

  int getFlightplanIndex() const
  {
    return index;
  }

  /* true if airway given but not found in database. Also true if one-way direction is violated */
  bool isAirwaySetAndInvalid(float altitudeFt, QStringList *errors = nullptr, bool *trackError = nullptr) const;

  bool isTrack() const
  {
    return airway.isValid() && airway.isTrack();
  }

  bool isAirway() const
  {
    return airway.isValid() && airway.isAirway();
  }

  bool isAirport() const
  {
    return airport.isValid();
  }

  void clearAirwayOrTrack();

  const atools::fs::pln::FlightplanEntry& getFlightplanEntry() const;
  atools::fs::pln::FlightplanEntry *getFlightplanEntry();

  /* Build leg labels also depending on procedure flags. Uses start course and normal declination (not VOR) */
  QStringList buildLegText(bool dist, bool magCourseFlag, bool trueCourseFlag, bool narrow) const;
  static QStringList buildLegText(float distance, float courseMag, float courseTrue, bool narrow);

  /* Course to waypoint at end of leg. Either uses geometry course or saved course if geometry is not valid */
  float getGeometryEndCourse() const;

  /* Course from waypoint at previous leg or at the beginning of this leg.
   * Either uses geometry course of this or saved course if geometry is not valid */
  float getGeometryStartCourse() const;

private:
  friend QDebug operator<<(QDebug out, const RouteLeg& leg);

  // TODO assign functions are duplicated in FlightplanEntryBuilder
  void assignIntersection(const map::MapResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignVor(const map::MapResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignNdb(const map::MapResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignAnyNavaid(atools::fs::pln::FlightplanEntry *flightplanEntry, const atools::geo::Pos& last, float maxDistance);
  void assignRunwayOrHelipad(const QString& name);
  void assignAirport(const map::MapResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignUser(atools::fs::pln::FlightplanEntry *flightplanEntry);

  /* Update altitude in flight plan for waypoints, user wapoints and VOR and NDB having no altitude. */
  void updateDepartAndDestAltitude(atools::fs::pln::FlightplanEntry *flightplanEntry);

  /* Parent flight plan */
  atools::fs::pln::Flightplan *flightplan = nullptr;
  /* Associated flight plan entry or approach leg entry */
  int index = -1;

  map::MapTypes type = map::NONE;
  map::MapAirport airport;
  map::MapParking parking;
  map::MapStart start;
  map::MapVor vor;
  map::MapNdb ndb;
  map::MapIls ils;
  map::MapRunwayEnd runwayEnd;
  map::MapWaypoint waypoint;
  proc::MapProcedureLeg procedureLeg;
  map::MapAirway airway;

  bool validWaypoint = false, alternate = false, valid = false;

  float distanceTo = 0.f, courseStartTrue = 0, courseEndTrue = 0.f, magvarStart = 0.f, magvarEnd = 0.f;
  atools::geo::LineString geometry;
};

QDebug operator<<(QDebug out, const RouteLeg& leg);

#endif // LITTLENAVMAP_ROUTELEG_H
