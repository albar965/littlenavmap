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

#include "routestring/routestringreader.h"

#include "common/mapresult.h"
#include "common/maptools.h"
#include "common/proctypes.h"
#include "common/unit.h"
#include "fs/pln/flightplan.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "navapp.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "query/waypointtrackquery.h"
#include "route/flightplanentrybuilder.h"
#include "util/htmlbuilder.h"

#include <QElapsedTimer>
#include <QRegularExpression>

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
// Runway specification directly after airport - ignored. Example: USSS/08R ... ZMUB1200/33
const static QRegularExpression AIRPORT_TIME_RUNWAY("^([A-Z0-9]{3,4})(\\d{4})?(/[LCR0-9]{2,3})?$");

const static QRegularExpression SID_STAR_TRANS("^([A-Z0-9]{1,7})(\\.([A-Z0-9]{1,6}))?$");

const static map::MapTypes ROUTE_TYPES_AND_AIRWAY(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE | map::AIRWAY);

const static map::MapTypes ROUTE_TYPES(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);

// No airports
const static map::MapTypes ROUTE_TYPES_NAVAIDS(map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);
const static std::initializer_list<map::MapTypes> ROUTE_TYPES_NAVAIDS_LIST =
{map::WAYPOINT, map::VOR, map::NDB, map::USERPOINTROUTE};

/* Internal parsing structure which holds all found potential candidates from a search */
struct RouteStringReader::ParseEntry
{
  QString item, airway;
  map::MapResult result;
};

RouteStringReader::RouteStringReader(FlightplanEntryBuilder *flightplanEntryBuilder)
  : entryBuilder(flightplanEntryBuilder)
{
  mapQuery = NavApp::getMapQueryGui();
  airportQuerySim = NavApp::getAirportQuerySim();
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
                                              atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs,
                                              float *speedKts, bool *altIncluded)
{
  qDebug() << Q_FUNC_INFO;

  QElapsedTimer timer;
  timer.start();

  airwayQuery->setUseTracks(!(options & rs::NO_TRACKS));
  waypointQuery->setUseTracks(false);

  messages.clear();
  hasWarnings = hasErrors = false;
  QStringList items = rs::cleanRouteString(routeString);

  // Create a pointer to temporary plan or passed pointer to plan
  // Avoids null pointer and prepare for reuse of flight plan attributes
  Flightplan *fp, tempFp;
  fp = flightplan != nullptr ? flightplan : &tempFp;

#ifdef DEBUG_INFORMATION
  map::MapObjectRefExtVector refTemp;
  mapObjectRefs = mapObjectRefs != nullptr ? mapObjectRefs : &refTemp;

  qDebug() << "items" << items;
#endif

  // Remove all unneeded adornment like speed and times and also combine waypoint coordinate pairs
  // Also extracts speed, altitude, SID and STAR
  float altitude;
  QStringList cleanItems = cleanItemList(items, speedKts, &altitude);

  if(altitude > 0.f)
  {
    if(altIncluded != nullptr)
      *altIncluded = true;
    fp->setCruisingAltitude(atools::roundToInt(Unit::altFeetF(altitude)));
  }
  else
  {
    if(altIncluded != nullptr)
      *altIncluded = false;
  }

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

  if(readNoAirports)
    // Try to get first position by various queries if no airports are given
    lastPos = findFirstCoordinate(cleanItems);
  else
  {
    // Get airports as well as SID and STAR ============================================
    // Insert first as departure
    if(!addDeparture(fp, mapObjectRefs, cleanItems))
      return false;

    // Append last as destination
    if(!addDestination(fp, mapObjectRefs, cleanItems, options))
      return false;

    lastPos = fp->getDeparturePosition();
  }

  if(altitude > 0.f && options.testFlag(rs::REPORT))
    appendMessage(tr("Using cruise altitude <b>%1</b> for flight plan.").arg(Unit::altFeet(altitude)));

  if(speedKts != nullptr && *speedKts > 0.f)
  {
    float cruise = NavApp::getRouteCruiseSpeedKts();
    if(atools::almostEqual(*speedKts, cruise, 1.f))
      appendWarning(tr("Ignoring speed instruction %1.").arg(Unit::speedKts(*speedKts)));
    else
      appendWarning(tr("Ignoring speed instruction %1 in favor of aircraft performance (%2 true airspeed).").
                    arg(Unit::speedKts(*speedKts)).
                    arg(Unit::speedKts(NavApp::getRouteCruiseSpeedKts())));
  }

  // Do not get any navaids that are too far away
  float maxDistance = ageo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM, fp->getDistanceNm() * 2.0f));

  if(!lastPos.isValid())
  {
    appendError(tr("Could not determine starting position for first item %1.").arg(cleanItems.constFirst()));
    return false;
  }

  // Collect all navaids, airports and coordinates into MapSearchResults =============================

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after start" << timer.restart();
#endif

  QList<ParseEntry> resultList;
  for(const QString& item : cleanItems)
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

  // Create airways - will fill the waypoint list in result with airway points
  // if airway is invalid it will be erased in result
  // Will erase NDB, VOR and adapt waypoint list in result if an airway/waypoint match was found
  // List of airways is unchanged
  for(int i = readNoAirports ? 0 : 1; i < resultList.size() - 1; i++)
    filterAirways(resultList, i);

