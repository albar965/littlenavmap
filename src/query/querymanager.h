/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_QUERYMANAGER_H
#define LNM_QUERYMANAGER_H

#include "util/locker.h"

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class AirspaceQueries;
class AirportQuery;
class AirspaceQuery;
class AirwayTrackQuery;
class InfoQuery;
class MapQuery;
class ProcedureQuery;
class WaypointTrackQuery;

/*
 * Collects the most important scenery library database query classes and
 * allows locking them using atools::util::Locker.
 * User has to take care of locking before using the SQL query classes.
 */
class Queries :
  public Lockable
{
public:
  ~Queries();

  AirportQuery *getAirportQuery(bool navdata) const
  {
    return navdata ? getAirportQueryNav() : getAirportQuerySim();
  }

  AirportQuery *getAirportQueryNav() const
  {
    return airportQueryNav;
  }

  AirportQuery *getAirportQuerySim() const
  {
    return airportQuerySim;
  }

  MapQuery *getMapQuery() const
  {
    return mapQuery;
  }

  AirwayTrackQuery *getAirwayTrackQuery() const
  {
    return airwayTrackQuery;
  }

  WaypointTrackQuery *getWaypointTrackQuery() const
  {
    return waypointTrackQuery;
  }

  InfoQuery *getInfoQuery() const
  {
    return infoQuery;
  }

  ProcedureQuery *getProcedureQuery() const
  {
    return procedureQuery;
  }

  AirspaceQueries *getAirspaceQueries() const
  {
    return airspaceQueries;
  }

  /* Get queries that only delegate to AirwayTrackQuery/getWaypointTrackQuery and allow modification using
   * setUseTracks(). Objects to be released manually. */
  AirwayTrackQuery *getClonedAirwayTrackQuery() const;
  WaypointTrackQuery *getClonedWaypointTrackQuery() const;

  /* Release cloned queries */
  void releaseClonedAirwayTrackQuery(AirwayTrackQuery *query) const;
  void releaseClonedWaypointTrackQuery(WaypointTrackQuery *query) const;

  /* True if table holding is present in schema and has at least one row */
  bool isHoldingsAvailable() const;

  /* True if table airport_msa is present in schema and has at least one row */
  bool isAirportMsaAvailable() const;

  /* True if table ils contains GLS/RNP approaches - GLS ground stations or GBAS threshold points */
  bool isGlsAvailable() const;

private:
  friend class QueryManager;

  /* Creates all contained query classes but does not initialize SQL queries. */
  explicit Queries();

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Initialize and finish track loading */
  void preTrackLoad();
  void postTrackLoad();

  /* Initialize and finish loading of user airspaces */
  void preLoadAirspaces();
  void postLoadAirspaces();

  /* Deinit and init queries on database change */
  void preDatabaseLoad();
  void postDatabaseLoad();

  AirportQuery *airportQueryNav = nullptr, *airportQuerySim = nullptr;
  MapQuery *mapQuery = nullptr;
  AirwayTrackQuery *airwayTrackQuery = nullptr;
  WaypointTrackQuery *waypointTrackQuery = nullptr;
  InfoQuery *infoQuery = nullptr;
  ProcedureQuery *procedureQuery = nullptr;
  AirspaceQueries *airspaceQueries = nullptr;
};

/* Locker to be used on Queries */
typedef Locker<Queries> QueryLocker;

/*
 * Singleton class that manages query objects for Web and GUI interfaces to ease locking of query classes.
 * Only the web query classes are synchronized
 */
class QueryManager
{
public:
  /* Not synchronized. Initialize at program start. */
  static QueryManager *instance()
  {
    return queryManagerInstance == nullptr ? queryManagerInstance = new QueryManager() : queryManagerInstance;
  }

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Initialize and finish track loading */
  void preTrackLoad();
  void postTrackLoad();

  /* Initialize and finish loading of user airspaces */
  void preLoadAirspaces();
  void postLoadAirspaces();

  /* Deinit and init GUI and Web queries on database change */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Not synchronized and to be called from main GUI event handler thread only.
   * Creates and initalizes queries.
   * Used by all user interface elements like search tabs or dialog windows. */
  Queries *getQueriesGui();

  /* Synchronized and can be called from web threads. You have to use QueryLocker in threads before using.
   * Creates and initalizes queries. */
  Queries *getQueriesWeb();

  /* Shutdown for good */
  void shutdown();

private:
  QueryManager();

  Queries *queriesGui = nullptr, /* User interface queries. All accessed from main event loop. No synchronization needed. */
          *queriesWeb = nullptr; /* Web interface queries. Accessed from web threads. Synchronization needed. */

  static QueryManager *queryManagerInstance;
  QMutex mutexWebQueries;
};

#endif // LNM_QUERYMANAGER_H
