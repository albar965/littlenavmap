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

#include "routestring/routestringreader.h"

#include "common/mapresult.h"
#include "common/maptools.h"
#include "common/proctypes.h"
#include "common/unit.h"
#include "fs/pln/flightplan.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "app/navapp.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "query/waypointtrackquery.h"
#include "route/flightplanentrybuilder.h"
#include "util/htmlbuilder.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QStringBuilder>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using map::MapResult;
using atools::geo::Pos;
namespace coords = atools::fs::util;
namespace ageo = atools::geo;
namespace plnentry = atools::fs::pln::entry;

// Maximum distance to previous waypoint - everything above will be sorted out
const static float MAX_WAYPOINT_DISTANCE_NM = 5000.f;

// Do not select alternate beyond 1000 NM
const static float MAX_ALTERNATE_DISTANCE_NM = 1000.f;

// Choose best navaid from a cluster with this distance.
// Used to prioritize VOR and waypoints before NDB with the same name
const static float MAX_CLOSE_NAVAIDS_DISTANCE_NM = 5.f;

const static QRegularExpression SPDALT_WAYPOINT("^([A-Z0-9]+)/[NMK]\\d{3,4}[FSAM]\\d{3,4}$");

// Time specification directly after airport - ignored
// Runway specification directly after airport. Example: USSS/08R ... ZMUB1200/33
const static QRegularExpression AIRPORT_TIME_RUNWAY("^(?<ap>[A-Z0-9]{3,4})" // Required airport
                                                    "(?<time>(\\d{4}))?" // Optional time - ignored
                                                    "(/(?<rw>[LCRTABW0-9]{2,3}))?$"); // Optional runway

// GCTS1200/TES2.I07-Y
const static QRegularExpression AIRPORT_TIME_APPROACH("^(?<ap>[A-Z0-9]{3,4})" // "GCTS" Required airport
                                                      "(?<time>\\d{4})?" // "1200" Optional time - ignored
                                                      "(/((?<trans>[A-Z0-9]+)\\.)?(?<appr>([A-Z][A-Z0-9\\-]*)))?$"); // Optional approach "TES2.I07-Y"

const static QRegularExpression SID_STAR_TRANS("^([A-Z0-9]{1,7})(\\.([A-Z0-9]{1,6}))?$");

const static map::MapTypes ROUTE_TYPES_AND_AIRWAY(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE | map::AIRWAY);

const static map::MapTypes ROUTE_TYPES(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);

// No airports
const static map::MapTypes ROUTE_TYPES_NAVAIDS(map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);
const static std::initializer_list<map::MapTypes> ROUTE_TYPES_NAVAIDS_LIST =
{map::WAYPOINT, map::VOR, map::NDB, map::USERPOINTROUTE};

/* ========================================================================================
 * Combines all features like airport, STAR, approach and runway found for a runway item */
struct DestInfo
{
  DestInfo(const proc::MapProcedureLegs& starParam, const proc::MapProcedureLegs& apprParam, const map::MapAirport& apParam,
           const map::MapRunwayEnd& runwayEndParam, int consumeParam)
    : star(starParam), approach(apprParam), airport(apParam), consume(consumeParam), runwayEnd(runwayEndParam)
  {
  }

  proc::MapProcedureLegs star, approach;

  // List of consecutive airports at end of string - destination is last
  map::MapAirport airport;

  // Number of entries that have to be consumed for SID and/or transition: 0, 1, or 2
  int consume;

  map::MapRunwayEnd runwayEnd;
};

/* ========================================================================================
 * Map object by type and distance */
struct TypeDist
{
  TypeDist(const ageo::Pos& posParam, const ageo::Pos& lastPosParam, const ageo::Pos& destPosParam, map::MapTypes typeParam,
           map::MapWaypointArtificial artificialParam = map::WAYPOINT_ARTIFICIAL_NONE)
    : pos(posParam), type(typeParam), artificial(artificialParam)
  {
    // From last point to this one
    distMeter = lastPosParam.distanceMeterTo(posParam);

    if(destPosParam.isValid())
      // From this point to destination
      distMeter += posParam.distanceMeterTo(destPosParam);
  }

  float distMeter;
  Pos pos;
  map::MapTypes type;
  map::MapWaypointArtificial artificial;
};

/*  ========================================================================================
 * Internal parsing structure which holds all found potential candidates from a search */
struct RouteStringReader::ParseEntry
{
  QString item, airway;
  map::MapResult result;
};

// ========================================================================================
RouteStringReader::RouteStringReader(FlightplanEntryBuilder *flightplanEntryBuilder)
  : entryBuilder(flightplanEntryBuilder)
{
  mapQuery = NavApp::getMapQueryGui();
  airportQuerySim = NavApp::getAirportQuerySim();
  airportQueryNav = NavApp::getAirportQueryNav();
  procQuery = NavApp::getProcedureQuery();

  // Create a copy of the delegate which uses the same AirwayQuery objects
  // This allows to disable track reading in the copy (setUseTracks())
  airwayQuery = new AirwayTrackQuery(*NavApp::getAirwayTrackQueryGui());
  waypointQuery = new WaypointTrackQuery(*NavApp::getWaypointTrackQueryGui());
}

RouteStringReader::~RouteStringReader()
{
  delete airwayQuery;
  delete waypointQuery;
}

bool RouteStringReader::createRouteFromString(const QString& routeString, rs::RouteStringOptions options,
                                              atools::fs::pln::Flightplan *flightplan, map::MapRefExtVector *mapObjectRefs,
                                              float *speedKtsParam, bool *altIncludedParam)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  QElapsedTimer timer;
  timer.start();

  airwayQuery->setUseTracks(!(options & rs::NO_TRACKS));
  waypointQuery->setUseTracks(false);

  logMessages.clear();
  warningMessages.clear();
  errorMessages.clear();

  // Cleanup and take all until the first empty line
  QStringList items = rs::cleanRouteStringList(routeString);

  // Create a pointer to temporary plan or passed pointer to plan
  // Avoids null pointer and prepare for reuse of flight plan attributes
  Flightplan *fp, tempFp;
  fp = flightplan != nullptr ? flightplan : &tempFp;

#ifdef DEBUG_INFORMATION
  map::MapRefExtVector refTemp;
  mapObjectRefs = mapObjectRefs != nullptr ? mapObjectRefs : &refTemp;

  qDebug() << "items" << items;
#endif

  // Remove all unneeded adornment like speed and times and also combine waypoint coordinate pairs
  // Also extracts speed, altitude, SID and STAR - Keeps airport runway and approach information intact
  float altitudeFt, speedKts;
  QStringList cleanItems = cleanItemList(items, speedKts, altitudeFt);

  if(speedKtsParam != nullptr)
    *speedKtsParam = speedKts;

  if(altIncludedParam != nullptr)
    *altIncludedParam = altitudeFt > 0.f;

  if(altitudeFt > 0.f)
    fp->setCruiseAltitudeFt(altitudeFt);

#ifdef DEBUG_INFORMATION
  qDebug() << "clean items" << cleanItems;
#endif

  if(cleanItems.size() < 2)
  {
    appendError(tr("Need at least departure and destination."));
    return false;
  }

  bool readNoAirports = options & rs::READ_NO_AIRPORTS;
  Pos lastPos;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after prepare" << timer.restart();
