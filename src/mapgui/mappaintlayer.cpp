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
#include "maplayersettings.h"
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
#include <QElapsedTimer>

using namespace Marble;

MapPaintLayer::MapPaintLayer(NavMapWidget *widget, atools::sql::SqlDatabase *sqlDb)
  : navMapWidget(widget), db(sqlDb)
{
  markBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);
  markFillPen = QPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);

  textBackgroundPen = QPen(QBrush(QColor::fromRgb(255, 255, 255)), 1, Qt::SolidLine, Qt::RoundCap);
  textPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 1, Qt::SolidLine, Qt::RoundCap);

  airportEmptyColor = QColor::fromRgb(150, 150, 150);
  toweredAirportColor = QColor::fromRgb(15, 70, 130);
  delete layers;

  unToweredAirportColor = QColor::fromRgb(126, 58, 91);

  initLayers();
}

MapPaintLayer::~MapPaintLayer()
{
  delete layers;
}

void MapPaintLayer::initLayers()
{
  if(layers != nullptr)
    delete layers;

  layers = new MapLayerSettings();

  MapLayer defApLayer = MapLayer(0, 0).airports().airportName().airportIdent().
                        airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL);
  layers->
  append(defApLayer.clone(0, 5).airportDiagram().airportSymbolSize(20).airportInfo()).
  append(defApLayer.clone(5, 50).airportSymbolSize(18).airportInfo()).
  append(defApLayer.clone(50, 100).airportSymbolSize(14)).
  append(defApLayer.clone(100, 150).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false)).
  append(defApLayer.clone(150, 300).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM)).
  append(defApLayer.clone(300, 1200).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE));
  layers->finishAppend();
  qDebug() << *layers;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos);
  Q_UNUSED(layer);

  int dist = static_cast<int>(navMapWidget->distance());

  const MapLayer *mapLayer = layers->getLayer(dist);

  if(mapLayer != nullptr)
  {
    const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
    QElapsedTimer t;
    t.start();

    if(navMapWidget->viewContext() == Marble::Still || airports.size() < 200)
    {
      airports.clear();
      MapQuery mq(db);
      mq.getAirports(curBox, mapLayer, airports);
    }

    if(navMapWidget->viewContext() == Marble::Still)
    {
      qDebug() << "Number of aiports" << airports.size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *mapLayer;
      t.restart();
    }

    if(navMapWidget->viewContext() == Marble::Still)
      painter->setRenderHint(QPainter::Antialiasing, true);
    else if(navMapWidget->viewContext() == Marble::Animation)
      painter->setRenderHint(QPainter::Antialiasing, false);

    QStringList texts;

    for(const MapAirport& airport : airports)
    {
      int x, y;
      bool visible = worldToScreen(airport.coords, x, y);

      if(!visible)
      {
        GeoDataLatLonBox apbox(airport.bounding.getNorth(), airport.bounding.getSouth(),
                               airport.bounding.getEast(), airport.bounding.getWest(),
                               GeoDataCoordinates::Degree);
        visible = curBox.intersects(apbox);
      }

      if(visible)
      {
        airportSymbol(painter, airport, x, y, mapLayer, navMapWidget->viewContext() == Marble::Animation);
        x += mapLayer->getAirportSymbolSize() + 2;

        texts.clear();

        if(mapLayer->isAirportInfo())
        {
          texts.append(airport.name + " (" + airport.ident + ")");

          if(airport.altitude > 0 || airport.longestRunwayLength > 0 || airport.isSet(LIGHT))
          {
            QString tower = (airport.towerFrequency == 0 ? QString() :
                             "CT - " + QString::number(airport.towerFrequency / 1000., 'f', 2));

            QString autoWeather;
            if(airport.atisFrequency > 0)
              autoWeather = "ATIS " + QString::number(airport.atisFrequency / 1000., 'f', 2);
            else if(airport.awosFrequency > 0)
              autoWeather = "AWOS " + QString::number(airport.awosFrequency / 1000., 'f', 2);
            else if(airport.asosFrequency > 0)
              autoWeather = "ASOS " + QString::number(airport.asosFrequency / 1000., 'f', 2);

            if(!tower.isEmpty() || !autoWeather.isEmpty())
              texts.append(tower + (tower.isEmpty() ? QString() : " ") + autoWeather);

            texts.append(QString::number(airport.altitude) + " " +
                         (airport.isSet(LIGHT) ? "L " : "- ") +
                         QString::number(airport.longestRunwayLength / 100) + " " +
                         (airport.unicomFrequency == 0 ? QString() :
                          QString::number(airport.unicomFrequency / 1000., 'f', 2)));
          }
        }
        else if(mapLayer->isAirportIdent())
          texts.append(airport.ident);
        else if(mapLayer->isAirportName())
          texts.append(airport.name);

        if(!texts.isEmpty())
          textBox(painter, airport, texts, colorForAirport(airport), x, y);

        if(mapLayer->isAirportDiagram())
          airportDiagram(painter, airport, x, y);
      }
    }
    if(navMapWidget->viewContext() == Marble::Still)
      qDebug() << "Time for paint" << t.elapsed() << " ms";
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

void MapPaintLayer::airportSymbol(GeoPainter *painter, const MapAirport& ap, int x, int y,
                                  const MapLayer *mapLayer, bool fast)
{
  QColor apColor = colorForAirport(ap);
  const QBrush localBrush = painter->brush();
  int size = mapLayer->getAirportSymbolSize();
  int radius = size / 2;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(ap.longestRunwayLength >= 8000 && mapLayer->isAirportOverviewRunway() && !ap.isSet(CLOSED) &&
     !ap.waterOnly())
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

    if(!fast)
    {
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
  }
  else
  {
    if(ap.isSet(HARD) && !ap.isSet(MIL) && !ap.isSet(CLOSED))
      // Use filled circle
      painter->setBrush(QBrush(apColor));
    else
      // Use white filled circle
      painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255)));

    if(!fast)
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

    if(!fast)
    {
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
    }

    if(!fast)
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

const MapAirport MapPaintLayer::getAirportAtPos(int xs, int ys)
{
  for(const MapAirport& a : airports)
  {
    int x, y;
    bool visible = worldToScreen(a.coords, x, y);

    if(visible)
      if((std::abs(x - xs) + std::abs(y - ys)) < 10)
        return a;
  }

  return MapAirport();
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

void MapPaintLayer::textBox(GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
                            const QPen& pen, int x, int y)
{
  painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255)));

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

QColor& MapPaintLayer::colorForAirport(const MapAirport& ap)
{
  if(!ap.isSet(SCENERY) && !ap.waterOnly())
    return airportEmptyColor;
  else if(ap.isSet(TOWER))
    return toweredAirportColor;
  else
    return unToweredAirportColor;
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
