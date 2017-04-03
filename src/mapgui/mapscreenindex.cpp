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

#include "mapgui/mapscreenindex.h"

#include "navapp.h"
#include "common/proctypes.h"
#include "route/routecontroller.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapwidget.h"
#include "mapgui/mappaintlayer.h"
#include "mapgui/maplayer.h"
#include "common/maptypes.h"
#include "common/maptools.h"
#include "mapgui/mapquery.h"
#include "common/coordinateconverter.h"
#include "common/constants.h"
#include "settings/settings.h"

using atools::geo::Pos;
using map::MapAirway;

MapScreenIndex::MapScreenIndex(MapWidget *parentWidget, MapQuery *mapQueryParam, MapPaintLayer *mapPaintLayer)
  : mapWidget(parentWidget), mapQuery(mapQueryParam), paintLayer(mapPaintLayer)
{
}

MapScreenIndex::~MapScreenIndex()
{

}

void MapScreenIndex::updateAirspaceScreenGeometry(const Marble::GeoDataLatLonAltBox& curBox)
{
  airspacePolygons.clear();

  CoordinateConverter conv(mapWidget->viewport());
  const MapScale *scale = paintLayer->getMapScale();
  if(scale->isValid())
  {
    const QList<map::MapAirspace> *airspaces = mapQuery->getAirspaces(
      curBox, paintLayer->getMapLayer(), mapWidget->getShownAirspaceTypesByLayer(),
      NavApp::getRoute().getCruisingAltitudeFeet(), false);

    if(airspaces != nullptr)
    {
      for(const map::MapAirspace& airspace : *airspaces)
      {
        Marble::GeoDataLatLonBox airspacebox(airspace.bounding.getNorth(), airspace.bounding.getSouth(),
                                             airspace.bounding.getEast(), airspace.bounding.getWest(),
                                             Marble::GeoDataCoordinates::Degree);

        if(airspacebox.intersects(curBox))
        {
          QPolygon polygon;
          int x, y;
          for(const Pos& pos : airspace.lines)
          {
            conv.wToS(pos, x, y);
            polygon.append(QPoint(x, y));
          }

          polygon = polygon.intersected(QPolygon(mapWidget->geometry()));
          airspacePolygons.append(std::make_pair(airspace.id, polygon));
        }
      }
    }
  }
}

void MapScreenIndex::updateAirwayScreenGeometry(const Marble::GeoDataLatLonAltBox& curBox)
{
  airwayLines.clear();

  CoordinateConverter conv(mapWidget->viewport());
  const MapScale *scale = paintLayer->getMapScale();
  bool showJet = paintLayer->getShownMapObjects().testFlag(map::AIRWAYJ);
  bool showVictor = paintLayer->getShownMapObjects().testFlag(map::AIRWAYV);

  if(scale->isValid() && paintLayer->getMapLayer()->isAirway() && (showJet || showVictor))
  {
    // Airways are visible on map - get them from the cache/database
    const QList<MapAirway> *airways = mapQuery->getAirways(curBox, paintLayer->getMapLayer(), false);
    const QRect& mapGeo = mapWidget->rect();

    for(int i = 0; i < airways->size(); i++)
    {
      const MapAirway& airway = airways->at(i);
      if((airway.type == map::VICTOR && !showVictor) || (airway.type == map::JET && !showJet))
        continue;

      Marble::GeoDataLatLonBox airwaybox(airway.bounding.getNorth(), airway.bounding.getSouth(),
                                         airway.bounding.getEast(), airway.bounding.getWest(),
                                         Marble::GeoDataCoordinates::Degree);

      if(airwaybox.intersects(curBox))
      {
        // Airway segment intersects with view rectangle
        float distanceMeter = airway.from.distanceMeterTo(airway.to);
        // Approximate the needed number of line segments
        float numSegments = std::min(std::max(scale->getPixelIntForMeter(distanceMeter) / 40.f, 2.f), 72.f);
        float step = 1.f / numSegments;

        // Split the segments into smaller lines and add them only if visible
        for(int j = 0; j < numSegments; j++)
        {
          float cur = step * static_cast<float>(j);
          int xs1, ys1, xs2, ys2;
          conv.wToS(airway.from.interpolate(airway.to, distanceMeter, cur), xs1, ys1);
          conv.wToS(airway.from.interpolate(airway.to, distanceMeter, cur + step), xs2, ys2);

          QRect rect(QPoint(xs1, ys1), QPoint(xs2, ys2));
          rect = rect.normalized();
          // Avoid points or flat rectangles (lines)
          rect.adjust(-1, -1, 1, 1);

          if(mapGeo.intersects(rect))
            airwayLines.append(std::make_pair(airway.id, QLine(xs1, ys1, xs2, ys2)));
        }
      }
    }
  }
}

