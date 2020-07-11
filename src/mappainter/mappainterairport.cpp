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

#include "mappainter/mappainterairport.h"

#include "common/symbolpainter.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "geo/calculations.h"
#include "common/formatter.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "route/routecontroller.h"
#include "util/paintercontextsaver.h"
#include "mapgui/aprongeometrycache.h"
#include "atools.h"
#include "navapp.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

/* Minimum width for runway diagram */
static const int RUNWAY_MIN_WIDTH_FT = 4;
static const int RUNWAY_OVERVIEW_WIDTH_PIX = 6;

/* All sizes in pixel */
static const int RUNWAY_HEADING_FONT_SIZE = 12;
static const int RUNWAY_TEXT_FONT_SIZE = 16;
static const int RUNWAY_NUMBER_FONT_SIZE = 20;
static const int RUNWAY_NUMBER_SMALL_FONT_SIZE = 12;
static const int TAXIWAY_TEXT_MIN_LENGTH = 20;
static const int RUNWAY_OVERVIEW_MIN_LENGTH_FEET = 8000;
static const float AIRPORT_DIAGRAM_BACKGROUND_METER = 200.f;

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterAirport::MapPainterAirport(MapPaintWidget *mapWidget, MapScale *mapScale,
                                     const Route *routeParam)
  : MapPainter(mapWidget, mapScale), route(routeParam)
{
}

MapPainterAirport::~MapPainterAirport()
{
}

void MapPainterAirport::render(PaintContext *context)
{
  if((!context->objectTypes.testFlag(map::AIRPORT) || !context->mapLayer->isAirport()) &&
     (!context->mapLayerEffective->isAirportDiagramRunway()) && context->routeIdMap.isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver)

  // Get airports from cache/database for the bounding rectangle and add them to the map
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  const QList<MapAirport> *airportCache = nullptr;
  if(context->mapLayerEffective->isAirportDiagramRunway())
    airportCache = mapQuery->getAirports(curBox, context->mapLayerEffective, context->lazyUpdate);
  else
    airportCache = mapQuery->getAirports(curBox, context->mapLayer, context->lazyUpdate);

  // Collect all airports that are visible
  QList<PaintAirportType> visibleAirports;
  for(const MapAirport& airport : *airportCache)
  {
    // Avoid drawing too many airports during animation when zooming out
    if(airport.longestRunwayLength >= context->mapLayer->getMinRunwayLength())
    {
      float x, y;
      bool hidden;
      bool visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);

      if(!hidden)
      {
        if(!visibleOnMap && context->mapLayer->isAirportOverviewRunway())
          // Check bounding rect for visibility if relevant - not for point symbols
          visibleOnMap = airport.bounding.overlaps(context->viewportRect);

        airport.bounding.overlaps(context->viewportRect);

        // Either part of the route or enabled in the actions/menus/toolbar
        bool drawAirport = airport.isVisible(context->objectTypes) ||
                           context->routeIdMap.contains(airport.getRef());

        if(visibleOnMap)
        {
          if(drawAirport)
            visibleAirports.append({&airport, QPointF(x, y)});
        }
      }
    }
  }

  std::sort(visibleAirports.begin(), visibleAirports.end(), sortAirportFunction);

  if(context->mapLayerEffective->isAirportDiagramRunway() && context->flags2 & opts2::MAP_AIRPORT_BOUNDARY)
  {
    // In diagram mode draw background first to avoid overwriting other airports
    for(const PaintAirportType& airport : visibleAirports)
      drawAirportDiagramBackground(context, *airport.airport);
  }

  // Draw the diagrams first
  for(const PaintAirportType& airport : visibleAirports)
  {
    // Airport diagram is not influenced by detail level
    if(context->mapLayerEffective->isAirportDiagramRunway())
      drawAirportDiagram(context, *airport.airport);
  }

  textflags::TextFlags apTextFlags = context->airportTextFlags();

  // Add airport symbols on top of diagrams
  for(int i = 0; i < visibleAirports.size(); i++)
  {
    const MapAirport *airport = visibleAirports.at(i).airport;
    const QPointF& pt = visibleAirports.at(i).point;
    float x = static_cast<float>(pt.x());
    float y = static_cast<float>(pt.y());

    // Airport diagram is not influenced by detail level
    if(!context->mapLayerEffective->isAirportDiagramRunway())
      // Draw simplified runway lines
      drawAirportSymbolOverview(context, *airport, x, y);

    // More detailed symbol will be drawn by the route or log painter - so skip here
    if(!context->routeIdMap.contains(airport->getRef()))
    {
      // Symbol will be omitted for runway overview
      drawAirportSymbol(context, *airport, x, y);

      context->szFont(context->textSizeAirport);
      symbolPainter->drawAirportText(context->painter, *airport, x, y,
                                     context->dispOpts, apTextFlags,
                                     context->sz(context->symbolSizeAirport,
                                                 context->mapLayerEffective->getAirportSymbolSize()),
                                     context->mapLayerEffective->isAirportDiagramRunway(),
                                     context->mapLayer->getMaxTextLengthAirport());
    }
  }
}

