/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/mappaintertop.h"

#include "common/mapcolors.h"

#include <marble/GeoPainter.h>

MapPainterTop::MapPainterTop(MapPaintWidget *mapWidgetParam, MapScale *mapScale)
  : MapPainter(mapWidgetParam, mapScale)
{

}

MapPainterTop::~MapPainterTop()
{

}

void MapPainterTop::render(PaintContext *context)
{
  if(!context->visibleWidget)
    return;

  optsd::DisplayOptionsNavAid opts = OptionData::instance().getDisplayOptionsNavAid();
  opts::MapNavigation nav = OptionData::instance().getMapNavigation();

  int size = context->sz(context->symbolSizeAirport, 10);
  int size2 = context->sz(context->symbolSizeAirport, 9);

  // Draw center cross =====================================
  // Usable in all modes
  if(opts & optsd::NAVAIDS_CENTER_CROSS)
  {
    QRect vp = context->painter->viewport();
    int x = vp.center().x();
    int y = vp.center().y();

    context->painter->setPen(mapcolors::centerCrossBackPen);
    drawCross(context, x, y, size);

    context->painter->setPen(mapcolors::centerCrossFillPen);
    drawCross(context, x, y, size2);
  }

  // Screen navigation areas =====================================
  // Show only if touch areas are enabled
  if(nav == opts::MAP_NAV_TOUCHSCREEN)
  {
    if(opts & optsd::NAVAIDS_TOUCHSCREEN_AREAS)
    {
      int areaSize = OptionData::instance().getMapNavTouchArea();

      context->painter->setPen(mapcolors::centerCrossBackPen);
      drawTouchMarks(context, size, areaSize);

      context->painter->setPen(mapcolors::centerCrossFillPen);
      drawTouchMarks(context, size2, areaSize);
    }

    // Navigation icons in the corners
    if(opts & optsd::NAVAIDS_TOUCHSCREEN_ICONS)
    {
      // Make icon size dependent on screen size but limit min and max
      int iconSize = std::max(context->painter->viewport().height(), context->painter->viewport().width()) / 20;
      drawTouchIcons(context, std::max(std::min(iconSize, 30), 10));
    }
  }
}

void MapPainterTop::drawTouchIcons(const PaintContext *context, int iconSize)
{
  Marble::GeoPainter *painter = context->painter;
  QRect vp = context->painter->viewport();
  int w = vp.width();
  int h = vp.height();
  static const int borderDist = 5;

  // Get pixmap from cache
  QPixmap pixmap;
  getPixmap(pixmap, ":/littlenavmap/resources/icons/zoomin.svg", iconSize);
  painter->drawPixmap(QPoint(borderDist, borderDist), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/arrowup.svg", iconSize);
  painter->drawPixmap(QPoint(w / 2 - iconSize, borderDist), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/zoomout.svg", iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, borderDist), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/arrowleft.svg", iconSize);
  painter->drawPixmap(QPoint(borderDist, h / 2 - iconSize), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/arrowright.svg", iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, h / 2 - iconSize), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/back.svg", iconSize);
  painter->drawPixmap(QPoint(borderDist, h - iconSize - borderDist), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/arrowdown.svg", iconSize);
  painter->drawPixmap(QPoint(w / 2 - iconSize, h - iconSize - borderDist), pixmap);

  getPixmap(pixmap, ":/littlenavmap/resources/icons/next.svg", iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, h - iconSize - borderDist), pixmap);
}

void MapPainterTop::drawTouchMarks(const PaintContext *context, int lineSize, int areaSize)
{
  QRect vp = context->painter->viewport();
  int w = vp.width() * areaSize / 100;
  int h = vp.height() * areaSize / 100;

  Marble::GeoPainter *painter = context->painter;
  // Top
  painter->drawLine(w, 0, w, lineSize);
  painter->drawLine(vp.width() - w, 0, vp.width() - w, lineSize);

  // Bottom
  painter->drawLine(w, vp.height() - lineSize, w, vp.height());
  painter->drawLine(vp.width() - w, vp.height() - lineSize, vp.width() - w, vp.height());

  // Left
  painter->drawLine(0, h, lineSize, h);
  painter->drawLine(0, vp.height() - h, lineSize, vp.height() - h);

  // Right
  painter->drawLine(vp.width() - lineSize, h, vp.width(), h);
  painter->drawLine(vp.width() - lineSize, vp.height() - h, vp.width(), vp.height() - h);

  // Top-left
  drawCross(context, w, h, lineSize);
  // Top-right
  drawCross(context, vp.width() - w, h, lineSize);
  // Bottom-right
  drawCross(context, vp.width() - w, vp.height() - h, lineSize);
  // Bottom-left
  drawCross(context, w, vp.height() - h, lineSize);
}
