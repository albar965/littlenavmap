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

#include "query/mapquery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "common/maptools.h"
#include "common/proctypes.h"
#include "mapgui/maplayer.h"
#include "online/onlinedatacontroller.h"
#include "airspace/airspacecontroller.h"
#include "logbook/logdatacontroller.h"
#include "userdata/userdatacontroller.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/waypointtrackquery.h"
#include "navapp.h"
#include "settings/settings.h"
#include "db/databasemanager.h"
#include "fs/util/fsutil.h"

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;
using map::MapAirport;
using map::MapVor;
using map::MapNdb;
using map::MapWaypoint;
using map::MapMarker;
using map::MapIls;
using map::MapParking;
using map::MapHelipad;
using map::MapUserpoint;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int MapQuery::queryMaxRows = map::MAX_MAP_OBJECTS;

// Queries only used for export ================================================
// Get assigned airport for navaid by name, region and coordinate closest to position
static QLatin1String AIRPORTIDENT_FROM_WAYPOINT("select a.ident, w.lonx, w.laty "
                                                "from waypoint w join airport a on a.airport_id = w.airport_id "
                                                "where w. ident = :ident and w.region like :region "
                                                "order by (abs(w.lonx - :lonx) + abs(w.laty - :laty)) limit 1");

static QLatin1String AIRPORTIDENT_FROM_VOR("select a.ident, v.lonx, v.laty "
                                           "from vor v join airport a on a.airport_id = v.airport_id "
                                           "where v.ident = :ident and v.region like :region "
                                           "order by (abs(v.lonx - :lonx) + abs(v.laty - :laty)) limit 1");

static QLatin1String AIRPORTIDENT_FROM_NDB("select a.ident, n.lonx, n.laty "
                                           "from ndb n join airport a on a.airport_id = n.airport_id "
                                           "where n.ident = :ident and n.region like :region "
                                           "order by (abs(n.lonx - :lonx) + abs(n.laty - :laty)) limit 1");
static float MAX_AIRPORT_IDENT_DISTANCE_M = atools::geo::nmToMeter(5.f);

MapQuery::MapQuery(atools::sql::SqlDatabase *sqlDb, SqlDatabase *sqlDbNav, SqlDatabase *sqlDbUser)
  : dbSim(sqlDb), dbNav(sqlDbNav), dbUser(sqlDbUser)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  runwayOverwiewCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "RunwayOverwiewCache",
                                                           1000).toInt());
  queryRectInflationFactor = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRows = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "MapQueryRowLimit", map::MAX_MAP_OBJECTS).toInt();
}

MapQuery::~MapQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

bool MapQuery::hasProcedures(const map::MapAirport& airport)
{
  map::MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasProcedures(airportNav);

  return false;
}

bool MapQuery::hasArrivalProcedures(const map::MapAirport& airport)
{
  map::MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasArrivalProcedures(airportNav);

  return false;
}

bool MapQuery::hasDepartureProcedures(const map::MapAirport& airport)
{
  map::MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasDepartureProcedures(airportNav);

  return false;
}

map::MapAirport MapQuery::getAirportSim(const map::MapAirport& airport)
{
  if(airport.navdata)
  {
    map::MapAirport retval;
    NavApp::getAirportQuerySim()->getAirportFuzzy(retval, airport);
    return retval;
  }
  return airport;
}

map::MapAirport MapQuery::getAirportNav(const map::MapAirport& airport)
{
  if(!airport.navdata)
  {
    map::MapAirport retval;
    NavApp::getAirportQueryNav()->getAirportFuzzy(retval, airport);
    return retval;
  }
  return airport;
}

void MapQuery::getAirportSimReplace(map::MapAirport& airport)
{
  if(airport.navdata)
    NavApp::getAirportQuerySim()->getAirportFuzzy(airport, airport);
}

void MapQuery::getAirportNavReplace(map::MapAirport& airport)
{
  if(!airport.navdata)
    NavApp::getAirportQueryNav()->getAirportFuzzy(airport, airport);
}

void MapQuery::getVorForWaypoint(map::MapVor& vor, int waypointId)
{
  vorByWaypointIdQuery->bindValue(":id", waypointId);
  vorByWaypointIdQuery->exec();
  if(vorByWaypointIdQuery->next())
    mapTypesFactory->fillVor(vorByWaypointIdQuery->record(), vor);
  vorByWaypointIdQuery->finish();
}

void MapQuery::getNdbForWaypoint(map::MapNdb& ndb, int waypointId)
{
  ndbByWaypointIdQuery->bindValue(":id", waypointId);
  ndbByWaypointIdQuery->exec();
  if(ndbByWaypointIdQuery->next())
    mapTypesFactory->fillNdb(ndbByWaypointIdQuery->record(), ndb);
  ndbByWaypointIdQuery->finish();
}

void MapQuery::getVorNearest(map::MapVor& vor, const atools::geo::Pos& pos)
{
  vorNearestQuery->bindValue(":lonx", pos.getLonX());
  vorNearestQuery->bindValue(":laty", pos.getLatY());
  vorNearestQuery->exec();
  if(vorNearestQuery->next())
    mapTypesFactory->fillVor(vorNearestQuery->record(), vor);
  vorNearestQuery->finish();
}

