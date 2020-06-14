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

#include "navapp.h"
#include "fs/util/coordinates.h"
#include "fs/util/fsutil.h"
#include "query/procedurequery.h"
#include "fs/pln/flightplan.h"
#include "util/htmlbuilder.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/airportquery.h"
#include "route/flightplanentrybuilder.h"
#include "common/maptools.h"
#include "common/unit.h"
#include "query/waypointtrackquery.h"

#include <QElapsedTimer>
#include <QRegularExpression>

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using map::MapSearchResult;
using atools::geo::Pos;
namespace coords = atools::fs::util;
namespace plnentry = atools::fs::pln::entry;

// Maximum distance to previous waypoint - everything above will be sorted out
const static float MAX_WAYPOINT_DISTANCE_NM = 5000.f;

// Choose best navaid from a cluster with this distance.
// Used to prioritize VOR and waypoints before NDB with the same name
const static float MAX_CLOSE_NAVAIDS_DISTANCE_NM = 5.f;

const static QRegularExpression SPDALT_WAYPOINT("^([A-Z0-9]+)/[NMK]\\d{3,4}[FSAM]\\d{3,4}$");

// Time specification directly after airport - ignored
// Runway specification directly after airport - ignored. Example: USSS/08R ... ZMUB1200/33
const static QRegularExpression AIRPORT_TIME_RUNWAY("^([A-Z0-9]{3,4})(\\d{4})?(/[LCR0-9]{2,3})?$");

const static QRegularExpression SID_STAR_TRANS("^([A-Z0-9]{1,7})(\\.([A-Z0-9]{1,6}))?$");

const static map::MapObjectTypes ROUTE_TYPES_AND_AIRWAY(map::AIRPORT | map::WAYPOINT |
                                                        map::VOR | map::NDB | map::USERPOINTROUTE |
                                                        map::AIRWAY);

const static map::MapObjectTypes ROUTE_TYPES(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);

// No airports
const static map::MapObjectTypes ROUTE_TYPES_NAVAIDS(map::WAYPOINT | map::VOR | map::NDB | map::USERPOINTROUTE);
const static std::initializer_list<map::MapObjectTypes> ROUTE_TYPES_NAVAIDS_LIST =
{map::WAYPOINT, map::VOR, map::NDB, map::USERPOINTROUTE};

/* Internal parsing structure which holds all found potential candidates from a search */
struct RouteStringReader::ParseEntry
{
  QString item, airway;
  map::MapSearchResult result;
};

RouteStringReader::RouteStringReader(FlightplanEntryBuilder *flightplanEntryBuilder, bool verboseParam)
  : verbose(verboseParam), entryBuilder(flightplanEntryBuilder)
{
  mapQuery = NavApp::getMapQuery();
  airportQuerySim = NavApp::getAirportQuerySim();
  procQuery = NavApp::getProcedureQuery();

  // Create a copy of the delegate which uses the same AirwayQuery objects
  // This allows to disable track reading in the copy (setUseTracks())
  airwayQuery = new AirwayTrackQuery(*NavApp::getAirwayTrackQuery());
  waypointQuery = new WaypointTrackQuery(*NavApp::getWaypointTrackQuery());
}

RouteStringReader::~RouteStringReader()
{
  delete airwayQuery;
  delete waypointQuery;
}

