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

#include "common/coordinates.h"
#include "route/routemapobjectlist.h"
#include "mapgui/mapquery.h"
#include "route/flightplanentrybuilder.h"
#include "common/maptools.h"

#include <QRegularExpression>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using maptypes::MapSearchResult;

const static int MAX_WAYPOINT_DISTANCE_NM = 5000;

const static QRegularExpression SPDALT("^[NMK]\\d{3,4}[FSAM]\\d{3,4}$");
const static QRegularExpression SPDALT_WAYPOINT("^([A-Z0-9]+)/[NMK]\\d{3,4}[FSAM]\\d{3,4}$");
const static QRegularExpression AIRPORT_TIME("^([A-Z0-9]{3,4})\\d{4}$");
const static QRegularExpression SIDSTAR_TRANS("^[A-Z_0-9]+(\\.[A-Z_0-9]+)+$");

const static maptypes::MapObjectTypes ROUTE_TYPES(maptypes::AIRPORT | maptypes::WAYPOINT |
                                                  maptypes::VOR | maptypes::NDB | maptypes::USER |
                                                  maptypes::AIRWAY);

RouteString::RouteString(FlightplanEntryBuilder *flightplanEntryBuilder)
  : entryBuilder(flightplanEntryBuilder)
{
  if(flightplanEntryBuilder != nullptr)
    query = flightplanEntryBuilder->getMapQuery();
}

RouteString::~RouteString()
{
}

/*
 *  Flight Plan File Guidelines
 *
 * Garmin uses a text based flight plan format that is derived from the IMI/IEI
 * messages format specified in ARINC 702A-3. But that’s just a side note.
 * Let’s take a look at the syntax of a usual Garmin flight plan:
 * FPN/RI:F:AIRPORT:F:WAYPOINT:F:WAYPOINT.AIRWAY.WAYPOINT:F:AIRPORT
 * Every flight plan always starts with the “FPN/RI” identifier. The “:F:” specifies
 * the different flight plan segments. A flight plan segment can be the departure or arrival
 * airport, a waypoint or a number of waypoints that are connected via airways.
 *
 * The entry and exit waypoint of an airway are connected to the airway via a dot “.”.
 * The flight plan must be contained in the first line of the file. Anything after the first
 * line will be discarded and may result in importing failures. Flight plans can only contain
 * upper case letters, numbers, colons, parenthesis, commas and periods. Spaces or any other
 * special characters are not allowed. When saved the flight plan name must have a “.gfp” extension.
 *
 * Here's an example, it's basically a .txt file with the extension .gfp
 *
 * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
 *
 * FPN/RI:F:KTEB:F:LGA:F:JFK:F:HOFFI:F:HTO:F:MONTT:F:OFTUR:F:KMVY
 */
QString RouteString::createGfpStringForRoute(const RouteMapObjectList& route)
{
  QString retval;

  QStringList string = createStringForRouteInternal(route, true);
  if(!string.isEmpty())
  {
    retval += "FPN/RI:F:" + string.first();

    if(string.size() > 2)
    {
      for(int i = 1; i < string.size() - 1; ++i)
      {
        const QString& str = string.at(i);
        if((i % 2) == 0)
          // Is a waypoint
          retval += str;
        else
        {
          // Is a airway or direct
          if(str == "DCT")
            retval += ":F:";
          else
            retval += "." + str + ".";
        }
      }
    }
    else
      retval += ":F:";

    retval += string.last();
  }
  qDebug() << "RouteString::createGfpStringForRoute" << retval;
  return retval.toUpper();
}

QString RouteString::createStringForRoute(const RouteMapObjectList& route)
{
  return createStringForRouteInternal(route, false).join(" ").simplified().toUpper();
}

