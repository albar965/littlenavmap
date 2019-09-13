/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "fs/util/fsutil.h"
#include "query/procedurequery.h"
#include "route/route.h"
#include "util/htmlbuilder.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "route/flightplanentrybuilder.h"
#include "common/maptools.h"
#include "common/unit.h"

#include <QRegularExpression>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using map::MapSearchResult;
using atools::geo::Pos;
namespace coords = atools::fs::util;
namespace plnentry = atools::fs::pln::entry;

// Maximum distance to previous waypoint - everything above will be sorted out
const static float MAX_WAYPOINT_DISTANCE_NM = 5000.f;

const static QRegularExpression SPDALT_WAYPOINT("^([A-Z0-9]+)/[NMK]\\d{3,4}[FSAM]\\d{3,4}$");

// Time specification directly after airport - ignored
// Runway specification directly after airport - ignored. Example: USSS/08R ... ZMUB1200/33
const static QRegularExpression AIRPORT_TIME_RUNWAY("^([A-Z0-9]{3,4})(\\d{4})?(/[LCR0-9]{2,3})?$");

const static QRegularExpression SID_STAR_TRANS("^([A-Z0-9]{1,7})(\\.([A-Z0-9]{1,6}))?$");

const static map::MapObjectTypes ROUTE_TYPES_AND_AIRWAY(map::AIRPORT | map::WAYPOINT |
                                                        map::VOR | map::NDB | map::USERPOINTROUTE |
                                                        map::AIRWAY);

const static map::MapObjectTypes ROUTE_TYPES(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);

RouteString::RouteString(FlightplanEntryBuilder *flightplanEntryBuilder)
  : entryBuilder(flightplanEntryBuilder)
{
  mapQuery = NavApp::getMapQuery();
  airportQuerySim = NavApp::getAirportQuerySim();
  procQuery = NavApp::getProcedureQuery();
}

RouteString::~RouteString()
{
}

QString RouteString::createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options)
{
  if(route.isEmpty())
    return QString();

#ifdef DEBUG_INFORMATION_ROUTE_STRING

  return createStringForRouteInternal(route, speed, options).join(" ").simplified().toUpper() + "\n\n" +
         "GTN:\n" + createGfpStringForRouteInternalProc(route, false) + "\n" +
         "GTN UWP:\n" + createGfpStringForRouteInternalProc(route, true) + "\n\n" +
         "GFP:\n" + createGfpStringForRouteInternal(route, false) + "\n" +
         "GFP UWP:\n" + createGfpStringForRouteInternal(route, true);

#else
  return createStringForRouteInternal(route, speed, options).join(" ").simplified().toUpper();

#endif
}

QStringList RouteString::createStringForRouteList(const Route& route, float speed, rs::RouteStringOptions options)
{
  if(route.isEmpty())
    return QStringList();

#ifdef DEBUG_INFORMATION_ROUTE_STRING

  return createStringForRouteInternal(route, speed, options) + "\n\n" +
         "GTN:\n" + createGfpStringForRouteInternalProc(route, false) + "\n" +
         "GTN UWP:\n" + createGfpStringForRouteInternalProc(route, true) + "\n\n" +
         "GFP:\n" + createGfpStringForRouteInternal(route, false) + "\n" +
         "GFP UWP:\n" + createGfpStringForRouteInternal(route, true);

#else
  return createStringForRouteInternal(route, speed, options);

#endif
}

QString RouteString::createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption)
{
  if(route.isEmpty())
    return QString();

  return procedures ?
         createGfpStringForRouteInternalProc(route, userWaypointOption) :
         createGfpStringForRouteInternal(route, userWaypointOption);
}

/*
 * Used for Reality XP GTN export
 *
 * Flight plan to depart from KPDX airport and arrive in KHIO through
 * Airway 448 and 204 using Runway 13 for final RNAV approach:
 * FPN/RI:DA:KPDX:D:LAVAA5.YKM:R:10R:F:YKM.V448.GEG.V204.HQM:F:SEA,N47261W 122186:AA:KHIO:A:HELNS5.SEA(13O):AP:R13
 *
 * Flight plan from KSLE to two user waypoints and then returning for the ILS approach to runway 31 via JAIME:
 * FPN/RI:F:KSLE:F:N45223W121419:F:N42568W122067:AA:KSLE:AP:I31.JAIME
 */