#endif

  QList<atools::fs::pln::FlightplanEntry> alternates;

  if(readNoAirports)
    // Try to get first position by various queries if no airports are given
    lastPos = findFirstCoordinate(cleanItems);
  else
  {
    // Get airports as well as SID and STAR ============================================
    // Insert first as departure
    QString sidTransWp; // remember item which was consumed as a SID transition in space separated notation
    if(!addDeparture(fp, mapObjectRefs, cleanItems, sidTransWp))
      return false;

    if(!sidTransWp.isEmpty())
      // Append consumed SID transition item again to allow next transition to potentially take the same item as STAR transition
      cleanItems.prepend(sidTransWp);

    // Append last as destination
    QString starTransWp; // Item which was consumed as a STAR transition in space separated notation "TRANS STAR"
    if(!addDestination(fp, &alternates, mapObjectRefs, cleanItems, starTransWp, options))
      return false;

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "sidTransWp" << sidTransWp << "starTransWp" << starTransWp;
    qDebug() << Q_FUNC_INFO << "cleanItems" << cleanItems;
#endif

    if(sidTransWp == starTransWp && !cleanItems.isEmpty() && cleanItems.first() == sidTransWp)
      // Item was appended and consumed for SID an STAR transition
      // Example: "MAXIM" in "MUHA EPMAR3 MAXIM SNDBR2 KMIA"
      cleanItems.removeFirst();
    else if(!starTransWp.isEmpty())
      // Add STAR transition waypoint in case of airway presence
      // Example: "ZZIPR" in "7L2 UFFDA Q156 ZZIPR FYTTE7 KORD"
      cleanItems.append(starTransWp);

    lastPos = fp->getDeparturePosition();
  }

  // Cruise speed =====================================================================
  if(speedKtsParam != nullptr && *speedKtsParam > 0.f)
  {
    float cruise = NavApp::getRouteCruiseSpeedKts();
    if(atools::almostEqual(*speedKtsParam, cruise, 1.f))
      appendWarning(tr("Ignoring speed instruction %1.").arg(Unit::speedKts(*speedKtsParam)));
    else
      // Detected speed differs from aircraft performance
      appendWarning(tr("Ignoring speed instruction %1 in favor of aircraft performance which uses %2 true airspeed.").
                    arg(Unit::speedKts(*speedKtsParam)).arg(Unit::speedKts(NavApp::getRouteCruiseSpeedKts())));
  }

  // Do not get any navaids that are too far away
  float maxDistance = ageo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM, fp->getDistanceNm() * 2.0f));

  if(!lastPos.isValid())
  {
    appendError(tr("Could not determine starting position for first item %1.").
                arg(cleanItems.isEmpty() ? QString() : cleanItems.constFirst()));
    return false;
  }

  // Remove consecutive duplicates ===========================================
  cleanItems.erase(std::unique(cleanItems.begin(), cleanItems.end()), cleanItems.end());

  // Collect all navaids, airports and coordinates into MapSearchResults =============================

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after start" << timer.restart();
#endif

  QList<ParseEntry> resultList;
  for(const QString& item : qAsConst(cleanItems))
  {
    // Fetch all possible waypoints
    MapResult result;
    findWaypoints(result, item, options & rs::READ_MATCH_WAYPOINTS);

    // Get last result so we can check for airway/waypoint matches when selecting the last position
    // The nearest is used if no airway matches
    const MapResult *lastResult = resultList.isEmpty() ? nullptr : &resultList.constLast().result;

    // Sort lists by distance and remove all which are too far away and update last pos
    filterWaypoints(result, lastPos, fp->getDestinationPosition(), lastResult, item, maxDistance);

    if(!result.isEmpty(ROUTE_TYPES_AND_AIRWAY))
      resultList.append({item, QString(), result});
    else
      appendWarning(tr("Nothing found for %1. Ignoring.").arg(item));
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after collecting navaids" << timer.restart();
#endif

  // Check for wrong inital airways in first and last entry =============================
  // resultList does not contain start and destination airport or procedures at this point
  if(!resultList.isEmpty())
  {
    ParseEntry& first = resultList.first();
    if(first.result.hasAirways())
    {
      if(first.result.hasNavaids() || first.result.hasAirports())
        // There are other navaids or airports - clear airway information
        first.result.airways.clear();
      else
        // Only airway found - show message
        appendWarning(tr("No waypoint before airway %1. Ignoring flight plan segment.").arg(first.result.airways.first().name));
    }

    ParseEntry& last = resultList.last();
    if(last.result.hasAirways())
    {
      if(last.result.hasNavaids() || last.result.hasAirports())
        // There are other navaids or airports - clear airway information
        last.result.airways.clear();
      else
        // Only airway found - show message
        appendWarning(tr("No waypoint after airway %1. Ignoring flight plan segment.").arg(last.result.airways.first().name));
    }
  }

  // ==========================================================
  // Create airways - will fill the waypoint list in result with airway points
  // if airway is invalid it will be erased in result
  // Will erase NDB, VOR and adapt waypoint list in result if an airway/waypoint match was found
  // List of airways is unchanged
  for(int i = readNoAirports ? 0 : 1; i < resultList.size() - 1; i++)
    filterAirways(resultList, i);

#ifdef DEBUG_INFORMATION

  for(const ParseEntry& parse : qAsConst(resultList))
    qDebug() << parse.item << parse.airway << parse.result;

#endif

  // Remove now unneeded results after airway evaluation
  removeEmptyResults(resultList);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after filter airways" << timer.restart();
#endif

  // Now build the flight plan =================================================================
  lastPos = fp->getDeparturePosition();

  // Either append (if no airports) or append before destination
  int insertOffset = readNoAirports ? 0 : 1;

  map::MapRefExt lastRef;
  for(int i = 0; i < resultList.size(); i++)
  {
    const QString& item = resultList.at(i).item;
    const MapResult& result = resultList.at(i).result;
    const ParseEntry *lastParseEntry = i > 0 ? &resultList.at(i - 1) : nullptr;
    map::MapRefExt curRef;

    // Add waypoints on airways =======================================
    if(result.hasAirways())
    {
      if(i == 0)
        appendWarning(tr("Found airway %1 instead of waypoint as first entry in en-route list. Ignoring.").arg(item));
      else
      {
        // Add all airway waypoints if any were found =================================
        for(const map::MapWaypoint& wp : result.waypoints)
        {
          // Will fetch objects in order: airport, waypoint, vor, ndb
          // Only waypoints are relevant since airways are used here
          FlightplanEntry entry;
          entryBuilder->entryFromWaypoint(wp, entry, true);

          if(entry.getPosition().isValid())
          {
            entry.setAirway(item);
            entry.setFlag(atools::fs::pln::entry::TRACK, result.airways.constFirst().isTrack());
            fp->insert(fp->size() - insertOffset, entry);

            // Build reference list if required
            if(mapObjectRefs != nullptr)
            {
              int insertPos = mapObjectRefs->size() - insertOffset;

              // Airway reference precedes waypoint
              QList<map::MapAirway> airways;
              airwayQuery->getAirwaysForWaypoints(airways, lastRef.id, wp.id, item);
              if(!airways.isEmpty())
                atools::insertInto(*mapObjectRefs, insertPos,
                                   map::MapRefExt(airways.constFirst().id, map::AIRWAY, airways.constFirst().name));

              insertPos = mapObjectRefs->size() - insertOffset;
              // Waypoint
              curRef = map::MapRefExt(wp.id, wp.position, map::WAYPOINT, wp.ident);
              atools::insertInto(*mapObjectRefs, insertPos, curRef);
            }
          }
          else
            appendWarning(tr("No navaid found for %1 on airway %2. Ignoring.").arg(wp.ident).arg(item));
          lastRef = curRef;
        }
      }
    }
    else
    {
      // Add a single waypoint from direct =======================================
      FlightplanEntry entry;
      if(!result.userpointsRoute.isEmpty())
      {
        // User point ==========================================

        // User entries are always a perfect match
        // Convert a coordinate to a user defined waypoint
        entryBuilder->buildFlightplanEntry(result.userpointsRoute.constFirst().position, result, entry, true, map::NONE);

        // Build reference list if required
        if(mapObjectRefs != nullptr)
        {
          // Add user defined waypoint with original coordinate string
          curRef = map::MapRefExt(-1, entry.getPosition(), map::USERPOINTROUTE, item);
          atools::insertInto(*mapObjectRefs, mapObjectRefs->size() - insertOffset, curRef);
        }

        // Use the original string as name but limit it for fs
        entry.setIdent(item);
      }
      else
      {
        // Navaid  ==========================================
        // Get nearest navaid and build entry from it - converts V and N waypoints to VOR and NDB
        buildEntryForResult(entry, result, lastPos, true /* resolveWaypoints */);

        // Build reference list if required
        if(mapObjectRefs != nullptr)
        {
          // Build entry from it - do not convert waypoints to VOR or NDB
          FlightplanEntry entryRef;
          buildEntryForResult(entryRef, result, lastPos, false /* resolveWaypoints */);
          curRef = mapObjectRefFromEntry(entryRef, result, item);

          if(lastParseEntry != nullptr && lastParseEntry->result.hasAirways())
          {
            // Insert airway entry leading towards following waypoint
            QList<map::MapAirway> airways;
            airwayQuery->getAirwaysForWaypoints(airways, lastRef.id, curRef.id, lastParseEntry->airway);
            if(!airways.isEmpty())
              atools::insertInto(*mapObjectRefs, mapObjectRefs->size() - insertOffset,
                                 map::MapRefExt(airways.constFirst().id, map::AIRWAY, airways.constFirst().name));
          }

          // Add navaid or airport including original name
          atools::insertInto(*mapObjectRefs, mapObjectRefs->size() - insertOffset, curRef);
        }
      }

      if(entry.getPosition().isValid())
      {
        const ParseEntry& parseEntry = resultList.at(i);
        // Assign airway if  this is the end point of an airway
        entry.setAirway(parseEntry.airway);

        if(parseEntry.result.hasAirways())
          entry.setFlag(atools::fs::pln::entry::TRACK, parseEntry.result.airways.constFirst().isTrack());
        fp->insert(fp->size() - insertOffset, entry);
      }
      else
        appendWarning(tr("No navaid found for %1. Ignoring.").arg(item));

      lastRef = curRef;
    }

    // Remember position for distance calculation
    lastPos = fp->at(fp->size() - 1 - insertOffset).getPosition();
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after build flight plan" << timer.restart();
#endif

  // Update departure and destination if no airports are used ==============================
  if(readNoAirports && !fp->isEmpty())
  {
    fp->setDepartureName(fp->constFirst().getIdent());
    fp->setDepartureIdent(fp->constFirst().getIdent());
    fp->setDeparturePosition(fp->constFirst().getPosition());
    fp->setDepartureParkingPosition(fp->constFirst().getPosition(), atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);

    fp->setDestinationName(fp->constLast().getIdent());
    fp->setDestinationIdent(fp->constLast().getIdent());
    fp->setDestinationPosition(fp->constLast().getPosition());
  }

  fp->append(alternates);

  // Report =====================================================================
  if(options.testFlag(rs::REPORT))
    addReport(fp, routeString, altitudeFt);

#ifdef DEBUG_INFORMATION
  qDebug() << "===============================";
  qDebug() << *fp;

  if(mapObjectRefs != nullptr)
  {
    qDebug() << "===============================";
    for(const map::MapRefExt& r : qAsConst(*mapObjectRefs))
      qDebug() << r;
  }
#endif

  return true;
}

const QStringList RouteStringReader::getAllMessages() const
{
  QStringList messages(errorMessages);

  if(!messages.isEmpty() && !warningMessages.isEmpty())
    messages << QString(); // Converted to newline
  messages << warningMessages;

  if(!messages.isEmpty() && !logMessages.isEmpty())
    messages << QString(); // Converted to newline
  messages << logMessages;

  return messages;
}

void RouteStringReader::addReport(atools::fs::pln::Flightplan *flightplan, const QString& rawRouteString, float cruiseAltitudeFt)
{
  QString departAirport, destAirport;
  int insertIndex = 0;

  // Prepare flight plan texts ================================================================================
  // Departure =============================
  if(!flightplan->getDepartureName().isEmpty() && flightplan->getDepartureName() != flightplan->getDepartureIdent())
    // Departure is airport
    departAirport =
      tr("%1 (%2)").arg(flightplan->getDepartureName()).arg(airportQuerySim->getDisplayIdent(flightplan->getDepartureIdent()));
  else
    // Departure is waypoint
    departAirport = flightplan->getDepartureIdent();

  // Destination ============================
  if(!flightplan->getDestinationName().isEmpty() && flightplan->getDestinationName() != flightplan->getDestinationIdent())
    // Destination is airport
    destAirport = tr("%1 (%2)").arg(flightplan->getDestinationName()).arg(airportQuerySim->getDisplayIdent(
                                                                            flightplan->getDestinationIdent()));
  else
    // Departure is waypoint
    destAirport = flightplan->getDestinationIdent();

  insertMessage(tr("Flight plan from <b>%1</b> to <b>%2</b>.").arg(departAirport).arg(destAirport), insertIndex++);
  insertMessage(tr("Distance without procedures is <b>%1</b>.").arg(Unit::distNm(flightplan->getDistanceNm())), insertIndex++);
  if(cruiseAltitudeFt > 0.f)
    insertMessage(tr("Using cruise altitude <b>%1</b> for flight plan.").arg(Unit::altFeet(cruiseAltitudeFt)), insertIndex++);

  // Departure/takeoff ===================================================================================
  const QHash<QString, QString>& properties = flightplan->getPropertiesConst();
  QString customDepartRunway = ProcedureQuery::getCustomDepartureRunway(properties);
  if(!customDepartRunway.isEmpty())
    insertMessage(tr("Takeoff from runway <b>%1</b>.").arg(customDepartRunway), insertIndex++);
  else
  {
    QString sidRunway;
    QString sid = ProcedureQuery::getSidAndTransition(properties, sidRunway);
    if(!sid.isEmpty())
    {
      QString rwMsg = !sidRunway.isEmpty() ? tr(" and runway <b>%1</b>").arg(sidRunway) : QString();
      insertMessage(tr("Depart using SID <b>%1</b>%2.").arg(sid).arg(rwMsg), insertIndex++);
    }
  }

  // Arrival/STAR ===================================================================================
  QString starRunway;
  QString star = ProcedureQuery::getStarAndTransition(properties, starRunway);
  if(!star.isEmpty())
  {
    QString rwMsg = !starRunway.isEmpty() ? tr(" and runway <b>%1</b>").arg(starRunway) : QString();
    insertMessage(tr("Arrive using STAR <b>%1</b>%2.").arg(star).arg(rwMsg), insertIndex++);
  }

  // Approach ===================================================================================
  QString customApprRunway = ProcedureQuery::getCustomApprochRunway(properties);
  if(!customApprRunway.isEmpty())
    insertMessage(tr("Land at runway <b>%1</b>.").arg(customApprRunway), insertIndex++);
  else
  {
    QString approach, transition, runway, arinc;
    ProcedureQuery::getApproachAndTransitionDisplay(properties, approach, arinc, transition, runway);

    if(!transition.isEmpty())
      transition = tr(" via <b>%1</b> to").arg(transition);

    if(!runway.isEmpty())
      runway = tr(", land at runway <b>%1</b>").arg(runway);

    if(!approach.isEmpty())
      insertMessage(tr("Approach%1 <b>%2</b> (<b>%3</b>)%4.").arg(transition).arg(approach).arg(arinc).arg(runway), insertIndex++);
  }

  // Alternates ===================================================================================
  const QVector<const atools::fs::pln::FlightplanEntry *> alternates = flightplan->getAlternates();
  if(!alternates.isEmpty())
  {
    QStringList alternateIdents;
    for(const atools::fs::pln::FlightplanEntry *alternate:alternates)
      alternateIdents.append(alternate->getIdent());

    insertMessage(tr("Alternate %1 <b>%2</b>.").
                  arg(alternateIdents.size() == 1 ? tr("airport is") : tr("airports are")).
                  arg(atools::strJoin(alternateIdents, tr("</b>, <b>"), tr("</b> and <b>"))), insertIndex++);
  }

  // Add empty line ==============
  insertMessage(QString(), insertIndex++);

  // Raw route description and found waypoints ===================================================================================
  insertMessage(tr("Route description:<br/>"
                   "<b>%1</b>.").arg(rs::cleanRouteString(rawRouteString)), insertIndex++);
  QStringList idents;
  for(atools::fs::pln::FlightplanEntry& entry : *flightplan)
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::AIRPORT)
      idents.append(airportQuerySim->getDisplayIdent(entry.getIdent()));
    else
      idents.append(entry.getIdent());
  }

  if(!idents.isEmpty())
  {
#ifdef DEBUG_INFORMATION
    QString identStr = idents.join(tr(" "));
#else
    QString identStr = atools::elideTextShortMiddle(idents.join(tr(" ")), 120);
#endif

    insertMessage(tr("Found %1 %2:<br/><b>%3</b>.").
                  arg(idents.size()).
                  arg(idents.size() == 1 ? tr("waypoint") : tr("waypoints")).
                  arg(identStr), insertIndex++);
  }

}

