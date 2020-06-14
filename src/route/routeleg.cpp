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

#include "route/routeleg.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/airportquery.h"
#include "geo/calculations.h"
#include "fs/pln/flightplan.h"
#include "common/maptools.h"
#include "common/proctypes.h"
#include "atools.h"
#include "fs/util/fsutil.h"
#include "route/route.h"
#include "options/optiondata.h"
#include "navapp.h"
#include "common/unit.h"

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

RouteLeg::RouteLeg()
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
  valid = validWaypoint = true;
}

void RouteLeg::createCopyFromProcedureLeg(int entryIndex, const RouteLeg& leg, const RouteLeg *prevLeg)
{
  createFromProcedureLeg(entryIndex, leg.getProcedureLeg(), prevLeg);

  if(waypoint.isValid())
    type = map::WAYPOINT;
  else if(vor.isValid())
    type = map::VOR;
  else if(ndb.isValid())
    type = map::NDB;
  else if(ils.isValid())
    type = map::ILS;
  else if(runwayEnd.isValid())
    type = map::RUNWAYEND;
  else
    type = map::USERPOINTROUTE;

  airway = leg.airway;

  procedureLeg = proc::MapProcedureLeg();

  updateMagvar();
  updateDistanceAndCourse(entryIndex, prevLeg);

}

void RouteLeg::createFromProcedureLeg(int entryIndex, const proc::MapProcedureLeg& leg, const RouteLeg *prevLeg)
{
  index = entryIndex;
  procedureLeg = leg;

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
  valid = validWaypoint = true;
}

void RouteLeg::createFromProcedureLegs(int entryIndex, const proc::MapProcedureLegs& legs, const RouteLeg *prevLeg)
{
  createFromProcedureLeg(entryIndex, legs.at(entryIndex), prevLeg);
}

void RouteLeg::assignAnyNavaid(atools::fs::pln::FlightplanEntry *flightplanEntry, const Pos& last, float maxDistance)
{
  map::MapSearchResult mapobjectResult;
  NavApp::getMapQuery()->getMapObjectByIdent(mapobjectResult, map::WAYPOINT | map::VOR | map::NDB | map::AIRPORT,
                                             flightplanEntry->getIdent(), flightplanEntry->getRegion(),
                                             QString(), last, maxDistance);

  if(mapobjectResult.hasVor())
  {
    assignVor(mapobjectResult, flightplanEntry);
    valid = validWaypoint = true;
  }
  else if(mapobjectResult.hasNdb())
  {
    assignNdb(mapobjectResult, flightplanEntry);
    valid = validWaypoint = true;
  }
  else if(mapobjectResult.hasWaypoints())
  {
    assignIntersection(mapobjectResult, flightplanEntry);
    valid = validWaypoint = true;
  }
}

