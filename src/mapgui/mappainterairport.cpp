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
#include "common/symbolpainter.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"
#include "mapgui/mapwidget.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

const int RUNWAY_HEADING_FONT_SIZE = 12;
const int RUNWAY_TEXT_FONT_SIZE = 16;
const int RUNWAY_NUMBER_FONT_SIZE = 20;

MapPainterAirport::MapPainterAirport(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                                     bool verboseMsg)
  : MapPainter(mapWidget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterAirport::~MapPainterAirport()
{
}

void MapPainterAirport::render(const PaintContext *context)
{
  if((!context->objectTypes.testFlag(maptypes::AIRPORT) || !context->mapLayer->isAirport()) &&
     (!context->mapLayerEffective->isAirportDiagram() || context->forcePaintObjects == nullptr))
    return;

  bool drawFast = mapWidget->viewContext() == Marble::Animation;

  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  const QList<MapAirport> *airports = nullptr;
  if(context->mapLayerEffective->isAirportDiagram())
    airports = query->getAirports(curBox, context->mapLayerEffective, drawFast);
  else
    airports = query->getAirports(curBox, context->mapLayer, drawFast);
  if(airports == nullptr)
    return;

  if(!drawFast && verbose)
  {
    qDebug() << "Number of aiports" << airports->size();
    qDebug() << "Time for query" << t.elapsed() << " ms";
    qDebug() << curBox.toString();
    qDebug() << *context->mapLayer;
    t.restart();
  }

  setRenderHints(context->painter);

  for(const MapAirport& airport : *airports)
  {
    const MapLayer *layer = context->mapLayer;
    bool forcedPaint = context->forcePaintObjects != nullptr &&
                       context->forcePaintObjects->contains({airport.id, maptypes::AIRPORT});

    if(!airport.isVisible(context->objectTypes) && !forcedPaint)
      continue;

    int x, y;
    bool visible = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding));

    if(!visible)
    {
      GeoDataLatLonBox apbox(airport.bounding.getNorth(), airport.bounding.getSouth(),
                             airport.bounding.getEast(), airport.bounding.getWest(),
                             DEG);
      visible = curBox.intersects(apbox);
    }

    if(visible)
    {
      if(context->mapLayerEffective->isAirportDiagram())
        drawAirportDiagram(context, airport, drawFast);
      else
        drawAirportSymbolOverview(context, airport, drawFast);

      drawAirportSymbol(context, airport, x, y, drawFast);

      textflags::TextFlags flags;

      if(layer->isAirportInfo())
        flags = textflags::IDENT | textflags::NAME | textflags::INFO;

      if(layer->isAirportIdent())
        flags |= textflags::IDENT;
      else if(layer->isAirportName())
        flags |= textflags::NAME;

      symbolPainter->drawAirportText(context->painter, airport, x, y, flags,
                                     context->mapLayerEffective->getAirportSymbolSize(),
                                     context->mapLayerEffective->isAirportDiagram(), true, drawFast);
    }
  }

  if(mapWidget->viewContext() == Marble::Still && verbose)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}

