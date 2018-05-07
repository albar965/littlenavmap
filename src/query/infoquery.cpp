/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "query/infoquery.h"

#include "sql/sqldatabase.h"
#include "settings/settings.h"
#include "common/constants.h"

using atools::sql::SqlQuery;
using atools::sql::SqlDatabase;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;

InfoQuery::InfoQuery(SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav)
  : db(sqlDb), dbNav(sqlDbNav)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  airportCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportCache", 100).toInt());
  vorCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "VorCache", 100).toInt());
  ndbCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "NdbCache", 100).toInt());
  waypointCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "WaypointCache", 100).toInt());
  airwayCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirwayCache", 100).toInt());
  runwayEndCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "RunwayEndCache", 100).toInt());
  ilsCacheSim.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "IlsCache", 100).toInt());
  ilsCacheNav.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "IlsCache", 100).toInt());
  ilsCacheSimByName.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "IlsCache", 100).toInt());
  comCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "ComCache", 100).toInt());
  runwayCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "RunwayCache", 100).toInt());
  helipadCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "HelipadCache", 100).toInt());
  startCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "StartCache", 100).toInt());
  approachCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "ApproachCache", 100).toInt());
  transitionCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "TransitionCache", 100).toInt());
  airspaceCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirspaceCache", 100).toInt());
  airportSceneryCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportSceneryCache",
                                                           100).toInt());
}

InfoQuery::~InfoQuery()
{
  deInitQueries();
}

const SqlRecord *InfoQuery::getAirportInformation(int airportId)
{
  return cachedRecord(airportCache, airportQuery, airportId);
}

const atools::sql::SqlRecordVector *InfoQuery::getAirportSceneryInformation(const QString& ident)
{
  return cachedRecordVector(airportSceneryCache, airportSceneryQuery, ident);
}

const SqlRecordVector *InfoQuery::getComInformation(int airportId)
{
  return cachedRecordVector(comCache, comQuery, airportId);
}

const SqlRecordVector *InfoQuery::getApproachInformation(int airportId)
{
  return cachedRecordVector(approachCache, approachQuery, airportId);
}

const SqlRecordVector *InfoQuery::getTransitionInformation(int approachId)
{
  return cachedRecordVector(transitionCache, transitionQuery, approachId);
}

const SqlRecordVector *InfoQuery::getRunwayInformation(int airportId)
{
  return cachedRecordVector(runwayCache, runwayQuery, airportId);
}

const SqlRecordVector *InfoQuery::getHelipadInformation(int airportId)
{
  return cachedRecordVector(helipadCache, helipadQuery, airportId);
}

const SqlRecordVector *InfoQuery::getStartInformation(int airportId)
{
  return cachedRecordVector(startCache, startQuery, airportId);
}

const atools::sql::SqlRecord *InfoQuery::getRunwayEndInformation(int runwayEndId)
{
  return cachedRecord(runwayEndCache, runwayEndQuery, runwayEndId);
}

const atools::sql::SqlRecord *InfoQuery::getIlsInformationSim(int runwayEndId)
{
  return cachedRecord(ilsCacheSim, ilsQuerySim, runwayEndId);
}

const atools::sql::SqlRecord *InfoQuery::getIlsInformationNav(int runwayEndId)
{
  return cachedRecord(ilsCacheNav, ilsQueryNav, runwayEndId);
}

const atools::sql::SqlRecordVector *InfoQuery::getIlsInformationSimByName(const QString& airportIdent,
                                                                          const QString& runway)
{
  std::pair<QString, QString> key = std::make_pair(airportIdent, runway);
  atools::sql::SqlRecordVector *rec = ilsCacheSimByName.object(key);

  if(rec == nullptr)
  {
    ilsQuerySimByName->bindValue(":apt", airportIdent);
    ilsQuerySimByName->bindValue(":rwy", runway);
    ilsQuerySimByName->exec();

    rec = new atools::sql::SqlRecordVector;
    while(ilsQuerySimByName->next())
      rec->append(ilsQuerySimByName->record());

    ilsCacheSimByName.insert(key, rec);
  }
  return rec;
}

const atools::sql::SqlRecord *InfoQuery::getVorInformation(int vorId)
{
  return cachedRecord(vorCache, vorQuery, vorId);
}

const atools::sql::SqlRecord InfoQuery::getVorByIdentAndRegion(const QString& ident, const QString& region)
{
  vorIdentRegionQuery->bindValue(":ident", ident);
  vorIdentRegionQuery->bindValue(":region", region);
  vorIdentRegionQuery->exec();

  if(vorIdentRegionQuery->next())
    return vorIdentRegionQuery->record();
  else
    return atools::sql::SqlRecord();
}

