/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "common/unit.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "route/route.h"

#include <QStringBuilder>

using atools::geo::Pos;
namespace coords = atools::fs::util;

RouteStringWriter::RouteStringWriter()
{
}

RouteStringWriter::~RouteStringWriter()
{
}

QString RouteStringWriter::createStringForRoute(const Route& route, float speedKts, rs::RouteStringOptions options) const
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
  return createStringForRouteInternal(route, speedKts, options).join(' ').simplified().toUpper();

#endif
}

QStringList RouteStringWriter::createStringListForRoute(const Route& route, float speedKts, rs::RouteStringOptions options) const
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
  return createStringForRouteInternal(route, speedKts, options);

#endif
}

QString RouteStringWriter::createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption, bool gfpCoordinates) const
{
  if(route.isEmpty())
    return QString();

  return procedures ?
         createGfpStringForRouteInternalProc(route, userWaypointOption, gfpCoordinates) :
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
QString RouteStringWriter::createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption, bool gfpCoordinates) const
{
  QString retval;

  rs::RouteStringOptions opts = rs::NONE;
  if(userWaypointOption)
    opts = rs::GFP | rs::DCT | rs::USR_WPT | rs::NO_AIRWAYS;
  else
    opts = rs::GFP | rs::DCT;

  if(gfpCoordinates)
    opts |= rs::GFP_COORDS;

  // Get string without start and destination
  QStringList string = createStringForRouteInternal(route, 0, opts);

  qDebug() << Q_FUNC_INFO << string;

  // Remove any useless DCTs
  if(!string.isEmpty() && string.constLast() == "DCT")
    string.removeLast();
  if(!string.isEmpty() && string.constFirst() == "DCT")
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
            retval += '.' % str % '.';
        }
      }
    }
    else
      retval += ":F:";

    retval += string.constLast();
  }

  // Add procedures and airports
  QString sid, sidTrans, star, starTrans, departureRw, approachRwDummy, starRw;
  route.getSidStarNames(sid, sidTrans, star, starTrans);
  route.getRunwayNames(departureRw, approachRwDummy);
  starRw = route.getStarRunwayName();

  if(route.hasCustomDeparture())
  {
    sid.clear();
    sidTrans.clear();
    departureRw.clear();
  }

  // Departure ===============================
  if(!retval.isEmpty() && !retval.startsWith(":F:"))
    retval.prepend(":F:");

  if(route.hasAnySidProcedure() && !route.hasCustomDeparture() && !userWaypointOption)
  {
    if(!departureRw.isEmpty() && !(departureRw.endsWith('L') || departureRw.endsWith('C') || departureRw.endsWith('R')))
      departureRw.append('O');

    // Add SID and departure airport
    retval.prepend("FPN/RI:DA:" % route.getDepartureAirportLeg().getIdent() %
                   ":D:" % sid % (sidTrans.isEmpty() ? QString() : '.' % sidTrans) %
                   (departureRw.isEmpty() ? QString() : ":R:" % departureRw));
  }
  else
  {
    // Add departure airport only - no coordinates since these are not accurate
    retval.prepend("FPN/RI:F:" % route.getDepartureAirportLeg().getIdent());
  }

  if((route.hasAnyApproachProcedure() || route.hasAnyStarProcedure()) && !userWaypointOption)
  {
    // Arrival airport - no coordinates
    retval.append(":AA:" % route.getDestinationAirportLeg().getIdent());

    // STAR ===============================
    if(route.hasAnyStarProcedure())
    {
      if(!starRw.isEmpty() && !(starRw.endsWith('L') || starRw.endsWith('C') || starRw.endsWith('R')))
        starRw.append('O');
      retval.append(":A:" % star % (starTrans.isEmpty() ? QString() : '.' % starTrans) %
                    (starRw.isEmpty() ? QString() : '(' % starRw % ')'));
    }

    // Approach ===============================
    if(route.hasAnyApproachProcedure() && !route.hasCustomApproach())
    {
      QString apprArinc, apprTrans, appSuffixDummy;
      route.getApproachNames(apprArinc, apprTrans, appSuffixDummy);
      retval.append(":AP:" % apprArinc % (apprTrans.isEmpty() ? QString() : '.' % apprTrans));
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
    retval += "FPN/RI:F:" % string.constFirst();

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

    retval += string.constLast();
  }
  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

QStringList RouteStringWriter::createStringForRouteInternal(const Route& routeParam, float speedKts, rs::RouteStringOptions options) const
{
  Route route = routeParam.adjustedToOptions(rf::DEFAULT_OPTS_ROUTESTRING);

  QStringList items;
  QString sid, sidTrans, star, starTrans, depRwy, destRwy, approachName, approachTransition, approachSuffix, customApprRw, customDepRw;
  route.getSidStarNames(sid, sidTrans, star, starTrans);
  route.getRunwayNames(depRwy, destRwy);
  route.getApproachNames(approachName, approachTransition, approachSuffix);

  // Keep runways in case there is a custom approach or custom departure
  customDepRw = depRwy;
  customApprRw = destRwy;

  if(route.hasCustomApproach())
  {
    approachName.clear();
    approachTransition.clear();
    approachSuffix.clear();
    destRwy.clear();
  }

  if(route.hasCustomDeparture())
  {
    sid.clear();
    sidTrans.clear();
    depRwy.clear();
  }

  if(route.hasAnyApproachProcedure() && !route.getApproachLegs().type.isEmpty() && !route.hasCustomApproach())
  {
    // Flight factor specialities - there are probably more to guess
    if(route.getApproachLegs().type == "RNAV")
      approachName = "RNV" % destRwy;
    else
      approachName = route.getApproachLegs().type % destRwy;
  }

  if(route.getSizeWithoutAlternates() == 0)
    return items;

  bool hasSid = (options.testFlag(rs::SID_STAR) && !sid.isEmpty()) || options.testFlag(rs::SID_STAR_GENERIC);
  bool hasStar = (options.testFlag(rs::SID_STAR) && !star.isEmpty()) || options.testFlag(rs::SID_STAR_GENERIC);
  bool gfpCoords = options.testFlag(rs::GFP_COORDS);
  bool userWpt = options.testFlag(rs::USR_WPT);
  bool firstDct = true;

  QString lastAirway, lastId;
  Pos lastPos;
  map::MapTypes lastType = map::NONE;
  int lastIndex = 0;
  for(int i = 0; i <= route.getDestinationAirportLegIndex(); i++)
  {
    const RouteLeg& leg = route.value(i);

    if(leg.isAnyProcedure())
      continue;

    // Ignore departure airport depending on options
    if(i == 0 && leg.getMapType() == map::AIRPORT && !options.testFlag(rs::START_AND_DEST))
      // Options is off
      continue;

    const QString& airway = leg.getAirwayName();
    // Internal ident or ICAO
    QString ident = leg.getDisplayIdent();

    if(leg.getMapType() == map::USERPOINTROUTE)
    {
      // CYCD DCT DUNCN V440 YYJ V495 CDGPN DCT N48194W123096 DCT WATTR V495 JAWBN DCT 0S9
      if(options.testFlag(rs::GFP))
        ident = coords::toGfpFormat(leg.getPosition());
      else if(options.testFlag(rs::SKYVECTOR_COORDS))
        ident = coords::toDegMinSecFormat(leg.getPosition());
      else
        ident = coords::toDegMinFormat(leg.getPosition());
    }

    if(airway.isEmpty() || leg.isAirwaySetAndInvalid(map::INVALID_ALTITUDE_VALUE) || options.testFlag(rs::NO_AIRWAYS))
    {
      // Do not use  airway string if not found in database
      if(!lastId.isEmpty())
      {
        if(userWpt && lastIndex > 0)
          items.append(coords::toGfpFormat(lastPos));
        else
          items.append(lastId % (gfpCoords && lastType != map::USERPOINTROUTE ? ',' % coords::toGfpFormat(lastPos) : QString()));

        if(lastIndex == 0 && options.testFlag(rs::CORTEIN_DEPARTURE_RUNWAY) && !depRwy.isEmpty())
          // Add runway after departure
          items.append(depRwy);
      }

      if(i > 0 && options.testFlag(rs::DCT))
      {
        if(!(firstDct && hasSid))
          // Add direct if not start airport - do not add where a SID is inserted
          items.append("DCT");
        firstDct = false;
      }
    }
    else
    {
      if(airway != lastAirway)
      {
        // Airways change add last ident of the last airway
        if(userWpt && lastIndex > 0)
          items.append(coords::toGfpFormat(lastPos));
        else
          items.append(lastId % (gfpCoords && lastType != map::USERPOINTROUTE ? ',' % coords::toGfpFormat(lastPos) : QString()));

        items.append(airway);
      }
      // else Airway is the same skip waypoint
    }

    // Append runway descriptor to departure airport if a custom runway is given
    // SID runway for real departure procedures is added above
    if(i == 0 && options.testFlag(rs::WRITE_APPROACH_RUNWAYS) && !customDepRw.isEmpty())
      ident += '/' % customDepRw;

    lastId = ident;
    lastPos = leg.getPosition();
    lastType = leg.getMapType();
    lastAirway = airway;
    lastIndex = i;
  }

  // Append if departure airport present
  int insertPosition = (route.hasValidDeparture() && options.testFlag(rs::START_AND_DEST)) ? 1 : 0;
  if(!items.isEmpty())
  {
    if(hasStar && items.constLast() == "DCT")
      // Remove last DCT so it does not collide with the STAR - destination airport not inserted yet
      items.removeLast();

    if(options.testFlag(rs::CORTEIN_APPROACH) && items.constLast() == "DCT")
      // Remove last DCT if approach information is desired
      items.removeLast();

    if(options.testFlag(rs::CORTEIN_DEPARTURE_RUNWAY) && !depRwy.isEmpty())
      insertPosition++;
  }

  // Get transition separator
  bool sidStarSpace = options.testFlag(rs::SID_STAR_SPACE);

  if(!route.getSidLegs().isCustomDeparture()) // Do not add custom departure as SID
  {
    // Add SID
    if(options.testFlag(rs::SID_STAR) && !sid.isEmpty())
    {
      if(!sidTrans.isEmpty() && sidStarSpace && sidTrans == items.value(insertPosition))
        // Avoid duplicate of SID transition and next waypoint if using space separated notation
        items.insert(insertPosition, sid);
      else
      {
        if(sidStarSpace)
        {
          if(!sidTrans.isEmpty())
            items.insert(insertPosition, sidTrans);
          items.insert(insertPosition, sid);
        }
        else
          items.insert(insertPosition, sid % (sidTrans.isEmpty() ? QString() : '.' % sidTrans));
      }
    }
    else if(options.testFlag(rs::SID_STAR_GENERIC))
      items.insert(insertPosition, "SID");
  }

  // Add speed and altitude
  if(!items.isEmpty() && options.testFlag(rs::ALT_AND_SPEED))
  {
    bool metric = options.testFlag(rs::ALT_AND_SPEED_METRIC);
    items.insert(insertPosition, atools::fs::util::createSpeedAndAltitude(speedKts, route.getCruiseAltitudeFt(),
                                                                          metric && Unit::getUnitSpeed() == opts::SPEED_KMH,
                                                                          metric && Unit::getUnitAlt() == opts::ALT_METER));
  }

  // Add STAR
  if(options.testFlag(rs::SID_STAR) && !star.isEmpty())
  {
    if(options.testFlag(rs::STAR_REV_TRANSITION))
    {
      // Reverse trans.STAR notation like it is used by SimBrief
      if(!starTrans.isEmpty() && sidStarSpace && starTrans == items.value(items.size() - 1))
        // Avoid duplicate of STAR transition and previouse waypoint if using space separated notation and reversed
        items.append(star);
      else
      {
        if(sidStarSpace)
        {
          if(!starTrans.isEmpty())
            items.append(starTrans);
          items.append(star);
        }
        else
          items.append((starTrans.isEmpty() ? QString() : starTrans % '.') % star);
      }
    }
    else
    {
      // Not reversed STAR.trans notation
      if(sidStarSpace)
      {
        items.append(star);
        if(!starTrans.isEmpty())
          items.append(starTrans);
      }
      else
        items.append(star % (starTrans.isEmpty() ? QString() : '.' % starTrans));
    }
  }
  else if(options.testFlag(rs::SID_STAR_GENERIC))
    items.append("STAR");

  // Remove last DCT for flight factor export
  if(options.testFlag(rs::NO_FINAL_DCT) && items.constLast() == "DCT")
    items.removeLast();

  // Add destination airport
  if(options.testFlag(rs::START_AND_DEST))
  {
    // Append slash separated approach descriptor if requested
    QString approachDest;
    if(options.testFlag(rs::WRITE_APPROACH_RUNWAYS))
    {
      if(route.hasAnyApproachProcedure())
      {
        if(route.hasCustomApproach())
          approachDest = '/' % customApprRw;
        else
          approachDest += route.getFullApproachName();
      }
      else if(route.hasAnyStarProcedure())
        approachDest += '/' % route.getStarRunwayName();
    }

    items.append(lastId % approachDest % (gfpCoords ? ',' % coords::toGfpFormat(lastPos) : QString()));
  }

  if(!route.hasCustomApproach()) // Do not add custom approach
  {
    if(options.testFlag(rs::CORTEIN_APPROACH) && !approachName.isEmpty())
    {
      items.append(approachName);
      if(!approachTransition.isEmpty())
        items.append(approachTransition);
    }
  }

  if(options.testFlag(rs::ALTERNATES))
    items.append(route.getAlternateDisplayIdents()); // Internal ident or ICAO

  if(options.testFlag(rs::FLIGHTLEVEL))
    items.append(QString("FL%1").arg(static_cast<int>(route.getCruiseAltitudeFt()) / 100));

  // Remove empty and consecutive duplicates ===========================
  items.removeAll(QString());
  items.erase(std::unique(items.begin(), items.end()), items.end());

  qDebug() << Q_FUNC_INFO << items;

  return items;
}