void MapQuery::getNdbNearest(map::MapNdb& ndb, const atools::geo::Pos& pos)
{
  ndbNearestQuery->bindValue(":lonx", pos.getLonX());
  ndbNearestQuery->bindValue(":laty", pos.getLatY());
  ndbNearestQuery->exec();
  if(ndbNearestQuery->next())
    mapTypesFactory->fillNdb(ndbNearestQuery->record(), ndb);
  ndbNearestQuery->finish();
}

map::MapResultIndex *MapQuery::getNearestNavaids(const Pos& pos, float distanceNm, map::MapTypes type,
                                                 int maxIls, float maxIlsDist)
{
  map::MapResultIndex *nearest = nearestNavaidsInternal(pos, distanceNm, type, maxIls, maxIlsDist);
  if(nearest == nullptr || nearest->size() < 5)
    nearest = nearestNavaidsInternal(pos, distanceNm * 4.f, type, maxIls, maxIlsDist);
  return nearest;
}

map::MapResultIndex *MapQuery::nearestNavaidsInternal(const Pos& pos, float distanceNm, map::MapTypes type,
                                                      int maxIls, float maxIlsDist)
{
  query::NearestCacheKeyNavaid key = {pos, distanceNm, type};

  map::MapResultIndex *result = nearestNavaidCache.object(key);

  if(result == nullptr)
  {
    map::MapResult res;

    // Create a rectangle that roughly covers the requested region
    atools::geo::Rect rect(pos, atools::geo::nmToMeter(distanceNm));

    if(type & map::VOR)
    {
      query::fetchObjectsForRect(rect, vorsByRectQuery, [ =, &res](atools::sql::SqlQuery *query) -> void {
        map::MapVor obj;
        mapTypesFactory->fillVor(query->record(), obj);
        res.vors.append(obj);
      });
    }

    if(type & map::NDB)
    {
      query::fetchObjectsForRect(rect, ndbsByRectQuery, [ =, &res](atools::sql::SqlQuery *query) -> void {
        map::MapNdb obj;
        mapTypesFactory->fillNdb(query->record(), obj);
        res.ndbs.append(obj);
      });
    }

    if(type & map::WAYPOINT)
    {
      query::fetchObjectsForRect(rect, NavApp::getWaypointTrackQuery()->getWaypointsByRectQueryTrack(),
                                 [ =, &res](atools::sql::SqlQuery *query) -> void {
        map::MapWaypoint obj;
        mapTypesFactory->fillWaypoint(query->record(), obj, true /* track database */);
        res.waypoints.append(obj);
      });

      query::fetchObjectsForRect(rect, NavApp::getWaypointTrackQuery()->getWaypointsByRectQuery(),
                                 [ =, &res](atools::sql::SqlQuery *query) -> void {
        map::MapWaypoint obj;
        mapTypesFactory->fillWaypoint(query->record(), obj, false /* track database */);

        if(!res.waypoints.contains(obj))
          res.waypoints.append(obj);
      });
      maptools::removeDuplicatesById(res.waypoints);
    }

    if(type & map::ILS)
    {
      QList<map::MapIls> ilsRes;

      query::fetchObjectsForRect(rect, ilsByRectQuery, [ =, &ilsRes](atools::sql::SqlQuery *query) -> void {
        map::MapIls obj;
        mapTypesFactory->fillIls(query->record(), obj);
        ilsRes.append(obj);
      });
      maptools::removeByDistance(ilsRes, pos, atools::geo::nmToMeter(maxIlsDist));
      maptools::sortByDistance(ilsRes, pos);
      res.ils.append(ilsRes.mid(0, maxIls));
    }

    result = new map::MapResultIndex;
    result->add(res);

    // Remove all that are too far away
    result->remove(pos, distanceNm);

    // Sort the rest by distance
    result->sort(pos, true /* sortNearToFar */);

    nearestNavaidCache.insert(key, result);
  }
  return result;
}

void MapQuery::getMapObjectByIdent(map::MapResult& result, map::MapTypes type,
                                   const QString& ident, const QString& region, const QString& airport,
                                   const Pos& sortByDistancePos, float maxDistanceMeter, bool airportFromNavDatabase)
{
  mapObjectByIdentInternal(result, type, ident, region, airport, sortByDistancePos, maxDistanceMeter,
                           airportFromNavDatabase);
}

void MapQuery::getMapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident,
                                   const QString& region, const QString& airport, bool airportFromNavDatabase)
{
  mapObjectByIdentInternal(result, type, ident, region, airport, EMPTY_POS, map::INVALID_DISTANCE_VALUE,
                           airportFromNavDatabase);
}

