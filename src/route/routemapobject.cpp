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

#include "route/routemapobject.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "fs/pln/flightplan.h"
#include "atools.h"
#include "route/routemapobjectlist.h"

#include <QRegularExpression>

using namespace atools::geo;

/* Extract parking name and number from FS flight plan */
const QRegularExpression PARKING_TO_NAME_AND_NUM("([A-Za-z_ ]*)([0-9]+)");

/* If region is not set search within this distance (not the read GC distance) for navaids with the same name */
const int MAX_WAYPOINT_DISTANCE_METER = 10000.f;

const static QString EMPTY_STRING;
const static atools::fs::pln::FlightplanEntry EMPTY_FLIGHTPLAN_ENTRY;

RouteMapObject::RouteMapObject(atools::fs::pln::Flightplan *parentFlightplan)
  : flightplan(parentFlightplan)
{

}

RouteMapObject::~RouteMapObject()
{

}

/* Find the nearest navaid object to position pos */
template<typename TYPE>
TYPE findMapObject(const QList<TYPE>& waypoints, const atools::geo::Pos& pos, bool& found)
{
  if(!waypoints.isEmpty())
  {
    TYPE nearest = waypoints.first();
    if(waypoints.size() > 1)
    {
      float distance = maptypes::INVALID_DISTANCE_VALUE;
      for(const TYPE& obj : waypoints)
      {
        float d = pos.distanceSimpleTo(obj.position);
        if(d < distance)
        {
          distance = d;
          nearest = obj;
        }
      }
    }
    found = true;
    return nearest;
  }
  found = false;
  return TYPE();
}

void RouteMapObject::createFromAirport(int entryIndex,
                                       const maptypes::MapAirport& newAirport,
                                       const RouteMapObject *predRouteMapObj,
                                       const RouteMapObjectList *routeList)
{
  index = entryIndex;
  predecessor = predRouteMapObj != nullptr;
  type = maptypes::AIRPORT;
  airport = newAirport;

  updateDistanceAndCourse(entryIndex, predRouteMapObj, routeList);
  valid = true;
}

void RouteMapObject::createFromApproachLeg(int entryIndex, const maptypes::MapApproachLegs& legs,
                                           const RouteMapObject *predRouteMapObj,
                                           const RouteMapObjectList *routeList)
{
  index = entryIndex;
  predecessor = predRouteMapObj != nullptr;
  approachLeg = legs.at(entryIndex);

  if(approachLeg.transition)
    type = maptypes::APPROACH_TRANSITION;
  else if(approachLeg.missed)
    type = maptypes::APPROACH_MISSED;
  else
    type = maptypes::APPROACH;

  updateDistanceAndCourse(entryIndex, predRouteMapObj, routeList);
  valid = true;
}

