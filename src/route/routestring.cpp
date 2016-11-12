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

RouteString::RouteString(MapQuery *mapQuery)
  : query(mapQuery)
{
  entryBuilder = new FlightplanEntryBuilder(query);
}

RouteString::~RouteString()
{
  delete entryBuilder;
}

// EDDF SID RID Y163 ODAVU Z114 RIDSU UL607 UTABA UM738 TIRUL Y740 NATAG UM738 ADOSA UP131 LUMAV UM726 GAVRA UY345 RITEB STAR LIRF
// EDDF DCT RIDSU UL607 UTABA UM738 TIRUL Y740 NATAG UM738 AMTEL UM727 TAQ DCT LIRF
// Ident;Region;Name;Airway;Type;Freq. MHz/kHz;Range nm;Course °M;Direct °M;Distance nm;Remaining nm;Leg Time hh:mm;ETA hh:mm UTC
// LOWI;;Innsbruck;;;;;;;;558,9;;0:00
// NORIN;LO;;;;;;12;12;7,8;551,0;0:04;0:04
// ALGOI;LO;;UT23;;;;275;274;36,7;514,3;0:22;0:26
// BAMUR;LS;;UN871;;;;278;278;38,8;475,5;0:23;0:49
// DINAR;LS;;Z2;;;;267;266;20,5;455,0;0:12;1:02
// KUDES;LS;;Z2;;;;266;266;7,3;447,7;0:04;1:06
// DITON;LS;;UN871;;;;238;238;25,0;422,7;0:15;1:21
// SUREP;LS;;UN871;;;;237;237;15,5;407,2;0:09;1:31
// BERSU;LS;;UN871;;;;237;237;3,4;403,8;0:02;1:33
// ROTOS;LS;;Z55;;;;290;290;9,4;394,4;0:05;1:38
// BADEP;LS;;UZ669;;;;231;231;15,7;378,7;0:09;1:48
// ULMES;LS;;UZ669;;;;231;231;6,9;371,8;0:04;1:52
// VADAR;LS;;UZ669;;;;231;231;28,5;343,3;0:17;2:09
// MILPA;LF;;UZ669;;;;239;239;41,9;301,4;0:25;2:34
// MOKIP;LF;;UL612;;;;286;285;33,7;267,7;0:20;2:54
// PODEP;LF;;UL612;;;;286;285;49,9;217,8;0:29;3:24
// MOU;LF;Moulins;UL612;VORDME (H);116,70;195;285;285;12,4;205,4;0:07;3:32
// KUKOR;LF;;UM129;;;;245;245;23,0;182,4;0:13;3:45
// LARON;LF;;UM129;;;;245;245;42,9;139,5;0:25;4:11
// GUERE;LF;;UM129;;;;245;245;5,1;134,4;0:03;4:14
// BEBIX;LF;;UM129;;;;245;245;32,2;102,3;0:19;4:33
// LMG;LF;Limoges;UM129;VORDME (H);114,50;195;245;245;17,6;84,7;0:10;4:44
// DIBAG;LF;;UN460;;;;262;262;10,1;74,6;0:06;4:50
// FOUCO;LF;;UN460;;;;262;262;13,0;61,6;0:07;4:58
// CNA;LF;Cognac;UN460;VORDME (H);114,65;195;263;262;33,7;27,9;0:20;5:18
// LFCY;;Medis;;;;;270;269;27,9;0,0;0:16;5:35
// LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
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
    errors.append(tr("Warning: SID and STAR are not supported. Ignoring."));

  bool direct = false;
  maptypes::MapAirway currentAirway;
  maptypes::MapSearchResult lastResult;
  atools::geo::Pos lastPos;

  for(int i = 0; i < list.size(); i++)
  {
    const QString& str = list.at(i);

    if(i == 0)
    {
      // Departure ==================
      maptypes::MapAirport departure;
      query->getAirportByIdent(departure, str);

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
        errors.append(tr("Departure airport %1 not found.").arg(str));
    }
    else if(i == list.size() - 1)
    {
      // Destination ==================
      maptypes::MapAirport destination;
      query->getAirportByIdent(destination, str);
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
        errors.append(tr("Destination airport %1 not found.").arg(str));
    }
    else
    {
      bool waypoint = (i % 2) == 0;
      qDebug() << "waypoint" << waypoint << "direct" << direct << str << currentAirway.name;

      if(waypoint)
      {
        // Reading waypoint string now ==================

        // Waypoint / VOR / NDB ==================
        maptypes::MapSearchResult result;
        QList<maptypes::MapWaypoint> airwayWaypoints;
        QList<maptypes::MapAirwayWaypoint> allAirwayWaypoints;

        if(!direct)
        {
          // Airway
          query->getWaypointsForAirway(result.waypoints, currentAirway.name, str);
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
              for(int idx = 0; idx < allAirwayWaypoints.size(); idx++)
              {
                qDebug() << "found idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
                         << "in" << currentAirway.name;

                if(lastId == allAirwayWaypoints.at(idx).waypoint.id)
                  startIndex = idx;
                if(curId == allAirwayWaypoints.at(idx).waypoint.id)
                  endIndex = idx;
              }

              qDebug() << "startidx" << startIndex << "endidx" << endIndex;
              if(startIndex != -1 && endIndex != -1)
              {
                if(startIndex < endIndex)
                {
                  for(int idx = startIndex + 1; idx <= endIndex; idx++)
                  {
                    qDebug() << "adding forward idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
                             << "to" << currentAirway.name;
                    airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
                  }
                }
                else if(startIndex > endIndex)
                {
                  // Reversed order
                  for(int idx = startIndex - 1; idx >= endIndex; idx--)
                  {
                    qDebug() << "adding reverse idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
                             << "to" << currentAirway.name;
                    airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
                  }
                }
              }
              else
                errors.append(tr("Waypoints not found in airway %1. Ignoring.").arg(str));
            }
            else
              errors.append(tr("No flight plan waypoints found at airway %1. Ignoring.").arg(str));
          }
          else
            errors.append(tr("No waypoints found at airway %1. Ignoring.").arg(str));
        }
        else
        {
          query->getMapObjectByIdent(result, maptypes::WAYPOINT, str);
          maptools::removeFarthest(lastPos, result.waypoints);

          query->getMapObjectByIdent(result, maptypes::VOR, str);
          maptools::removeFarthest(lastPos, result.vors);

          query->getMapObjectByIdent(result, maptypes::NDB, str);
          maptools::removeFarthest(lastPos, result.ndbs);

          // query->getMapObjectByIdent(result, maptypes::AIRPORT, str);
          // maptools::removeFarthest(lastPos, result.airports);
        }

        // All all airway waypoints if any were found
        FlightplanEntry entry;
        for(const maptypes::MapWaypoint& wp : airwayWaypoints)
        {
          // Will fetch objects in order: airport, waypoint, vor, ndb
          entryBuilder->entryFromWaypoint(wp, entry);
          if(entry.getPosition().isValid())
          {
            entry.setAirway(currentAirway.name);
            flightplan.getEntries().append(entry);
          }
          else
            errors.append(tr("No navaid found for %1 on airway %2. Ignoring.").
                          arg(str).arg(currentAirway.name));
        }

        // Remember result before it is replaced by VOR and NDB
        lastResult = result;

        if(airwayWaypoints.isEmpty())
        {
          // Add a single waypoint from direct
          entry = FlightplanEntry();
          entryBuilder->buildFlightplanEntry(result, entry, true);
          if(entry.getPosition().isValid())
            flightplan.getEntries().append(entry);
          else
            errors.append(tr("No navaid found for %1. Ignoring.").arg(str));
        }

        lastPos = entry.getPosition();
        direct = false;
      }
      else
      {
        // Reading airway string now ==================
        if(str == "SID" || str == "STAR" || str == "DCT" || str == "DIRECT")
          // Direct connection ==================
          direct = true;
        else
        {
          // Airway connection ==================
          maptypes::MapSearchResult result;
          query->getMapObjectByIdent(result, maptypes::AIRWAY, str);

          if(result.airways.isEmpty())
          {
            errors.append(tr("Airway %1 not found. Using direct.").arg(str));
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