#ifdef DEBUG_INFORMATION

  for(const ParseEntry& parse : resultList)
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

  atools::fs::pln::FlightplanEntryListType& entries = fp->getEntries();
  map::MapObjectRefExt lastRef;
  for(int i = 0; i < resultList.size(); i++)
  {
    const QString& item = resultList.at(i).item;
    const MapResult& result = resultList.at(i).result;
    const ParseEntry *lastParseEntry = i > 0 ? &resultList.at(i - 1) : nullptr;
    map::MapObjectRefExt curRef;

    // Add waypoints on airways =======================================
    if(result.hasAirways())
    {
      if(i == 0)
        appendWarning(tr("Found airway %1 instead of waypoint as first entry in enroute list. Ignoring.").arg(item));
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
            entries.insert(entries.size() - insertOffset, entry);

            // Build reference list if required
            if(mapObjectRefs != nullptr)
            {
              int insertPos = mapObjectRefs->size() - insertOffset;

              // Airway reference precedes waypoint
              QList<map::MapAirway> airways;
              airwayQuery->getAirwaysForWaypoints(airways, lastRef.id, wp.id, item);
              if(!airways.isEmpty())
                atools::insertInto(*mapObjectRefs, insertPos,
                                   map::MapObjectRefExt(airways.constFirst().id, map::AIRWAY, airways.constFirst().name));

              insertPos = mapObjectRefs->size() - insertOffset;
              // Waypoint
              curRef = map::MapObjectRefExt(wp.id, wp.position, map::WAYPOINT, wp.ident);
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
    // Add a single waypoint from direct =======================================
    {
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
          curRef = map::MapObjectRefExt(-1, entry.getPosition(), map::USERPOINTROUTE, item);
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
                                 map::MapObjectRefExt(airways.constFirst().id, map::AIRWAY, airways.constFirst().name));
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
        entries.insert(entries.size() - insertOffset, entry);
      }
      else
        appendWarning(tr("No navaid found for %1. Ignoring.").arg(item));

      lastRef = curRef;
    }

    // Remember position for distance calculation
    lastPos = entries.at(entries.size() - 1 - insertOffset).getPosition();
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "after build flight plan" << timer.restart();
#endif

  // Update departure and destination if no airports are used ==============================
  if(readNoAirports && !entries.isEmpty())
  {
    fp->setDepartureName(entries.constFirst().getIdent());
    fp->setDepartureIdent(entries.constFirst().getIdent());
    fp->setDeparturePosition(entries.constFirst().getPosition());
    fp->setDepartureParkingPosition(entries.constFirst().getPosition(),
                                    atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);

    fp->setDestinationName(entries.constLast().getIdent());
    fp->setDestinationIdent(entries.constLast().getIdent());
    fp->setDestinationPosition(entries.constLast().getPosition());
  }

  if(options.testFlag(rs::REPORT))
    addReport(fp);

#ifdef DEBUG_INFORMATION
  qDebug() << "===============================";
  qDebug() << *fp;

  if(mapObjectRefs != nullptr)
  {
    qDebug() << "===============================";
    for(const map::MapObjectRefExt& r : *mapObjectRefs)
      qDebug() << r;
  }
#endif

  return true;
}

