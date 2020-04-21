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

#include "mappainter/mappaintertop.h"

#include "common/mapcolors.h"
#include "navapp.h"
#include "util/paintercontextsaver.h"

#ifdef DEBUG_INFORMATION
#include "common/proctypes.h"
#include "mapgui/mappaintwidget.h"
#endif

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
  paintCopyright(context);

  if(!context->visibleWidget)
    return;

  optsd::DisplayOptionsNavAid opts = OptionData::instance().getDisplayOptionsNavAid();
  opts::MapNavigation nav = OptionData::instance().getMapNavigation();

  int size = context->sz(context->symbolSizeAirport, 10);
  int size2 = context->sz(context->symbolSizeAirport, 9);

  // Draw center cross =====================================
  // Usable in all modes
  Marble::GeoPainter *painter = context->painter;
  if(opts & optsd::NAVAIDS_CENTER_CROSS)
  {
    QRect vp = painter->viewport();
    int x = vp.center().x();
    int y = vp.center().y();

    painter->setPen(mapcolors::touchMarkBackPen);
    drawCross(context, x, y, size);

    painter->setPen(mapcolors::touchMarkFillPen);
    drawCross(context, x, y, size2);
  }

  // Screen navigation areas =====================================
  // Show only if touch areas are enabled
  if(nav == opts::MAP_NAV_TOUCHSCREEN)
  {
    int areaSize = OptionData::instance().getMapNavTouchArea();
    if(opts & optsd::NAVAIDS_TOUCHSCREEN_REGIONS)
      drawTouchRegions(context, areaSize);

    if(opts & optsd::NAVAIDS_TOUCHSCREEN_AREAS)
    {
      painter->setPen(mapcolors::touchMarkBackPen);
      drawTouchMarks(context, size, areaSize);

      painter->setPen(mapcolors::touchMarkFillPen);
      drawTouchMarks(context, size2, areaSize);
    }

    // Navigation icons in the corners
    if(opts & optsd::NAVAIDS_TOUCHSCREEN_ICONS)
    {
      // Make icon size dependent on screen size but limit min and max
      int iconSize = std::max(painter->viewport().height(), painter->viewport().width()) / 20;
      drawTouchIcons(context, std::max(std::min(iconSize, 30), 10));
    }
  }

#ifdef DEBUG_INFORMATION
  {
    const proc::MapProcedureLeg& leg = mapPaintWidget->getProcedureLegHighlights();
    atools::util::PainterContextSaver saver(context->painter);
    painter->setBrush(Qt::black);
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBackground(Qt::black);

    if(leg.geometry.isValid())
    {
      painter->setPen(QPen(Qt::red, 6));
      drawLineString(context, leg.geometry);
    }
    if(leg.line.isValid())
    {
      painter->setPen(QPen(Qt::yellow, 3));
      drawLine(context, leg.line);
      drawText(context, leg.line.getPos1(), "P1", false, false);
      drawText(context, leg.line.getPos2(), QString("P2,%1°,%2nm").
               arg(leg.course, 0, 'f', 1).arg(leg.distance, 0, 'f', 1), false, false);

    }
    if(leg.holdLine.isValid())
    {
      painter->setPen(QPen(Qt::blue, 3));
      drawLine(context, leg.line);
    }
    if(leg.procedureTurnPos.isValid())
    {
      painter->setPen(QPen(Qt::blue, 2));
      drawText(context, leg.procedureTurnPos, "PTFIX", false, false);
    }
    if(leg.interceptPos.isValid())
    {
      painter->setPen(QPen(Qt::yellow, 2));
      drawText(context, leg.interceptPos, "ICPT", true, false);
    }
    if(leg.recFixPos.isValid())
    {
      painter->setPen(QPen(Qt::lightGray, 2));
      drawText(context, leg.recFixPos, leg.recFixIdent.isEmpty() ? "RECFIX" : leg.recFixIdent, false, true);
    }
    if(leg.fixPos.isValid())
    {
      painter->setPen(QPen(Qt::white, 2));
      drawText(context, leg.fixPos, leg.fixIdent.isEmpty() ? "FIX" : leg.fixIdent, true, true);
    }
  }
#endif
}

void MapPainterTop::paintCopyright(PaintContext *context)
{
  QString mapCopyright = NavApp::getMapCopyright();
  if(!mapCopyright.isEmpty())
  {
    Marble::GeoPainter *painter = context->painter;
    atools::util::PainterContextSaver saver(painter);

    // Move text more into the center for web apps
    int rightOffset = context->visibleWidget ? 0 : 20;
    int bottomOffset = context->visibleWidget ? 0 : 4;

    // Draw text
    context->szFont(0.8f);
    QFont font = painter->font();
    font.setBold(false);
    painter->setFont(font);
    painter->setPen(Qt::black);
    painter->setBackground(QColor("#b0ffffff"));
    painter->setBrush(Qt::NoBrush);
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->drawText(painter->viewport().width() - painter->fontMetrics().width(mapCopyright) - rightOffset,
                      painter->viewport().height() - painter->fontMetrics().descent() - bottomOffset, mapCopyright);
  }
}

void MapPainterTop::drawTouchIcons(const PaintContext *context, int iconSize)
{
  Marble::GeoPainter *painter = context->painter;
  QRect vp = painter->viewport();
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

void MapPainterTop::drawTouchRegions(const PaintContext *context, int areaSize)
{
  Marble::GeoPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(painter);

  QRect vp = context->painter->viewport();
  QPolygon poly;
  poly << vp.topLeft() << vp.topRight() << vp.bottomRight() << vp.bottomLeft();

  int w = vp.width() * areaSize / 100;
  int h = vp.height() * areaSize / 100;
  QPolygon hole;
  hole << QPoint(vp.left() + w, vp.top() + h)
       << QPoint(vp.right() - w, vp.top() + h)
       << QPoint(vp.right() - w, vp.bottom() - h)
       << QPoint(vp.left() + w, vp.bottom() - h);

  poly = poly.subtracted(hole);

  painter->setBrush(mapcolors::touchRegionFillColor);
  painter->setPen(Qt::transparent);
  painter->setBackground(painter->brush().color());
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->drawPolygon(poly);
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
