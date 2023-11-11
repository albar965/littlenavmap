/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "query/waypointtrackquery.h"

#include "common/maptools.h"
#include "query/waypointquery.h"
#include "mapgui/maplayer.h"

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;
using map::MapWaypoint;

WaypointTrackQuery::WaypointTrackQuery(WaypointQuery *waypointQueryParam, WaypointQuery *trackQueryParam)
  : waypointQuery(waypointQueryParam), trackQuery(trackQueryParam)
{
}

WaypointTrackQuery::~WaypointTrackQuery()
{
}

map::MapWaypoint WaypointTrackQuery::getWaypointById(int id)
{
  map::MapWaypoint waypoint;
  if(useTracks)
    waypoint = trackQuery->getWaypointById(id);

  if(!waypoint.isValid())
    waypoint = waypointQuery->getWaypointById(id);

  return waypoint;
}

MapWaypoint WaypointTrackQuery::getWaypointByNavId(int navId, map::MapType type)
{
  map::MapWaypoint waypoint;
  if(useTracks)
    waypoint = trackQuery->getWaypointByNavId(navId, type);

  if(!waypoint.isValid())
    waypoint = waypointQuery->getWaypointByNavId(navId, type);

  return waypoint;
}

void WaypointTrackQuery::getWaypointByIdent(QList<map::MapWaypoint>& waypoints, const QString& ident, const QString& region)
{
  if(useTracks)
    trackQuery->getWaypointByByIdent(waypoints, ident, region);

  QList<map::MapWaypoint> navWaypoints;
  waypointQuery->getWaypointByByIdent(navWaypoints, ident, region);
  copy(navWaypoints, waypoints);
}

void WaypointTrackQuery::getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer,
                                                 map::MapTypes types, int xs,
                                                 int ys, int screenDistance, map::MapResult& result)
{
  if(useTracks)
    trackQuery->getNearestScreenObjects(conv, mapLayer, types, xs, ys, screenDistance, result);

  waypointQuery->getNearestScreenObjects(conv, mapLayer, types, xs, ys, screenDistance, result);
}

void WaypointTrackQuery::getWaypointNearest(map::MapWaypoint& waypoint, const Pos& pos)
{
  if(useTracks)
    trackQuery->getWaypointNearest(waypoint, pos);

  if(!waypoint.isValid())
    waypointQuery->getWaypointNearest(waypoint, pos);
}

void WaypointTrackQuery::getWaypointsRect(QVector<map::MapWaypoint>& waypoints, const Pos& pos, float distanceNm)
{
  if(useTracks)
    trackQuery->getWaypointsRect(waypoints, pos, distanceNm);

  QVector<map::MapWaypoint> navWaypoints;
  waypointQuery->getWaypointsRect(navWaypoints, pos, distanceNm);
  copy(navWaypoints, waypoints);

  maptools::sortByDistance(waypoints, pos);
}

void WaypointTrackQuery::getWaypointRectNearest(map::MapWaypoint& waypoint, const Pos& pos, float distanceNm)
{
  if(useTracks)
    trackQuery->getWaypointRectNearest(waypoint, pos, distanceNm);

  if(!waypoint.isValid())
    waypointQuery->getWaypointRectNearest(waypoint, pos, distanceNm);
}

void WaypointTrackQuery::getWaypoints(QList<map::MapWaypoint>& waypoints, const GeoDataLatLonBox& rect,
                                      const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  const QList<map::MapWaypoint> *wp;
  if(useTracks && mapLayer->isTrackWaypoint())
  {
    wp = trackQuery->getWaypoints(rect, mapLayer, lazy, overflow);
    if(wp != nullptr)
      waypoints.append(*wp);
  }

  if(!overflow && mapLayer->isAirwayWaypoint())
  {
    wp = waypointQuery->getWaypoints(rect, mapLayer, lazy, overflow);
    if(wp != nullptr)
      copy(*wp, waypoints);
  }
}

void WaypointTrackQuery::getWaypointsAirway(QList<map::MapWaypoint>& waypoints, const Marble::GeoDataLatLonBox& rect,
                                            const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  const QList<map::MapWaypoint> *wp;
  if(useTracks && mapLayer->isTrackWaypoint())
  {
    wp = trackQuery->getWaypointsAirway(rect, mapLayer, lazy, overflow);
    if(wp != nullptr)
      waypoints.append(*wp);
  }

  if(!overflow && mapLayer->isAirwayWaypoint())
  {
    wp = waypointQuery->getWaypointsAirway(rect, mapLayer, lazy, overflow);
    if(wp != nullptr)
      copy(*wp, waypoints);
  }
}

const QList<map::MapWaypoint> WaypointTrackQuery::getWaypointsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer,
                                                                     bool lazy, bool& overflow)
{
  const GeoDataLatLonBox latLonBox = GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(),
                                                      rect.getWest(), GeoDataCoordinates::Degree);
  QList<map::MapWaypoint> waypoints = QList<map::MapWaypoint>();
  getWaypoints(waypoints, latLonBox, mapLayer, lazy, overflow);
  return waypoints;
}

const SqlRecord *WaypointTrackQuery::getWaypointInformation(int waypointId)
{
  const SqlRecord *rec = waypointQuery->getWaypointInformation(waypointId);

  if(useTracks && (rec == nullptr || rec->isEmpty()))
    rec = trackQuery->getWaypointInformation(waypointId);

  return rec;
}

void WaypointTrackQuery::initQueries()
{
  trackQuery->initQueries();
  waypointQuery->initQueries();
}

void WaypointTrackQuery::deInitQueries()
{
  trackQuery->deInitQueries();
  waypointQuery->deInitQueries();
}

void WaypointTrackQuery::clearCache()
{
  trackQuery->clearCache();
}

void WaypointTrackQuery::deleteChildren()
{
  ATOOLS_DELETE(trackQuery);
  ATOOLS_DELETE(waypointQuery);
}

SqlQuery *WaypointTrackQuery::getWaypointsByRectQuery() const
{
  return waypointQuery->getWaypointsByRectQuery();
}

SqlQuery *WaypointTrackQuery::getWaypointsByRectQueryTrack() const
{
  return trackQuery->getWaypointsByRectQuery();
}

void WaypointTrackQuery::copy(const QList<map::MapWaypoint>& from, QList<map::MapWaypoint>& to)
{
  for(const map::MapWaypoint& w : from)
  {
    if(!to.contains(w))
      to.append(w);
  }
}

void WaypointTrackQuery::copy(const QVector<map::MapWaypoint>& from, QVector<map::MapWaypoint>& to)
{
  for(const map::MapWaypoint& w : from)
  {
    if(!to.contains(w))
      to.append(w);
  }
}
