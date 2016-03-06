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

#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>

using namespace Marble;

MapPainterMark::MapPainterMark(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(widget, mapQuery, mapScale), navMapWidget(widget)
{
  markBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);
  markFillPen = QPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::paint(const MapLayer *mapLayer, GeoPainter *painter, ViewportParams *viewport)
{
  Q_UNUSED(mapLayer);
  Q_UNUSED(viewport);

  paintMark(painter);
}

void MapPainterMark::paintMark(GeoPainter *painter)
{
  int x, y;

  GeoDataCoordinates mark = navMapWidget->getMark();

  bool visible = wToS(mark, x, y);

  if(visible)
  {
    painter->save();
    int xc = x, yc = y;
    painter->setPen(markBackPen);

    painter->drawLine(xc, yc - 10, xc, yc + 10);
    painter->drawLine(xc - 10, yc, xc + 10, yc);

    painter->setPen(markFillPen);
    painter->drawLine(xc, yc - 8, xc, yc + 8);
    painter->drawLine(xc - 8, yc, xc + 8, yc);
    painter->restore();
  }
}
