/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

class InfoQuery :
  public QObject
{
  Q_OBJECT

public:
  InfoQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~InfoQuery();

  const atools::sql::SqlRecord *getAirportInformation(int airportId);

  const atools::sql::SqlRecordVector *getComInformation(int airportId);
  const atools::sql::SqlRecord *getVorInformation(int vorId);
  const atools::sql::SqlRecord *getNdbInformation(int ndbId);
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);
  const atools::sql::SqlRecord *getAirwayInformation(int airwayId);
  atools::sql::SqlRecordVector getAirwayWaypointInformation(const QString& name, int fragment);

  void initQueries();
  void deInitQueries();

  const atools::sql::SqlRecordVector *getRunwayInformation(int airportId);
  const atools::sql::SqlRecord *getRunwayEndInformation(int runwayEndId);
  const atools::sql::SqlRecord *getIlsInformation(int runwayEndId);

  const atools::sql::SqlRecordVector *getApproachInformation(int airportId);
  const atools::sql::SqlRecordVector *getTransitionInformation(int approachId);

private:
  const atools::sql::SqlRecord *cachedRecord(QCache<int, atools::sql::SqlRecord>& cache,
                                             atools::sql::SqlQuery *query, int id);

  const atools::sql::SqlRecordVector *cachedRecordVector(QCache<int, atools::sql::SqlRecordVector>& cache,
                                                         atools::sql::SqlQuery *query, int id);

  QCache<int, atools::sql::SqlRecord> airportCache, vorCache, ndbCache, waypointCache, airwayCache,
                                      runwayEndCache, ilsCache;
  QCache<int, atools::sql::SqlRecordVector> comCache, runwayCache, approachCache, transitionCache;

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *airportQuery = nullptr, *vorQuery = nullptr, *ndbQuery = nullptr,
  *waypointQuery = nullptr, *airwayQuery = nullptr, *comQuery = nullptr,
  *runwayQuery = nullptr, *runwayEndQuery = nullptr, *ilsQuery = nullptr,
  *airwayWaypointQuery = nullptr, *approachQuery = nullptr, *transitionQuery = nullptr;

};

#endif // LITTLENAVMAP_INFOQUERY_H
