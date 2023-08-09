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

#ifndef LITTLENAVMAP_INFOQUERY_H
#define LITTLENAVMAP_INFOQUERY_H

#include "sql/sqltypes.h"

#include <QCache>
#include <QObject>

namespace maptools {
class RwVector;
}
namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
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
  InfoQuery(atools::sql::SqlDatabase *sqlDbSim, atools::sql::SqlDatabase *sqlDbNav, atools::sql::SqlDatabase *sqlDbTrack);
  ~InfoQuery();

  InfoQuery(const InfoQuery& other) = delete;
  InfoQuery& operator=(const InfoQuery& other) = delete;

  /* Get record for joined tables airport, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getAirportInformation(int airportId);
  const atools::sql::SqlRecordList *getAirportSceneryInformation(const QString& ident);

  /* Returns true if airport is *not* a part of the X-Plane stock scenery like
   * "Resources/default scenery/default apt dat/Earth nav data/apt.dat" or
   * "Custom Scenery/Global Airports/Earth nav data/apt.dat" */
  bool isAirportXplaneCustomOnly(const QString& ident);

  /* Get record for table com */
  const atools::sql::SqlRecordList *getComInformation(int airportId);

  /* Get record for joined tables vor, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getVorInformation(int vorId);
  const atools::sql::SqlRecord getVorByIdentAndRegion(const QString& ident, const QString& region);

  /* Get record for joined tables ndb, bgl_file and scenery_area */
  const atools::sql::SqlRecord *getNdbInformation(int ndbId);

  const atools::sql::SqlRecord *getMsaInformation(int msaId);
  const atools::sql::SqlRecord *getHoldingInformation(int holdingId);

  /* Get record list for table runway of an airport */
  const atools::sql::SqlRecordList *getRunwayInformation(int airportId);

  /* Get record for table runway_end */
  const atools::sql::SqlRecord *getRunwayEndInformation(int runwayEndId);

  /* Get runways paired with runway ends. Closed are excluded. */
  void getRunwayEnds(maptools::RwVector& ends, int airportId);

  const atools::sql::SqlRecordList *getHelipadInformation(int airportId);
  const atools::sql::SqlRecordList *getStartInformation(int airportId);

  /* Get runway name and all columns from table approach */
  const atools::sql::SqlRecordList *getApproachInformation(int airportId);

  /* Get record for table transition */
  const atools::sql::SqlRecordList *getTransitionInformation(int approachId);

  /* Get a record from table trackmeta for given track id */
  atools::sql::SqlRecord getTrackMetadata(int trackId);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

private:
  /* Caches */
  QCache<int, atools::sql::SqlRecord> airportCache, vorCache, ndbCache, runwayEndCache, msaCache, holdingCache;

  QCache<int, atools::sql::SqlRecordList> comCache, runwayCache, helipadCache, startCache, approachCache,
                                          transitionCache;

  QCache<QString, atools::sql::SqlRecordList> airportSceneryCache;

  atools::sql::SqlDatabase *dbSim, *dbNav, *dbTrack;

  /* Prepared database queries */
  atools::sql::SqlQuery *airportQuery = nullptr, *airportSceneryQuery = nullptr, *vorQuery = nullptr, *msaQuery = nullptr,
                        *holdingQuery = nullptr, *ndbQuery = nullptr, *comQuery = nullptr, *runwayQuery = nullptr,
                        *runwayEndQuery = nullptr, *helipadQuery = nullptr, *startQuery = nullptr, *vorIdentRegionQuery = nullptr,
                        *approachQuery = nullptr, *transitionQuery = nullptr;

};

#endif // LITTLENAVMAP_INFOQUERY_H
