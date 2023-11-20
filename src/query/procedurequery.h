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

#ifndef LITTLENAVMAP_PROCEDUREQUERY_H
#define LITTLENAVMAP_PROCEDUREQUERY_H

#include "common/procflags.h"
#include "common/mapflags.h"

#include <QCache>
#include <QCoreApplication>
#include <functional>

namespace atools {
namespace geo {
class Pos;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

namespace proc {
struct MapProcedureLeg;
struct MapProcedureLegs;
}

namespace map {
struct MapAirport;
struct MapRunwayEnd;
struct MapResult;
}
class MapQuery;
class AirportQuery;

/* Loads and caches procedures and transitions. Procedures include
 * final approaches, SID and STAR but excludes transitions.
 *
 * The corresponding procedure is also loaded and cached if a
 * transition is loaded since legs depend on each other.
 *
 * All navaids and procedure are taken from the nav database which might contain data from nav or simulator.
 * All structs of MapAirport are converted to simulator database airports when passed in.
 *
 * This class does not contain MapWidget related caches.
 */
class ProcedureQuery
{
  Q_DECLARE_TR_FUNCTIONS(ProcedureQuery)

public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  ProcedureQuery(atools::sql::SqlDatabase *sqlDbNav);
  ~ProcedureQuery();

  /* Do not allow copying */
  ProcedureQuery(const ProcedureQuery& other) = delete;
  ProcedureQuery& operator=(const ProcedureQuery& other) = delete;

  /* Get procedure legs from cached procedures */
  const proc::MapProcedureLeg *getProcedureLeg(const map::MapAirport& airport, int procedureId, int legId);
  const proc::MapProcedureLeg *getTransitionLeg(const map::MapAirport& airport, int legId);

  /* Get either procedure or procedurer with transition */
  const proc::MapProcedureLegs *getProcedureLegs(const map::MapAirport& airport, int procedureId, int transitionId);

  /* Get all legs of an procedure */
  const proc::MapProcedureLegs *getProcedureLegs(map::MapAirport airport, int procedureId);

  /* Get transition and its procedure */
  const proc::MapProcedureLegs *getTransitionLegs(map::MapAirport airport, int transitionId);

  /* Get all available transitions for the given procedure ID (approach.approach_id in database */
  QVector<int> getTransitionIdsForProcedure(int procedureId);

  /* Resolves all procedures based on given properties and loads them from the database.
   * Procedures are partially resolved in a fuzzy way. */
  void getLegsForFlightplanProperties(const QHash<QString, QString>& properties,
                                      const map::MapAirport& departure, const map::MapAirport& destination,
                                      proc::MapProcedureLegs& approachLegs, proc::MapProcedureLegs& starLegs,
                                      proc::MapProcedureLegs& sidLegs, QStringList& errors, bool autoresolveTransition);

  /* Get the assigned runway of a user defined departure. Empty if real SID. */
  static QString getCustomDepartureRunway(const QHash<QString, QString>& properties);

  /* Get the assigned runway of a user defined approach. Empty if real approach. */
  static QString getCustomApprochRunway(const QHash<QString, QString>& properties);

  /* Get dot-separated SID/STAR and the respective transition from the properties */
  static QString getSidAndTransition(const QHash<QString, QString>& properties, QString& runway);
  static QString getStarAndTransition(const QHash<QString, QString>& properties, QString& runway);

  /* Get ARINC format of approach and transition. Dot separated. */
  static QString getApproachAndTransitionArinc(const QHash<QString, QString>& properties);
  static void getApproachAndTransitionDisplay(const QHash<QString, QString>& properties, QString& approach, QString& arinc,
                                              QString& transition, QString& runway);

  /* Populate the property list for given procedures */
  static void fillFlightplanProcedureProperties(QHash<QString, QString>& properties, const proc::MapProcedureLegs& approachLegs,
                                                const proc::MapProcedureLegs& starLegs, const proc::MapProcedureLegs& sidLegs);

  /* Check if structs are filled according to properties. If a struct is empty return flag for missing (not resolved) procedure */
  static proc::MapProcedureTypes getMissingProcedures(QHash<QString, QString>& properties, const proc::MapProcedureLegs& procedureLegs,
                                                      const proc::MapProcedureLegs& starLegs, const proc::MapProcedureLegs& sidLegs);

  /* Removes properties from the given map based on given types */
  static void clearFlightplanProcedureProperties(QHash<QString, QString>& properties, const proc::MapProcedureTypes& type);

  /* Get SID id optionally looking for runway. Returns -1 if not found.  */
  int getSidId(map::MapAirport departure, const QString& sid, const QString& runway = QString(), bool strict = false);

  /* Get a transition for a SID */
  int getSidTransitionId(map::MapAirport departure, const QString& sidTrans, int sidId, bool strict = false);

