/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_APPROACHQUERY_H
#define LITTLENAVMAP_APPROACHQUERY_H

#include "geo/pos.h"
#include "common/maptypes.h"

#include <QCache>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class ApproachQuery
{
public:
  ApproachQuery(atools::sql::SqlDatabase *sqlDb);
  virtual ~ApproachQuery();

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

  const maptypes::MapApproachLegList *getApproachLegs(int approachId);
  const maptypes::MapApproachLegList *getTransitionLegs(int transitionId);

private:
  atools::geo::Pos buildPos(atools::sql::SqlQuery *query, int id);

  void buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapApproachLeg& entry);
  maptypes::MapApproachLeg buildTransitionLegEntry(atools::sql::SqlQuery *query);
  maptypes::MapApproachLeg buildApproachLegEntry(atools::sql::SqlQuery *query);

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *approachQuery = nullptr, *transitionQuery = nullptr, *vorQuery = nullptr,
  *ndbQuery = nullptr, *waypointQuery = nullptr, *ilsQuery = nullptr, *runwayQuery = nullptr;
  QCache<int, maptypes::MapApproachLegList> approachCache, transitionCache;

};

#endif // LITTLENAVMAP_APPROACHQUERY_H
