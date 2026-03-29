/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
#include "atools.h"
#include "common/unit.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "query/airportquery.h"
#include "query/querymanager.h"
#include "route/route.h"

#include <QStringBuilder>

using atools::geo::Pos;
namespace coords = atools::fs::util;

QString RouteStringWriter::createGfpStringForFlightplan(const atools::fs::pln::Flightplan& flightplan) const
{
  // FLP and GFP are a sort of route string
  // New waypoints along airways have to be inserted and waypoints have to be resolved without coordinate backup
  // KYKM/09 WENAS7.PERTT MIVSE FOGIG EAT 4741N12051W DROGA KRUZR GLASR TUNNL 4804N12114W
  // 4805N12118W 4805N12121W 4807N12128W 4808N12121W 4807N12116W 4806N12109W
  // ROZSE 6S9 DIABO J503 FOLDY PIGLU5.YDC CYLW/HUMEK.I16-Z CYYF CZGF

  // Create a simple route string ====================================
  QStringList routeString;

  // Add departure airport, runway and SID - "KYKM/09 WENAS7.PERTT"  =========================
  const QHash<QString, QString>& props = flightplan.getPropertiesConst();
  routeString.append(atools::strJoin({flightplan.constFirst().getIdent(), props.value(atools::fs::pln::SID_RW)}, '/'));
  routeString.append(atools::strJoin({props.value(atools::fs::pln::SID), props.value(atools::fs::pln::SID_TRANS)}, '.'));

  // Add enroute waypoints =========================
  for(int i = 1; i < flightplan.size() - 1; i++)
  {
    const atools::fs::pln::FlightplanEntry& entry = flightplan.at(i);
    if(!entry.getAirway().isEmpty())
      routeString.append(entry.getAirway());
    routeString.append(entry.getIdent());
  }

  // Add STAR - "PIGLU5.YDC" =========================
  routeString.append(atools::strJoin({props.value(atools::fs::pln::STAR), props.value(atools::fs::pln::STAR_TRANS)}, '.'));

  // Add destination and approach - "CYLW/HUMEK.I16-Z" =========================
  routeString.append(atools::strJoin({flightplan.constLast().getIdent(),
                                      atools::strJoin({props.value(atools::fs::pln::TRANSITION),
                                                       props.value(atools::fs::pln::APPROACH_ARINC)}, '.')}, '/'));

  // Remove empty strings - allow duplicates
  routeString.removeAll(QStringLiteral());

  // Try to resolve departure and destination airport if given as coordinates by formats save as waypoints =========
  // Get the nearest airport at the coordinate
  AirportQuery *airportQuery = QueryManager::instance()->getQueriesGui()->getAirportQuerySim();
  Pos departurePos = atools::fs::util::fromGfpFormat(routeString.constFirst());
  if(departurePos.isValidRange())
  {
    const map::MapResultIndex *index = airportQuery->getNearestAirportObjects(departurePos, 5);
    if(index != nullptr && !index->isEmpty())
      routeString.first() = index->constFirst()->asObj<map::MapAirport>().ident;
  }

  Pos destinationPos = atools::fs::util::fromGfpFormat(routeString.constLast());
  if(destinationPos.isValidRange())
  {
    const map::MapResultIndex *index = airportQuery->getNearestAirportObjects(destinationPos, 5);
    if(index != nullptr && !index->isEmpty())
      routeString.last() = index->constFirst()->asObj<map::MapAirport>().ident;
  }

  return routeString.join(QStringLiteral(" "));
}

QString RouteStringWriter::createStringForRoute(const Route& route, float speedKts, rs::RouteStringOptions options) const
{
  if(route.isEmpty())
    return QStringLiteral();

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

QString RouteStringWriter::createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption, bool gfpCoordinates,
                                                   bool departDestAsCoords) const
{
  if(route.isEmpty())
    return QStringLiteral();

  if(procedures)
    return createGfpStringForRouteInternalProc(route, userWaypointOption, gfpCoordinates, departDestAsCoords);
  else
    return createGfpStringForRouteInternal(route, userWaypointOption);
}