void RouteMapObject::createFromDatabaseByEntry(int entryIndex, MapQuery *query,
                                               const RouteMapObject *predRouteMapObj,
                                               const RouteMapObjectList *routeList)
{
  index = entryIndex;

  predecessor = predRouteMapObj != nullptr;

  atools::fs::pln::FlightplanEntry *flightplanEntry = &(*flightplan)[index];

  QString region = flightplanEntry->getIcaoRegion();

  if(region == "KK") // Invalid route finder region
    region.clear();

  bool found;
  maptypes::MapSearchResult mapobjectResult;
  switch(flightplanEntry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      break;
    case atools::fs::pln::entry::AIRPORT:
      query->getMapObjectByIdent(mapobjectResult, maptypes::AIRPORT, flightplanEntry->getIcaoIdent());
      if(!mapobjectResult.airports.isEmpty())
      {
        type = maptypes::AIRPORT;
        airport = mapobjectResult.airports.first();
        valid = true;

        QString name = flightplan->getDepartureParkingName().trimmed();
        if(!name.isEmpty() && predRouteMapObj == nullptr)
        {
          if(!name.isEmpty())
          {
            // Resolve parking if first airport
            QRegularExpressionMatch match = PARKING_TO_NAME_AND_NUM.match(name);

            // Convert parking name to the format used in the database
            QString parkingName = match.captured(1).trimmed().toUpper().replace(" ", "_");

            if(!parkingName.isEmpty())
            {
              // Seems to be a parking position
              int number = QString(match.captured(2)).toInt();
              QList<maptypes::MapParking> parkings;
              query->getParkingByNameAndNumber(parkings, airport.id,
                                               maptypes::parkingDatabaseName(parkingName), number);

              if(parkings.isEmpty())
              {
                qWarning() << "Found no parking spots";
                flightplan->setDepartureParkingName(QString());
              }
              else
              {
                if(parkings.size() > 1)
                  qWarning() << "Found multiple parking spots";

                parking = parkings.first();
                // Update flightplan with found name
                flightplan->setDepartureParkingName(maptypes::parkingNameForFlightplan(parking));
              }
            }
            else
            {
              // Runway or helipad
              query->getStartByNameAndPos(start, airport.id, name, flightplan->getDeparturePosition());

              if(!start.isValid())
              {
                qWarning() << "Found no start positions";
                // Clear departure position in flight plan
                flightplan->setDepartureParkingName(QString());
              }
              else
              {
                if(start.helipadNumber > 0)
                  // Helicopter pad
                  flightplan->setDepartureParkingName(QString::number(start.helipadNumber));
                else
                  // Runway name
                  flightplan->setDepartureParkingName(start.runwayName);
              }
            }
          }
        }
        else
        {
          // Airport is not departure reset start and parking
          start = maptypes::MapStart();
          parking = maptypes::MapParking();
        }
      }
      break;
    case atools::fs::pln::entry::INTERSECTION:
      {
        // Navaid waypoint
        query->getMapObjectByIdent(mapobjectResult, maptypes::WAYPOINT,
                                   flightplanEntry->getIcaoIdent(), region);
        const maptypes::MapWaypoint& obj = findMapObject(mapobjectResult.waypoints,
                                                         flightplanEntry->getPosition(), found);
        if(found)
        {
          type = maptypes::WAYPOINT;
          waypoint = obj;
          valid = waypoint.position.distanceMeterTo(flightplanEntry->getPosition()) <
                  MAX_WAYPOINT_DISTANCE_METER;
          if(valid)
          {
            // Update all fields in entry if found - otherwise leave as is
            flightplanEntry->setIcaoRegion(waypoint.region);
            flightplanEntry->setIcaoIdent(waypoint.ident);
            flightplanEntry->setPosition(waypoint.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::VOR:
      {
        query->getMapObjectByIdent(mapobjectResult, maptypes::VOR, flightplanEntry->getIcaoIdent(), region);
        const maptypes::MapVor& obj = findMapObject(mapobjectResult.vors,
                                                    flightplanEntry->getPosition(), found);
        if(found)
        {
          type = maptypes::VOR;
          vor = obj;
          valid = vor.position.distanceMeterTo(flightplanEntry->getPosition()) < MAX_WAYPOINT_DISTANCE_METER;
          if(valid)
          {
            // Update all fields in entry if found - otherwise leave as is
            flightplanEntry->setIcaoRegion(vor.region);
            flightplanEntry->setIcaoIdent(vor.ident);
            flightplanEntry->setPosition(vor.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::NDB:
      {
        query->getMapObjectByIdent(mapobjectResult, maptypes::NDB, flightplanEntry->getIcaoIdent(), region);
        const maptypes::MapNdb& obj = findMapObject(mapobjectResult.ndbs,
                                                    flightplanEntry->getPosition(), found);
        if(found)
        {
          type = maptypes::NDB;
          ndb = obj;
          valid = ndb.position.distanceMeterTo(flightplanEntry->getPosition()) < MAX_WAYPOINT_DISTANCE_METER;
          if(valid)
          {
            // Update all fields in entry if found - otherwise leave as is
            flightplanEntry->setIcaoRegion(ndb.region);
            flightplanEntry->setIcaoIdent(ndb.ident);
            flightplanEntry->setPosition(ndb.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::USER:
      valid = true;
      type = maptypes::USER;
      flightplanEntry->setIcaoIdent(QString());
      flightplanEntry->setIcaoRegion(QString());
      // flightplanEntry->setWaypointId(userName);
      break;
  }

  if(!valid)
    type = maptypes::INVALID;

  updateDistanceAndCourse(entryIndex, predRouteMapObj, routeList);
}

void RouteMapObject::setDepartureParking(const maptypes::MapParking& departureParking)
{
  parking = departureParking;
  start = maptypes::MapStart();
}

void RouteMapObject::setDepartureStart(const maptypes::MapStart& departureStart)
{
  start = departureStart;
  parking = maptypes::MapParking();
}

void RouteMapObject::updateDistanceAndCourse(int entryIndex, const RouteMapObject *predRouteMapObj,
                                             const RouteMapObjectList *routeList)
{
  index = entryIndex;

  if(predRouteMapObj != nullptr)
  {
    if(isAnyApproach())
    {
      distanceTo = approachLeg.calculatedDistance;
      distanceToRhumb = approachLeg.calculatedDistance;
      courseTo = normalizeCourse(approachLeg.calculatedTrueCourse - approachLeg.magvar);
      courseRhumbTo = normalizeCourse(approachLeg.calculatedTrueCourse - approachLeg.magvar);
    }
    else
    {
      const Pos& prevPos = predRouteMapObj->getPosition();

      distanceTo = meterToNm(getPosition().distanceMeterTo(prevPos));
      distanceToRhumb = meterToNm(getPosition().distanceMeterToRhumb(prevPos));

      float magvar = getMagvar();
      if(magvar == 0.f)
      {
        // Get magnetic variance from one of the next and previous waypoints if not set
        float magvarnext = 0.f, magvarprev = 0.f;
        for(int i = std::min(entryIndex, routeList->size() - 1); i >= 0; i--)
        {
          if(atools::almostNotEqual(routeList->at(i).getMagvar(), 0.f))
          {
            magvarnext = routeList->at(i).getMagvar();
            break;
          }
        }

        for(int i = std::min(entryIndex, routeList->size() - 1); i < routeList->size(); i++)
        {
          if(atools::almostNotEqual(routeList->at(i).getMagvar(), 0.f))
          {
            magvarprev = routeList->at(i).getMagvar();
            break;
          }
        }

        // Use average of previous and next or one valid value
        if(std::abs(magvarnext) > 0.f && std::abs(magvarprev) > 0.f)
          magvar = (magvarnext + magvarprev) / 2.f;
        else if(std::abs(magvarnext) > 0.f)
          magvar = magvarnext;
        else if(std::abs(magvarprev) > 0.f)
          magvar = magvarprev;
      }

      courseTo = normalizeCourse(predRouteMapObj->getPosition().angleDegTo(getPosition()) - magvar);
      courseRhumbTo = normalizeCourse(predRouteMapObj->getPosition().angleDegToRhumb(getPosition()) - magvar);
    }
  }
  else
  {
    // No predecessor - this one is the first in the list
    predecessor = false;
    distanceTo = 0.f;
    distanceToRhumb = 0.f;
    courseTo = 0.f;
    courseRhumbTo = 0.f;
  }
}

void RouteMapObject::updateUserName(const QString& name)
{
  flightplan->getEntries()[index].setWaypointId(name);
}

int RouteMapObject::getId() const
{
  if(type == maptypes::INVALID)
    return -1;

  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().id;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().id;
    else if(!approachLeg.navaids.waypoints.isEmpty())
      return approachLeg.navaids.waypoints.first().id;
    else if(!approachLeg.navaids.ils.isEmpty())
      return approachLeg.navaids.ils.first().id;
  }
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
        return -1;

      case atools::fs::pln::entry::AIRPORT:
        return airport.id;

      case atools::fs::pln::entry::INTERSECTION:
        return waypoint.id;

      case atools::fs::pln::entry::VOR:
        return vor.id;

      case atools::fs::pln::entry::NDB:
        return ndb.id;

      case atools::fs::pln::entry::USER:
        return -1;
    }
  }
  return -1;
}

float RouteMapObject::getMagvar() const
{
  if(type == maptypes::INVALID)
    return -1;

  if(isAnyApproach())
    return approachLeg.magvar;
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
        return 0.f;

      case atools::fs::pln::entry::AIRPORT:
        return airport.magvar;

      case atools::fs::pln::entry::INTERSECTION:
        return waypoint.magvar;

      case atools::fs::pln::entry::VOR:
        return vor.magvar;

      case atools::fs::pln::entry::NDB:
        return ndb.magvar;

      case atools::fs::pln::entry::USER:
        return 0.f;
    }
  }
  return 0.f;
}

int RouteMapObject::getRange() const
{
  if(type == maptypes::INVALID)
    return -1;

  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().range;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().range;
    else if(!approachLeg.navaids.ils.isEmpty())
      return approachLeg.navaids.ils.first().range;
  }
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
      case atools::fs::pln::entry::AIRPORT:
      case atools::fs::pln::entry::INTERSECTION:
      case atools::fs::pln::entry::USER:
        return -1;

      case atools::fs::pln::entry::VOR:
        return vor.range;

      case atools::fs::pln::entry::NDB:
        return ndb.range;
    }
  }
  return -1;
}

QString RouteMapObject::getMapObjectTypeName() const
{
  if(type == maptypes::INVALID)
    return tr("Invalid");

  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return maptypes::vorType(approachLeg.navaids.vors.first()) +
             " (" + maptypes::navTypeNameVor(approachLeg.navaids.vors.first().type) + ")";
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return tr("NDB (%1)").arg(maptypes::navTypeNameNdb(approachLeg.navaids.ndbs.first().type));
    else if(!approachLeg.navaids.waypoints.isEmpty())
      return tr("Waypoint");
    else if(!approachLeg.navaids.runwayEnds.isEmpty())
      return tr("Runway");
  }
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
        return tr("Unknown");

      case atools::fs::pln::entry::AIRPORT:
        return tr("Airport");

      case atools::fs::pln::entry::INTERSECTION:
        return tr("Waypoint");

      case atools::fs::pln::entry::USER:
        return EMPTY_STRING;

      case atools::fs::pln::entry::VOR:
        return maptypes::vorType(vor) + " (" + maptypes::navTypeNameVor(vor.type) + ")";

      case atools::fs::pln::entry::NDB:
        return tr("NDB (%1)").arg(maptypes::navTypeNameNdb(ndb.type));
    }
  }
  return EMPTY_STRING;
}

const atools::geo::Pos& RouteMapObject::getPosition() const
{
  if(isAnyApproach())
    return approachLeg.line.getPos2();
  else
  {
    if(type == maptypes::INVALID)
    {
      if(curEntry().getPosition().isValid())
        return curEntry().getPosition();
      else
        return atools::geo::EMPTY_POS;
    }

    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
        return atools::geo::EMPTY_POS;

      case atools::fs::pln::entry::AIRPORT:
        return airport.position;

      case atools::fs::pln::entry::INTERSECTION:
        return waypoint.position;

      case atools::fs::pln::entry::VOR:
        return vor.position;

      case atools::fs::pln::entry::NDB:
        return ndb.position;

      case atools::fs::pln::entry::USER:
        return curEntry().getPosition();
    }
  }
  return atools::geo::EMPTY_POS;
}

