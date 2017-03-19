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

#include "route/routeleg.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "fs/pln/flightplan.h"
#include "atools.h"
#include "route/route.h"

#include <QRegularExpression>

using namespace atools::geo;

/* Extract parking name and number from FS flight plan */
const QRegularExpression PARKING_TO_NAME_AND_NUM("([A-Za-z_ ]*)([0-9]+)");

/* If region is not set search within this distance (not the read GC distance) for navaids with the same name */
const int MAX_WAYPOINT_DISTANCE_METER = 10000.f;

const static QString EMPTY_STRING;
const static atools::fs::pln::FlightplanEntry EMPTY_FLIGHTPLAN_ENTRY;

RouteLeg::RouteLeg(atools::fs::pln::Flightplan *parentFlightplan)
  : flightplan(parentFlightplan)
{

}

RouteLeg::~RouteLeg()
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

void RouteLeg::createFromAirport(int entryIndex, const maptypes::MapAirport& newAirport,
                                       const RouteLeg *predRouteMapObj)
{
  index = entryIndex;
  type = maptypes::AIRPORT;
  airport = newAirport;

  updateMagvar();
  updateDistanceAndCourse(entryIndex, predRouteMapObj);
  valid = true;
}

void RouteLeg::createFromApproachLeg(int entryIndex, const maptypes::MapProcedureLegs& legs,
                                           const RouteLeg *predRouteMapObj)
{
  index = entryIndex;
  procedureLeg = legs.at(entryIndex);
  magvar = procedureLeg.magvar;

  type = procedureLeg.mapType;

  if(procedureLeg.navaids.hasWaypoints())
    waypoint = procedureLeg.navaids.waypoints.first();
  if(procedureLeg.navaids.hasVor())
    vor = procedureLeg.navaids.vors.first();
  if(procedureLeg.navaids.hasNdb())
    ndb = procedureLeg.navaids.ndbs.first();
  if(procedureLeg.navaids.hasIls())
    ils = procedureLeg.navaids.ils.first();
  if(procedureLeg.navaids.hasRunwayEnd())
    runwayEnd = procedureLeg.navaids.runwayEnds.first();

  updateMagvar();
  updateDistanceAndCourse(entryIndex, predRouteMapObj);
  valid = true;
}

void RouteLeg::createFromDatabaseByEntry(int entryIndex, MapQuery *query, const RouteLeg *predRouteMapObj)
{
  index = entryIndex;

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

  updateMagvar();
  updateDistanceAndCourse(entryIndex, predRouteMapObj);
}

void RouteLeg::setDepartureParking(const maptypes::MapParking& departureParking)
{
  parking = departureParking;
  start = maptypes::MapStart();
}

void RouteLeg::setDepartureStart(const maptypes::MapStart& departureStart)
{
  start = departureStart;
  parking = maptypes::MapParking();
}

void RouteLeg::updateMagvar()
{
  if(isAnyProcedure())
    magvar = procedureLeg.magvar;
  else if(airport.isValid())
    magvar = airport.magvar;
  else if(vor.isValid())
    magvar = vor.magvar;
  else if(ndb.isValid())
    magvar = ndb.magvar;
  else if(waypoint.isValid())
    magvar = waypoint.magvar;
  else
    magvar = 0.f;
}

