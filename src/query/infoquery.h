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

#ifndef LITTLENAVMAP_INFOQUERY_H
#define LITTLENAVMAP_INFOQUERY_H

#include <QCache>
#include <QObject>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
typedef QVector<atools::sql::SqlRecord> SqlRecordVector;
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
  InfoQuery(atools::sql::SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav, atools::sql::SqlDatabase *sqlDbTrack);
  ~InfoQuery();

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

  /* Get record list for table runway of an airport */
  const atools::sql::SqlRecordVector *getRunwayInformation(int airportId);

  /* Get record for table runway_end */
  const atools::sql::SqlRecord *getRunwayEndInformation(int runwayEndId);

  const atools::sql::SqlRecordVector *getHelipadInformation(int airportId);
  const atools::sql::SqlRecordVector *getStartInformation(int airportId);

  /* Get record for table ils for an runway end */
  const atools::sql::SqlRecord *getIlsInformationSim(int runwayEndId);
  atools::sql::SqlRecord getIlsInformationSimById(int ilsId);
  atools::sql::SqlRecord getIlsInformationNavById(int ilsId);

  /* Get record for table ils for an runway end */
  const atools::sql::SqlRecord *getIlsInformationNav(int runwayEndId);
  const atools::sql::SqlRecordVector *getIlsInformationSimByName(const QString& airportIdent, const QString& runway);

  /* Get runway name and all columns from table approach */
  const atools::sql::SqlRecordVector *getApproachInformation(int airportId);

  /* Get record for table transition */
  const atools::sql::SqlRecordVector *getTransitionInformation(int approachId);

  /* Get a record from table trackmeta for given track id */
  atools::sql::SqlRecord getTrackMetadata(int trackId);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

private:
  const atools::sql::SqlRecordVector *ilsInformationSimByName(const QString& airportIdent, const QString& runway);

  /* Caches */
  QCache<int, atools::sql::SqlRecord> airportCache, vorCache, ndbCache, runwayEndCache,
                                      ilsCacheNav, ilsCacheSim;

  QCache<int, atools::sql::SqlRecordVector> comCache, runwayCache, helipadCache, startCache, approachCache,
                                            transitionCache;
  QCache<std::pair<QString, QString>, atools::sql::SqlRecordVector> ilsCacheSimByName;

  QCache<QString, atools::sql::SqlRecordVector> airportSceneryCache;

  atools::sql::SqlDatabase *dbSim, *dbNav, *dbTrack;

  /* Prepared database queries */
  atools::sql::SqlQuery *airportQuery = nullptr, *airportSceneryQuery = nullptr, *vorQuery = nullptr,
                        *ndbQuery = nullptr, *comQuery = nullptr, *runwayQuery = nullptr, *runwayEndQuery = nullptr,
                        *helipadQuery = nullptr, *startQuery = nullptr, *ilsQuerySim = nullptr, *ilsQueryNav = nullptr,
                        *ilsQuerySimByName = nullptr, *ilsQueryNavById = nullptr, *ilsQuerySimById = nullptr,
                        *vorIdentRegionQuery = nullptr, *approachQuery = nullptr, *transitionQuery = nullptr;

};

#endif // LITTLENAVMAP_INFOQUERY_H
