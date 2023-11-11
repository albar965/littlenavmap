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

#include "query/waypointquery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "common/mapresult.h"
#include "common/maptools.h"
#include "mapgui/maplayer.h"
#include "settings/settings.h"
#include "sql/sqlutil.h"

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;
using map::MapWaypoint;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int WaypointQuery::queryMaxRowsWaypoints = map::MAX_MAP_OBJECTS;

WaypointQuery::WaypointQuery(SqlDatabase *sqlDbNav, bool trackDatabaseParam)
  : dbNav(sqlDbNav), trackDatabase(trackDatabaseParam)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  queryRectInflationFactor = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRowsWaypoints = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "WaypointQueryRowLimit1", map::MAX_MAP_OBJECTS * 2).toInt();
  waypointInfoCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_INFOQUERY + "WaypointCache", 100).toInt());
}

WaypointQuery::~WaypointQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

map::MapWaypoint WaypointQuery::getWaypointById(int id)
{
  map::MapWaypoint wp;

  if(!query::valid(Q_FUNC_INFO, waypointByIdQuery))
    return wp;

  waypointByIdQuery->bindValue(":id", id);
  waypointByIdQuery->exec();
  if(waypointByIdQuery->next())
    mapTypesFactory->fillWaypoint(waypointByIdQuery->record(), wp, trackDatabase);
  waypointByIdQuery->finish();
  return wp;
}

MapWaypoint WaypointQuery::getWaypointByNavId(int navId, map::MapType type)
{
  map::MapWaypoint wp;
  if(!query::valid(Q_FUNC_INFO, waypointByNavIdQuery))
    return wp;

  waypointByNavIdQuery->bindValue(":id", navId);

  if(type == map::VOR)
    waypointByNavIdQuery->bindValue(":type", "V");
  else if(type == map::NDB)
    waypointByNavIdQuery->bindValue(":type", "N");
  else
    waypointByNavIdQuery->bindValue(":type", "%");

  waypointByNavIdQuery->exec();
  if(waypointByNavIdQuery->next())
    mapTypesFactory->fillWaypoint(waypointByNavIdQuery->record(), wp, trackDatabase);
  waypointByNavIdQuery->finish();
  return wp;
}

void WaypointQuery::getWaypointByByIdent(QList<map::MapWaypoint>& waypoints, const QString& ident,
                                         const QString& region)
{
  if(!query::valid(Q_FUNC_INFO, waypointByIdentQuery))
    return;

  waypointByIdentQuery->bindValue(":ident", ident);
  waypointByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
  waypointByIdentQuery->exec();
  while(waypointByIdentQuery->next())
  {
    map::MapWaypoint wp;
    mapTypesFactory->fillWaypoint(waypointByIdentQuery->record(), wp, trackDatabase);
    waypoints.append(wp);
  }
}

void WaypointQuery::getWaypointNearest(map::MapWaypoint& waypoint, const Pos& pos)
{
  if(!query::valid(Q_FUNC_INFO, waypointNearestQuery))
    return;

  waypointNearestQuery->bindValue(":lonx", pos.getLonX());
  waypointNearestQuery->bindValue(":laty", pos.getLatY());
  waypointNearestQuery->exec();
  if(waypointNearestQuery->next())
    mapTypesFactory->fillWaypoint(waypointNearestQuery->record(), waypoint, trackDatabase);
  waypointNearestQuery->finish();
}

void WaypointQuery::getWaypointsRect(QVector<map::MapWaypoint>& waypoints, const Pos& pos, float distanceNm)
{
  if(!query::valid(Q_FUNC_INFO, waypointRectQuery))
    return;

  for(const Rect& r : Rect(pos, nmToMeter(distanceNm), true /* fast */).splitAtAntiMeridian())
  {
    query::bindRect(r, waypointRectQuery);
    waypointRectQuery->exec();
    while(waypointRectQuery->next())
    {
      map::MapWaypoint waypoint;
      mapTypesFactory->fillWaypoint(waypointRectQuery->record(), waypoint, trackDatabase);
      waypoints.append(waypoint);
    }
  }

  maptools::sortByDistance(waypoints, pos);
}

void WaypointQuery::getWaypointRectNearest(map::MapWaypoint& waypoint, const Pos& pos, float distanceNm)
{
  QVector<map::MapWaypoint> waypoints;
  getWaypointsRect(waypoints, pos, distanceNm);
  if(!waypoints.isEmpty())
    waypoint = waypoints.constFirst();
}

const QList<map::MapWaypoint> *WaypointQuery::getWaypoints(const GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                                           bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, waypointsByRectQuery))
    return nullptr;

  waypointCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                            [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersWaypoint(newLayer);
  });

  if(waypointCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, waypointsByRectQuery);
      waypointsByRectQuery->exec();
      while(waypointsByRectQuery->next())
      {
        map::MapWaypoint wp;
        mapTypesFactory->fillWaypoint(waypointsByRectQuery->record(), wp, trackDatabase);

        // Avoid artificial waypoints created only for procedure or airway resolution
        if(wp.artificial == map::WAYPOINT_ARTIFICIAL_NONE)
          waypointCache.list.append(wp);
      }
    }
  }
  overflow = waypointCache.validate(queryMaxRowsWaypoints);
  return &waypointCache.list;
}