  /* Get a SID transition by last known waypoint ident. */
  int getSidTransitionIdByWp(map::MapAirport departure, const QString& transWaypoint, int sidId, bool strict = false);

  /* Get STAR id optionally looking for runway. Returns -1 if not found.  */
  int getStarId(map::MapAirport destination, const QString& star, const QString& runway = QString(), bool strict = false);

  /* Get a transition for a STAR */
  int getStarTransitionId(map::MapAirport destination, const QString& starTrans, int starId, bool strict = false);

  /* Get a STAR or approach transition by first known waypoint ident. */
  int getApprOrStarTransitionIdByWp(map::MapAirport destination, const QString& transWaypoint, int starId, bool strict = false);

  /* Get approach id by ARINC name optionally looking for runway. Returns -1 if not found.  */
  int getApproachId(map::MapAirport destination, const QString& arincName, const QString& runway);

  /* Get a transition only for an approach */
  int getTransitionId(map::MapAirport destination, const QString& fixIdent, const QString& type, int approachId);

  /* Creates a user defined approach procedure. Handled like an approach procedure. */
  void createCustomApproach(proc::MapProcedureLegs& procedure, const map::MapAirport& airport, const QString& runwayEnd,
                            float finalLegDistance, float entryAltitude, float offsetAngle);
  void createCustomApproach(proc::MapProcedureLegs& procedure, const map::MapAirport& airportSim,
                            const map::MapRunwayEnd& runwayEndSim, float finalLegDistance, float entryAltitude, float offsetAngle);

  /* Creates a user defined departure procedure. Handled like a SID. */
  void createCustomDeparture(proc::MapProcedureLegs& procedure, const map::MapAirport& airport, const QString& runwayEnd, float distance);
  void createCustomDeparture(proc::MapProcedureLegs& procedure, const map::MapAirport& airportSim,
                             const map::MapRunwayEnd& runwayEndSim, float distance);

  /* Flush the cache to update units */
  void clearCache();

  /* Create all queries */
  void initQueries();

  /* Delete all queries */
  void deInitQueries();

  /* Change procedure to insert runway from flight plan as departure or start for arinc names "ALL" or "RW10B".
   * Only for SID or STAR.
   *  Should only be used on a copy of a procedure object and not the cached object.*/
  void insertSidStarRunway(proc::MapProcedureLegs& legs, const QString& runway);

  /* Stitch manual legs between either STAR and airport or STAR and procedure together.
   * This will modify the procedures.*/
  void postProcessLegsForRoute(proc::MapProcedureLegs& starLegs, const proc::MapProcedureLegs& procedureLegs,
                               const map::MapAirport& airport);

  static bool doesRunwayMatchSidOrStar(const proc::MapProcedureLegs& procedure, const QString& runway);

private:
  proc::MapProcedureLeg buildTransitionLegEntry(const map::MapAirport& airport);
  proc::MapProcedureLeg buildProcedureLegEntry(const map::MapAirport& airport);
  void buildLegEntry(atools::sql::SqlQuery *query, proc::MapProcedureLeg& leg, const map::MapAirport& airport);

  /* See comments in postProcessLegs about the steps below */
  void postProcessLegs(const map::MapAirport& airport, proc::MapProcedureLegs& legs, bool addArtificialLegs) const;
  void processLegs(proc::MapProcedureLegs& legs) const;
  void processLegErrors(proc::MapProcedureLegs& legs) const;
  void processAltRestrictions(proc::MapProcedureLegs& procedure) const;
  void processLegsFafAndFacf(proc::MapProcedureLegs& legs) const;

  /* Fill the courese and heading to intercept legs after all other lines are calculated */
  void processCourseInterceptLegs(proc::MapProcedureLegs& legs) const;

  /* Fill calculatedDistance, geometry from line and calculated course fields */
  void processLegsDistanceAndCourse(proc::MapProcedureLegs& legs) const;

  /* Add an artificial (not in the database) runway leg if no connection to the end is given */
  void processArtificialLegs(const map::MapAirport& airport, proc::MapProcedureLegs& legs,
                             bool addArtificialLegs) const;

  /* Adjust conflicting altitude restrictions where a transition ends with "A2000" and is the same as the following
   * initial fix having "2000". Also corrects final altitude restriction if below airport. */
  void processLegsFixRestrictions(const map::MapAirport& airport, proc::MapProcedureLegs& legs) const;

  /* Assign magnetic variation from the navaids */
  void updateMagvar(const map::MapAirport& airport, proc::MapProcedureLegs& legs) const;
  void updateBounding(proc::MapProcedureLegs& legs) const;

  void assignType(proc::MapProcedureLegs& procedure) const;