map::MapRefExt RouteStringReader::mapObjectRefFromEntry(const FlightplanEntry& entry,
                                                        const map::MapResult& result, const QString& name)
{
  atools::fs::pln::entry::WaypointType type = entry.getWaypointType();

  // Look at the entry types to use the same preference as the FlightplanEntryBuilder
  if(type == atools::fs::pln::entry::AIRPORT && result.hasAirports())
    return result.airports.constFirst().getRefExt(name);
  else if(type == atools::fs::pln::entry::WAYPOINT && result.hasWaypoints())
    return result.waypoints.constFirst().getRefExt(name);
  else if(type == atools::fs::pln::entry::VOR && result.hasVor())
    return result.vors.constFirst().getRefExt(name);
  else if(type == atools::fs::pln::entry::NDB && result.hasNdb())
    return result.ndbs.constFirst().getRefExt(name);
  else if(type == atools::fs::pln::entry::USER && result.hasUserpointsRoute())
    return map::MapRefExt(-1, result.userpointsRoute.constFirst().position, map::USERPOINTROUTE, name);
  else
    return map::MapRefExt();
}

void RouteStringReader::buildEntryForResult(FlightplanEntry& entry, const MapResult& result,
                                            const atools::geo::Pos& nearestPos, bool resolveWaypoints)
{
  MapResult newResult;
  resultWithClosest(newResult, result, nearestPos, map::WAYPOINT | map::VOR | map::NDB | map::AIRPORT);
  entryBuilder->buildFlightplanEntry(newResult, entry, resolveWaypoints);
}