bool RouteStringReader::createRouteFromString(const QString& routeString, rs::RouteStringOptions options,
                                              atools::fs::pln::Flightplan *flightplan,
                                              map::MapObjectRefExtVector *mapObjectRefs,
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

  // Remove all unneded adornment like speed and times and also combine waypoint coordinate pairs
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

  if(altitude > 0.f)
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
  float maxDistance = atools::geo::nmToMeter(std::max(MAX_WAYPOINT_DISTANCE_NM, fp->getDistanceNm() * 2.0f));

  if(!lastPos.isValid())
  {
    appendError(tr("Could not determine starting position for first item %1.").arg(cleanItems.first()));
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
    MapSearchResult result;
    findWaypoints(result, item, options & rs::READ_MATCH_WAYPOINTS);

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
    const MapSearchResult& result = resultList.at(i).result;
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
            entry.setFlag(atools::fs::pln::entry::TRACK, result.airways.first().isTrack());
            entries.insert(entries.size() - insertOffset, entry);

            // Build reference list if required
            if(mapObjectRefs != nullptr)
            {
              int insertPos = mapObjectRefs->size() - insertOffset;

              // Airway reference precedes waypoint
              QList<map::MapAirway> airways;
              airwayQuery->getAirwaysForWaypoints(airways, lastRef.id, wp.id, item);
              if(!airways.isEmpty())
                mapObjectRefs->insert(insertPos,
                                      map::MapObjectRefExt(airways.first().id, map::AIRWAY, airways.first().name));

              insertPos = mapObjectRefs->size() - insertOffset;
              // Waypoint
              curRef = map::MapObjectRefExt(wp.id, wp.position, map::WAYPOINT, wp.ident);
              mapObjectRefs->insert(insertPos, curRef);
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
        entryBuilder->buildFlightplanEntry(result.userpointsRoute.first().position, result, entry, true, map::NONE);

        // Build reference list if required
        if(mapObjectRefs != nullptr)
        {
          // Add user defined waypoint with original coordinate string
          curRef = map::MapObjectRefExt(-1, entry.getPosition(), map::USERPOINTROUTE, item);
          mapObjectRefs->insert(mapObjectRefs->size() - insertOffset, curRef);
        }

        // Use the original string as name but limit it for fs
        entry.setIdent(item);
      }
      else
      {
        // Navaid  ==========================================
        // Get nearest navaid and build entry from it
        buildEntryForResult(entry, result, lastPos);

        // Build reference list if required
        if(mapObjectRefs != nullptr)
        {
          curRef = mapObjectRefFromEntry(entry, result, item);

          if(lastParseEntry != nullptr && lastParseEntry->result.hasAirways())
          {
            // Insert airway entry leading towards following waypoint
            QList<map::MapAirway> airways;
            airwayQuery->getAirwaysForWaypoints(airways, lastRef.id, curRef.id, lastParseEntry->airway);
            if(!airways.isEmpty())
              mapObjectRefs->insert(mapObjectRefs->size() - insertOffset,
                                    map::MapObjectRefExt(airways.first().id, map::AIRWAY, airways.first().name));
          }

          // Add navaid or airport including original name
          mapObjectRefs->insert(mapObjectRefs->size() - insertOffset, curRef);
        }
      }

      if(entry.getPosition().isValid())
      {
        const ParseEntry& parseEntry = resultList.at(i);
        // Assign airway if  this is the end point of an airway
        entry.setAirway(parseEntry.airway);

        if(parseEntry.result.hasAirways())
          entry.setFlag(atools::fs::pln::entry::TRACK, parseEntry.result.airways.first().isTrack());
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
    fp->setDepartureName(entries.first().getIdent());
    fp->setDepartureIdent(entries.first().getIdent());
    fp->setDeparturePosition(entries.first().getPosition());

    fp->setDestinationName(entries.last().getIdent());
    fp->setDestinationIdent(entries.last().getIdent());
    fp->setDestinationPosition(entries.last().getPosition());
  }

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

map::MapObjectRefExt RouteStringReader::mapObjectRefFromEntry(const FlightplanEntry& entry,
                                                              const map::MapSearchResult& result, const QString& name)
{
  atools::fs::pln::entry::WaypointType type = entry.getWaypointType();

  // Look at the entry types to use the same preference as the FlightplanEntryBuilder
  if(type == atools::fs::pln::entry::AIRPORT && result.hasAirports())
    return result.airports.first().getRefExt(name);
  else if(type == atools::fs::pln::entry::WAYPOINT && result.hasWaypoints())
    return result.waypoints.first().getRefExt(name);
  else if(type == atools::fs::pln::entry::VOR && result.hasVor())
    return result.vors.first().getRefExt(name);
  else if(type == atools::fs::pln::entry::NDB && result.hasNdb())
    return result.ndbs.first().getRefExt(name);
  else if(type == atools::fs::pln::entry::USER && result.hasUserpointsRoute())
    return map::MapObjectRefExt(-1, result.userpointsRoute.first().position, map::USERPOINTROUTE, name);
  else
    return map::MapObjectRefExt();
}

void RouteStringReader::buildEntryForResult(FlightplanEntry& entry, const MapSearchResult& result,
                                            const atools::geo::Pos& nearestPos)
{
  MapSearchResult newResult;
  resultWithClosest(newResult, result, nearestPos, map::WAYPOINT | map::VOR | map::NDB | map::AIRPORT);
  entryBuilder->buildFlightplanEntry(newResult, entry, true);
}

void RouteStringReader::resultWithClosest(map::MapSearchResult& resultWithClosest, const map::MapSearchResult& result,
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

void RouteStringReader::appendMessage(const QString& message)
{
  messages.append(message);
  qInfo() << "Message:" << message;
}

void RouteStringReader::appendWarning(const QString& message)
{
  hasWarnings = true;
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(atools::util::HtmlBuilder::warningMessage(message));
  qWarning() << "Warning:" << message;
}

void RouteStringReader::appendError(const QString& message)
{
  hasErrors = true;
  if(plaintextMessages)
    messages.append(message);
  else
    messages.append(atools::util::HtmlBuilder::errorMessage(message));
  qWarning() << "Error:" << message;
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

bool RouteStringReader::addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs,
                                     QStringList& cleanItems)
{
  QString ident = extractAirportIdent(cleanItems.takeFirst());

  map::MapAirport departure;
  airportQuerySim->getAirportByIdent(departure, ident);
  if(departure.isValid())
  {
    // qDebug() << "found" << departure.ident << "id" << departure.id;

    flightplan->setDepartureName(departure.name);
    flightplan->setDepartureIdent(departure.ident);
    flightplan->setDeparturePosition(departure.position);

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(departure, entry, false /* alternate */);
    flightplan->getEntries().append(entry);
    if(mapObjectRefs != nullptr)
      mapObjectRefs->append(map::MapObjectRefExt(departure.id, departure.position, map::AIRPORT, departure.ident));

    if(!cleanItems.isEmpty())
    {
      QString sidTrans = cleanItems.first();

      // Check if the next couple of strings are an waypoint/airway triplet
      // Skip procedure detection if yes
      bool foundAirway = false;
      if(cleanItems.size() > 2)
      {
        foundAirway =
          airwayQuery->hasAirwayForNameAndWaypoint(cleanItems.at(1), cleanItems.at(0)) &&
          airwayQuery->hasAirwayForNameAndWaypoint(cleanItems.at(1), cleanItems.at(2));
      }

      bool foundSid = false;
      QRegularExpressionMatch sidMatch = SID_STAR_TRANS.match(sidTrans);

      if(!foundAirway && sidMatch.hasMatch())
      {
        QString sid = sidMatch.captured(1);
        if(sid.size() == 7)
          // Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A
          sid.remove(4, 1);

        QString trans = sidMatch.captured(3);

        int sidTransId = -1;
        int sidId = procQuery->getSidId(departure, sid, QString(), true /* strict */);
        if(sidId != -1 && !trans.isEmpty())
        {
          sidTransId = procQuery->getSidTransitionId(departure, trans, sidId, true /* strict */);
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
          procQuery->fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, starLegs,
                                                       departureLegs);
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

bool RouteStringReader::addDestination(atools::fs::pln::Flightplan *flightplan,
                                       map::MapObjectRefExtVector *mapObjectRefs,
                                       QStringList& cleanItems, rs::RouteStringOptions options)
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
        // Consume STAR in list
        cleanItems.removeLast();

        // Add information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        procQuery->fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, star, departureLegs);
      }

      // Collect alternates ========================
      QStringList alternateIdents;
      for(const map::MapAirport& alt : airports)
        alternateIdents.prepend(alt.ident);

      if(!alternateIdents.isEmpty())
      {
        flightplan->getProperties().insert(atools::fs::pln::ALTERNATES, alternateIdents.join("#"));
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
      flightplan->setDestinationName(dest.name);
      flightplan->setDestinationIdent(dest.ident);
      flightplan->setDestinationPosition(dest.getPosition());

      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(dest, entry, false /* alternate */);
      flightplan->getEntries().append(entry);
      if(mapObjectRefs != nullptr)
        mapObjectRefs->append(map::MapObjectRefExt(dest.id, dest.position, map::AIRPORT, dest.ident));

      // Consume airport
      cleanItems.removeLast();

      // Get destination STAR ========================
      if(!star.isEmpty())
      {
        // Consume STAR in list
        cleanItems.removeLast();

        // Add information to the flight plan property list
        proc::MapProcedureLegs arrivalLegs, departureLegs;
        procQuery->fillFlightplanProcedureProperties(flightplan->getProperties(), arrivalLegs, star, departureLegs);
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

void RouteStringReader::destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs,
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
        int starId = procQuery->getStarId(destination, star, QString(), true /* strict */);
        if(starId != -1 && !trans.isEmpty())
        {
          starTransId = procQuery->getStarTransitionId(destination, trans, starId, true /* strict */);
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

atools::geo::Pos RouteStringReader::findFirstCoordinate(const QStringList& cleanItems)
{
  Pos lastPos;
  // Get coordinate of the first and last navaid ============================================
  MapSearchResult result;
  findWaypoints(result, cleanItems.first(), false);

  if(result.isEmpty(ROUTE_TYPES_NAVAIDS))
    // No navaid found ===============================================
    appendError(tr("First item %1 not found as waypoint, VOR or NDB.").arg(cleanItems.first()));
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
        if(cleanItems.size() > 1)
        {
          const QString& secondItem = cleanItems.at(1);
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
        for(const QString& item : cleanItems)
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
void RouteStringReader::extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints,
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

void RouteStringReader::filterWaypoints(MapSearchResult& result, atools::geo::Pos& lastPos,
                                        const MapSearchResult *lastResult,
                                        float maxDistance)
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

  if(!result.userpointsRoute.isEmpty())
    // User points have preference since they can be clearly identified
    lastPos = result.userpointsRoute.first().position;
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
        // Check for special case where a NDB and a VOR or waypoint with the same name are nearby.
        // Prefer VOR or waypoint in this case.
        if(dists.size() >= 2)
        {
          const ObjectDist& first = dists.first();
          const ObjectDist& second = dists.at(1);
          if(first.type == map::NDB &&
             (second.type == map::VOR || second.type == map::WAYPOINT) &&
             first.pos.distanceMeterTo(second.pos) < atools::geo::nmToMeter(MAX_CLOSE_NAVAIDS_DISTANCE_NM))
          {
            dists.removeFirst();

            if(result.hasNdb())
              result.ndbs.removeFirst();
          }
        }
        // Use position of nearest to last
        lastPos = dists.first().pos;
      }
    }
  }
}

void RouteStringReader::filterAirways(QList<ParseEntry>& resultList, int i)
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

void RouteStringReader::findWaypoints(MapSearchResult& result, const QString& item, bool matchWaypoints)
{
  bool searchCoords = false;
  if(item.length() > 5)
    // User coordinates for sure
    searchCoords = true;
  else
  {
    mapQuery->getMapObjectByIdent(result, ROUTE_TYPES_AND_AIRWAY, item);

    if(item.length() == 5 && result.waypoints.isEmpty())
      // Nothing found - try NAT waypoint (a few of these are also in the database)
      searchCoords = true;
  }

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
