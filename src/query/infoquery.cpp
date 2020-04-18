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

#include "query/infoquery.h"

#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "settings/settings.h"
#include "query/querytypes.h"
#include "common/constants.h"
#include "common/maptypes.h"

using atools::sql::SqlQuery;
using atools::sql::SqlDatabase;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;

InfoQuery::InfoQuery(SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav, atools::sql::SqlDatabase *sqlDbTrack)
  : dbSim(sqlDb), dbNav(sqlDbNav), dbTrack(sqlDbTrack)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  airportCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportCache", 100).toInt());
  vorCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "VorCache", 100).toInt());
  ndbCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "NdbCache", 100).toInt());
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
  airportSceneryCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportSceneryCache",
                                                           100).toInt());
}

InfoQuery::~InfoQuery()
{
  deInitQueries();
}

const SqlRecord *InfoQuery::getAirportInformation(int airportId)
{
  airportQuery->bindValue(":id", airportId);
  return query::cachedRecord(airportCache, airportQuery, airportId);
}

const atools::sql::SqlRecordVector *InfoQuery::getAirportSceneryInformation(const QString& ident)
{
  airportSceneryQuery->bindValue(":id", ident);
  return query::cachedRecordVector(airportSceneryCache, airportSceneryQuery, ident);
}

const SqlRecordVector *InfoQuery::getComInformation(int airportId)
{
  comQuery->bindValue(":id", airportId);
  return query::cachedRecordVector(comCache, comQuery, airportId);
}

const SqlRecordVector *InfoQuery::getApproachInformation(int airportId)
{
  approachQuery->bindValue(":id", airportId);
  return query::cachedRecordVector(approachCache, approachQuery, airportId);
}

const SqlRecordVector *InfoQuery::getTransitionInformation(int approachId)
{
  transitionQuery->bindValue(":id", approachId);
  return query::cachedRecordVector(transitionCache, transitionQuery, approachId);
}

const SqlRecordVector *InfoQuery::getRunwayInformation(int airportId)
{
  runwayQuery->bindValue(":id", airportId);
  return query::cachedRecordVector(runwayCache, runwayQuery, airportId);
}

const SqlRecordVector *InfoQuery::getHelipadInformation(int airportId)
{
  helipadQuery->bindValue(":id", airportId);
  return query::cachedRecordVector(helipadCache, helipadQuery, airportId);
}

const SqlRecordVector *InfoQuery::getStartInformation(int airportId)
{
  startQuery->bindValue(":id", airportId);
  return query::cachedRecordVector(startCache, startQuery, airportId);
}

const atools::sql::SqlRecord *InfoQuery::getRunwayEndInformation(int runwayEndId)
{
  runwayEndQuery->bindValue(":id", runwayEndId);
  return query::cachedRecord(runwayEndCache, runwayEndQuery, runwayEndId);
}

const atools::sql::SqlRecord *InfoQuery::getIlsInformationSim(int runwayEndId)
{
  ilsQuerySim->bindValue(":id", runwayEndId);
  return query::cachedRecord(ilsCacheSim, ilsQuerySim, runwayEndId);
}

atools::sql::SqlRecord InfoQuery::getIlsInformationSimById(int ilsId)
{
  ilsQuerySimById->bindValue(":id", ilsId);
  ilsQuerySimById->exec();
  atools::sql::SqlRecord rec;

  if(ilsQuerySimById->next())
    rec = ilsQuerySimById->record();

  ilsQuerySimById->finish();
  return rec;
}

atools::sql::SqlRecord InfoQuery::getIlsInformationNavById(int ilsId)
{
  ilsQueryNavById->bindValue(":id", ilsId);
  ilsQueryNavById->exec();
  atools::sql::SqlRecord rec;

  if(ilsQueryNavById->next())
    rec = ilsQueryNavById->record();

  ilsQueryNavById->finish();
  return rec;
}

const atools::sql::SqlRecord *InfoQuery::getIlsInformationNav(int runwayEndId)
{
  ilsQueryNav->bindValue(":id", runwayEndId);
  return query::cachedRecord(ilsCacheNav, ilsQueryNav, runwayEndId);
}

