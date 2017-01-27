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
#include <QApplication>
#include <functional>

namespace atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class MapQuery;

class ApproachQuery
{
  Q_DECLARE_TR_FUNCTIONS(ApproachQuery)

public:
  ApproachQuery(atools::sql::SqlDatabase *sqlDb, MapQuery *mapQueryParam);
  virtual ~ApproachQuery();

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

  const maptypes::MapApproachLeg *getApproachLeg(const maptypes::MapAirport& airport, int legId);
  const maptypes::MapApproachLeg *getTransitionLeg(const maptypes::MapAirport& airport, int legId);
  const maptypes::MapApproachLegList *getApproachLegs(const maptypes::MapAirport& airport, int approachId);
  const maptypes::MapApproachLegList *getTransitionLegs(const maptypes::MapAirport& airport, int transitionId);

private:
  void buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapApproachLeg& entry);
  maptypes::MapApproachLeg buildTransitionLegEntry(atools::sql::SqlQuery *query);
  maptypes::MapApproachLeg buildApproachLegEntry(atools::sql::SqlQuery *query);
  void postProcessLegList(maptypes::MapApproachLegList *legs);
  void updateMagvar(const maptypes::MapAirport& airport,
                    maptypes::MapApproachLegList *legs);

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *approachQuery = nullptr, *transitionQuery = nullptr,
  *approachLegQuery = nullptr, *transitionLegQuery = nullptr;

  // approach ID and transition ID to lists
  QCache<int, maptypes::MapApproachLegList> approachCache, transitionCache;

  // maps leg ID to approach/transition ID and index in list
  QHash<int, std::pair<int, int> > approachLegIndex, transitionLegIndex;

  MapQuery *mapQuery = nullptr;

  maptypes::MapApproachLegList *buildEntries(const maptypes::MapAirport& airport,
                                             QCache<int, maptypes::MapApproachLegList>& cache,
                                             QHash<int, std::pair<int, int> >& index,
                                             atools::sql::SqlQuery *query, int approachId,
                                             std::function<maptypes::MapApproachLeg(
                                                             atools::sql::SqlQuery *)> factoryFunction);

};

#endif // LITTLENAVMAP_APPROACHQUERY_H