QString RouteStringWriter::createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption, bool gfpCoordinates,
                                                               bool departDestAsCoords) const
{
  QString retval;
  rs::RouteStringOptions options = rs::GFP | rs::DCT;

  if(userWaypointOption)
    options |= rs::USR_WPT | rs::NO_AIRWAYS;

  if(gfpCoordinates)
    options |= rs::GFP_COORDS;

  // Get string without start and destination
  QStringList string = createStringForRouteInternal(route, 0, options);

  qDebug() << Q_FUNC_INFO << string;

  // Remove any useless DCTs
  if(!string.isEmpty() && string.constLast() == QStringLiteral("DCT"))
    string.removeLast();

  if(!string.isEmpty() && string.constFirst() == QStringLiteral("DCT"))
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
          if(str == QStringLiteral("DCT"))
            retval += QStringLiteral(":F:");
          else
            retval += '.' % str % '.';
        }
      }
    }
    else
      retval += QStringLiteral(":F:");

    retval += string.constLast();
  }

  // Add procedures and airports
  QString sid, sidTrans, star, starTrans;
  route.getSidStarNames(sid, sidTrans, star, starTrans);

  QString departureRw = atools::fs::util::normalizeRunway(route.getDepartureRunwayName());
  QString starRw = atools::fs::util::normalizeRunway(route.getStarRunwayName());

  if(route.hasCustomDeparture())
  {
    sid.clear();
    sidTrans.clear();
    departureRw.clear();
  }

  // Departure ===============================
  if(!retval.isEmpty() && !retval.startsWith(QStringLiteral(":F:")))
    retval.prepend(QStringLiteral(":F:"));

  if(route.hasAnySidProcedure() && !route.hasCustomDeparture() && !userWaypointOption)
  {
    if(!departureRw.isEmpty() && !(departureRw.endsWith('L') || departureRw.endsWith('C') || departureRw.endsWith('R')))
      departureRw.append('O');

    // Add SID and departure airport
    retval.prepend(QStringLiteral("FPN/RI:DA:") % route.getDepartureAirportLeg().getIdent() %
                   QStringLiteral(":D:") % sid % (sidTrans.isEmpty() ? QStringLiteral() : '.' % sidTrans) %
                   (departureRw.isEmpty() ? QStringLiteral() : QStringLiteral(":R:") % departureRw));
  }
  else
  {
    // Add departure airport only - no coordinates since these are not accurate - exception "TDS GTNXi with user defined waypoints"
    retval.prepend(QStringLiteral("FPN/RI:F:") % legIdent(route.getDepartureAirportLeg(), options,
                                                          departDestAsCoords, true /* departure */, false /* destination */));
  }

  if((route.hasAnyApproachProcedure() || route.hasAnyStarProcedure()) && !userWaypointOption)
  {
    // Arrival airport - no coordinates
    retval.append(QStringLiteral(":AA:") % route.getDestinationAirportLeg().getIdent());

    // STAR ===============================
    if(route.hasAnyStarProcedure())
    {
      if(!starRw.isEmpty() && !(starRw.endsWith('L') || starRw.endsWith('C') || starRw.endsWith('R')))
        starRw.append('O');
      retval.append(QStringLiteral(":A:") % star % (starTrans.isEmpty() ? QStringLiteral() : '.' % starTrans) %
                    (starRw.isEmpty() ? QStringLiteral() : '(' % starRw % ')'));
    }

    // Approach ===============================
    if(route.hasAnyApproachProcedure() && !route.hasCustomApproach())
    {
      QString apprArinc, apprTrans, appSuffixDummy;
      route.getApproachNames(apprArinc, apprTrans, appSuffixDummy);
      retval.append(QStringLiteral(":AP:") % apprArinc % (apprTrans.isEmpty() ? QStringLiteral() : '.' % apprTrans));
    }
  }
  else
  {
    if(!retval.endsWith(QStringLiteral(":F:")))
      retval.append(QStringLiteral(":F:"));

    // Arrival airport only - no coordinates since these are not accurate - exception "TDS GTNXi with user defined waypoints"
    retval.append(legIdent(route.getDestinationAirportLeg(), options, departDestAsCoords, false /* departure */, true /* destination */));
  }

  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

