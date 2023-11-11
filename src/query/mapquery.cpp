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

#include "query/mapquery.h"

#include "airspace/airspacecontroller.h"
#include "common/constants.h"
#include "common/mapresult.h"
#include "common/maptools.h"
#include "common/maptypesfactory.h"
#include "fs/util/fsutil.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/maplayer.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/waypointtrackquery.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlutil.h"
#include "userdata/userdatacontroller.h"

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
using map::MapAirportMsa;
using map::MapHolding;

static double queryRectInflationFactor = 0.5;
static double queryRectInflationIncrement = 0.5;
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

MapQuery::MapQuery(atools::sql::SqlDatabase *sqlDbSim, SqlDatabase *sqlDbNav, SqlDatabase *sqlDbUser)
  : dbSim(sqlDbSim), dbNav(sqlDbNav), dbUser(sqlDbUser)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  runwayOverwiewCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "RunwayOverwiewCache", 1000).toInt());
  queryRectInflationFactor = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "QueryRectInflationFactor", 0.5).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "QueryRectInflationIncrement", 0.5).toDouble();
  queryMaxRows = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "MapQueryRowLimit", map::MAX_MAP_OBJECTS).toInt();
}

MapQuery::~MapQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

bool MapQuery::hasProcedures(const map::MapAirport& airport) const
{
  MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasProcedures(airportNav);

  return false;
}

bool MapQuery::hasArrivalProcedures(const map::MapAirport& airport) const
{
  MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasArrivalProcedures(airportNav);

  return false;
}

bool MapQuery::hasDepartureProcedures(const map::MapAirport& airport) const
{
  MapAirport airportNav = getAirportNav(airport);
  if(airportNav.isValid())
    return NavApp::getAirportQueryNav()->hasDepartureProcedures(airportNav);

  return false;
}

map::MapAirport MapQuery::getAirportSim(const map::MapAirport& airport) const
{
  if(airport.navdata)
  {
    MapAirport retval;
    NavApp::getAirportQuerySim()->getAirportFuzzy(retval, airport);
    return retval;
  }
  return airport;
}

map::MapAirport MapQuery::getAirportNav(const map::MapAirport& airport) const
{
  if(!airport.navdata)
  {
    MapAirport retval;
    NavApp::getAirportQueryNav()->getAirportFuzzy(retval, airport);
    return retval;
  }
  return airport;
}

void MapQuery::getAirportSimReplace(map::MapAirport& airport) const
{
  if(airport.navdata)
    NavApp::getAirportQuerySim()->getAirportFuzzy(airport, airport);
}

void MapQuery::getAirportNavReplace(map::MapAirport& airport) const
{
  if(!airport.navdata)
    NavApp::getAirportQueryNav()->getAirportFuzzy(airport, airport);
}

void MapQuery::getAirportTransitionAltiudeAndLevel(const map::MapAirport& airport, float& transitionAltitude, float& transitionLevel) const
{
  map::MapAirport nav(getAirportNav(airport));
  transitionAltitude = transitionLevel = 0.f;

  if(nav.isValid())
  {
    // Get both values from navdatabase since it is more reliable
    transitionAltitude = nav.transitionAltitude;
    transitionLevel = nav.transitionLevel;
  }
  else
  {
    // Fall back to simulator database
    map::MapAirport sim(getAirportSim(airport));
    if(sim.isValid())
    {
      transitionAltitude = sim.transitionAltitude;
      transitionLevel = sim.transitionLevel;
    }
  }
}

void MapQuery::getVorForWaypoint(map::MapVor& vor, int waypointId) const
{
  if(!query::valid(Q_FUNC_INFO, vorByWaypointIdQuery))
    return;

  vorByWaypointIdQuery->bindValue(":id", waypointId);
  vorByWaypointIdQuery->exec();
  if(vorByWaypointIdQuery->next())
    mapTypesFactory->fillVor(vorByWaypointIdQuery->record(), vor);
  vorByWaypointIdQuery->finish();
}

void MapQuery::getNdbForWaypoint(map::MapNdb& ndb, int waypointId) const
{
  if(!query::valid(Q_FUNC_INFO, ndbByWaypointIdQuery))
    return;

  ndbByWaypointIdQuery->bindValue(":id", waypointId);
  ndbByWaypointIdQuery->exec();
  if(ndbByWaypointIdQuery->next())
    mapTypesFactory->fillNdb(ndbByWaypointIdQuery->record(), ndb);
  ndbByWaypointIdQuery->finish();
}

void MapQuery::getVorNearest(map::MapVor& vor, const atools::geo::Pos& pos) const
{
  if(!query::valid(Q_FUNC_INFO, vorNearestQuery))
    return;

  vorNearestQuery->bindValue(":lonx", pos.getLonX());
  vorNearestQuery->bindValue(":laty", pos.getLatY());
  vorNearestQuery->exec();
  if(vorNearestQuery->next())
    mapTypesFactory->fillVor(vorNearestQuery->record(), vor);
  vorNearestQuery->finish();
}

void MapQuery::getNdbNearest(map::MapNdb& ndb, const atools::geo::Pos& pos) const
{
  if(!query::valid(Q_FUNC_INFO, ndbNearestQuery))
    return;

  ndbNearestQuery->bindValue(":lonx", pos.getLonX());
  ndbNearestQuery->bindValue(":laty", pos.getLatY());
  ndbNearestQuery->exec();
  if(ndbNearestQuery->next())
    mapTypesFactory->fillNdb(ndbNearestQuery->record(), ndb);
  ndbNearestQuery->finish();
}