void RouteLeg::updateInvalidMagvar(int entryIndex, const Route *routeList)
{
  if(type == maptypes::USER || type == maptypes::INVALID)
  {
    magvar = 0.f;
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
}

void RouteLeg::updateDistanceAndCourse(int entryIndex, const RouteLeg *predRouteMapObj)
{
  index = entryIndex;

  if(isAnyProcedure())
  {
    if(predRouteMapObj != nullptr && predRouteMapObj->isRoute() && procedureLeg.line.isPoint())
    {
      const Pos& prevPos = predRouteMapObj->getPosition();
      courseTo = normalizeCourse(prevPos.angleDegTo(procedureLeg.line.getPos1()));
      courseRhumbTo = normalizeCourse(prevPos.angleDegToRhumb(procedureLeg.line.getPos1()));
      distanceTo = meterToNm(procedureLeg.line.getPos1().distanceMeterTo(prevPos));
      distanceToRhumb = meterToNm(procedureLeg.line.getPos1().distanceMeterToRhumb(prevPos));
    }
    else
    {
      courseTo = procedureLeg.calculatedTrueCourse;
      courseRhumbTo = procedureLeg.calculatedTrueCourse;
      distanceTo = procedureLeg.calculatedDistance;
      distanceToRhumb = procedureLeg.calculatedDistance;
    }

    geometry = procedureLeg.geometry;
  }
  else if(predRouteMapObj != nullptr)
  {
    const Pos& prevPos = predRouteMapObj->getPosition();
    distanceTo = meterToNm(getPosition().distanceMeterTo(prevPos));
    distanceToRhumb = meterToNm(getPosition().distanceMeterToRhumb(prevPos));
    courseTo = normalizeCourse(predRouteMapObj->getPosition().angleDegTo(getPosition()));
    courseRhumbTo = normalizeCourse(predRouteMapObj->getPosition().angleDegToRhumb(getPosition()));
    geometry = LineString({prevPos, getPosition()});
  }
  else
  {
    // No predecessor - this one is the first in the list
    distanceTo = 0.f;
    distanceToRhumb = 0.f;
    courseTo = 0.f;
    courseRhumbTo = 0.f;
    geometry = LineString({getPosition()});
  }
}

void RouteLeg::updateUserName(const QString& name)
{
  flightplan->getEntries()[index].setWaypointId(name);
}

int RouteLeg::getId() const
{
  if(type == maptypes::INVALID)
    return -1;

  if(waypoint.isValid())
    return waypoint.id;
  else if(vor.isValid())
    return vor.id;
  else if(ndb.isValid())
    return ndb.id;
  else if(airport.isValid())
    return airport.id;
  else if(ils.isValid())
    return ils.id;

  return -1;
}

int RouteLeg::getRange() const
{
  if(type == maptypes::INVALID)
    return -1;

  if(vor.isValid())
    return vor.range;
  else if(ndb.isValid())
    return ndb.range;
  else if(ils.isValid())
    return ils.range;

  return -1;
}

QString RouteLeg::getMapObjectTypeName() const
{
  if(type == maptypes::INVALID)
    return tr("Invalid");
  else if(waypoint.isValid())
    return tr("Waypoint");
  else if(vor.isValid())
    return maptypes::vorType(vor) + " (" + maptypes::navTypeNameVor(vor.type) + ")";
  else if(ndb.isValid())
    return tr("NDB (%1)").arg(maptypes::navTypeNameNdb(ndb.type));
  else if(airport.isValid())
    return tr("Airport");
  else if(ils.isValid())
    return tr("ILS");
  else if(runwayEnd.isValid())
    return tr("Runway");
  else if(type == maptypes::USER)
    return EMPTY_STRING;
  else
    return tr("Unknown");
}

float RouteLeg::getCourseToMag() const
{
  return atools::geo::normalizeCourse(courseTo - magvar);
}

float RouteLeg::getCourseToRhumbMag() const
{
  return atools::geo::normalizeCourse(courseRhumbTo - magvar);
}

const atools::geo::Pos& RouteLeg::getPosition() const
{
  if(isAnyProcedure())
    return procedureLeg.line.getPos2();
  else
  {
    if(type == maptypes::INVALID)
    {
      if(curEntry().getPosition().isValid())
        return curEntry().getPosition();
      else
        return atools::geo::EMPTY_POS;
    }

    if(airport.isValid())
      return airport.position;
    else if(vor.isValid())
      return vor.position;
    else if(ndb.isValid())
      return ndb.position;
    else if(waypoint.isValid())
      return waypoint.position;
    else if(ils.isValid())
      return ils.position;
    else if(runwayEnd.isValid())
      return runwayEnd.position;
    else if(curEntry().getWaypointType() == atools::fs::pln::entry::USER)
      return curEntry().getPosition();
  }
  return atools::geo::EMPTY_POS;
}

QString RouteLeg::getIdent() const
{
  if(airport.isValid())
    return airport.ident;
  else if(vor.isValid())
    return vor.ident;
  else if(ndb.isValid())
    return ndb.ident;
  else if(waypoint.isValid())
    return waypoint.ident;
  else if(ils.isValid())
    return ils.ident;
  else if(runwayEnd.isValid())
    return "RW" + runwayEnd.name;
  else if(!procedureLeg.displayText.isEmpty())
    return procedureLeg.displayText.first();
  else if(type == maptypes::INVALID)
    return curEntry().getIcaoIdent();
  else if(curEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return curEntry().getWaypointId();
  else if(curEntry().getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    return tr("Unknown Waypoint Type");
  else
    return EMPTY_STRING;
}

QString RouteLeg::getRegion() const
{
  if(vor.isValid())
    return vor.region;
  else if(ndb.isValid())
    return ndb.region;
  else if(waypoint.isValid())
    return waypoint.region;

  return EMPTY_STRING;
}

QString RouteLeg::getName() const
{
  if(type == maptypes::INVALID)
    return EMPTY_STRING;

  if(airport.isValid())
    return airport.name;
  else if(vor.isValid())
    return vor.name;
  else if(ndb.isValid())
    return ndb.name;
  else if(ils.isValid())
    return ils.name;
  else
    return EMPTY_STRING;
}

const QString& RouteLeg::getAirway() const
{
  if(isRoute())
    return curEntry().getAirway();
  else
    return EMPTY_STRING;
}

int RouteLeg::getFrequency() const
{
  if(type == maptypes::INVALID)
    return 0;

  if(vor.isValid())
    return vor.frequency;
  else if(ndb.isValid())
    return ndb.frequency;
  else if(ils.isValid())
    return ils.frequency;

  return 0;
}

const atools::fs::pln::FlightplanEntry& RouteLeg::curEntry() const
{
  if(isRoute())
    return flightplan->at(index);
  else
    return EMPTY_FLIGHTPLAN_ENTRY;
}

const LineString& RouteLeg::getGeometry() const
{
  return geometry;
}

bool RouteLeg::isApproachPoint() const
{
  return isAnyProcedure() &&
         !atools::contains(procedureLeg.type,
                           {maptypes::HOLD_TO_ALTITUDE, maptypes::HOLD_TO_FIX,
                            maptypes::HOLD_TO_MANUAL_TERMINATION}) &&
         (procedureLeg.geometry.isPoint() || procedureLeg.type == maptypes::INITIAL_FIX ||
          procedureLeg.type == maptypes::START_OF_PROCEDURE);
}
