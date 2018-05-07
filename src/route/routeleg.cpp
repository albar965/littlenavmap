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

#include "route/routeleg.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "geo/calculations.h"
#include "fs/pln/flightplan.h"
#include "common/maptools.h"
#include "atools.h"
#include "fs/util/fsutil.h"
#include "route/route.h"
#include "navapp.h"

#include <QRegularExpression>

using namespace atools::geo;

/* Extract parking name and number from FS flight plan */
const QRegularExpression PARKING_TO_NAME_AND_NUM("([A-Za-z_ ]*)([0-9]+)");

/* If region is not set search within this distance (not the read GC distance) for navaids with the same name */
const int MAX_WAYPOINT_DISTANCE_METER = 10000.f;

/* Maximum distance to the predecessor waypoint if position is invalid */
const int MAX_WAYPOINT_DISTANCE_INVALID_METER = 10000000.f;

const static QString EMPTY_STRING;
const static atools::fs::pln::FlightplanEntry EMPTY_FLIGHTPLAN_ENTRY;

// Appended to X-Plane free parking names - obsolete
const static QLatin1Literal PARKING_NO_NUMBER(" NULL");

RouteLeg::RouteLeg(atools::fs::pln::Flightplan *parentFlightplan)
  : flightplan(parentFlightplan)
{

}

RouteLeg::~RouteLeg()
{

}

void RouteLeg::createFromAirport(int entryIndex, const map::MapAirport& newAirport, const RouteLeg *prevLeg)
{
  index = entryIndex;
  type = map::AIRPORT;
  airport = newAirport;

  updateMagvar();
  updateDistanceAndCourse(entryIndex, prevLeg);
  valid = true;
}

void RouteLeg::createFromApproachLeg(int entryIndex, const proc::MapProcedureLegs& legs, const RouteLeg *prevLeg)
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

void RouteLeg::assignAnyNavaid(atools::fs::pln::FlightplanEntry *flightplanEntry, const atools::geo::Pos& last,
                               float maxDistance)
{
  map::MapSearchResult mapobjectResult;
  NavApp::getMapQuery()->getMapObjectByIdent(mapobjectResult, map::WAYPOINT | map::VOR | map::NDB | map::AIRPORT,
                                             flightplanEntry->getIcaoIdent(), flightplanEntry->getIcaoRegion(),
                                             QString(), last, maxDistance);

  if(mapobjectResult.hasVor())
  {
    assignVor(mapobjectResult, flightplanEntry);
    valid = true;
  }
  else if(mapobjectResult.hasNdb())
  {
    assignNdb(mapobjectResult, flightplanEntry);
    valid = true;
  }
  else if(mapobjectResult.hasWaypoints())
  {
    assignIntersection(mapobjectResult, flightplanEntry);
    valid = true;
  }
}