void MapScreenIndex::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  s.setValueVar(lnm::MAP_DISTANCEMARKERS, QVariant::fromValue<QList<map::DistanceMarker> >(distanceMarks));
  s.setValueVar(lnm::MAP_RANGEMARKERS, QVariant::fromValue<QList<map::RangeMarker> >(rangeMarks));
}

void MapScreenIndex::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  distanceMarks = s.valueVar(lnm::MAP_DISTANCEMARKERS).value<QList<map::DistanceMarker> >();
  rangeMarks = s.valueVar(lnm::MAP_RANGEMARKERS).value<QList<map::RangeMarker> >();
}

void MapScreenIndex::updateRouteScreenGeometry()
{
  const Route& route = NavApp::getRoute();

  routeLines.clear();
  routePoints.clear();

  QList<std::pair<int, QPoint> > airportPoints;
  QList<std::pair<int, QPoint> > otherPoints;

  CoordinateConverter conv(mapWidget->viewport());
  const MapScale *scale = paintLayer->getMapScale();
  if(scale->isValid())
  {
    Pos p1;
    const QRect& mapGeo = mapWidget->rect();

    for(int i = 0; i < route.size(); i++)
    {
      const RouteLeg& routeLeg = route.at(i);

      const Pos& p2 = routeLeg.getPosition();
      int x2, y2;
      conv.wToS(p2, x2, y2);

      map::MapObjectTypes type = routeLeg.getMapObjectType();
      if(type == map::AIRPORT && (i == 0 || i == route.size() - 1))
        // Departure or destination airport
        airportPoints.append(std::make_pair(i, QPoint(x2, y2)));
      else if(route.canEditPoint(i))
        otherPoints.append(std::make_pair(i, QPoint(x2, y2)));

      if(!route.canEditLeg(i))
      {
        p1 = p2;
        continue;
      }

      if(p1.isValid())
      {
        float distanceMeter = p2.distanceMeterTo(p1);
        // Approximate the needed number of line segments
        float numSegments = std::min(std::max(scale->getPixelIntForMeter(distanceMeter) / 140.f, 4.f), 288.f);
        float step = 1.f / numSegments;

        // Split the legs into smaller lines and add them only if visible
        for(int j = 0; j < numSegments; j++)
        {
          float cur = step * static_cast<float>(j);
          int xs1, ys1, xs2, ys2;
          conv.wToS(p1.interpolate(p2, distanceMeter, cur), xs1, ys1);
          conv.wToS(p1.interpolate(p2, distanceMeter, cur + step), xs2, ys2);

          QRect rect(QPoint(xs1, ys1), QPoint(xs2, ys2));
          rect = rect.normalized();
          // Avoid points or flat rectangles (lines)
          rect.adjust(-1, -1, 1, 1);

          if(mapGeo.intersects(rect))
            routeLines.append(std::make_pair(i - 1, QLine(xs1, ys1, xs2, ys2)));
        }
      }
      p1 = p2;
    }

    routePoints.append(airportPoints);
    routePoints.append(otherPoints);
  }
}

