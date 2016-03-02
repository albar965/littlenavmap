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

#include "mapgui/mappaintlayer.h"
#include "mapgui/navmapwidget.h"
#include "mapquery.h"

#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoPainter.h>
#include <marble/LayerInterface.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleLocale.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>
#include <QContextMenuEvent>

using namespace Marble;

MapPaintLayer::MapPaintLayer(NavMapWidget *widget, atools::sql::SqlDatabase *sqlDb)
  : navMapWidget(widget), db(sqlDb)
{
  markBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);
  markFillPen = QPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);

  textBackgroundPen = QPen(QBrush(QColor::fromRgb(255, 255, 255, 200)), 1, Qt::SolidLine, Qt::RoundCap);
  textPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 1, Qt::SolidLine, Qt::RoundCap);

  airportEmptyColor = QColor::fromRgb(150, 150, 150);
  toweredAirportColor = QColor::fromRgb(15, 70, 130);
  unToweredAirportColor = QColor::fromRgb(126, 58, 91);
}

void MapPaintLayer::textBox(ViewportParams *viewport, GeoPainter *painter, const QStringList& texts,
                            const QPen& pen, int x, int y)
{
  QFontMetrics metrics = painter->fontMetrics();
  int h = metrics.height();

  int yoffset = 0;
  painter->setPen(textBackgroundPen);
  for(const QString& t : texts)
  {
    int w = metrics.width(t);
    painter->drawRoundedRect(x - 2, y - h + metrics.descent() + yoffset, w + 4, h, 5, 5);
    yoffset += h;
  }

  yoffset = 0;
  painter->setPen(pen);
  for(const QString& t : texts)
  {
    painter->drawText(x, y + yoffset, t);
    yoffset += h;
  }
}

void MapPaintLayer::paintMark(ViewportParams *viewport, GeoPainter *painter)
{
  if(navMapWidget->getMark().isValid())
  {
    qreal x, y;
    bool hidden = false;
    bool visible = viewport->screenCoordinates(navMapWidget->getMark(), x, y, hidden);

    if(visible && !hidden)
    {
      int xc = x, yc = y;
      painter->setPen(markBackPen);

      painter->drawLine(xc, yc - 10, xc, yc + 10);
      painter->drawLine(xc - 10, yc, xc + 10, yc);

      painter->setPen(markFillPen);
      painter->drawLine(xc, yc - 8, xc, yc + 8);
      painter->drawLine(xc - 8, yc, xc + 8, yc);
    }
  }
}

QColor& MapPaintLayer::colorForAirport(const MapAirport& ap)
{
  if(!ap.isSet(SCENERY))
    return airportEmptyColor;
  else if(ap.isSet(TOWER))
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  // qDebug() << "zoom" << marbleWidget->zoom() << "distance" << marbleWidget->distance();
  // qDebug() << "render cur box" << curBox.toString(GeoDataCoordinates::Degree);

  if(navMapWidget->distance() < 500)
  {
    MapQuery mq(db);

    if(navMapWidget->viewContext() == Marble::Still || navMapWidget->distance() < 100)
    {
      ap.clear();
      mq.getAirports(curBox, ap);
    }

    painter->mapQuality();
    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255, 200)));
    QFont f = painter->font();

    if(navMapWidget->distance() > 200)
      f.setPointSizeF(f.pointSizeF() / 1.2f);

    painter->setFont(f);
    QFontMetrics metrics(f);

    for(const MapAirport& a : ap)
    {
      qreal x, y;
      bool hidden = false;
      bool visible = viewport->screenCoordinates(a.coords, x, y, hidden);

      int xc = x, yc = y;
      if(visible && !hidden)
      {
        painter->setPen(colorForAirport(a));

        if(navMapWidget->distance() < 100)
        {
          airportSymbol(painter, a, 14, xc, yc);
          xc += 16;
          textBox(viewport, painter, {a.ident, a.name}, colorForAirport(a), xc, yc);
        }
        else if(navMapWidget->distance() < 200)
        {
          airportSymbol(painter, a, 10, xc, yc);
          xc += 12;
          textBox(viewport, painter, {a.ident}, colorForAirport(a), xc, yc);
        }
        else if(navMapWidget->distance() < 500)
          painter->drawEllipse(xc, yc, 4, 4);
      }
    }
  }

  paintMark(viewport, painter);

  return true;
}

void MapPaintLayer::airportSymbol(GeoPainter *painter, const MapAirport& ap, int size, int x, int y)
{
  const QBrush localBrush = painter->brush();
  int radius = size / 2;

  painter->setBackgroundMode(Qt::OpaqueMode);
  if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
    painter->setBrush(QBrush(colorForAirport(ap)));
  else
    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255)));

  if(ap.isSet(FUEL) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
  {
    painter->setPen(QPen(QBrush(colorForAirport(ap)), size / 4, Qt::SolidLine, Qt::FlatCap));
    painter->drawLine(x, y - radius - 3, x, y + radius + 3);
    painter->drawLine(x - radius - 3, y, x + radius + 3, y);
  }

  painter->setPen(QPen(QBrush(colorForAirport(ap)), size / 5, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPoint(x, y), radius, radius);

  if(ap.isSet(MIL))
    painter->drawEllipse(QPoint(x, y), radius / 2, radius / 2);
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(localBrush);

  if(ap.isSet(CLOSED))
  {
    painter->setPen(QPen(QBrush(colorForAirport(ap)), size / 7, Qt::SolidLine, Qt::FlatCap));
    painter->drawLine(x - radius, y - radius, x + radius, y + radius);
    painter->drawLine(x - radius, y + radius, x + radius, y - radius);
  }

  if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
  {
    painter->translate(x, y);
    painter->rotate(ap.longestRunwayHeading);
    painter->setPen(QPen(QBrush(QColor::fromRgb(255, 255, 255)), size / 5, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(0, -radius + 2, 0, radius - 2);
    painter->resetTransform();
  }
}

const MapAirport *MapPaintLayer::getAirportAtPos(int xs, int ys)
{
  const ViewportParams *vp = navMapWidget->viewport();

  for(const MapAirport& a : ap)
  {
    qreal x, y;
    bool visible = vp->screenCoordinates(a.coords, x, y);
    int xa = x, ya = y;

    if(visible)
      if((std::abs(xs - xa) + std::abs(ys - ya)) < 10)
        return &a;
  }

  return nullptr;
}
