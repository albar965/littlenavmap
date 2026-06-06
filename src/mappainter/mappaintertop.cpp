/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapthemehandler.h"
#include "mappainter/paintcontext.h"
#include "options/optiondata.h"
#include "util/paintercontextsaver.h"

#ifdef DEBUG_APPROACH_PAINT
#include "common/proctypes.h"
#endif

#include <marble/GeoPainter.h>

MapPainterTop::MapPainterTop(MapPaintWidget *mapWidgetParam, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidgetParam, mapScale, paintContext)
{

}

MapPainterTop::~MapPainterTop()
{

}

void MapPainterTop::render()
{
  paintCopyright();

  if(!context->visibleWidget)
    return;

  optsd::DisplayOptionsNavAid opts = OptionData::instance().getDisplayOptionsNavAid();
  opts::MapNavigation nav = OptionData::instance().getMapNavigation();

  float size = context->szF(context->symbolSizeAirport, 10.f);
  float size2 = context->szF(context->symbolSizeAirport, 9.f);

  if(!context->webMap && context->paintNavigation)
  {
    // Draw center cross =====================================
    // Usable in all modes
    Marble::GeoPainter *painter = context->painter;
    if(opts & optsd::NAVAIDS_CENTER_CROSS)
    {
      QRect vp = painter->viewport();
      int x = vp.center().x();
      int y = vp.center().y();

      painter->setPen(mapcolors::touchMarkBackPen);
      drawCross(painter, x, y, size);

      painter->setPen(mapcolors::touchMarkFillPen);
      drawCross(painter, x, y, size2);
    }

    // Screen navigation areas =====================================
    // Show only if touch areas are enabled
    if(nav == opts::MAP_NAV_TOUCHSCREEN)
    {
      int areaSize = OptionData::instance().getMapNavTouchArea();
      if(opts & optsd::NAVAIDS_TOUCHSCREEN_REGIONS)
        drawTouchRegions(areaSize);

      // Navigation icons in the corners
      if(opts & optsd::NAVAIDS_TOUCHSCREEN_ICONS)
      {
        // Make icon size dependent on screen size but limit min and max
        int iconSize = std::max(painter->viewport().height(), painter->viewport().width()) / 20;
        drawTouchIcons(std::max(std::min(iconSize, 30), 10));
      }
    }
  }

#ifdef DEBUG_APPROACH_PAINT
  {
    const proc::MapProcedureLeg& leg = mapPaintWidget->getProcedureLegHighlight();
    atools::util::PainterContextSaver saver(context->painter);
    context->painter->setBrush(Qt::black);
    context->painter->setBackgroundMode(Qt::OpaqueMode);
    context->painter->setBackground(Qt::black);

    if(leg.geometry.isValid())
    {
      context->painter->setPen(QPen(Qt::red, 6));
      drawPolyline(context->painter, leg.geometry);
    }
    if(leg.line.isValid())
    {
      context->painter->setPen(QPen(Qt::yellow, 3));
      drawLine(context->painter, leg.line);
      drawText(context->painter, leg.line.getPos1(), "P1", false, false);
      drawText(context->painter, leg.line.getPos2(), QStringLiteral("P2,%1°,%2nm").
               arg(leg.course, 0, 'f', 1).arg(leg.distance, 0, 'f', 1), false, false);

    }
    if(leg.holdLine.isValid())
    {
      context->painter->setPen(QPen(Qt::blue, 3));
      drawLine(context->painter, leg.line);
    }
    if(leg.procedureTurnPos.isValid())
    {
      context->painter->setPen(QPen(Qt::blue, 2));
      drawText(context->painter, leg.procedureTurnPos, "PTFIX", false, false);
    }
    if(leg.interceptPos.isValid())
    {
      context->painter->setPen(QPen(Qt::yellow, 2));
      drawText(context->painter, leg.interceptPos, "ICPT", true, false);
    }
    if(leg.recFixPos.isValid())
    {
      context->painter->setPen(QPen(Qt::lightGray, 2));
      drawText(context->painter, leg.recFixPos, leg.recFixIdent.isEmpty() ? "RECFIX" : leg.recFixIdent, false, true);
    }
    if(leg.fixPos.isValid())
    {
      context->painter->setPen(QPen(Qt::white, 2));
      drawText(context->painter, leg.fixPos, leg.fixIdent.isEmpty() ? "FIX" : leg.fixIdent, true, true);
    }
  }
#endif

  if(context->verboseDraw && !context->webMap)
  {
    atools::util::PainterContextSaver dbgsaver(context->painter);
    context->szFont(0.8f);

    QStringList labels;
    labels.append(QStringLiteral("Layer %1").arg(context->mapLayer->getMaxRange()));
    labels.append(QStringLiteral("Layer text %1").arg(context->mapLayerText->getMaxRange()));
    labels.append(QStringLiteral("Layer effective %1").arg(context->mapLayerEffective->getMaxRange()));
    labels.append(QStringLiteral("Layer route %1").arg(context->mapLayerRoute->getMaxRange()));
    labels.append(QStringLiteral("Layer route text %1").arg(context->mapLayerRouteText->getMaxRange()));
    labels.append(QStringLiteral("Airport sym %1").arg(context->mapLayer->getAirportSymbolSize()));
    labels.append(QStringLiteral("Min RW %1").arg(context->mapLayer->getMinRunwayLength()));
    labels.append(QStringLiteral("-"));

    for(auto it = context->renderTimesMs.constBegin(); it != context->renderTimesMs.constEnd(); ++it)
      labels.append(QStringLiteral("%1: %2 ms").arg(it.key()).arg(it.value()));

    symbolPainter->textBox(context->painter, labels, QPen(Qt::black), 1, 1, text::BELOW);
  }
}

