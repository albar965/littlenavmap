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

#include "mappainterairport.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "maptypes.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;

MapPainterAirport::MapPainterAirport(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(widget, mapQuery, mapScale)
{
  airportEmptyColor = QColor::fromRgb(150, 150, 150);
  toweredAirportColor = QColor::fromRgb(15, 70, 130);
  unToweredAirportColor = QColor::fromRgb(126, 58, 91);

  airportDetailBackColor = QColor(Qt::white);
}

void MapPainterAirport::paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                              Marble::ViewportParams *viewport)
{
  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  if(widget->viewContext() == Marble::Still || airports.size() < 100)
  {
    airports.clear();
    query->getAirports(curBox, mapLayer, airports);
  }

  if(widget->viewContext() == Marble::Still)
  {
    qDebug() << "Number of aiports" << airports.size();
    qDebug() << "Time for query" << t.elapsed() << " ms";
    qDebug() << curBox.toString();
    qDebug() << *mapLayer;
    t.restart();
  }

  if(widget->viewContext() == Marble::Still)
    painter->setRenderHint(QPainter::Antialiasing, true);
  else if(widget->viewContext() == Marble::Animation)
    painter->setRenderHint(QPainter::Antialiasing, false);

  for(const MapAirport& airport : airports)
  {
    int x, y;
    bool visible = wToS(airport.coords, x, y);

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
        airportDiagram(mapLayer, painter, airport);
      else
        airportSymbolOverview(painter, airport, mapLayer,
                              widget->viewContext() == Marble::Animation);

      airportSymbol(painter, airport, x, y, mapLayer, widget->viewContext() == Marble::Animation);
      x += mapLayer->getAirportSymbolSize() + 2;

      QStringList texts = airportTexts(mapLayer, airport);

      if(!texts.isEmpty())
        textBox(painter, airport, texts, colorForAirport(airport), x, y, mapLayer->isAirportDiagram());
    }
  }
  if(widget->viewContext() == Marble::Still)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}

MapAirport MapPainterAirport::getAirportAtPos(int xs, int ys)
{
  for(int i = airports.size() - 1; i >= 0; i--)
  {
    int x, y;
    bool visible = wToS(airports.at(i).coords, x, y);

    if(visible)
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < 10)
        return airports.at(i);
  }
  return MapAirport();
}

