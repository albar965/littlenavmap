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

#include "route/routestring.h"

#include "navapp.h"
#include "fs/util/coordinates.h"
#include "common/procedurequery.h"
#include "route/route.h"
#include "mapgui/mapquery.h"
#include "query/airportquery.h"
#include "route/flightplanentrybuilder.h"
#include "common/maptools.h"
#include "common/unit.h"

#include <QRegularExpression>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using map::MapSearchResult;
namespace coords = atools::fs::util;

const static int MAX_WAYPOINT_DISTANCE_NM = 1000;

const static QRegularExpression SPDALT("^([NMK])(\\d{3,4})([FSAM])(\\d{3,4})$");
const static QRegularExpression SPDALT_WAYPOINT("^([A-Z0-9]+)/[NMK]\\d{3,4}[FSAM]\\d{3,4}$");
const static QRegularExpression AIRPORT_TIME("^([A-Z0-9]{3,4})\\d{4}$");
const static QRegularExpression SID_STAR_TRANS("^([A-Z0-9]{1,6})(\\.([A-Z0-9]{1,6}))?$");

const static map::MapObjectTypes ROUTE_TYPES(map::AIRPORT | map::WAYPOINT |
                                             map::VOR | map::NDB | map::USER |
                                             map::AIRWAY);

const static QString SPANERR("<span style=\"color: #ff0000; font-weight:600\">");
const static QString SPANWARN("<span style=\"color: #ff3000\">");
const static QString SPANEND("</span>");