void RouteStringReader::addReport(atools::fs::pln::Flightplan *flightplan)
{
  int insertIndex = 0;
  QString from, to;
  AirportQuery *airportQuery = NavApp::getAirportQuerySim();

  if(!flightplan->getDepartureName().isEmpty() &&
     flightplan->getDepartureName() != flightplan->getDepartureIdent())
    // Departure is airport
    from = tr("%1 (%2)").
           arg(flightplan->getDepartureName()).
           arg(airportQuery->getDisplayIdent(flightplan->getDepartureIdent()));
  else
    // Departure is waypoint
    from = flightplan->getDepartureIdent();

  if(!flightplan->getDestinationName().isEmpty() &&
     flightplan->getDestinationName() != flightplan->getDestinationIdent())
    // Departure is airport
    to = tr("%1 (%2)").
         arg(flightplan->getDestinationName()).
         arg(airportQuery->getDisplayIdent(flightplan->getDestinationIdent()));
  else
    // Departure is waypoint
    to = flightplan->getDestinationIdent();

  insertMessage(tr("Flight plan from <b>%1</b> to <b>%2</b>.").arg(from).arg(to), insertIndex++);

  insertMessage(tr("Distance without procedures: <b>%1</b>.").arg(Unit::distNm(flightplan->getDistanceNm())), insertIndex++);

  QStringList idents;
  for(atools::fs::pln::FlightplanEntry& entry : flightplan->getEntries())
  {
    if(entry.getWaypointType() == atools::fs::pln::entry::AIRPORT)
      idents.append(airportQuery->getDisplayIdent(entry.getIdent()));
    else
      idents.append(entry.getIdent());
  }

  if(!idents.isEmpty())
    insertMessage(tr("Found %1 %2: <b>%3</b>.").
                  arg(idents.size()).
                  arg(idents.size() == 1 ? tr("waypoint") : tr("waypoints")).
                  arg(atools::elideTextShortMiddle(idents.join(tr(" ")), 150)), insertIndex++);

  QString sid = ProcedureQuery::getSidAndTransition(flightplan->getProperties());
  if(!sid.isEmpty())
    insertMessage(tr("Found SID: <b>%1</b>.").arg(sid), insertIndex++);

  QString star = ProcedureQuery::getStarAndTransition(flightplan->getProperties());
  if(!star.isEmpty())
    insertMessage(tr("Found STAR: <b>%1</b>.").arg(star), insertIndex++);
}

map::MapObjectRefExt RouteStringReader::mapObjectRefFromEntry(const FlightplanEntry& entry,
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
    return map::MapObjectRefExt(-1, result.userpointsRoute.constFirst().position, map::USERPOINTROUTE, name);
  else
    return map::MapObjectRefExt();
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
  messages.insert(index, message);
  qInfo() << Q_FUNC_INFO << "Message:" << message;
}

void RouteStringReader::appendMessage(const QString& message)
{
  messages.append(message);
  qInfo() << Q_FUNC_INFO << "Message:" << message;
}

void RouteStringReader::appendWarning(const QString& message)
{
  hasWarnings = true;
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(atools::util::HtmlBuilder::warningMessage(message));
  qWarning() << Q_FUNC_INFO << "Warning:" << message;
}

void RouteStringReader::appendError(const QString& message)
{
  hasErrors = true;
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(atools::util::HtmlBuilder::errorMessage(message));
  qWarning() << Q_FUNC_INFO << "Error:" << message;
}

QString RouteStringReader::extractAirportIdent(QString ident)
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

QString RouteStringReader::sidStarAbbrev(QString sid)
{
  if(sid.size() == 7)
    // Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A
    sid.remove(4, 1);
  return sid;
}