void RouteStringReader::resultWithClosest(map::MapResult& resultWithClosest, const map::MapResult& result,
                                          const atools::geo::Pos& nearestPos, map::MapTypes types)
{
  struct DistData
  {
    map::MapTypes type;
    int index;
    ageo::Pos pos;

    const ageo::Pos& getPosition() const
    {
      return pos;
    }

  };

  // Add all navaids to the list with mixed types
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
    const DistData& nearest = sorted.constFirst();

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

void RouteStringReader::insertMessage(const QString& message, int index)
{
  logMessages.insert(index, message);
  qInfo() << Q_FUNC_INFO << "Message:" << message;
}

void RouteStringReader::appendMessage(const QString& message)
{
  logMessages.append(message);
  qInfo() << Q_FUNC_INFO << "Message:" << message;
}

void RouteStringReader::appendWarning(const QString& message)
{
  if(plaintextMessages)
    warningMessages.append(message);
  else
    warningMessages.append(atools::util::HtmlBuilder::warningMessage(message));
  qWarning() << Q_FUNC_INFO << "Warning:" << message;
}

void RouteStringReader::appendError(const QString& message)
{
  if(plaintextMessages)
    errorMessages.append(message);
  else
    errorMessages.append(atools::util::HtmlBuilder::errorMessage(message));
  qWarning() << Q_FUNC_INFO << "Error:" << message;
}

void RouteStringReader::extractAirportIdentDeparture(QString item, QString& airport, QString& runway)
{
  // Copy item to avoid underlying modification if item and airport are the same

  // Try to match AIRPORTTIME/RUNWAY
  QRegularExpressionMatch match = AIRPORT_TIME_RUNWAY.match(item);
  if(match.hasMatch())
  {
    airport = match.captured("ap");

    QString time = match.captured("time");
    if(!time.isEmpty())
      appendWarning(tr("Ignoring time specification \"%1\" for departure airport %2.").arg(time).arg(item));

    runway = match.captured("rw");
  }
  else
    // Return as airport to catch error later
    airport = item;
}

void RouteStringReader::extractAirportIdentDestination(QString item, QString& airport, QString& approach, QString& transition,
                                                       QString& runway)
{
  // Copy item to avoid underlying modification if item and airport are the same
  QString time;
  // Try to match AIRPORTTIME/TRANSITION.APPROACH ===================================
  QRegularExpressionMatch match = AIRPORT_TIME_APPROACH.match(item);
  if(match.hasMatch())
  {
    // Match for full item including time, transition and approach
    // GCTS1200/TES2.I07-Y
    airport = match.captured("ap");
    time = match.captured("time");
    approach = match.captured("appr");
    transition = match.captured("trans");
  }
  else
  {
    // Try to match AIRPORTTIME/RUNWAY =======================================
    match = AIRPORT_TIME_RUNWAY.match(item);
    if(match.hasMatch())
    {
      // Match for full item including time and runway
      // GCTS1200/07
      airport = match.captured("ap");
      runway = match.captured("rw");
      time = match.captured("time");
    }
    else
      // Nothing matched Return as airport to catch error later
      airport = item;
  }

  if(!time.isEmpty())
    appendWarning(tr("Ignoring time specification \"%1\" for destination airport %2.").arg(time).arg(item));
}

void RouteStringReader::airportSim(map::MapAirport& airport, const QString& ident)
{
  airport = map::MapAirport();
  airportQuerySim->getAirportByIdent(airport, ident);
  if(!airport.isValid())
  {
    // Try again with official codes but not ident
    QList<map::MapAirport> airports = airportQuerySim->getAirportsByOfficialIdent(ident, nullptr, map::INVALID_DISTANCE_VALUE,
                                                                                  map::AP_QUERY_OFFICIAL);
    if(!airports.isEmpty())
      airport = airports.constFirst();
  }
}

QString RouteStringReader::sidStarAbbrev(QString sid)
{
  if(sid.size() == 7)
    // Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A
    sid.remove(4, 1);
  return sid;
}

void RouteStringReader::readSidAndTrans(QStringList& items, QString& sidTransWp, int& sidId, int& sidTransId,
                                        const map::MapAirport& departure, const QString& runway)
{
  sidTransId = -1;
  sidId = -1;

  // Check if the next couple of strings are an waypoint/airway triplet
  // Skip procedure detection if yes
  bool foundAirway = items.size() > 2 &&
                     airwayQuery->hasAirwayForNameAndWaypoint(items.at(1), items.at(0)) &&
                     airwayQuery->hasAirwayForNameAndWaypoint(items.at(1), items.at(2));

  if(!foundAirway)
  {
    // Read "SID TRANS" notation if no dot found ======================================================
    if(!items.constFirst().contains('.'))
    {
      QString sid = sidStarAbbrev(items.constFirst());
      QString trans = items.value(1);

      sidId = procQuery->getSidId(departure, sid, runway, true /* strict */);

      if(sidId != -1)
        // Consume SID
        items.removeFirst();

      if(sidId != -1 && !trans.isEmpty())
      {
        sidTransId = procQuery->getSidTransitionId(departure, trans, sidId, true /* strict */);

        if(sidTransId != -1)
          // Consume transition and remember item which was consumed as a SID transition in space separated notation
          sidTransWp = items.takeFirst();
      }
    }

    if(sidId == -1 && !items.isEmpty())
    {
      // Read "SID.TRANS" notation ======================================================
      QString sidTrans = items.constFirst();
      QRegularExpressionMatch sidMatch = SID_STAR_TRANS.match(sidTrans);

      if(sidMatch.hasMatch())
      {
        QString sid = sidStarAbbrev(sidMatch.captured(1));
        QString trans = sidMatch.captured(3);

        sidId = procQuery->getSidId(departure, sid, runway, true /* strict */);
        if(sidId != -1 && !trans.isEmpty())
        {
          sidTransId = procQuery->getSidTransitionId(departure, trans, sidId, true /* strict */);
          if(sidTransId == -1)
            sidId = -1; // Transition not found - invalidate whole SID
        }
      }

      if(sidId != -1)
        // Consume SID and transition
        items.removeFirst();
    }
  } // if(!foundAirway)
}

bool RouteStringReader::addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapRefExtVector *mapObjectRefs, QStringList& items,
                                     QString& sidTransWp)
{
  QString airportIdent, runway;
  extractAirportIdentDeparture(items.takeFirst(), airportIdent, runway);

  map::MapAirport departureAirport;
  airportSim(departureAirport, airportIdent);

  if(departureAirport.isValid())
  {
    // Fill flight plan departure
    flightplan->setDepartureName(departureAirport.name);
    flightplan->setDepartureIdent(departureAirport.ident);
    flightplan->setDeparturePosition(departureAirport.position);
    flightplan->setDepartureParkingPosition(departureAirport.position, atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);

    // Build departure airport entry
    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departureAirport, entry, false /* alternate */);
    flightplan->append(entry);
    if(mapObjectRefs != nullptr)
      mapObjectRefs->append(map::MapRefExt(departureAirport.id, departureAirport.position, map::AIRPORT, departureAirport.ident));

    if(!items.isEmpty())
    {
      // Try to read SID and transition and consume items
      // Pass in runway to allow selection of ambigious SID
      int sidId, sidTransId;
      readSidAndTrans(items, sidTransWp, sidId, sidTransId, departureAirport, runway);

      // Fill procedure into route if found =============================
      proc::MapProcedureLegs arrivalLegsDummy, starLegsDummy, sidLegs;
      if(sidId != -1)
      {
        if(sidId != -1 && sidTransId == -1)
        {
          // Only SID
          const proc::MapProcedureLegs *legs = procQuery->getProcedureLegs(departureAirport, sidId);
          if(legs != nullptr)
            sidLegs = *legs;
        }
        else if(sidId != -1 && sidTransId != -1)
        {
          // SID and transition
          const proc::MapProcedureLegs *legs = procQuery->getTransitionLegs(departureAirport, sidTransId);
          if(legs != nullptr)
            sidLegs = *legs;
        }
      }
      else if(!runway.isEmpty())
      {
        // No SID but runway given with airport
        map::MapRunwayEnd runwayEnd = airportQuerySim->getRunwayEndByName(departureAirport.id, runway);
        // mapQuery->getRunwayEndByNameFuzzy(runwayEnds, runway, airport, true /* navdata */);

        if(runwayEnd.isValid())
          // Create a custom departure with 3 NM leg
          procQuery->createCustomDeparture(sidLegs, departureAirport, runway, 3.f);
        else
        {
          appendWarning(tr("Runway %1 not found for departure airport %2. Ignoring runway.").arg(runway).arg(departureAirport.ident));

          // Clear runway from SID
          sidLegs.runway.clear();

          // Clear to avoid further tests
          runway.clear();
        }
      }

      // Compare SID allowed runways with given runway if this is not a user defined departure
      if(!sidLegs.isEmpty() && !runway.isEmpty() && !sidLegs.isCustomDeparture())
      {
        if(!ProcedureQuery::doesRunwayMatchSidOrStar(sidLegs, runway))
        {
          appendWarning(tr("Runway %1 does not match SID %2 at departure airport %3. Ignoring runway.").
                        arg(runway).arg(sidLegs.procedureFixIdent).arg(airportIdent));
          sidLegs.runway.clear();
          runway.clear();
        }
        else
          sidLegs.runway = runway;
      }

      // Add information to the flight plan property list
      ProcedureQuery::fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegsDummy, starLegsDummy, sidLegs);
    }
    return true;
  }
  else
  {
    if(airportIdent.contains('/'))
      appendError(tr("Required departure airport %1 not found or wrong runway given after \"/\".").arg(airportIdent));
    else
      appendError(tr("Required departure airport %1 not found.").arg(airportIdent));
    return false;
  }
}