QString RouteMapObject::getIdent() const
{
  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().ident;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().ident;
    else if(!approachLeg.navaids.waypoints.isEmpty())
      return approachLeg.navaids.waypoints.first().ident;
    else if(!approachLeg.navaids.runwayEnds.isEmpty())
      return approachLeg.navaids.runwayEnds.first().name;
    else if(!approachLeg.navaids.ils.isEmpty())
      return approachLeg.navaids.ils.first().ident;
  }
  else
  {
    if(type == maptypes::INVALID)
      return curEntry().getIcaoIdent();

    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
        return tr("Unknown Waypoint Type");

      case atools::fs::pln::entry::USER:
        return curEntry().getWaypointId();

      case atools::fs::pln::entry::AIRPORT:
        return airport.ident;

      case atools::fs::pln::entry::INTERSECTION:
        return waypoint.ident;

      case atools::fs::pln::entry::VOR:
        return vor.ident;

      case atools::fs::pln::entry::NDB:
        return ndb.ident;
    }
  }  return EMPTY_STRING;
}

QString RouteMapObject::getRegion() const
{
  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().region;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().region;
    else if(!approachLeg.navaids.waypoints.isEmpty())
      return approachLeg.navaids.waypoints.first().region;
  }
  else
  {
    if(type == maptypes::INVALID)
      return curEntry().getIcaoRegion();

    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
      case atools::fs::pln::entry::USER:
        return QString();

      case atools::fs::pln::entry::AIRPORT:
        return curEntry().getIcaoRegion();

      case atools::fs::pln::entry::INTERSECTION:
        return waypoint.region;

      case atools::fs::pln::entry::VOR:
        return vor.region;

      case atools::fs::pln::entry::NDB:
        return ndb.region;
    }
  }
  return EMPTY_STRING;
}