void MapQuery::mapObjectByIdentInternal(map::MapResult& result, map::MapTypes type, const QString& ident,
                                        const QString& region, const QString& airport, const Pos& sortByDistancePos,
                                        float maxDistanceMeter, bool airportFromNavDatabase)
{
  if(type & map::AIRPORT)
  {
    AirportQuery *airportQuery = airportFromNavDatabase ? NavApp::getAirportQueryNav() : NavApp::getAirportQuerySim();

    // Try exact ident first =====================
    map::MapAirport ap = airportQuery->getAirportByIdent(ident);
    if(ap.isValid())
      result.airports.append(ap);

    // Check for truncated X-Plane idents but only in X-Plane database
    if(ident.size() == 6 && NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
      airportQuery->getAirportsByTruncatedIdent(result.airports, ident);

    // Remove airports too far away from given distance
    maptools::removeByDistance(result.airports, sortByDistancePos, maxDistanceMeter);

    if(result.airports.isEmpty())
    {
      // Try fuzzy search for nearest by official ids =====================
      // Look through all fields (ICAO, IATA, FAA and local) for the given ident
      QList<map::MapAirport> airports =
        airportQuery->getAirportsByOfficialIdent(ident, &sortByDistancePos, maxDistanceMeter);
      result.airports.append(airports);
    }
  }

  if(type & map::VOR)
  {
    vorByIdentQuery->bindValue(":ident", ident);
    vorByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    vorByIdentQuery->exec();
    while(vorByIdentQuery->next())
    {
      map::MapVor vor;
      mapTypesFactory->fillVor(vorByIdentQuery->record(), vor);
      result.vors.append(vor);
    }
    maptools::sortByDistance(result.vors, sortByDistancePos);
    maptools::removeByDistance(result.vors, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::NDB)
  {
    ndbByIdentQuery->bindValue(":ident", ident);
    ndbByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    ndbByIdentQuery->exec();
    while(ndbByIdentQuery->next())
    {
      map::MapNdb ndb;
      mapTypesFactory->fillNdb(ndbByIdentQuery->record(), ndb);
      result.ndbs.append(ndb);
    }
    maptools::sortByDistance(result.ndbs, sortByDistancePos);
    maptools::removeByDistance(result.ndbs, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::WAYPOINT)
  {
    NavApp::getWaypointTrackQuery()->getWaypointByIdent(result.waypoints, ident, region);
    maptools::sortByDistance(result.waypoints, sortByDistancePos);
    maptools::removeByDistance(result.waypoints, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::ILS)
  {
    ilsByIdentQuery->bindValue(":ident", ident);
    ilsByIdentQuery->bindValue(":airport", airport);
    ilsByIdentQuery->exec();
    while(ilsByIdentQuery->next())
    {
      map::MapIls ils;
      mapTypesFactory->fillIls(ilsByIdentQuery->record(), ils);
      result.ils.append(ils);
    }
    maptools::sortByDistance(result.ils, sortByDistancePos);
    maptools::removeByDistance(result.ils, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::RUNWAYEND)
  {
    if(airportFromNavDatabase)
      NavApp::getAirportQueryNav()->getRunwayEndByNames(result, ident, airport);
    else
      NavApp::getAirportQuerySim()->getRunwayEndByNames(result, ident, airport);
  }

  if(type & map::AIRWAY)
    NavApp::getAirwayTrackQuery()->getAirwaysByName(result.airways, ident);
}

void MapQuery::getMapObjectById(map::MapResult& result, map::MapTypes type, map::MapAirspaceSources src,
                                int id, bool airportFromNavDatabase)
{
  if(type == map::AIRPORT)
  {
    map::MapAirport airport = (airportFromNavDatabase ?
                               NavApp::getAirportQueryNav() :
                               NavApp::getAirportQuerySim())->getAirportById(id);
    if(airport.isValid())
      result.airports.append(airport);
  }
  else if(type == map::VOR)
  {
    map::MapVor vor = getVorById(id);
    if(vor.isValid())
      result.vors.append(vor);
  }
  else if(type == map::NDB)
  {
    map::MapNdb ndb = getNdbById(id);
    if(ndb.isValid())
      result.ndbs.append(ndb);
  }
  else if(type == map::WAYPOINT)
  {
    map::MapWaypoint waypoint = NavApp::getWaypointTrackQuery()->getWaypointById(id);
    if(waypoint.isValid())
      result.waypoints.append(waypoint);
  }
  else if(type == map::USERPOINT)
  {
    map::MapUserpoint userPoint = NavApp::getUserdataController()->getUserpointById(id);
    if(userPoint.isValid())
      result.userpoints.append(userPoint);
  }
  else if(type == map::LOGBOOK)
  {
    map::MapLogbookEntry logEntry = NavApp::getLogdataController()->getLogEntryById(id);
    if(logEntry.isValid())
      result.logbookEntries.append(logEntry);
  }
  else if(type == map::ILS)
  {
    map::MapIls ils = getIlsById(id);
    if(ils.isValid())
      result.ils.append(ils);
  }
  else if(type == map::RUNWAYEND)
  {
    map::MapRunwayEnd end = (airportFromNavDatabase ?
                             NavApp::getAirportQueryNav() :
                             NavApp::getAirportQuerySim())->getRunwayEndById(id);
    if(end.isValid())
      result.runwayEnds.append(end);
  }
  else if(type == map::AIRSPACE)
  {
    map::MapAirspace airspace = NavApp::getAirspaceController()->getAirspaceById({id, src});
    if(airspace.isValidAirspace())
      result.airspaces.append(airspace);
  }
  else if(type == map::AIRCRAFT_ONLINE)
  {
    atools::fs::sc::SimConnectAircraft aircraft;
    NavApp::getOnlinedataController()->getClientAircraftById(aircraft, id);
    result.onlineAircraft.append(map::MapOnlineAircraft(aircraft));
  }
  else if(type == map::AIRWAY)
  {
    map::MapAirway airway = NavApp::getAirwayTrackQuery()->getAirwayById(id);
    if(airway.isValid())
      result.airways.append(airway);
  }
}

map::MapVor MapQuery::getVorById(int id)
{
  map::MapVor vor;
  vorByIdQuery->bindValue(":id", id);
  vorByIdQuery->exec();
  if(vorByIdQuery->next())
    mapTypesFactory->fillVor(vorByIdQuery->record(), vor);
  vorByIdQuery->finish();
  return vor;
}

map::MapNdb MapQuery::getNdbById(int id)
{
  map::MapNdb ndb;
  ndbByIdQuery->bindValue(":id", id);
  ndbByIdQuery->exec();
  if(ndbByIdQuery->next())
    mapTypesFactory->fillNdb(ndbByIdQuery->record(), ndb);
  ndbByIdQuery->finish();
  return ndb;
}

map::MapIls MapQuery::getIlsById(int id)
{
  map::MapIls ils;
  ilsByIdQuery->bindValue(":id", id);
  ilsByIdQuery->exec();
  if(ilsByIdQuery->next())
    mapTypesFactory->fillIls(ilsByIdQuery->record(), ils);
  ilsByIdQuery->finish();
  return ils;
}

QVector<map::MapIls> MapQuery::getIlsByAirportAndRunway(const QString& airportIdent, const QString& runway)
{
  QVector<map::MapIls> ils;
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(runway))
  {
    ils = ilsByAirportAndRunway(airportIdent, rname);
    if(!ils.isEmpty())
      break;
  }
  return ils;
}

QVector<MapIls> MapQuery::getIlsByAirportAndIdent(const QString& airportIdent, const QString& ilsIdent)
{
  QVector<map::MapIls> ilsList;
  ilsQuerySimByAirportAndIdent->bindValue(":apt", airportIdent);
  ilsQuerySimByAirportAndIdent->bindValue(":ident", ilsIdent);
  ilsQuerySimByAirportAndIdent->exec();
  while(ilsQuerySimByAirportAndIdent->next())
  {
    map::MapIls ils;
    mapTypesFactory->fillIls(ilsQuerySimByAirportAndIdent->record(), ils);
    ilsList.append(ils);
  }
  return ilsList;
}

QVector<map::MapIls> MapQuery::ilsByAirportAndRunway(const QString& airportIdent, const QString& runway)
{
  QVector<map::MapIls> ilsList;
  ilsQuerySimByAirportAndRw->bindValue(":apt", airportIdent);
  ilsQuerySimByAirportAndRw->bindValue(":rwy", runway);
  ilsQuerySimByAirportAndRw->exec();
  while(ilsQuerySimByAirportAndRw->next())
  {
    map::MapIls ils;
    mapTypesFactory->fillIls(ilsQuerySimByAirportAndRw->record(), ils);
    ilsList.append(ils);
  }
  return ilsList;
}

void MapQuery::getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer,
                                       bool airportDiagram, map::MapTypes types,
                                       int xs, int ys, int screenDistance, map::MapResult& result)
{
  using maptools::insertSortedByDistance;
  using maptools::insertSortedByTowerDistance;

  int x, y;
  if(mapLayer->isAirport() && types.testFlag(map::AIRPORT))
  {
    for(int i = airportCache.list.size() - 1; i >= 0; i--)
    {
      const MapAirport& airport = airportCache.list.at(i);

      if(airport.isVisible(types))
      {
        if(conv.wToS(airport.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, airport);

        if(airportDiagram)
        {
          // Include tower for airport diagrams
          if(conv.wToS(airport.towerCoords, x, y) &&
             atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByTowerDistance(conv, result.towers, xs, ys, airport);
        }
      }
    }
  }

  if(mapLayer->isVor() && types.testFlag(map::VOR))
  {
    for(int i = vorCache.list.size() - 1; i >= 0; i--)
    {
      const MapVor& vor = vorCache.list.at(i);
      if(conv.wToS(vor.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, vor);
    }
  }

  if(mapLayer->isNdb() && types.testFlag(map::NDB))
  {
    for(int i = ndbCache.list.size() - 1; i >= 0; i--)
    {
      const MapNdb& ndb = ndbCache.list.at(i);
      if(conv.wToS(ndb.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, ndb);
    }
  }

  // No flag since visibility is defined by type
  if(mapLayer->isUserpoint())
  {
    for(int i = userpointCache.list.size() - 1; i >= 0; i--)
    {
      const MapUserpoint& wp = userpointCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.userpoints, &result.userpointIds, xs, ys, wp);
    }
  }

  // Add waypoints that displayed together with airways =================================
  if((mapLayer->isAirwayWaypoint() && (types.testFlag(map::AIRWAYV) || types.testFlag(map::AIRWAYJ))) ||
     (mapLayer->isTrackWaypoint() && types.testFlag(map::TRACK)) ||
     (mapLayer->isWaypoint() && types.testFlag(map::WAYPOINT)))
    NavApp::getWaypointTrackQuery()->getNearestScreenObjects(conv, mapLayer, types, xs, ys, screenDistance, result);

  if(mapLayer->isMarker() && types.testFlag(map::MARKER))
  {
    for(int i = markerCache.list.size() - 1; i >= 0; i--)
    {
      const MapMarker& wp = markerCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.markers, nullptr, xs, ys, wp);
    }
  }

  if(mapLayer->isIls() && types.testFlag(map::ILS))
  {
    for(int i = ilsCache.list.size() - 1; i >= 0; i--)
    {
      const MapIls& wp = ilsCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ils, nullptr, xs, ys, wp);
    }
  }

  // Get objects from airport diagram =====================================================
  if(mapLayer->isAirport() && types.testFlag(map::AIRPORT))
  {
    if(airportDiagram)
    {
      QHash<int, QList<map::MapParking> > parkingCache = NavApp::getAirportQuerySim()->getParkingCache();

      // Also check parking and helipads in airport diagrams
      for(const QList<MapParking>& parkings : parkingCache)
      {
        for(const MapParking& p : parkings)
        {
          if(conv.wToS(p.position, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByDistance(conv, result.parkings, nullptr, xs, ys, p);
        }
      }

      QHash<int, QList<map::MapHelipad> > helipadCache = NavApp::getAirportQuerySim()->getHelipadCache();

      for(const QList<MapHelipad>& helipads : helipadCache)
      {
        for(const MapHelipad& p : helipads)
        {
          if(conv.wToS(p.position, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByDistance(conv, result.helipads, nullptr, xs, ys, p);
        }
      }
    }
  }
}

const QList<map::MapAirport> *MapQuery::getAirports(const Marble::GeoDataLatLonBox& rect,
                                                    const MapLayer *mapLayer, bool lazy, map::MapTypes types,
                                                    bool& overflow)
{
  // Get flags for running separate queries for add-on and normal airports
  bool addon = types.testFlag(map::AIRPORT_ADDON);
  bool normal = types & (map::AIRPORT_HARD | map::AIRPORT_SOFT | map::AIRPORT_EMPTY);

  airportCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                           [ = ](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirport(newLayer) &&
    // Invalidate cache if settings differ
    airportCacheAddonFlag == addon && airportCacheNormalFlag == normal;
  });

  airportCacheAddonFlag = addon;
  airportCacheNormalFlag = normal;

  switch(mapLayer->getDataSource())
  {
    case layer::ALL:
      airportByRectQuery->bindValue(":minlength", mapLayer->getMinRunwayLength());
      return fetchAirports(rect, airportByRectQuery, lazy, false /* overview */, addon, normal, overflow);

    case layer::MEDIUM:
      // Airports > 4000 ft
      return fetchAirports(rect, airportMediumByRectQuery, lazy, true /* overview */, addon, normal, overflow);

    case layer::LARGE:
      // Airports > 8000 ft
      return fetchAirports(rect, airportLargeByRectQuery, lazy, true /* overview */, addon, normal, overflow);

  }
  return nullptr;
}

const QList<map::MapVor> *MapQuery::getVors(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                            bool lazy, bool& overflow)
{
  vorCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                       [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersVor(newLayer);
  });

  if(vorCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, vorsByRectQuery);
      vorsByRectQuery->exec();
      while(vorsByRectQuery->next())
      {
        map::MapVor vor;
        mapTypesFactory->fillVor(vorsByRectQuery->record(), vor);
        vorCache.list.append(vor);
      }
    }
  }
  overflow = vorCache.validate(queryMaxRows);
  return &vorCache.list;
}

const QList<map::MapNdb> *MapQuery::getNdbs(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                            bool lazy, bool& overflow)
{
  ndbCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                       [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersNdb(newLayer);
  });

  if(ndbCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, ndbsByRectQuery);
      ndbsByRectQuery->exec();
      while(ndbsByRectQuery->next())
      {
        map::MapNdb ndb;
        mapTypesFactory->fillNdb(ndbsByRectQuery->record(), ndb);
        ndbCache.list.append(ndb);
      }
    }
  }
  overflow = ndbCache.validate(queryMaxRows);
  return &ndbCache.list;
}

const QList<map::MapUserpoint> MapQuery::getUserdataPoints(const GeoDataLatLonBox& rect, const QStringList& types,
                                                           const QStringList& typesAll, bool unknownType,
                                                           float distance)
{
  // No caching here since points can change and the dataset is usually small
  QList<map::MapUserpoint> retval;
  userpointCache.clear();

  // Display either unknown or any type
  if(unknownType || !types.isEmpty())
  {
    bool allTypesSelected = types == typesAll;

    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, userdataPointByRectQuery);
      userdataPointByRectQuery->bindValue(":dist", distance);

      QStringList queryTypes;
      if(unknownType || (allTypesSelected && unknownType))
        // Either query all unknows too and filter later or all and unknown are selected
        queryTypes.append("%");
      else
        queryTypes = types;

      for(const QString& queryType : queryTypes)
      {
        userdataPointByRectQuery->bindValue(":type", queryType);
        userdataPointByRectQuery->exec();
        while(userdataPointByRectQuery->next())
        {
          if(unknownType && !allTypesSelected)
          {
            // Need to filter manually here
            QString pointType = userdataPointByRectQuery->valueStr("type");

            // Ignore if not unknown and not in selected types
            if(typesAll.contains(pointType) && !types.contains(pointType))
              continue;
          }

          map::MapUserpoint userPoint;
          mapTypesFactory->fillUserdataPoint(userdataPointByRectQuery->record(), userPoint);
          retval.append(userPoint);

          // Cache has to be kept for map screen index
          userpointCache.list.append(userPoint);
        }
      }
    }
  }
  return retval;
}