bool RouteStringReader::addDestination(atools::fs::pln::Flightplan *flightplan, QList<atools::fs::pln::FlightplanEntry> *alternates,
                                       map::MapRefExtVector *mapObjectRefs, QStringList& items, QString& starTransWp,
                                       rs::RouteStringOptions options)
{
  // Collect information for all trailing airports
  QVector<DestInfo> destInfos;

  // Iterate from end of list and collect airports with possible STAR, runways or approaches in reverse order
  int idx = items.size() - 1;
  while(idx >= 0)
  {
    proc::MapProcedureLegs star, approach;
    map::MapAirport ap;
    map::MapRunwayEnd runwayEnd;

    // Get airport and STAR
    int consume = 0;
    destinationInternal(ap, star, approach, items, starTransWp, runwayEnd, consume, idx);

    if(!star.isEmpty() || !approach.isEmpty())
    {
      // Found a matching STAR or approach - this must be the destination - stop here
      // Approach might also be a user defined one
      destInfos.append(DestInfo(star, approach, ap, runwayEnd, !star.isEmpty() ? consume : -1));
      break;
    }

    if(ap.isValid() && (destInfos.isEmpty() ||
                        destInfos.constFirst().airport.position.distanceMeterTo(ap.position) < ageo::nmToMeter(MAX_ALTERNATE_DISTANCE_NM)))
    {
      // Check if given alternate airport ident matches with any VOR, NDB or WAYPOINT nearby =====================
      QString item = items.value(idx);
      if(item.length() == 3 || item.length() == 5)
      {
        // Get VOR, NDB or waypoints
        map::MapResult result;
        mapQuery->getMapObjectByIdent(result, map::VOR | map::NDB | map::WAYPOINT, items.value(idx), QString(), QString(),
                                      ap.position, ageo::nmToMeter(MAX_ALTERNATE_DISTANCE_NM));
        if(result.hasNavaids())
          // Have overlapping navaid names - ignore airport identifier and stop here
          break;
      }

      // Found a valid airport, add and continue with previous one
      destInfos.append(DestInfo(star, approach, ap, runwayEnd, consume));
      idx--;
    }
    else
      // No valid airport - stop here
      break;

    if(!(options & rs::READ_ALTERNATES))
      // Read airports at end of list as alternates - bail out here after first iteration
      break;
  }

  if(!destInfos.isEmpty())
  {
    // Found airports - Consume all airports in item list =======================================================================
    for(int i = 0; i < destInfos.size() && !items.isEmpty(); i++)
      items.removeLast();

    // Get destination airport from the end of the list ========================
    DestInfo destInfo = destInfos.takeLast();

    // Fill flight plan destination info
    flightplan->setDestinationName(destInfo.airport.name);
    flightplan->setDestinationIdent(destInfo.airport.ident);
    flightplan->setDestinationPosition(destInfo.airport.getPosition());

    // Create destination entry ===============
    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(destInfo.airport, entry, false /* alternate */);
    flightplan->append(entry);
    if(mapObjectRefs != nullptr)
      mapObjectRefs->append(map::MapRefExt(destInfo.airport.id, destInfo.airport.position, map::AIRPORT, destInfo.airport.ident));

    // Check if STAR matches runway for given approach
    if(!destInfo.star.isEmpty() && !destInfo.approach.isEmpty() && !destInfo.approach.isCustomApproach())
    {
      if(!ProcedureQuery::doesRunwayMatchSidOrStar(destInfo.star, destInfo.approach.runway))
      {
        appendWarning(tr("Approach runway %1 does not match STAR %2 at destination airport %3. Ignoring approach.").
                      arg(destInfo.approach.runway).arg(destInfo.star.procedureFixIdent).arg(destInfo.airport.ident));

        // Clear all approach and runway related to ignore when creating flight plan properties
        destInfo.star.runwayEnd = map::MapRunwayEnd();
        destInfo.star.runway.clear();
        destInfo.runwayEnd = map::MapRunwayEnd();
        destInfo.approach = proc::MapProcedureLegs();
      }
      else
        // Assign runway end to STAR to allow disambiguation
        destInfo.star.runwayEnd = destInfo.runwayEnd;
    }

    // Check if STAR allows given runway end
    if(!destInfo.star.isEmpty() && destInfo.runwayEnd.isValid())
    {
      if(!ProcedureQuery::doesRunwayMatchSidOrStar(destInfo.star, destInfo.runwayEnd.name))
      {
        appendWarning(tr("Runway %1 does not match STAR %2 at destination airport %3. Ignoring runway.").
                      arg(destInfo.runwayEnd.name).arg(destInfo.star.procedureFixIdent).arg(destInfo.airport.ident));

        // Clear all approach and runway related to ignore when creating flight plan properties
        destInfo.star.runwayEnd = map::MapRunwayEnd();
        destInfo.star.runway.clear();
        destInfo.runwayEnd = map::MapRunwayEnd();
        destInfo.approach = proc::MapProcedureLegs();
      }
      else
        // Assign runway end to STAR to allow disambiguation
        destInfo.star.runwayEnd = destInfo.runwayEnd;
    }

    // Get destination STAR ========================
    if(!destInfo.star.isEmpty() || !destInfo.approach.isEmpty())
    {
      if(!destInfo.star.isEmpty())
      {
        // Consume STAR in list - either one dot separated transtion or from space separated transition two entries
        for(int i = 0; i < destInfo.consume; i++)
          items.removeLast();
      }

      // Add STAR and approach information to the flight plan property list
      proc::MapProcedureLegs departLegsDummy;
      ProcedureQuery::fillFlightplanProcedureProperties(flightplan->getProperties(), destInfo.approach, destInfo.star, departLegsDummy);
    }

    // Collect remaining alternates ========================
    if(alternates != nullptr)
    {
      QStringList alternateDisplayIdents;
      for(const DestInfo& destInf : destInfos)
      {
        FlightplanEntry altEntry;
        entryBuilder->buildFlightplanEntry(destInf.airport, altEntry, true /* alternate */);
        alternates->prepend(altEntry);

        alternateDisplayIdents.prepend(airportQuerySim->getDisplayIdent(destInf.airport.ident));
      }
    }
  } // if(!airports.isEmpty())
  else
  {
    QString airport = items.isEmpty() ? QString() : items.constLast();
    if(airport.contains('/'))
      appendError(tr("Required destination airport %1 not found or wrong runway given after \"/\".").arg(airport));
    else
      appendError(tr("Required destination airport %1 not found.").arg(airport));
    return false;
  }

  return true;
}