QStringList RouteString::createStringForRouteInternal(const RouteMapObjectList& route, bool gfpWaypoints)
{
  QStringList retval;
  QString lastAw, lastId;
  for(int i = 0; i < route.size(); i++)
  {
    const RouteMapObject& entry = route.at(i);
    const QString& airway = entry.getAirway();
    QString ident = entry.getIdent();

    if(entry.getMapObjectType() == maptypes::INVALID || entry.getMapObjectType() == maptypes::USER)
      // CYCD DCT DUNCN V440 YYJ V495 CDGPN DCT N48194W123096 DCT WATTR V495 JAWBN DCT 0S9
      ident = gfpWaypoints ?
              coords::toGfpFormat(entry.getPosition()) : coords::toDegMinFormat(entry.getPosition());

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
  qDebug() << "RouteString::createStringForRouteInternal" << retval;

  return retval;
}

bool RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan)
{
  qDebug() << Q_FUNC_INFO;
  errors.clear();
  QStringList items = cleanRouteString(routeString).split(" ");

  qDebug() << "items" << items;

  // Remove all unneded adornment like speed and times and also combine waypoint coordinate pairs
  QStringList cleanItems = cleanItemList(items);
  qDebug() << "clean items" << cleanItems;

  if(cleanItems.isEmpty())
  {
    appendMessage(tr("Nothing found after cleaning up route."));
    return false;
  }

  if(!addDeparture(flightplan, cleanItems.first()))
    return false;

  int maxDistance =
    atools::geo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM,
                                    atools::roundToInt(flightplan.getDistanceNm() /* * 1.5f*/)));

  // Collect all navaids, airports and coordinates
  atools::geo::Pos lastPos(flightplan.getDeparturePosition());
  QList<ParseEntry> resultList;
  for(int i = 1; i < cleanItems.size(); i++)
  {
    QString item = cleanItems.at(i);
    MapSearchResult result;
    findWaypoints(result, item);

    // Sort lists by distance and remove all which are too far away and update last pos
    filterWaypoints(result, lastPos, maxDistance);

    if(!result.isEmpty(ROUTE_TYPES))
      resultList.append({item, QString(), result});
    else
      appendMessage(tr("Nothing found for %1. Ignoring.").arg(item));
  }

  // Create airways - will fill the waypoint list in result with airway points
  // if airway is invalid it will be erased in result
  for(int i = 1; i < resultList.size() - 1; i++)
    filterAirways(resultList, i);

  // Remove now unneeded results after airway evaluation
  removeEmptyResults(resultList);

  // Now build the flight plan
  for(int i = 0; i < resultList.size(); i++)
  {
    const QString& item = resultList.at(i).item;
    MapSearchResult& result = resultList[i].result;

    if(!result.airways.isEmpty())
    {
      // Add all airway waypoints if any were found
      for(const maptypes::MapWaypoint& wp : result.waypoints)
      {
        // Reset but keep the last one
        FlightplanEntry entry;

        // Will fetch objects in order: airport, waypoint, vor, ndb
        entryBuilder->entryFromWaypoint(wp, entry, true);

        if(entry.getPosition().isValid())
        {
          entry.setAirway(item);
          flightplan.getEntries().append(entry);
          qDebug() << entry.getIcaoIdent() << entry.getAirway();
        }
        else
          qDebug() << "No navaid found for" << wp.ident << "on airway" << item;
      }
    }
    else
    {
      // Add a single waypoint from direct
      FlightplanEntry entry;
      int userWaypointDummy = -1;
      if(!result.userPoints.isEmpty())
      {
        // Convert a coordinate to a user defined waypoint
        entryBuilder->buildFlightplanEntry(
          result.userPoints.first().position, result, entry, true, userWaypointDummy);

        // Use the original string as name
        entry.setWaypointId(item);
      }
      else
        entryBuilder->buildFlightplanEntry(result, entry, true);

      if(entry.getPosition().isValid())
      {
        // Assign airway if  this is the end point of an airway
        entry.setAirway(resultList.at(i).airway);
        flightplan.getEntries().append(entry);
      }
      else
        appendMessage(tr("No navaid found for %1. Ignoring.").arg(item));
      qDebug() << entry.getIcaoIdent() << entry.getAirway();
    }

    // Remember position for distance calculation
    lastPos = flightplan.getEntries().last().getPosition();
  }

  return true;
}

QString RouteString::cleanRouteString(const QString& string)
{
  QString retval = string.toUpper();
  retval.replace(QRegularExpression("[^A-Z0-9/\\.]"), " ");
  return retval.simplified();
}

void RouteString::appendMessage(const QString& err)
{
  errors.append(err);
  qInfo() << "Error:" << err;
}