const QList<MapWaypoint> *WaypointQuery::getWaypointsAirway(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                                            bool& overflow)
{
  if(!query::valid(Q_FUNC_INFO, waypointsAirwayByRectQuery))
    return nullptr;

  waypointAirwayCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                                  [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersWaypoint(newLayer) && curLayer->hasSameQueryParametersAirwayTrack(newLayer);
  });

  if(waypointAirwayCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, waypointsAirwayByRectQuery);
      waypointsAirwayByRectQuery->exec();
      while(waypointsAirwayByRectQuery->next())
      {
        map::MapWaypoint wp;
        mapTypesFactory->fillWaypoint(waypointsAirwayByRectQuery->record(), wp, trackDatabase);

        // Also insert artificial waypoints
        waypointAirwayCache.list.append(wp);
      }
    }
  }
  overflow = waypointAirwayCache.validate(queryMaxRowsWaypoints);
  return &waypointAirwayCache.list;
}

void WaypointQuery::getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, map::MapTypes types, int xs, int ys,
                                            int screenDistance, map::MapResult& result)
{
  int x, y;

  if(mapLayer->isWaypoint() && types.testFlag(map::WAYPOINT))
  {
    for(int i = waypointCache.list.size() - 1; i >= 0; i--)
    {
      const MapWaypoint& wp = waypointCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          maptools::insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, wp);
    }
  }

  if((!trackDatabase && mapLayer->isAirwayWaypoint() &&
      (types.testFlag(map::AIRWAYV) || types.testFlag(map::AIRWAYJ))) ||
     (trackDatabase && mapLayer->isTrackWaypoint() && types.testFlag(map::TRACK)))
  {
    for(int i = waypointAirwayCache.list.size() - 1; i >= 0; i--)
    {
      const MapWaypoint& wp = waypointAirwayCache.list.at(i);
      if((wp.hasVictorAirways && types.testFlag(map::AIRWAYV)) ||
         (wp.hasJetAirways && types.testFlag(map::AIRWAYJ)) ||
         (wp.hasTracks && types.testFlag(map::TRACK)))
        if(conv.wToS(wp.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            maptools::insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, wp);
    }
  }
}

const atools::sql::SqlRecord *WaypointQuery::getWaypointInformation(int waypointId)
{
  if(!query::valid(Q_FUNC_INFO, waypointInfoQuery))
    return nullptr;

  waypointInfoQuery->bindValue(":id", waypointId);
  return query::cachedRecord(waypointInfoCache, waypointInfoQuery, waypointId);
}

void WaypointQuery::initQueries()
{
  QString table = trackDatabase ? "trackpoint" : "waypoint";
  QString id = trackDatabase ? "trackpoint_id" : "waypoint_id";

  // Common where clauses
  static const QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static const QString whereIdentRegion("ident = :ident and region like :region");
  static const QString whereLimit("limit " + QString::number(queryMaxRowsWaypoints));

  // Common select statements
  QString waypointQueryBase(id + ", ident, region, type, num_victor_airway, num_jet_airway, mag_var, lonx, laty ");

  if(atools::sql::SqlUtil(dbNav).hasTableAndColumn("waypoint", "artificial"))
    waypointQueryBase.append(", artificial");

  if(atools::sql::SqlUtil(dbNav).hasTableAndColumn("waypoint", "arinc_type"))
    waypointQueryBase.append(", arinc_type");

  deInitQueries();

  waypointByIdentQuery = new SqlQuery(dbNav);
  waypointByIdentQuery->prepare("select " + waypointQueryBase + " from " + table + " where " + whereIdentRegion);

  // Get nearest Waypoint
  waypointNearestQuery = new SqlQuery(dbNav);
  waypointNearestQuery->prepare(
    "select " + waypointQueryBase + " from " + table + " order by (abs(lonx - :lonx) + abs(laty - :laty)) limit 1");

  waypointByIdQuery = new SqlQuery(dbNav);
  waypointByIdQuery->prepare("select " + waypointQueryBase + " from " + table + " where " + id + " = :id");

  waypointByNavIdQuery = new SqlQuery(dbNav);
  waypointByNavIdQuery->prepare("select " + waypointQueryBase + " from " + table + " where nav_id = :id and type like :type");

  // Get Waypoint in rect
  waypointRectQuery = new SqlQuery(dbNav);
  waypointRectQuery->prepare("select " + waypointQueryBase + " from " + table + " where " + whereRect + " " + whereLimit);

  waypointsByRectQuery = new SqlQuery(dbNav);
  waypointsByRectQuery->prepare("select " + waypointQueryBase + " from " + table + " where " + whereRect + " " + whereLimit);

  QString airwayCond = trackDatabase ? QString() : "num_victor_airway + num_jet_airway > 0 and";
  waypointsAirwayByRectQuery = new SqlQuery(dbNav);
  waypointsAirwayByRectQuery->prepare("select " + waypointQueryBase + " from " + table +
                                      " where " + airwayCond + " " + whereRect + " " + whereLimit);

  waypointInfoQuery = new SqlQuery(dbNav);

  if(trackDatabase)
    waypointInfoQuery->prepare("select * from " + table + " where " + id + " = :id");
  else
    waypointInfoQuery->prepare("select * from waypoint "
                               "join bgl_file on waypoint.file_id = bgl_file.bgl_file_id "
                               "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                               "where waypoint_id = :id");
}

void WaypointQuery::deInitQueries()
{
  clearCache();

ATOOLS_DELETE(waypointsByRectQuery);
ATOOLS_DELETE(waypointsAirwayByRectQuery);
ATOOLS_DELETE(waypointByIdentQuery);
ATOOLS_DELETE(waypointNearestQuery);
ATOOLS_DELETE(waypointRectQuery);
ATOOLS_DELETE(waypointByIdQuery);
ATOOLS_DELETE(waypointByNavIdQuery);
ATOOLS_DELETE(waypointInfoQuery);
}

void WaypointQuery::clearCache()
{
  waypointCache.clear();
  waypointAirwayCache.clear();
  waypointInfoCache.clear();
}
