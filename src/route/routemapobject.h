/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_ROUTEMAPOBJECT_H
#define LITTLENAVMAP_ROUTEMAPOBJECT_H

#include "common/maptypes.h"

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
class RouteMapObjectList;

/*
 * A flight plan waypoint, departure or destination. Data is loaded from the database. Provides
 * methods to get route table information, like distance, course and more.
 */
class RouteMapObject
{
  Q_DECLARE_TR_FUNCTIONS(RouteMapObject)

public:
  RouteMapObject(atools::fs::pln::Flightplan *parentFlightplan);
  ~RouteMapObject();

  /*
   * Creates a route map object from a flight plan entry. Queries the database for existing navaids and airports.
   * If the flight plan entry is incomplete it will be updated. If no database object can be found
   * the flight plan entry will remain unchanged. The flight plan entry will be assigned to this object.
   *
   * @param planEntry Flight plan entry used to query database objects. Valid data is written back to the entry.
   * @param query Database query object
   * @param predRouteMapObj Predecessor of this entry or null if this is the first waypoint in the list
   */
  void createFromDatabaseByEntry(int entryIndex, MapQuery *query,
                                 const RouteMapObject *predRouteMapObj, const RouteMapObjectList *routeList);

  /*
   * Creates a route map object from an airport database object.
   * @param planEntry Complete flight plan entry. Assigned to this object but not updated.
   * @param newAirport
   * @param predRouteMapObj Predecessor of this entry or null if this is the first waypoint in the list
   */
  void createFromAirport(int entryIndex, const maptypes::MapAirport& newAirport,
                         const RouteMapObject *predRouteMapObj, const RouteMapObjectList *routeList);

  void createFromApproachLeg(int entryIndex, const maptypes::MapApproachLegs& legs,
                             const RouteMapObject *predRouteMapObj, const RouteMapObjectList *routeList);

  /*
   * Updates distance and course to this object if the predecessor is not null. Will reset values otherwise.
   * @param predRouteMapObj
   */
  void updateDistanceAndCourse(int entryIndex, const RouteMapObject *predRouteMapObj,
                               const RouteMapObjectList *routeList);

  /* Change user defined waypoint name */
  void updateUserName(const QString& name);

  /* Set parking and start position. Does not modify the flight plan entry. */
  void setDepartureParking(const maptypes::MapParking& departureParking);
  void setDepartureStart(const maptypes::MapStart& departureStart);

  /* Get database id of airport or navaid. -1 for invalid or user position. */
  int getId() const;

  /* Get position of airport or navaid. Source can be flight plan entry or database. */
  const atools::geo::Pos& getPosition() const;

  /* Get ident of airport or navaid. Source can be flight plan entry or database. */
  QString getIdent() const;

  QString getRegion() const;

  /* Get name of airport or navaid. Empty for waypoint or user. Source can be flight plan entry or database. */
  QString getName() const;

  /* Get airway  name. */
  const QString& getAirway() const;

  /* Get frequency of radio navaid. 0 if not a radio navaid. Source is always database. */
  int getFrequency() const;

  /* Get magnetic variation. Source is always database. */
  float getMagvar() const;

  /* Get range of radio navaid. -1 if not a radio navaid. Source is always database. */
  int getRange() const;

  maptypes::MapObjectTypes getMapObjectType() const
  {
    return type;
  }

  QString getMapObjectTypeName() const;

  /* Get airport or empty airport object if not an airport. Use position.isValid to check for empty */
  const maptypes::MapAirport& getAirport() const
  {
    return airport;
  }

  /* Get parking or empty parking object if parking is not assigned. Use position.isValid to check for empty */
  const maptypes::MapParking& getDepartureParking() const
  {
    return parking;
  }

  /* Get start position or empty object if not assigned. Use position.isValid to check for empty */
  const maptypes::MapStart& getDepartureStart() const
  {
    return start;
  }

  /* Get VOR or empty object if not assigned. Use position.isValid to check for empty */
  const maptypes::MapVor& getVor() const
  {
    return vor;
  }

  /* Get NDB or empty object if not assigned. Use position.isValid to check for empty */
  const maptypes::MapNdb& getNdb() const
  {
    return ndb;
  }

  /* Get Waypoint or empty object if not assigned. Use position.isValid to check for empty */
  const maptypes::MapWaypoint& getWaypoint() const
  {
    return waypoint;
  }

  bool hasPredecessor() const
  {
    return predecessor;
  }

  /* Great circle distance to this route map object from the predecessor in nautical miles or 0 if first in route */
  float getDistanceTo() const
  {
    return distanceTo;
  }

  /* Rhumb line distance to this route map object from the predecessor in nautical miles or 0 if first in route */
  float getDistanceToRhumb() const
  {
    return distanceToRhumb;
  }

  /* Great circle start course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseTo() const
  {
    return courseTo;
  }

  /* Rhumb line course to this route map object from the predecessor in degrees or 0 if first in route */
  float getCourseToRhumb() const
  {
    return courseRhumbTo;
  }

  /* @return false if this waypoint was not found in the database */
  bool isValid() const
  {
    return valid;
  }

  bool isRoute() const;
  bool isAnyApproach() const;

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

  const maptypes::MapApproachLeg& getApproachLeg() const
  {
    return approachLeg;
  }

  bool isApproach() const;
  bool isMissed() const;
  bool isTransition() const;

private:
  const atools::fs::pln::FlightplanEntry& curEntry() const;

  /* Parent flight plan */
  atools::fs::pln::Flightplan *flightplan = nullptr;
  /* Associated flight plan entry or approach leg entry */
  int index = -1;

  maptypes::MapObjectTypes type = maptypes::NONE;
  maptypes::MapAirport airport;
  maptypes::MapParking parking;
  maptypes::MapStart start;
  maptypes::MapVor vor;
  maptypes::MapNdb ndb;
  maptypes::MapWaypoint waypoint;
  maptypes::MapApproachLeg approachLeg;

  bool predecessor = false;
  bool valid = false;

  float distanceTo = 0.f,
        distanceToRhumb = 0.f,
        courseTo = 0.f,
        courseRhumbTo = 0.f,
        groundAltitude = 0.f;

};

#endif // LITTLENAVMAP_ROUTEMAPOBJECT_H