void MapPainterTop::paintCopyright()
{
  if(context->paintCopyright)
  {
    QString mapCopyright = NavApp::getMapThemeHandler()->getTheme(mapPaintWidget->getCurrentThemeId()).getCopyright();
    if(!mapCopyright.isEmpty())
    {
      Marble::GeoPainter *painter = context->painter;
      atools::util::PainterContextSaver saver(painter);

      painter->setFont(QApplication::font());
      mapcolors::scaleFont(painter, 0.9f);

      // Move text more into the center for web apps
      int rightOffset = context->visibleWidget ? 0 : 20;
      int bottomOffset = context->visibleWidget ? 0 : 4;

      // Draw text
      painter->setPen(Qt::black);
      painter->setBackground(QColor(QStringLiteral("#b0ffffff")));
      painter->setBrush(Qt::NoBrush);
      painter->setBackgroundMode(Qt::OpaqueMode);
      painter->drawText(painter->viewport().width() - painter->fontMetrics().horizontalAdvance(mapCopyright) - rightOffset,
                        painter->viewport().height() - painter->fontMetrics().descent() - bottomOffset, mapCopyright);
    }
  }
}

void MapPainterTop::drawTouchIcons(int iconSize)
{
  Marble::GeoPainter *painter = context->painter;
  QRect vp = painter->viewport();
  int w = vp.width();
  int h = vp.height();
  static const int borderDist = 5;

  // Get pixmap from cache
  QPixmap pixmap;
  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/zoomin.svg"), iconSize);
  painter->drawPixmap(QPoint(borderDist, borderDist), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/arrowup.svg"), iconSize);
  painter->drawPixmap(QPoint(w / 2 - iconSize, borderDist), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/zoomout.svg"), iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, borderDist), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/arrowleft.svg"), iconSize);
  painter->drawPixmap(QPoint(borderDist, h / 2 - iconSize), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/arrowright.svg"), iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, h / 2 - iconSize), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/back.svg"), iconSize);
  painter->drawPixmap(QPoint(borderDist, h - iconSize - borderDist), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/arrowdown.svg"), iconSize);
  painter->drawPixmap(QPoint(w / 2 - iconSize, h - iconSize - borderDist), pixmap);

  getPixmap(pixmap, QStringLiteral(":/littlenavmap/resources/icons/next.svg"), iconSize);
  painter->drawPixmap(QPoint(w - iconSize - borderDist, h - iconSize - borderDist), pixmap);
}

void MapPainterTop::drawTouchRegions(int areaSize)
{
  Marble::GeoPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(painter);

  QRect vp = context->painter->viewport();

  int w = vp.width() * areaSize / 100;
  int h = vp.height() * areaSize / 100;

  painter->setBrush(mapcolors::touchRegionFillColor);
  painter->setBackground(mapcolors::touchRegionFillColor);
  painter->setPen(Qt::transparent);
  painter->setBackgroundMode(Qt::OpaqueMode);

  // Top (up)
  painter->drawPolygon(QPolygon({QPoint(vp.left() + w, vp.top()), QPoint(vp.right() - w, vp.top()),
                                 QPoint(vp.right() - w, vp.top() + h), QPoint(vp.left() + w, vp.top() + h)}));

  // Bottom (down)
  painter->drawPolygon(QPolygon({QPoint(vp.left() + w, vp.bottom() - h), QPoint(vp.right() - w, vp.bottom() - h),
                                 QPoint(vp.right() - w, vp.bottom()), QPoint(vp.left() + w, vp.bottom())}));

  // Left
  painter->drawPolygon(QPolygon({QPoint(vp.left(), vp.top() + h), QPoint(vp.left() + w, vp.top() + h),
                                 QPoint(vp.left() + w, vp.bottom() - h), QPoint(vp.left(), vp.bottom() - h)}));

  // Right
  painter->drawPolygon(QPolygon({QPoint(vp.right() - w, vp.top() + h), QPoint(vp.right(), vp.top() + h),
                                 QPoint(vp.right(), vp.bottom() - h), QPoint(vp.right() - w, vp.bottom() - h)}));

  painter->setBrush(mapcolors::touchRegionCornerFillColor);
  painter->setBackground(mapcolors::touchRegionCornerFillColor);

  // Top left (zoom in)
  painter->drawPolygon(QPolygon({QPoint(vp.left(), vp.top()), QPoint(vp.left() + w, vp.top()),
                                 QPoint(vp.left() + w, vp.top() + h), QPoint(vp.left(), vp.top() + h)}));

  // Top right (zoom out)
  painter->drawPolygon(QPolygon({QPoint(vp.right() - w, vp.top()), QPoint(vp.right(), vp.top()),
                                 QPoint(vp.right(), vp.top() + h), QPoint(vp.right() - w, vp.top() + h)}));

  // Bottom left (history back)
  painter->drawPolygon(QPolygon({QPoint(vp.left(), vp.bottom() - h), QPoint(vp.left() + w, vp.bottom() - h),
                                 QPoint(vp.left() + w, vp.bottom()), QPoint(vp.left(), vp.bottom())}));

  // Bottom right (history forward)
  painter->drawPolygon(QPolygon({QPoint(vp.right() - w, vp.bottom() - h), QPoint(vp.right(), vp.bottom() - h),
                                 QPoint(vp.right(), vp.bottom()), QPoint(vp.right() - w, vp.bottom())}));
}
