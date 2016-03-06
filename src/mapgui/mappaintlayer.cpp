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
using namespace atools::geo;

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

  append(defApLayer.clone(100, 150).airportSymbolSize(10).minRunwayLength(2500).
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

    if(navMapWidget->viewContext() == Marble::Still || airports.size() < 100)
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
        if(mapLayer->isAirportDiagram())
          airportDiagram(painter, airport, x, y);
        else
          airportSymbolOverview(painter, airport, mapLayer,
                                navMapWidget->viewContext() == Marble::Animation);

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

bool MapPaintLayer::worldToScreen(const atools::geo::Pos& coords, float& x, float& y)
{
  const ViewportParams *viewport = navMapWidget->viewport();

  qreal xr, yr;
  bool hidden = false;
  bool visible = viewport->screenCoordinates(
    Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0,
                               GeoDataCoordinates::Degree), xr, yr, hidden);

  x = static_cast<float>(xr);
  y = static_cast<float>(yr);
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

QPoint MapPaintLayer::worldToScreen(const atools::geo::Pos& coords, bool *visible)
{
  int xr, yr;
  bool isVisible = worldToScreen(coords, xr, yr);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPoint(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPoint(xr, yr);
    else
      return QPoint();
  }
}

QPoint MapPaintLayer::worldToScreen(const Marble::GeoDataCoordinates& coords, bool *visible)
{
  int xr, yr;
  bool isVisible = worldToScreen(coords, xr, yr);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPoint(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPoint(xr, yr);
    else
      return QPoint();
  }
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
  painter->save();
  painter->setBackgroundMode(Qt::OpaqueMode);
  MapQuery mq(db);

  QList<MapRunway> rw;
  mq.getRunways(ap.id, rw);

  QList<MapApron> aprons;
  mq.getAprons(ap.id, aprons);

  QList<QPoint> centers;
  QList<QRect> rects, backRects;
  runwayCoords(rw, &centers, &rects, nullptr, &backRects);

  QList<MapTaxiPath> taxipaths;
  mq.getTaxiPaths(ap.id, taxipaths);
  QVector<QPoint> points;

  for(int i = 0; i < centers.size(); i++)
    if(rw.at(i).surface != "WATER")
    {
      painter->translate(centers.at(i));
      painter->rotate(rw.at(i).heading);

      painter->setBrush(QColor::fromRgb(255, 255, 255));
      painter->setPen(QPen(QColor(Qt::white), 1, Qt::SolidLine, Qt::FlatCap));
      const QRect backRect = backRects.at(i);
      painter->drawRect(backRect);

      painter->resetTransform();
    }
  painter->setBrush(QColor::fromRgb(255, 255, 255));
  painter->setPen(QPen(QColor(Qt::white), 40, Qt::SolidLine, Qt::RoundCap));

  for(const MapTaxiPath& tp : taxipaths)
  {
    bool visible;

    QPoint start = worldToScreen(tp.start, &visible);
    QPoint end = worldToScreen(tp.end, &visible);
    painter->drawLine(start, end);
  }

  for(const MapApron& apron : aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(worldToScreen(pos, &visible));

    painter->QPainter::drawPolyline(points.data(), points.size());
  }

  for(const MapTaxiPath& tp : taxipaths)
  {
    bool visible;
    painter->setBrush(colorForSurface(tp.surface));
    painter->setPen(QPen(QColor(colorForSurface(tp.surface)), 5, Qt::SolidLine, Qt::RoundCap));

    QPoint start = worldToScreen(tp.start, &visible);
    QPoint end = worldToScreen(tp.end, &visible);
    painter->drawLine(start, end);
  }

  for(const MapApron& apron : aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(worldToScreen(pos, &visible));

    painter->setBrush(QBrush(colorForSurface(apron.surface), Qt::Dense1Pattern));
    painter->setPen(QPen(colorForSurface(apron.surface), 1, Qt::SolidLine, Qt::FlatCap));
    painter->QPainter::drawPolygon(points.data(), points.size());
  }

  for(int i = 0; i < centers.size(); i++)
  {
    painter->translate(centers.at(i));
    painter->rotate(rw.at(i).heading);

    painter->setBrush(colorForSurface(rw.at(i).surface));
    painter->setPen(QPen(QColor(Qt::black), 1, Qt::SolidLine, Qt::FlatCap));
    painter->drawRect(rects.at(i));

    painter->resetTransform();
  }

  painter->restore();
}

void MapPaintLayer::airportSymbolOverview(GeoPainter *painter, const MapAirport& ap,
                                          const MapLayer *mapLayer, bool fast)
{
  if(ap.longestRunwayLength >= 8000 && mapLayer->isAirportOverviewRunway() && !ap.isSet(CLOSED) &&
     !ap.waterOnly())
  {
    painter->save();

    QColor apColor = colorForAirport(ap);
    painter->setBackgroundMode(Qt::OpaqueMode);
    MapQuery mq(db);

    QList<MapRunway> rw;
    mq.getRunwaysForOverview(ap.id, rw);

    QList<QPoint> centers;
    QList<QRect> rects, innerRects;
    runwayCoords(rw, &centers, &rects, &innerRects, nullptr);

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
    painter->restore();
  }
}

void MapPaintLayer::airportSymbol(GeoPainter *painter, const MapAirport& ap, int x, int y,
                                  const MapLayer *mapLayer, bool fast)
{
  if(!mapLayer->isAirportOverviewRunway() || ap.isSet(CLOSED) || ap.waterOnly() ||
     ap.longestRunwayLength < 8000 || mapLayer->isAirportDiagram())
  {
    painter->save();

    QColor apColor = colorForAirport(ap);
    int size = mapLayer->getAirportSymbolSize();
    int radius = size / 2;
    painter->setBackgroundMode(Qt::OpaqueMode);

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

    painter->restore();
  }
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

void MapPaintLayer::runwayCoords(const QList<MapRunway>& rw, QList<QPoint> *centers, QList<QRect> *rects,
                                 QList<QRect> *innerRects, QList<QRect> *backRects)
{
  for(const MapRunway& r : rw)
  {
    // Get the two endpoints as screen coords
    float xr1, yr1, xr2, yr2;
    worldToScreen(r.primary, xr1, yr1);
    worldToScreen(r.secondary, xr2, yr2);

    // Get the center point as screen coords
    float xc, yc;
    worldToScreen(r.center, xc, yc);
    if(centers != nullptr)
      centers->append(QPoint(static_cast<int>(std::round(xc)), static_cast<int>(std::round(yc))));

    // Get an approximation of the runway length on the screen
    int length = static_cast<int>(std::round(sqrt((xr1 - xr2) * (xr1 - xr2) + (yr1 - yr2) * (yr1 - yr2))));

    int width = 6;
    if(r.width > 0)
    {
      Pos wPos = r.center.endpoint(feetToMeter(static_cast<float>(r.width)),
                                   static_cast<float>(r.heading + 90));

      float xw, yw;
      worldToScreen(wPos, xw, yw);

      // Get an approximation of the runway width on the screen
      width = static_cast<int>(std::round(sqrt((xc - xw) * (xc - xw) + (yc - yw) * (yc - yw)))) * 2;
    }

    if(backRects != nullptr)
      backRects->append(QRect(-width * 4, -length / 2 - width * 4, width * 8, length + width * 8));
    if(rects != nullptr)
      rects->append(QRect(-(width / 2), -length / 2, width, length));
    if(innerRects != nullptr)
      innerRects->append(QRect(-(width / 6), -length / 2 + 2, width - 4, length - 4));
  }

}

void MapPaintLayer::textBox(GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
                            const QPen& pen, int x, int y)
{
  painter->save();
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
  painter->restore();
}

void MapPaintLayer::paintMark(GeoPainter *painter)
{
  if(navMapWidget->getMark().isValid())
  {
    int x, y;
    bool visible = worldToScreen(navMapWidget->getMark(), x, y);

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

QColor MapPaintLayer::colorForSurface(const QString& surface)
{
  if(surface == "CONCRETE")
    return QColor(Qt::lightGray);
  else if(surface == "GRASS")
    return QColor(Qt::green);
  else if(surface == "WATER")
    return QColor(Qt::blue);
  else if(surface == "ASPHALT")
    return QColor(Qt::darkGray);
  else if(surface == "CEMENT")
    return QColor(Qt::lightGray);
  else if(surface == "CLAY")
    return QColor(Qt::gray);
  else if(surface == "SNOW")
    return QColor(Qt::white);
  else if(surface == "ICE")
    return QColor(Qt::white);
  else if(surface == "DIRT")
    return QColor(Qt::gray);
  else if(surface == "CORAL")
    return QColor(Qt::gray);
  else if(surface == "GRAVEL")
    return QColor(Qt::gray);
  else if(surface == "OIL_TREATED")
    return QColor(Qt::darkGray);
  else if(surface == "STEEL_MATS")
    return QColor(Qt::gray);
  else if(surface == "BITUMINOUS")
    return QColor(Qt::darkGray);
  else if(surface == "BRICK")
    return QColor(Qt::gray);
  else if(surface == "MACADAM")
    return QColor(Qt::gray);
  else if(surface == "PLANKS")
    return QColor(Qt::gray);
  else if(surface == "SAND")
    return QColor(Qt::lightGray);
  else if(surface == "SHALE")
    return QColor(Qt::lightGray);
  else if(surface == "TARMAC")
    return QColor(Qt::gray);

  // else if(rw.surface == "UNKNOWN")
  return QColor(Qt::black);
}
