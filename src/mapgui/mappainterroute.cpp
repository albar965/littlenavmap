/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "mappainterroute.h"
#include "navmapwidget.h"
#include "mapscale.h"
#include "symbolpainter.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"

#include <algorithm>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>
#include <route/routecontroller.h>

using namespace Marble;
using namespace atools::geo;

MapPainterRoute::MapPainterRoute(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                                 RouteController *controller, bool verbose)
  : MapPainter(widget, mapQuery, mapScale, verbose), routeController(controller), navMapWidget(widget)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::paint(const PaintContext *context)
{

  bool drawFast = widget->viewContext() == Marble::Animation;
  setRenderHints(context->painter);

  context->painter->save();
  // Use layer independent of declutter factor
  paintRoute(context->mapLayerEffective, context->painter, drawFast);
  context->painter->restore();
}

void MapPainterRoute::paintRoute(const MapLayer *mapLayer, GeoPainter *painter, bool fast)
{
  Q_UNUSED(fast);
  const QList<RouteMapObject> routeMapObjects = routeController->getRouteMapObjects();

  painter->setBrush(Qt::NoBrush);

  QList<GeoDataLineString> lines;
  QList<GeoDataCoordinates> textCoords;
  QList<qreal> textBearing;

  bool first = true;
  RouteMapObject last;
  for(const RouteMapObject& obj : routeMapObjects)
  {
    if(!first)
    {
      GeoDataCoordinates from(last.getPosition().getLonX(),
                              last.getPosition().getLatY(), 0, GeoDataCoordinates::Degree);
      GeoDataCoordinates to(obj.getPosition().getLonX(),
                            obj.getPosition().getLatY(), 0, GeoDataCoordinates::Degree);

      qreal init = from.bearing(to, GeoDataCoordinates::Degree, GeoDataCoordinates::InitialBearing);
      qreal final = from.bearing(to, GeoDataCoordinates::Degree, GeoDataCoordinates::FinalBearing);
      textBearing.append((init + final) / 2);

      textCoords.append(from.interpolate(to, 0.5));

      GeoDataLineString line;
      line.append(from);
      line.append(to);
      line.setTessellate(true);
      lines.append(line);
    }
    else
      first = false;
    last = obj;
  }

  painter->setPen(QPen(QColor(Qt::black), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(const GeoDataLineString& line : lines)
    painter->drawPolyline(line);

  painter->setPen(QPen(QColor(Qt::yellow), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(const GeoDataLineString& line : lines)
    painter->drawPolyline(line);

  int x, y;
  QList<QPoint> points;
  for(const RouteMapObject& obj : routeMapObjects)
    if(wToS(obj.getPosition(), x, y))
      points.append(QPoint(x, y));
    else
      points.append(QPoint());

  int i = 0;
  for(const QPoint& pt : points)
  {
    if(!pt.isNull())
    {
      x = pt.x();
      y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::NONE:
          break;
        case maptypes::AIRPORT:
          paintAirport(mapLayer, painter, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVor(mapLayer, painter, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdb(mapLayer, painter, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypoint(mapLayer, painter, x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }

  i = 0;
  for(const QPoint& pt : points)
  {
    if(!pt.isNull())
    {
      x = pt.x();
      y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::NONE:
          break;
        case maptypes::AIRPORT:
          paintAirport(mapLayer, painter, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVorText(mapLayer, painter, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdbText(mapLayer, painter, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypointText(mapLayer, painter, x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }

  painter->setPen(QColor(Qt::black));
  i = 0;
  for(const GeoDataCoordinates& coord : textCoords)
  {
    if(wToS(coord, x, y))
    {
      const RouteMapObject& obj = routeMapObjects.at(i + 1);
      if(obj.hasPredecessor())
      {
        QString text(QString::number(obj.getDistanceTo(), 'f', 0) + " nm" + " / " +
                     QString::number(obj.getCourseRhumb(), 'f', 0) + "°M");

        qreal brg = textBearing.at(i);
        if(brg > 180)
          brg -= 180;

        painter->translate(x, y);
        painter->rotate(brg - 90.);
        painter->drawText(-painter->fontMetrics().width(text) / 2, -painter->fontMetrics().descent(), text);
        painter->resetTransform();
      }
    }
    i++;
  }
}

void MapPainterRoute::paintAirport(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapAirport& obj)
{
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT;
  int size = mapLayer != nullptr ? mapLayer->getAirportSymbolSize() : 10;

  if(mapLayer != nullptr && mapLayer->isAirportRouteInfo())
    flags |= textflags::NAME | textflags::INFO;

  symbolPainter->drawAirportSymbol(painter, obj, x, y, size, false, false);
  symbolPainter->drawAirportText(painter, obj, x, y, flags, size, false, true, false);
}

void MapPainterRoute::paintVor(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                               const maptypes::MapVor& obj)
{
  int size = mapLayer != nullptr ? mapLayer->getVorSymbolSize() : 10;
  bool large = mapLayer != nullptr ? mapLayer->isVorLarge() : false;
  symbolPainter->drawVorSymbol(painter, obj, x, y, size, true, false, large);
}

void MapPainterRoute::paintVorText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapVor& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(mapLayer != nullptr && mapLayer->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  int size = mapLayer != nullptr ? mapLayer->getVorSymbolSize() : 10;
  symbolPainter->drawVorText(painter, obj, x, y, flags, size, true, false);
}

void MapPainterRoute::paintNdb(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                               const maptypes::MapNdb& obj)
{
  int size = mapLayer != nullptr ? mapLayer->getNdbSymbolSize() : 10;
  symbolPainter->drawNdbSymbol(painter, obj, x, y, size, true, false);
}

void MapPainterRoute::paintNdbText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                   const maptypes::MapNdb& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(mapLayer != nullptr && mapLayer->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  int size = mapLayer != nullptr ? mapLayer->getNdbSymbolSize() : 10;
  symbolPainter->drawNdbText(painter, obj, x, y, flags, size, true, false);
}

void MapPainterRoute::paintWaypoint(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                    const maptypes::MapWaypoint& obj)
{
  int size = mapLayer != nullptr ? mapLayer->getWaypointSymbolSize() : 10;
  symbolPainter->drawWaypointSymbol(painter, obj, x, y, size, true, false);
}

void MapPainterRoute::paintWaypointText(const MapLayer *mapLayer, GeoPainter *painter, int x, int y,
                                        const maptypes::MapWaypoint& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(mapLayer != nullptr && mapLayer->isWaypointRouteName())
    flags |= textflags::IDENT;

  int size = mapLayer != nullptr ? mapLayer->getWaypointSymbolSize() : 10;
  symbolPainter->drawWaypointText(painter, obj, x, y, flags, size, true, false);
}