QString RouteStringWriter::createGfpStringForRouteInternal(const Route& route, bool userWaypointOption) const
{
  QString retval;

  rs::RouteStringOptions opts = rs::GFP | rs::START_AND_DEST | rs::DCT;
  if(userWaypointOption)
    opts |= rs::USR_WPT | rs::NO_AIRWAYS;

  // Get string including start and destination
  QStringList string = createStringForRouteInternal(route, 0, opts);

  if(!string.isEmpty())
  {
    retval += QStringLiteral("FPN/RI:F:") % string.constFirst();

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
          retval += str == QStringLiteral("DCT") ? QStringLiteral(":F:") : QStringLiteral(".") + str + QStringLiteral(".");
      }
    }
    else
      retval += QStringLiteral(":F:");

    retval += string.constLast();
  }
  qDebug() << Q_FUNC_INFO << retval;
  return retval.toUpper();
}

QStringList RouteStringWriter::createStringForRouteInternal(const Route& routeParam, float speedKts, rs::RouteStringOptions options) const
{
  qDebug() << Q_FUNC_INFO << options << "speedKts" << speedKts;

  Route route = routeParam.adjustedToOptions(rf::DEFAULT_OPTS_ROUTESTRING);

  QStringList items;
  QString sid, sidTrans, star, starTrans, approachName, approachTransition, approachSuffix, departureRunway, destinationRunway;

  // Get approach runway and information
  if(route.hasCustomApproach())
    destinationRunway = atools::fs::util::normalizeRunway(route.getApproachRunwayName());
  else
  {
    route.getApproachNames(approachName, approachTransition, approachSuffix);
    destinationRunway = atools::fs::util::normalizeRunway(route.getStarRunwayName());
  }

  // Get departure runway and/or information
  route.getSidStarNames(sid, sidTrans, star, starTrans);

  if(route.hasCustomDeparture())
  {
    // Omit SID for custom departure
    sid.clear();
    sidTrans.clear();
  }

  departureRunway = atools::fs::util::normalizeRunway(route.getDepartureRunwayName());

  if(route.hasAnyApproachProcedure() && !route.getApproachLegs().type.isEmpty() && !route.hasCustomApproach())
  {
    // FlightFactor specialities - there are probably more to guess
    if(route.getApproachLegs().type == QStringLiteral("RNAV"))
      approachName = QStringLiteral("RNV") % destinationRunway;
    else
      approachName = route.getApproachLegs().type % destinationRunway;
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
    // CYCD DCT DUNCN V440 YYJ V495 CDGPN DCT N48194W123096 DCT WATTR V495 JAWBN DCT 0S9
    QString ident = legIdent(leg, options, false /* departDestAsCoords */, false /* departure */, false /* destination */);

    if(airway.isEmpty() || leg.isAirwaySetAndInvalid(map::INVALID_ALTITUDE_VALUE) || options.testFlag(rs::NO_AIRWAYS))
    {
      // Do not use  airway string if not found in database
      if(!lastId.isEmpty())
      {
        if(userWpt && lastIndex > 0)
          items.append(coords::toGfpFormat(lastPos));
        else
          items.append(lastId % (gfpCoords && lastType != map::USERPOINTROUTE ? ',' % coords::toGfpFormat(lastPos) : QStringLiteral()));

        if(lastIndex == 0 && options.testFlag(rs::CORTEIN_DEPARTURE_RUNWAY) && !departureRunway.isEmpty() && !route.hasCustomDeparture())
          // Add runway after departure
          items.append(departureRunway);
      }

      if(i > 0 && options.testFlag(rs::DCT))
      {
        if(!(firstDct && hasSid))
          // Add direct if not start airport - do not add where a SID is inserted
          items.append(QStringLiteral("DCT"));
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
          items.append(lastId % (gfpCoords && lastType != map::USERPOINTROUTE ? ',' % coords::toGfpFormat(lastPos) : QStringLiteral()));

        // Convert track characters into NAT notation like "A" to "NATA" if tracks are available, enabled and if
        // the coordinates are in the right range
        if(leg.isTrack() && NavApp::hasNatTracks() && airway.size() == 1 && airway.at(0) >= QChar('A') && airway.at(0) <= QChar('Z') &&
           atools::inRange(-80.f, 0.f, leg.getPosition().getLonX()))
          items.append(QStringLiteral("NAT") % airway);
        else
          items.append(airway);
      }
      // else Airway is the same skip waypoint
    }

    // Append runway descriptor to departure airport if a custom runway is given
    // SID runway for real departure procedures is added above
    if(i == 0 && options.testFlag(rs::WRITE_RUNWAYS) && !departureRunway.isEmpty())
      ident += '/' % departureRunway;

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
    if(hasStar && items.constLast() == QStringLiteral("DCT"))
      // Remove last DCT so it does not collide with the STAR - destination airport not inserted yet
      items.removeLast();

    if(options.testFlag(rs::CORTEIN_APPROACH) && items.constLast() == QStringLiteral("DCT"))
      // Remove last DCT if approach information is desired
      items.removeLast();

    if(options.testFlag(rs::CORTEIN_DEPARTURE_RUNWAY) && !departureRunway.isEmpty())
      insertPosition++;
  }

  // Get transition separator
  bool sidStarSpace = options.testFlag(rs::SID_STAR_SPACE);

  if(!route.getSidLegs().isCustomDeparture() && options.testFlag(rs::SID_STAR) && !sid.isEmpty()) // Do not add custom departure as SID
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
        items.insert(insertPosition, sid % (sidTrans.isEmpty() ? QStringLiteral() : '.' % sidTrans));
    }
  }
  else if(options.testFlag(rs::SID_STAR_GENERIC))
    items.insert(insertPosition, QStringLiteral("SID"));

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
          items.append((starTrans.isEmpty() ? QStringLiteral() : starTrans % '.') % star);
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
        items.append(star % (starTrans.isEmpty() ? QStringLiteral() : '.' % starTrans));
    }
  }
  else if(options.testFlag(rs::SID_STAR_GENERIC))
    items.append(QStringLiteral("STAR"));

  // Remove last DCT for FlightFactor export
  if(options.testFlag(rs::NO_FINAL_DCT) && items.constLast() == QStringLiteral("DCT"))
    items.removeLast();

  // Add destination airport
  if(options.testFlag(rs::START_AND_DEST))
  {
    // Append slash separated approach descriptor if requested
    QString approachDest;
    if(options.testFlag(rs::WRITE_RUNWAYS))
    {
      if(route.hasAnyApproachProcedure())
      {
        if(route.hasCustomApproach())
        {
          if(!destinationRunway.isEmpty())
            approachDest = '/' % destinationRunway;
        }
        else
          approachDest = route.getFullApproachName();
      }
      else if(route.hasAnyStarProcedure())
      {
        QString starRunway = atools::fs::util::normalizeRunway(route.getStarRunwayName());
        if(!starRunway.isEmpty())
          approachDest = '/' % starRunway;
      }
    }

    items.append(lastId % approachDest % (gfpCoords ? ',' % coords::toGfpFormat(lastPos) : QStringLiteral()));
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
    items.append(QStringLiteral("FL%1").arg(static_cast<int>(route.getCruiseAltitudeFt()) / 100));

  // Remove empty and consecutive duplicates ===========================
  items.removeAll(QStringLiteral());
  items.erase(std::unique(items.begin(), items.end()), items.end());

  qDebug() << Q_FUNC_INFO << items;

  return items;
}

QString RouteStringWriter::legIdentCoords(const RouteLeg& leg, rs::RouteStringOptions options) const
{
  if(options.testFlag(rs::GFP))
    return coords::toGfpFormat(leg.getPosition());
  else if(options.testFlag(rs::SKYVECTOR_COORDS))
    return coords::toDegMinSecFormat(leg.getPosition());
  else
    return coords::toDegMinFormat(leg.getPosition());
}

QString RouteStringWriter::legIdent(const RouteLeg& leg, rs::RouteStringOptions options, bool departDestAsCoords, bool departure,
                                    bool destination) const
{
  if(departure || destination)
  {
    // Either departure or destination airport
    if(departDestAsCoords)
      // Store as coordinates for TDSGTNXIWP
      return legIdentCoords(leg, options);
    else
      return leg.getIdent();
  }
  else if(leg.getMapType() == map::USERPOINTROUTE)
    // User points are always stored as coordinate
    return legIdentCoords(leg, options);
  else
    return leg.getDisplayIdent();
}
