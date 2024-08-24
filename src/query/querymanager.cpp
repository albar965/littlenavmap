/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "query/querymanager.h"

#include "app/navapp.h"
#include "atools.h"
#include "db/databasemanager.h"
#include "query/airportquery.h"
#include "query/airspacequeries.h"
#include "query/airwayquery.h"
#include "query/airwaytrackquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "query/waypointquery.h"
#include "query/waypointtrackquery.h"

QueryManager *QueryManager::queryManagerInstance = nullptr;

void QueryManager::initQueries()
{
  if(queriesGui != nullptr)
    queriesGui->initQueries();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->initQueries();
  }
}

void QueryManager::deInitQueries()
{
  if(queriesGui != nullptr)
    queriesGui->deInitQueries();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->deInitQueries();
  }
}

void QueryManager::preTrackLoad()
{
  if(queriesGui != nullptr)
    queriesGui->preTrackLoad();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->preTrackLoad();
  }
}

void QueryManager::postTrackLoad()
{
  if(queriesGui != nullptr)
    queriesGui->postTrackLoad();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->postTrackLoad();
  }
}

void QueryManager::preLoadAirspaces()
{
  if(queriesGui != nullptr)
    queriesGui->preLoadAirspaces();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->preLoadAirspaces();
  }
}

void QueryManager::postLoadAirspaces()
{
  if(queriesGui != nullptr)
    queriesGui->postLoadAirspaces();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->postLoadAirspaces();
  }
}

void QueryManager::preDatabaseLoad()
{
  if(queriesGui != nullptr)
    queriesGui->preDatabaseLoad();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->preDatabaseLoad();
  }
}

void QueryManager::postDatabaseLoad()
{
  if(queriesGui != nullptr)
    queriesGui->postDatabaseLoad();

  if(queriesWeb != nullptr)
  {
    QueryLocker locker(queriesWeb);
    queriesWeb->postDatabaseLoad();
  }
}

Queries *QueryManager::getQueriesGui()
{
  if(queriesGui == nullptr)
  {
    queriesGui = new Queries();
    queriesGui->initQueries();
  }

  return queriesGui;
}

Queries *QueryManager::getQueriesWeb()
{
  QMutexLocker locker(&mutexWebQueries);

  if(queriesWeb == nullptr)
  {
    queriesWeb = new Queries();
    queriesWeb->initQueries();
  }

  return queriesWeb;
}

void QueryManager::shutdown()
{
  ATOOLS_DELETE_LOG(queriesGui);
  ATOOLS_DELETE_LOG(queriesWeb);
}

QueryManager::QueryManager()
{
}

// ==============================================================================================
Queries::Queries()
{
  DatabaseManager *db = NavApp::getDatabaseManager();

  airportQuerySim = new AirportQuery(db->getDatabaseSim(), this, false /* nav */);
  airportQueryNav = new AirportQuery(db->getDatabaseNav(), this, true /* nav */);
  mapQuery = new MapQuery(db->getDatabaseSim(), db->getDatabaseNav(), db->getDatabaseUser(), this);

  airwayTrackQuery = new AirwayTrackQuery(new AirwayQuery(db->getDatabaseNav(), false /* track */),
                                          new AirwayQuery(db->getDatabaseTrack(), true /* track */));

  waypointTrackQuery = new WaypointTrackQuery(new WaypointQuery(db->getDatabaseNav(), false /* track */),
                                              new WaypointQuery(db->getDatabaseTrack(), true /* track */));

  infoQuery = new InfoQuery(db->getDatabaseSim(), db->getDatabaseNav(), db->getDatabaseTrack());
  procedureQuery = new ProcedureQuery(db->getDatabaseNav(), this);

  airspaceQueries = new AirspaceQueries(db->getDatabaseSim(), db->getDatabaseNav(), db->getDatabaseUserAirspace(),
                                        db->getDatabaseOnline());
}

Queries::~Queries()
{
  ATOOLS_DELETE_LOG(airspaceQueries); //

  ATOOLS_DELETE_LOG(airportQuerySim);
  ATOOLS_DELETE_LOG(airportQueryNav);
  ATOOLS_DELETE_LOG(mapQuery);

  airwayTrackQuery->deleteChildren();
  ATOOLS_DELETE_LOG(airwayTrackQuery);

  waypointTrackQuery->deleteChildren();
  ATOOLS_DELETE_LOG(waypointTrackQuery);

  ATOOLS_DELETE_LOG(infoQuery);
  ATOOLS_DELETE_LOG(procedureQuery);
}

AirwayTrackQuery *Queries::getClonedAirwayTrackQuery() const
{
  return new AirwayTrackQuery(*getAirwayTrackQuery());
}

WaypointTrackQuery *Queries::getClonedWaypointTrackQuery() const
{
  return new WaypointTrackQuery(*getWaypointTrackQuery());
}

void Queries::releaseClonedAirwayTrackQuery(AirwayTrackQuery *query) const
{
  ATOOLS_DELETE_LOG(query);
}

void Queries::releaseClonedWaypointTrackQuery(WaypointTrackQuery *query) const
{
  ATOOLS_DELETE_LOG(query);
}

bool Queries::isHoldingsAvailable() const
{
  return getMapQuery()->hasHoldings();
}

bool Queries::isAirportMsaAvailable() const
{
  return getMapQuery()->hasAirportMsa();
}

bool Queries::isGlsAvailable() const
{
  return getMapQuery()->hasGls();
}

void Queries::preTrackLoad()
{

}

void Queries::postTrackLoad()
{
  if(waypointTrackQuery != nullptr)
  {
    waypointTrackQuery->clearCache();
    waypointTrackQuery->initQueries();
  }

  if(airwayTrackQuery != nullptr)
  {
    airwayTrackQuery->clearCache();
    airwayTrackQuery->initQueries();
  }
}

void Queries::preLoadAirspaces()
{
  airspaceQueries->preLoadAirspaces();
}

void Queries::postLoadAirspaces()
{
  airspaceQueries->postLoadAirspaces();
}

void Queries::preDatabaseLoad()
{
  deInitQueries();
}

void Queries::postDatabaseLoad()
{
  initQueries();
}

void Queries::initQueries()
{
  if(airportQueryNav != nullptr)
    airportQueryNav->initQueries();

  if(airportQuerySim != nullptr)
    airportQuerySim->initQueries();

  if(mapQuery != nullptr)
    mapQuery->initQueries();

  if(airwayTrackQuery != nullptr)
    airwayTrackQuery->initQueries();

  if(waypointTrackQuery != nullptr)
    waypointTrackQuery->initQueries();

  if(infoQuery != nullptr)
    infoQuery->initQueries();

  if(procedureQuery != nullptr)
    procedureQuery->initQueries();

  if(airspaceQueries != nullptr)
    airspaceQueries->initQueries();
}

void Queries::deInitQueries()
{
  if(airportQueryNav != nullptr)
    airportQueryNav->deInitQueries();

  if(airportQuerySim != nullptr)
    airportQuerySim->deInitQueries();

  if(mapQuery != nullptr)
    mapQuery->deInitQueries();

  if(airwayTrackQuery != nullptr)
    airwayTrackQuery->deInitQueries();

  if(waypointTrackQuery != nullptr)
    waypointTrackQuery->deInitQueries();

  if(infoQuery != nullptr)
    infoQuery->deInitQueries();

  if(procedureQuery != nullptr)
    procedureQuery->deInitQueries();

  if(airspaceQueries != nullptr)
    airspaceQueries->deInitQueries();
}
