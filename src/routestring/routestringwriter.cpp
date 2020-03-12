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

#include "routestring/routestringwriter.h"

#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "route/route.h"

using atools::geo::Pos;
namespace coords = atools::fs::util;

RouteStringWriter::RouteStringWriter()
{
}

RouteStringWriter::~RouteStringWriter()
{
}

QString RouteStringWriter::createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options) const
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

QStringList RouteStringWriter::createStringForRouteList(const Route& route, float speed,
                                                        rs::RouteStringOptions options) const
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

QString RouteStringWriter::createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption) const
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
QString RouteStringWriter::createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption) const
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
QString RouteStringWriter::createGfpStringForRouteInternal(const Route& route, bool userWaypointOption) const
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

QStringList RouteStringWriter::createStringForRouteInternal(const Route& route, float speed,
                                                            rs::RouteStringOptions options) const
{
  QStringList retval;
  QString sid, sidTrans, star, starTrans, depRwy, destRwy, arrivalName, arrivalTransition;
  route.getSidStarNames(sid, sidTrans, star, starTrans);
  route.getRunwayNames(depRwy, destRwy);
  route.getArrivalNames(arrivalName, arrivalTransition);
  if(route.hasAnyArrivalProcedure() && !route.getApproachLegs().approachType.isEmpty())
  {
    // Flight factor specialities - there are probably more to guess
    if(route.getApproachLegs().approachType == "RNAV")
      arrivalName = "RNV" + destRwy;
    else
      arrivalName = route.getApproachLegs().approachType + destRwy;
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
    const RouteLeg& leg = route.value(i);
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

    if(airway.isEmpty() || leg.isAirwaySetAndInvalid(map::INVALID_ALTITUDE_VALUE) || options & rs::NO_AIRWAYS)
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