void MapQuery::resolveWaypointNavaids(const QList<MapWaypoint>& allWaypoints, QHash<int, MapWaypoint>& waypoints, QHash<int, MapVor>& vors,
                                      QHash<int, MapNdb>& ndbs, bool flightplan, bool normalWaypoints, bool victorWaypoints,
                                      bool jetWaypoints, bool trackWaypoints) const
{
  for(const MapWaypoint& wp : allWaypoints)
  {
    // Add waypoint if airway/track status matches
    if(normalWaypoints || // All waypoints shown
       (wp.hasJetAirways && jetWaypoints) || // Jet airways shown - show waypoints too
       (wp.hasVictorAirways && victorWaypoints) || // Victor airways shown - show waypoints too
       (wp.hasTracks && trackWaypoints) || // Tracks shown - show waypoints too
       (flightplan && wp.routeIndex >= 0)) // Flightplan shown and waypoint is part of the route
    {
      if(wp.isVor() && wp.artificial != map::WAYPOINT_ARTIFICIAL_NONE)
      {
        // Get related VOR for artificial waypoint
        MapVor vor;
        getVorForWaypoint(vor, wp.id);
        if(vor.isValid())
          vors.insert(vor.id, vor);
      }
      else if(wp.isNdb() && wp.artificial != map::WAYPOINT_ARTIFICIAL_NONE)
      {
        // Get related NDB for artificial waypoint
        MapNdb ndb;
        getNdbForWaypoint(ndb, wp.id);
        if(ndb.isValid())
          ndbs.insert(ndb.id, ndb);
      }
      else
        // Normal waypoint - artificial should never occur to normal ones
        waypoints.insert(wp.id, wp);
    }
  }
}

map::MapResultIndex *MapQuery::getNearestNavaids(const Pos& pos, float distanceNm, map::MapTypes type, int maxIls, float maxIlsDistNm)
{
  map::MapResultIndex *nearest = nearestNavaidsInternal(pos, distanceNm, type, maxIls, maxIlsDistNm);
  if(nearest == nullptr || nearest->size() < 5)
    nearest = nearestNavaidsInternal(pos, distanceNm * 4.f, type, maxIls, maxIlsDistNm);
  return nearest;
}

map::MapResultIndex *MapQuery::nearestNavaidsInternal(const Pos& pos, float distanceNm, map::MapTypes type, int maxIls, float maxIlsDist)
{
  query::NearestCacheKeyNavaid key = {pos, distanceNm, type};

  map::MapResultIndex *result = nearestNavaidCache.object(key);

  if(result == nullptr)
  {
    map::MapResult res;

    // Create a rectangle that roughly covers the requested region
    atools::geo::Rect rect(pos, atools::geo::nmToMeter(distanceNm), true /* fast */);

    if(type & map::VOR)
    {
      query::fetchObjectsForRect(rect, vorsByRectQuery, [ =, &res](atools::sql::SqlQuery *query) -> void {
        MapVor obj;
        mapTypesFactory->fillVor(query->record(), obj);
        res.vors.append(obj);
      });
    }

    if(type & map::NDB)
    {
      query::fetchObjectsForRect(rect, ndbsByRectQuery, [ =, &res](atools::sql::SqlQuery *query) -> void {
        MapNdb obj;
        mapTypesFactory->fillNdb(query->record(), obj);
        res.ndbs.append(obj);
      });
    }

    if(type & map::WAYPOINT)
    {
      query::fetchObjectsForRect(rect, NavApp::getWaypointTrackQueryGui()->getWaypointsByRectQueryTrack(),
                                 [ =, &res](atools::sql::SqlQuery *query) -> void {
        MapWaypoint obj;
        mapTypesFactory->fillWaypoint(query->record(), obj, true /* track database */);
        res.waypoints.append(obj);
      });

      query::fetchObjectsForRect(rect, NavApp::getWaypointTrackQueryGui()->getWaypointsByRectQuery(),
                                 [ =, &res](atools::sql::SqlQuery *query) -> void {
        MapWaypoint obj;
        mapTypesFactory->fillWaypoint(query->record(), obj, false /* track database */);

        if(!res.waypoints.contains(obj))
          res.waypoints.append(obj);
      });
      maptools::removeDuplicatesById(res.waypoints);
    }

    if(type & map::ILS)
    {
      if(query::valid(Q_FUNC_INFO, ilsByRectQuery))
      {
        QList<MapIls> ilsRes;

        query::fetchObjectsForRect(rect, ilsByRectQuery, [ =, &ilsRes](atools::sql::SqlQuery *query) -> void {
          MapIls obj;
          mapTypesFactory->fillIls(query->record(), obj);
          ilsRes.append(obj);
        });
        maptools::removeByDistance(ilsRes, pos, atools::geo::nmToMeter(maxIlsDist));
        maptools::sortByDistance(ilsRes, pos);
        res.ils.append(ilsRes.mid(0, maxIls));
      }
    }

    result = new map::MapResultIndex;
    result->add(res);

    // Remove all that are too far away
    result->remove(pos, distanceNm);

    // Sort the rest by distance
    result->sort(pos);

    nearestNavaidCache.insert(key, result);
  }
  return result;
}

void MapQuery::getMapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident, const QString& region,
                                   const QString& airport, const Pos& sortByDistancePos, float maxDistanceMeter,
                                   bool airportFromNavDatabase, map::AirportQueryFlags flags) const
{
  mapObjectByIdentInternal(result, type, ident, region, airport, sortByDistancePos, maxDistanceMeter, airportFromNavDatabase, flags);
}

void MapQuery::getMapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident,
                                   const QString& region, const QString& airport, bool airportFromNavDatabase,
                                   map::AirportQueryFlags flags) const
{
  mapObjectByIdentInternal(result, type, ident, region, airport, EMPTY_POS, map::INVALID_DISTANCE_VALUE, airportFromNavDatabase, flags);
}