void RouteLeg::createFromDatabaseByEntry(int entryIndex, const RouteLeg *prevLeg)
{
  index = entryIndex;

  atools::fs::pln::FlightplanEntry *flightplanEntry = &(*flightplan)[index];
  MapQuery *mapQuery = NavApp::getMapQuery();
  AirwayTrackQuery *airwayQuery = NavApp::getAirwayTrackQuery();
  AirportQuery *airportQuery = NavApp::getAirportQuerySim();

  QString region = flightplanEntry->getRegion();

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
          airwayQuery->getWaypointsForAirway(mapobjectResult.waypoints,
                                             flightplanEntry->getAirway(), flightplanEntry->getIdent());
          maptools::sortByDistance(mapobjectResult.waypoints, prevLeg->getPosition());
          if(mapobjectResult.hasWaypoints())
          {
            // Use navaid at airway
            assignIntersection(mapobjectResult, flightplanEntry);
            validWaypoint = true;
          }
          else
          {
            // Not found at airway search for any navaid with the given name nearby
            assignAnyNavaid(flightplanEntry, last, maxDistance);
            qWarning() << "No waypoints for" << flightplanEntry->getIdent() << flightplanEntry->getAirway();
          }
        }
        else
          qWarning() << "Cannot resolve unknwon flight plan entry" << flightplanEntry->getIdent();
      }
      break;

    // ====================== Create for airport and assign parking position
    case atools::fs::pln::entry::AIRPORT:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::AIRPORT, flightplanEntry->getIdent());
      if(!mapobjectResult.airports.isEmpty())
      {
        assignAirport(mapobjectResult, flightplanEntry);

        alternate = flightplanEntry->getFlags() & atools::fs::pln::entry::ALTERNATE;

        validWaypoint = true;

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
    case atools::fs::pln::entry::WAYPOINT:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::WAYPOINT | map::AIRPORT,
                                    flightplanEntry->getIdent(), region, /* region is ignored for airports */
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.waypoints.isEmpty())
      {
        assignIntersection(mapobjectResult, flightplanEntry);
        validWaypoint = true;
      }
      else if(!mapobjectResult.airports.isEmpty())
      {
        // FSC saves airports in the flight plan wrongly as intersections
        assignAirport(mapobjectResult, flightplanEntry);
        validWaypoint = true;
      }
      else if(!atools::fs::util::isValidIdent(flightplanEntry->getIdent()))
      {
        // Name contains funny characters or is too long - must me a user fix from FSC
        // flightplanEntry->setWaypointId(flightplanEntry->getIdent());
        assignUser(flightplanEntry);
        validWaypoint = true;
      }
      break;

    // =============================== Navaid VOR
    case atools::fs::pln::entry::VOR:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::VOR, flightplanEntry->getIdent(), region,
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.vors.isEmpty())
      {
        assignVor(mapobjectResult, flightplanEntry);
        validWaypoint = true;
      }
      break;

    // =============================== Navaid NDB
    case atools::fs::pln::entry::NDB:
      mapQuery->getMapObjectByIdent(mapobjectResult, map::NDB, flightplanEntry->getIdent(), region,
                                    QString(), flightplanEntry->getPosition(), MAX_WAYPOINT_DISTANCE_METER);
      if(!mapobjectResult.ndbs.isEmpty())
      {
        assignNdb(mapobjectResult, flightplanEntry);
        validWaypoint = true;
      }
      break;

    // =============================== Navaid user coordinates
    case atools::fs::pln::entry::USER:
      assignUser(flightplanEntry);
      validWaypoint = true;
      break;
  }

  if(!validWaypoint)
    // Leave the flight plan type as is and change internal type only
    type = map::INVALID;

  valid = true;

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
  magvarPos = NavApp::getMagVar(getPosition());
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
    magvar = magvarPos;
}