QString MapQuery::getAirportIdentFromWaypoint(const QString& ident, const QString& region, const Pos& pos,
                                              bool found)
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_WAYPOINT, ident, region, pos, found);
}

QString MapQuery::getAirportIdentFromVor(const QString& ident, const QString& region, const Pos& pos, bool found)
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_VOR, ident, region, pos, found);
}

QString MapQuery::getAirportIdentFromNdb(const QString& ident, const QString& region, const Pos& pos, bool found)
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_NDB, ident, region, pos, found);
}

QString MapQuery::airportIdentFromQuery(const QString& queryStr, const QString& ident, const QString& region,
                                        const Pos& pos, bool& found)
{
  found = false;

  // Query for nearest navaid
  SqlQuery query(dbNav);
  query.prepare(queryStr);
  query.bindValue(":ident", ident);
  query.bindValue(":region", region.isEmpty() ? "%" : region);
  query.bindValue(":lonx", pos.getLonX());
  query.bindValue(":laty", pos.getLatY());

  QString airport;
  query.exec();
  if(query.next())
  {
    // Check if max distance is not exceeded
    Pos navaidPos(query.valueFloat("lonx"), query.valueFloat("laty"));
    if(pos.distanceMeterTo(pos) < MAX_AIRPORT_IDENT_DISTANCE_M)
    {
      // Get airport ident and record that a navaid was found
      found = true;
      airport = query.valueStr("ident");
    }
  }
  query.finish();
  return airport;
}