void MapQuery::mapObjectByIdentInternal(map::MapResult& result, map::MapTypes type, const QString& ident,
                                        const QString& region, const QString& airport, const Pos& sortByDistancePos,
                                        float maxDistanceMeter, bool airportFromNavDatabase, map::AirportQueryFlags flags) const
{
  if(type & map::AIRPORT)
  {
    AirportQuery *airportQuery = airportFromNavDatabase ? NavApp::getAirportQueryNav() : NavApp::getAirportQuerySim();

    // Try exact ident first =====================
    MapAirport ap = airportQuery->getAirportByIdent(ident);
    if(ap.isValid())
      result.airports.append(ap);

    // Check for truncated X-Plane idents but only in X-Plane database
    if(ident.size() == 6 && NavApp::isAirportDatabaseXPlane(airportFromNavDatabase))
      airportQuery->getAirportsByTruncatedIdent(result.airports, ident);

    // Remove airports too far away from given distance
    maptools::removeByDistance(result.airports, sortByDistancePos, maxDistanceMeter);

    if(result.airports.isEmpty())
    {
      // Try fuzzy search for nearest by official ids =====================
      // Look through all fields (ICAO, IATA, FAA and local) for the given ident
      QList<MapAirport> airports = airportQuery->getAirportsByOfficialIdent(ident, &sortByDistancePos, maxDistanceMeter, flags);
      result.airports.append(airports);
    }
  }

  if(type & map::AIRPORT_MSA && airportMsaByIdentQuery != nullptr)
  {
    airportMsaByIdentQuery->bindValue(":navident", ident);
    airportMsaByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    airportMsaByIdentQuery->bindValue(":airportident", airport.isEmpty() ? "%" : airport);
    airportMsaByIdentQuery->exec();
    while(airportMsaByIdentQuery->next())
    {
      MapAirportMsa msa;
      mapTypesFactory->fillAirportMsa(airportMsaByIdentQuery->record(), msa);
      result.airportMsa.append(msa);
    }
    maptools::sortByDistance(result.airportMsa, sortByDistancePos);
    maptools::removeByDistance(result.airportMsa, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::VOR && query::valid(Q_FUNC_INFO, vorByIdentQuery))
  {
    vorByIdentQuery->bindValue(":ident", ident);
    vorByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    vorByIdentQuery->exec();
    while(vorByIdentQuery->next())
    {
      MapVor vor;
      mapTypesFactory->fillVor(vorByIdentQuery->record(), vor);
      result.vors.append(vor);
    }
    maptools::sortByDistance(result.vors, sortByDistancePos);
    maptools::removeByDistance(result.vors, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::NDB && query::valid(Q_FUNC_INFO, ndbByIdentQuery))
  {
    ndbByIdentQuery->bindValue(":ident", ident);
    ndbByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    ndbByIdentQuery->exec();
    while(ndbByIdentQuery->next())
    {
      MapNdb ndb;
      mapTypesFactory->fillNdb(ndbByIdentQuery->record(), ndb);
      result.ndbs.append(ndb);
    }
    maptools::sortByDistance(result.ndbs, sortByDistancePos);
    maptools::removeByDistance(result.ndbs, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::WAYPOINT)
  {
    NavApp::getWaypointTrackQueryGui()->getWaypointByIdent(result.waypoints, ident, region);
    maptools::sortByDistance(result.waypoints, sortByDistancePos);
    maptools::removeByDistance(result.waypoints, sortByDistancePos, maxDistanceMeter);
  }

  if(type & map::ILS && query::valid(Q_FUNC_INFO, ilsByIdentQuery))
  {
    ilsByIdentQuery->bindValue(":ident", ident);
    ilsByIdentQuery->bindValue(":airport", airport);
    ilsByIdentQuery->exec();
    while(ilsByIdentQuery->next())
    {
      MapIls ils;
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
    NavApp::getAirwayTrackQueryGui()->getAirwaysByName(result.airways, ident);
}

void MapQuery::getMapObjectById(map::MapResult& result, map::MapTypes type, map::MapAirspaceSources src, int id,
                                bool airportFromNavDatabase) const
{
  if(type == map::AIRPORT)
  {
    MapAirport airport = (airportFromNavDatabase ?
                          NavApp::getAirportQueryNav() :
                          NavApp::getAirportQuerySim())->getAirportById(id);
    if(airport.isValid())
      result.airports.append(airport);
  }
  else if(type == map::AIRPORT_MSA)
  {
    MapAirportMsa msa = getAirportMsaById(id);
    if(msa.isValid())
      result.airportMsa.append(msa);
  }
  else if(type == map::VOR)
  {
    MapVor vor = getVorById(id);
    if(vor.isValid())
      result.vors.append(vor);
  }
  else if(type == map::NDB)
  {
    MapNdb ndb = getNdbById(id);
    if(ndb.isValid())
      result.ndbs.append(ndb);
  }
  else if(type == map::HOLDING)
  {
    MapHolding holding = getHoldingById(id);
    if(holding.isValid())
      result.holdings.append(holding);
  }
  else if(type == map::WAYPOINT)
  {
    MapWaypoint waypoint = NavApp::getWaypointTrackQueryGui()->getWaypointById(id);
    if(waypoint.isValid())
      result.waypoints.append(waypoint);
  }
  else if(type == map::USERPOINT)
  {
    MapUserpoint userPoint = NavApp::getUserdataController()->getUserpointById(id);
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
    MapIls ils = getIlsById(id);
    if(ils.isValid())
      result.ils.append(ils);
  }
  else if(type == map::RUNWAYEND)
  {
    map::MapRunwayEnd end = (airportFromNavDatabase ? NavApp::getAirportQueryNav() : NavApp::getAirportQuerySim())->getRunwayEndById(id);
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
    result.onlineAircraft.append(map::MapOnlineAircraft(NavApp::getOnlinedataController()->getClientAircraftById(id)));
  }
  else if(type == map::AIRWAY)
  {
    map::MapAirway airway = NavApp::getAirwayTrackQueryGui()->getAirwayById(id);
    if(airway.isValid())
      result.airways.append(airway);
  }
}

map::MapVor MapQuery::getVorById(int id) const
{
  MapVor vor;
  if(!query::valid(Q_FUNC_INFO, vorByIdQuery))
    return vor;

  vorByIdQuery->bindValue(":id", id);
  vorByIdQuery->exec();
  if(vorByIdQuery->next())
    mapTypesFactory->fillVor(vorByIdQuery->record(), vor);
  vorByIdQuery->finish();
  return vor;
}

map::MapNdb MapQuery::getNdbById(int id) const
{
  MapNdb ndb;
  if(!query::valid(Q_FUNC_INFO, ndbByIdQuery))
    return ndb;

  ndbByIdQuery->bindValue(":id", id);
  ndbByIdQuery->exec();
  if(ndbByIdQuery->next())
    mapTypesFactory->fillNdb(ndbByIdQuery->record(), ndb);
  ndbByIdQuery->finish();
  return ndb;
}

map::MapIls MapQuery::getIlsById(int id) const
{
  MapIls ils;

  if(!query::valid(Q_FUNC_INFO, ilsByIdQuery))
    return ils;

  ilsByIdQuery->bindValue(":id", id);
  ilsByIdQuery->exec();
  if(ilsByIdQuery->next())
    mapTypesFactory->fillIls(ilsByIdQuery->record(), ils);
  ilsByIdQuery->finish();
  return ils;
}

map::MapAirportMsa MapQuery::getAirportMsaById(int id) const
{
  MapAirportMsa msa;
  if(airportMsaByIdQuery != nullptr)
  {
    airportMsaByIdQuery->bindValue(":id", id);
    airportMsaByIdQuery->exec();
    if(airportMsaByIdQuery->next())
      mapTypesFactory->fillAirportMsa(airportMsaByIdQuery->record(), msa);
    airportMsaByIdQuery->finish();
  }
  return msa;
}

map::MapHolding MapQuery::getHoldingById(int id) const
{
  MapHolding holding;
  if(holdingByIdQuery != nullptr)
  {
    holdingByIdQuery->bindValue(":id", id);
    holdingByIdQuery->exec();
    if(holdingByIdQuery->next())
      mapTypesFactory->fillHolding(holdingByIdQuery->record(), holding);
    holdingByIdQuery->finish();
  }
  return holding;
}

const QVector<map::MapIls> MapQuery::getIlsByAirportAndRunway(const QString& airportIdent, const QString& runway) const
{
  QVector<MapIls> ils;
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(runway))
  {
    ils = ilsByAirportAndRunway(airportIdent, rname);
    if(!ils.isEmpty())
      break;
  }
  return ils;
}

QVector<MapIls> MapQuery::getIlsByAirportAndIdent(const QString& airportIdent, const QString& ilsIdent) const
{
  QVector<MapIls> ilsList;

  if(!query::valid(Q_FUNC_INFO, ilsQuerySimByAirportAndIdent))
    return ilsList;

  ilsQuerySimByAirportAndIdent->bindValue(":apt", airportIdent);
  ilsQuerySimByAirportAndIdent->bindValue(":ident", ilsIdent);
  ilsQuerySimByAirportAndIdent->exec();
  while(ilsQuerySimByAirportAndIdent->next())
  {
    MapIls ils;
    mapTypesFactory->fillIls(ilsQuerySimByAirportAndIdent->record(), ils);
    ilsList.append(ils);
  }
  return ilsList;
}

QVector<map::MapIls> MapQuery::ilsByAirportAndRunway(const QString& airportIdent, const QString& runway) const
{
  QVector<MapIls> ilsList;
  if(!query::valid(Q_FUNC_INFO, ilsQuerySimByAirportAndRw))
    return ilsList;

  ilsQuerySimByAirportAndRw->bindValue(":apt", airportIdent);
  ilsQuerySimByAirportAndRw->bindValue(":rwy", runway);
  ilsQuerySimByAirportAndRw->exec();
  while(ilsQuerySimByAirportAndRw->next())
  {
    MapIls ils;
    mapTypesFactory->fillIls(ilsQuerySimByAirportAndRw->record(), ils);
    ilsList.append(ils);
  }
  return ilsList;
}

void MapQuery::getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, const QSet<int>& shownDetailAirportIds,
                                       bool airportDiagram, map::MapTypes types, map::MapDisplayTypes displayTypes,
                                       int xs, int ys, int screenDistance, map::MapResult& result) const
{
  using maptools::insertSortedByDistance;
  using maptools::insertSortedByTowerDistance;

  int x, y;
  if(mapLayer->isAirport() && types.testFlag(map::AIRPORT))
  {
    int minRunwayLength = NavApp::getMapAirportHandler()->getMinimumRunwayFt(); // GUI setting
    for(int i = airportCache.list.size() - 1; i >= 0; i--)
    {
      const MapAirport& airport = airportCache.list.at(i);

      if(airport.isVisible(types, minRunwayLength, mapLayer))
      {
        if(conv.wToS(airport.position, x, y))
          if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, airport);

        if(airportDiagram)
        {
          // Include tower for airport diagrams
          if(conv.wToS(airport.towerCoords, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByTowerDistance(conv, result.towers, xs, ys, airport);
        }
      }
    }
  }

  if(mapLayer->isAirportMsa() && types.testFlag(map::AIRPORT_MSA))
  {
    for(int i = airportMsaCache.list.size() - 1; i >= 0; i--)
    {
      const MapAirportMsa& msa = airportMsaCache.list.at(i);
      if(conv.wToS(msa.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
          insertSortedByDistance(conv, result.airportMsa, &result.airportMsaIds, xs, ys, msa);
    }
  }

  if(mapLayer->isVor() && types.testFlag(map::VOR))
  {
    for(int i = vorCache.list.size() - 1; i >= 0; i--)
    {
      const MapVor& vor = vorCache.list.at(i);
      if(conv.wToS(vor.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
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

  if(mapLayer->isHolding() && types.testFlag(map::HOLDING))
  {
    for(int i = holdingCache.list.size() - 1; i >= 0; i--)
    {
      const MapHolding& holding = holdingCache.list.at(i);
      if(conv.wToS(holding.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
          insertSortedByDistance(conv, result.holdings, &result.holdingIds, xs, ys, holding);
    }
  }

  // No flag since visibility is defined by type
  if(mapLayer->isUserpoint())
  {
    for(int i = userpointCache.list.size() - 1; i >= 0; i--)
    {
      const MapUserpoint& wp = userpointCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
          insertSortedByDistance(conv, result.userpoints, &result.userpointIds, xs, ys, wp);
    }
  }

  // Add waypoints that displayed together with airways =================================
  bool victorWaypoints = mapLayer->isAirwayWaypoint() && types.testFlag(map::AIRWAYV);
  bool jetWaypoints = mapLayer->isAirwayWaypoint() && types.testFlag(map::AIRWAYJ);
  bool trackWaypoints = mapLayer->isTrackWaypoint() && types.testFlag(map::TRACK);
  bool normalWaypoints = mapLayer->isWaypoint() && types.testFlag(map::WAYPOINT);
  bool flightplan = displayTypes.testFlag(map::FLIGHTPLAN);

  if(victorWaypoints || jetWaypoints || trackWaypoints || normalWaypoints || flightplan)
  {
    // Get all close waypoints
    NavApp::getWaypointTrackQueryGui()->getNearestScreenObjects(conv, mapLayer, types, xs, ys, screenDistance, result);

    // Filter waypoints by airway/track type and remove artificial ones
    QHash<int, MapWaypoint> waypoints;
    QHash<int, MapVor> vors;
    QHash<int, MapNdb> ndbs;
    resolveWaypointNavaids(result.waypoints, waypoints, vors, ndbs,
                           flightplan, normalWaypoints, victorWaypoints, jetWaypoints, trackWaypoints);

    // Add filtered waypoints back to list
    result.waypoints.clear();
    result.waypointIds.clear();
    for(const map::MapWaypoint& wp : qAsConst(waypoints))
    {
      if(wp.artificial == map::WAYPOINT_ARTIFICIAL_NONE)
        insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, wp);
    }

    // Add VOR fetched for artificial waypoints
    for(const map::MapVor& vor : qAsConst(vors))
      insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, vor);

    // Add NDB fetched for artificial waypoints
    for(const map::MapNdb& ndb : qAsConst(ndbs))
      insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, ndb);
  }

  if(mapLayer->isMarker() && types.testFlag(map::MARKER))
  {
    for(int i = markerCache.list.size() - 1; i >= 0; i--)
    {
      const MapMarker& wp = markerCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
          insertSortedByDistance(conv, result.markers, nullptr, xs, ys, wp);
    }
  }

  if(mapLayer->isIls() && types.testFlag(map::ILS))
  {
    for(int i = ilsCache.list.size() - 1; i >= 0; i--)
    {
      const MapIls& wp = ilsCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if(atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
          insertSortedByDistance(conv, result.ils, nullptr, xs, ys, wp);
    }
  }

  // Get objects from airport diagram =====================================================
  if(mapLayer->isAirport() && airportDiagram)
  {
    // Also check parking and helipads in airport diagrams
    QHash<int, QList<MapParking> > parkingCache = NavApp::getAirportQuerySim()->getParkingCache();
    for(auto it = parkingCache.constBegin(); it != parkingCache.constEnd(); ++it)
    {
      // Only draw if airport is actually drawn on map
      if(shownDetailAirportIds.contains(it.key()))
      {
        for(const MapParking& parking : it.value())
        {
          if(conv.wToS(parking.position, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByDistance(conv, result.parkings, nullptr, xs, ys, parking);
        }
      }
    }

    QHash<int, QList<MapHelipad> > helipadCache = NavApp::getAirportQuerySim()->getHelipadCache();
    for(auto it = helipadCache.constBegin(); it != helipadCache.constEnd(); ++it)
    {
      if(shownDetailAirportIds.contains(it.key()))
      {
        for(const MapHelipad& helipad : it.value())
        {
          if(conv.wToS(helipad.position, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < screenDistance)
            insertSortedByDistance(conv, result.helipads, nullptr, xs, ys, helipad);
        }
      }
    }
  }
}

const QList<map::MapAirport> *MapQuery::getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                                    map::MapTypes types, bool& overflow)
{
  // Get flags for running separate queries for add-on and normal airports
  bool addon = types.testFlag(map::AIRPORT_ADDON);
  bool normal = types & map::AIRPORT_ALL;

  airportCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                           [this, addon, normal](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirport(newLayer) &&
    // Invalidate cache if settings differ
    airportCacheAddonFlag == addon && airportCacheNormalFlag == normal;
  });

  airportCacheAddonFlag = addon;
  airportCacheNormalFlag = normal;

  airportByRectQuery->bindValue(":minlength", mapLayer->getMinRunwayLength());
  return fetchAirports(rect, airportByRectQuery, lazy, false /* overview */, addon, normal, overflow);
}

const QList<map::MapAirport> *MapQuery::getAirportsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy,
                                                          map::MapTypes types, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, airportByRectQuery))
    return nullptr;

  const GeoDataLatLonBox latLonBox = GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest());

  // Get flags for running separate queries for add-on and normal airports
  bool addon = types.testFlag(map::AIRPORT_ADDON);
  bool normal = types & map::AIRPORT_ALL;

  airportCacheAddonFlag = addon;
  airportCacheNormalFlag = normal;

  airportByRectQuery->bindValue(":minlength", mapLayer->getMinRunwayLength());
  return fetchAirports(latLonBox, airportByRectQuery, lazy, false /* overview */, addon, normal, overflow);
}

const QList<map::MapVor> *MapQuery::getVors(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                            bool lazy, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, vorsByRectQuery))
    return nullptr;

  vorCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                       [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersVor(newLayer);
  });

  if(vorCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, vorsByRectQuery);
      vorsByRectQuery->exec();
      while(vorsByRectQuery->next())
      {
        MapVor vor;
        mapTypesFactory->fillVor(vorsByRectQuery->record(), vor);
        vorCache.list.append(vor);
      }
    }
  }
  overflow = vorCache.validate(queryMaxRows);
  return &vorCache.list;
}

const QList<map::MapVor> *MapQuery::getVorsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  const GeoDataLatLonBox latLonBox = GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(),
                                                      rect.getWest(), GeoDataCoordinates::Degree);
  return getVors(latLonBox, mapLayer, lazy, overflow);
}