/* Draws the full airport diagram including runway, taxiways, apron, parking and more */
void MapPainterAirport::drawAirportDiagramBackground(const PaintContext *context, const map::MapAirport& airport)
{
  Marble::GeoPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setFont(context->defaultFont);

  painter->setBrush(mapcolors::airportDetailBackColor);
  // Build a thick pen of around 200 meters to draw the airport background
  painter->setPen(QPen(mapcolors::airportDetailBackColor,
                       scale->getPixelIntForMeter(AIRPORT_DIAGRAM_BACKGROUND_METER),
                       Qt::SolidLine, Qt::RoundCap));

  if(context->flags2.testFlag(opts2::MAP_AIRPORT_RUNWAYS))
  {
    // Get all runways for this airport
    const QList<MapRunway> *runways = airportQuery->getRunways(airport.id);

    // Calculate all runway screen coordinates
    QList<QPoint> runwayCenters;
    QList<QRect> runwayRects, runwayOutlineRects;
    runwayCoords(runways, &runwayCenters, &runwayRects, nullptr, &runwayOutlineRects, false /* overview */);

    // Draw white background ---------------------------------
    // For runways
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      if(runways->at(i).surface != "W")
      {
        painter->translate(runwayCenters.at(i));
        painter->rotate(runways->at(i).heading);

        const QRect backRect = runwayOutlineRects.at(i);
        painter->drawRect(backRect);

        painter->resetTransform();
      }
    }
  }

  if(context->mapLayerEffective->isAirportDiagram() && context->flags2.testFlag(opts2::MAP_AIRPORT_DIAGRAM))
  {
    // For taxipaths
    const QList<MapTaxiPath> *taxipaths = airportQuery->getTaxiPaths(airport.id);
    for(const MapTaxiPath& taxipath : *taxipaths)
    {
      bool visible;
      QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
      QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
      painter->drawLine(start, end);
    }

    // For aprons
    const QList<MapApron> *aprons = airportQuery->getAprons(airport.id);
    for(const MapApron& apron : *aprons)
    {
      // FSX/P3D geometry
      if(!apron.vertices.isEmpty())
        drawFsApron(context, apron);
      if(!apron.geometry.boundary.isEmpty())
        drawXplaneApron(context, apron, true /* draw fast */);
    }
  }
}

/* Draw simple FSX/P3D aprons */
void MapPainterAirport::drawFsApron(const PaintContext *context, const map::MapApron& apron)
{
  QVector<QPoint> apronPoints;
  bool visible;
  for(const Pos& pos : apron.vertices)
    apronPoints.append(wToS(pos, DEFAULT_WTOS_SIZE, &visible));
  context->painter->QPainter::drawPolygon(apronPoints.data(), apronPoints.size());
}

void MapPainterAirport::drawXplaneApron(const PaintContext *context, const map::MapApron& apron, bool fast)
{
  // Create the apron boundary or get it from the cache for this zoom distance
  QPainterPath boundaryPath =
    mapPaintWidget->getApronGeometryCache()->getApronGeometry(apron, context->zoomDistanceMeter, fast);

  if(!boundaryPath.isEmpty())
    context->painter->drawPath(boundaryPath);
}

