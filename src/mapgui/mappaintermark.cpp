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

#include "mappaintermark.h"
#include "navmapwidget.h"

#include "mapgui/mapquery.h"
#include "common/mapcolors.h"

#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>

using namespace Marble;

MapPainterMark::MapPainterMark(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg)
  : MapPainter(widget, mapQuery, mapScale, verboseMsg), navMapWidget(widget)
{
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::paint(const MapLayer *mapLayer, GeoPainter *painter, ViewportParams *viewport,
                           maptypes::ObjectTypes objectTypes)
{
  Q_UNUSED(mapLayer);
  Q_UNUSED(viewport);
  Q_UNUSED(objectTypes);

  bool drawFast = widget->viewContext() == Marble::Animation;

  painter->save();
  paintHighlights(mapLayer, painter, drawFast);
  paintMark(painter);
  paintHome(painter);
  painter->restore();
}

void MapPainterMark::paintMark(GeoPainter *painter)
{
  int x, y;
  if(wToS(navMapWidget->getMarkPos(), x, y))
  {
    painter->setPen(mapcolors::markBackPen);

    painter->drawLine(x, y - 10, x, y + 10);
    painter->drawLine(x - 10, y, x + 10, y);

    painter->setPen(mapcolors::markFillPen);
    painter->drawLine(x, y - 8, x, y + 8);
    painter->drawLine(x - 8, y, x + 8, y);
  }
}

void MapPainterMark::paintHome(GeoPainter *painter)
{
  int x, y;
  if(wToS(navMapWidget->getHomePos(), x, y))
  {
    painter->setPen(mapcolors::homeBackPen);
    painter->setBrush(mapcolors::homeFillColor);

    QPolygon roof;
    painter->drawRect(x - 5, y - 5, 10, 10);
    roof << QPoint(x, y - 10) << QPoint(x + 7, y - 3) << QPoint(x - 7, y - 3);
    painter->drawConvexPolygon(roof);
    painter->drawPoint(x, y);
  }
}

void MapPainterMark::paintHighlights(const MapLayer *mapLayer, GeoPainter *painter, bool fast)
{
  using namespace maptypes;

  const MapSearchResult& highlightResults = navMapWidget->getHighlightMapObjects();
  int size = 6;

  QList<atools::geo::Pos> positions;

  for(const MapAirport *ap : highlightResults.airports)
    positions.append(ap->position);
  for(const MapWaypoint *wp : highlightResults.waypoints)
    positions.append(wp->position);
  for(const MapVor *vor : highlightResults.vors)
    positions.append(vor->position);
  for(const MapNdb *ndb : highlightResults.ndbs)
    positions.append(ndb->position);

  if(mapLayer != nullptr)
    if(mapLayer->isAirport())
      size = std::max(size, mapLayer->getAirportSymbolSize());

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::highlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const atools::geo::Pos& pos : positions)
  {
  int x, y;
  if(wToS(pos, x, y))
  {
    if(!fast)
    {
      painter->setPen(QPen(QBrush(mapcolors::highlightBackColor), size / 3 + 2, Qt::SolidLine, Qt::FlatCap));
      painter->drawEllipse(QPoint(x, y), size, size);
      painter->setPen(QPen(QBrush(mapcolors::highlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
    }
    painter->drawEllipse(QPoint(x, y), size, size);
  }
  }
}