  /* Check if procedure has hard errors. Fills error list if any and resets id to -1*/
  bool procedureValid(const proc::MapProcedureLegs *legs, QStringList *errors);

  /* Create artificial legs, i.e. legs which are not official ones */
  proc::MapProcedureLeg createRunwayLeg(const proc::MapProcedureLeg& leg, const proc::MapProcedureLegs& legs) const;
  proc::MapProcedureLeg createStartLeg(const proc::MapProcedureLeg& leg, const proc::MapProcedureLegs& legs,
                                       const QStringList& displayText) const;

  proc::MapProcedureLegs *buildProcedureLegs(const map::MapAirport& airport, int procedureId);
  proc::MapProcedureLegs *fetchProcedureLegs(const map::MapAirport& airport, int procedureId);
  proc::MapProcedureLegs *fetchTransitionLegs(const map::MapAirport& airport, int procedureId, int transitionId);
  int procedureIdForTransitionId(int transitionId);
  void mapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident, const QString& region, const QString& airport,
                        const atools::geo::Pos& sortByDistancePos);

  int findTransitionId(const map::MapAirport& airport, atools::sql::SqlQuery *query, bool strict);
  int findProcedureId(const map::MapAirport& airport, atools::sql::SqlQuery *query, const QString& suffix, const QString& runway,
                      bool strict);
  int findProcedureLegId(const map::MapAirport& airport, atools::sql::SqlQuery *query, const QString& suffix, const QString& runway,
                         bool transition, bool strict);

  /* Get runway end and try lower and higher numbers if nothing was found - adds a dummy entry with airport
   * position if no runway ends were found */
  void runwayEndByName(map::MapResult& result, const QString& name, const map::MapAirport& airport);
  void runwayEndByNameSim(map::MapResult& result, const QString& name, const map::MapAirport& airport);

  /* Check if a runway matches an SID/STAR "ALL" or e.g. "RW10B" pattern or matches exactly */
  static bool doesRunwayMatch(const QString& runway, const QString& runwayFromQuery, const QString& arincName,
                              const QStringList& airportRunways, bool matchEmptyRunway);

  /* Check if a runway matches an SID/STAR "ALL" or e.g. "RW10B" pattern - otherwise false */
  static bool doesSidStarRunwayMatch(const QString& runway, const QString& arincName, const QStringList& airportRunways);

  /* Get first runway of an airport which matches an SID/STAR "ALL" or e.g. "RW10B" pattern. */
  static QString anyMatchingRunwayForSidStar(const QString& arincName, const QStringList& airportRunways);

  QString runwayErrorString(const QString& runway);

  atools::sql::SqlDatabase *dbNav;
  atools::sql::SqlQuery *procedureLegQuery = nullptr, *transitionLegQuery = nullptr,
                        *transitionIdForLegQuery = nullptr, *procedureIdForTransQuery = nullptr,
                        *runwayEndIdQuery = nullptr, *transitionQuery = nullptr, *procedureQuery = nullptr,
                        *transitionIdByNameQuery = nullptr, *sidTransIdByWpQuery = nullptr, *starTransIdByWpQuery = nullptr,
                        *procedureIdByNameQuery = nullptr, *procedureIdByArincNameQuery = nullptr,
                        *transitionIdsForProcedureQuery = nullptr;

  /* approach ID and transition ID to full lists
   * The procedure also has to be stored for transitions since the handover can modify procedure legs (CI legs, etc.) */
  QCache<int, proc::MapProcedureLegs> procedureCache, transitionCache;

  /* maps leg ID to procedure/transition ID and index in list */
  QHash<int, std::pair<int, int> > procedureLegIndex, transitionLegIndex;

  AirportQuery *airportQueryNav = nullptr;

  /* Dummy used for custom approaches. */
  Q_DECL_CONSTEXPR static int CUSTOM_APPROACH_ID = 1000000000;
  Q_DECL_CONSTEXPR static int CUSTOM_DEPARTURE_ID = 1000000001;

  /* Use this value as an id base for the artifical vector legs. */
  Q_DECL_CONSTEXPR static int VECTOR_LEG_ID_BASE = 1250000000;

  /* Base id for artificial transition/procedure connections */
  Q_DECL_CONSTEXPR static int TRANS_CONNECT_LEG_ID_BASE = 1000000000;

  /* Use this value as an id base for the artifical runway legs. Add id of the predecessor to it to be able to find the
   * leg again */
  Q_DECL_CONSTEXPR static int RUNWAY_LEG_ID_BASE = 750000000;

  /* Base id for artificial start legs */
  Q_DECL_CONSTEXPR static int START_LEG_ID_BASE = 500000000;
};

#endif // LITTLENAVMAP_PROCEDUREQUERY_H