void MapPainterAirport::drawAirportDiagram(const PaintContext *context, const maptypes::MapAirport& airport,
                                           bool fast)
{
  Marble::GeoPainter *painter = context->painter;
  painter->save();
  painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setBrush(mapcolors::airportDetailBackColor);
  painter->setPen(QPen(mapcolors::airportDetailBackColor, scale->getPixelIntForMeter(200.f),
                       Qt::SolidLine, Qt::RoundCap));

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
    QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
    QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
    painter->drawLine(start, end);
  }

  const QList<MapApron> *aprons = query->getAprons(airport.id);
  QVector<QPoint> points;
  for(const MapApron& apron : *aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, DEFAULT_WTOS_SIZE, &visible));

    painter->QPainter::drawPolyline(points.data(), points.size());
  }

  // Draw aprons ---------------------------------
  painter->setBackground(Qt::transparent);
  for(const MapApron& apron : *aprons)
  {
    points.clear();
    bool visible;
    for(const Pos& pos : apron.vertices)
      points.append(wToS(pos, DEFAULT_WTOS_SIZE, &visible));

    QColor col = mapcolors::colorForSurface(apron.surface);
    col = col.darker(110);

    painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));

    if(!apron.drawSurface)
      painter->setBrush(QBrush(col, Qt::Dense6Pattern));
    else
      painter->setBrush(QBrush(col));

    painter->QPainter::drawPolygon(points.data(), points.size());
  }

  // Draw taxiways ---------------------------------
  for(const MapTaxiPath& taxipath : *taxipaths)
  {
    int pathThickness = scale->getPixelIntForFeet(taxipath.width);
    QColor col = mapcolors::colorForSurface(taxipath.surface);

    if(!taxipath.drawSurface)
      painter->setPen(QPen(QBrush(col, Qt::Dense4Pattern), pathThickness, Qt::SolidLine, Qt::RoundCap));
    else
      painter->setPen(QPen(col, pathThickness, Qt::SolidLine, Qt::RoundCap));

    bool visible;
    QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
    QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
    painter->drawLine(start, end);
  }

  // Draw taxiway names ---------------------------------
  if(!fast && context->mapLayerEffective->isAirportDiagramDetail())
  {
    QFontMetrics taxiMetrics = painter->fontMetrics();
    painter->setBackgroundMode(Qt::TransparentMode);
    painter->setPen(QPen(mapcolors::taxiwayNameColor, 2, Qt::SolidLine, Qt::FlatCap));

    // Map all vivible names to paths
    QMultiMap<QString, MapTaxiPath> map;
    for(const MapTaxiPath& taxipath : *taxipaths)
      if(!taxipath.name.isEmpty())
      {
        bool visible;
        wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
        wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
        if(visible)
          map.insert(taxipath.name, taxipath);
      }

    for(QString taxiname : map.keys())
    {
      QList<MapTaxiPath> paths = map.values(taxiname);
      QList<MapTaxiPath> paths2;

      // Simplified text placement - take first, last and middle name for a path
      paths2.append(paths.first());
      if(paths.size() > 2)
        paths2.append(paths.at(paths.size() / 2));
      paths2.append(paths.last());

      for(const MapTaxiPath& taxipath : paths2)
      {
        bool visible;
        QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
        QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);

        QRect textrect = taxiMetrics.boundingRect(taxiname);

        int length = atools::geo::simpleDistance(start.x(), start.y(), end.x(), end.y());
        if(length > 40)
        {
          int x = ((start.x() + end.x()) / 2) - textrect.width() / 2;
          int y = ((start.y() + end.y()) / 2) + textrect.height() / 2 - taxiMetrics.descent();
          textrect.moveTo(x, y - textrect.height() + taxiMetrics.descent());
          textrect.adjust(-1, -1, 1, 1);
          painter->fillRect(textrect, mapcolors::taxiwayNameBackgroundColor);
          painter->drawText(x, y, taxiname);
        }
      }
    }
    painter->setBackgroundMode(Qt::OpaqueMode);
  }

  if(!fast)
  {
    // Draw runway overrun and blast pads --------------------------------
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 1, Qt::SolidLine, Qt::FlatCap));
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      const MapRunway& runway = runways->at(i);
      const QRect& rect = runwayRects.at(i);
      QColor col = mapcolors::colorForSurface(runway.surface);

      painter->translate(runwayCenters.at(i));
      painter->rotate(runways->at(i).heading);

      if(runway.primOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.primOverrun, runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.bottom(), rect.width(), offs);
      }
      if(runway.secOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.secOverrun, runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.top() - offs, rect.width(), offs);
      }
      if(runway.primBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.primBlastPad, runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.bottom(), rect.width(), offs);
      }
      if(runway.secBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.secBlastPad, runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.top() - offs, rect.width(), offs);
      }

      painter->resetTransform();
    }
  }

  if(!fast)
  {
    // Draw runway outlines --------------------------------
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 1, Qt::SolidLine, Qt::FlatCap));
    painter->setBrush(mapcolors::runwayOutlineColor);
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      painter->translate(runwayCenters.at(i));
      painter->rotate(runways->at(i).heading);
      painter->drawRect(runwayRects.at(i).marginsAdded(QMargins(2, 2, 2, 2)));
      painter->resetTransform();
    }
  }

  // Draw runways --------------------------------
  for(int i = 0; i < runwayCenters.size(); i++)
  {
    const MapRunway& runway = runways->at(i);
    const QRect& rect = runwayRects.at(i);

    QColor col = mapcolors::colorForSurface(runway.surface);

    painter->translate(runwayCenters.at(i));
    painter->rotate(runway.heading);

    painter->setBrush(col);
    painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));
    painter->drawRect(rect);
    painter->resetTransform();
  }

  if(!fast)
  {
    // Draw runways offsets --------------------------------
    painter->setBackgroundMode(Qt::TransparentMode);
    painter->setBrush(mapcolors::runwayOffsetColor);
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
          painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::SolidLine, Qt::FlatCap));
          painter->drawLine(rect.left(), rect.bottom() - offs, rect.right(), rect.bottom() - offs);

          painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::DashLine, Qt::FlatCap));
          painter->drawLine(0, rect.bottom(), 0, rect.bottom() - offs);
        }

        if(runway.secOffset > 0)
        {
          int offs = scale->getPixelIntForFeet(runway.secOffset, runway.heading);
          painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::SolidLine, Qt::FlatCap));
          painter->drawLine(rect.left(), rect.top() + offs, rect.right(), rect.top() + offs);

          painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::DashLine, Qt::FlatCap));
          painter->drawLine(0, rect.top(), 0, rect.top() + offs);
        }
        painter->resetTransform();
      }
    }
    painter->setBackgroundMode(Qt::OpaqueMode);
  }

  // Draw parking --------------------------------
  const QList<MapParking> *parkings = query->getParkingsForAirport(airport.id);
  if(!parkings->isEmpty())
    painter->setPen(QPen(mapcolors::parkingOutlineColor, 2, Qt::SolidLine, Qt::FlatCap));
  for(const MapParking& parking : *parkings)
  {
    bool visible;
    QPoint pt = wToS(parking.position, DEFAULT_WTOS_SIZE, &visible);
    if(visible)
    {
      int w = scale->getPixelIntForFeet(parking.radius, 90);
      int h = scale->getPixelIntForFeet(parking.radius, 0);

      painter->setBrush(mapcolors::colorForParkingType(parking.type));
      painter->drawEllipse(pt, w, h);

      if(!fast)
      {
        if(parking.jetway)
          painter->drawEllipse(pt, w * 3 / 4, h * 3 / 4);

        painter->translate(pt);
        painter->rotate(parking.heading);
        painter->drawLine(0, h * 2 / 3, 0, h);
        painter->resetTransform();
      }
    }
  }

  // Draw helipads ------------------------------------------------
  const QList<MapHelipad> *helipads = query->getHelipads(airport.id);
  if(!helipads->isEmpty())
    painter->setPen(QPen(mapcolors::helipadOutlineColor, 2, Qt::SolidLine, Qt::FlatCap));
  for(const MapHelipad& helipad : *helipads)
  {
    bool visible;
    QPoint pt = wToS(helipad.position, DEFAULT_WTOS_SIZE, &visible);
    if(visible)
    {
      painter->setBrush(mapcolors::colorForSurface(helipad.surface));

      int w = scale->getPixelIntForFeet(helipad.width, 90) / 2;
      int h = scale->getPixelIntForFeet(helipad.length, 0) / 2;

      painter->drawEllipse(pt, w, h);

      if(!fast)
      {
        painter->translate(pt);
        painter->rotate(helipad.heading);
        painter->drawLine(-w / 3, -h / 2, -w / 3, h / 2);
        painter->drawLine(-w / 3, 0, w / 3, 0);
        painter->drawLine(w / 3, -h / 2, w / 3, h / 2);

        if(helipad.closed)
        {
          // Cross out runway number
          painter->drawLine(-w, -w, w, w);
          painter->drawLine(-w, w, w, -w);
        }
      }

      painter->resetTransform();
    }
  }

  // Draw tower -------------------------------------------------
  if(airport.towerCoords.isValid())
  {
    bool visible;
    QPoint pt = wToS(airport.towerCoords, DEFAULT_WTOS_SIZE, &visible);
    if(visible)
    {
      if(airport.towerFrequency > 0)
      {
        painter->setPen(QPen(mapcolors::activeTowerOutlineColor, 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(mapcolors::activeTowerColor);
      }
      else
      {
        painter->setPen(QPen(mapcolors::inactiveTowerOutlineColor, 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(mapcolors::inactiveTowerColor);
      }

      int w = scale->getPixelIntForMeter(10, 90);
      int h = scale->getPixelIntForMeter(10, 0);
      painter->drawEllipse(pt, w < 5 ? 5 : w, h < 5 ? 5 : h);
    }
  }

  painter->setBackgroundMode(Qt::TransparentMode);

  // Draw parking and tower texts -------------------------------------------------
  QFontMetrics metrics = painter->fontMetrics();
  if(!fast && context->mapLayerEffective->isAirportDiagramDetail())
  {
    for(const MapParking& parking : *parkings)
      if(context->mapLayerEffective->isAirportDiagramDetail2() || parking.radius > 40)
      {
        bool visible;
        QPoint pt = wToS(parking.position, DEFAULT_WTOS_SIZE, &visible);
        if(visible)
        {
          if(parking.type.startsWith("RAMP_GA") || parking.type.startsWith("DOCK_GA") ||
             parking.type.startsWith("FUEL"))
            painter->setPen(QPen(mapcolors::darkParkingTextColor, 2, Qt::SolidLine, Qt::FlatCap));
          else
            painter->setPen(QPen(mapcolors::brightParkingTextColor, 2, Qt::SolidLine, Qt::FlatCap));

          QString text;
          if(parking.type.startsWith("FUEL"))
            text = "F";
          else
            text = QLocale().toString(parking.number) + " " + parkingTypeName(parking.name);
          pt.setY(pt.y() + metrics.ascent() / 2);
          pt.setX(pt.x() - metrics.width(text) / 2);

          painter->drawText(pt, text);
        }
      }
  }

  if(!fast && airport.towerCoords.isValid())
  {
    bool visible;
    QPoint pt = wToS(airport.towerCoords, DEFAULT_WTOS_SIZE, &visible);
    if(visible)
    {
      pt.setY(pt.y() + metrics.ascent() / 2);
      pt.setX(pt.x() - metrics.width("T") / 2);
      painter->setPen(QPen(mapcolors::towerTextColor, 2, Qt::SolidLine, Qt::FlatCap));
      painter->drawText(pt, "T");
    }
  }

  // Draw runway texts --------------------------------
  if(!fast)
  {
    QFont rwTextFont = painter->font();
    rwTextFont.setPixelSize(RUNWAY_TEXT_FONT_SIZE);
    painter->setFont(rwTextFont);
    QFontMetrics rwMetrics = painter->fontMetrics();

    QVector<int> runwayTextLengths;
    // Draw dimensions at runway side
    painter->setPen(QPen(mapcolors::runwayDimsTextColor, 3, Qt::SolidLine, Qt::FlatCap));
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      const MapRunway& runway = runways->at(i);
      const QRect& runwayRect = runwayRects.at(i);

      QString text = QLocale().toString(runway.length);

      if(runway.width > 8)
        text += " x " + QLocale().toString(runway.width);

      if(!runway.edgeLight.isEmpty())
        text += " / L";
      text += " / " + maptypes::surfaceName(runway.surface);

      QRect textRect = rwMetrics.boundingRect(text);
      int textWidth = textRect.width();
      if(textWidth > runwayRect.height())
        textWidth = runwayRect.height();

      int textx = -textWidth / 2, texty = -runwayRect.width() / 2;
      textRect.moveTo(textx, texty - textRect.height() - 3);

      runwayTextLengths.append(textWidth);
      text = rwMetrics.elidedText(text, Qt::ElideRight, runwayRect.height());

      painter->translate(runwayCenters.at(i));
      if(runway.heading > 180.f)
        painter->rotate(runway.heading + 90.f);
      else
        painter->rotate(runway.heading - 90.f);
      painter->fillRect(textRect, mapcolors::runwayTextBackgroundColor);
      painter->drawText(textx, texty - rwMetrics.descent() - 3, text);
      painter->resetTransform();
    }

    // Draw runway heading at the side
    QFont rwHdgTextFont = painter->font();
    rwHdgTextFont.setPixelSize(RUNWAY_HEADING_FONT_SIZE);
    painter->setFont(rwHdgTextFont);
    QFontMetrics rwHdgMetrics = painter->fontMetrics();

    for(int i = 0; i < runwayCenters.size(); i++)
    {
      const MapRunway& runway = runways->at(i);
      const QRect& runwayRect = runwayRects.at(i);
      float magHeading = normalizeCourse(runway.heading + airport.magvar);

      QString textPrim;
      QString textSec;

      float rotate;
      if(runway.heading > 180.f)
      {
        // This case is rare (eg. LTAI)
        rotate = runway.heading + 90.f;
        textPrim = QString("► " +
                           QString::number(normalizeCourse(opposedCourseDeg(magHeading)), 'f', 0) + " °M");
        textSec = QString(QString::number(magHeading, 'f', 0) + " °M ◄");
      }
      else
      {
        rotate = runway.heading - 90.f;
        textPrim = QString("► " + QString::number(magHeading, 'f', 0) + " °M");
        textSec = QString(QString::number(
                            normalizeCourse(opposedCourseDeg(magHeading)), 'f', 0) + " °M ◄");
      }

      QRect textRectPrim = rwHdgMetrics.boundingRect(textPrim);
      QRect textRectSec = rwHdgMetrics.boundingRect(textSec);

      if(textRectPrim.width() + textRectSec.width() + runwayTextLengths.at(i) < runwayRect.height())
      {
        painter->translate(runwayCenters.at(i));
        painter->rotate(rotate);

        textRectPrim.moveTo(-runwayRect.height() / 2, -runwayRect.width() / 2 - textRectPrim.height() - 3);
        painter->fillRect(textRectPrim, mapcolors::runwayTextBackgroundColor);
        painter->drawText(-runwayRect.height() / 2,
                          -runwayRect.width() / 2 - rwHdgMetrics.descent() - 3, textPrim);

        textRectSec.moveTo(runwayRect.height() / 2 - textRectSec.width(),
                           -runwayRect.width() / 2 - textRectSec.height() - 3);
        painter->fillRect(textRectSec, mapcolors::runwayTextBackgroundColor);
        painter->drawText(runwayRect.height() / 2 - textRectSec.width(),
                          -runwayRect.width() / 2 - rwHdgMetrics.descent() - 3, textSec);
        painter->resetTransform();
      }
    }

    // Draw numbers at end
    rwTextFont.setPixelSize(RUNWAY_NUMBER_FONT_SIZE);
    painter->setFont(rwTextFont);
    QFontMetrics rwTextMetrics = painter->fontMetrics();
    const int CROSS_SIZE = 10;
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      const MapRunway& runway = runways->at(i);
      bool primaryVisible, secondaryVisible;
      QPoint prim = wToS(runway.primary, DEFAULT_WTOS_SIZE, &primaryVisible);
      QPoint sec = wToS(runway.secondary, DEFAULT_WTOS_SIZE, &secondaryVisible);

      // TODO why is primary and secondary reversed
      if(primaryVisible)
      {
        painter->translate(prim);
        painter->rotate(runway.heading + 180.f);

        QRect rectSec = rwTextMetrics.boundingRect(runway.secName);
        rectSec.moveTo(-rectSec.width() / 2, 4);

        painter->fillRect(rectSec, mapcolors::runwayTextBackgroundColor);
        painter->drawText(-rectSec.width() / 2, rwTextMetrics.ascent() + 4, runway.secName);

        if(runway.secClosed)
        {
          // Cross out runway number
          painter->drawLine(-CROSS_SIZE + 4, -CROSS_SIZE + 10 + 4, CROSS_SIZE + 4, CROSS_SIZE + 10 + 4);
          painter->drawLine(-CROSS_SIZE + 4, CROSS_SIZE + 10 + 4, CROSS_SIZE + 4, -CROSS_SIZE + 10 + 4);
        }
        painter->resetTransform();
      }

      if(secondaryVisible)
      {
        painter->translate(sec);
        painter->rotate(runway.heading);

        QRect rectPrim = rwTextMetrics.boundingRect(runway.primName);
        rectPrim.moveTo(-rectPrim.width() / 2, 4);

        painter->fillRect(rectPrim, mapcolors::runwayTextBackgroundColor);
        painter->drawText(-rectPrim.width() / 2, rwTextMetrics.ascent() + 4, runway.primName);

        if(runway.primClosed)
        {
          // Cross out runway number
          painter->drawLine(-CROSS_SIZE + 4, -CROSS_SIZE + 10 + 4, CROSS_SIZE + 4, CROSS_SIZE + 10 + 4);
          painter->drawLine(-CROSS_SIZE + 4, CROSS_SIZE + 10 + 4, CROSS_SIZE + 4, -CROSS_SIZE + 10 + 4);
        }
        painter->resetTransform();
      }
    }
  }

  painter->restore();
}

void MapPainterAirport::drawAirportSymbolOverview(const PaintContext *context, const maptypes::MapAirport& ap,
                                                  bool fast)
{
  Marble::GeoPainter *painter = context->painter;

  if(ap.longestRunwayLength >= 8000 && context->mapLayerEffective->isAirportOverviewRunway() &&
     !ap.flags.testFlag(maptypes::AP_CLOSED) && !ap.waterOnly())
  {
    painter->save();

    QColor apColor = mapcolors::colorForAirport(ap);
    painter->setBackgroundMode(Qt::OpaqueMode);

    const QList<maptypes::MapRunway> *rw = query->getRunwaysForOverview(ap.id);

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

    if(!fast || context->mapLayerEffective->isAirportDiagram())
    {
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), 1, Qt::SolidLine, Qt::FlatCap));
      painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));
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

