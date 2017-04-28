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
#include "common/proctypes.h"
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

  const proc::MapProcedureLeg *getApproachLeg(const map::MapAirport& airport, int approachId, int legId);
  const proc::MapProcedureLeg *getTransitionLeg(const map::MapAirport& airport, int legId);

  /* Get approach only */
  const proc::MapProcedureLegs *getApproachLegs(const map::MapAirport& airport, int approachId);

  /* Get transition and its approach */
  const proc::MapProcedureLegs *getTransitionLegs(const map::MapAirport& airport, int transitionId);

  QVector<int> getTransitionIdsForApproach(int approachId);

  bool getLegsForFlightplanProperties(const QHash<QString, QString> properties, const map::MapAirport& departure,
                                      const map::MapAirport& destination,
                                      proc::MapProcedureLegs& arrivalLegs, proc::MapProcedureLegs& starLegs,
                                      proc::MapProcedureLegs& departureLegs);

  static QString getSidAndTransition(QHash<QString, QString>& properties);
  static QString getStarAndTransition(QHash<QString, QString>& properties);

  static void extractLegsForFlightplanProperties(QHash<QString, QString>& properties,
                                                 const proc::MapProcedureLegs& arrivalLegs,
                                                 const proc::MapProcedureLegs& starLegs,
                                                 const proc::MapProcedureLegs& departureLegs);

  static void clearFlightplanProcedureProperties(QHash<QString, QString>& properties,
                                                 const proc::MapProcedureTypes& type);

  int getSidId(const map::MapAirport& departure, const QString& sid,
               const QString& runway = QString(), float distance = map::INVALID_DISTANCE_VALUE, int size = -1);
  int getSidTransitionId(const map::MapAirport& departure, const QString& sidTrans, int sidId,
                         float distance = map::INVALID_DISTANCE_VALUE, int size = -1);

  int getStarId(const map::MapAirport& destination, const QString& star,
                float distance = map::INVALID_DISTANCE_VALUE, int size = -1);
  int getStarTransitionId(const map::MapAirport& destination, const QString& starTrans, int starId,
                          float distance = map::INVALID_DISTANCE_VALUE, int size = -1);

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

private:
  proc::MapProcedureLeg buildTransitionLegEntry(const map::MapAirport& airport);
  proc::MapProcedureLeg buildApproachLegEntry(const map::MapAirport& airport);
  void buildLegEntry(atools::sql::SqlQuery *query, proc::MapProcedureLeg& leg, const map::MapAirport& airport);

  void postProcessLegs(const map::MapAirport& airport, proc::MapProcedureLegs& legs);
  void processLegs(proc::MapProcedureLegs& legs);
  void processLegErrors(proc::MapProcedureLegs& legs);

  /* Fill the courese and heading to intercept legs after all other lines are calculated */
  void processCourseInterceptLegs(proc::MapProcedureLegs& legs);

  /* Fill calculatedDistance and calculated course fields */
  void processLegsDistanceAndCourse(proc::MapProcedureLegs& legs);

  /* Add an artificial (not in the database) runway leg if no connection to the end is given */
  void processArtificialLegs(const map::MapAirport& airport, proc::MapProcedureLegs& legs);

  /* Adjust conflicting altitude restrictions where a transition ends with "A2000" and is the same as the following
   * initial fix having "2000" */
  void processLegsFixRestrictions(proc::MapProcedureLegs& legs);

  /* Assign magnetic variation from the navaids */
  void updateMagvar(const map::MapAirport& airport, proc::MapProcedureLegs& legs);
  void updateBounding(proc::MapProcedureLegs& legs);

  void assignType(proc::MapProcedureLegs& procedure);
  proc::MapProcedureLeg createRunwayLeg(const proc::MapProcedureLeg& leg,
                                        const proc::MapProcedureLegs& legs);
  proc::MapProcedureLeg createStartLeg(const proc::MapProcedureLeg& leg,
                                       const proc::MapProcedureLegs& legs);

  proc::MapProcedureLegs *buildApproachLegs(const map::MapAirport& airport, int approachId);
  proc::MapProcedureLegs *fetchApproachLegs(const map::MapAirport& airport, int approachId);
  proc::MapProcedureLegs *fetchTransitionLegs(const map::MapAirport& airport, int approachId,
                                              int transitionId);
  int approachIdForTransitionId(int transitionId);
  void mapObjectByIdent(map::MapSearchResult& result, map::MapObjectTypes type, const QString& ident,
                        const QString& region, const QString& airport,
                        const atools::geo::Pos& sortByDistancePos = atools::geo::EMPTY_POS);

  int findTransitionId(const map::MapAirport& airport, atools::sql::SqlQuery *query, float distance, int size);
  int findApproachId(const map::MapAirport& airport, atools::sql::SqlQuery *query, const QString& suffix,
                     const QString& runway, float distance, int size);
  int findProcedureLegId(const map::MapAirport& airport, atools::sql::SqlQuery *query,
                         const QString& suffix, const QString& runway, float distance, int size, bool transition);
  void runwayEndByName(map::MapSearchResult& result, const QString& name, const map::MapAirport& airport);

  atools::sql::SqlDatabase *db;
  atools::sql::SqlQuery *approachLegQuery = nullptr, *transitionLegQuery = nullptr,
  *transitionIdForLegQuery = nullptr, *approachIdForTransQuery = nullptr,
  *runwayEndIdQuery = nullptr, *transitionQuery = nullptr, *approachQuery = nullptr,
  *transitionIdByNameQuery = nullptr, *approachIdByNameQuery = nullptr, *transitionIdsForApproachQuery = nullptr;

  /* approach ID and transition ID to full lists
   * The approach also has to be stored for transitions since the handover can modify approach legs (CI legs, etc.) */
  QCache<int, proc::MapProcedureLegs> approachCache, transitionCache;

  /* maps leg ID to approach/transition ID and index in list */
  QHash<int, std::pair<int, int> > approachLegIndex, transitionLegIndex;

  MapQuery *mapQuery = nullptr;

  /* Use this value as an id base for the artifical runway legs. Add id of the predecessor to it to be able to find the
   * leg again */
  Q_DECL_CONSTEXPR static int RUNWAY_LEG_ID_BASE = 1000000000;
  Q_DECL_CONSTEXPR static int START_LEG_ID_BASE = 500000000;

};

#endif // LITTLENAVMAP_APPROACHQUERY_H