/* Draws the full airport diagram including runway, taxiways, apron, parking and more */
void MapPainterAirport::drawAirportDiagram(const PaintContext *context, const map::MapAirport& airport)
{
  Marble::GeoPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(painter);
  bool fast = context->drawFast;

  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setFont(context->defaultFont);

  QList<QPoint> runwayCenters;
  QList<QRect> runwayRects, runwayOutlineRects;

  const QList<MapRunway> *runways = nullptr;
  if(context->flags2.testFlag(opts2::MAP_AIRPORT_RUNWAYS))
  {
    // Get all runways for this airport
    runways = airportQuery->getRunways(airport.id);

    // Calculate all runway screen coordinates
    runwayCoords(runways, &runwayCenters, &runwayRects, nullptr, &runwayOutlineRects, false /* overview */);

    if(!fast && context->flags2 & opts2::MAP_AIRPORT_DIAGRAM && context->mapLayerEffective->isAirportDiagram())
    {
      // Draw runway shoulders (X-Plane) --------------------------------
      painter->setPen(Qt::NoPen);
      for(int i = 0; i < runwayCenters.size(); i++)
      {
        if(!runways->at(i).shoulder.isEmpty())
        {
          painter->translate(runwayCenters.at(i));
          painter->rotate(runways->at(i).heading);
          painter->setBrush(mapcolors::colorForSurface(runways->at(i).shoulder));
          float width = static_cast<float>(runwayRects.at(i).width()) / 4.f;
          painter->drawRect(QRectF(runwayRects.at(i)).marginsAdded(QMarginsF(width, 0.f, width, 0.f)));
          painter->resetTransform();
        }
      }
    }
  }

  if(context->flags2 & opts2::MAP_AIRPORT_DIAGRAM)
  {
    if(context->mapLayerEffective->isAirportDiagram())
    {
      // Draw aprons ---------------------------------
      painter->setBackground(Qt::transparent);
      const QList<MapApron> *aprons = airportQuery->getAprons(airport.id);

      for(const MapApron& apron : *aprons)
      {
        // Draw aprons a bit darker so we can see the taxiways
        QColor col = mapcolors::colorForSurface(apron.surface);
        col = col.darker(110);

        painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));

        if(!apron.drawSurface)
          // Use pattern for transparent aprons
          painter->setBrush(QBrush(col, Qt::Dense6Pattern));
        else
          painter->setBrush(QBrush(col));

        // FSX/P3D geometry
        if(!apron.vertices.isEmpty())
          drawFsApron(context, apron);

        // X-Plane geometry
        if(!apron.geometry.boundary.isEmpty())
          drawXplaneApron(context, apron, context->drawFast);
      }

      // Draw taxiways ---------------------------------
      painter->setBackgroundMode(Qt::OpaqueMode);
      QVector<QPoint> startPts, endPts;
      QVector<int> pathThickness;

      // Collect coordinates first
      const QList<MapTaxiPath> *taxipaths = airportQuery->getTaxiPaths(airport.id);
      for(const MapTaxiPath& taxipath : *taxipaths)
      {
        bool visible;
        // Do not do any clipping here
        startPts.append(wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible));
        endPts.append(wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible));

        if(taxipath.width == 0)
          // Special X-Plane case - width is not given for path
          pathThickness.append(0);
        else
          pathThickness.append(std::max(2, scale->getPixelIntForFeet(taxipath.width)));
      }

      // Draw closed and other taxi paths first to have real taxiways on top
      for(int i = 0; i < taxipaths->size(); i++)
      {
        const MapTaxiPath& taxipath = taxipaths->at(i);
        int thickness = pathThickness.at(i);

        if(thickness > 0)
        {
          QColor col = mapcolors::colorForSurface(taxipath.surface);

          const QPoint& start = startPts.at(i);
          const QPoint& end = endPts.at(i);

          if(taxipath.closed)
          {
            painter->setPen(QPen(col, thickness, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(start, end);

            painter->setPen(QPen(mapcolors::taxiwayClosedBrush, thickness, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(start, end);

          }
          else if(!taxipath.drawSurface)
          {
            painter->setPen(QPen(QBrush(col, Qt::Dense4Pattern), thickness, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(start, end);
          }
        }
      }

      // Draw real taxiways
      for(int i = 0; i < taxipaths->size(); i++)
      {
        const MapTaxiPath& taxipath = taxipaths->at(i);
        if(!taxipath.closed && taxipath.drawSurface && pathThickness.at(i) > 0)
        {
          painter->setPen(QPen(mapcolors::colorForSurface(taxipath.surface),
                               pathThickness.at(i), Qt::SolidLine, Qt::RoundCap));
          painter->drawLine(startPts.at(i), endPts.at(i));
        }
      }

      // Draw center lines - also for X-Plane on the pavement
      if(!fast && context->mapLayerEffective->isAirportDiagramDetail())
      {
        for(int i = 0; i < taxipaths->size(); i++)
        {
          painter->setPen(mapcolors::taxiwayLinePen);
          painter->drawLine(startPts.at(i), endPts.at(i));
        }
      }

      // Draw taxiway names ---------------------------------
      if(!fast && context->mapLayerEffective->isAirportDiagramDetail())
      {
        QFontMetrics taxiMetrics = painter->fontMetrics();
        painter->setBackgroundMode(Qt::TransparentMode);
        painter->setPen(QPen(mapcolors::taxiwayNameColor, 2, Qt::SolidLine, Qt::FlatCap));

        // Map all visible names to paths
        QMultiMap<QString, MapTaxiPath> map;
        for(const MapTaxiPath& taxipath : *taxipaths)
        {
          if(!taxipath.name.isEmpty())
          {
            bool visible;
            wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
            wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
            if(visible)
              map.insert(taxipath.name, taxipath);
          }
        }

        for(QString taxiname : map.keys())
        {
          QList<MapTaxiPath> paths = map.values(taxiname);
          QList<MapTaxiPath> pathsToLabel;

          // Simplified text placement - take first, last and middle name for a path
          if(!paths.isEmpty())
            pathsToLabel.append(paths.first());
          if(paths.size() > 2)
            pathsToLabel.append(paths.at(paths.size() / 2));
          pathsToLabel.append(paths.last());

          for(const MapTaxiPath& taxipath : pathsToLabel)
          {
            bool visible;
            QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
            QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);

            QRect textrect = taxiMetrics.boundingRect(taxiname);

            int length = atools::geo::simpleDistance(start.x(), start.y(), end.x(), end.y());
            if(length > TAXIWAY_TEXT_MIN_LENGTH)
            {
              // Only draw if segment is longer than 40 pixels
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
    }
  }

  if(!fast && runways != nullptr)
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

      // Draw overrun areas
      if(runway.primaryOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.primaryOverrun, runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.bottom(), rect.width(), offs);
      }
      if(runway.secondaryOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.secondaryOverrun, runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.top() - offs, rect.width(), offs);
      }

      // Draw blast pads
      if(runway.primaryBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.primaryBlastPad, runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.bottom(), rect.width(), offs);
      }
      if(runway.secondaryBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(runway.secondaryBlastPad, runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->setBackground(col);
        painter->drawRect(rect.left(), rect.top() - offs, rect.width(), offs);
      }

      painter->resetTransform();
    }
  }

  if(!fast && runways != nullptr)
  {
    // Draw black runway outlines --------------------------------
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 1, Qt::SolidLine, Qt::FlatCap));
    painter->setBrush(mapcolors::runwayOutlineColor);
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      if(runways->at(i).surface != "W")
      {
        painter->translate(runwayCenters.at(i));
        painter->rotate(runways->at(i).heading);
        painter->drawRect(runwayRects.at(i).marginsAdded(QMargins(2, 2, 2, 2)));
        painter->resetTransform();
      }
    }
  }

  // Draw runways --------------------------------
  if(runways != nullptr)
  {
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
      // Draw runway offset thresholds --------------------------------
      painter->setBackgroundMode(Qt::TransparentMode);
      painter->setBrush(mapcolors::runwayOffsetColor);
      for(int i = 0; i < runwayCenters.size(); i++)
      {
        const MapRunway& runway = runways->at(i);

        if(runway.primaryOffset > 0 || runway.secondaryOffset > 0)
        {
          const QRect& rect = runwayRects.at(i);

          painter->translate(runwayCenters.at(i));
          painter->rotate(runway.heading);

          if(runway.primaryOffset > 0)
          {
            int offs = scale->getPixelIntForFeet(runway.primaryOffset, runway.heading);

            // Draw solid boundary to runway
            painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(rect.left(), rect.bottom() - offs, rect.right(), rect.bottom() - offs);

            // Draw dashed line
            painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::DashLine, Qt::FlatCap));
            painter->drawLine(0, rect.bottom(), 0, rect.bottom() - offs);
          }

          if(runway.secondaryOffset > 0)
          {
            int offs = scale->getPixelIntForFeet(runway.secondaryOffset, runway.heading);

            // Draw solid boundary to runway
            painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(rect.left(), rect.top() + offs, rect.right(), rect.top() + offs);

            // Draw dashed line
            painter->setPen(QPen(mapcolors::runwayOffsetColor, 3, Qt::DashLine, Qt::FlatCap));
            painter->drawLine(0, rect.top(), 0, rect.top() + offs);
          }
          painter->resetTransform();
        }
      }
      painter->setBackgroundMode(Qt::OpaqueMode);
    }
  }

  if(context->flags2 & opts2::MAP_AIRPORT_DIAGRAM && context->mapLayerEffective->isAirportDiagram())
  {
    // Draw parking --------------------------------
    const QList<MapParking> *parkings = airportQuery->getParkingsForAirport(airport.id);
    for(const MapParking& parking : *parkings)
    {
      bool visible;
      QPoint pt = wToS(parking.position, DEFAULT_WTOS_SIZE, &visible);
      if(visible)
      {
        // Calculate approximate screen width and height
        int w = scale->getPixelIntForFeet(parking.radius, 90);
        int h = scale->getPixelIntForFeet(parking.radius, 0);

        painter->setPen(QPen(mapcolors::colorOutlineForParkingType(parking.type), 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(mapcolors::colorForParkingType(parking.type));
        painter->drawEllipse(pt, w, h);

        if(!fast)
        {
          if(parking.jetway)
            // Draw second ring for jetway
            painter->drawEllipse(pt, w * 3 / 4, h * 3 / 4);

          // Draw heading tick mark
          painter->translate(pt);
          painter->rotate(parking.heading);
          painter->drawLine(0, h * 2 / 3, 0, h);
          painter->resetTransform();
        }
      }
    }

    // Draw helipads ------------------------------------------------
    const QList<MapHelipad> *helipads = airportQuery->getHelipads(airport.id);
    if(!helipads->isEmpty())
    {
      for(const MapHelipad& helipad : *helipads)
      {
        bool visible;
        QPoint pt = wToS(helipad.position, DEFAULT_WTOS_SIZE, &visible);
        if(visible)
        {
          int w = scale->getPixelIntForFeet(helipad.width, 90) / 2;
          int h = scale->getPixelIntForFeet(helipad.length, 0) / 2;
          symbolPainter->drawHelipadSymbol(painter, helipad, pt.x(), pt.y(), w, h, fast);
        }
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
        painter->drawEllipse(pt, w < 6 ? 6 : w, h < 6 ? 6 : h);
      }
    }

    painter->setBackgroundMode(Qt::TransparentMode);

    // Draw texts ------------------------------------------------------------------

    // Draw parking and tower texts -------------------------------------------------
    QFontMetrics metrics = painter->fontMetrics();
    if(!fast && context->mapLayerEffective->isAirportDiagramDetail())
    {
      for(const MapParking& parking : *parkings)
      {
        if(context->mapLayerEffective->isAirportDiagramDetail2() || parking.radius > 40)
        {
          bool visible;
          QPoint pt = wToS(parking.position, DEFAULT_WTOS_SIZE, &visible);
          if(visible)
          {
            // Use different text pen for better readability depending on background
            painter->setPen(QPen(mapcolors::colorTextForParkingType(parking.type), 2, Qt::SolidLine, Qt::FlatCap));

            QString text;
            if(parking.type.startsWith("FUEL"))
              text = context->mapLayerEffective->isAirportDiagramDetail3() ? tr("Fuel") : tr("F");
            else if(parking.number != -1)
            {
              // FSX/P3D style names
              if(context->mapLayerEffective->isAirportDiagramDetail3())
                text = map::parkingName(parking.name) + " " + QLocale().toString(parking.number);
              else
                text = QLocale().toString(parking.number);
            }
            else if(!parking.name.isEmpty())
            {
              // X-Plane style names
              if(parking.name.size() <= 5 || context->mapLayerEffective->isAirportDiagramDetail3())
                // Use short name or full name when zoomed in enough
                text = parking.name;
              else
              {
                bool ok;
                int num = parking.name.section(" ", -1, -1).toInt(&ok);

                if(ok)
                  // Use first character and last number
                  text = parking.name.at(0) + QString::number(num);
                else
                {
                  QString firstWord = parking.name.section(" ", 0, 0);
                  if(firstWord.size() <= 5)
                    text = firstWord;
                  else
                    text = firstWord.left(4) + ".";
                }
              }
            }

            pt.setY(pt.y() + metrics.ascent() / 2);
            pt.setX(pt.x() - metrics.width(text) / 2);

            painter->drawText(pt, text);
          }
        } // if(context->mapLayerEffective->isAirportDiagramDetail2() || parking.radius > 40)
      } // for(const MapParking& parking : *parkings)
    } // if(!fast && context->mapLayerEffective->isAirportDiagramDetail())

    // Draw tower T -----------------------------
    if(!fast && airport.towerCoords.isValid())
    {
      bool visible;
      QPoint pt = wToS(airport.towerCoords, DEFAULT_WTOS_SIZE, &visible);
      if(visible)
      {
        QString text = context->mapLayerEffective->isAirportDiagramDetail3() ? tr("Tower") : tr("T");
        pt.setY(pt.y() + metrics.ascent() / 2);
        pt.setX(pt.x() - metrics.width(text) / 2);
        painter->setPen(QPen(mapcolors::towerTextColor, 2, Qt::SolidLine, Qt::FlatCap));
        painter->drawText(pt, text);
      }
    }
  }

  // Draw runway texts --------------------------------
  if(!fast && runways != nullptr)
  {
    painter->setBackgroundMode(Qt::TransparentMode);

    QFont rwTextFont = context->defaultFont;
    rwTextFont.setPixelSize(RUNWAY_TEXT_FONT_SIZE);
    painter->setFont(rwTextFont);
    QFontMetrics rwMetrics = painter->fontMetrics();

    painter->setPen(QPen(mapcolors::runwayDimsTextColor, 3, Qt::SolidLine, Qt::FlatCap));
    if(context->mapLayerEffective->isAirportDiagram())
    {
      QVector<int> runwayTextLengths;
      // Draw dimensions at runway side
      for(int i = 0; i < runwayCenters.size(); i++)
      {
        const MapRunway& runway = runways->at(i);
        const QRect& runwayRect = runwayRects.at(i);

        QString text = QString::number(Unit::distShortFeetF(runway.length), 'f', 0);

        if(runway.width > 8)
          // Skip dummy lines where the runway is done by photo scenery or similar
          text += tr(" x ") + QString::number(Unit::distShortFeetF(runway.width), 'f', 0);
        text += " " + Unit::getUnitShortDistStr();

        // Add light indicator
        if(!runway.edgeLight.isEmpty())
          text += tr(" / L");

        if(!runway.surface.isEmpty() && runway.surface != "TR" && runway.surface != "UNKNOWN" &&
           runway.surface != "INVALID")
        {
          // Draw only if valid
          QString surface = map::surfaceName(runway.surface);
          if(!surface.isEmpty())
            text += tr(" / ") + surface;
        }

        int textWidth = rwMetrics.width(text);
        if(textWidth > runwayRect.height())
          textWidth = runwayRect.height();

        int textx = -textWidth / 2, texty = -runwayRect.width() / 2;

        runwayTextLengths.append(textWidth);
        // Truncate text to runway length
        text = rwMetrics.elidedText(text, Qt::ElideRight, runwayRect.height());

        painter->translate(runwayCenters.at(i));
        painter->rotate(runway.heading > 180.f ? runway.heading + 90.f : runway.heading - 90.f);

        // Draw semi-transparent rectangle behind text
        QRect textBackRect = rwMetrics.boundingRect(text);
        textBackRect.moveTo(textx, texty - textBackRect.height() - 5);
        painter->fillRect(textBackRect, mapcolors::runwayTextBackgroundColor);

        // Draw runway length x width / L / surface
        painter->drawText(textx, texty - rwMetrics.descent() - 5, text);
        painter->resetTransform();
      }

      // Draw runway heading with arrow at the side
      QFont rwHdgTextFont = painter->font();
      rwHdgTextFont.setPixelSize(RUNWAY_HEADING_FONT_SIZE);
      painter->setFont(rwHdgTextFont);
      QFontMetrics rwHdgMetrics = painter->fontMetrics();

      for(int i = 0; i < runwayCenters.size(); i++)
      {
        const MapRunway& runway = runways->at(i);
        const QRect& runwayRect = runwayRects.at(i);

        QString textPrim;
        QString textSec;

        float rotate;
        if(runway.heading > 180.f)
        {
          // This case is rare (eg. LTAI) - probably primary in the wrong place
          rotate = runway.heading + 90.f;
          textPrim = tr("► ") +
                     formatter::courseTextFromTrue(opposedCourseDeg(runway.heading), airport.magvar, false, false);
          textSec = formatter::courseTextFromTrue(runway.heading, airport.magvar, false, false) +
                    tr(" ◄");
        }
        else
        {
          rotate = runway.heading - 90.f;
          textPrim = tr("► ") +
                     formatter::courseTextFromTrue(runway.heading, airport.magvar, false, false);
          textSec = formatter::courseTextFromTrue(opposedCourseDeg(runway.heading), airport.magvar, false, false) +
                    tr(" ◄");
        }

        QRect textRectPrim = rwHdgMetrics.boundingRect(textPrim);
        QRect textRectSec = rwHdgMetrics.boundingRect(textSec);

        if(textRectPrim.width() + textRectSec.width() + runwayTextLengths.at(i) < runwayRect.height())
        {
          // If all texts fit along the runway side draw heading
          painter->translate(runwayCenters.at(i));
          painter->rotate(rotate);

          textRectPrim.moveTo(-runwayRect.height() / 2, -runwayRect.width() / 2 - textRectPrim.height() - 5);
          painter->fillRect(textRectPrim, mapcolors::runwayTextBackgroundColor);
          painter->drawText(-runwayRect.height() / 2,
                            -runwayRect.width() / 2 - rwHdgMetrics.descent() - 5, textPrim);

          textRectSec.moveTo(runwayRect.height() / 2 - textRectSec.width(),
                             -runwayRect.width() / 2 - textRectSec.height() - 5);
          painter->fillRect(textRectSec, mapcolors::runwayTextBackgroundColor);
          painter->drawText(runwayRect.height() / 2 - textRectSec.width(),
                            -runwayRect.width() / 2 - rwHdgMetrics.descent() - 5, textSec);
          painter->resetTransform();
        }
      }
    }

    // Draw runway numbers at end
    int numSize = context->mapLayerEffective->isAirportDiagram() ?
                  RUNWAY_NUMBER_FONT_SIZE : RUNWAY_NUMBER_SMALL_FONT_SIZE;
    rwTextFont.setPixelSize(numSize);
    painter->setFont(rwTextFont);
    QFontMetrics rwTextMetrics = painter->fontMetrics();
    for(int i = 0; i < runwayCenters.size(); i++)
    {
      const MapRunway& runway = runways->at(i);
      bool primaryVisible, secondaryVisible;
      QPoint prim = wToS(runway.primaryPosition, DEFAULT_WTOS_SIZE, &primaryVisible);
      QPoint sec = wToS(runway.secondaryPosition, DEFAULT_WTOS_SIZE, &secondaryVisible);

      if(primaryVisible)
      {
        painter->translate(prim);
        painter->rotate(runway.heading);

        QRect rectSec = rwTextMetrics.boundingRect(runway.primaryName);
        rectSec.moveTo(-rectSec.width() / 2, 4);

        painter->fillRect(rectSec, mapcolors::runwayTextBackgroundColor);
        painter->drawText(-rectSec.width() / 2, rwTextMetrics.ascent() + 4, runway.primaryName);

        if(runway.primaryClosed)
        {
          // Cross out runway number
          painter->drawLine(rectSec.topLeft(), rectSec.bottomRight());
          painter->drawLine(rectSec.topRight(), rectSec.bottomLeft());
        }
        painter->resetTransform();
      }

      if(secondaryVisible)
      {
        painter->translate(sec);
        painter->rotate(runway.heading + 180.f);

        QRect rectPrim = rwTextMetrics.boundingRect(runway.secondaryName);
        rectPrim.moveTo(-rectPrim.width() / 2, 4);

        painter->fillRect(rectPrim, mapcolors::runwayTextBackgroundColor);
        painter->drawText(-rectPrim.width() / 2, rwTextMetrics.ascent() + 4, runway.secondaryName);

        if(runway.secondaryClosed)
        {
          // Cross out runway number
          painter->drawLine(rectPrim.topLeft(), rectPrim.bottomRight());
          painter->drawLine(rectPrim.topRight(), rectPrim.bottomLeft());
        }
        painter->resetTransform();
      }
    }
  }
}

/* Draw airport runway overview as in VFR maps (runways with white center line) */
void MapPainterAirport::drawAirportSymbolOverview(const PaintContext *context, const map::MapAirport& ap,
                                                  float x, float y)
{
  Marble::GeoPainter *painter = context->painter;

  if(ap.longestRunwayLength >= RUNWAY_OVERVIEW_MIN_LENGTH_FEET &&
     context->mapLayerEffective->isAirportOverviewRunway() &&
     !ap.flags.testFlag(map::AP_CLOSED) && !ap.waterOnly())
  {
    // Draw only for airports with a runway longer than 8000 feet otherwise use symbol
    atools::util::PainterContextSaver saver(painter);

    QColor apColor = mapcolors::colorForAirport(ap);
    painter->setBackgroundMode(Qt::OpaqueMode);

    // Get all runways longer than 4000 feet
    const QList<map::MapRunway> *rw = mapQuery->getRunwaysForOverview(ap.id);

    QList<QPoint> centers;
    QList<QRect> rects, innerRects;
    runwayCoords(rw, &centers, &rects, &innerRects, nullptr, true /* overview */);

    // Draw outline in airport color (magenta or green depending on tower)
    painter->setBrush(QBrush(apColor));
    painter->setPen(QPen(QBrush(apColor), 1, Qt::SolidLine, Qt::FlatCap));
    for(int i = 0; i < centers.size(); i++)
    {
      painter->translate(centers.at(i));
      painter->rotate(rw->at(i).heading);
      painter->drawRect(rects.at(i));
      painter->resetTransform();
    }

    if(!context->drawFast || context->mapLayerEffective->isAirportDiagramRunway())
    {
      // Draw white center lines
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

    // Draw small symbol on top to find a clickspot
    symbolPainter->drawAirportSymbol(context->painter, ap, x, y, 10, false, context->drawFast);
  }
}

/* Draws the airport symbol. This is not drawn if the airport is drawn using runway overview */
void MapPainterAirport::drawAirportSymbol(PaintContext *context, const map::MapAirport& ap, float x, float y)
{
  if(!context->mapLayerEffective->isAirportOverviewRunway() || ap.flags.testFlag(map::AP_CLOSED) ||
     ap.waterOnly() || ap.longestRunwayLength < RUNWAY_OVERVIEW_MIN_LENGTH_FEET ||
     context->mapLayerEffective->isAirportDiagramRunway())
  {
    if(context->objCount())
      return;

    int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
    bool isAirportDiagram = context->mapLayerEffective->isAirportDiagramRunway();

    symbolPainter->drawAirportSymbol(context->painter, ap, x, y, size, isAirportDiagram, context->drawFast);
  }
}

/*
 * Fill coordinate arrays for all runways of an airport.
 * @param runways runway input object
 * @param centers center points
 * @param rects Runway rectangles
 * @param innerRects Fill rectangles
 * @param outlineRects Big white outline
 */
void MapPainterAirport::runwayCoords(const QList<map::MapRunway> *runways, QList<QPoint> *centers,
                                     QList<QRect> *rects, QList<QRect> *innerRects,
                                     QList<QRect> *outlineRects, bool overview)
{
  for(const map::MapRunway& r : *runways)
  {
    Rect bounding(r.primaryPosition);
    bounding.extend(r.secondaryPosition);
    QSize size = scale->getScreeenSizeForRect(bounding);

    // Get the two endpoints as screen coords
    float xr1, yr1, xr2, yr2;
    wToS(r.primaryPosition, xr1, yr1, size);
    wToS(r.secondaryPosition, xr2, yr2, size);

    // Get the center point as screen coords
    float xc, yc;
    wToS(r.position, xc, yc, size);
    if(centers != nullptr)
      centers->append(QPoint(static_cast<int>(std::round(xc)), static_cast<int>(std::round(yc))));

    // Get an approximation of the runway length on the screen
    int length = atools::geo::simpleDistance(xr1, yr1, xr2, yr2);

    // Get an approximation of the runway width on the screen minimum six feet
    int width = overview ?
                RUNWAY_OVERVIEW_WIDTH_PIX :
                scale->getPixelIntForFeet(std::max(r.width, RUNWAY_MIN_WIDTH_FT), r.heading + 90.f);

    int backgroundSize = scale->getPixelIntForMeter(AIRPORT_DIAGRAM_BACKGROUND_METER);

    if(outlineRects != nullptr)
      outlineRects->append(QRect(-width - backgroundSize, -length / 2 - backgroundSize,
                                 width + backgroundSize * 2, length + backgroundSize * 2));

    if(rects != nullptr)
      rects->append(QRect(-(width / 2), -length / 2, width, length));

    if(innerRects != nullptr)
      innerRects->append(QRect(-(width / 6), -length / 2 + 2, width - 4, length - 4));
  }
}
