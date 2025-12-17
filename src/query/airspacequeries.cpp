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

#include "airspacequeries.h"

#include "query/airspacequery.h"

AirspaceQueries::AirspaceQueries(atools::sql::SqlDatabase *dbSim, atools::sql::SqlDatabase *dbNav, atools::sql::SqlDatabase *dbUser,
                                 atools::sql::SqlDatabase *dbOnline)
{
  // Create all query objects =================================
  if(dbSim != nullptr)
    airspaceQueries.insert(map::AIRSPACE_SRC_SIM, new AirspaceQuery(dbSim, map::AIRSPACE_SRC_SIM));

  if(dbNav != nullptr)
    airspaceQueries.insert(map::AIRSPACE_SRC_NAV, new AirspaceQuery(dbNav, map::AIRSPACE_SRC_NAV));

  if(dbUser != nullptr)
    airspaceQueries.insert(map::AIRSPACE_SRC_USER, new AirspaceQuery(dbUser, map::AIRSPACE_SRC_USER));

  if(dbOnline != nullptr)
    airspaceQueries.insert(map::AIRSPACE_SRC_ONLINE, new AirspaceQuery(dbOnline, map::AIRSPACE_SRC_ONLINE));
}

AirspaceQueries::~AirspaceQueries()
{
  qDeleteAll(airspaceQueries);
  airspaceQueries.clear();
}

void AirspaceQueries::initQueries()
{
  for(AirspaceQuery *query : std::as_const(airspaceQueries))
    query->initQueries();
}

void AirspaceQueries::deInitQueries()
{
  for(AirspaceQuery *query : std::as_const(airspaceQueries))
    query->deInitQueries();
}

void AirspaceQueries::onlineClientAndAtcUpdated()
{
  if(airspaceQueries.contains(map::AIRSPACE_SRC_ONLINE))
  {
    airspaceQueries.value(map::AIRSPACE_SRC_ONLINE)->clearCache();
    airspaceQueries.value(map::AIRSPACE_SRC_ONLINE)->updateAirspaceStatus();
  }
}

void AirspaceQueries::resetAirspaceOnlineScreenGeometry()
{
  if(airspaceQueries.contains(map::AIRSPACE_SRC_ONLINE))
  {
    airspaceQueries.value(map::AIRSPACE_SRC_ONLINE)->deInitQueries();
    airspaceQueries.value(map::AIRSPACE_SRC_ONLINE)->initQueries();
    airspaceQueries.value(map::AIRSPACE_SRC_ONLINE)->updateAirspaceStatus();
  }
}

void AirspaceQueries::getAirspaceById(map::MapAirspace& airspace, map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return;

  AirspaceQuery *query = airspaceQueries.value(id.src);
  if(query != nullptr)
    query->getAirspaceById(airspace, id.id);
}

map::MapAirspace AirspaceQueries::getAirspaceById(map::MapAirspaceId id)
{
  map::MapAirspace airspace;
  getAirspaceById(airspace, id);
  return airspace;
}

bool AirspaceQueries::hasAirspaceById(map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return false;

  AirspaceQuery *query = airspaceQueries.value(id.src);
  if(query != nullptr)
    return query->hasAirspaceById(id.id);

  return false;
}

atools::sql::SqlRecord AirspaceQueries::getOnlineAirspaceRecordById(int airspaceId)
{
  // Only for online centers
  AirspaceQuery *query = airspaceQueries.value(map::AIRSPACE_SRC_ONLINE);
  if(query != nullptr)
    return query->getOnlineAirspaceRecordById(airspaceId);
  else
    return atools::sql::SqlRecord();
}

atools::sql::SqlRecord AirspaceQueries::getAirspaceInfoRecordById(map::MapAirspaceId id)
{
  atools::sql::SqlRecord rec;

  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return rec;

  if(!(id.src & map::AIRSPACE_SRC_ONLINE))
  {
    AirspaceQuery *query = airspaceQueries.value(id.src);
    if(query != nullptr)
      return query->getAirspaceInfoRecordById(id.id);
  }

  return rec;
}

void AirspaceQueries::getAirspacesInternal(AirspaceList& airspaceVector, const Marble::GeoDataLatLonBox& rect,
                                           const MapLayer *mapLayer, const map::MapAirspaceFilter& filter, float flightplanAltitude,
                                           bool lazy, map::MapAirspaceSources src, bool& overflow)
{
  if((src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return;

  // Check if requested source and enabled sources overlap
  if(src & sources)
  {
    AirspaceQuery *airspaceQuery = airspaceQueries.value(src);
    if(airspaceQuery != nullptr)
    {
      // Get airspaces from cache
      const QList<map::MapAirspace> *airspaces = airspaceQuery->getAirspaces(rect, mapLayer, filter, flightplanAltitude, lazy, overflow);

      if(airspaces != nullptr)
      {
        // Append pointers
        for(const map::MapAirspace& airspace : *airspaces)
          airspaceVector.append(&airspace);
      }
    }
  }
}

void AirspaceQueries::getAirspaces(AirspaceList& airspaces, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                   const map::MapAirspaceFilter& filter, float flightplanAltitude, bool lazy,
                                   map::MapAirspaceSources sourcesParam, bool& overflow)
{
  // Merge airspace pointers from all sources/caches into one list
  for(map::MapAirspaceSource src : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if(sourcesParam & src)
      getAirspacesInternal(airspaces, rect, mapLayer, filter, flightplanAltitude, lazy, src, overflow);

    if(overflow)
      break;
  }
}

const atools::geo::LineString *AirspaceQueries::getAirspaceGeometry(map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return nullptr;

  AirspaceQuery *query = airspaceQueries.value(id.src);
  if(query != nullptr)
    return query->getAirspaceGeometryById(id.id);

  return nullptr;
}

bool AirspaceQueries::hasAnyAirspaces() const
{
  for(map::MapAirspaceSource src : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if((sources & src) && airspaceQueries.contains(src) && airspaceQueries.value(src)->hasAirspacesDatabase())
      return true;
  }
  return false;
}

void AirspaceQueries::preLoadAirspaces()
{
  setLoadingUserAirspaces();
  if(airspaceQueries.contains(map::AIRSPACE_SRC_USER))
    airspaceQueries.value(map::AIRSPACE_SRC_USER)->deInitQueries();
}

void AirspaceQueries::postLoadAirspaces()
{
  if(airspaceQueries.contains(map::AIRSPACE_SRC_USER))
    airspaceQueries.value(map::AIRSPACE_SRC_USER)->initQueries();
  setLoadingUserAirspaces(false);
}

const atools::geo::LineString *AirspaceQueries::getOnlineAirspaceGeoByFile(const QString& callsign)
{
  // Avoid deadlock while loading user airspaces
  if(!loadingUserAirspaces)
  {
    AirspaceQuery *query = airspaceQueries.value(map::AIRSPACE_SRC_USER);
    if(query != nullptr)
      return query->getAirspaceGeometryByFile(callsign);
  }
  return nullptr;
}

const atools::geo::LineString *AirspaceQueries::getOnlineAirspaceGeoByName(const QString& callsign, const QString& facilityType)
{
  if(!loadingUserAirspaces)
  {
    AirspaceQuery *query = airspaceQueries.value(map::AIRSPACE_SRC_USER);
    if(query != nullptr)
      return query->getAirspaceGeometryByName(callsign, facilityType);
  }
  return nullptr;
}

QStringList AirspaceQueries::getAirspaceSourcesStr() const
{
  QStringList retval;
  for(map::MapAirspaceSource src : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if(sources & src)
      retval.append(map::airspaceSourceText(src));
  }

  return retval;
}
