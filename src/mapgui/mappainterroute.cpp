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

void MapPainterRoute::paint(const MapLayer *mapLayer, GeoPainter *painter, ViewportParams *viewport,
                            maptypes::MapObjectTypes objectTypes)
{
  Q_UNUSED(mapLayer);
  Q_UNUSED(viewport);
  Q_UNUSED(objectTypes);

  bool drawFast = widget->viewContext() == Marble::Animation;
  setRenderHints(painter);

  painter->save();
  paintRoute(mapLayer, painter, drawFast);
  painter->restore();
}

void MapPainterRoute::paintRoute(const MapLayer *mapLayer, GeoPainter *painter, bool fast)
{
  const QList<RouteMapObject> routeMapObjects = routeController->getRouteMapObjects();

  painter->setBrush(Qt::NoBrush);

  QList<GeoDataLineString> lines;

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
  SymbolPainter syms;
  for(const RouteMapObject& obj : routeMapObjects)
    if(wToS(obj.getPosition(), x, y))
    {

      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::NONE:
          break;
        case maptypes::AIRPORT:
          syms.drawAirportSymbol(painter, obj.getAirport(), x, y, 14, false, false);
          break;
        case maptypes::VOR:
          syms.drawVorSymbol(painter, obj.getVor(), x, y, 14, false, false);
          break;
        case maptypes::NDB:
          syms.drawNdbSymbol(painter, obj.getNdb(), x, y, 14, false);
          break;
        case maptypes::WAYPOINT:
          syms.drawWaypointSymbol(painter, obj.getWaypoint(), x, y, 10, false);
          break;
      }
    }
}