QString RouteMapObject::getName() const
{
  if(type == maptypes::INVALID)
    return EMPTY_STRING;

  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().name;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().name;
    else if(!approachLeg.navaids.ils.isEmpty())
      return approachLeg.navaids.ils.first().name;
  }
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
      case atools::fs::pln::entry::USER:
      case atools::fs::pln::entry::INTERSECTION:
        return EMPTY_STRING;

      case atools::fs::pln::entry::AIRPORT:
        return airport.name;

      case atools::fs::pln::entry::VOR:
        return atools::capString(vor.name);

      case atools::fs::pln::entry::NDB:
        return atools::capString(ndb.name);
    }
  }
  return EMPTY_STRING;
}

const QString& RouteMapObject::getAirway() const
{
  if(isRoute())
    return curEntry().getAirway();
  else
    return EMPTY_STRING;
}

int RouteMapObject::getFrequency() const
{
  if(type == maptypes::INVALID)
    return 0;

  if(isAnyApproach())
  {
    if(!approachLeg.navaids.vors.isEmpty())
      return approachLeg.navaids.vors.first().frequency;
    else if(!approachLeg.navaids.ndbs.isEmpty())
      return approachLeg.navaids.ndbs.first().frequency;
    else if(!approachLeg.navaids.ils.isEmpty())
      return approachLeg.navaids.ils.first().frequency;
  }
  else
  {
    switch(curEntry().getWaypointType())
    {
      case atools::fs::pln::entry::UNKNOWN:
      case atools::fs::pln::entry::USER:
      case atools::fs::pln::entry::INTERSECTION:
      case atools::fs::pln::entry::AIRPORT:
        return 0;

      case atools::fs::pln::entry::VOR:
        return vor.frequency;

      case atools::fs::pln::entry::NDB:
        return ndb.frequency;
    }
  }
  return 0;
}

const atools::fs::pln::FlightplanEntry& RouteMapObject::curEntry() const
{
  if(isRoute())
    return flightplan->at(index);
  else
    return EMPTY_FLIGHTPLAN_ENTRY;
}

bool RouteMapObject::isRoute() const
{
  return !isAnyApproach();
}

bool RouteMapObject::isAnyApproach() const
{
  return type & maptypes::APPROACH_ALL;
}

bool RouteMapObject::isApproach() const
{
  return type & maptypes::APPROACH;
}

bool RouteMapObject::isMissed() const
{
  return type & maptypes::APPROACH_MISSED;
}

bool RouteMapObject::isTransition() const
{
  return type & maptypes::APPROACH_TRANSITION;
}
