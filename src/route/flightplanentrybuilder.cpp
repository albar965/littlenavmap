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

#include "route/flightplanentrybuilder.h"

#include "common/proctypes.h"
#include "fs/pln/flightplan.h"
#include "fs/pln/flightplanentry.h"
#include "fs/util/fsutil.h"
#include "query/mapquery.h"
#include "navapp.h"

using atools::fs::pln::FlightplanEntry;

FlightplanEntryBuilder::FlightplanEntryBuilder()
{
  mapQuery = NavApp::getMapQuery();
}

/* Copy airport attributes to flight plan entry */
void FlightplanEntryBuilder::buildFlightplanEntry(const map::MapAirport& airport, FlightplanEntry& entry,
                                                  bool alternate) const
{
  entryFromAirport(airport, entry, alternate);
}

/* create a flight plan entry from object id/type or user position */
void FlightplanEntryBuilder::buildFlightplanEntry(int id, const atools::geo::Pos& userPos,
                                                  map::MapObjectTypes type, FlightplanEntry& entry,
                                                  bool resolveWaypoints)
{
  map::MapSearchResult result;
  mapQuery->getMapObjectById(result, type, map::AIRSPACE_SRC_NONE, id, false /* airport from nav database */);
  buildFlightplanEntry(userPos, result, entry, resolveWaypoints, map::NONE);
}

/* create a flight plan entry from object id/type or user position */
void FlightplanEntryBuilder::entryFromUserPos(const atools::geo::Pos& userPos, FlightplanEntry& entry,
                                              const QString& ident, const QString& region, const QString& name)
{
  entry.setPosition(userPos);
  entry.setWaypointType(atools::fs::pln::entry::USER);

  if(ident.isEmpty())
    entry.setIdent("WP" + QString::number(curUserpointNumber++));
  else
    entry.setIdent(ident);
  entry.setName(name);
  entry.setRegion(region);
  entry.setMagvar(NavApp::getMagVar(userPos));
}

/* create a flight plan entry from userpoint */
void FlightplanEntryBuilder::entryFromUserpoint(const map::MapUserpoint& userpoint, FlightplanEntry& entry)
{
  entry.setPosition(userpoint.position);
  entry.setWaypointType(atools::fs::pln::entry::USER);

  if(!userpoint.ident.isEmpty())
    entry.setIdent(userpoint.ident);
  else
    entry.setIdent("WP" + QString::number(curUserpointNumber++));

  entry.setName(userpoint.name);
  entry.setRegion(userpoint.region);
  entry.setComment(userpoint.description);

  entry.setMagvar(NavApp::getMagVar(userpoint.position));
}

void FlightplanEntryBuilder::entryFromNdb(const map::MapNdb& ndb, FlightplanEntry& entry) const
{
  entry.setIdent(ndb.ident);
  entry.setPosition(ndb.position);
  entry.setRegion(ndb.region);
  entry.setWaypointType(atools::fs::pln::entry::NDB);
  entry.setName(ndb.name);
  entry.setMagvar(ndb.magvar);
  entry.setFrequency(ndb.frequency);
}

void FlightplanEntryBuilder::entryFromVor(const map::MapVor& vor, FlightplanEntry& entry) const
{
  entry.setIdent(vor.ident);
  entry.setPosition(vor.position);
  entry.setRegion(vor.region);
  entry.setWaypointType(atools::fs::pln::entry::VOR);
  entry.setName(vor.name);
  entry.setMagvar(vor.magvar);
  entry.setFrequency(vor.frequency);
}

void FlightplanEntryBuilder::entryFromAirport(const map::MapAirport& airport, FlightplanEntry& entry,
                                              bool alternate) const
{
  entry.setIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(atools::fs::pln::entry::AIRPORT);
  entry.setName(airport.name);
  entry.setMagvar(airport.magvar);
  entry.setFlag(atools::fs::pln::entry::ALTERNATE, alternate);
}

bool FlightplanEntryBuilder::vorForWaypoint(const map::MapWaypoint& waypoint, map::MapVor& vor) const
{
  mapQuery->getVorForWaypoint(vor, waypoint.id);

  // Check for invalid references that are caused by the navdata update or disabled navaids at the north pole
  return !vor.ident.isEmpty() && vor.isValid() && !vor.position.isPole() &&
         vor.position.almostEqual(waypoint.position, atools::geo::Pos::POS_EPSILON_10M);
}

