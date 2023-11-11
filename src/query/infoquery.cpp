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

#include "query/infoquery.h"

#include "common/constants.h"
#include "app/navapp.h"
#include "common/maptools.h"
#include "query/querytypes.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "sql/sqlutil.h"

using atools::sql::SqlQuery;
using atools::sql::SqlDatabase;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordList;
using atools::sql::SqlUtil;

InfoQuery::InfoQuery(SqlDatabase *sqlDbSim, atools::sql::SqlDatabase *sqlDbNav, atools::sql::SqlDatabase *sqlDbTrack)
  : dbSim(sqlDbSim), dbNav(sqlDbNav), dbTrack(sqlDbTrack)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  airportCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportCache", 100).toInt());
  vorCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "VorCache", 100).toInt());
  ndbCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "NdbCache", 100).toInt());
  msaCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "MsaCache", 100).toInt());
  holdingCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "HoldingCache", 100).toInt());
  runwayEndCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "RunwayEndCache", 100).toInt());
  comCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "ComCache", 100).toInt());
  runwayCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "RunwayCache", 100).toInt());
  helipadCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "HelipadCache", 100).toInt());
  startCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "StartCache", 100).toInt());
  approachCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "ApproachCache", 100).toInt());
  transitionCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "TransitionCache", 100).toInt());
  airportSceneryCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "AirportSceneryCache", 100).toInt());
}

InfoQuery::~InfoQuery()
{
  deInitQueries();
}

const SqlRecord *InfoQuery::getAirportInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, airportQuery))
    return nullptr;

  airportQuery->bindValue(":id", airportId);
  return query::cachedRecord(airportCache, airportQuery, airportId);
}

const atools::sql::SqlRecordList *InfoQuery::getAirportSceneryInformation(const QString& ident)
{
  if(!query::valid(Q_FUNC_INFO, airportSceneryQuery))
    return nullptr;

  airportSceneryQuery->bindValue(":id", ident);
  return query::cachedRecordList(airportSceneryCache, airportSceneryQuery, ident);
}

bool InfoQuery::isAirportXplaneCustomOnly(const QString& ident)
{
  // X-Plane 11/Resources/default scenery/default apt dat/Earth nav data/apt.dat
  // X-Plane 11/Custom Scenery/Global Airports/Earth nav data/apt.dat

  // X-Plane 12/Global Scenery/Global Airports/Earth nav data/apt.dat

  const atools::sql::SqlRecordList *airportSceneryInformation = getAirportSceneryInformation(ident);
  if(airportSceneryInformation != nullptr)
  {
    for(const atools::sql::SqlRecord& rec : *airportSceneryInformation)
    {
      QString filepath = rec.valueStr("filepath").replace('\\', '/');

      if(filepath.endsWith("Resources/default scenery/default apt dat/Earth nav data/apt.dat", Qt::CaseInsensitive) ||
         filepath.endsWith("Custom Scenery/Global Airports/Earth nav data/apt.dat", Qt::CaseInsensitive) ||
         filepath.endsWith("Global Scenery/Global Airports/Earth nav data/apt.dat", Qt::CaseInsensitive))
        // Also in stock and maybe only overloaded by an add-on
        return false;
    }
  }
  // No stock locations found - custom / add-on only
  return true;
}

const SqlRecordList *InfoQuery::getComInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, comQuery))
    return nullptr;

  comQuery->bindValue(":id", airportId);
  return query::cachedRecordList(comCache, comQuery, airportId);
}

const SqlRecordList *InfoQuery::getApproachInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, approachQuery))
    return nullptr;

  approachQuery->bindValue(":id", airportId);
  return query::cachedRecordList(approachCache, approachQuery, airportId);
}

const SqlRecordList *InfoQuery::getTransitionInformation(int approachId)
{
  if(!query::valid(Q_FUNC_INFO, transitionQuery))
    return nullptr;

  transitionQuery->bindValue(":id", approachId);
  return query::cachedRecordList(transitionCache, transitionQuery, approachId);
}

const SqlRecordList *InfoQuery::getRunwayInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, runwayQuery))
    return nullptr;

  runwayQuery->bindValue(":id", airportId);
  return query::cachedRecordList(runwayCache, runwayQuery, airportId);
}

void InfoQuery::getRunwayEnds(maptools::RwVector& ends, int airportId)
{
  const SqlRecordList *recVector = getRunwayInformation(airportId);
  if(recVector != nullptr)
  {
    // Collect runway ends and wind conditions =======================================
    for(const SqlRecord& rec : *recVector)
    {
      int length = rec.valueInt("length");
      QString surface = rec.valueStr("surface");
      const SqlRecord *recPrim = getRunwayEndInformation(rec.valueInt("primary_end_id"));
      if(!recPrim->valueBool("has_closed_markings"))
        ends.appendRwEnd(recPrim->valueStr("name"), surface, length, recPrim->valueFloat("heading"));

      const SqlRecord *recSec = getRunwayEndInformation(rec.valueInt("secondary_end_id"));
      if(!recSec->valueBool("has_closed_markings"))
        ends.appendRwEnd(recSec->valueStr("name"), surface, length, recSec->valueFloat("heading"));
    }
  }

  // Sort by headwind and merge entries =======================================
  ends.sortRunwayEnds();
}