void RouteStringReader::readApproachAndTrans(const QString& approachArinc, const QString& approachTrans,
                                             int& approachId, int& transitionId, const map::MapAirport& destination)
{
  approachId = transitionId = -1;

  if(!approachArinc.isEmpty())
    approachId = procQuery->getApproachId(destination, approachArinc, QString());

  if(approachId != -1 && !approachTrans.isEmpty())
    transitionId = procQuery->getTransitionId(destination, approachTrans, QString(), approachId);
}

void RouteStringReader::readStarAndTransDot(const QString& starTrans, const QString& runway, int& starId, int& starTransId,
                                            const map::MapAirport& destination)
{
  starId = starTransId = -1;

  QRegularExpressionMatch starMatch = SID_STAR_TRANS.match(starTrans);
  if(starMatch.hasMatch())
  {
    QString star = sidStarAbbrev(starMatch.captured(1));
    QString trans = starMatch.captured(3);

    starId = procQuery->getStarId(destination, star, runway, true /* strict */);
    if(starId != -1 && !trans.isEmpty())
    {
      // Try tansition if STAR was found
      starTransId = procQuery->getStarTransitionId(destination, trans, starId, true /* strict */);
      if(starTransId == -1)
        // Invalidate STAR if transition is given and not found
        starId = -1;
    }
  }
}

void RouteStringReader::readStarAndTransSpace(const QString& star, QString trans, const QString& runway, int& starId, int& starTransId,
                                              const map::MapAirport& destination)
{
  starId = procQuery->getStarId(destination, sidStarAbbrev(star), runway, true /* strict */);

  if(starId != -1 && !trans.isEmpty())
    starTransId = procQuery->getSidTransitionId(destination, trans, starId, true /* strict */);
}