const QList<map::MapNdb> *MapQuery::getNdbs(const GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, ndbsByRectQuery))
    return nullptr;

  ndbCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                       [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersNdb(newLayer);
  });

  if(ndbCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, ndbsByRectQuery);
      ndbsByRectQuery->exec();
      while(ndbsByRectQuery->next())
      {
        MapNdb ndb;
        mapTypesFactory->fillNdb(ndbsByRectQuery->record(), ndb);
        ndbCache.list.append(ndb);
      }
    }
  }
  overflow = ndbCache.validate(queryMaxRows);
  return &ndbCache.list;
}

const QList<map::MapNdb> *MapQuery::getNdbsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  const GeoDataLatLonBox latLonBox = GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(),
                                                      rect.getWest(), GeoDataCoordinates::Degree);
  return getNdbs(latLonBox, mapLayer, lazy, overflow);
}

const QList<map::MapUserpoint> MapQuery::getUserdataPoints(const GeoDataLatLonBox& rect, const QStringList& types,
                                                           const QStringList& typesAll, bool unknownType, float distanceNm)
{
  QList<MapUserpoint> retval;

  if(!query::valid(Q_FUNC_INFO, userdataPointByRectQuery))
    return retval;

  // No caching here since points can change and the dataset is usually small
  userpointCache.clear();

  // Display either unknown or any type
  if(unknownType || !types.isEmpty())
  {
    bool allTypesSelected = types == typesAll;

    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, userdataPointByRectQuery);
      userdataPointByRectQuery->bindValue(":dist", distanceNm);

      QStringList queryTypes;
      if(unknownType || (allTypesSelected && unknownType))
        // Either query all unknows too and filter later or all and unknown are selected
        queryTypes.append("%");
      else
        queryTypes = types;

      for(const QString& queryType : qAsConst(queryTypes))
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

          MapUserpoint userPoint;
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

QString MapQuery::getAirportIdentFromWaypoint(const QString& ident, const QString& region, const Pos& pos, bool found) const
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_WAYPOINT, ident, region, pos, found);
}