QString RouteString::createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption)
{
  QString retval;

  rs::RouteStringOptions opts = rs::NONE;
  if(userWaypointOption)
    opts = rs::GFP | rs::GFP_COORDS | rs::DCT | rs::USR_WPT | rs::NO_AIRWAYS;
  else
    opts = rs::GFP | rs::GFP_COORDS | rs::DCT;

  // Get string without start and destination
  QStringList string = createStringForRouteInternal(route, 0, opts);

  qDebug() << Q_FUNC_INFO << string;

  // Remove any useless DCTs
  if(!string.isEmpty() && string.last() == "DCT")
    string.removeLast();
  if(!string.isEmpty() && string.first() == "DCT")
    string.removeFirst();

  if(!string.isEmpty())
  {
    // Loop through waypoints and airways
    if(string.size() > 2)
    {
      for(int i = 0; i < string.size() - 1; i++)
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

  // Add procedures and airports
  QString sid, sidTrans, star, starTrans, departureRw, approachRwDummy, starRw;
  route.getSidStarNames(sid, sidTrans, star, starTrans);
  route.getRunwayNames(departureRw, approachRwDummy);
  starRw = route.getStarRunwayName();

  // Departure ===============================
  if(!retval.isEmpty() && !retval.startsWith(":F:"))
    retval.prepend(":F:");

  if(route.hasAnySidProcedure() && !userWaypointOption)
  {
    if(!departureRw.isEmpty() && !(departureRw.endsWith("L") || departureRw.endsWith("C") || departureRw.endsWith("R")))
      departureRw.append("O");

    // Add SID and departure airport
    retval.prepend("FPN/RI:DA:" + route.getDepartureAirportLeg().getIdent() +
                   ":D:" + sid + (sidTrans.isEmpty() ? QString() : "." + sidTrans) +
                   (departureRw.isEmpty() ? QString() : ":R:" + departureRw));
  }
  else
  {
    // Add departure airport only - no coordinates since these are not accurate
    retval.prepend("FPN/RI:F:" + route.getDepartureAirportLeg().getIdent());
  }

  if((route.hasAnyArrivalProcedure() || route.hasAnyStarProcedure()) && !userWaypointOption)
  {
    // Arrival airport - no coordinates
    retval.append(":AA:" + route.getDestinationAirportLeg().getIdent());

    // STAR ===============================
    if(route.hasAnyStarProcedure())
    {
      if(!starRw.isEmpty() && !(starRw.endsWith("L") || starRw.endsWith("C") || starRw.endsWith("R")))
        starRw.append("O");
      retval.append(":A:" + star + (starTrans.isEmpty() ? QString() : "." + starTrans) +
                    (starRw.isEmpty() ? QString() : "(" + starRw + ")"));
    }

    // Approach ===============================
    if(route.hasAnyArrivalProcedure())
    {
      QString apprArinc, apprTrans;
      route.getArrivalNames(apprArinc, apprTrans);
      retval.append(":AP:" + apprArinc + (apprTrans.isEmpty() ? QString() : "." + apprTrans));
    }
  }
  else
  {
    if(!retval.endsWith(":F:"))
      retval.append(":F:");
    // Arrival airport only - no coordinates since these are not accurate
    retval.append(route.getDestinationAirportLeg().getIdent());
  }

  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

/*
 * Used for Flight1 GTN export
 *
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
QString RouteString::createGfpStringForRouteInternal(const Route& route, bool userWaypointOption)
{
  QString retval;

  rs::RouteStringOptions opts = rs::NONE;
  if(userWaypointOption)
    opts = rs::GFP | rs::START_AND_DEST | rs::DCT | rs::USR_WPT | rs::NO_AIRWAYS;
  else
    opts = rs::GFP | rs::START_AND_DEST | rs::DCT;

  // Get string including start and destination
  QStringList string = createStringForRouteInternal(route, 0, opts);

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
          // Is a airway or direct
          retval += str == "DCT" ? ":F:" : "." + str + ".";
      }
    }
    else
      retval += ":F:";

    retval += string.last();
  }
  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

QStringList RouteString::createStringForRouteInternal(const Route& route, float speed, rs::RouteStringOptions options)
{
  QStringList retval;
  QString sid, sidTrans, star, starTrans, depRwy, destRwy, arrivalName, arrivalTransition;
  route.getSidStarNames(sid, sidTrans, star, starTrans);
  route.getRunwayNames(depRwy, destRwy);
  route.getArrivalNames(arrivalName, arrivalTransition);
  if(route.hasAnyArrivalProcedure() && !route.getArrivalLegs().approachType.isEmpty())
  {
    // Flight factor specialities - there are probably more to guess
    if(route.getArrivalLegs().approachType == "RNAV")
      arrivalName = "RNV" + destRwy;
    else
      arrivalName = route.getArrivalLegs().approachType + destRwy;
  }

  if(route.getSizeWithoutAlternates() == 0)
    return retval;

  bool hasSid = ((options& rs::SID_STAR) && !sid.isEmpty()) || (options & rs::SID_STAR_GENERIC);
  bool hasStar = ((options& rs::SID_STAR) && !star.isEmpty()) || (options & rs::SID_STAR_GENERIC);
  bool gfpCoords = options.testFlag(rs::GFP_COORDS);
  bool userWpt = options.testFlag(rs::USR_WPT);
  bool firstDct = true;

  QString lastAirway, lastId;
  Pos lastPos;
  map::MapObjectTypes lastType = map::NONE;
  int lastIndex = 0;
  for(int i = 0; i <= route.getDestinationAirportLegIndex(); i++)
  {
    const RouteLeg& leg = route.at(i);
    if(leg.isAnyProcedure())
      continue;

    // Ignore departure airport depending on options
    if(i == 0 && leg.getMapObjectType() == map::AIRPORT && !(options & rs::START_AND_DEST))
      // Options is off
      continue;

    const QString& airway = leg.getAirwayName();
    QString ident = leg.getIdent();

    if(leg.getMapObjectType() == map::USERPOINTROUTE)
    {
      // CYCD DCT DUNCN V440 YYJ V495 CDGPN DCT N48194W123096 DCT WATTR V495 JAWBN DCT 0S9
      if(options & rs::GFP)
        ident = coords::toGfpFormat(leg.getPosition());
      else if(options & rs::SKYVECTOR_COORDS)
        ident = coords::toDegMinSecFormat(leg.getPosition());
      else
        ident = coords::toDegMinFormat(leg.getPosition());
    }

    if(airway.isEmpty() || leg.isAirwaySetAndInvalid() || options & rs::NO_AIRWAYS)
    {
      // Do not use  airway string if not found in database
      if(!lastId.isEmpty())
      {
        if(userWpt && lastIndex > 0)
          retval.append(coords::toGfpFormat(lastPos));
        else
          retval.append(lastId +
                        (gfpCoords &&
                         lastType != map::USERPOINTROUTE ? "," + coords::toGfpFormat(lastPos) : QString()));

        if(lastIndex == 0 && options & rs::RUNWAY && !depRwy.isEmpty())
          // Add runway after departure
          retval.append(depRwy);
      }

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
      if(airway != lastAirway)
      {
        // Airways change add last ident of the last airway
        if(userWpt && lastIndex > 0)
          retval.append(coords::toGfpFormat(lastPos));
        else
          retval.append(lastId +
                        (gfpCoords &&
                         lastType != map::USERPOINTROUTE ? "," + coords::toGfpFormat(lastPos) : QString()));

        retval.append(airway);
      }
      // else Airway is the same skip waypoint
    }

    lastId = ident;
    lastPos = leg.getPosition();
    lastType = leg.getMapObjectType();
    lastAirway = airway;
    lastIndex = i;
  }

  // Append if departure airport present
  int insertPosition = (route.hasValidDeparture() && options & rs::START_AND_DEST) ? 1 : 0;
  if(!retval.isEmpty())
  {
    if(hasStar && retval.last() == "DCT")
      // Remove last DCT so it does not collide with the STAR - destination airport not inserted yet
      retval.removeLast();

    if(options & rs::APPROACH && retval.last() == "DCT")
      // Remove last DCT if approach information is desired
      retval.removeLast();

    if(options & rs::RUNWAY && !depRwy.isEmpty())
      insertPosition++;
  }

  QString transSeparator = options & rs::SID_STAR_SPACE ? " " : ".";

  // Add SID
  if((options& rs::SID_STAR) && !sid.isEmpty())
    retval.insert(insertPosition, sid + (sidTrans.isEmpty() ? QString() : transSeparator + sidTrans));
  else if(options & rs::SID_STAR_GENERIC)
    retval.insert(insertPosition, "SID");

  // Add speed and altitude
  if(!retval.isEmpty() && options & rs::ALT_AND_SPEED)
    retval.insert(insertPosition, atools::fs::util::createSpeedAndAltitude(speed, route.getCruisingAltitudeFeet()));

  // Add STAR
  if((options& rs::SID_STAR) && !star.isEmpty())
    retval.append(star + (starTrans.isEmpty() ? QString() : transSeparator + starTrans));
  else if(options & rs::SID_STAR_GENERIC)
    retval.append("STAR");

  // Remove last DCT for flight factor export
  if(options & rs::NO_FINAL_DCT && retval.last() == "DCT")
    retval.removeLast();

  // Add destination airport
  if(options & rs::START_AND_DEST)
    retval.append(lastId + (gfpCoords ? "," + coords::toGfpFormat(lastPos) : QString()));

  if(options & rs::APPROACH && !arrivalName.isEmpty())
  {
    retval.append(arrivalName);
    if(!arrivalTransition.isEmpty())
      retval.append(arrivalTransition);
  }

  if(options & rs::ALTERNATES)
    retval.append(route.getAlternateIdents());

  if(options & rs::FLIGHTLEVEL)
    retval.append(QString("FL%1").arg(static_cast<int>(route.getCruisingAltitudeFeet()) / 100));

  qDebug() << Q_FUNC_INFO << retval;

  return retval;
}

bool RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan,
                                        rs::RouteStringOptions options)
{
  float speeddummy;
  bool altdummy;
  return createRouteFromString(routeString, flightplan, speeddummy, altdummy, options);
}

bool RouteString::createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan,
                                        float& speedKts, bool& altIncluded, rs::RouteStringOptions options)
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

  if(!addDestination(flightplan, cleanItems, options))
    return false;

  if(altitude > 0.f)
    appendMessage(tr("Using cruise altitude <b>%1</b> for flight plan.").arg(Unit::altFeet(altitude)));

  if(speedKts > 0.f)
  {
    float cruise = NavApp::getRouteCruiseSpeedKts();
    if(atools::almostEqual(speedKts, cruise, 1.f))
      appendWarning(tr("Ignoring speed instruction %1.").arg(Unit::speedKts(speedKts)));
    else
      appendWarning(tr("Ignoring speed instruction %1 in favor of aircraft performance (%2 true airspeed).").
                    arg(Unit::speedKts(speedKts)).
                    arg(Unit::speedKts(NavApp::getRouteCruiseSpeedKts())));
  }

  // Do not get any navaids that are too far away
  float maxDistance = atools::geo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM, flightplan.getDistanceNm() * 2.0f));

  // Collect all navaids, airports and coordinates
  Pos lastPos(flightplan.getDeparturePosition());
  QList<ParseEntry> resultList;
  for(const QString& item : cleanItems)
  {
    // Simply fetch all possible waypoints
    MapSearchResult result;
    findWaypoints(result, item);

    // Get last result so we can check for airway/waypoint matches when selecting the last position
    // The nearest is used if no airway matches
    const MapSearchResult *lastResult = resultList.isEmpty() ? nullptr : &resultList.last().result;

    // Sort lists by distance and remove all which are too far away and update last pos
    filterWaypoints(result, lastPos, lastResult, maxDistance);

    if(!result.isEmpty(ROUTE_TYPES_AND_AIRWAY))
      resultList.append({item, QString(), result});
    else
      appendWarning(tr("Nothing found for %1. Ignoring.").arg(item));
  }

  // Create airways - will fill the waypoint list in result with airway points
  // if airway is invalid it will be erased in result
  // Will erase NDB, VOR and adapt waypoint list in result if an airway/waypoint match was found
  for(int i = 1; i < resultList.size() - 1; i++)
    filterAirways(resultList, i);

#ifdef DEBUG_INFORMATION

  for(const ParseEntry& parse : resultList)
    qDebug() << parse.item << parse.airway << parse.result;

#endif

  // Remove now unneeded results after airway evaluation
  removeEmptyResults(resultList);

  // Now build the flight plan
  lastPos = flightplan.getDeparturePosition();
  for(int i = 0; i < resultList.size(); i++)
  {
    const QString& item = resultList.at(i).item;
    MapSearchResult& result = resultList[i].result;

    if(!result.airways.isEmpty())
    {
      if(i == 0)
        appendWarning(tr("Found airway %1 instead of waypoint as first entry in enroute list. Ignoring.").arg(item));
      else
      {
        // Add all airway waypoints if any were found
        for(const map::MapWaypoint& wp : result.waypoints)
        {
          // Reset but keep the last one
          FlightplanEntry entry;

          // Will fetch objects in order: airport, waypoint, vor, ndb
          // Only waypoints are relevant since airways are used here
          entryBuilder->entryFromWaypoint(wp, entry, true);

          if(entry.getPosition().isValid())
          {
            entry.setAirway(item);
            flightplan.getEntries().insert(flightplan.getEntries().size() - 1, entry);
            // qDebug() << entry.getIcaoIdent() << entry.getAirway();
          }
          else
            appendWarning(tr("No navaid found for %1 on airway %2. Ignoring.").arg(wp.ident).arg(item));
        }
      }
    }
    else
    {
      // Add a single waypoint from direct
      FlightplanEntry entry;
      if(!result.userPointsRoute.isEmpty())
      {
        // User entries are always a perfect match
        // Convert a coordinate to a user defined waypoint
        entryBuilder->buildFlightplanEntry(result.userPointsRoute.first().position, result, entry, true, map::NONE);

        // Use the original string as name but limit it for fs
        entry.setWaypointId(item);
      }
      else
        // Get nearest navaid, whatever it is
        buildEntryForResult(entry, result, lastPos);

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

#ifdef DEBUG_INFORMATION
  qDebug() << flightplan;
#endif
  return true;
}

void RouteString::buildEntryForResult(FlightplanEntry& entry, const MapSearchResult& result,
                                      const atools::geo::Pos& nearestPos)
{
  MapSearchResult newResult;
  resultWithClosest(newResult, result, nearestPos, map::WAYPOINT | map::VOR | map::NDB | map::AIRPORT);
  entryBuilder->buildFlightplanEntry(newResult, entry, true);
}

void RouteString::resultWithClosest(map::MapSearchResult& resultWithClosest, const map::MapSearchResult& result,
                                    const atools::geo::Pos& nearestPos, map::MapObjectTypes types)
{
  struct DistData
  {
    map::MapObjectTypes type;
    int index;
    atools::geo::Pos pos;

    const atools::geo::Pos& getPosition() const
    {
      return pos;
    }

  };

  // Add all elements to the list
  QList<DistData> sorted;

  if(types & map::WAYPOINT)
  {
    for(int i = 0; i < result.waypoints.size(); i++)
      sorted.append({map::WAYPOINT, i, result.waypoints.at(i).getPosition()});
  }

  if(types & map::VOR)
  {
    for(int i = 0; i < result.vors.size(); i++)
      sorted.append({map::VOR, i, result.vors.at(i).getPosition()});
  }

  if(types & map::NDB)
  {
    for(int i = 0; i < result.ndbs.size(); i++)
      sorted.append({map::NDB, i, result.ndbs.at(i).getPosition()});
  }

  if(types & map::AIRPORT)
  {
    for(int i = 0; i < result.airports.size(); i++)
      sorted.append({map::AIRPORT, i, result.airports.at(i).getPosition()});
  }

  if(!sorted.isEmpty())
  {
    // Sort the list by distance - closest first
    maptools::sortByDistance(sorted, nearestPos);

    // Get the first and closest element
    const DistData& nearest = sorted.first();

    // Add the closest to the result
    if(nearest.type == map::WAYPOINT)
      resultWithClosest.waypoints.append(result.waypoints.at(nearest.index));
    else if(nearest.type == map::VOR)
      resultWithClosest.vors.append(result.vors.at(nearest.index));
    else if(nearest.type == map::NDB)
      resultWithClosest.ndbs.append(result.ndbs.at(nearest.index));
    else if(nearest.type == map::AIRPORT)
      resultWithClosest.airports.append(result.airports.at(nearest.index));
  }
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
    messages.append(atools::util::HtmlBuilder::warningMessage(message));
  qWarning() << "Warning:" << message;
}

void RouteString::appendError(const QString& message)
{
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(atools::util::HtmlBuilder::errorMessage(message));
  qWarning() << "Error:" << message;
}

QString RouteString::extractAirportIdent(QString ident)
{
  QRegularExpressionMatch match = AIRPORT_TIME_RUNWAY.match(ident);
  if(match.hasMatch())
  {
    ident = match.captured(1);

    if(!match.captured(2).isEmpty())
      appendWarning(tr("Ignoring time specification for airport %1.").arg(ident));
    if(!match.captured(3).isEmpty())
      appendWarning(tr("Ignoring runway specification for airport %1.").arg(ident));
  }
  return ident;
}

bool RouteString::addDeparture(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems)
{
  QString ident = extractAirportIdent(cleanItems.takeFirst());

  map::MapAirport departure;
  airportQuerySim->getAirportByIdent(departure, ident);
  if(departure.isValid())
  {
    // qDebug() << "found" << departure.ident << "id" << departure.id;

    flightplan.setDepartureAiportName(departure.name);
    flightplan.setDepartureIdent(departure.ident);
    flightplan.setDeparturePosition(departure.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry, false /* alternate */);
    flightplan.getEntries().append(entry);

    if(!cleanItems.isEmpty())
    {
      QString sidTrans = cleanItems.first();

      bool foundSid = false;
      QRegularExpressionMatch sidMatch = SID_STAR_TRANS.match(sidTrans);
      if(sidMatch.hasMatch())
      {
        QString sid = sidMatch.captured(1);
        if(sid.size() == 7)
          // Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A
          sid.remove(4, 1);

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
          procQuery->fillFlightplanProcedureProperties(
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

bool RouteString::addDestination(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems,
                                 rs::RouteStringOptions options)
{
  if(options & rs::READ_ALTERNATES)
  {
    // Read airports at end of list as alternates =============================================

    // List of consecutive airports at end of string - destination is last entry, alternates before
    QVector<proc::MapProcedureLegs> stars;
    QVector<map::MapAirport> airports;

    // Iterate from end of list
    int idx = cleanItems.size() - 1;
    while(idx >= 0)
    {
      proc::MapProcedureLegs star;
      map::MapAirport ap;

      // Get airport and STAR
      destinationInternal(ap, star, cleanItems, idx);

      if(!star.isEmpty())
      {
        // Found a STAR - this is the destination
        stars.append(star);
        airports.append(ap);
        break;
      }

      if(ap.isValid())
      {
        // Found a valid airport, add and continue with previous one
        stars.append(star);
        airports.append(ap);
        idx--;
      }
      else
        // No valid airport - stop here
        break;
    }

    if(!airports.isEmpty())
    {
      // Consume items in list
      for(int i = 0; i < airports.size() && !cleanItems.isEmpty(); i++)
        cleanItems.removeLast();

      // Get and remove destination airport ========================
      map::MapAirport dest = airports.takeLast();
      proc::MapProcedureLegs star = stars.takeLast();

      flightplan.setDestinationAiportName(dest.name);
      flightplan.setDestinationIdent(dest.ident);
      flightplan.setDestinationPosition(dest.getPosition());

      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(dest, entry, false /* alternate */);
      flightplan.getEntries().append(entry);

      // Get destination STAR ========================
      if(!star.isEmpty())
      {
        // Consume STAR in list
        cleanItems.removeLast();

        // Add information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        procQuery->fillFlightplanProcedureProperties(flightplan.getProperties(), arrivalLegs, star, departureLegs);
      }

      // Collect alternates ========================
      QStringList alternateIdents;
      for(const map::MapAirport& alt : airports)
        alternateIdents.prepend(alt.ident);

      if(!alternateIdents.isEmpty())
      {
        flightplan.getProperties().insert(atools::fs::pln::ALTERNATES, alternateIdents.join("#"));
        appendMessage(tr("Found alternate %1 <b>%2</b>.").
                      arg(alternateIdents.size() == 1 ? tr("airport") : tr("airports")).
                      arg(alternateIdents.join(tr(", "))));
      }
    } // if(!airports.isEmpty())
    else
    {
      appendError(tr("Required destination airport %1 not found.").arg(cleanItems.last()));
      return false;
    }
  } // if(options & rs::READ_ALTERNATES)
  else
  {
    // Read airports at end of list as waypoints =============================================
    proc::MapProcedureLegs star;
    map::MapAirport dest;

    // Get airport and STAR at end of list ================================
    destinationInternal(dest, star, cleanItems, cleanItems.size() - 1);

    if(dest.isValid())
    {
      flightplan.setDestinationAiportName(dest.name);
      flightplan.setDestinationIdent(dest.ident);
      flightplan.setDestinationPosition(dest.getPosition());

      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(dest, entry, false /* alternate */);
      flightplan.getEntries().append(entry);

      // Consume airport
      cleanItems.removeLast();

      // Get destination STAR ========================
      if(!star.isEmpty())
      {
        // Consume STAR in list
        cleanItems.removeLast();

        // Add information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        procQuery->fillFlightplanProcedureProperties(flightplan.getProperties(), arrivalLegs, star, departureLegs);
      }

      // Remaining airports are handled by other waypoint code
    }
    else
    {
      appendError(tr("Required destination airport %1 not found.").arg(cleanItems.last()));
      return false;
    }
  }
  return true;
}

void RouteString::destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs,
                                      const QStringList& cleanItems, int index)
{
  airportQuerySim->getAirportByIdent(destination, extractAirportIdent(cleanItems.at(index)));
  if(destination.isValid())
  {
    if(cleanItems.size() > 1 && index > 0)
    {
      QString starTrans = cleanItems.at(index - 1);

      QRegularExpressionMatch starMatch = SID_STAR_TRANS.match(starTrans);
      if(starMatch.hasMatch())
      {
        QString star = starMatch.captured(1);
        QString trans = starMatch.captured(3);

        if(star.size() == 7)
          // Convert to abbreviated STAR: BELGU3L ENGM -> BELG3L ENGM
          star.remove(4, 1);

        int starTransId = -1;
        bool foundStar = false;
        int starId = procQuery->getStarId(destination, star);
        if(starId != -1 && !trans.isEmpty())
        {
          starTransId = procQuery->getStarTransitionId(destination, trans, starId);
          foundStar = starTransId != -1;
        }
        else
          foundStar = starId != -1;

        if(foundStar)
        {
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
        }
      }
    }
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
    qWarning() << Q_FUNC_INFO << "Fragmented airway: lastId"
               << lastId << "nextId" << nextId << "not found in" << airway;

#ifdef DEBUG_INFORMATION
    qDebug() << "Fragmented Airway has waypoints";
    for(const map::MapAirwayWaypoint& wp : allAirwayWaypoints)
      qDebug() << wp.waypoint.id << wp.waypoint.ident << wp.waypoint.region;

#endif

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

void RouteString::filterWaypoints(MapSearchResult& result, atools::geo::Pos& lastPos, const MapSearchResult *lastResult,
                                  float maxDistance)
{
  maptools::sortByDistance(result.airports, lastPos);
  // maptools::removeByDistance(result.airports, lastPos, maxDistance);

  maptools::sortByDistance(result.waypoints, lastPos);
  maptools::removeByDistance(result.waypoints, lastPos, maxDistance);

  maptools::sortByDistance(result.vors, lastPos);
  maptools::removeByDistance(result.vors, lastPos, maxDistance);

  maptools::sortByDistance(result.ndbs, lastPos);
  maptools::removeByDistance(result.ndbs, lastPos, maxDistance);

  maptools::sortByDistance(result.userPointsRoute, lastPos);
  maptools::removeByDistance(result.userPointsRoute, lastPos, maxDistance);

  if(!result.userPointsRoute.isEmpty())
    // User points have preference since they can be clearly identified
    lastPos = result.userPointsRoute.first().position;
  else
  {
    // First check if there is a match to an airway
    bool updateNearestPos = true;
    if(lastResult != nullptr && result.hasAirways() && lastResult->hasWaypoints())
    {
      // Extract unique airways names
      QSet<QString> airwayNames;
      for(const map::MapAirway& a : result.airways)
        airwayNames.insert(a.name);

      // Extract unique waypoint idents
      QSet<QString> waypointIdents;
      for(const map::MapWaypoint& a : lastResult->waypoints)
        waypointIdents.insert(a.ident);

      // Iterate through all airway/waypoint combinations
      for(const QString& airwayName : airwayNames)
      {
        for(const QString& waypointIdent : waypointIdents)
        {
          QList<map::MapWaypoint> waypoints;
          // Get all waypoints for first
          mapQuery->getWaypointsForAirway(waypoints, airwayName, waypointIdent);

          if(!waypoints.isEmpty())
          {
            // Found waypoint on airway - leave position as it is
            updateNearestPos = false;
            break;
          }
        }
        if(updateNearestPos == false)
          break;
      }
    }

    // Get the nearest position only if there is no match with an airway
    if(updateNearestPos)
    {
      struct ObjectDist
      {
        float distMeter;
        Pos pos;
        map::MapObjectTypes type;
      };

      // Find something whatever is nearest and use that for the last position
      QVector<ObjectDist> dists;
      if(result.hasAirports())
        dists.append({
          result.airports.first().position.distanceMeterTo(lastPos),
          result.airports.first().position,
          map::AIRPORT
        });
      if(result.hasWaypoints())
        dists.append({
          result.waypoints.first().position.distanceMeterTo(lastPos),
          result.waypoints.first().position,
          map::WAYPOINT
        });
      if(result.hasVor())
        dists.append({
          result.vors.first().position.distanceMeterTo(lastPos),
          result.vors.first().position,
          map::VOR
        });
      if(result.hasNdb())
        dists.append({
          result.ndbs.first().position.distanceMeterTo(lastPos),
          result.ndbs.first().position,
          map::NDB
        });

      // Sort by distance
      if(dists.size() > 1)
        std::sort(dists.begin(), dists.end(), [](const ObjectDist& p1, const ObjectDist& p2) -> bool {
          return p1.distMeter < p2.distMeter;
        });

      if(!dists.isEmpty())
      {
        // Check for special case where a NDB and a VOR with the same name are nearby. Prefer VOR in this case.
        if(dists.size() >= 2 && dists.at(0).type == map::NDB && dists.at(1).type == map::VOR &&
           atools::almostEqual(dists.at(0).distMeter, dists.at(1).distMeter, 10000.f))
        {
          dists.removeFirst();

          if(result.hasNdb())
            result.ndbs.removeFirst();
        }

        // Use position of nearest to last
        lastPos = dists.first().pos;
      }
    }
  }
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
    const QString& waypointNameStart = resultList.at(i - 1).item;

    if(lastResult.waypoints.isEmpty())
    {
      if(result.isEmpty(ROUTE_TYPES))
        // Print a warning only if nothing else was found
        appendWarning(tr("No waypoint before airway %1. Ignoring flight plan segment.").arg(airwayName));
      result.airways.clear();
      return;
    }

    if(nextResult.waypoints.isEmpty())
    {
      if(result.isEmpty(ROUTE_TYPES))
        // Print a warning only if nothing else was found
        appendWarning(tr("No waypoint after airway %1. Ignoring flight plan segment.").arg(airwayName));
      result.airways.clear();
      return;
    }

    // Get all waypoints for first
    mapQuery->getWaypointsForAirway(waypoints, airwayName, waypointNameStart);

    if(!waypoints.isEmpty())
    {
      QList<map::MapAirwayWaypoint> allAirwayWaypoints;

      // Get all waypoints for the airway sorted by fragment and sequence
      mapQuery->getWaypointListForAirwayName(allAirwayWaypoints, airwayName);

#ifdef DEBUG_INFORMATION
      for(const map::MapAirwayWaypoint& w : allAirwayWaypoints)
        qDebug() << w.waypoint.id << w.waypoint.ident << w.waypoint.region;
#endif

      if(!allAirwayWaypoints.isEmpty())
      {
        int startIndex = -1, endIndex = -1;

        // Iterate through all found waypoint combinations (same ident) and try to match them to an airway
        for(const map::MapWaypoint& wpLast : lastResult.waypoints)
        {
          for(const map::MapWaypoint& wpNext : nextResult.waypoints)
          {
            // Find the waypoint indexes by id in the list of all waypoints for this airway
            findIndexesInAirway(allAirwayWaypoints, wpLast.id, wpNext.id, startIndex, endIndex, airwayName);
            if(startIndex != -1 && endIndex != -1)
              break;
          }
          if(startIndex != -1 && endIndex != -1)
            break;
        }

        // qDebug() << "startidx" << startIndex << "endidx" << endIndex;
        if(startIndex != -1 && endIndex != -1)
        {
          // Get the list of waypoints from start to end index
          result.waypoints.clear();

          // Override airway for the next
          resultList[i + 1].airway = airwayName;
          extractWaypoints(allAirwayWaypoints, startIndex, endIndex, result.waypoints);

          // Clear previous and next result to force waypoint/airway usage
          lastResult.vors.clear();
          lastResult.ndbs.clear();
          lastResult.airports.clear();
          lastResult.waypoints.clear();
          lastResult.waypoints.append(allAirwayWaypoints.at(startIndex).waypoint);

          nextResult.vors.clear();
          nextResult.ndbs.clear();
          nextResult.airports.clear();
          nextResult.waypoints.clear();
          nextResult.waypoints.append(allAirwayWaypoints.at(endIndex).waypoint);
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
                    arg(waypointNameStart).arg(airwayName));
      result.airways.clear();
    }
  }
}

void RouteString::findWaypoints(MapSearchResult& result, const QString& item)
{
  if(item.length() > 5)
  {
    // User defined waypoint
    Pos pos = coords::fromAnyWaypointFormat(item);
    if(pos.isValid())
    {
      map::MapUserpointRoute user;
      user.position = pos;
      result.userPointsRoute.append(user);
    }
  }
  else
  {
    mapQuery->getMapObjectByIdent(result, ROUTE_TYPES_AND_AIRWAY, item);

    if(item.length() == 5 && result.waypoints.isEmpty())
    {
      // Try NAT waypoint (a few of these are also in the database)
      Pos pos = coords::fromAnyWaypointFormat(item);
      if(pos.isValid())
      {
        map::MapUserpointRoute user;
        user.position = pos;
        result.userPointsRoute.append(user);
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

    if(atools::fs::util::speedAndAltitudeMatch(item))
    {
      if(!atools::fs::util::extractSpeedAndAltitude(item, speedKnots, altFeet))
        appendWarning(tr("Ignoring invalid speed and altitude instruction %1.").arg(item));
      continue;
    }

    QRegularExpressionMatch match = SPDALT_WAYPOINT.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendWarning(tr("Ignoring speed and altitude at waypoint %1.").arg(item));
    }

    item = extractAirportIdent(item);

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
    return type.result.isEmpty(ROUTE_TYPES_AND_AIRWAY);
  });

  if(it != resultList.end())
    resultList.erase(it, resultList.end());
}
