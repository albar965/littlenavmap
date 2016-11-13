/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "route/routestring.h"

#include "route/routemapobjectlist.h"
#include "mapgui/mapquery.h"
#include "route/flightplanentrybuilder.h"
#include "common/maptools.h"

#include <QRegularExpression>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;

const int MAX_WAYPOINT_DISTANCE_NM = 5000;

RouteString::RouteString(MapQuery *mapQuery)
  : query(mapQuery)
{
  entryBuilder = new FlightplanEntryBuilder(query);
}

RouteString::~RouteString()
{
  delete entryBuilder;
}

QString RouteString::createStringForRoute(const atools::fs::pln::Flightplan& flightplan)
{
  QStringList retval;
  QString lastAw, lastId;
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    const FlightplanEntry& entry = flightplan.at(i);
    const QString& airway = entry.getAirway();
    const QString& ident = entry.getIcaoIdent();

    if(airway.isEmpty())
    {
      if(!lastId.isEmpty())
        retval.append(lastId);
      if(i > 0)
        // Add direct if not start airport
        retval.append("DCT");
    }
    else
    {
      if(airway != lastAw)
      {
        // Airways change add last ident of the last airway
        retval.append(lastId);
        retval.append(airway);
      }
      // else Airway is the same skip waypoint
    }

    lastId = ident;
    lastAw = airway;
  }

  // Add destination
  retval.append(lastId);

  return retval.join(" ");
}

void RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan)
{
  errors.clear();
  QStringList list = cleanRouteString(routeString).split(" ");
  qDebug() << "parsing" << list;

  if(list.size() < 2)
  {
    errors.append(tr("Flight plan string is too small. Need at least departure and destination."));
    return;
  }

  if(list.size() > 2 && (list.at(1) == "SID" || list.at(1) == "STAR" ||
                         list.at(list.size() - 2) == "SID" || list.at(list.size() - 2) == "STAR"))
    errors.append(tr("SID and STAR are not supported. Replacing with direct (DCT)."));

  bool direct = false;
  maptypes::MapAirway currentAirway;
  maptypes::MapSearchResult lastResult;
  atools::geo::Pos lastPos;

  for(int i = 0; i < list.size(); i++)
  {
    const QString& strItem = list.at(i);

    if(i == 0)
      // Departure ==================
      addDeparture(flightplan, lastPos, strItem);
    else if(i == list.size() - 1)
      // Destination ==================
      addDestination(flightplan, strItem);
    else
    {
      bool waypoint = (i % 2) == 0;
      qDebug() << "waypoint" << waypoint << "direct" << direct << currentAirway.name << "->" << strItem;

      if(waypoint)
      {
        // Reading waypoint string now ==================

        // Waypoint / VOR / NDB ==================
        maptypes::MapSearchResult result;
        QList<maptypes::MapWaypoint> airwayWaypoints;
        QList<maptypes::MapAirwayWaypoint> allAirwayWaypoints;

        if(!direct)
        {
          if(!lastResult.waypoints.isEmpty())
          {
            // Airway
            query->getWaypointsForAirway(result.waypoints, currentAirway.name, strItem);
            maptools::removeFarthest(lastPos, result.waypoints);

            if(!result.waypoints.isEmpty())
            {
              // List fragment and sequence
              query->getWaypointListForAirwayName(allAirwayWaypoints, currentAirway.name);

              if(!allAirwayWaypoints.isEmpty())
              {
                int lastId = lastResult.waypoints.first().id;
                int curId = result.waypoints.first().id;

                int startIndex = -1, endIndex = -1;
                findIndexesInAirway(allAirwayWaypoints, curId, lastId, startIndex, endIndex);

                qDebug() << "startidx" << startIndex << "endidx" << endIndex;
                if(startIndex != -1 && endIndex != -1)
                  extractWaypoints(allAirwayWaypoints, startIndex, endIndex, airwayWaypoints);
                else
                {
                  if(startIndex == -1)
                    errors.append(tr("Waypoint %1 not found in airway %2. Ignoring.").
                                  arg(lastResult.waypoints.first().ident).
                                  arg(currentAirway.name));

                  if(endIndex == -1)
                    errors.append(tr("Waypoint %1 not found in airway %2. Ignoring.").
                                  arg(result.waypoints.first().ident).
                                  arg(currentAirway.name));

                  direct = true;
                }
              }
              else
              {
                errors.append(tr("No waypoints found for airway %1. Ignoring.").
                              arg(currentAirway.name));
                direct = true;
              }
            }
            else
            {
              errors.append(tr("No waypoint %1 found at airway %2. Ignoring.").
                            arg(strItem).arg(currentAirway.name));
              direct = true;
            }
          }
          else
            direct = true;
        }

        if(direct)
        {
          // Direct connection as requested by DCT or by missing airway
          airwayWaypoints.clear();
          findBestNavaid(strItem, lastPos, result);
        }

        // All  airway waypoints if any were found
        FlightplanEntry entry;
        for(const maptypes::MapWaypoint& wp : airwayWaypoints)
        {
          // Reset but keep the last one
          entry = FlightplanEntry();

          // Will fetch objects in order: airport, waypoint, vor, ndb
          entryBuilder->entryFromWaypoint(wp, entry, true);
          if(entry.getPosition().isValid())
          {
            entry.setAirway(currentAirway.name);
            flightplan.getEntries().append(entry);
          }
          else
            qDebug() << "No navaid found for" << strItem << "on airway" << currentAirway.name;
        }

        // Replace last result only if something was found
        if(!result.waypoints.isEmpty() || !result.vors.isEmpty() || !result.ndbs.isEmpty())
          lastResult = result;

        if(airwayWaypoints.isEmpty())
        {
          // Add a single waypoint from direct
          entry = FlightplanEntry();
          entryBuilder->buildFlightplanEntry(result, entry, true);
          if(entry.getPosition().isValid())
            flightplan.getEntries().append(entry);
          else
            errors.append(tr("No navaid found for %1. Ignoring.").arg(strItem));
        }

        if(entry.getPosition().isValid())
          // Remember position for distance calculation
          lastPos = entry.getPosition();
        direct = false;
      }
      else
      {
        // Reading airway string now ==================
        if(strItem == "SID" || strItem == "STAR" || strItem == "DCT" || strItem == "DIRECT")
          // Direct connection ==================
          direct = true;
        else
        {
          // Airway connection ==================
          maptypes::MapSearchResult result;
          query->getMapObjectByIdent(result, maptypes::AIRWAY, strItem);

          if(result.airways.isEmpty())
          {
            errors.append(tr("Airway %1 not found. Using direct.").arg(strItem));
            direct = true;
          }
          else
          {
            currentAirway = result.airways.first();
            direct = false;
          }
        }
      }
    }
  }
}

