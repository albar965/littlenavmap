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
      float distance = map::INVALID_DISTANCE_VALUE;
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

void RouteLeg::createFromAirport(int entryIndex, const map::MapAirport& newAirport,
                                 const RouteLeg *prevLeg)
{
  index = entryIndex;
  type = map::AIRPORT;
  airport = newAirport;

  updateMagvar();
  updateDistanceAndCourse(entryIndex, prevLeg);
  valid = true;
}

void RouteLeg::createFromApproachLeg(int entryIndex, const proc::MapProcedureLegs& legs,
                                     const RouteLeg *prevLeg)
{
  index = entryIndex;
  procedureLeg = legs.at(entryIndex);
  magvar = procedureLeg.magvar;

  type = map::PROCEDURE;

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
  updateDistanceAndCourse(entryIndex, prevLeg);
  valid = true;
}

void RouteLeg::createFromDatabaseByEntry(int entryIndex, MapQuery *query, const RouteLeg *prevLeg)
{
  index = entryIndex;

  atools::fs::pln::FlightplanEntry *flightplanEntry = &(*flightplan)[index];

  QString region = flightplanEntry->getIcaoRegion();

  if(region == "KK") // Invalid route finder region
    region.clear();

  bool found;
  map::MapSearchResult mapobjectResult;
  switch(flightplanEntry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      break;
    case atools::fs::pln::entry::AIRPORT:
      query->getMapObjectByIdent(mapobjectResult, map::AIRPORT, flightplanEntry->getIcaoIdent());
      if(!mapobjectResult.airports.isEmpty())
      {
        type = map::AIRPORT;
        airport = mapobjectResult.airports.first();
        valid = true;

        QString name = flightplan->getDepartureParkingName().trimmed();
        if(!name.isEmpty() && prevLeg == nullptr)
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
              QList<map::MapParking> parkings;
              query->getParkingByNameAndNumber(parkings, airport.id,
                                               map::parkingDatabaseName(parkingName), number);

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
                flightplan->setDepartureParkingName(map::parkingNameForFlightplan(parking));
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
          start = map::MapStart();
          parking = map::MapParking();
        }
      }
      break;
    case atools::fs::pln::entry::INTERSECTION:
      {
        // Navaid waypoint
        query->getMapObjectByIdent(mapobjectResult, map::WAYPOINT,
                                   flightplanEntry->getIcaoIdent(), region);
        const map::MapWaypoint& obj = findMapObject(mapobjectResult.waypoints,
                                                    flightplanEntry->getPosition(), found);
        if(found)
        {
          type = map::WAYPOINT;
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
        query->getMapObjectByIdent(mapobjectResult, map::VOR, flightplanEntry->getIcaoIdent(), region);
        const map::MapVor& obj = findMapObject(mapobjectResult.vors,
                                               flightplanEntry->getPosition(), found);
        if(found)
        {
          type = map::VOR;
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
        query->getMapObjectByIdent(mapobjectResult, map::NDB, flightplanEntry->getIcaoIdent(), region);
        const map::MapNdb& obj = findMapObject(mapobjectResult.ndbs,
                                               flightplanEntry->getPosition(), found);
        if(found)
        {
          type = map::NDB;
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
      type = map::USER;
      flightplanEntry->setIcaoIdent(QString());
      flightplanEntry->setIcaoRegion(QString());
      // flightplanEntry->setWaypointId(userName);
      break;
  }

  if(!valid)
    type = map::INVALID;

  updateMagvar();
  updateDistanceAndCourse(entryIndex, prevLeg);
}

void RouteLeg::setDepartureParking(const map::MapParking& departureParking)
{
  parking = departureParking;
  start = map::MapStart();
}

void RouteLeg::setDepartureStart(const map::MapStart& departureStart)
{
  start = departureStart;
  parking = map::MapParking();
}

void RouteLeg::updateMagvar()
{
  if(isAnyProcedure())
    magvar = procedureLeg.magvar;
  else if(waypoint.isValid())
    magvar = waypoint.magvar;
  else if(vor.isValid())
    magvar = vor.magvar;
  else if(ndb.isValid())
    magvar = ndb.magvar;
  // Airport is least reliable and often wrong
  else if(airport.isValid())
    magvar = airport.magvar;
  else
    magvar = 0.f;
}

void RouteLeg::updateInvalidMagvar(int entryIndex, const Route *routeList)
{
  if(type == map::USER || type == map::INVALID)
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

void RouteLeg::updateDistanceAndCourse(int entryIndex, const RouteLeg *prevLeg)
{
  index = entryIndex;

  if(prevLeg != nullptr)
  {
    const Pos& prevPos = prevLeg->getPosition();

    if(isAnyProcedure())
    {
      if(
        (prevLeg->isRoute() || // Transition from route to procedure
         (prevLeg->getProcedureLeg().isAnyDeparture() && procedureLeg.isAnyArrival()) || // from SID to aproach, STAR or transition
         (prevLeg->getProcedureLeg().isStar() && procedureLeg.isAnyArrival()) // from STAR aproach or transition

        ) && // Direct connection between procedures

        atools::contains(procedureLeg.type, {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) // Beginning of procedure
        )
      {
        qDebug() << Q_FUNC_INFO << "special transition for leg" << index << procedureLeg;

        // Use course and distance from last route leg to get to this point legs
        courseTo = normalizeCourse(prevPos.angleDegTo(procedureLeg.line.getPos1()));
        courseRhumbTo = normalizeCourse(prevPos.angleDegToRhumb(procedureLeg.line.getPos1()));
        distanceTo = meterToNm(procedureLeg.line.getPos1().distanceMeterTo(prevPos));
        distanceToRhumb = meterToNm(procedureLeg.line.getPos1().distanceMeterToRhumb(prevPos));
      }
      else
      {
        // Use course and distance from last procedure leg
        courseTo = procedureLeg.calculatedTrueCourse;
        courseRhumbTo = procedureLeg.calculatedTrueCourse;
        distanceTo = procedureLeg.calculatedDistance;
        distanceToRhumb = procedureLeg.calculatedDistance;
      }
      geometry = procedureLeg.geometry;
    }
    else
    {
      if(getPosition() == prevPos)
      {
        // Collapse any overlapping waypoints to avoid course display
        distanceTo = 0.f;
        distanceToRhumb = 0.f;
        courseTo = map::INVALID_COURSE_VALUE;
        courseRhumbTo = map::INVALID_COURSE_VALUE;
        geometry = LineString({getPosition()});
      }
      else
      {
        distanceTo = meterToNm(getPosition().distanceMeterTo(prevPos));
        distanceToRhumb = meterToNm(getPosition().distanceMeterToRhumb(prevPos));
        courseTo = normalizeCourse(prevLeg->getPosition().angleDegTo(getPosition()));
        courseRhumbTo = normalizeCourse(prevLeg->getPosition().angleDegToRhumb(getPosition()));
        geometry = LineString({prevPos, getPosition()});
      }
    }
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
  if(type == map::INVALID)
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

bool RouteLeg::isNavaidEqualTo(const RouteLeg& other) const
{
  if(waypoint.isValid() && other.waypoint.isValid())
    return waypoint.id == other.waypoint.id;

  if(vor.isValid() && other.vor.isValid())
    return vor.id == other.vor.id;

  if(ndb.isValid() && other.ndb.isValid())
    return ndb.id == other.ndb.id;

  return false;
}

int RouteLeg::getRange() const
{
  if(type == map::INVALID)
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
  if(type == map::INVALID)
    return tr("Invalid");
  else if(waypoint.isValid())
    return tr("Waypoint");
  else if(vor.isValid())
    return map::vorType(vor) + " (" + map::navTypeNameVor(vor.type) + ")";
  else if(ndb.isValid())
    return tr("NDB (%1)").arg(map::navTypeNameNdb(ndb.type));
  else if(airport.isValid())
    return tr("Airport");
  else if(ils.isValid())
    return tr("ILS");
  else if(runwayEnd.isValid())
    return tr("Runway");
  else if(type == map::USER)
    return EMPTY_STRING;
  else
    return tr("Unknown");
}

float RouteLeg::getCourseToMag() const
{
  return courseTo < map::INVALID_COURSE_VALUE ? atools::geo::normalizeCourse(courseTo - magvar) : courseTo;
}

float RouteLeg::getCourseToRhumbMag() const
{
  return courseRhumbTo <
         map::INVALID_COURSE_VALUE ? atools::geo::normalizeCourse(courseRhumbTo - magvar) : courseTo;
}

const atools::geo::Pos& RouteLeg::getPosition() const
{
  if(isAnyProcedure())
    return procedureLeg.line.getPos2();
  else
  {
    if(type == map::INVALID)
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
  else if(type == map::INVALID)
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
  if(type == map::INVALID)
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

QString RouteLeg::getFrequencyOrChannel() const
{
  if(getFrequency() > 0)
    return QString::number(getFrequency());
  else
    return getChannel();
}

QString RouteLeg::getChannel() const
{
  if(vor.isValid() && vor.tacan)
    return vor.channel;
  else
    return QString();
}

int RouteLeg::getFrequency() const
{
  if(type == map::INVALID)
    return 0;

  if(vor.isValid() && !vor.tacan)
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
                           {proc::HOLD_TO_ALTITUDE, proc::HOLD_TO_FIX,
                            proc::HOLD_TO_MANUAL_TERMINATION}) &&
         (procedureLeg.geometry.isPoint() || procedureLeg.type == proc::INITIAL_FIX ||
          procedureLeg.type == proc::START_OF_PROCEDURE);
}

QDebug operator<<(QDebug out, const RouteLeg& leg)
{
  out << "RouteLeg[id" << leg.getId() << leg.getPosition()
  << "magvar" << leg.getMagvar() << "distance" << leg.getDistanceTo()
  << "course mag" << leg.getCourseToMag() << "course true" << leg.getCourseToTrue()
  << leg.getIdent() << leg.getMapObjectTypeName()
  << proc::procedureLegTypeStr(leg.getProcedureLegType()) << "]";
  return out;
}
