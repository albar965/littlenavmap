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
#include "geo/calculations.h"
#include <cmath>
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

void MapPaintLayer::textBox(GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
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

  QFont f = painter->font();
  if(ap.isSet(ADDON))
  {
    f.setBold(true);
    painter->setFont(f);
  }

  yoffset = 0;
  painter->setPen(pen);
  for(const QString& t : texts)
  {
    painter->drawText(x, y + yoffset, t);
    yoffset += h;
  }

  if(ap.isSet(ADDON))
  {
    f.setBold(false);
    painter->setFont(f);
  }
}

void MapPaintLayer::paintMark(GeoPainter *painter)
{
  if(navMapWidget->getMark().isValid())
  {
    int x, y;
    bool visible = worldToScreen(navMapWidget->getMark(), x, y);

    if(visible)
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
  if(!ap.isSet(SCENERY) && !ap.waterOnly())
    return airportEmptyColor;
  else if(ap.isSet(TOWER))
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos);
  Q_UNUSED(layer);
  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();

  if(navMapWidget->distance() < 800)
  {
    MapQuery mq(db);

    if(navMapWidget->viewContext() == Marble::Still || navMapWidget->distance() < 100)
    {
      airports.clear();
      mq.getAirports(curBox, airports);
    }

    if(navMapWidget->viewContext() == Marble::Still)
      painter->setRenderHint(QPainter::Antialiasing, true);
    else if(navMapWidget->viewContext() == Marble::Animation)
      painter->setRenderHint(QPainter::Antialiasing, false);

    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255, 200)));
    QFont f = painter->font();
    if(navMapWidget->distance() > 200)
      f.setPointSizeF(f.pointSizeF() / 1.2f);

    painter->setFont(f);

    for(const MapAirport& a : airports)
    {
      int x, y;
      bool visible = worldToScreen(a.coords, x, y);

      GeoDataLatLonBox apbox(a.bounding.getNorth(), a.bounding.getSouth(),
                             a.bounding.getEast(), a.bounding.getWest(),
                             GeoDataCoordinates::Degree);

      if(!visible)
        visible = curBox.intersects(apbox);

      if(visible)
      {
        painter->setPen(colorForAirport(a));

        if(navMapWidget->distance() < 5)
          airportDiagram(painter, a, x, y);
        // x += 20;
        // textBox(painter, a, {a.ident, a.name}, colorForAirport(a), x, y);
        else if(navMapWidget->distance() < 50)
        {
          airportSymbol(painter, a, 18, x, y);
          x += 20;
          textBox(painter, a, {a.ident, a.name}, colorForAirport(a), x, y);
        }
        else if(navMapWidget->distance() < 100)
        {
          airportSymbol(painter, a, 14, x, y);
          x += 16;
          textBox(painter, a, {a.ident, a.name}, colorForAirport(a), x, y);
        }
        else if(navMapWidget->distance() < 200)
        {
          airportSymbol(painter, a, 10, x, y);
          x += 12;
          textBox(painter, a, {a.ident}, colorForAirport(a), x, y);
        }
//        else if(navMapWidget->distance() < 800)
//          if(a.longestRunwayLength > 8000)
//          {
//            airportSymbol(painter, a, 10, x, y);
//            x += 12;
//            textBox(painter, a, {a.ident}, colorForAirport(a), x, y);
//          }
      }
    }
  }

  paintMark(painter);

  return true;
}

bool MapPaintLayer::worldToScreen(const atools::geo::Pos& coords, int& x, int& y)
{
  const ViewportParams *viewport = navMapWidget->viewport();

  qreal xr, yr;
  bool hidden = false;
  bool visible = viewport->screenCoordinates(
    Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0,
                               GeoDataCoordinates::Degree), xr, yr, hidden);

  x = static_cast<int>(std::round(xr));
  y = static_cast<int>(std::round(yr));
  return visible && !hidden;
}

bool MapPaintLayer::worldToScreen(const Marble::GeoDataCoordinates& coords, int& x, int& y)
{
  const ViewportParams *viewport = navMapWidget->viewport();

  qreal xr, yr;
  bool hidden = false;
  bool visible = viewport->screenCoordinates(coords, xr, yr, hidden);

  x = static_cast<int>(std::round(xr));
  y = static_cast<int>(std::round(yr));
  return visible && !hidden;
}

bool MapPaintLayer::screenToWorld(int x, int y, Marble::GeoDataCoordinates& coords)
{
  const ViewportParams *viewport = navMapWidget->viewport();

  qreal lon, lat;

  bool visible = viewport->geoCoordinates(x, y, lon, lat, GeoDataCoordinates::Degree);

  coords.setLongitude(lon, GeoDataCoordinates::Degree);
  coords.setLatitude(lat, GeoDataCoordinates::Degree);
  return visible;
}