const atools::sql::SqlRecord *InfoQuery::getNdbInformation(int ndbId)
{
  return cachedRecord(ndbCache, ndbQuery, ndbId);
}

const atools::sql::SqlRecord *InfoQuery::getAirspaceInformation(int airspaceId)
{
  return cachedRecord(airspaceCache, airspaceQuery, airspaceId);
}

const atools::sql::SqlRecord *InfoQuery::getWaypointInformation(int waypointId)
{
  return cachedRecord(waypointCache, waypointQuery, waypointId);
}

const atools::sql::SqlRecord *InfoQuery::getAirwayInformation(int airwayId)
{
  return cachedRecord(airwayCache, airwayQuery, airwayId);
}

atools::sql::SqlRecordVector InfoQuery::getAirwayWaypointInformation(const QString& name, int fragment)
{
  airwayWaypointQuery->bindValue(":name", name);
  airwayWaypointQuery->bindValue(":fragment", fragment);
  airwayWaypointQuery->exec();

  SqlRecordVector rec;
  while(airwayWaypointQuery->next())
    rec.append(airwayWaypointQuery->record());
  return rec;
}

/* Get a record from the cache of get it from a database query */
template<typename ID>
const SqlRecord *InfoQuery::cachedRecord(QCache<ID, SqlRecord>& cache, SqlQuery *query, ID id)
{
  SqlRecord *rec = cache.object(id);
  if(rec != nullptr)
  {
    // Found record in cache
    if(rec->isEmpty())
      // Empty record that indicates that no result was found
      return nullptr;
    else
      return rec;
  }
  else
  {
    query->bindValue(":id", id);
    query->exec();
    if(query->next())
    {
      // Insert it into the cache
      rec = new SqlRecord(query->record());
      cache.insert(id, rec);
    }
    else
      // Add empty record to indicate nothing found for this id
      cache.insert(id, new SqlRecord());
  }
  query->finish();
  return rec;
}

/* Get a record vector from the cache of get it from a database query */
template<typename ID>
const SqlRecordVector *InfoQuery::cachedRecordVector(QCache<ID, SqlRecordVector>& cache, SqlQuery *query, ID id)
{
  SqlRecordVector *rec = cache.object(id);
  if(rec != nullptr)
  {
    // Found record in cache
    if(rec->isEmpty())
      // Empty record that indicates that no result was found
      return nullptr;
    else
      return rec;
  }
  else
  {
    query->bindValue(":id", id);
    query->exec();

    rec = new SqlRecordVector;

    while(query->next())
      rec->append(query->record());

    // Insert it into the cache
    cache.insert(id, rec);

    if(rec->isEmpty())
      return nullptr;
    else
      return rec;
  }
  // Keep this here although it is never executed since some compilers throw an error
  return nullptr;
}

