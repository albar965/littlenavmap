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
#include "fs/fspaths.h"

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

/* Loads and caches approaches and transitions. The corresponding approach is also loaded and cached if a
 * transition is loaded since legs depend on each other.*/
class ProcedureQuery
{
  Q_DECLARE_TR_FUNCTIONS(ProcedureQuery)

public:
  ProcedureQuery(atools::sql::SqlDatabase *sqlDb, MapQuery *mapQueryParam);
  virtual ~ProcedureQuery();

  const maptypes::MapProcedureLeg *getApproachLeg(const maptypes::MapAirport& airport, int approachId, int legId);
  const maptypes::MapProcedureLeg *getTransitionLeg(const maptypes::MapAirport& airport, int legId);

  /* Get approach only */
  const maptypes::MapProcedureLegs *getApproachLegs(const maptypes::MapAirport& airport, int approachId);

  /* Get transition and its approach */
  const maptypes::MapProcedureLegs *getTransitionLegs(const maptypes::MapAirport& airport, int transitionId);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

  void setCurrentSimulator(atools::fs::FsPaths::SimulatorType simType);

  bool getLegsForFlightplanProperties(const QHash<QString, QString> properties, const maptypes::MapAirport& departure,
                                      const maptypes::MapAirport& destination,
                                      maptypes::MapProcedureLegs& arrivalLegs, maptypes::MapProcedureLegs& starLegs,
                                      maptypes::MapProcedureLegs& departureLegs);

  static void extractLegsForFlightplanProperties(QHash<QString, QString>& properties,
                                                 const maptypes::MapProcedureLegs& arrivalLegs,
                                                 const maptypes::MapProcedureLegs& starLegs,
                                                 const maptypes::MapProcedureLegs& departureLegs);

  static void clearFlightplanProcedureProperties(QHash<QString, QString>& properties, maptypes::MapObjectTypes type);

private:
  maptypes::MapProcedureLeg buildTransitionLegEntry(const maptypes::MapAirport& airport);
  maptypes::MapProcedureLeg buildApproachLegEntry(const maptypes::MapAirport& airport);
  void buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapProcedureLeg& leg, const maptypes::MapAirport& airport);

  void postProcessLegs(const maptypes::MapAirport& airport, maptypes::MapProcedureLegs& legs);
  void processLegs(maptypes::MapProcedureLegs& legs);

  /* Fill the courese and heading to intercept legs after all other lines are calculated */
  void processCourseInterceptLegs(maptypes::MapProcedureLegs& legs);

  /* Fill calculatedDistance and calculated course fields */
  void processLegsDistanceAndCourse(maptypes::MapProcedureLegs& legs);

  /* Add an artificial (not in the database) runway leg if no connection to the end is given */
  void processArtificialLegs(const maptypes::MapAirport& airport, maptypes::MapProcedureLegs& legs);

  /* Adjust conflicting altitude restrictions where a transition ends with "A2000" and is the same as the following
   * initial fix having "2000" */
  void processLegsFixRestrictions(maptypes::MapProcedureLegs& legs);

  /* Assign magnetic variation from the navaids */
  void updateMagvar(maptypes::MapProcedureLegs& legs);
  void updateBoundingAndDirection(const maptypes::MapAirport& airport, maptypes::MapProcedureLegs& legs);

  void assignType(maptypes::MapProcedureLegs& approach);
  maptypes::MapProcedureLeg createRunwayLeg(const maptypes::MapProcedureLeg& leg,
                                           const maptypes::MapProcedureLegs& legs);
  maptypes::MapProcedureLeg createStartLeg(const maptypes::MapProcedureLeg& leg, const maptypes::MapProcedureLegs& legs);

  maptypes::MapProcedureLegs *buildApproachLegs(const maptypes::MapAirport& airport, int approachId);
  maptypes::MapProcedureLegs *fetchApproachLegs(const maptypes::MapAirport& airport, int approachId);
  maptypes::MapProcedureLegs *fetchTransitionLegs(const maptypes::MapAirport& airport, int approachId, int transitionId);
  int approachIdForTransitionId(int transitionId);
  void mapObjectByIdent(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type, const QString& ident,
                        const QString& region, const QString& airport,
                        const atools::geo::Pos& sortByDistancePos = atools::geo::EMPTY_POS);
  int findProcedureLegId(const maptypes::MapAirport& airport, atools::sql::SqlQuery *query,
                         const QString& suffix, float distance, int size, bool transition);

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *approachLegQuery = nullptr, *transitionLegQuery = nullptr,
  *transitionIdForLegQuery = nullptr, *approachIdForTransQuery = nullptr,
  *runwayEndIdQuery = nullptr, *transitionQuery = nullptr, *approachQuery = nullptr,
  *transitionIdByNameQuery = nullptr, *approachIdByNameQuery = nullptr, *transitionIdsForApproachQuery = nullptr;

  /* approach ID and transition ID to full lists
   * The approach also has to be stored for transitions since the handover can modify approach legs (CI legs, etc.) */
  QCache<int, maptypes::MapProcedureLegs> approachCache, transitionCache;

  /* maps leg ID to approach/transition ID and index in list */
  QHash<int, std::pair<int, int> > approachLegIndex, transitionLegIndex;

  MapQuery *mapQuery = nullptr;

  /* Use this value as an id base for the artifical runway legs. Add id of the predecessor to it to be able to find the
   * leg again */
  Q_DECL_CONSTEXPR static int RUNWAY_LEG_ID_BASE = 1000000000;
  Q_DECL_CONSTEXPR static int START_LEG_ID_BASE = 500000000;

  atools::fs::FsPaths::SimulatorType simulatorType = atools::fs::FsPaths::UNKNOWN;

};

#endif // LITTLENAVMAP_APPROACHQUERY_H