void MapPainterAirport::airportDiagram(const MapLayer *mapLayer, GeoPainter *painter,
                                       const MapAirport& airport)
{
  painter->save();
  painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setBrush(airportDetailBackColor);
  painter->setPen(QPen(airportDetailBackColor, scale->getPixelIntForMeter(200.f), Qt::SolidLine, Qt::RoundCap));

  QFont defaultFont = painter->font();
  defaultFont.setBold(true);
  painter->setFont(defaultFont);

  const QList<MapRunway> *runways = query->getRunways(airport.id);
  QList<QPoint> runwayCenters;
  QList<QRect> runwayRects, runwayBackRects;
  runwayCoords(runways, &runwayCenters, &runwayRects, nullptr, &runwayBackRects);

  // Draw white background ---------------------------------
  for(int i = 0; i < runwayCenters.size(); i++)
    if(runways->at(i).surface != "WATER")
    {
      painter->translate(runwayCenters.at(i));
      painter->rotate(runways->at(i).heading);

      const QRect backRect = runwayBackRects.at(i);
      painter->drawRect(backRect);

      painter->resetTransform();
    }

  const QList<MapTaxiPath> *taxipaths = query->getTaxiPaths(airport.id);
  for(const MapTaxiPath& taxipath : *taxipaths)
  {
    bool visible;
    QPoint start = wToS(taxipath.start, &visible);
    QPoint end = wToS(taxipath.end, &visible);
    painter->drawLine(start, end);
  }

  const QList<MapApron> *aprons = query->getAprons(airport.id);
  QVector<QPoint> points;
  for(const MapApron& apron : *aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, &visible));

    painter->QPainter::drawPolyline(points.data(), points.size());
  }

  // Draw aprons ---------------------------------
  for(const MapApron& apron : *aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, &visible));

    QColor col = colorForSurface(apron.surface);
    painter->setBrush(QBrush(col, Qt::Dense2Pattern));
    painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));
    painter->QPainter::drawPolygon(points.data(), points.size());
  }

  // Draw taxiways ---------------------------------
  for(const MapTaxiPath& taxipath : *taxipaths)
  {
    int pathThickness = scale->getPixelIntForFeet(taxipath.width);
    QColor col = colorForSurface(taxipath.surface);
    painter->setBrush(col);
    painter->setPen(QPen(col, pathThickness, Qt::SolidLine, Qt::RoundCap));

    bool visible;
    QPoint start = wToS(taxipath.start, &visible);
    QPoint end = wToS(taxipath.end, &visible);
    painter->drawLine(start, end);
  }

  // Draw taxiway names ---------------------------------
  // TODO workaround - add nameplacement hints in compiler
  if(mapLayer->isAirportDiagramDetail())
  {
    painter->setBackgroundMode(Qt::TransparentMode);

    QFont font = painter->font();
    font.setPixelSize(12);
    painter->setFont(font);

    painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));

    QMultiMap<QString, MapTaxiPath> map;
    for(const MapTaxiPath& taxipath : *taxipaths)
      if(!taxipath.name.isEmpty())
      {
        bool visible;
        wToS(taxipath.start, &visible);
        wToS(taxipath.end, &visible);
        if(visible)
          map.insert(taxipath.name, taxipath);
      }

    for(QString taxiname : map.keys())
    {
      QList<MapTaxiPath> paths = map.values(taxiname);
      QList<MapTaxiPath> paths2;

      paths2.append(paths.first());
      if(paths.size() > 2)
        paths2.append(paths.at(paths.size() / 2));
      paths2.append(paths.last());

      for(const MapTaxiPath& taxipath : paths2)
      {

        bool visible;
        QPoint start = wToS(taxipath.start, &visible);
        QPoint end = wToS(taxipath.end, &visible);

        int w = painter->fontMetrics().width(taxiname);
        int h = painter->fontMetrics().height();

        int length = atools::geo::simpleDistance(start.x(), start.y(), end.x(), end.y());
        if(length > 40)
          painter->drawText(QPoint(((start.x() + end.x()) / 2) - w / 2,
                                   ((start.y() + end.y()) / 2) + h / 2), taxiname);
      }
    }
    painter->setBackgroundMode(Qt::OpaqueMode);
  }

  // Draw runway outlines --------------------------------
  painter->setPen(QPen(QColor(Qt::black), 1, Qt::SolidLine, Qt::FlatCap));
  painter->setBrush(QColor(Qt::black));
  for(int i = 0; i < runwayCenters.size(); i++)
  {
    painter->translate(runwayCenters.at(i));
    painter->rotate(runways->at(i).heading);
    painter->drawRect(runwayRects.at(i).marginsAdded(QMargins(2, 2, 2, 2)));
    painter->resetTransform();
  }

  // Draw runways --------------------------------
  for(int i = 0; i < runwayCenters.size(); i++)
  {
    const MapRunway& runway = runways->at(i);
    const QRect& rect = runwayRects.at(i);

    QColor col = colorForSurface(runway.surface);
    painter->setBrush(col);
    painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));

    painter->translate(runwayCenters.at(i));
    painter->rotate(runway.heading);
    painter->drawRect(rect);
    painter->resetTransform();
  }

  // Draw runways offsets --------------------------------
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(QColor(Qt::white));
  for(int i = 0; i < runwayCenters.size(); i++)
  {
    const MapRunway& runway = runways->at(i);

    if(runway.primOffset > 0 || runway.secOffset > 0)
    {
      const QRect& rect = runwayRects.at(i);

      painter->translate(runwayCenters.at(i));
      painter->rotate(runway.heading);

      if(runway.primOffset > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.primOffset, runway.heading);
        painter->setPen(QPen(QColor(Qt::white), 3, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(rect.left(), rect.bottom() - offs, rect.right(), rect.bottom() - offs);

        painter->setPen(QPen(QColor(Qt::white), 3, Qt::DashLine, Qt::FlatCap));
        painter->drawLine(0, rect.bottom(), 0, rect.bottom() - offs);
      }

      if(runway.secOffset > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.secOffset, runway.heading);
        painter->setPen(QPen(QColor(Qt::white), 3, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(rect.left(), rect.top() + offs, rect.right(), rect.top() + offs);

        painter->setPen(QPen(QColor(Qt::white), 3, Qt::DashLine, Qt::FlatCap));
        painter->drawLine(0, rect.top(), 0, rect.top() + offs);
      }
      painter->resetTransform();
    }
  }
  painter->setBackgroundMode(Qt::OpaqueMode);

  // Draw parking --------------------------------
  const QList<MapParking> *parkings = query->getParking(airport.id);
  if(!parkings->isEmpty())
    painter->setPen(QPen(QColor::fromRgb(80, 80, 80), 2, Qt::SolidLine, Qt::FlatCap));
  for(const MapParking& parking : *parkings)
  {
    bool visible;
    QPoint pt = wToS(parking.pos, &visible);
    if(visible)
    {
      int w = scale->getPixelIntForFeet(parking.radius, 90);
      int h = scale->getPixelIntForFeet(parking.radius, 0);

      painter->setBrush(colorForParkingType(parking.type));
      painter->drawEllipse(pt, w, h);

      if(parking.jetway && !parking.type.startsWith("FUEL"))
        painter->drawEllipse(pt, w * 3 / 4, h * 3 / 4);

      painter->translate(pt);
      painter->rotate(parking.heading);
      painter->drawLine(0, h * 2 / 3, 0, h);
      painter->resetTransform();
    }
  }

  // Draw helipads ------------------------------------------------
  const QList<MapHelipad> *helipads = query->getHelipads(airport.id);
  if(!helipads->isEmpty())
    painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
  for(const MapHelipad& helipad : *helipads)
  {
    bool visible;
    QPoint pt = wToS(helipad.pos, &visible);
    if(visible)
    {
      painter->setBrush(colorForSurface(helipad.surface));

      int w = scale->getPixelIntForFeet(helipad.width, 90) / 2;
      int h = scale->getPixelIntForFeet(helipad.length, 0) / 2;

      painter->drawEllipse(pt, w, h);

      painter->translate(pt);
      painter->rotate(helipad.heading);
      painter->drawLine(-w / 3, -h / 2, -w / 3, h / 2);
      painter->drawLine(-w / 3, 0, w / 3, 0);
      painter->drawLine(w / 3, -h / 2, w / 3, h / 2);
      painter->resetTransform();
    }
  }

  // Draw tower -------------------------------------------------
  if(airport.towerCoords.isValid())
  {
    bool visible;
    QPoint pt = wToS(airport.towerCoords, &visible);
    if(visible)
    {
      if(airport.towerFrequency > 0)
      {
        painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(QColor(Qt::red));
      }
      else
      {
        painter->setPen(QPen(QColor(Qt::darkGray), 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(QColor(Qt::lightGray));
      }

      int w = scale->getPixelIntForMeter(10, 90);
      int h = scale->getPixelIntForMeter(10, 0);
      painter->drawEllipse(pt, w < 5 ? 5 : w, h < 5 ? 5 : h);
    }
  }

  painter->setBackgroundMode(Qt::TransparentMode);

  // Draw parking and tower texts -------------------------------------------------
  if(mapLayer->isAirportDiagramDetail())
  {

    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);

    for(const MapParking& parking : *parkings)
      if(mapLayer->isAirportDiagramDetail2() || parking.radius > 40)
      {
        bool visible;
        QPoint pt = wToS(parking.pos, &visible);
        if(visible)
        {
          if(parking.type.startsWith("RAMP_GA") || parking.type.startsWith("DOCK_GA") ||
             parking.type.startsWith("FUEL"))
            painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
          else
            painter->setPen(QPen(QColor(Qt::white), 2, Qt::SolidLine, Qt::FlatCap));

          QString text;
          if(parking.type.startsWith("FUEL"))
            text = "F";
          else
            text = QString::number(parking.number) + " " + parkingName(parking.name);
          pt.setY(pt.y() + painter->fontMetrics().ascent() / 2);
          pt.setX(pt.x() - painter->fontMetrics().width(text) / 2);

          painter->drawText(pt, text);
        }
      }
  }

  if(airport.towerCoords.isValid())
  {
    bool visible;
    QPoint pt = wToS(airport.towerCoords, &visible);
    if(visible)
    {
      pt.setY(pt.y() + painter->fontMetrics().ascent() / 2);
      pt.setX(pt.x() - painter->fontMetrics().width("T") / 2);
      painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
      painter->drawText(pt, "T");
    }
  }

  // Draw runway texts --------------------------------
  QFont rwTextFont = painter->font();
  rwTextFont.setPixelSize(16);
  painter->setFont(rwTextFont);

  // Draw dimensions at runway side
  painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
  for(int i = 0; i < runwayCenters.size(); i++)
  {
    const MapRunway& runway = runways->at(i);
    const QRect& rect = runwayRects.at(i);

    painter->translate(runwayCenters.at(i));
    if(runway.heading > 180)
      painter->rotate(runway.heading + 90);
    else
      painter->rotate(runway.heading - 90);

    QString text = QString::number(runway.length) + " x " + QString::number(runway.width);

    if(!runway.edgeLight.isEmpty())
      text += " / L";
    text += " / " + maptypes::surfaceName(runway.surface);

    int textWidth = painter->fontMetrics().width(text);
    if(textWidth > rect.height())
      textWidth = rect.height();
    text = painter->fontMetrics().elidedText(text, Qt::ElideRight, rect.height());

    painter->drawText(-textWidth / 2,
                      -rect.width() / 2 - painter->fontMetrics().descent(),
                      text);
    painter->resetTransform();
  }

  rwTextFont.setPixelSize(20);
  painter->setFont(rwTextFont);

  // Draw numbers at end
  const int crossSize = 10;

  for(int i = 0; i < runwayCenters.size(); i++)
  {
    const MapRunway& runway = runways->at(i);
    bool primaryVisible, secondaryVisible;
    QPoint prim = wToS(runway.primary, &primaryVisible);
    QPoint sec = wToS(runway.secondary, &secondaryVisible);

    if(primaryVisible)
    {
      painter->translate(prim);
      painter->rotate(runway.heading + 180);

      painter->drawText(-painter->fontMetrics().width(runway.secName) / 2,
                        painter->fontMetrics().ascent(), runway.secName);

      if(runway.secClosed)
      {
        // Cross out runway number
        painter->drawLine(-crossSize, -crossSize + 10, crossSize, crossSize + 10);
        painter->drawLine(-crossSize, crossSize + 10, crossSize, -crossSize + 10);
      }
      painter->resetTransform();
    }

    if(secondaryVisible)
    {
      painter->translate(sec);
      painter->rotate(runway.heading - 180);
      painter->drawText(-painter->fontMetrics().width(runway.primName) / 2,
                        -painter->fontMetrics().descent() - 2, runway.primName);

      if(runway.primClosed)
      {
        // Cross out runway number
        painter->drawLine(-crossSize, -crossSize - 10, crossSize, crossSize - 10);
        painter->drawLine(-crossSize, crossSize - 10, crossSize, -crossSize - 10);
      }
      painter->resetTransform();
    }
  }
  painter->restore();
}

void MapPainterAirport::airportSymbolOverview(GeoPainter *painter, const MapAirport& ap,
                                              const MapLayer *mapLayer, bool fast)
{
  if(ap.longestRunwayLength >= 8000 && mapLayer->isAirportOverviewRunway() && !ap.isSet(CLOSED) &&
     !ap.waterOnly())
  {
    painter->save();

    QColor apColor = colorForAirport(ap);
    painter->setBackgroundMode(Qt::OpaqueMode);

    const QList<MapRunway> *rw = query->getRunwaysForOverview(ap.id);

    QList<QPoint> centers;
    QList<QRect> rects, innerRects;
    runwayCoords(rw, &centers, &rects, &innerRects, nullptr);

    painter->setBrush(QBrush(apColor));
    painter->setPen(QPen(QBrush(apColor), 1, Qt::SolidLine, Qt::FlatCap));
    for(int i = 0; i < centers.size(); i++)
    {
      painter->translate(centers.at(i));
      painter->rotate(rw->at(i).heading);
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
        painter->rotate(rw->at(i).heading);
        painter->drawRect(innerRects.at(i));
        painter->resetTransform();
      }
    }
    painter->restore();
  }
}

void MapPainterAirport::airportSymbol(GeoPainter *painter, const MapAirport& ap, int x, int y,
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

      if(ap.waterOnly() && size > 6)
      {
        int lineWidth = size / 7;
        painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(x - lineWidth / 4, y - radius / 2, x - lineWidth / 4, y + radius / 2);
        painter->drawArc(x - radius / 2, y - radius / 2, radius, radius, 0 * 16, -180 * 16);
      }

      if(ap.isHeliport() && size > 6)
      {
        int lineWidth = size / 7;
        painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(x - lineWidth / 4 - size / 5, y - radius / 2,
                          x - lineWidth / 4 - size / 5, y + radius / 2);

        painter->drawLine(x - lineWidth / 4 - size / 5, y,
                          x - lineWidth / 4 + size / 5, y);

        painter->drawLine(x - lineWidth / 4 + size / 5, y - radius / 2,
                          x - lineWidth / 4 + size / 5, y + radius / 2);
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

void MapPainterAirport::runwayCoords(const QList<MapRunway> *rw, QList<QPoint> *centers, QList<QRect> *rects,
                                     QList<QRect> *innerRects, QList<QRect> *backRects)
{
  for(const MapRunway& r : *rw)
  {
    // Get the two endpoints as screen coords
    float xr1, yr1, xr2, yr2;
    wToS(r.primary, xr1, yr1);
    wToS(r.secondary, xr2, yr2);

    // Get the center point as screen coords
    float xc, yc;
    wToS(r.center, xc, yc);
    if(centers != nullptr)
      centers->append(QPoint(static_cast<int>(std::round(xc)), static_cast<int>(std::round(yc))));

    // Get an approximation of the runway length on the screen
    int length = atools::geo::simpleDistance(xr1, yr1, xr2, yr2);

    int width = 6;
    if(r.width > 0)
      // Get an approximation of the runway width on the screen
      width = scale->getPixelIntForFeet(r.width, r.heading + 90.f);

    int backgroundSize = scale->getPixelIntForMeter(200.f);

    if(backRects != nullptr)
      backRects->append(QRect(-width - backgroundSize, -length / 2 - backgroundSize,
                              width + backgroundSize * 2, length + backgroundSize * 2));

    if(rects != nullptr)
      rects->append(QRect(-(width / 2), -length / 2, width, length));

    if(innerRects != nullptr)
      innerRects->append(QRect(-(width / 6), -length / 2 + 2, width - 4, length - 4));
  }
}

void MapPainterAirport::textBox(GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
                                const QPen& pen, int x, int y, bool transparent)
{
  painter->save();

  if(transparent)
    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255, 180)));
  else
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

QColor& MapPainterAirport::colorForAirport(const MapAirport& ap)
{
  if(!ap.isSet(SCENERY) && !ap.waterOnly())
    return airportEmptyColor;
  else if(ap.isSet(TOWER))
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

QColor MapPainterAirport::colorForSurface(const QString& surface)
{
  if(surface == "CONCRETE")
    return QColor(Qt::lightGray);
  else if(surface == "GRASS")
    return QColor(Qt::darkGreen);
  else if(surface == "WATER")
    return QColor::fromRgb(133, 133, 255);
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

QString MapPainterAirport::parkingName(const QString& name)
{
  if(name == "PARKING")
    return "P";
  else if(name == "N_PARKING")
    return "N";
  else if(name == "NE_PARKING")
    return "NE";
  else if(name == "E_PARKING")
    return "E";
  else if(name == "SE_PARKING")
    return "SE";
  else if(name == "S_PARKING")
    return "S";
  else if(name == "SW_PARKING")
    return "SW";
  else if(name == "W_PARKING")
    return "W";
  else if(name == "NW_PARKING")
    return "NW";
  else if(name == "GATE")
    return QString();
  else if(name == "DOCK")
    return "D";
  else if(name.startsWith("GATE_"))
    return name.right(1);
  else
    return QString();
}

QColor MapPainterAirport::colorForParkingType(const QString& type)
{
  if(type.startsWith("RAMP_MIL"))
    return QColor(Qt::red);
  else if(type.startsWith("GATE"))
    return QColor::fromRgb(100, 100, 255);
  else if(type.startsWith("RAMP_GA") || type.startsWith("DOCK_GA"))
    return QColor(Qt::green);
  else if(type.startsWith("RAMP_CARGO"))
    return QColor(Qt::darkGreen);
  else if(type.startsWith("FUEL"))
    return QColor(Qt::yellow);
  else
    return QColor();
}

QStringList MapPainterAirport::airportTexts(const MapLayer *mapLayer, const MapAirport& airport)
{
  QStringList texts;

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
  return texts;
}