RouteString::RouteString(FlightplanEntryBuilder *flightplanEntryBuilder)
  : entryBuilder(flightplanEntryBuilder)
{
  mapQuery = NavApp::getMapQuery();
  airportQuery = NavApp::getAirportQuery();
  procQuery = NavApp::getProcedureQuery();
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
 * line will be discarded and may result i
 * n importing failures. Flight plans can only contain
 * upper case letters, numbers, colons, parenthesis, commas and periods. Spaces or any other
 * special characters are not allowed. When saved the flight plan name must have a “.gfp” extension.
 *
 * Here's an example, it's basically a .txt file with the extension .gfp
 *
 * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
 *
 * FPN/RI:F:KTEB:F:LGA:F:JFK:F:HOFFI:F:HTO:F:MONTT:F:OFTUR:F:KMVY
 */
QString RouteString::createGfpStringForRoute(const Route& route)
{
  QString retval;

  QStringList string = createStringForRouteInternal(route, 0, rs::GFP | rs::START_AND_DEST | rs::DCT);
  if(!string.isEmpty())
  {
    retval += "FPN/RI:F:" + string.first();

    if(string.size() > 2)
    {
      for(int i = 1; i < string.size() - 1; i++)
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
  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

QString RouteString::createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options)
{
  return createStringForRouteInternal(route, speed, options).join(" ").simplified().toUpper();
}

QStringList RouteString::createStringForRouteInternal(const Route& route, float speed, rs::RouteStringOptions options)
{
  QStringList retval;
  QString sid, sidTrans, star, starTrans;
  route.getSidStarNames(sid, sidTrans, star, starTrans);

  if(route.isEmpty())
    return retval;

  bool hasSid = ((options& rs::SID_STAR) && !sid.isEmpty()) || (options & rs::SID_STAR_GENERIC);
  bool hasStar = ((options& rs::SID_STAR) && !star.isEmpty()) || (options & rs::SID_STAR_GENERIC);

  bool firstDct = true;
  QString lastAw, lastId;
  for(int i = 0; i < route.size(); i++)
  {
    const RouteLeg& leg = route.at(i);
    if(leg.isAnyProcedure())
      continue;

    // Add departure airport
    if(i == 0 && leg.getMapObjectType() == map::AIRPORT && !(options & rs::START_AND_DEST))
      // Options is off
      continue;

    const QString& airway = leg.getAirwayName();
    QString ident = leg.getIdent();

    if(leg.getMapObjectType() == map::USER)
      // CYCD DCT DUNCN V440 YYJ V495 CDGPN DCT N48194W123096 DCT WATTR V495 JAWBN DCT 0S9
      ident = (options& rs::GFP) ? coords::toGfpFormat(leg.getPosition()) : coords::toDegMinFormat(leg.getPosition());

    if(airway.isEmpty() || leg.isAirwaySetAndInvalid() || options & rs::NO_AIRWAYS)
    {
      // Do not use  airway string if not found in database
      if(!lastId.isEmpty())
        retval.append(lastId);
      if(i > 0 && (options & rs::DCT))
      {
        if(!(firstDct && hasSid))
          // Add direct if not start airport - do not add where a SID is inserted
          retval.append("DCT");
        firstDct = false;
      }
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

  if(!retval.isEmpty())
  {
    if(hasStar && retval.last() == "DCT")
      // Remove last DCT so it does not collide with the STAR - destination airport not inserted yet
      retval.removeLast();

    int insertPosition = (route.hasValidDeparture() && options & rs::START_AND_DEST) ? 1 : 0;

    // Add SID
    if((options& rs::SID_STAR) && !sid.isEmpty())
      retval.insert(insertPosition, sid + (sidTrans.isEmpty() ? QString() : "." + sidTrans));
    else if(options & rs::SID_STAR_GENERIC)
      retval.insert(insertPosition, "SID");

    // Add speed and altitude
    if(!retval.isEmpty() && options & rs::ALT_AND_SPEED)
      retval.insert(insertPosition, createSpeedAndAltitude(speed, route.getCruisingAltitudeFeet()));

    // Add STAR
    if((options& rs::SID_STAR) && !star.isEmpty())
      retval.append(star + (starTrans.isEmpty() ? QString() : "." + starTrans));
    else if(options & rs::SID_STAR_GENERIC)
      retval.append("STAR");

    // Add destination airport
    if(options & rs::START_AND_DEST)
      retval.append(lastId);
  }

  qDebug() << Q_FUNC_INFO << retval;

  return retval;
}

bool RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan)
{
  float speeddummy;
  bool altdummy;
  return createRouteFromString(routeString, flightplan, speeddummy, altdummy);
}

bool RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan,
                                        float& speedKts, bool& altIncluded)
{
  qDebug() << Q_FUNC_INFO;
  messages.clear();
  QStringList items = cleanRouteString(routeString);

  qDebug() << "items" << items;

  // Remove all unneded adornment like speed and times and also combine waypoint coordinate pairs
  // Also extracts speed, altitude, SID and STAR
  float altitude;
  QStringList cleanItems = cleanItemList(items, speedKts, altitude);

  if(altitude > 0.f)
  {
    altIncluded = true;
    flightplan.setCruisingAltitude(atools::roundToInt(Unit::altFeetF(altitude)));
  }
  else
    altIncluded = false;

  qDebug() << "clean items" << cleanItems;

  if(cleanItems.size() < 2)
  {
    appendError(tr("Need at least departure and destination."));
    return false;
  }

  if(!addDeparture(flightplan, cleanItems))
    return false;

  if(!addDestination(flightplan, cleanItems))
    return false;

  if(speedKts > 0.f && altitude > 0.f)
    appendMessage(tr("Using <b>%1</b> and cruise altitude <b>%2</b>.").
                  arg(Unit::speedKts(speedKts)).arg(Unit::altFeet(altitude)));

  int maxDistance =
    atools::geo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM, atools::roundToInt(flightplan.getDistanceNm() * 1.5f)));

  // Collect all navaids, airports and coordinates
  atools::geo::Pos lastPos(flightplan.getDeparturePosition());
  QList<ParseEntry> resultList;
  for(const QString& item : cleanItems)
  {
    MapSearchResult result;
    findWaypoints(result, item);

    // Sort lists by distance and remove all which are too far away and update last pos
    filterWaypoints(result, lastPos, maxDistance);

    if(!result.isEmpty(ROUTE_TYPES))
      resultList.append({item, QString(), result});
    else
      appendWarning(tr("Nothing found for %1. Ignoring.").arg(item));
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
      for(const map::MapWaypoint& wp : result.waypoints)
      {
        // Reset but keep the last one
        FlightplanEntry entry;

        // Will fetch objects in order: airport, waypoint, vor, ndb
        entryBuilder->entryFromWaypoint(wp, entry, true);

        if(entry.getPosition().isValid())
        {
          entry.setAirway(item);
          flightplan.getEntries().insert(flightplan.getEntries().size() - 1, entry);
          // qDebug() << entry.getIcaoIdent() << entry.getAirway();
        }
        else
          qWarning() << "No navaid found for" << wp.ident << "on airway" << item;
      }
    }
    else
    {
      // Add a single waypoint from direct
      FlightplanEntry entry;
      if(!result.userPoints.isEmpty())
      {
        // Convert a coordinate to a user defined waypoint
        entryBuilder->buildFlightplanEntry(result.userPoints.first().position, result, entry, true);

        // Use the original string as name but limit it for fs
        entry.setWaypointId(item);
      }
      else
        entryBuilder->buildFlightplanEntry(result, entry, true);

      if(entry.getPosition().isValid())
      {
        // Assign airway if  this is the end point of an airway
        entry.setAirway(resultList.at(i).airway);
        flightplan.getEntries().insert(flightplan.getEntries().size() - 1, entry);
      }
      else
        appendWarning(tr("No navaid found for %1. Ignoring.").arg(item));
      // qDebug() << entry.getIcaoIdent() << entry.getAirway();
    }

    // Remember position for distance calculation
    lastPos = flightplan.getEntries().at(flightplan.getEntries().size() - 2).getPosition();
  }

  return true;
}