bool RouteString::addDeparture(atools::fs::pln::Flightplan& flightplan, const QString& airportIdent)
{
  QString ident = airportIdent;
  QRegularExpressionMatch match = AIRPORT_TIME.match(ident);
  if(match.hasMatch())
  {
    ident = match.captured(1);
    appendMessage(tr("Ignoring time specification for airport %1.").arg(ident));
  }

  maptypes::MapAirport departure;
  query->getAirportByIdent(departure, ident);

  if(departure.position.isValid())
  {
    qDebug() << "found" << departure.ident << "id" << departure.id;

    flightplan.setDepartureAiportName(departure.name);
    flightplan.setDepartureIdent(departure.ident);
    flightplan.setDeparturePosition(departure.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry);
    flightplan.getEntries().append(entry);
    return true;
  }
  else
  {
    appendMessage(tr("Mandatory departure airport %1 not found. Reading stopped.").arg(airportIdent));
    return false;
  }
}

void RouteString::findIndexesInAirway(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                                      int lastId, int nextId, int& startIndex, int& endIndex)
{
  int startFragment = -1, endFragment = -1;
  // TODO handle fragments properly
  for(int idx = 0; idx < allAirwayWaypoints.size(); idx++)
  {
    const maptypes::MapAirwayWaypoint& wp = allAirwayWaypoints.at(idx);
    // qDebug() << "found idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident
    // << "in" << currentAirway.name;

    if(startIndex == -1 && lastId == wp.waypoint.id)
    {
      startIndex = idx;
      startFragment = wp.airwayFragmentId;
    }
    if(endIndex == -1 && nextId == wp.waypoint.id)
    {
      endIndex = idx;
      endFragment = wp.airwayFragmentId;
    }
  }

  if(startFragment != endFragment)
  {
    qDebug() << "Fragmented airway";
    startIndex = -1;
    endIndex = -1;
  }
}

/* Always excludes the first waypoint */
void RouteString::extractWaypoints(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                                   int startIndex, int endIndex,
                                   QList<maptypes::MapWaypoint>& airwayWaypoints)
{
  if(startIndex < endIndex)
  {
    for(int idx = startIndex + 1; idx < endIndex; idx++)
    {
      qDebug() << "adding forward idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident;
      airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
    }
  }
  else if(startIndex > endIndex)
  {
    // Reversed order
    for(int idx = startIndex - 1; idx > endIndex; idx--)
    {
      qDebug() << "adding reverse idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident;
      airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
    }
  }
}

void RouteString::filterWaypoints(MapSearchResult& result, atools::geo::Pos& lastPos, int maxDistance)
{
  maptools::sortByDistance(result.airports, lastPos);
  maptools::removeByDistance(result.airports, lastPos, maxDistance);

  maptools::sortByDistance(result.waypoints, lastPos);
  maptools::removeByDistance(result.waypoints, lastPos, maxDistance);

  maptools::sortByDistance(result.vors, lastPos);
  maptools::removeByDistance(result.vors, lastPos, maxDistance);

  maptools::sortByDistance(result.ndbs, lastPos);
  maptools::removeByDistance(result.ndbs, lastPos, maxDistance);

  maptools::sortByDistance(result.userPoints, lastPos);
  maptools::removeByDistance(result.userPoints, lastPos, maxDistance);

  if(!result.userPoints.isEmpty())
    // User points have preference since they can be clearly identified
    lastPos = result.userPoints.first().position;
  else if(!result.airports.isEmpty())
    lastPos = result.airports.first().position;
  else if(!result.waypoints.isEmpty())
    lastPos = result.waypoints.first().position;
  else if(!result.vors.isEmpty())
    lastPos = result.vors.first().position;
  else if(!result.ndbs.isEmpty())
    lastPos = result.ndbs.first().position;
}