void InfoQuery::initQueries()
{
  deInitQueries();

  // TODO limit number of columns - remove star query
  airportQuery = new SqlQuery(db);
  airportQuery->prepare("select * from airport "
                        "join bgl_file on airport.file_id = bgl_file.bgl_file_id "
                        "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                        "where airport_id = :id");

  airportSceneryQuery = new SqlQuery(db);
  airportSceneryQuery->prepare("select * from airport_file f "
                               "join bgl_file b on f.file_id = b.bgl_file_id  "
                               "join scenery_area s on b.scenery_area_id = s.scenery_area_id "
                               "where f.ident = :id order by f.airport_file_id");

  comQuery = new SqlQuery(db);
  comQuery->prepare("select * from com where airport_id = :id order by type, frequency");

  vorQuery = new SqlQuery(dbNav);
  vorQuery->prepare("select * from vor "
                    "join bgl_file on vor.file_id = bgl_file.bgl_file_id "
                    "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                    "where vor_id = :id");

  ndbQuery = new SqlQuery(dbNav);
  ndbQuery->prepare("select * from ndb "
                    "join bgl_file on ndb.file_id = bgl_file.bgl_file_id "
                    "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                    "where ndb_id = :id");

  waypointQuery = new SqlQuery(dbNav);
  waypointQuery->prepare("select * from waypoint "
                         "join bgl_file on waypoint.file_id = bgl_file.bgl_file_id "
                         "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                         "where waypoint_id = :id");

  airspaceQuery = new SqlQuery(dbNav);
  airspaceQuery->prepare("select * from boundary "
                         "join bgl_file on boundary.file_id = bgl_file.bgl_file_id "
                         "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                         "where boundary_id = :id");

  airwayQuery = new SqlQuery(dbNav);
  airwayQuery->prepare("select * from airway where airway_id = :id");

  runwayQuery = new SqlQuery(db);
  runwayQuery->prepare("select * from runway where airport_id = :id order by heading");

  runwayEndQuery = new SqlQuery(db);
  runwayEndQuery->prepare("select * from runway_end where runway_end_id = :id");

  helipadQuery = new SqlQuery(db);
  helipadQuery->prepare("select h.*, s.number as start_number, s.runway_name from helipad h "
                        " left outer join start s on s.start_id= h.start_id "
                        " where h.airport_id = :id order by s.runway_name");

  startQuery = new SqlQuery(db);
  startQuery->prepare("select * from start where airport_id = :id order by type asc, runway_name");

  ilsQuerySim = new SqlQuery(db);
  ilsQuerySim->prepare("select * from ils where loc_runway_end_id = :id");

  ilsQueryNav = new SqlQuery(dbNav);
  ilsQueryNav->prepare("select * from ils where loc_runway_end_id = :id");

  ilsQuerySimByName = new SqlQuery(db);
  ilsQuerySimByName->prepare("select * from ils where loc_airport_ident = :apt and loc_runway_name = :rwy");

  airwayWaypointQuery = new SqlQuery(dbNav);
  airwayWaypointQuery->prepare("select "
                               " w1.ident as from_ident, w1.region as from_region, "
                               " w1.lonx as from_lonx, w1.laty as from_laty, "
                               " w2.ident as to_ident, w2.region as to_region, "
                               " w2.lonx as to_lonx, w2.laty as to_laty "
                               " from airway a "
                               " join waypoint w1 on w1.waypoint_id = a.from_waypoint_id "
                               " join waypoint w2 on w2.waypoint_id = a.to_waypoint_id "
                               " where airway_name = :name and airway_fragment_no = :fragment "
                               " order by a.sequence_no");

  vorIdentRegionQuery = new SqlQuery(dbNav);
  vorIdentRegionQuery->prepare("select * from vor where ident = :ident and region = :region");

  approachQuery = new SqlQuery(dbNav);
  approachQuery->prepare("select a.runway_name, r.runway_end_id, a.* from approach a "
                         "left outer join runway_end r on a.runway_end_id = r.runway_end_id "
                         "where a.airport_id = :id "
                         "order by a.runway_name, a.type, a.fix_ident");

  transitionQuery = new SqlQuery(dbNav);
  transitionQuery->prepare("select * from transition where approach_id = :id order by fix_ident");
}

void InfoQuery::deInitQueries()
{
  airportCache.clear();
  vorCache.clear();
  ndbCache.clear();
  airspaceCache.clear();
  waypointCache.clear();
  airwayCache.clear();
  runwayEndCache.clear();
  ilsCacheSim.clear();
  ilsCacheNav.clear();
  ilsCacheSimByName.clear();
  comCache.clear();
  runwayCache.clear();
  helipadCache.clear();
  startCache.clear();
  approachCache.clear();
  transitionCache.clear();
  airportSceneryCache.clear();

  delete airportQuery;
  airportQuery = nullptr;

  delete airportSceneryQuery;
  airportSceneryQuery = nullptr;

  delete comQuery;
  comQuery = nullptr;

  delete vorQuery;
  vorQuery = nullptr;

  delete ndbQuery;
  ndbQuery = nullptr;

  delete airspaceQuery;
  airspaceQuery = nullptr;

  delete waypointQuery;
  waypointQuery = nullptr;

  delete airwayQuery;
  airwayQuery = nullptr;

  delete runwayQuery;
  runwayQuery = nullptr;

  delete helipadQuery;
  helipadQuery = nullptr;

  delete startQuery;
  startQuery = nullptr;

  delete runwayEndQuery;
  runwayEndQuery = nullptr;

  delete ilsQueryNav;
  ilsQueryNav = nullptr;

  delete ilsQuerySim;
  ilsQuerySim = nullptr;

  delete ilsQuerySimByName;
  ilsQuerySimByName = nullptr;

  delete airwayWaypointQuery;
  airwayWaypointQuery = nullptr;

  delete vorIdentRegionQuery;
  vorIdentRegionQuery = nullptr;

  delete approachQuery;
  approachQuery = nullptr;

  delete transitionQuery;
  transitionQuery = nullptr;
}