void MapScreenIndex::getAllNearest(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                   QList<proc::MapProcedurePoint>& procPoints)
{
  CoordinateConverter conv(mapWidget->viewport());
  const MapLayer *mapLayer = paintLayer->getMapLayer();
  const MapLayer *mapLayerEffective = paintLayer->getMapLayerEffective();

  // Check for user aircraft
  result.userAircraft = atools::fs::sc::SimConnectUserAircraft();
  if(paintLayer->getShownMapObjects() & map::AIRCRAFT && mapWidget->isConnected())
  {
    int x, y;
    if(conv.wToS(simData.getUserAircraft().getPosition(), x, y))
      if(atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
        result.userAircraft = simData.getUserAircraft();
  }

  // Check for AI / multiplayer aircraft
  result.aiAircraft.clear();
  if(mapWidget->distance() < 500 && paintLayer->getShownMapObjects() & map::AIRCRAFT_AI &&
     mapWidget->isConnected())
  {
    using maptools::insertSortedByDistance;
    int x, y;
    for(const atools::fs::sc::SimConnectAircraft& obj : simData.getAiAircraft())
    {
      if(mapLayerEffective->isAirportDiagram() || !obj.isOnGround())
        if(conv.wToS(obj.getPosition(), x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
            insertSortedByDistance(conv, result.aiAircraft, nullptr, xs, ys, obj);
    }
  }

  // Airways use a screen coordinate buffer
  getNearestAirways(xs, ys, maxDistance, result);
  getNearestAirspaces(xs, ys, result);

  if(paintLayer->getShownMapObjects().testFlag(map::FLIGHTPLAN))
    // Get copies from flight plan if visible
    NavApp::getRoute().getNearest(conv, xs, ys, maxDistance, result, procPoints, true /* include procs */);

  // Get points of procedure preview
  getNearestProcedureHighlights(xs, ys, maxDistance, result, procPoints);

  // Get copies from highlightMapObjects
  getNearestHighlights(xs, ys, maxDistance, result);

  // Get objects from cache - already present objects will be skipped
  mapQuery->getNearestObjects(conv, mapLayer, mapLayerEffective->isAirportDiagram(),
                              paintLayer->getShownMapObjects() &
                              (map::AIRPORT_ALL | map::VOR | map::NDB | map::WAYPOINT |
                               map::MARKER | map::AIRWAYJ | map::AIRWAYV),
                              xs, ys, maxDistance, result);

  // Update all incomplete objects, especially from search
  for(map::MapAirport& obj : result.airports)
    if(!obj.complete())
      mapQuery->getAirportById(obj, obj.getId());
}

void MapScreenIndex::getNearestHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result)
{
  CoordinateConverter conv(mapWidget->viewport());
  int x, y;

  using maptools::insertSortedByDistance;

  for(const map::MapAirport& obj : highlights.airports)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, obj);

  for(const map::MapVor& obj : highlights.vors)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, obj);

  for(const map::MapNdb& obj : highlights.ndbs)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, obj);

  for(const map::MapWaypoint& obj : highlights.waypoints)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, obj);

  for(const map::MapIls& obj : highlights.ils)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.ils, nullptr, xs, ys, obj);

  for(const map::MapRunwayEnd& obj : highlights.runwayEnds)
    if(conv.wToS(obj.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        insertSortedByDistance(conv, result.runwayEnds, nullptr, xs, ys, obj);
}

void MapScreenIndex::getNearestProcedureHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                                   QList<proc::MapProcedurePoint>& procPoints)
{
  CoordinateConverter conv(mapWidget->viewport());
  int x, y;

  using maptools::insertSortedByDistance;

  for(int i = 0; i < approachHighlight.size(); i++)
  {
    const proc::MapProcedureLeg& leg = approachHighlight.at(i);

    for(const map::MapAirport& obj : leg.navaids.airports)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, obj);

    for(const map::MapVor& obj : leg.navaids.vors)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, obj);

    for(const map::MapNdb& obj : leg.navaids.ndbs)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, obj);

    for(const map::MapWaypoint& obj : leg.navaids.waypoints)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, obj);

    for(const map::MapIls& obj : leg.navaids.ils)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.ils, nullptr, xs, ys, obj);

    for(const map::MapRunwayEnd& obj : leg.navaids.runwayEnds)
      if(conv.wToS(obj.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          insertSortedByDistance(conv, result.runwayEnds, nullptr, xs, ys, obj);

    if(conv.wToS(leg.line.getPos2(), x, y))
    {
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        procPoints.append(proc::MapProcedurePoint(leg));
    }
  }
}