void MapPaintLayer::airportDiagram(GeoPainter *painter, const MapAirport& ap, int x, int y)
{
  Q_UNUSED(x);
  Q_UNUSED(y);
  const QBrush localBrush = painter->brush();
  painter->setBackgroundMode(Qt::OpaqueMode);

  QList<MapRunway> rw;
  MapQuery mq(db);
  mq.getRunways(ap.id, rw);

  QList<QPoint> centers;
  QList<QRect> rects, innerRects;
  runwayCoords(rw, centers, rects, innerRects);

  painter->setBrush(QColor::fromRgb(80, 80, 80));
  painter->setPen(QPen(QColor::fromRgb(80, 80, 80), 1, Qt::SolidLine, Qt::FlatCap));
  for(int i = 0; i < centers.size(); i++)
  {
    painter->translate(centers.at(i));
    painter->rotate(rw.at(i).heading);
    painter->drawRect(rects.at(i));
    painter->resetTransform();
  }
  painter->setBrush(localBrush);
  painter->setBackgroundMode(Qt::TransparentMode);
}

void MapPaintLayer::airportSymbol(GeoPainter *painter, const MapAirport& ap, int size, int x, int y)
{
  QColor apColor = colorForAirport(ap);
  const QBrush localBrush = painter->brush();
  int radius = size / 2;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(ap.longestRunwayLength >= 8000 && navMapWidget->distance() < 100 && !ap.isSet(CLOSED) && !ap.waterOnly())
  {
    MapQuery mq(db);

    QList<MapRunway> rw;
    mq.getRunwaysForOverview(ap.id, rw);

    QList<QPoint> centers;
    QList<QRect> rects, innerRects;
    runwayCoords(rw, centers, rects, innerRects);

    painter->setBrush(QBrush(apColor));
    painter->setPen(QPen(QBrush(apColor), 1, Qt::SolidLine, Qt::FlatCap));
    for(int i = 0; i < centers.size(); i++)
    {
      painter->translate(centers.at(i));
      painter->rotate(rw.at(i).heading);
      painter->drawRect(rects.at(i));
      painter->resetTransform();
    }

    painter->setPen(QPen(QBrush(QColor::fromRgb(255, 255, 255)), 1, Qt::SolidLine, Qt::FlatCap));
    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255)));
    for(int i = 0; i < centers.size(); i++)
    {
      painter->translate(centers.at(i));
      painter->rotate(rw.at(i).heading);
      painter->drawRect(innerRects.at(i));
      painter->resetTransform();
    }
  }
  else
  {
    if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
      // Use filled circle
      painter->setBrush(QBrush(apColor));
    else
      // Use white filled circle
      painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255)));

    if(ap.isSet(FUEL) && !ap.isSet(MIL) && !ap.isSet(CLOSED) && size > 6)
    {
      // Draw fuel spikes
      int fuelRadius = static_cast<int>(std::round(static_cast<double>(radius) * 1.4));
      painter->setPen(QPen(QBrush(apColor), size / 4, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x, y - fuelRadius, x, y + fuelRadius);
      painter->drawLine(x - fuelRadius, y, x + fuelRadius, y);
    }

    painter->setPen(QPen(QBrush(apColor), size / 5, Qt::SolidLine, Qt::FlatCap));
    painter->drawEllipse(QPoint(x, y), radius, radius);

    if(ap.isSet(MIL))
      painter->drawEllipse(QPoint(x, y), radius / 2, radius / 2);

    if(ap.isSet(WATER) && !ap.isSet(HARD) && !ap.isSet(SOFT) && size > 6)
    {
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4, y - radius / 2, x - lineWidth / 4, y + radius / 2);
      painter->drawArc(x - radius / 2, y - radius / 2, radius, radius, 0 * 16, -180 * 16);
    }

    if(ap.isSet(CLOSED) && size > 6)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), size / 7, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - radius, y - radius, x + radius, y + radius);
      painter->drawLine(x - radius, y + radius, x + radius, y - radius);
    }

    if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED) && size > 6)
    {
      painter->translate(x, y);
      painter->rotate(ap.longestRunwayHeading);
      painter->setPen(QPen(QBrush(QColor::fromRgb(255, 255, 255)), size / 5, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(0, -radius + 2, 0, radius - 2);
      painter->resetTransform();
    }

  }
  painter->setBrush(localBrush);
  painter->setBackgroundMode(Qt::TransparentMode);
}

const MapAirport *MapPaintLayer::getAirportAtPos(int xs, int ys)
{
  for(const MapAirport& a : airports)
  {
    int x, y;
    bool visible = worldToScreen(a.coords, x, y);

    if(visible)
      if((std::abs(x - xs) + std::abs(y - ys)) < 10)
        return &a;
  }

  return nullptr;
}

void MapPaintLayer::runwayCoords(const QList<MapRunway>& rw, QList<QPoint>& centers, QList<QRect>& rects,
                                 QList<QRect>& innerRects)
{
  for(const MapRunway& r : rw)
  {
    // Get the two endpoints as screen coords
    int xr1, yr1, xr2, yr2;
    worldToScreen(r.primary, xr1, yr1);
    worldToScreen(r.secondary, xr2, yr2);

    // Get the center point as screen coords
    int xc, yc;
    worldToScreen(r.center, xc, yc);
    centers.append(QPoint(xc, yc));

    // Get an approximation of the runway length on the screen
    int length = static_cast<int>(std::round(sqrt((xr1 - xr2) * (xr1 - xr2) + (yr1 - yr2) * (yr1 - yr2))));

    rects.append(QRect(-3, -length / 2, 6, length));
    innerRects.append(QRect(-1, -length / 2 + 2, 2, length - 4));
  }

}