const QList<map::MapMarker> *MapQuery::getMarkers(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                  bool lazy, bool& overflow)
{
  markerCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                          [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersMarker(newLayer);
  });

  if(markerCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, markersByRectQuery);
      markersByRectQuery->exec();
      while(markersByRectQuery->next())
      {
        map::MapMarker marker;
        mapTypesFactory->fillMarker(markersByRectQuery->record(), marker);
        markerCache.list.append(marker);
      }
    }
  }
  overflow = markerCache.validate(queryMaxRows);
  return &markerCache.list;
}

const QList<map::MapIls> *MapQuery::getIls(GeoDataLatLonBox rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  ilsCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                       [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersIls(newLayer);
  });

  if(ilsCache.list.isEmpty() && !lazy)
  {
    // ILS length is 9 NM * 1' per degree
    double increase = atools::geo::toRadians(9. / 60.);

    // Increase bounding rect since ILS has no bounding to query
    rect.setBoundaries(rect.north() + increase, rect.south() - increase,
                       rect.east() + increase, rect.west() - increase);

    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, ilsByRectQuery);

      ilsByRectQuery->exec();
      while(ilsByRectQuery->next())
      {
        map::MapIls ils;
        mapTypesFactory->fillIls(ilsByRectQuery->record(), ils);
        ilsCache.list.append(ils);
      }
    }
  }
  overflow = ilsCache.validate(queryMaxRows);
  return &ilsCache.list;
}

