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

#ifndef LITTLENAVMAP_ROUTELEG_H
#define LITTLENAVMAP_ROUTELEG_H

#include "common/proctypes.h"

#include <QApplication>

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
  RouteLeg();
  RouteLeg(atools::fs::pln::Flightplan *parentFlightplan);
  ~RouteLeg();

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

  void createFromProcedureLeg(int entryIndex, const proc::MapProcedureLegs& legs, const RouteLeg *prevLeg);

  /*
   * Updates distance and course to this object if the predecessor is not null. Will reset values otherwise.
   * @param predRouteMapObj
   */
  void updateDistanceAndCourse(int entryIndex, const RouteLeg *prevLeg);

  /* Get magvar from all known objects */
  void updateMagvar();

  /* Set parking and start position. Does not modify the flight plan entry.
   * Parking clears start and vice versa. */
  void setDepartureParking(const map::MapParking& departureParking);
  void setDepartureStart(const map::MapStart& departureStart);

  /* Get database id of airport or navaid. -1 for invalid or user position. */
  int getId() const;

  /* Compares only the navaids (waypoints, NDB and VOR) - this works also with route and approach legs*/
  bool isNavaidEqualTo(const RouteLeg& other) const;

  /* Get position of airport or navaid. Source can be flight plan entry or database. */
  const atools::geo::Pos& getPosition() const;
  float getAltitude() const;

  /* Get ident of airport or navaid. Source can be flight plan entry or database. */
  QString getIdent() const;

  /* Comment section from flight plan entry */
  QString getComment() const;

  QString getRegion() const;

  /* Get name of airport or navaid. Empty for waypoint or user. Source can be flight plan entry or database. */
  QString getName() const;

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
  QString getChannel() const;
  QString getFrequencyOrChannel() const;

  /* Get magnetic variation. Source is always database. Depending on settings
   * either environment or radio navaid declination. */
  float getMagvar() const
  {
    return magvar;
  }

  void setMagvar(float value)
  {
    magvar = value;
  }

  /* Get calculated declination from environment. */
  float getMagvarPos() const
  {
    return magvarPos;
  }

  /* Gets either radio navaid declination or environment value depending on settings */
  float getMagVarBySettings() const;

  /* Get range of radio navaid. -1 if not a radio navaid. Source is always database. */
  int getRange() const;

  map::MapObjectTypes getMapObjectType() const
  {
    return type;
  }

  QString getMapObjectTypeName() const;

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

  /* Get VOR or empty object if not assigned. Use position.isValid to check for empty */
  const map::MapVor& getVor() const
  {
    return vor;
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

  /* Get Waypoint or empty object if not assigned. Use position.isValid to check for empty */
  map::MapRunwayEnd getRunwayEnd() const
  {
    return runwayEnd;
  }

  /* Great circle distance to this route map object from the predecessor in nautical miles or 0 if first in route */
  float getDistanceTo() const
  {
    return distanceTo;
  }

  /* Great circle start magnetic course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseToMag() const;

  /* Great circle start true course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseToTrue() const
  {
    return courseTo;
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
    return type & map::PROCEDURE;
  }

  /* User defined waypoint */
  bool isUser() const;

  float getGroundAltitude() const
  {
    return groundAltitude;
  }

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

  const atools::geo::LineString& getGeometry() const;

  /* true if approach and inital fix or any other point that should be skipped for certain calculations */
  bool isApproachPoint() const;

  int getFlightplanIndex() const
  {
    return index;
  }

  /* true if airway given but not found in database. Also true if one-way direction is violated */
  bool isAirwaySetAndInvalid(float altitudeFt, QStringList *errors = nullptr) const;

  bool isTrack() const
  {
    return airway.isValid() && airway.isTrack();
  }

  bool isAirway() const
  {
    return airway.isValid() && airway.isAirway();
  }

  void clearAirwayOrTrack();

  const atools::fs::pln::FlightplanEntry& getFlightplanEntry() const;

private:
  // TODO assign functions are duplicatd in FlightplanEntryBuilder
  void assignIntersection(const map::MapSearchResult& mapobjectResult,
                          atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignVor(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignNdb(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignAnyNavaid(atools::fs::pln::FlightplanEntry *flightplanEntry, const atools::geo::Pos& last,
                       float maxDistance);
  void assignRunwayOrHelipad(const QString& name);
  void assignAirport(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry);
  void assignUser(atools::fs::pln::FlightplanEntry *flightplanEntry);

  /* Parent flight plan */
  atools::fs::pln::Flightplan *flightplan = nullptr;
  /* Associated flight plan entry or approach leg entry */
  int index = -1;

  map::MapObjectTypes type = map::NONE;
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

  float distanceTo = 0.f,
        courseTo = 0.f,
        groundAltitude = 0.f,
        magvar = 0.f, /* Either taken from navaid or average across the route */
        magvarPos = 0.f; /* Calculate environment value */
  atools::geo::LineString geometry;

};

QDebug operator<<(QDebug out, const RouteLeg& leg);

#endif // LITTLENAVMAP_ROUTELEG_H