void RouteLeg::createFromDatabaseByEntry(int entryIndex, const RouteLeg *prevLeg)
{
  index = entryIndex;

  atools::fs::pln::FlightplanEntry *flightplanEntry = &(*flightplan)[index];
  MapQuery *mapQuery = NavApp::getMapQuery();
  AirportQuery *airportQuery = NavApp::getAirportQuerySim();

  QString region = flightplanEntry->getIcaoRegion();

  if(region == "KK" || region == "ZZ") // Invalid route finder region
    region.clear();

  map::MapSearchResult mapobjectResult;
  switch(flightplanEntry->getWaypointType())
  {
    // ====================== Create by totally unknown type with probably invalid position
    case atools::fs::pln::entry::UNKNOWN:
      {
        // Use either the given position or the last point for nearest search of duplicate navaids
        Pos last = flightplanEntry->getPosition();
        float maxDistance = MAX_WAYPOINT_DISTANCE_METER;
        if(!last.isValid() && prevLeg != nullptr)
        {
          last = prevLeg->getPosition();
          maxDistance = MAX_WAYPOINT_DISTANCE_INVALID_METER;
        }

        if(flightplanEntry->getAirway().isEmpty())
          // Look for an arbitrary navaid
          assignAnyNavaid(flightplanEntry, last, maxDistance);
        else if(prevLeg != nullptr && !flightplanEntry->getAirway().isEmpty())
        {
          // Look for navaid at an airway
          mapQuery->getWaypointsForAirway(mapobjectResult.waypoints,
                                          flightplanEntry->getAirway(), flightplanEntry->getIcaoIdent());
          maptools::sortByDistance(mapobjectResult.waypoints, prevLeg->getPosition());
          if(mapobjectResult.hasWaypoints())
          {
            // Use navaid at airway
            assignIntersection(mapobjectResult, flightplanEntry);
            valid = true;
          }
          else
          {
            // Not found at airway search for any navaid with the given name nearby
            assignAnyNavaid(flightplanEntry, last, maxDistance);
            qWarning() << "No waypoints for" << flightplanEntry->getIcaoIdent() << flightplanEntry->getAirway();
          }
        }
        else
          qWarning() << "Cannot resolve unknwon flight plan entry" << flightplanEntry->getIcaoIdent();
      }
      break;

    // ====================== Create for airport and assign parking position
    case atools::fs::pln::entry::AIRPORT:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::AIRPORT, flightplanEntry->getIcaoIdent());
      if(!mapobjectResult.airports.isEmpty())
      {
        assignAirport(mapobjectResult, flightplanEntry);
        valid = true;

        // Resolve parking ==============================
        QString name = flightplan->getDepartureParkingName().trimmed();
        if(!name.isEmpty() && prevLeg == nullptr)
        {
          if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11 || name.endsWith(PARKING_NO_NUMBER))
          {
            // X-Plane =============================================================================
            // Do not resolve parking for X-Plane databases
            // X-Plane style parking - name only
            if(name.endsWith(PARKING_NO_NUMBER))
              name.chop(PARKING_NO_NUMBER.size());

            // Get nearest with the same name
            QList<map::MapParking> parkings;
            airportQuery->getParkingByName(parkings, airport.id, name, flightplan->getDeparturePosition());

            if(parkings.isEmpty())
            {
              // Always try runway or helipad if no start positions found
              qWarning() << "Found no parking spots for" << name;
              assignRunwayOrHelipad(name);
            }
            else
            {
              parking = parkings.first();
              // Update flightplan with found name
              flightplan->setDepartureParkingName(name);
            }
          }
          else
          {
            // FSX/P3D =============================================================================
            // Resolve parking if first airport
            QRegularExpressionMatch match = PARKING_TO_NAME_AND_NUM.match(name);

            // Convert parking name to the format used in the database
            QString parkingName = match.captured(1).trimmed().toUpper().replace(" ", "_");

            if(!parkingName.isEmpty())
            {
              // Seems to be a parking position
              int number = QString(match.captured(2)).toInt();
              QList<map::MapParking> parkings;
              airportQuery->getParkingByNameAndNumber(parkings, airport.id,
                                                      map::parkingDatabaseName(parkingName), number);

              if(parkings.isEmpty())
              {
                // Always try runway or helipad if no start positions found
                qWarning() << "Found no parking spots for" << parkingName << number;
                assignRunwayOrHelipad(name);
              }
              else
              {
                if(parkings.size() > 1)
                  qWarning() << "Found multiple parking spots for" << parkingName << number;

                parking = parkings.first();
                // Update flightplan with found name
                flightplan->setDepartureParkingName(map::parkingNameForFlightplan(parking));
              }
            }
            else
              // Name does not match FSX patter try runway or helipads
              assignRunwayOrHelipad(name);
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

    // =============================== Navaid waypoint
    case atools::fs::pln::entry::INTERSECTION:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::WAYPOINT | map::AIRPORT,
                                    flightplanEntry->getIcaoIdent(), region, /* region is ignored for airports */
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.waypoints.isEmpty())
      {
        assignIntersection(mapobjectResult, flightplanEntry);
        valid = true;
      }
      else if(!mapobjectResult.airports.isEmpty())
      {
        // FSC saves airports in the flight plan wrongly as intersections
        assignAirport(mapobjectResult, flightplanEntry);
        valid = true;
      }
      else if(!atools::fs::util::isValidIdent(flightplanEntry->getIcaoIdent()))
      {
        // Name contains funny characters - must me a user fix from FSC
        flightplanEntry->setWaypointId(flightplanEntry->getIcaoIdent());
        assignUser(flightplanEntry);
        valid = true;
      }
      break;

    // =============================== Navaid VOR
    case atools::fs::pln::entry::VOR:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::VOR, flightplanEntry->getIcaoIdent(), region,
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.vors.isEmpty())
      {
        assignVor(mapobjectResult, flightplanEntry);
        valid = true;
      }
      break;

    // =============================== Navaid NDB
    case atools::fs::pln::entry::NDB:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::NDB, flightplanEntry->getIcaoIdent(), region,
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.ndbs.isEmpty())
      {
        assignNdb(mapobjectResult, flightplanEntry);
        valid = true;
      }
      break;

    // =============================== Navaid user coordinates
    case atools::fs::pln::entry::USER:
      assignUser(flightplanEntry);
      valid = true;
      break;
  }

  if(!valid)
    // Leave the flight plan type as is and change internal type only
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
    magvar = NavApp::getMagVar(getPosition());
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

        (atools::contains(procedureLeg.type, {proc::INITIAL_FIX, proc::START_OF_PROCEDURE}) ||
         procedureLeg.line.isPoint()) // Beginning of procedure
        )
      {
        // qDebug() << Q_FUNC_INFO << "special transition for leg" << index << procedureLeg;

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

void RouteLeg::updateUserPosition(const atools::geo::Pos& pos)
{
  flightplan->getEntries()[index].setPosition(pos);
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
    return ndb.type.isEmpty() ? tr("NDB") : tr("NDB (%1)").arg(map::navTypeNameNdb(ndb.type));
  else if(airport.isValid())
    return tr("Airport");
  else if(ils.isValid())
    return tr("ILS");
  else if(runwayEnd.isValid())
    return tr("Runway");
  else if(type == map::USERPOINTROUTE)
    return EMPTY_STRING;
  else
    return EMPTY_STRING;
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
      if(getFlightplanEntry().getPosition().isValid())
        return getFlightplanEntry().getPosition();
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
    else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
      return getFlightplanEntry().getPosition();
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
    return getFlightplanEntry().getIcaoIdent();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return getFlightplanEntry().getWaypointId();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    return EMPTY_STRING;
  else
    return EMPTY_STRING;
}

bool RouteLeg::isNavdata() const
{
  if(airport.isValid())
    return airport.navdata;
  else if(vor.isValid())
    return true;
  else if(ndb.isValid())
    return true;
  else if(waypoint.isValid())
    return true;
  else if(ils.isValid())
    return true;
  else if(runwayEnd.isValid())
    return runwayEnd.navdata;
  else if(type == map::INVALID)
    return true;
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return true;
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    return true;

  return true;
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

const QString& RouteLeg::getAirwayName() const
{
  if(isRoute())
    return getFlightplanEntry().getAirway();
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

const atools::fs::pln::FlightplanEntry& RouteLeg::getFlightplanEntry() const
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

bool RouteLeg::isAirwaySetAndInvalid() const
{
  if(!getAirwayName().isEmpty() && airway.isValid())
  {
    // Set and valid - check direction
    if(airway.direction == map::DIR_BOTH)
      // Always valid in both directions
      return false;
    else if(airway.direction == map::DIR_FORWARD)
      // From this to airway "from" is wrong direction - invalid
      return getId() == airway.fromWaypointId;
    else if(airway.direction == map::DIR_BACKWARD)
      // From this to airway "to" is wrong direction for backward - invalid
      return getId() == airway.toWaypointId;
  }

  return !getAirwayName().isEmpty() && !airway.isValid();
}

// TODO assign functions are duplicatd in FlightplanEntryBuilder
void RouteLeg::assignUser(atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::USERPOINTROUTE;
  flightplanEntry->setWaypointType(atools::fs::pln::entry::USER);
  flightplanEntry->setIcaoIdent(QString());
  flightplanEntry->setIcaoRegion(QString());
  flightplanEntry->setMagvar(NavApp::getMagVar(flightplanEntry->getPosition()));
}

void RouteLeg::assignAirport(const map::MapSearchResult& mapobjectResult,
                             atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::AIRPORT;
  airport = mapobjectResult.airports.first();

  flightplanEntry->setWaypointType(atools::fs::pln::entry::AIRPORT);
  if(!flightplanEntry->getPosition().isValid())
    flightplanEntry->setPosition(airport.position);

  // values which are not saved in PLN but other formats
  flightplanEntry->setName(airport.name);
  flightplanEntry->setMagvar(airport.magvar);
}

void RouteLeg::assignIntersection(const map::MapSearchResult& mapobjectResult,
                                  atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::WAYPOINT;
  waypoint = mapobjectResult.waypoints.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setIcaoRegion(waypoint.region);
  flightplanEntry->setIcaoIdent(waypoint.ident);
  flightplanEntry->setPosition(waypoint.position);
  flightplanEntry->setWaypointType(atools::fs::pln::entry::INTERSECTION);
  flightplanEntry->setMagvar(waypoint.magvar);
}

void RouteLeg::assignVor(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::VOR;
  vor = mapobjectResult.vors.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setIcaoRegion(vor.region);
  flightplanEntry->setIcaoIdent(vor.ident);
  flightplanEntry->setPosition(vor.position);
  flightplanEntry->setWaypointType(atools::fs::pln::entry::VOR);
  flightplanEntry->setName(vor.name);
  flightplanEntry->setMagvar(vor.magvar);
}

void RouteLeg::assignNdb(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::NDB;
  ndb = mapobjectResult.ndbs.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setIcaoRegion(ndb.region);
  flightplanEntry->setIcaoIdent(ndb.ident);
  flightplanEntry->setPosition(ndb.position);
  flightplanEntry->setWaypointType(atools::fs::pln::entry::NDB);
  flightplanEntry->setName(ndb.name);
  flightplanEntry->setMagvar(ndb.magvar);
}

void RouteLeg::assignRunwayOrHelipad(const QString& name)
{
  NavApp::getAirportQuerySim()->getStartByNameAndPos(start, airport.id, name, flightplan->getDeparturePosition());

  if(!start.isValid())
  {
    qWarning() << "Found no start positions";
    // Clear departure position in flight plan
    flightplan->setDepartureParkingName(QString());
  }
  else
    // Helicopter pad or runway name
    flightplan->setDepartureParkingName(start.runwayName);
}

QDebug operator<<(QDebug out, const RouteLeg& leg)
{
  out << "RouteLeg[id" << leg.getId() << leg.getPosition()
      << "magvar" << leg.getMagvar()
      << "distance" << leg.getDistanceTo()
      << "course mag" << leg.getCourseToMag()
      << "course true" << leg.getCourseToTrue()
      << "id" << leg.getId()
      << "ident" << leg.getIdent()
      << "nav" << leg.isNavdata()
      << leg.getIdent() << leg.getMapObjectTypeName()
      << proc::procedureLegTypeStr(leg.getProcedureLegType())
      << "airway" << leg.getAirway().id << leg.getAirwayName() << (leg.getAirway().isValid() ? "valid" : "invalid")
      << "]";
  return out;
}