/*
 * Get airport cache
 * @param reverse reverse order of airports to have unimportant small ones below in painting order
 * @param lazy do not update cache - instead return incomplete resut
 * @param overview fetch only incomplete data for overview airports
 * @return pointer to the airport cache
 */
const QList<map::MapAirport> *MapQuery::fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                                      atools::sql::SqlQuery *query,
                                                      bool lazy, bool overview, bool addon, bool normal, bool& overflow)
{
  if(airportCache.list.isEmpty() && !lazy)
  {
    bool navdata = NavApp::getDatabaseManager()->getNavDatabaseStatus() == dm::NAVDATABASE_ALL;
    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      // Avoid duplicates between both queries
      QSet<int> ids;

      // Get normal airports ==========
      if(normal)
      {
        query::bindRect(r, query);
        query->exec();
        while(query->next())
        {
          map::MapAirport ap;
          if(overview)
            // Fill only a part of the object
            mapTypesFactory->fillAirportForOverview(query->record(), ap, navdata,
                                                    NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
          else
            mapTypesFactory->fillAirport(query->record(), ap, true /* complete */, navdata,
                                         NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);

          ids.insert(ap.id);
          airportCache.list.append(ap);
        }
      }

      // Get add-on airports ==========
      if(addon)
      {
        query::bindRect(r, airportAddonByRectQuery);
        airportAddonByRectQuery->exec();
        while(airportAddonByRectQuery->next())
        {
          map::MapAirport ap;
          if(overview)
            // Fill only a part of the object
            mapTypesFactory->fillAirportForOverview(airportAddonByRectQuery->record(), ap, navdata,
                                                    NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
          else
            mapTypesFactory->fillAirport(airportAddonByRectQuery->record(), ap, true /* complete */, navdata,
                                         NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
          if(!ids.contains(ap.id))
            airportCache.list.append(ap);
        }
      }
    }
  }
  overflow = airportCache.validate(queryMaxRows);
  return &airportCache.list;
}

const QList<map::MapRunway> *MapQuery::getRunwaysForOverview(int airportId)
{
  if(runwayOverwiewCache.contains(airportId))
    return runwayOverwiewCache.object(airportId);
  else
  {
    using atools::geo::Pos;

    runwayOverviewQuery->bindValue(":airportId", airportId);
    runwayOverviewQuery->exec();

    QList<map::MapRunway> *rws = new QList<map::MapRunway>;
    while(runwayOverviewQuery->next())
    {
      map::MapRunway runway;
      mapTypesFactory->fillRunway(runwayOverviewQuery->record(), runway, true);
      rws->append(runway);
    }
    runwayOverwiewCache.insert(airportId, rws);
    return rws;
  }
}

/* Get runway end and try lower and higher numbers if nothing was found - adds a dummy entry with airport
 * position if no runway ends were found */
void MapQuery::getRunwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                                       const map::MapAirport& airport, bool navData)
{
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(name))
  {
    runwayEndByNameFuzzy(runwayEnds, rname, airport, navData);
    if(!runwayEnds.isEmpty())
      return;
  }
}