void RouteStringReader::readStarAndTrans(QStringList& items, QString& startTransWp, const QString& runway, int& starId, int& starTransId,
                                         int& consume, const map::MapAirport& destination, int index)
{
  starId = -1;
  starTransId = -1;
  consume = 0;

  QString item1 = items.value(index - 1);
  QString item2 = items.value(index - 2);

  if(!item1.contains('.'))
  {
    // Read "TRANS STAR" notation ======================================================
    readStarAndTransSpace(item1, item2, runway, starId, starTransId, destination);

    if(starId == -1)
      // Read "STAR TRANS" notation ======================================================
      readStarAndTransSpace(item2, item1, runway, starId, starTransId, destination);
    else
      // Remember item which was consumed as a STAR transition in space separated notation "TRANS STAR"
      startTransWp = item2;
  }

  if(starId == -1)
  {
    // Read "SID.TRANS" notation ======================================================
    QString starTrans = items.value(index - 1);
    item1 = starTrans.section('.', 0, 0);
    item2 = starTrans.section('.', 1, 1);
    readStarAndTransDot(item1 % '.' % item2, runway, starId, starTransId, destination);

    if(starId == -1)
      // Read "TRANS.SID" notation ======================================================
      readStarAndTransDot(item2 % '.' % item1, runway, starId, starTransId, destination);

    if(starId != -1)
      consume = 1;
  }
  else
  {
    // Return items to delete depending on found procedures
    if(starTransId != -1)
      consume = 2;
    else if(starId != -1)
      consume = 1;
  }
}

void RouteStringReader::destinationInternal(map::MapAirport& destinationAirport, proc::MapProcedureLegs& starLegs,
                                            proc::MapProcedureLegs& approachLegs, QStringList& items,
                                            QString& starTransWp, map::MapRunwayEnd& runwayEnd, int& consume, int index)
{
  QString airportIdent, approach, transition, runway;
  extractAirportIdentDestination(items.at(index), airportIdent, approach, transition, runway);

  map::MapAirport departureAirport;
  airportSim(destinationAirport, airportIdent);

  // Check if list is large enough to hold a destination
  if(destinationAirport.isValid() && items.size() > 1 && index > 0)
  {
    // Resolve approach information =================================
    if(!approach.isEmpty())
    {
      int approachId = -1, transitionId = -1;
      readApproachAndTrans(approach, transition, approachId, transitionId, destinationAirport);

      if(transitionId != -1)
      {
        // Approach and transition - fetch both
        const proc::MapProcedureLegs *legs = procQuery->getTransitionLegs(destinationAirport, transitionId);
        if(legs != nullptr)
          approachLegs = *legs;
      }
      else if(approachId != -1)
      {
        // Only approach
        const proc::MapProcedureLegs *legs = procQuery->getProcedureLegs(destinationAirport, approachId);
        if(legs != nullptr)
          approachLegs = *legs;
      }

      if(!approach.isEmpty() && approachLegs.isProcedureEmpty())
        appendWarning(tr("Approach %1 not found for destination airport %2. Ignoring approach.").
                      arg(approach).arg(destinationAirport.ident));
      else if(!transition.isEmpty() && approachLegs.isTransitionEmpty())
        appendWarning(tr("Transition %1 not found for destination airport %2. Ignoring transition.").
                      arg(transition).arg(destinationAirport.ident));
    }
    else if(!runway.isEmpty())
    {
      // Resolve runway only information =================================
      runwayEnd = airportQuerySim->getRunwayEndByName(destinationAirport.id, runway);
      // mapQuery->getRunwayEndByNameFuzzy(runwayEnds, runway, airport, true /* navdata */);

      if(runwayEnd.isValid())
        // Create a user defined approach using 1000 ft above threshold and 3 NM final
        procQuery->createCustomApproach(approachLegs, destinationAirport, runway, 3.f, 1000.f, 0.f);
      else
        appendWarning(tr("Runway %1 not found for destination airport %2. Ignoring runway.").
                      arg(runway).arg(destinationAirport.ident));
    }

    if(!approachLegs.isEmpty())
      runway = approachLegs.runway;

    // Try to read STAR and transition in several possible combination with and without dot
    int starId, starTransId;
    readStarAndTrans(items, starTransWp, runway, starId, starTransId, consume, destinationAirport, index);

    if(starId != -1 && starTransId == -1)
    {
      // Only STAR
      const proc::MapProcedureLegs *legs = procQuery->getProcedureLegs(destinationAirport, starId);
      if(legs != nullptr)
        starLegs = *legs;
    }
    else if(starId != -1 && starTransId != -1)
    {
      // STAR and transition
      const proc::MapProcedureLegs *legs = procQuery->getTransitionLegs(destinationAirport, starTransId);
      if(legs != nullptr)
        starLegs = *legs;
    }
  }
}

atools::geo::Pos RouteStringReader::findFirstCoordinate(const QStringList& items)
{
  Pos lastPos;
  // Get coordinate of the first and last navaid ============================================
  MapResult result;
  findWaypoints(result, items.constFirst(), false);

  if(result.isEmpty(ROUTE_TYPES_NAVAIDS))
    // No navaid found ===============================================
    appendError(tr("First item %1 not found as waypoint, VOR or NDB.").arg(items.isEmpty() ? QString() : items.constFirst()));
  else
  {
    if(result.size(ROUTE_TYPES_NAVAIDS) == 1)
      // Found exactly one navaid ===============================================
      lastPos = result.getPosition(ROUTE_TYPES_NAVAIDS_LIST);
    else
    {
      // More than one waypoint check waypoint airway combination ===============================================
      if(result.size(map::WAYPOINT) > 1)
      {
        if(items.size() > 1)
        {
          const QString& secondItem = items.at(1);
          for(const map::MapWaypoint& w : qAsConst(result.waypoints))
          {
            QList<map::MapWaypoint> waypoints;
            airwayQuery->getWaypointsForAirway(waypoints, secondItem, w.ident);
            if(!waypoints.isEmpty())
              lastPos = w.getPosition();
          }
        }
      }

      if(!lastPos.isValid())
      {
        // Fetch all navaids until we find a unique one and use this as start position
        for(const QString& item : items)
        {
          result.clear();
          findWaypoints(result, item, false);

          if(result.size(ROUTE_TYPES_NAVAIDS) == 1)
          {
            lastPos = result.getPosition(ROUTE_TYPES_NAVAIDS_LIST);
            break;
          }
        }
      }
    }
  }
  return lastPos;
}