void MapPainterAirport::drawAirportSymbol(const PaintContext *context, const maptypes::MapAirport& ap,
                                          int x, int y, bool fast)
{
  if(!context->mapLayerEffective->isAirportOverviewRunway() || ap.flags.testFlag(maptypes::AP_CLOSED) ||
     ap.waterOnly() || ap.longestRunwayLength < 8000 || context->mapLayerEffective->isAirportDiagram())
  {
    int size = context->mapLayerEffective->getAirportSymbolSize();
    bool isAirportDiagram = context->mapLayerEffective->isAirportDiagram();

    symbolPainter->drawAirportSymbol(context->painter, ap, x, y, size, isAirportDiagram, fast);
  }
}

void MapPainterAirport::runwayCoords(const QList<maptypes::MapRunway> *rw, QList<QPoint> *centers,
                                     QList<QRect> *rects, QList<QRect> *innerRects, QList<QRect> *backRects)
{
  for(const maptypes::MapRunway& r : *rw)
  {
    Rect bounding(r.primary);
    bounding.extend(r.secondary);
    QSize size = scale->getScreeenSizeForRect(bounding);

    // Get the two endpoints as screen coords
    float xr1, yr1, xr2, yr2;
    wToS(r.primary, xr1, yr1, size);
    wToS(r.secondary, xr2, yr2, size);

    // Get the center point as screen coords
    float xc, yc;
    wToS(r.position, xc, yc, size);
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