void MapQuery::runwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                                    const map::MapAirport& airport, bool navData)
{
  AirportQuery *aquery = navData ? NavApp::getAirportQueryNav() : NavApp::getAirportQuerySim();
  map::MapResult result;

  if(!name.isEmpty())
  {
    QString bestRunway = atools::fs::util::runwayBestFit(name, aquery->getRunwayNames(airport.id));

    if(!bestRunway.isEmpty())
      getMapObjectByIdent(result, map::RUNWAYEND, bestRunway, QString(), airport.ident,
                          navData /* airport or runway from nav database */);
  }

  if(result.runwayEnds.isEmpty())
  {
    // Get heading of runway by name
    int rwnum = 0;
    atools::fs::util::runwayNameSplit(name, &rwnum);

    // Create a dummy with the airport position as the last resort
    map::MapRunwayEnd end;
    end.navdata = true;
    end.name = name.startsWith("RW") ? name.mid(2) : name;
    end.heading = rwnum * 10.f;
    end.position = airport.position;
    end.secondary = false;
    result.runwayEnds.append(end);

#ifdef DEBUG_INFORMATION
    qWarning() << "Created runway dummy" << name << "for airport" << airport.ident;
#endif
  }
#ifdef DEBUG_INFORMATION
  else if(result.runwayEnds.first().name != name)
    qWarning() << "Found runway" << result.runwayEnds.first().name
               << "as replacement for" << name << "airport" << airport.ident;
#endif

  runwayEnds = result.runwayEnds;
}