QString MapQuery::getAirportIdentFromVor(const QString& ident, const QString& region, const Pos& pos, bool found) const
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_VOR, ident, region, pos, found);
}

QString MapQuery::getAirportIdentFromNdb(const QString& ident, const QString& region, const Pos& pos, bool found) const
{
  return airportIdentFromQuery(AIRPORTIDENT_FROM_NDB, ident, region, pos, found);
}

QString MapQuery::airportIdentFromQuery(const QString& queryStr, const QString& ident, const QString& region, const Pos& pos,
                                        bool& found) const
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

const QList<map::MapMarker> *MapQuery::getMarkers(const GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, markersByRectQuery))
    return nullptr;

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

const QList<map::MapMarker> *MapQuery::getMarkersByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer,
                                                        bool lazy, bool& overflow)
{
  const GeoDataLatLonBox latLonBox = GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(),
                                                      rect.getWest(), GeoDataCoordinates::Degree);
  return getMarkers(latLonBox, mapLayer, lazy, overflow);
}

const QList<map::MapHolding> *MapQuery::getHoldings(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                    bool lazy, bool& overflow)
{
  if(holdingByRectQuery != nullptr)
  {
    holdingCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                             [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
    {
      return curLayer->hasSameQueryParametersHolding(newLayer);
    });

    if(holdingCache.list.isEmpty() && !lazy)
    {
      for(const GeoDataLatLonBox& r :
          query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
      {
        query::bindRect(r, holdingByRectQuery);
        holdingByRectQuery->exec();
        while(holdingByRectQuery->next())
        {
          MapHolding holding;
          mapTypesFactory->fillHolding(holdingByRectQuery->record(), holding);
          holdingCache.list.append(holding);
        }
      }
    }
    overflow = holdingCache.validate(queryMaxRows);
    return &holdingCache.list;
  }
  return nullptr;
}

const QList<map::MapAirportMsa> *MapQuery::getAirportMsa(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                                         bool& overflow)
{
  if(airportMsaByRectQuery != nullptr)
  {
    airportMsaCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                                [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
    {
      return curLayer->hasSameQueryParametersAirportMsa(newLayer);
    });

    if(airportMsaCache.list.isEmpty() && !lazy)
    {
      for(const GeoDataLatLonBox& r :
          query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
      {
        query::bindRect(r, airportMsaByRectQuery);

        airportMsaByRectQuery->exec();
        while(airportMsaByRectQuery->next())
        {
          MapAirportMsa msa;
          mapTypesFactory->fillAirportMsa(airportMsaByRectQuery->record(), msa);
          airportMsaCache.list.append(msa);
        }
      }
    }
    overflow = airportMsaCache.validate(queryMaxRows);
    return &airportMsaCache.list;
  }
  return nullptr;
}

const QList<map::MapIls> *MapQuery::getIls(GeoDataLatLonBox rect, const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, ilsByRectQuery))
    return nullptr;

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
    rect.setBoundaries(rect.north() + increase, rect.south() - increase, rect.east() + increase, rect.west() - increase);

    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, ilsByRectQuery);

      ilsByRectQuery->exec();
      while(ilsByRectQuery->next())
      {
        // ILS is always loaded from nav except if all is off
        map::MapRunwayEnd end;
        if(mapLayer->isIlsDetail() && !NavApp::isNavdataOff())
          // Get the runway end to fix graphical alignment issues in map
          end = NavApp::getAirportQueryNav()->getRunwayEndById(ilsByRectQuery->valueInt("loc_runway_end_id"));

        MapIls ils;
        mapTypesFactory->fillIls(ilsByRectQuery->record(), ils, end.isFullyValid() ? end.heading : map::INVALID_HEADING_VALUE);
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
const QList<map::MapAirport> *MapQuery::fetchAirports(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                                                      bool lazy, bool overview, bool addon, bool normal, bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, query))
    return nullptr;

  AirportQuery *airportQueryNav = NavApp::getAirportQueryNav();

  if(airportCache.list.isEmpty() && !lazy)
  {
    bool navdata = NavApp::isNavdataAll();

    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
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
          MapAirport airport;
          if(overview)
            // Fill only a part of the object
            mapTypesFactory->fillAirportForOverview(query->record(), airport, navdata, NavApp::isAirportDatabaseXPlane(navdata));
          else
            mapTypesFactory->fillAirport(query->record(), airport, true /* complete */, navdata, NavApp::isAirportDatabaseXPlane(navdata));

          // Need to update airport procedure flag for mixed mode databases to enable procedure filter on map
          airportQueryNav->correctAirportProcedureFlag(airport);

          ids.insert(airport.id);
          airportCache.list.append(airport);
        }
      }

      // Get add-on airports ==========
      if(addon && airportAddonByRectQuery != nullptr)
      {
        query::bindRect(r, airportAddonByRectQuery);
        airportAddonByRectQuery->exec();
        while(airportAddonByRectQuery->next())
        {
          MapAirport airport;
          if(overview)
            // Fill only a part of the object
            mapTypesFactory->fillAirportForOverview(airportAddonByRectQuery->record(), airport, navdata,
                                                    NavApp::isAirportDatabaseXPlane(navdata));
          else
            mapTypesFactory->fillAirport(airportAddonByRectQuery->record(), airport, true /* complete */, navdata,
                                         NavApp::isAirportDatabaseXPlane(navdata));

          // Need to update airport procedure flag for mixed mode databases to enable procedure filter on map
          airportQueryNav->correctAirportProcedureFlag(airport);

          if(!ids.contains(airport.id))
            airportCache.list.append(airport);
        }
      }
    }
  }
  overflow = airportCache.validate(queryMaxRows);
  return &airportCache.list;
}