void RouteString::filterAirways(QList<ParseEntry>& resultList, int i)
{
  if(i == 0 || i > resultList.size() - 1)
    return;

  MapSearchResult& result = resultList[i].result;

  if(!result.airways.isEmpty())
  {
    QList<maptypes::MapWaypoint> waypoints;
    MapSearchResult& lastResult = resultList[i - 1].result;
    MapSearchResult& nextResult = resultList[i + 1].result;
    const QString& airwayName = resultList.at(i).item;
    const QString& waypointStart = resultList.at(i - 1).item;

    if(lastResult.waypoints.isEmpty())
    {
      appendMessage(tr("No waypoint before airway %1. Ignoring flight plan segment.").
                    arg(airwayName));
      result.airways.clear();
      return;
    }

    if(nextResult.waypoints.isEmpty())
    {
      appendMessage(tr("No waypoint after airway %1. Ignoring flight plan segment.").
                    arg(airwayName));
      result.airways.clear();
      return;
    }

    // Get all airways that match name and waypoint - normally only one
    query->getWaypointsForAirway(waypoints, airwayName, waypointStart);

    if(!waypoints.isEmpty())
    {
      QList<maptypes::MapAirwayWaypoint> allAirwayWaypoints;

      // Get all waypoints for the airway sorted by fragment and sequence
      query->getWaypointListForAirwayName(allAirwayWaypoints, airwayName);

      if(!allAirwayWaypoints.isEmpty())
      {
        int startIndex = -1, endIndex = -1;

        // Find the waypoint indexes by id in the list of all waypoints for this airway
        findIndexesInAirway(allAirwayWaypoints, lastResult.waypoints.first().id,
                            nextResult.waypoints.first().id, startIndex, endIndex);

        qDebug() << "startidx" << startIndex << "endidx" << endIndex;
        if(startIndex != -1 && endIndex != -1)
        {
          // Get the list of waypoints from start to end index
          result.waypoints.clear();

          // Override airway for the next
          resultList[i + 1].airway = airwayName;
          extractWaypoints(allAirwayWaypoints, startIndex, endIndex, result.waypoints);
        }
        else
        {
          if(startIndex == -1)
            appendMessage(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(lastResult.waypoints.first().ident).
                          arg(airwayName));

          if(endIndex == -1)
            appendMessage(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(nextResult.waypoints.first().ident).
                          arg(airwayName));
          result.airways.clear();
        }
      }
      else
      {
        appendMessage(tr("No waypoints found for airway %1. Ignoring flight plan segment.").
                      arg(airwayName));
        result.airways.clear();
      }
    }
    else
    {
      appendMessage(tr("No waypoint %1 found at airway %2. Ignoring flight plan segment.").
                    arg(waypointStart).arg(airwayName));
      result.airways.clear();
    }
  }
}

void RouteString::findWaypoints(MapSearchResult& result, const QString& item)
{
  if(item.length() > 5)
  {
    // User defined waypoint
    atools::geo::Pos pos = coords::fromAnyWaypointFormat(item);
    if(pos.isValid())
    {
      maptypes::MapUserpoint user;
      user.position = pos;
      result.userPoints.append(user);
    }
  }
  else
  {
    query->getMapObjectByIdent(result, ROUTE_TYPES, item);

    if(item.length() == 5 && result.waypoints.isEmpty())
    {
      // Try NAT waypoint (a few of these are also in the database)
      atools::geo::Pos pos = coords::fromAnyWaypointFormat(item);
      if(pos.isValid())
      {
        maptypes::MapUserpoint user;
        user.position = pos;
        result.userPoints.append(user);
      }
    }
  }
}

QStringList RouteString::cleanItemList(const QStringList& items)
{
  QStringList cleanItems;
  for(int i = 0; i < items.size(); i++)
  {
    QString item = items.at(i);
    if(item == "DCT")
      continue;

    if(item == "SID")
    {
      appendMessage(tr("Replacing SID with DCT (direct)."));
      continue;
    }

    if(item == "STAR")
    {
      appendMessage(tr("Replacing STAR with DCT (direct)."));
      continue;
    }

    if(SPDALT.match(item).hasMatch())
    {
      appendMessage(tr("Ignoring speed and altitude %1.").arg(item));
      continue;
    }

    if(SIDSTAR_TRANS.match(item).hasMatch())
    {
      appendMessage(tr("Replacing SID/STAR/transition %1 with DCT (direct).").arg(item));
      continue;
    }

    QRegularExpressionMatch match = SPDALT_WAYPOINT.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendMessage(tr("Ignoring speed and altitude at waypoint %1.").arg(item));
    }

    match = AIRPORT_TIME.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendMessage(tr("Ignoring time for airport %1.").arg(item));
    }

    if(i < items.size() - 1)
    {
      QString itemPair = item + "/" + items.at(i + 1);
      if(coords::fromDegMinPairFormat(itemPair).isValid())
      {
        cleanItems.append(itemPair);
        i++;
        continue;
      }
    }

    cleanItems.append(item);
  }
  return cleanItems;
}

void RouteString::removeEmptyResults(QList<ParseEntry>& resultList)
{
  auto it = std::remove_if(resultList.begin(), resultList.end(),
                           [] (const ParseEntry &type)->bool
                           {
                             return type.result.isEmpty(ROUTE_TYPES);
                           });

  if(it != resultList.end())
    resultList.erase(it, resultList.end());
}