QStringList RouteString::cleanRouteString(const QString& string)
{
  QString cleanstr = string.toUpper();
  cleanstr.replace(QRegularExpression("[^A-Z0-9/\\.]"), " ");

  QStringList list = cleanstr.simplified().split(" ");
  list.removeAll(QString());
  return list;
}

void RouteString::appendMessage(const QString& message)
{
  messages.append(message);
  qInfo() << "Message:" << message;
}

void RouteString::appendWarning(const QString& message)
{
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(SPANWARN + message + SPANEND);
  qWarning() << "Warning:" << message;
}

void RouteString::appendError(const QString& message)
{
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(SPANERR + message + SPANEND);
  qWarning() << "Error:" << message;
}

bool RouteString::addDeparture(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems)
{
  QString ident = cleanItems.takeFirst();
  QRegularExpressionMatch match = AIRPORT_TIME.match(ident);
  if(match.hasMatch())
  {
    ident = match.captured(1);
    appendWarning(tr("Ignoring time specification for airport %1.").arg(ident));
  }

  map::MapAirport departure;
  airportQuery->getAirportByIdent(departure, ident);
  if(departure.isValid())
  {
    // qDebug() << "found" << departure.ident << "id" << departure.id;

    flightplan.setDepartureAiportName(departure.name);
    flightplan.setDepartureIdent(departure.ident);
    flightplan.setDeparturePosition(departure.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry);
    flightplan.getEntries().append(entry);

    if(!cleanItems.isEmpty())
    {
      QString sidTrans = cleanItems.first();

      bool foundSid = false;
      QRegularExpressionMatch sidMatch = SID_STAR_TRANS.match(sidTrans);
      if(sidMatch.hasMatch())
      {
        QString sid = sidMatch.captured(1);
        QString trans = sidMatch.captured(3);

        int sidTransId = -1;
        int sidId = procQuery->getSidId(departure, sid);
        if(sidId != -1 && !trans.isEmpty())
        {
          sidTransId = procQuery->getSidTransitionId(departure, trans, sidId);
          foundSid = sidTransId != -1;
        }
        else
          foundSid = sidId != -1;

        if(foundSid)
        {
          // Consume item
          cleanItems.removeFirst();

          proc::MapProcedureLegs arrivalLegs, starLegs, departureLegs;
          if(sidId != -1 && sidTransId == -1)
          {
            // Only SID
            const proc::MapProcedureLegs *legs = procQuery->getApproachLegs(departure, sidId);
            if(legs != nullptr)
              departureLegs = *legs;
          }
          else if(sidId != -1 && sidTransId != -1)
          {
            // SID and transition
            const proc::MapProcedureLegs *legs = procQuery->getTransitionLegs(departure, sidTransId);
            if(legs != nullptr)
              departureLegs = *legs;
          }

          // Add information to the flight plan property list
          procQuery->extractLegsForFlightplanProperties(
            flightplan.getProperties(), arrivalLegs, starLegs, departureLegs);
        }
      }
    }
    return true;
  }
  else
  {
    appendError(tr("Mandatory departure airport %1 not found.").arg(ident));
    return false;
  }
}