QString RouteString::cleanRouteString(const QString& string)
{
  QString retval(string);

  retval.replace(QRegularExpression("[^A-Za-z0-9]"), " ");

  return retval.simplified().toUpper();
}

void RouteString::addDeparture(atools::fs::pln::Flightplan& flightplan, atools::geo::Pos& lastPos,
                               const QString& airportIdent)
{
  maptypes::MapAirport departure;
  query->getAirportByIdent(departure, airportIdent);

  if(departure.position.isValid())
  {
    qDebug() << "found" << departure.ident << "id" << departure.id;

    flightplan.setDepartureAiportName(departure.name);
    flightplan.setDepartureIdent(departure.ident);
    flightplan.setDeparturePosition(departure.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry);
    flightplan.getEntries().append(entry);
    lastPos = entry.getPosition();
  }
  else
    errors.append(tr("Departure airport %1 not found. Ignoring.").arg(airportIdent));
}

void RouteString::addDestination(atools::fs::pln::Flightplan& flightplan, const QString& airportIdent)
{
  maptypes::MapAirport destination;
  query->getAirportByIdent(destination, airportIdent);
  if(destination.position.isValid())
  {
    qDebug() << "found" << destination.ident << "id" << destination.id;

    flightplan.setDestinationAiportName(destination.name);
    flightplan.setDestinationIdent(destination.ident);
    flightplan.setDestinationPosition(destination.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(destination, entry);
    flightplan.getEntries().append(entry);
  }
  else
    errors.append(tr("Destination airport %1 not found. Ignoring.").arg(airportIdent));
}

void RouteString::findIndexesInAirway(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                                      int curId, int lastId, int& startIndex, int& endIndex)
{
  // TODO handle fragments properly
  for(int idx = 0; idx < allAirwayWaypoints.size(); idx++)
  {
    // qDebug() << "found idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
    // << "in" << currentAirway.name;

    if(startIndex == -1 && lastId == allAirwayWaypoints.at(idx).waypoint.id)
      startIndex = idx;
    if(endIndex == -1 && curId == allAirwayWaypoints.at(idx).waypoint.id)
      endIndex = idx;
  }
}

void RouteString::extractWaypoints(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                                   int startIndex, int endIndex,
                                   QList<maptypes::MapWaypoint>& airwayWaypoints)
{
  if(startIndex < endIndex)
  {
    for(int idx = startIndex + 1; idx <= endIndex; idx++)
    {
      // qDebug() << "adding forward idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
      // << "to" << currentAirway.name;
      airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
    }
  }
  else if(startIndex > endIndex)
  {
    // Reversed order
    for(int idx = startIndex - 1; idx >= endIndex; idx--)
    {
      // qDebug() << "adding reverse idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
      // << "to" << currentAirway.name;
      airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
    }
  }
}

void RouteString::findBestNavaid(const QString& strItem, const atools::geo::Pos& lastPos,
                                 maptypes::MapSearchResult& result)
{
  query->getMapObjectByIdent(result, maptypes::WAYPOINT, strItem);
  float waypointDistNm = atools::geo::meterToNm(maptools::removeFarthest(lastPos, result.waypoints));

  query->getMapObjectByIdent(result, maptypes::VOR, strItem);
  float vorDistNm = atools::geo::meterToNm(maptools::removeFarthest(lastPos, result.vors));

  query->getMapObjectByIdent(result, maptypes::NDB, strItem);
  float ndbDistNm = atools::geo::meterToNm(maptools::removeFarthest(lastPos, result.ndbs));

  if(waypointDistNm > MAX_WAYPOINT_DISTANCE_NM)
  {
    if(vorDistNm > MAX_WAYPOINT_DISTANCE_NM)
    {
      if(ndbDistNm < MAX_WAYPOINT_DISTANCE_NM)
      {
        result.waypoints.clear();
        result.vors.clear();
      }
    }
    result.waypoints.clear();
    // errors.append(tr("Distance to waypoint %1 > %2 nm. Ignoring.").
    // arg(strItem).arg(MAX_WAYPOINT_DISTANCE_NM));
  }
  // query->getMapObjectByIdent(result, maptypes::AIRPORT, str);
  // maptools::removeFarthest(lastPos, result.airports);
}
