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

#ifndef INFOQUERY_H
#define INFOQUERY_H

#include <QCache>
#include <QObject>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
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
  const atools::sql::SqlRecord *getVorInformation(int vorId);
  const atools::sql::SqlRecord *getNdbInformation(int ndbId);
  const atools::sql::SqlRecord *getWaypointInformation(int waypointId);
  const atools::sql::SqlRecord *getAirwayInformation(int airwayId);

  void initQueries();
  void deInitQueries();

private:
  QCache<int, atools::sql::SqlRecord> airportCache, vorCache, ndbCache, waypointCache, airwayCache;

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *airportQuery = nullptr, *vorQuery = nullptr, *ndbQuery = nullptr,
  *waypointQuery = nullptr, *airwayQuery = nullptr;

  const atools::sql::SqlRecord *cachedRecord(QCache<int, atools::sql::SqlRecord>& cache,
                                             atools::sql::SqlQuery *query, int id);

};

#endif // INFOQUERY_H
