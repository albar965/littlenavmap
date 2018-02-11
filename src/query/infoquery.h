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

#ifndef LITTLENAVMAP_INFOQUERY_H
#define LITTLENAVMAP_INFOQUERY_H

#include <QCache>
#include <QObject>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
class SqlRecordVector;
}
}

/*
 * Database queries for the info controller. Does not return objects but sql records. Records are cached.
 */
class InfoQuery
{
public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  InfoQuery(atools::sql::SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav);
  virtual ~InfoQuery();

  /* Get record for joined tables airport, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getAirportInformation(int airportId);
  const atools::sql::SqlRecordVector *getAirportSceneryInformation(const QString& ident);

  /* Get record for table com */
  const atools::sql::SqlRecordVector *getComInformation(int airportId);

  /* Get record for joined tables vor, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getVorInformation(int vorId);
  const atools::sql::SqlRecord getVorByIdentAndRegion(const QString& ident, const QString& region);

  /* Get record for joined tables ndb, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getNdbInformation(int ndbId);

  /* Get record for joined tables boundary, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getAirspaceInformation(int airspaceId);

  /* Get record for joined tables waypoint, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);

  /* Get record for table airway */
  const atools::sql::SqlRecord *getAirwayInformation(int airwayId);

  /* Get records with pairs of from/to waypoints (ident and region) for an airway.
   * The records are ordered as they appear in the airway. */
  atools::sql::SqlRecordVector getAirwayWaypointInformation(const QString& name, int fragment);

  /* Get record list for table runway of an airport */
  const atools::sql::SqlRecordVector *getRunwayInformation(int airportId);

  /* Get record for table runway_end */
  const atools::sql::SqlRecord *getRunwayEndInformation(int runwayEndId);

  const atools::sql::SqlRecordVector *getHelipadInformation(int airportId);
  const atools::sql::SqlRecordVector *getStartInformation(int airportId);

  /* Get record for table ils for an runway end */
  const atools::sql::SqlRecord *getIlsInformationSim(int runwayEndId);

  /* Get record for table ils for an runway end */
  const atools::sql::SqlRecord *getIlsInformationNav(int runwayEndId);
  const atools::sql::SqlRecordVector *getIlsInformationSimByName(const QString& airportIdent, const QString& runway);

  /* Get runway name and all columns from table approach */
  const atools::sql::SqlRecordVector *getApproachInformation(int airportId);

  /* Get record for table transition */
  const atools::sql::SqlRecordVector *getTransitionInformation(int approachId);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

private:
  template<typename ID>
  static const atools::sql::SqlRecord *cachedRecord(QCache<ID,
                                                           atools::sql::SqlRecord>& cache,
                                                    atools::sql::SqlQuery *query,
                                                    ID id);

  template<typename ID>
  static const atools::sql::SqlRecordVector *cachedRecordVector(QCache<ID,
                                                                       atools::sql::SqlRecordVector>& cache,
                                                                atools::sql::SqlQuery *query,
                                                                ID id);

  /* Caches */
  QCache<int, atools::sql::SqlRecord> airportCache,
                                      vorCache, ndbCache, waypointCache, airspaceCache, airwayCache, runwayEndCache,
                                      ilsCacheNav, ilsCacheSim;

  QCache<int, atools::sql::SqlRecordVector> comCache, runwayCache, helipadCache, startCache, approachCache,
                                            transitionCache;
  QCache<std::pair<QString, QString>, atools::sql::SqlRecordVector> ilsCacheSimByName;

  QCache<QString, atools::sql::SqlRecordVector> airportSceneryCache;

  atools::sql::SqlDatabase *db, *dbNav;

  /* Prepared database queries */
  atools::sql::SqlQuery *airportQuery = nullptr, *airportSceneryQuery = nullptr,
                        *vorQuery = nullptr, *ndbQuery = nullptr,
                        *waypointQuery = nullptr, *airspaceQuery = nullptr, *airwayQuery = nullptr, *comQuery = nullptr,
                        *runwayQuery = nullptr, *runwayEndQuery = nullptr, *helipadQuery = nullptr,
                        *startQuery = nullptr, *ilsQuerySim = nullptr, *ilsQueryNav = nullptr,
                        *ilsQuerySimByName = nullptr,
                        *airwayWaypointQuery = nullptr, *vorIdentRegionQuery = nullptr, *approachQuery = nullptr,
                        *transitionQuery = nullptr;

};

#endif // LITTLENAVMAP_INFOQUERY_H