const atools::sql::SqlRecordVector *InfoQuery::getIlsInformationSimByName(const QString& airportIdent,
                                                                          const QString& runway)
{
  const atools::sql::SqlRecordVector *retval = nullptr;
  for(const QString& rname: map::runwayNameZeroPrefixVariants(runway))
  {
    retval = ilsInformationSimByName(airportIdent, rname);
    if(retval != nullptr && !retval->isEmpty())
      break;
  }
  return retval;
}

const atools::sql::SqlRecordVector *InfoQuery::ilsInformationSimByName(const QString& airportIdent,
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
  vorQuery->bindValue(":id", vorId);
  return query::cachedRecord(vorCache, vorQuery, vorId);
}

const atools::sql::SqlRecord InfoQuery::getVorByIdentAndRegion(const QString& ident, const QString& region)
{
  vorIdentRegionQuery->bindValue(":ident", ident);
  vorIdentRegionQuery->bindValue(":region", region);
  vorIdentRegionQuery->exec();
  atools::sql::SqlRecord rec;

  if(vorIdentRegionQuery->next())
    rec = vorIdentRegionQuery->record();

  vorIdentRegionQuery->finish();
  return rec;
}

const atools::sql::SqlRecord *InfoQuery::getNdbInformation(int ndbId)
{
  ndbQuery->bindValue(":id", ndbId);
  return query::cachedRecord(ndbCache, ndbQuery, ndbId);
}

atools::sql::SqlRecord InfoQuery::getTrackMetadata(int trackId)
{
  SqlQuery query(dbTrack);
  query.prepare("select m.* from track t join trackmeta m on t.trackmeta_id = m.trackmeta_id where track_id = :id");
  query.bindValue(":id", trackId);
  query.exec();
  if(query.next())
    return query.record();
  else
    return SqlRecord();
}

void InfoQuery::initQueries()
{
  deInitQueries();

  // TODO limit number of columns - remove star query
  airportQuery = new SqlQuery(dbSim);
  airportQuery->prepare("select * from airport "
                        "join bgl_file on airport.file_id = bgl_file.bgl_file_id "
                        "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                        "where airport_id = :id");

  airportSceneryQuery = new SqlQuery(dbSim);
  airportSceneryQuery->prepare("select * from airport_file f "
                               "join bgl_file b on f.file_id = b.bgl_file_id  "
                               "join scenery_area s on b.scenery_area_id = s.scenery_area_id "
                               "where f.ident = :id order by f.airport_file_id");

  comQuery = new SqlQuery(dbSim);
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

  runwayQuery = new SqlQuery(dbSim);
  runwayQuery->prepare("select * from runway where airport_id = :id order by heading");

  runwayEndQuery = new SqlQuery(dbSim);
  runwayEndQuery->prepare("select * from runway_end where runway_end_id = :id");

  helipadQuery = new SqlQuery(dbSim);
  helipadQuery->prepare("select h.*, s.number as start_number, s.runway_name from helipad h "
                        " left outer join start s on s.start_id= h.start_id "
                        " where h.airport_id = :id order by s.runway_name");

  startQuery = new SqlQuery(dbSim);
  startQuery->prepare("select * from start where airport_id = :id order by type asc, runway_name");

  ilsQuerySimById = new SqlQuery(dbSim);
  ilsQuerySimById->prepare("select * from ils where ils_id = :id");

  ilsQueryNavById = new SqlQuery(dbSim);
  ilsQueryNavById->prepare("select * from ils where ils_id = :id");

  ilsQuerySim = new SqlQuery(dbSim);
  ilsQuerySim->prepare("select * from ils where loc_runway_end_id = :id");

  ilsQueryNav = new SqlQuery(dbNav);
  ilsQueryNav->prepare("select * from ils where loc_runway_end_id = :id");

  ilsQuerySimByName = new SqlQuery(dbSim);
  ilsQuerySimByName->prepare("select * from ils where loc_airport_ident = :apt and loc_runway_name = :rwy");

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

  delete ilsQuerySimById;
  ilsQuerySimById = nullptr;

  delete ilsQueryNavById;
  ilsQueryNavById = nullptr;

  delete vorIdentRegionQuery;
  vorIdentRegionQuery = nullptr;

  delete approachQuery;
  approachQuery = nullptr;

  delete transitionQuery;
  transitionQuery = nullptr;
}