bool FlightplanEntryBuilder::ndbForWaypoint(const map::MapWaypoint& waypoint, map::MapNdb& ndb) const
{
  mapQuery->getNdbForWaypoint(ndb, waypoint.id);

  // Check for invalid references that are caused by the navdata update or disabled navaids at the north pole
  return !ndb.ident.isEmpty() && ndb.isValid() && !ndb.position.isPole() &&
         ndb.position.almostEqual(waypoint.position, atools::geo::Pos::POS_EPSILON_10M);

}

void FlightplanEntryBuilder::entryFromWaypoint(const map::MapWaypoint& waypoint, FlightplanEntry& entry,
                                               bool resolveWaypoints) const
{
  bool useWaypoint = true;
  map::MapVor vor;
  map::MapNdb ndb;

  if(resolveWaypoints && waypoint.type == "V")
  {
    // Convert waypoint to underlying VOR for airway routes
    if(vorForWaypoint(waypoint, vor))
    {
      useWaypoint = false;
      entryFromVor(vor, entry);
    }
  }
  else if(resolveWaypoints && waypoint.type == "N")
  {
    // Convert waypoint to underlying NDB for airway routes

    // Workaround for source data error - wrongly assigned VOR waypoints that are assigned to NDBs
    mapQuery->getVorNearest(vor, waypoint.position);
    if(!vor.dmeOnly && !vor.ident.isEmpty() && vor.isValid() && !vor.position.isPole() &&
       vor.position.almostEqual(waypoint.position, atools::geo::Pos::POS_EPSILON_10M))
    {
      // Get the vor if there is one at the waypoint position
      useWaypoint = false;
      entryFromVor(vor, entry);
    }
    else if(ndbForWaypoint(waypoint, ndb))
    {
      useWaypoint = false;
      entryFromNdb(ndb, entry);
    }
  }

  if(useWaypoint)
  {
    entry.setIdent(waypoint.ident);
    entry.setPosition(waypoint.position);
    entry.setRegion(waypoint.region);
    entry.setWaypointType(atools::fs::pln::entry::WAYPOINT);
    entry.setMagvar(waypoint.magvar);
  }
}

void FlightplanEntryBuilder::buildFlightplanEntry(const atools::geo::Pos& userPos,
                                                  const map::MapSearchResult& result,
                                                  FlightplanEntry& entry,
                                                  bool resolveWaypoints,
                                                  map::MapObjectTypes type)
{
  map::MapObjectTypes moType = type;

  if(moType == map::NONE)
  {
    // Determine type if not given
    if(result.hasAirports())
      moType = map::AIRPORT;
    else if(result.hasWaypoints())
      moType = map::WAYPOINT;
    else if(result.hasVor())
      moType = map::VOR;
    else if(result.hasNdb())
      moType = map::NDB;
    else if(result.hasUserpoints())
      moType = map::USERPOINT;
    else if(userPos.isValid())
      moType = map::USERPOINTROUTE;
  }

  if(moType == map::AIRPORT)
    entryFromAirport(result.airports.first(), entry, false /* alternate */);
  else if(moType == map::WAYPOINT)
    entryFromWaypoint(result.waypoints.first(), entry, resolveWaypoints);
  else if(moType == map::VOR)
    entryFromVor(result.vors.first(), entry);
  else if(moType == map::NDB)
    entryFromNdb(result.ndbs.first(), entry);
  else if(moType == map::USERPOINT)
    entryFromUserpoint(result.userpoints.first(), entry);
  else if(moType == map::USERPOINTROUTE)
    entryFromUserPos(userPos, entry, QString(), QString(), QString());
  else
    qWarning() << "Unknown Map object type" << moType;
}

void FlightplanEntryBuilder::buildFlightplanEntry(const map::MapSearchResult& result,
                                                  atools::fs::pln::FlightplanEntry& entry,
                                                  bool resolveWaypoints)
{
  buildFlightplanEntry(atools::geo::EMPTY_POS, result, entry, resolveWaypoints, map::NONE);
}

void FlightplanEntryBuilder::buildFlightplanEntry(const proc::MapProcedureLeg& leg,
                                                  atools::fs::pln::FlightplanEntry& entry,
                                                  bool resolveWaypoints)
{
  if(leg.navaids.hasWaypoints())
    entryFromWaypoint(leg.navaids.waypoints.first(), entry, resolveWaypoints);
  else if(leg.navaids.hasVor())
    entryFromVor(leg.navaids.vors.first(), entry);
  else if(leg.navaids.hasNdb())
    entryFromNdb(leg.navaids.ndbs.first(), entry);
  else
    entryFromUserPos(leg.line.getPos1(), entry, leg.fixIdent, leg.fixRegion, QString());

  // Do not save procedure legs
  entry.setFlag(atools::fs::pln::entry::PROCEDURE);
}