void RouteStringReader::readSidAndTrans(QStringList& items, int& sidId, int& sidTransId, const map::MapAirport& departure)
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

      sidId = procQuery->getSidId(departure, sid, QString(), true /* strict */);

      if(sidId != -1)
        // Consume SID
        items.removeFirst();

      if(sidId != -1 && !trans.isEmpty())
      {
        sidTransId = procQuery->getSidTransitionId(departure, trans, sidId, true /* strict */);

        if(sidTransId != -1)
          // Consume transition
          items.removeFirst();
      }
    }

    if(sidId == -1)
    {
      // Read "SID.TRANS" notation ======================================================
      QString sidTrans = items.constFirst();
      QRegularExpressionMatch sidMatch = SID_STAR_TRANS.match(sidTrans);

      if(sidMatch.hasMatch())
      {
        QString sid = sidStarAbbrev(sidMatch.captured(1));
        QString trans = sidMatch.captured(3);

        sidId = procQuery->getSidId(departure, sid, QString(), true /* strict */);
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
  }
}

bool RouteStringReader::addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs, QStringList& items)
{
  QString ident = extractAirportIdent(items.takeFirst());

  map::MapAirport departure;
  airportQuerySim->getAirportByIdent(departure, ident);
  if(!departure.isValid())
  {
    // Try again with official codes but not ident
    QList<map::MapAirport> airports = airportQuerySim->getAirportsByOfficialIdent(ident, nullptr, map::INVALID_DISTANCE_VALUE,
                                                                                  map::AP_QUERY_OFFICIAL);
    if(!airports.isEmpty())
      departure = airports.constFirst();
  }

  if(departure.isValid())
  {
    flightplan->setDepartureName(departure.name);
    flightplan->setDepartureIdent(departure.ident);
    flightplan->setDeparturePosition(departure.position);
    flightplan->setDepartureParkingPosition(departure.position, atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry, false /* alternate */);
    flightplan->getEntries().append(entry);
    if(mapObjectRefs != nullptr)
      mapObjectRefs->append(map::MapObjectRefExt(departure.id, departure.position, map::AIRPORT, departure.ident));

    if(!items.isEmpty())
    {
      // Try to read SID and transition and consume items
      int sidId, sidTransId;
      readSidAndTrans(items, sidId, sidTransId, departure);

      // Fill procedure into route if found =============================
      if(sidId != -1)
      {
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
        ProcedureQuery::fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, starLegs, departureLegs);
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

bool RouteStringReader::addDestination(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs,
                                       QStringList& items, rs::RouteStringOptions options)
{
  if(options & rs::READ_ALTERNATES)
  {
    // Read airports at end of list as alternates =============================================

    // List of consecutive airports at end of string - destination is last entry, alternates before
    QVector<proc::MapProcedureLegs> stars;
    QVector<map::MapAirport> airports;

    // Number of entries that have to be consumed for SID and/or transition - 0, 1, or 2
    QVector<int> consumeList;

    // Iterate from end of list and collect airports in reverse order
    int idx = items.size() - 1;
    while(idx >= 0)
    {
      proc::MapProcedureLegs star;
      map::MapAirport ap;

      // Get airport and STAR
      int consume = 0;
      destinationInternal(ap, star, items, consume, idx);

      if(!star.isEmpty())
      {
        // Found a matching STAR - this is the destination - stop here
        stars.append(star);
        airports.append(ap);
        consumeList.append(consume);
        break;
      }

      if(ap.isValid() &&
         (airports.isEmpty() || airports.constFirst().position.distanceMeterTo(ap.position) < ageo::nmToMeter(MAX_ALTERNATE_DISTANCE_NM)))
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
          {
            // Have overlapping navaid names - ignore airport identifier and stop here
            break;
          }
        }

        // Found a valid airport, add and continue with previous one
        stars.append(star);
        airports.append(ap);
        consumeList.append(consume);
        idx--;
      }
      else
        // No valid airport - stop here
        break;
    }

    if(!airports.isEmpty())
    {
      // Found airports
      // Consume all airports in item list
      for(int i = 0; i < airports.size() && !items.isEmpty(); i++)
        items.removeLast();

      // Get and consume destination airport at the end of the list ========================
      map::MapAirport dest = airports.takeLast();
      proc::MapProcedureLegs star = stars.takeLast();
      int consume = consumeList.takeLast();

      flightplan->setDestinationName(dest.name);
      flightplan->setDestinationIdent(dest.ident);
      flightplan->setDestinationPosition(dest.getPosition());

      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(dest, entry, false /* alternate */);
      flightplan->getEntries().append(entry);
      if(mapObjectRefs != nullptr)
        mapObjectRefs->append(map::MapObjectRefExt(dest.id, dest.position, map::AIRPORT, dest.ident));

      // Get destination STAR ========================
      if(!star.isEmpty())
      {
        // Consume STAR in list - one or two entries
        for(int i = 0; i < consume; i++)
          items.removeLast();

        // Add STAR information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        ProcedureQuery::fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, star, departureLegs);
      }

      // Collect remaining alternates ========================
      QStringList alternateIdents, alternateDisplayIdents;
      for(const map::MapAirport& alt : airports)
      {
        alternateIdents.prepend(alt.ident);
        alternateDisplayIdents.prepend(airportQuerySim->getDisplayIdent(alt.ident));
      }

      if(!alternateIdents.isEmpty())
      {
        flightplan->getProperties().insert(atools::fs::pln::ALTERNATES, alternateIdents.join("#"));

        if(options.testFlag(rs::REPORT))
          appendMessage(tr("Found alternate %1 <b>%2</b>.").
                        arg(alternateDisplayIdents.size() == 1 ? tr("airport") : tr("airports")).
                        arg(alternateDisplayIdents.join(tr(", "))));
      }
    } // if(!airports.isEmpty())
    else
    {
      appendError(tr("Required destination airport %1 not found.").arg(items.constLast()));
      return false;
    }
  } // if(options & rs::READ_ALTERNATES)
  else
  {
    // Read airports at end of list as waypoints =============================================
    proc::MapProcedureLegs star;
    map::MapAirport dest;

    // Get airport and STAR at end of list ================================
    int consume = 0;
    destinationInternal(dest, star, items, consume, items.size() - 1);

    if(dest.isValid())
    {
      flightplan->setDestinationName(dest.name);
      flightplan->setDestinationIdent(dest.ident);
      flightplan->setDestinationPosition(dest.getPosition());

      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(dest, entry, false /* alternate */);
      flightplan->getEntries().append(entry);
      if(mapObjectRefs != nullptr)
        mapObjectRefs->append(map::MapObjectRefExt(dest.id, dest.position, map::AIRPORT, dest.ident));

      // Consume airport
      items.removeLast();

      // Get destination STAR ========================
      if(!star.isEmpty())
      {
        // Consume STAR in list
        for(int i = 0; i < consume; i++)
          items.removeLast();

        // Add information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        ProcedureQuery::fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, star, departureLegs);
      }

      // Remaining airports are handled by other waypoint code
    }
    else
    {
      appendError(tr("Required destination airport %1 not found.").arg(items.constLast()));
      return false;
    }
  }
  return true;
}

void RouteStringReader::readStarAndTransDot(const QString& starTrans, int& starId, int& starTransId, const map::MapAirport& destination)
{
  starId = -1;
  starTransId = -1;

  QRegularExpressionMatch starMatch = SID_STAR_TRANS.match(starTrans);
  if(starMatch.hasMatch())
  {
    QString star = sidStarAbbrev(starMatch.captured(1));
    QString trans = starMatch.captured(3);

    starId = procQuery->getStarId(destination, star, QString(), true /* strict */);
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

void RouteStringReader::readStarAndTransSpace(const QString& star, QString trans, int& starId, int& starTransId,
                                              const map::MapAirport& destination)
{
  starId = procQuery->getStarId(destination, sidStarAbbrev(star), QString(), true /* strict */);

  if(starId != -1 && !trans.isEmpty())
    starTransId = procQuery->getSidTransitionId(destination, trans, starId, true /* strict */);
}

void RouteStringReader::readStarAndTrans(QStringList& items, int& starId, int& starTransId, int& consume,
                                         const map::MapAirport& destination, int index)
{
  starId = -1;
  starTransId = -1;
  consume = 0;

  QString item1 = items.value(index - 1);
  QString item2 = items.value(index - 2);

  if(!item1.contains('.'))
  {
    // Read "STAR TRANS" notation ======================================================
    readStarAndTransSpace(item1, item2, starId, starTransId, destination);

    if(starId == -1)
      // Read "TRANS STAR" notation ======================================================
      readStarAndTransSpace(item2, item1, starId, starTransId, destination);
  }

  if(starId == -1)
  {
    // Read "SID.TRANS" notation ======================================================
    QString starTrans = items.value(index - 1);
    item1 = starTrans.section('.', 0, 0);
    item2 = starTrans.section('.', 1, 1);
    readStarAndTransDot(item1 + "." + item2, starId, starTransId, destination);

    if(starId == -1)
      // Read "TRANS.SID" notation ======================================================
      readStarAndTransDot(item2 + "." + item1, starId, starTransId, destination);

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

void RouteStringReader::destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs, QStringList& items,
                                            int& consume, int index)
{
  QString ident = extractAirportIdent(items.at(index));
  airportQuerySim->getAirportByIdent(destination, ident);

  if(!destination.isValid())
  {
    // Try again official codes but not ident
    QList<map::MapAirport> airports = airportQuerySim->getAirportsByOfficialIdent(ident, nullptr, map::INVALID_DISTANCE_VALUE,
                                                                                  map::AP_QUERY_OFFICIAL);
    if(!airports.isEmpty())
      destination = airports.constFirst();
  }

  if(destination.isValid() && items.size() > 1 && index > 0)
  {
    // Try to read STAR and transition in several possible combination with and without dot
    int starId, starTransId;
    readStarAndTrans(items, starId, starTransId, consume, destination, index);

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

atools::geo::Pos RouteStringReader::findFirstCoordinate(const QStringList& items)
{
  Pos lastPos;
  // Get coordinate of the first and last navaid ============================================
  MapResult result;
  findWaypoints(result, items.constFirst(), false);

  if(result.isEmpty(ROUTE_TYPES_NAVAIDS))
    // No navaid found ===============================================
    appendError(tr("First item %1 not found as waypoint, VOR or NDB.").arg(items.constFirst()));
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
          for(const map::MapWaypoint& w : result.waypoints)
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
      struct Dist
      {
        Dist(const ageo::Pos& posParam, const ageo::Pos& lastPosParam, const ageo::Pos& destPosParam,
             map::MapTypes typeParam)
          : pos(posParam), type(typeParam)
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
      };

      ageo::Pos pos;
      map::MapTypes type;

      // Find something whatever is nearest and use that for the last position
      QVector<Dist> dists;
      if(result.hasAirports())
        dists.append(Dist(result.airports.constFirst().position, lastPos, destPos, map::AIRPORT));

      if(result.hasWaypoints())
        dists.append(Dist(result.waypoints.constFirst().position, lastPos, destPos, map::WAYPOINT));

      if(result.hasVor())
        dists.append(Dist(result.vors.constFirst().position, lastPos, destPos, map::VOR));

      if(result.hasNdb())
        dists.append(Dist(result.ndbs.constFirst().position, lastPos, destPos, map::NDB));

      // Sort by distance from last plus distance to destination similar to A*
      if(dists.size() > 1)
        std::sort(dists.begin(), dists.end(), [](const Dist& p1, const Dist& p2) -> bool {
          return p1.distMeter < p2.distMeter;
        });

      if(!dists.isEmpty())
      {
        if(dists.size() >= 2)
        {
          const Dist& first = dists.constFirst();
          const Dist& second = dists.at(1);

          // Check for special case where a NDB and a VOR or waypoint with the same name are nearby.
          // Prefer VOR or waypoint in this case.
          if(first.type == map::NDB && (second.type == map::VOR || second.type == map::WAYPOINT) &&
             first.pos.distanceMeterTo(second.pos) < ageo::nmToMeter(MAX_CLOSE_NAVAIDS_DISTANCE_NM))
          {
            dists.removeFirst();

            if(result.hasNdb())
              result.ndbs.removeFirst();
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
                          arg(lastResult.waypoints.constFirst().ident).
                          arg(airwayName));

          if(endIndex == -1)
            appendWarning(tr("Waypoint %1 not found in airway %2. Ignoring flight plan segment.").
                          arg(nextResult.waypoints.constFirst().ident).
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

QStringList RouteStringReader::cleanItemList(const QStringList& items, float *speedKnots, float *altFeet)
{
  if(speedKnots != nullptr)
    *speedKnots = 0.f;
  if(altFeet != nullptr)
    *altFeet = 0.f;

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
        if(speedKnots != nullptr)
          *speedKnots = spd;
        if(altFeet != nullptr)
          *altFeet = alt;
      }
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