void RouteStringReader::findIndexesInAirway(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints,
                                            int lastId, int nextId, int& startIndex, int& endIndex,
                                            const QString& airway)
{
  // TODO handle fragments properly

  // Default value is always 1
  int startFragment = 1, endFragment = 1;

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
void RouteStringReader::extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int startIndex, int endIndex,
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

void RouteStringReader::filterWaypoints(MapResult& result, atools::geo::Pos& lastPos, const atools::geo::Pos& destPos,
                                        const MapResult *lastResult, const QString& item, float maxDistance)
{
  // Remove artificial waypoints which were created for procedures only
  result.waypoints.erase(std::remove_if(result.waypoints.begin(), result.waypoints.end(),
                                        [](const map::MapWaypoint& wp) -> bool {
    return wp.artificial == map::WAYPOINT_ARTIFICIAL_PROCEDURES;
  }), result.waypoints.end());

  if(lastPos.isValid())
  {
    maptools::sortByDistance(result.airports, lastPos);
    // maptools::removeByDistance(result.airports, lastPos, maxDistance);

    maptools::sortByDistance(result.waypoints, lastPos);
    maptools::removeByDistance(result.waypoints, lastPos, maxDistance);

    maptools::sortByDistance(result.vors, lastPos);
    maptools::removeByDistance(result.vors, lastPos, maxDistance);

    maptools::sortByDistance(result.ndbs, lastPos);
    maptools::removeByDistance(result.ndbs, lastPos, maxDistance);

    maptools::sortByDistance(result.userpointsRoute, lastPos);
    maptools::removeByDistance(result.userpointsRoute, lastPos, maxDistance);
  }

  // Avoid airports by IATA or FAA codes if VOR and NDB are also present
  if(item.length() == 3 && (result.hasVor() || result.hasNdb()))
    result.airports.clear();

  if(!result.userpointsRoute.isEmpty())
    // User points have preference since they can be clearly identified
    lastPos = result.userpointsRoute.constFirst().position;
  else
  {
    // First check if there is a match to an airway
    bool updateNearestPos = true;
    if(lastResult != nullptr && result.hasAirways() && lastResult->hasWaypoints())
    {
      // Extract unique airways names
      QSet<QString> airwayNames;
      for(const map::MapAirway& a : qAsConst(result.airways))
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
          airwayQuery->getWaypointsForAirway(waypoints, airwayName, waypointIdent);

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
    if(updateNearestPos && lastPos.isValid())
    {
      ageo::Pos pos;
      map::MapTypes type;

      // Find something whatever is nearest and use that for the last position
      QVector<TypeDist> dists;
      if(result.hasAirports())
        dists.append(TypeDist(result.airports.constFirst().position, lastPos, destPos, map::AIRPORT));

      if(result.hasWaypoints())
        dists.append(TypeDist(result.waypoints.constFirst().position, lastPos, destPos, map::WAYPOINT,
                              result.waypoints.constFirst().artificial));

      if(result.hasVor())
        dists.append(TypeDist(result.vors.constFirst().position, lastPos, destPos, map::VOR));

      if(result.hasNdb())
        dists.append(TypeDist(result.ndbs.constFirst().position, lastPos, destPos, map::NDB));

      // Sort by distance from last plus distance to destination similar to A*
      if(dists.size() > 1)
        std::sort(dists.begin(), dists.end(), [](const TypeDist& p1, const TypeDist& p2) -> bool {
          return p1.distMeter < p2.distMeter;
        });

      if(!dists.isEmpty())
      {
        // Do selection between two first closest candidates
        if(dists.size() >= 2)
        {
          const TypeDist& first = dists.constFirst();
          const TypeDist& second = dists.at(1);

          // Check for special case where a NDB and a VOR or waypoint with the same name are nearby.
          // Prefer VOR or waypoint in this case.
          if(first.pos.distanceMeterTo(second.pos) < 2000.f)
          {
            if(first.type == map::NDB && (second.type == map::VOR || second.type == map::WAYPOINT))
            {
              dists.removeFirst();
              if(result.hasNdb())
                result.ndbs.removeFirst();
            }
          }
        }

        // Use position of nearest to last
        lastPos = dists.constFirst().pos;
      }
    }
  }
}

void RouteStringReader::filterAirways(QList<ParseEntry>& resultList, int i)
{
  if(i == 0 || i > resultList.size() - 1)
    return;

  MapResult& result = resultList[i].result;

  if(!result.airways.isEmpty())
  {
    QList<map::MapWaypoint> waypoints;
    MapResult& lastResult = resultList[i - 1].result;
    MapResult& nextResult = resultList[i + 1].result;
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
    airwayQuery->getWaypointsForAirway(waypoints, airwayName, waypointNameStart);

    if(!waypoints.isEmpty())
    {
      QList<map::MapAirwayWaypoint> allAirwayWaypoints;

      // Get all waypoints for the airway sorted by fragment and sequence
      airwayQuery->getWaypointListForAirwayName(allAirwayWaypoints, airwayName);

#ifdef DEBUG_INFORMATION
      for(const map::MapAirwayWaypoint& w : qAsConst(allAirwayWaypoints))
        qDebug() << w.waypoint.id << w.waypoint.ident << w.waypoint.region;
#endif

      if(!allAirwayWaypoints.isEmpty())
      {
        int startIndex = -1, endIndex = -1;

        // Iterate through all found waypoint combinations (same ident) and try to match them to an airway
        for(const map::MapWaypoint& wpLast : qAsConst(lastResult.waypoints))
        {
          for(const map::MapWaypoint& wpNext : qAsConst(nextResult.waypoints))
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
          //
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
                          arg(lastResult.waypoints.isEmpty() ? QString() : lastResult.waypoints.constFirst().ident).
                          arg(airwayName));

          if(endIndex == -1)
            appendWarning(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(nextResult.waypoints.isEmpty() ? QString() : nextResult.waypoints.constFirst().ident).
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

void RouteStringReader::findWaypoints(MapResult& result, const QString& item, bool matchWaypoints)
{
  bool searchCoords = false;
  if(item.length() > 5)
    // User coordinates for sure
    searchCoords = true;

  mapQuery->getMapObjectByIdent(result, ROUTE_TYPES_AND_AIRWAY, item, QString(), QString(), false /* airportFromNavdatabase */,
                                map::AP_QUERY_ALL);

  if(item.length() == 5 && result.waypoints.isEmpty())
    // Nothing found - try NAT waypoint (a few of these are also in the database)
    searchCoords = true;

  if(searchCoords)
  {
    // Try user defined waypoint coordinates =================
    Pos pos = coords::fromAnyWaypointFormat(item);
    if(pos.isValid())
    {
      if(matchWaypoints)
      {
        // Try to find a waypoint at the position =================
        map::MapWaypoint waypoint;
        waypointQuery->getWaypointRectNearest(waypoint, pos, 0.01f);

        if(waypoint.isValid() && waypoint.getPosition().distanceMeterTo(pos) < 50)
          // Use real waypoint at same position closer than 50 meter =================
          result.waypoints.append(waypoint);
        else
        {
          // No waypoint at position - create user waypoint =================
          map::MapUserpointRoute user;
          user.position = pos;
          result.userpointsRoute.append(user);
        }
      }
      else
      {
        // No matching wanted - create user waypoint =================
        map::MapUserpointRoute user;
        user.position = pos;
        result.userpointsRoute.append(user);
      }
    }
  }
}

QStringList RouteStringReader::cleanItemList(const QStringList& items, float& speedKnots, float& altFeet)
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
      float spd = 0.f, alt = 0.f;
      if(!atools::fs::util::extractSpeedAndAltitude(item, spd, alt))
        appendWarning(tr("Ignoring invalid speed and altitude instruction %1.").arg(item));
      else
      {
        speedKnots = spd;
        altFeet = alt;
      }
      continue;
    }

    QRegularExpressionMatch match = SPDALT_WAYPOINT.match(item);
    if(match.hasMatch())
    {
      item = match.captured(1);
      appendWarning(tr("Ignoring speed and altitude at waypoint %1.").arg(item));
    }

    if(i < items.size() - 1)
    {
      QString itemPair = item % '/' % items.at(i + 1);
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

map::MapAirway RouteStringReader::extractAirway(const QList<map::MapAirway>& airways, int waypointId1, int waypointId2,
                                                const QString& airwayName)
{
  for(const map::MapAirway& airway : airways)
  {
    if(airway.name == airwayName &&
       ((waypointId1 == airway.fromWaypointId && waypointId2 == airway.toWaypointId) ||
        (waypointId1 == airway.toWaypointId && waypointId2 == airway.fromWaypointId)))
      return airway;
  }
  return map::MapAirway();
}

void RouteStringReader::removeEmptyResults(QList<ParseEntry>& resultList)
{
  auto it = std::remove_if(resultList.begin(), resultList.end(),
                           [](const ParseEntry& type) -> bool
  {
    return type.result.isEmpty(ROUTE_TYPES_AND_AIRWAY);
  });

  if(it != resultList.end())
    resultList.erase(it, resultList.end());
}
