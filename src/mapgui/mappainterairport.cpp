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

  QStringList texts;

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
        airportDiagram(mapLayer, painter, airport, x, y);
      else
        airportSymbolOverview(painter, airport, mapLayer,
                              widget->viewContext() == Marble::Animation);

      airportSymbol(painter, airport, x, y, mapLayer, widget->viewContext() == Marble::Animation);
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
      if((std::abs(x - xs) + std::abs(y - ys)) < 10)
        return airports.at(i);
  }

  return MapAirport();
}

void MapPainterAirport::airportDiagram(const MapLayer *mapLayer, GeoPainter *painter, const MapAirport& ap,
                                       int x, int y)
{
  Q_UNUSED(x);
  Q_UNUSED(y);
  painter->save();
  painter->setBackgroundMode(Qt::OpaqueMode);

  QList<MapRunway> rw;
  query->getRunways(ap.id, rw);

  QList<MapApron> aprons;
  query->getAprons(ap.id, aprons);

  QList<QPoint> centers;
  QList<QRect> rects, backRects;
  runwayCoords(rw, &centers, &rects, nullptr, &backRects);

  QList<MapTaxiPath> taxipaths;
  query->getTaxiPaths(ap.id, taxipaths);

  QList<MapParking> parkings;
  query->getParking(ap.id, parkings);

  int back = scale->getPixelIntForMeter(200.f);
  painter->setBrush(QColor::fromRgb(250, 250, 250));
  painter->setPen(QPen(QColor::fromRgb(250, 250, 250), back, Qt::SolidLine, Qt::RoundCap));

  for(int i = 0; i < centers.size(); i++)
    if(rw.at(i).surface != "WATER")
    {
      painter->translate(centers.at(i));
      painter->rotate(rw.at(i).heading);

      const QRect backRect = backRects.at(i);
      painter->drawRect(backRect);

      painter->resetTransform();
    }

  for(const MapTaxiPath& tp : taxipaths)
  {
    bool visible;

    QPoint start = wToS(tp.start, &visible);
    QPoint end = wToS(tp.end, &visible);
    painter->drawLine(start, end);
  }

  QVector<QPoint> points;
  for(const MapApron& apron : aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, &visible));

    painter->QPainter::drawPolyline(points.data(), points.size());
  }

  for(const MapApron& apron : aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, &visible));

    painter->setBrush(QBrush(colorForSurface(apron.surface), Qt::Dense2Pattern));
    painter->setPen(QPen(colorForSurface(apron.surface), 1, Qt::SolidLine, Qt::FlatCap));
    painter->QPainter::drawPolygon(points.data(), points.size());
  }

  for(const MapTaxiPath& tp : taxipaths)
  {
    bool visible;
    painter->setBrush(colorForSurface(tp.surface));

    int thick = scale->getPixelIntForFeet(tp.width);
    painter->setPen(QPen(QColor(colorForSurface(tp.surface)), thick, Qt::SolidLine, Qt::RoundCap));

    QPoint start = wToS(tp.start, &visible);
    QPoint end = wToS(tp.end, &visible);
    painter->drawLine(start, end);
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

  painter->setPen(QPen(QColor::fromRgb(50, 50, 50), 2, Qt::SolidLine, Qt::FlatCap));
  for(const MapParking& p : parkings)
  {

    bool visible;
    QPoint pt = wToS(p.pos, &visible);
    if(visible)
    {
      int w = scale->getPixelIntForFeet(p.radius, 90);
      int h = scale->getPixelIntForFeet(p.radius, 0);

      if(p.type.startsWith("RAMP_MIL"))
        painter->setBrush(QColor(Qt::red));
      else if(p.type.startsWith("GATE"))
        painter->setBrush(QColor::fromRgb(100, 100, 255));
      else if(p.type.startsWith("RAMP_GA") || p.type.startsWith("DOCK_GA"))
        painter->setBrush(QColor(Qt::green));
      else if(p.type.startsWith("RAMP_CARGO"))
        painter->setBrush(QColor(Qt::darkGreen));
      else if(p.type.startsWith("FUEL"))
        painter->setBrush(QColor(Qt::yellow));

      painter->drawEllipse(pt, w, h);

      if(p.jetway && !p.type.startsWith("FUEL"))
        painter->drawEllipse(pt, w * 3 / 4, h * 3 / 4);

      painter->translate(pt);
      painter->rotate(p.heading);
      painter->drawLine(0, 0, 0, h);
      painter->resetTransform();

    }
  }

  if(ap.towerCoords.isValid())
  {
    bool visible;
    QPoint pt = wToS(ap.towerCoords, &visible);
    if(visible)
    {
      painter->setPen(QPen(QColor(Qt::black), 3, Qt::SolidLine, Qt::FlatCap));
      painter->setBrush(QColor(Qt::yellow));

      int w = scale->getPixelIntForMeter(30, 90);
      int h = scale->getPixelIntForMeter(30, 0);
      painter->drawEllipse(pt, w, h);
    }
  }

  if(mapLayer->isAirportDiagramDetail())
  {
    painter->setBackgroundMode(Qt::TransparentMode);

    QFont f = painter->font();
    f.setBold(true);
    painter->setFont(f);
    QFontMetrics metrics = painter->fontMetrics();

    for(const MapParking& p : parkings)
    {
      bool visible;
      QPoint pt = wToS(p.pos, &visible);
      if(visible)
      {
        if(p.type.startsWith("RAMP_GA") || p.type.startsWith("DOCK_GA") || p.type.startsWith("FUEL"))
          painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
        else
          painter->setPen(QPen(QColor(Qt::white), 2, Qt::SolidLine, Qt::FlatCap));

        if(p.type.startsWith("FUEL"))
          painter->drawText(pt, "F");
        else
          painter->drawText(pt, QString::number(p.number) + " " + parkingName(p.name));
      }
    }

    if(ap.towerCoords.isValid())
    {
      bool visible;
      QPoint pt = wToS(ap.towerCoords, &visible);
      if(visible)
      {
        painter->setPen(QPen(QColor(Qt::black), 2, Qt::SolidLine, Qt::FlatCap));
        painter->drawText(pt, "T");
      }
    }
  }

  painter->restore();
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

void MapPainterAirport::airportSymbolOverview(GeoPainter *painter, const MapAirport& ap,
                                              const MapLayer *mapLayer, bool fast)
{
  if(ap.longestRunwayLength >= 8000 && mapLayer->isAirportOverviewRunway() && !ap.isSet(CLOSED) &&
     !ap.waterOnly())
  {
    painter->save();

    QColor apColor = colorForAirport(ap);
    painter->setBackgroundMode(Qt::OpaqueMode);

    QList<MapRunway> rw;
    query->getRunwaysForOverview(ap.id, rw);

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

void MapPainterAirport::runwayCoords(const QList<MapRunway>& rw, QList<QPoint> *centers, QList<QRect> *rects,
                                     QList<QRect> *innerRects, QList<QRect> *backRects)
{
  for(const MapRunway& r : rw)
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
    int length = static_cast<int>(std::round(sqrt((xr1 - xr2) * (xr1 - xr2) + (yr1 - yr2) * (yr1 - yr2))));

    int width = 6;
    if(r.width > 0)
      // Get an approximation of the runway width on the screen
      width = scale->getPixelIntForFeet(r.width, r.heading + 90.f);

    int back = scale->getPixelIntForMeter(200.f);

    if(backRects != nullptr)
      backRects->append(QRect(-width - back, -length / 2 - back, width + back * 2, length + back * 2));

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
    return QColor(Qt::darkBlue);
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
