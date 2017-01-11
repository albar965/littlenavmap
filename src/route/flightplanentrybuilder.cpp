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

#include "route/flightplanentrybuilder.h"

#include "fs/pln/flightplan.h"
#include "fs/pln/flightplanentry.h"
#include "mapgui/mapquery.h"

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;

FlightplanEntryBuilder::FlightplanEntryBuilder(MapQuery *mapQuery)
  : query(mapQuery)
{

}

FlightplanEntryBuilder::~FlightplanEntryBuilder()
{

}

/* Copy airport attributes to flight plan entry */
void FlightplanEntryBuilder::buildFlightplanEntry(const maptypes::MapAirport& airport, FlightplanEntry& entry)
{
  entry.setIcaoIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(atools::fs::pln::entry::AIRPORT);
  entry.setWaypointId(entry.getIcaoIdent());
}

/* create a flight plan entry from object id/type or user position */
void FlightplanEntryBuilder::buildFlightplanEntry(int id, const atools::geo::Pos& userPos,
                                                  maptypes::MapObjectTypes type, FlightplanEntry& entry,
                                                  bool resolveWaypoints, int& curUserpointNumber)
{
  maptypes::MapSearchResult result;
  query->getMapObjectById(result, type, id);
  buildFlightplanEntry(userPos, result, entry, resolveWaypoints, curUserpointNumber, maptypes::NONE);
}

/* create a flight plan entry from object id/type or user position */
void FlightplanEntryBuilder::entryFromUserPos(const atools::geo::Pos& userPos, FlightplanEntry& entry,
                                              int& curUserpointNumber)
{
  entry.setPosition(userPos);
  entry.setWaypointType(atools::fs::pln::entry::USER);
  entry.setIcaoIdent(QString());
  entry.setWaypointId("WP" + QString::number(curUserpointNumber++));
}

void FlightplanEntryBuilder::entryFromNdb(const maptypes::MapNdb& ndb, FlightplanEntry& entry)
{
  entry.setIcaoIdent(ndb.ident);
  entry.setPosition(ndb.position);
  entry.setIcaoRegion(ndb.region);
  entry.setWaypointType(atools::fs::pln::entry::NDB);
  entry.setWaypointId(entry.getIcaoIdent());
}

void FlightplanEntryBuilder::entryFromVor(const maptypes::MapVor& vor, FlightplanEntry& entry)
{
  entry.setIcaoIdent(vor.ident);
  entry.setPosition(vor.position);
  entry.setIcaoRegion(vor.region);
  entry.setWaypointType(atools::fs::pln::entry::VOR);
  entry.setWaypointId(entry.getIcaoIdent());
}

void FlightplanEntryBuilder::entryFromAirport(const maptypes::MapAirport& airport, FlightplanEntry& entry)
{
  entry.setIcaoIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(atools::fs::pln::entry::AIRPORT);
  entry.setWaypointId(entry.getIcaoIdent());
}

bool FlightplanEntryBuilder::vorForWaypoint(const maptypes::MapWaypoint& waypoint, maptypes::MapVor& vor)
{
  query->getVorForWaypoint(vor, waypoint.id);

  // Check for invalid references that are caused by the navdata update
  // Check pole to avoid FSAD bugs
  return !vor.ident.isEmpty() && vor.position.isValid() && !vor.position.isPole() &&
         vor.position.almostEqual(waypoint.position, atools::geo::Pos::POS_EPSILON_10M);
}

bool FlightplanEntryBuilder::ndbForWaypoint(const maptypes::MapWaypoint& waypoint, maptypes::MapNdb& ndb)
{
  query->getNdbForWaypoint(ndb, waypoint.id);

  // Check for invalid references that are caused by the navdata update
  // Check pole to avoid FSAD bugs
  return !ndb.ident.isEmpty() && ndb.position.isValid() && !ndb.position.isPole() &&
         ndb.position.almostEqual(waypoint.position, atools::geo::Pos::POS_EPSILON_10M);

}

void FlightplanEntryBuilder::entryFromWaypoint(const maptypes::MapWaypoint& waypoint, FlightplanEntry& entry,
                                               bool resolveWaypoints)
{
  bool useWaypoint = true;
  maptypes::MapVor vor;
  maptypes::MapNdb ndb;

  if(resolveWaypoints && waypoint.type == "VOR")
  {
    // Convert waypoint to underlying VOR for airway routes
    if(vorForWaypoint(waypoint, vor))
    {
      useWaypoint = false;
      entryFromVor(vor, entry);
    }
  }
  else if(resolveWaypoints && waypoint.type == "NDB")
  {
    // Convert waypoint to underlying NDB for airway routes

    // Workaround for source data error - wrongly assigned VOR waypoints that are assigned to NDBs
    query->getVorNearest(vor, waypoint.position);
    if(!vor.dmeOnly && !vor.ident.isEmpty() && vor.position.isValid() && !vor.position.isPole() &&
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
    entry.setIcaoIdent(waypoint.ident);
    entry.setPosition(waypoint.position);
    entry.setIcaoRegion(waypoint.region);
    entry.setWaypointType(atools::fs::pln::entry::INTERSECTION);
    entry.setWaypointId(entry.getIcaoIdent());
  }
}

void FlightplanEntryBuilder::buildFlightplanEntry(const atools::geo::Pos& userPos,
                                                  const maptypes::MapSearchResult& result,
                                                  FlightplanEntry& entry,
                                                  bool resolveWaypoints, int& curUserpointNumber,
                                                  maptypes::MapObjectTypes type)
{
  maptypes::MapObjectTypes moType = type;

  if(moType == maptypes::NONE)
  {
    if(!result.airports.isEmpty())
      moType = maptypes::AIRPORT;
    else if(!result.waypoints.isEmpty())
      moType = maptypes::WAYPOINT;
    else if(!result.vors.isEmpty())
      moType = maptypes::VOR;
    else if(!result.ndbs.isEmpty())
      moType = maptypes::NDB;
    else if(userPos.isValid())
      moType = maptypes::USER;
  }

  if(moType == maptypes::AIRPORT)
    entryFromAirport(result.airports.first(), entry);
  else if(moType == maptypes::WAYPOINT)
    entryFromWaypoint(result.waypoints.first(), entry, resolveWaypoints);
  else if(moType == maptypes::VOR)
    entryFromVor(result.vors.first(), entry);
  else if(moType == maptypes::NDB)
    entryFromNdb(result.ndbs.first(), entry);
  else if(moType == maptypes::USER)
    entryFromUserPos(userPos, entry, curUserpointNumber);
  else
    qWarning() << "Unknown Map object type" << moType;
}

void FlightplanEntryBuilder::buildFlightplanEntry(const maptypes::MapSearchResult& result,
                                                  atools::fs::pln::FlightplanEntry& entry,
                                                  bool resolveWaypoints)
{
  int userWaypointDummy = -1;
  buildFlightplanEntry(atools::geo::EMPTY_POS, result, entry, resolveWaypoints, userWaypointDummy);
}