void RouteLeg::updateDistanceAndCourse(int entryIndex, const RouteLeg *prevLeg)
{
  index = entryIndex;

  if(prevLeg != nullptr)
  {
    const Pos& prevPos = prevLeg->getPosition();

    if(isAnyProcedure())
    {
      if((prevLeg->isRoute() || // Transition from route to procedure
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
        distanceTo = meterToNm(procedureLeg.line.getPos1().distanceMeterTo(prevPos));
      }
      else
      {
        // Use course and distance from last procedure leg
        courseTo = procedureLeg.calculatedTrueCourse;
        distanceTo = procedureLeg.calculatedDistance;
      }
      geometry = procedureLeg.geometry;
    }
    else
    {
      if(getPosition() == prevPos)
      {
        // Collapse any overlapping waypoints to avoid course display
        distanceTo = 0.f;
        courseTo = map::INVALID_COURSE_VALUE;
        geometry = LineString({getPosition()});
      }
      else
      {
        // Calculate for normal or alternate leg - prev is destination airport for alternate legs
        distanceTo = meterToNm(getPosition().distanceMeterTo(prevPos));
        courseTo = normalizeCourse(prevLeg->getPosition().angleDegTo(getPosition()));
        geometry = LineString({prevPos, getPosition()});
      }
    }
  }
  else
  {
    // No predecessor - this one is the first in the list
    distanceTo = 0.f;
    courseTo = 0.f;
    geometry = LineString({getPosition()});
  }
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
    return tr("User");
  else
    return EMPTY_STRING;
}

float RouteLeg::getCourseToMag() const
{
  if(OptionData::instance().getFlags() & opts::ROUTE_IGNORE_VOR_DECLINATION)
    return courseTo < map::INVALID_COURSE_VALUE ? normalizeCourse(courseTo - magvarPos) : courseTo;
  else
    return courseTo < map::INVALID_COURSE_VALUE ? normalizeCourse(courseTo - magvar) : courseTo;
}

float RouteLeg::getMagVarBySettings() const
{
  return OptionData::instance().getFlags() & opts::ROUTE_IGNORE_VOR_DECLINATION ? magvarPos : magvar;
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
        return EMPTY_POS;
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
  return EMPTY_POS;
}

float RouteLeg::getAltitude() const
{
  if(airport.isValid())
    return airport.position.getAltitude();
  else if(vor.isValid())
    return vor.position.getAltitude();
  else if(ndb.isValid())
    return ndb.position.getAltitude();
  else if(waypoint.isValid())
    return 0.f;
  else if(ils.isValid())
    return ils.position.getAltitude();
  else if(runwayEnd.isValid())
    return runwayEnd.getPosition().getAltitude();
  else if(!procedureLeg.displayText.isEmpty())
    return 0.f;
  else if(type == map::INVALID)
    return 0.f;
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return 0.f;
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    return 0.f;
  else
    return 0.f;
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
    return getFlightplanEntry().getIdent();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return getFlightplanEntry().getIdent();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    return EMPTY_STRING;
  else
    return EMPTY_STRING;
}

QString RouteLeg::getComment() const
{
  return getFlightplanEntry().getComment();
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

bool RouteLeg::isUser() const
{
  return getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER;
}

QString RouteLeg::getRegion() const
{
  if(airport.isValid())
    return airport.region;
  else if(vor.isValid())
    return vor.region;
  else if(ndb.isValid())
    return ndb.region;
  else if(waypoint.isValid())
    return waypoint.region;
  else if(type == map::INVALID)
    return getFlightplanEntry().getRegion();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return getFlightplanEntry().getRegion();

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
  else if(type == map::INVALID)
    return getFlightplanEntry().getName();
  else if(getFlightplanEntry().getWaypointType() == atools::fs::pln::entry::USER)
    return getFlightplanEntry().getName();
  else
    return EMPTY_STRING;
}

const QString& RouteLeg::getAirwayName() const
{
  // if(isRoute())
  return getFlightplanEntry().getAirway();
  // else
  // return EMPTY_STRING;
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
  if(index >= 0)
  {
    // if(isRoute())
    return flightplan->at(index);
  }
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << index;

  return EMPTY_FLIGHTPLAN_ENTRY;
}

atools::fs::pln::FlightplanEntry *RouteLeg::getFlightplanEntry()
{
  if(index >= 0)
  {
    // if(isRoute())
    return &flightplan->getEntries()[index];
  }
  else
    qWarning() << Q_FUNC_INFO << "invalid index" << index;

  return nullptr;
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

bool RouteLeg::isAirwaySetAndInvalid(float altitudeFt, QStringList *errors) const
{
  bool invalid = true;
  if(airway.isValid())
  {
    // Set and valid - check direction
    if(airway.direction == map::DIR_BOTH)
      // Always valid in both directions
      invalid = false;
    else if(airway.direction == map::DIR_FORWARD)
      // From this to airway "from" is wrong direction - invalid
      invalid = getId() == airway.fromWaypointId;
    else if(airway.direction == map::DIR_BACKWARD)
      // From this to airway "to" is wrong direction for backward - invalid
      invalid = getId() == airway.toWaypointId;

    if(errors != nullptr && invalid)
      errors->append(tr("Wrong direction in one-way segment."));

    if(altitudeFt < map::INVALID_ALTITUDE_VALUE)
    {
      // Use a buffer for comparison to avoid rounding errors
      if(altitudeFt < airway.minAltitude - 10.f)
      {
        invalid = true;
        if(errors != nullptr)
          errors->append(tr("Cruise altitude %1 is below minimum altitude of %2.").
                         arg(Unit::altFeet(altitudeFt)).arg(Unit::altFeet(airway.minAltitude)));
      }

      if(airway.maxAltitude > 0 && altitudeFt > airway.maxAltitude + 10.f)
      {
        invalid = true;
        if(errors != nullptr)
          errors->append(tr("Cruise altitude %1 is above maximum altitude of %2.").
                         arg(Unit::altFeet(altitudeFt)).arg(Unit::altFeet(airway.maxAltitude)));
      }
    }

    if(invalid && errors != nullptr)
      // General violations message
      errors->prepend(tr("Leg to \"%1\" violates restrictions for airway \"%2\":").
                      arg(getIdent()).arg(getAirwayName()));
    return invalid;
  }
  else
  {
    if(!getAirwayName().isEmpty())
    {
      invalid = true;
      if(errors != nullptr)
        errors->append(tr("Airway %1 not found for %2.").arg(getAirwayName()).arg(getIdent()));
    }
  }

  return !getAirwayName().isEmpty() && !airway.isValid();
}

void RouteLeg::clearAirwayOrTrack()
{
  airway = map::MapAirway();
}

// TODO assign functions are duplicatd in FlightplanEntryBuilder
void RouteLeg::assignUser(atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::USERPOINTROUTE;
  flightplanEntry->setWaypointType(atools::fs::pln::entry::USER);
  flightplanEntry->setIdent(getIdent());
  flightplanEntry->setName(getName());
  flightplanEntry->setRegion(getRegion());
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
  flightplanEntry->setIdent(airport.ident);
  flightplanEntry->setName(airport.name);
  flightplanEntry->setMagvar(airport.magvar);
}

void RouteLeg::assignIntersection(const map::MapSearchResult& mapobjectResult,
                                  atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::WAYPOINT;
  waypoint = mapobjectResult.waypoints.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setRegion(waypoint.region);
  flightplanEntry->setIdent(waypoint.ident);
  flightplanEntry->setCoords(waypoint.position); // Use setCoords to keep altitude
  flightplanEntry->setWaypointType(atools::fs::pln::entry::WAYPOINT);
  flightplanEntry->setMagvar(waypoint.magvar);
}

void RouteLeg::assignVor(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::VOR;
  vor = mapobjectResult.vors.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setRegion(vor.region);
  flightplanEntry->setIdent(vor.ident);
  flightplanEntry->setCoords(vor.position); // Use setCoords to keep altitude
  flightplanEntry->setWaypointType(atools::fs::pln::entry::VOR);
  flightplanEntry->setName(vor.name);
  flightplanEntry->setMagvar(vor.magvar);
  flightplanEntry->setFrequency(vor.frequency);
}

void RouteLeg::assignNdb(const map::MapSearchResult& mapobjectResult, atools::fs::pln::FlightplanEntry *flightplanEntry)
{
  type = map::NDB;
  ndb = mapobjectResult.ndbs.first();

  // Update all fields in entry if found - otherwise leave as is
  flightplanEntry->setRegion(ndb.region);
  flightplanEntry->setIdent(ndb.ident);
  flightplanEntry->setCoords(ndb.position); // Use setCoords to keep altitude
  flightplanEntry->setWaypointType(atools::fs::pln::entry::NDB);
  flightplanEntry->setName(ndb.name);
  flightplanEntry->setMagvar(ndb.magvar);
  flightplanEntry->setFrequency(ndb.frequency);
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
  QDebugStateSaver saver(out);

  out.noquote().nospace() << "RouteLeg["
                          << "id " << leg.getId()
                          << ", " << leg.getIdent()
                          << ", " << leg.getMapObjectTypeName()
                          << ", distance " << leg.getDistanceTo()
                          << ", " << leg.getPosition()
                          << ", course " << leg.getCourseToMag() << "°M " << leg.getCourseToTrue() << "°T"
                          << ", magvar " << leg.getMagvar()
                          << ", magvarPos " << leg.getMagvarPos()
                          << (leg.isValidWaypoint() ? ", valid" : QString())
                          << (leg.isNavdata() ? ", nav" : QString())
                          << (leg.isAlternate() ? ", alternate" : QString())
                          << (leg.isAnyProcedure() ? ", procedure" : QString())
                          << ", " << proc::procedureLegTypeStr(leg.getProcedureLegType())
                          << ", airway " << leg.getAirway().id << " " << leg.getAirwayName()
                          << (leg.getAirway().isValid() ? " valid" : " invalid")
                          << "]";
  return out;
}