int MapScreenIndex::getNearestDistanceMarkIndex(int xs, int ys, int maxDistance)
{
  CoordinateConverter conv(mapWidget->viewport());
  int index = 0;
  int x, y;
  for(const map::DistanceMarker& marker : distanceMarks)
  {
    if(conv.wToS(marker.to, x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
      return index;

    index++;
  }
  return -1;
}

int MapScreenIndex::getNearestRoutePointIndex(int xs, int ys, int maxDistance)
{
  if(!paintLayer->getShownMapObjects().testFlag(map::FLIGHTPLAN))
    return -1;

  int minIndex = -1;
  float minDist = map::INVALID_DISTANCE_VALUE;

  for(const std::pair<int, QPoint>& rsp : routePoints)
  {
    const QPoint& point = rsp.second;
    float dist = atools::geo::manhattanDistance(point.x(), point.y(), xs, ys);
    if(dist < minDist && dist < maxDistance)
    {
      minDist = dist;
      minIndex = rsp.first;
    }
  }

  return minIndex;
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirspaces(int xs, int ys, map::MapSearchResult& result)
{
  if(!paintLayer->getShownMapObjects().testFlag(map::AIRSPACE))
    return;

  for(int i = 0; i < airspacePolygons.size(); i++)
  {
    const std::pair<int, QPolygon>& polyPair = airspacePolygons.at(i);

    const QPolygon& poly = polyPair.second;

    if(poly.containsPoint(QPoint(xs, ys), Qt::OddEvenFill))
    {
      map::MapAirspace airspace;
      mapQuery->getAirspaceById(airspace, polyPair.first);
      result.airspaces.append(airspace);
    }
  }
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirways(int xs, int ys, int maxDistance, map::MapSearchResult& result)
{
  if(!paintLayer->getShownMapObjects().testFlag(map::AIRWAYJ) &&
     !paintLayer->getShownMapObjects().testFlag(map::AIRWAYV))
    return;

  for(int i = 0; i < airwayLines.size(); i++)
  {
    const std::pair<int, QLine>& linePair = airwayLines.at(i);

    const QLine& line = linePair.second;

    if(atools::geo::distanceToLine(xs, ys, line.x1(), line.y1(), line.x2(), line.y2(),
                                   true /* no dist to points */) < maxDistance)
    {
      map::MapAirway airway;
      mapQuery->getAirwayById(airway, linePair.first);
      result.airways.append(airway);
    }
  }
}

int MapScreenIndex::getNearestRouteLegIndex(int xs, int ys, int maxDistance)
{
  if(!paintLayer->getShownMapObjects().testFlag(map::FLIGHTPLAN))
    return -1;

  int minIndex = -1;
  float minDist = std::numeric_limits<float>::max();

  for(int i = 0; i < routeLines.size(); i++)
  {
    const std::pair<int, QLine>& line = routeLines.at(i);

    QLine l = line.second;

    float dist = atools::geo::distanceToLine(xs, ys, l.x1(), l.y1(), l.x2(), l.y2(), true /* no dist to points */);

    if(dist < minDist && dist < maxDistance)
    {
      minDist = dist;
      minIndex = line.first;
    }
  }
  return minIndex;
}

int MapScreenIndex::getNearestRangeMarkIndex(int xs, int ys, int maxDistance)
{
  CoordinateConverter conv(mapWidget->viewport());
  int index = 0;
  int x, y;
  for(const map::RangeMarker& marker : rangeMarks)
  {
    if(conv.wToS(marker.center, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        return index;

    index++;
  }
  return -1;
}