bool RouteString::addDestination(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems)
{
  QString ident = cleanItems.takeLast();
  QRegularExpressionMatch match = AIRPORT_TIME.match(ident);
  if(match.hasMatch())
  {
    ident = match.captured(1);
    appendWarning(tr("Ignoring time specification for airport %1.").arg(ident));
  }

  map::MapAirport destination;
  airportQuery->getAirportByIdent(destination, ident);
  if(destination.isValid())
  {
    // qDebug() << "found" << destination.ident << "id" << destination.id;

    flightplan.setDestinationAiportName(destination.name);
    flightplan.setDestinationIdent(destination.ident);
    flightplan.setDestinationPosition(destination.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(destination, entry);
    flightplan.getEntries().append(entry);

    if(!cleanItems.isEmpty())
    {
      QString starTrans = cleanItems.last();

      bool foundStar = false;
      QRegularExpressionMatch starMatch = SID_STAR_TRANS.match(starTrans);
      if(starMatch.hasMatch())
      {
        QString star = starMatch.captured(1);
        QString trans = starMatch.captured(3);

        int starTransId = -1;
        int starId = procQuery->getStarId(destination, star);
        if(starId != -1 && !trans.isEmpty())
        {
          starTransId = procQuery->getSidTransitionId(destination, trans, starId);
          foundStar = starTransId != -1;
        }
        else
          foundStar = starId != -1;

        if(foundStar)
        {
          // Consume item
          cleanItems.removeLast();

          proc::MapProcedureLegs arrivalLegs, starLegs, departureLegs;
          if(starId != -1 && starTransId == -1)
          {
            // Only STAR
            const proc::MapProcedureLegs *legs = procQuery->getApproachLegs(destination, starId);
            if(legs != nullptr)
              starLegs = *legs;
          }
          else if(starId != -1 && starTransId != -1)
          {
            // STAR and transition
            const proc::MapProcedureLegs *legs = procQuery->getTransitionLegs(destination, starTransId);
            if(legs != nullptr)
              starLegs = *legs;
          }
          // Add information to the flight plan property list
          procQuery->extractLegsForFlightplanProperties(
            flightplan.getProperties(), arrivalLegs, starLegs, departureLegs);
        }
      }
    }

    return true;
  }
  else
  {
    appendError(tr("Mandatory destination airport %1 not found.").arg(ident));
    return false;
  }
}

void RouteString::findIndexesInAirway(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints,
                                      int lastId, int nextId, int& startIndex, int& endIndex,
                                      const QString& airway)
{
  int startFragment = -1, endFragment = -1;
  // TODO handle fragments properly
  for(int idx = 0; idx < allAirwayWaypoints.size(); idx++)
  {
    const map::MapAirwayWaypoint& wp = allAirwayWaypoints.at(idx);
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
    appendWarning(tr("Found fragmented airway %1.").arg(airway));
    startIndex = -1;
    endIndex = -1;
  }
}

/* Always excludes the first waypoint */
void RouteString::extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints,
                                   int startIndex, int endIndex,
                                   QList<map::MapWaypoint>& airwayWaypoints)
{
  if(startIndex < endIndex)
  {
    for(int idx = startIndex + 1; idx < endIndex; idx++)
    {
      // qDebug() << "adding forward idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident;
      airwayWaypoints.append(allAirwayWaypoints.at(idx).waypoint);
    }
  }
  else if(startIndex > endIndex)
  {
    // Reversed order
    for(int idx = startIndex - 1; idx > endIndex; idx--)
    {
      // qDebug() << "adding reverse idx" << idx << allAirwayWaypoints.at(idx).waypoint.ident;
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
    QList<map::MapWaypoint> waypoints;
    MapSearchResult& lastResult = resultList[i - 1].result;
    MapSearchResult& nextResult = resultList[i + 1].result;
    const QString& airwayName = resultList.at(i).item;
    const QString& waypointStart = resultList.at(i - 1).item;

    if(lastResult.waypoints.isEmpty())
    {
      appendWarning(tr("No waypoint before airway %1. Ignoring flight plan segment.").arg(airwayName));
      result.airways.clear();
      return;
    }

    if(nextResult.waypoints.isEmpty())
    {
      appendWarning(tr("No waypoint after airway %1. Ignoring flight plan segment.").arg(airwayName));
      result.airways.clear();
      return;
    }

    // Get all waypoints
    mapQuery->getWaypointsForAirway(waypoints, airwayName, waypointStart);

    if(!waypoints.isEmpty())
    {
      QList<map::MapAirwayWaypoint> allAirwayWaypoints;

      // Get all waypoints for the airway sorted by fragment and sequence
      mapQuery->getWaypointListForAirwayName(allAirwayWaypoints, airwayName);

      if(!allAirwayWaypoints.isEmpty())
      {
        int startIndex = -1, endIndex = -1;

        // Find the waypoint indexes by id in the list of all waypoints for this airway
        findIndexesInAirway(allAirwayWaypoints, lastResult.waypoints.first().id,
                            nextResult.waypoints.first().id, startIndex, endIndex, airwayName);

        // qDebug() << "startidx" << startIndex << "endidx" << endIndex;
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
            appendWarning(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(lastResult.waypoints.first().ident).
                          arg(airwayName));

          if(endIndex == -1)
            appendWarning(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(nextResult.waypoints.first().ident).
                          arg(airwayName));
          result.airways.clear();
        }
      }
      else
      {
        appendWarning(tr("No waypoints found for airway %1. Ignoring flight plan segment.").
                      arg(airwayName));
        result.airways.clear();
      }
    }
    else
    {
      appendWarning(tr("No waypoint %1 found at airway %2. Ignoring flight plan segment.").
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
      map::MapUserpoint user;
      user.position = pos;
      result.userPoints.append(user);
    }
  }
  else
  {
    mapQuery->getMapObjectByIdent(result, ROUTE_TYPES, item);

    if(item.length() == 5 && result.waypoints.isEmpty())
    {
      // Try NAT waypoint (a few of these are also in the database)
      atools::geo::Pos pos = coords::fromAnyWaypointFormat(item);
      if(pos.isValid())
      {
        map::MapUserpoint user;
        user.position = pos;
        result.userPoints.append(user);
      }
    }
  }
}

QStringList RouteString::cleanItemList(const QStringList& items, float& speedKnots, float& altFeet)
{
  speedKnots = 0.f;
  altFeet = 0.f;

  QStringList cleanItems;
  for(int i = 0; i < items.size(); i++)
  {
    QString item = items.at(i);
    if(item == "DCT")
      continue;

    if(item == "SID")
    {
      appendWarning(tr("Replacing plain SID instruction with DCT (direct)."));
      continue;
    }

    if(item == "STAR")
    {
      appendWarning(tr("Replacing STAR instruction with DCT (direct)."));
      continue;
    }

    if(SPDALT.match(item).hasMatch())
    {
      if(!extractSpeedAndAltitude(item, speedKnots, altFeet))
        appendWarning(tr("Ignoring invalid speed and altitude instruction %1.").arg(item));
      continue;
    }

    QRegularExpressionMatch match = SPDALT_WAYPOINT.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendWarning(tr("Ignoring speed and altitude at waypoint %1.").arg(item));
    }

    match = AIRPORT_TIME.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendWarning(tr("Ignoring time for airport %1.").arg(item));
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
                           [](const ParseEntry& type) -> bool
  {
    return type.result.isEmpty(ROUTE_TYPES);
  });

  if(it != resultList.end())
    resultList.erase(it, resultList.end());
}