void MapQuery::initQueries()
{
  // Common where clauses
  static const QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static const QString whereIdentRegion("ident = :ident and region like :region");
  static const QString whereLimit("limit " + QString::number(queryMaxRows));

  // Common select statements
  QStringList const airportQueryBase = AirportQuery::airportColumns(dbSim);
  QStringList const airportQueryBaseOverview = AirportQuery::airportOverviewColumns(dbSim);

  static const QString vorQueryBase(
    "vor_id, ident, name, region, type, name, frequency, channel, range, dme_only, dme_altitude, "
    "mag_var, altitude, lonx, laty ");
  static const QString ndbQueryBase(
    "ndb_id, ident, name, region, type, name, frequency, range, mag_var, altitude, lonx, laty ");

  static const QString ilsQueryBase(
    "ils_id, ident, name, region, mag_var, loc_heading, gs_pitch, frequency, range, dme_range, loc_width, "
    "end1_lonx, end1_laty, end_mid_lonx, end_mid_laty, end2_lonx, end2_laty, altitude, lonx, laty");

  deInitQueries();

  vorByIdentQuery = new SqlQuery(dbNav);
  vorByIdentQuery->prepare("select " + vorQueryBase + " from vor where " + whereIdentRegion);

  ndbByIdentQuery = new SqlQuery(dbNav);
  ndbByIdentQuery->prepare("select " + ndbQueryBase + " from ndb where " + whereIdentRegion);

  ilsByIdentQuery = new SqlQuery(dbSim);
  ilsByIdentQuery->prepare("select " + ilsQueryBase +
                           " from ils where ident = :ident and loc_airport_ident = :airport");

  vorByIdQuery = new SqlQuery(dbNav);
  vorByIdQuery->prepare("select " + vorQueryBase + " from vor where vor_id = :id");

  ndbByIdQuery = new SqlQuery(dbNav);
  ndbByIdQuery->prepare("select " + ndbQueryBase + " from ndb where ndb_id = :id");

  // Get VOR for waypoint
  vorByWaypointIdQuery = new SqlQuery(dbNav);
  vorByWaypointIdQuery->prepare("select " + vorQueryBase +
                                " from vor where vor_id in "
                                "(select nav_id from waypoint w where w.waypoint_id = :id)");

  // Get NDB for waypoint
  ndbByWaypointIdQuery = new SqlQuery(dbNav);
  ndbByWaypointIdQuery->prepare("select " + ndbQueryBase +
                                " from ndb where ndb_id in "
                                "(select nav_id from waypoint w where w.waypoint_id = :id)");

  // Get nearest VOR
  vorNearestQuery = new SqlQuery(dbNav);
  vorNearestQuery->prepare(
    "select " + vorQueryBase + " from vor order by (abs(lonx - :lonx) + abs(laty - :laty)) limit 1");

  // Get nearest NDB
  ndbNearestQuery = new SqlQuery(dbNav);
  ndbNearestQuery->prepare(
    "select " + ndbQueryBase + " from ndb order by (abs(lonx - :lonx) + abs(laty - :laty)) limit 1");

  ilsByIdQuery = new SqlQuery(dbSim);
  ilsByIdQuery->prepare("select " + ilsQueryBase + " from ils where ils_id = :id");

  ilsQuerySimByAirportAndRw = new SqlQuery(dbSim);
  ilsQuerySimByAirportAndRw->prepare("select " + ilsQueryBase +
                                     " from ils where loc_airport_ident = :apt and loc_runway_name = :rwy");

  ilsQuerySimByAirportAndIdent = new SqlQuery(dbSim);
  ilsQuerySimByAirportAndIdent->prepare("select " + ilsQueryBase +
                                        " from ils where loc_airport_ident = :apt and ident = :ident");

  airportByRectQuery = new SqlQuery(dbSim);
  airportByRectQuery->prepare(
    "select " + airportQueryBase.join(", ") + " from airport where " + whereRect +
    " and longest_runway_length >= :minlength "
    + whereLimit);

  airportAddonByRectQuery = new SqlQuery(dbSim);
  airportAddonByRectQuery->prepare(
    "select " + airportQueryBase.join(", ") + " from airport where " + whereRect + " and is_addon = 1 " + whereLimit);

  airportMediumByRectQuery = new SqlQuery(dbSim);
  airportMediumByRectQuery->prepare(
    "select " + airportQueryBaseOverview.join(", ") + " from airport_medium where " + whereRect + " " + whereLimit);

  airportLargeByRectQuery = new SqlQuery(dbSim);
  airportLargeByRectQuery->prepare(
    "select " + airportQueryBaseOverview.join(", ") + " from airport_large where " + whereRect + " " + whereLimit);

  // Runways > 4000 feet for simplyfied runway overview
  runwayOverviewQuery = new SqlQuery(dbSim);
  runwayOverviewQuery->prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000 " + whereLimit);

  vorsByRectQuery = new SqlQuery(dbNav);
  vorsByRectQuery->prepare("select " + vorQueryBase + " from vor where " + whereRect + " " + whereLimit);

  ndbsByRectQuery = new SqlQuery(dbNav);
  ndbsByRectQuery->prepare("select " + ndbQueryBase + " from ndb where " + whereRect + " " + whereLimit);

  userdataPointByRectQuery = new SqlQuery(dbUser);
  userdataPointByRectQuery->prepare("select * from userdata "
                                    "where " + whereRect + " and visible_from > :dist and type like :type " +
                                    whereLimit);

  markersByRectQuery = new SqlQuery(dbSim);
  markersByRectQuery->prepare(
    "select marker_id, type, ident, heading, lonx, laty "
    "from marker "
    "where " + whereRect + " " + whereLimit);

  ilsByRectQuery = new SqlQuery(dbSim);
  ilsByRectQuery->prepare("select " + ilsQueryBase + " from ils where " + whereRect + " " + whereLimit);
}

void MapQuery::deInitQueries()
{
  airportCache.clear();
  vorCache.clear();
  ndbCache.clear();
  markerCache.clear();
  ilsCache.clear();
  runwayOverwiewCache.clear();

  delete airportByRectQuery;
  airportByRectQuery = nullptr;
  delete airportAddonByRectQuery;
  airportAddonByRectQuery = nullptr;
  delete airportMediumByRectQuery;
  airportMediumByRectQuery = nullptr;
  delete airportLargeByRectQuery;
  airportLargeByRectQuery = nullptr;

  delete runwayOverviewQuery;
  runwayOverviewQuery = nullptr;

  delete vorsByRectQuery;
  vorsByRectQuery = nullptr;
  delete ndbsByRectQuery;
  ndbsByRectQuery = nullptr;
  delete markersByRectQuery;
  markersByRectQuery = nullptr;
  delete ilsByRectQuery;
  ilsByRectQuery = nullptr;

  delete userdataPointByRectQuery;
  userdataPointByRectQuery = nullptr;

  delete vorByIdentQuery;
  vorByIdentQuery = nullptr;
  delete ndbByIdentQuery;
  ndbByIdentQuery = nullptr;
  delete ilsByIdentQuery;
  ilsByIdentQuery = nullptr;

  delete vorByIdQuery;
  vorByIdQuery = nullptr;
  delete ndbByIdQuery;
  ndbByIdQuery = nullptr;

  delete vorByWaypointIdQuery;
  vorByWaypointIdQuery = nullptr;
  delete ndbByWaypointIdQuery;
  ndbByWaypointIdQuery = nullptr;

  delete vorNearestQuery;
  vorNearestQuery = nullptr;
  delete ndbNearestQuery;
  ndbNearestQuery = nullptr;

  delete ilsByIdQuery;
  ilsByIdQuery = nullptr;

  delete ilsQuerySimByAirportAndRw;
  ilsQuerySimByAirportAndRw = nullptr;

  delete ilsQuerySimByAirportAndIdent;
  ilsQuerySimByAirportAndIdent = nullptr;
}