const QList<map::MapRunway> *MapQuery::getRunwaysForOverview(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, runwayOverviewQuery))
    return nullptr;

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
      mapTypesFactory->fillRunway(runwayOverviewQuery->record(), runway, true /* overview */);
      rws->append(runway);
    }
    runwayOverwiewCache.insert(airportId, rws);
    return rws;
  }
}

/* Get runway end and try lower and higher numbers if nothing was found - adds a dummy entry with airport
 * position if no runway ends were found */
void MapQuery::getRunwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                                       const map::MapAirport& airport, bool navData) const
{
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(name))
  {
    runwayEndByNameFuzzy(runwayEnds, rname, airport, navData);
    if(!runwayEnds.isEmpty())
      return;
  }
}

void MapQuery::runwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                                    const map::MapAirport& airport, bool navData) const
{
  AirportQuery *aquery = navData ? NavApp::getAirportQueryNav() : NavApp::getAirportQuerySim();
  map::MapResult result;

  if(!name.isEmpty())
  {
    QString bestRunway = atools::fs::util::runwayBestFit(name, aquery->getRunwayNames(airport.id));

    if(!bestRunway.isEmpty())
      getMapObjectByIdent(result, map::RUNWAYEND, bestRunway, QString(), airport.ident, navData /* airport or runway from nav database */);
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
  else if(result.runwayEnds.constFirst().name != name)
    qWarning() << "Found runway" << result.runwayEnds.constFirst().name
               << "as replacement for" << name << "airport" << airport.ident;
#endif

  runwayEnds = result.runwayEnds;
}

void MapQuery::initQueries()
{
  // Common where clauses
  static const QLatin1String whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static const QLatin1String whereIdentRegion("ident = :ident and region like :region");
  static const QString whereLimit("limit " + QString::number(queryMaxRows));

  // Common select statements
  QStringList const airportQueryBase = AirportQuery::airportColumns(dbSim);
  QStringList const airportQueryBaseOverview = AirportQuery::airportOverviewColumns(dbSim);

  static const QLatin1String vorQueryBase("vor_id, ident, name, region, type, name, frequency, channel, range, dme_only, dme_altitude, "
                                          "mag_var, altitude, lonx, laty ");
  static const QLatin1String ndbQueryBase("ndb_id, ident, name, region, type, name, frequency, range, mag_var, altitude, lonx, laty ");

  QString ilsQueryBase("ils_id, ident, name, region, mag_var, loc_heading, has_backcourse, loc_runway_end_id, loc_airport_ident, "
                       "loc_runway_name, gs_pitch, frequency, range, dme_range, loc_width, "
                       "end1_lonx, end1_laty, end_mid_lonx, end_mid_laty, end2_lonx, end2_laty, altitude, lonx, laty");

  static const QLatin1String holdingQueryBase("holding_id, airport_ident, nav_ident, nav_type, vor_type, vor_dme_only, vor_has_dme, "
                                              "name, mag_var, course, turn_direction, leg_length, leg_time, "
                                              "minimum_altitude, maximum_altitude, speed_limit, lonx, laty");

  static const QLatin1String msaQueryBase("airport_msa_id, airport_ident, nav_ident, nav_type, vor_type, "
                                          "vor_dme_only, vor_has_dme, region, multiple_code, true_bearing, mag_var, "
                                          "left_lonx, top_laty, right_lonx, bottom_laty, radius, lonx, laty, geometry ");

  SqlUtil navUtil(dbNav);
  QString extraIlsCols = navUtil.buildColumnListIf("ils", {"type", "perf_indicator", "provider"}).join(", ");
  if(!extraIlsCols.isEmpty())
    ilsQueryBase.append(", " + extraIlsCols);

  deInitQueries();

  // Check for GLS ground station or GBAS threshold
  gls = navUtil.hasTableAndColumn("ils", "type") && navUtil.hasRows("ils", "type in ('G', 'T')");

  // Check for holding table in nav (Navigraph) database and then in simulator database (X-Plane only)
  // Reverse search order depending on scenery library settings
  SqlDatabase *holdingDb = NavApp::isNavdataOff() ?
                           SqlUtil::getDbWithTableAndRows("holding", {dbSim, dbNav}) :
                           SqlUtil::getDbWithTableAndRows("holding", {dbNav, dbSim});

  // Same as above for airport MSA table
  SqlDatabase *msaDb = NavApp::isNavdataOff() ?
                       SqlUtil::getDbWithTableAndRows("airport_msa", {dbSim, dbNav}) :
                       SqlUtil::getDbWithTableAndRows("airport_msa", {dbNav, dbSim});

  vorByIdentQuery = new SqlQuery(dbNav);
  vorByIdentQuery->prepare("select " + vorQueryBase + " from vor where " + whereIdentRegion);

  ndbByIdentQuery = new SqlQuery(dbNav);
  ndbByIdentQuery->prepare("select " + ndbQueryBase + " from ndb where " + whereIdentRegion);

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

  if(holdingDb != nullptr)
  {
    holdingByIdQuery = new SqlQuery(holdingDb);
    holdingByIdQuery->prepare("select " + holdingQueryBase + " from holding where holding_id = :id");
  }

  ilsByIdQuery = new SqlQuery(dbNav);
  ilsByIdQuery->prepare("select " + ilsQueryBase + " from ils where ils_id = :id");

  ilsByIdentQuery = new SqlQuery(dbNav);
  ilsByIdentQuery->prepare("select " + ilsQueryBase + " from ils where ident = :ident and loc_airport_ident = :airport");

  ilsQuerySimByAirportAndRw = new SqlQuery(dbNav);
  ilsQuerySimByAirportAndRw->prepare("select " + ilsQueryBase + " from ils where loc_airport_ident = :apt and loc_runway_name = :rwy");

  ilsQuerySimByAirportAndIdent = new SqlQuery(dbNav);
  ilsQuerySimByAirportAndIdent->prepare("select " + ilsQueryBase + " from ils where loc_airport_ident = :apt and ident = :ident");

  ilsByRectQuery = new SqlQuery(dbNav);
  ilsByRectQuery->prepare("select " + ilsQueryBase + " from ils where " + whereRect + " " + whereLimit);

  airportByRectQuery = new SqlQuery(dbSim);
  airportByRectQuery->prepare("select " + airportQueryBase.join(", ") + " from airport where " + whereRect +
                              " and longest_runway_length >= :minlength " + whereLimit);

  airportAddonByRectQuery = new SqlQuery(dbSim);
  airportAddonByRectQuery->prepare(
    "select " + airportQueryBase.join(", ") + " from airport where " + whereRect + " and is_addon = 1 " + whereLimit);

  // Runways > 4000 feet for simplyfied runway overview
  runwayOverviewQuery = new SqlQuery(dbSim);
  runwayOverviewQuery->prepare("select runway_id, length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
                               "from runway where airport_id = :airportId and length > 4000 " + whereLimit);

  vorsByRectQuery = new SqlQuery(dbNav);
  vorsByRectQuery->prepare("select " + vorQueryBase + " from vor where " + whereRect + " " + whereLimit);

  ndbsByRectQuery = new SqlQuery(dbNav);
  ndbsByRectQuery->prepare("select " + ndbQueryBase + " from ndb where " + whereRect + " " + whereLimit);

  if(msaDb != nullptr)
  {
    airportMsaByRectQuery = new SqlQuery(msaDb);
    airportMsaByRectQuery->prepare("select " + msaQueryBase + " from airport_msa where " + whereRect + " " + whereLimit);

    airportMsaByIdentQuery = new SqlQuery(msaDb);
    airportMsaByIdentQuery->prepare("select " + msaQueryBase + " from airport_msa " +
                                    "where airport_ident like :airportident and nav_ident like :navident and region like :region");

    airportMsaByIdQuery = new SqlQuery(msaDb);
    airportMsaByIdQuery->prepare("select " + msaQueryBase + " from airport_msa where airport_msa_id = :id");
  }

  userdataPointByRectQuery = new SqlQuery(dbUser);
  userdataPointByRectQuery->prepare("select * from userdata "
                                    "where " + whereRect + " and visible_from > :dist and type like :type " +
                                    whereLimit);

  markersByRectQuery = new SqlQuery(dbSim);
  markersByRectQuery->prepare(
    "select marker_id, type, ident, heading, lonx, laty "
    "from marker "
    "where " + whereRect + " " + whereLimit);

  if(holdingDb != nullptr)
  {
    holdingByRectQuery = new SqlQuery(holdingDb);
    holdingByRectQuery->prepare("select " + holdingQueryBase + " from holding where " + whereRect + " " + whereLimit);
  }
}

void MapQuery::deInitQueries()
{
  airportCache.clear();
  airportMsaCache.clear();
  vorCache.clear();
  ndbCache.clear();
  markerCache.clear();
  holdingCache.clear();
  ilsCache.clear();
  runwayOverwiewCache.clear();

  ATOOLS_DELETE(airportByRectQuery);
  ATOOLS_DELETE(airportAddonByRectQuery);
  ATOOLS_DELETE(runwayOverviewQuery);
  ATOOLS_DELETE(vorsByRectQuery);
  ATOOLS_DELETE(ndbsByRectQuery);
  ATOOLS_DELETE(markersByRectQuery);
  ATOOLS_DELETE(holdingByRectQuery);
  ATOOLS_DELETE(userdataPointByRectQuery);
  ATOOLS_DELETE(vorByIdentQuery);
  ATOOLS_DELETE(ndbByIdentQuery);
  ATOOLS_DELETE(vorByIdQuery);
  ATOOLS_DELETE(ndbByIdQuery);
  ATOOLS_DELETE(vorByWaypointIdQuery);
  ATOOLS_DELETE(ndbByWaypointIdQuery);
  ATOOLS_DELETE(vorNearestQuery);
  ATOOLS_DELETE(ndbNearestQuery);
  ATOOLS_DELETE(ilsByRectQuery);
  ATOOLS_DELETE(ilsByIdentQuery);
  ATOOLS_DELETE(ilsByIdQuery);
  ATOOLS_DELETE(ilsQuerySimByAirportAndRw);
  ATOOLS_DELETE(ilsQuerySimByAirportAndIdent);
  ATOOLS_DELETE(holdingByIdQuery);
  ATOOLS_DELETE(airportMsaByIdentQuery);
  ATOOLS_DELETE(airportMsaByRectQuery);
  ATOOLS_DELETE(airportMsaByIdQuery);
}