bool RouteString::extractSpeedAndAltitude(const QString& item, float& speedKnots, float& altFeet)
{
  // N0490F360
  // M084F330
  // Speed
  // K0800 (800 Kilometers)
  // N0490 (490 Knots)
  // M082 (Mach 0.82)
  // Level/altitude
  // F340 (Flight level 340)
  // S1260 (12600 Meters)
  // A100 (10000 Feet)
  // M0890 (8900 Meters)

  speedKnots = 0.f;
  altFeet = 0.f;

  // const static QRegularExpression SPDALT("^([NMK])(\\d{3,4})([FSAM])(\\d{3,4})$");
  QRegularExpressionMatch match = SPDALT.match(item);
  if(match.hasMatch())
  {
    bool ok = true;
    QString speedUnit = match.captured(1);
    float speed = match.captured(2).toFloat(&ok);
    if(!ok)
      return false;

    QString altUnit = match.captured(3);
    float alt = match.captured(4).toFloat(&ok);
    if(!ok)
      return false;

    if(altUnit == "F") // Flight Level
      altFeet = alt * 100.f;
    else if(altUnit == "S") // Standard Metric Level in tens of meters
      altFeet = atools::geo::meterToFeet(alt * 10.f);
    else if(altUnit == "A") // Altitude in hundreds of feet
      altFeet = alt * 100.f;
    else if(altUnit == "M") // Altitude in tens of meters
      altFeet = atools::geo::meterToFeet(alt * 10.f);
    else
      return false;

    if(speedUnit == "K") // km/h
      speedKnots = atools::geo::meterToNm(speed * 1000.f);
    else if(speedUnit == "N") // knots
      speedKnots = speed;
    else if(speedUnit == "M") // mach
      speedKnots = atools::geo::machToTasFromAlt(altFeet, speed / 100.f);
    else
      return false;
  }
  return true;
}

QString RouteString::createSpeedAndAltitude(float speedKnots, float altFeet)
{
  if(altFeet < 18000.f)
    return QString("N%1A%2").arg(speedKnots, 4, 'f', 0, QChar('0')).arg(altFeet / 100.f, 3, 'f', 0, QChar('0'));
  else
    return QString("N%1F%2").arg(speedKnots, 4, 'f', 0, QChar('0')).arg(altFeet / 100.f, 3, 'f', 0, QChar('0'));
}