const SqlRecordList *InfoQuery::getHelipadInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, helipadQuery))
    return nullptr;

  helipadQuery->bindValue(":id", airportId);
  return query::cachedRecordList(helipadCache, helipadQuery, airportId);
}

const SqlRecordList *InfoQuery::getStartInformation(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, startQuery))
    return nullptr;

  startQuery->bindValue(":id", airportId);
  return query::cachedRecordList(startCache, startQuery, airportId);
}

const atools::sql::SqlRecord *InfoQuery::getRunwayEndInformation(int runwayEndId)
{
  if(!query::valid(Q_FUNC_INFO, runwayEndQuery))
    return nullptr;

  runwayEndQuery->bindValue(":id", runwayEndId);
  return query::cachedRecord(runwayEndCache, runwayEndQuery, runwayEndId);
}

const atools::sql::SqlRecord *InfoQuery::getVorInformation(int vorId)
{
  if(!query::valid(Q_FUNC_INFO, vorQuery))
    return nullptr;

  vorQuery->bindValue(":id", vorId);
  return query::cachedRecord(vorCache, vorQuery, vorId);
}

const atools::sql::SqlRecord InfoQuery::getVorByIdentAndRegion(const QString& ident, const QString& region)
{
  atools::sql::SqlRecord rec;
  if(!query::valid(Q_FUNC_INFO, vorIdentRegionQuery))
    return rec;

  vorIdentRegionQuery->bindValue(":ident", ident);
  vorIdentRegionQuery->bindValue(":region", region);
  vorIdentRegionQuery->exec();

  if(vorIdentRegionQuery->next())
    rec = vorIdentRegionQuery->record();

  vorIdentRegionQuery->finish();
  return rec;
}

const atools::sql::SqlRecord *InfoQuery::getNdbInformation(int ndbId)
{
  if(!query::valid(Q_FUNC_INFO, ndbQuery))
    return nullptr;

  ndbQuery->bindValue(":id", ndbId);
  return query::cachedRecord(ndbCache, ndbQuery, ndbId);
}

const SqlRecord *InfoQuery::getMsaInformation(int msaId)
{
  if(msaQuery != nullptr)
  {
    msaQuery->bindValue(":id", msaId);
    return query::cachedRecord(msaCache, msaQuery, msaId);
  }
  else
    return nullptr;
}

const SqlRecord *InfoQuery::getHoldingInformation(int holdingId)
{
  if(holdingQuery != nullptr)
  {
    holdingQuery->bindValue(":id", holdingId);
    return query::cachedRecord(holdingCache, holdingQuery, holdingId);
  }
  else
    return nullptr;
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

  // airport_file_id	file_id	ident	bgl_file_id	scenery_area_id	bgl_create_time	file_modification_time
  // filepath	filename	size	comment	scenery_area_id:1	number	layer	title	remote_path	local_path	active	required	exclude
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

  // Check for holding table in nav (Navigraph) database and then in simulator database (X-Plane only)
  SqlDatabase *msaDb = NavApp::isNavdataOff() ?
                       SqlUtil::getDbWithTableAndRows("airport_msa", {dbSim, dbNav}) :
                       SqlUtil::getDbWithTableAndRows("airport_msa", {dbNav, dbSim});

  if(msaDb != nullptr)
  {
    msaQuery = new SqlQuery(msaDb);
    msaQuery->prepare("select * from airport_msa "
                      "join bgl_file on airport_msa.file_id = bgl_file.bgl_file_id "
                      "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                      "where airport_msa_id = :id");
  }

  // Same as above for airport MSA table
  SqlDatabase *holdingDb = NavApp::isNavdataOff() ?
                           SqlUtil::getDbWithTableAndRows("holding", {dbSim, dbNav}) :
                           SqlUtil::getDbWithTableAndRows("holding", {dbNav, dbSim});
  if(holdingDb != nullptr)
  {
    holdingQuery = new SqlQuery(holdingDb);
    holdingQuery->prepare("select * from holding "
                          "join bgl_file on holding.file_id = bgl_file.bgl_file_id "
                          "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                          "where holding_id = :id");
  }

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
  msaCache.clear();
  holdingCache.clear();
  runwayEndCache.clear();
  comCache.clear();
  runwayCache.clear();
  helipadCache.clear();
  startCache.clear();
  approachCache.clear();
  transitionCache.clear();
  airportSceneryCache.clear();

  ATOOLS_DELETE(airportQuery);
  ATOOLS_DELETE(airportSceneryQuery);
  ATOOLS_DELETE(comQuery);
  ATOOLS_DELETE(vorQuery);
  ATOOLS_DELETE(msaQuery);
  ATOOLS_DELETE(holdingQuery);
  ATOOLS_DELETE(ndbQuery);
  ATOOLS_DELETE(runwayQuery);
  ATOOLS_DELETE(helipadQuery);
  ATOOLS_DELETE(startQuery);
  ATOOLS_DELETE(runwayEndQuery);
  ATOOLS_DELETE(vorIdentRegionQuery);
  ATOOLS_DELETE(approachQuery);
  ATOOLS_DELETE(transitionQuery);
}
