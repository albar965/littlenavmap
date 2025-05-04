/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "atools.h"
#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/maptypes.h"
#include "common/symbolpainter.h"
#include "common/textpointer.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "mapgui/aprongeometrycache.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "route/route.h"
#include "util/paintercontextsaver.h"

#include <QElapsedTimer>
#include <QPainterPath>
#include <QStringBuilder>
#include <QRegularExpression>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

/* Minimum width for runway diagram */
static const float RUNWAY_MIN_WIDTH_FT = 4.f;
static const double RUNWAY_OVERVIEW_WIDTH_PIX = 6.;

/* All sizes in pixel */
static const int TAXIWAY_TEXT_MIN_LENGTH = 10;
static const int RUNWAY_OVERVIEW_MIN_LENGTH_FEET = 8000;
static const float AIRPORT_DIAGRAM_BACKGROUND_METER = 200.f;

/* All taxiway paths wider than this are not drawn */
static const int MAX_TAXI_WIDTH_FT = 1000;

using namespace Marble;
using namespace atools::geo;
using namespace map;

/* Collects all runway coordinates needed for drawing */
class RunwayPaintData
{
public:
  RunwayPaintData(const map::MapRunway& runwayParam, QPointF centerParam, const QRectF& rectParam, const QRectF& outlineParam,
                  const QRectF& innerParam)
    : runway(runwayParam), center(centerParam), rect(rectParam), outline(outlineParam), inner(innerParam)
  {
  }

  const map::MapRunway& getRunway() const
  {
    return runway;
  }

  QPointF getCenter() const
  {
    return center;
  }

  const QRectF& getRect() const
  {
    return rect;
  }

  const QRectF& getOutline() const
  {
    return outline;
  }

  const QRectF& getInner() const
  {
    return inner;
  }

private:
  map::MapRunway runway;
  QPointF center;
  QRectF rect, outline, inner;
};

MapPainterAirport::MapPainterAirport(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterAirport::~MapPainterAirport()
{
}

void MapPainterAirport::render()
{
  context->startTimer("Airport");

  QVector<AirportPaintData> visibleAirports;
  collectVisibleAirports(visibleAirports);

  // In diagram mode draw background first to avoid overwriting other airports ===========================
  if(context->mapLayer->isAirportDiagramRunway() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_BOUNDARY))
  {
    for(const AirportPaintData& airportPaintData : qAsConst(visibleAirports))
      drawAirportDiagramBackground(airportPaintData.getAirport());
  }

  // Draw full airport diagrams or simple runway diagrams ===========================
  // NOT runways with white fill which is done in drawAirportSymbolOverview()
  for(const AirportPaintData& airportPaintData : qAsConst(visibleAirports))
  {
    // Either at least runway diagram enabled or airport is a part of the flight plan
    if(context->mapLayer->isAirportDiagramRunway() ||
       (context->routeProcIdMap.contains(airportPaintData.getAirport().getRef()) && context->mapLayer->isAirportOverviewRunway()))
      drawAirportDiagram(airportPaintData.getAirport());
  }

  float airportFontScale = context->mapLayerText->getAirportFontScale();
  float airportSoftFontScale = context->mapLayerText->getAirportMinorFontScale();

  // Calculate parameters for normal and soft airports
  float symsize = context->szF(context->symbolSizeAirport, context->mapLayer->getAirportSymbolSize());
  float symsizeMinor = context->szF(context->symbolSizeAirport, context->mapLayer->getAirportMinorSymbolSize());

  float apSymSize = context->mapLayer->isAirportDiagram() ? symsize * 2.f : symsize;
  float apMinorSymSize = context->mapLayer->isAirportDiagram() ? symsizeMinor * 2.f : symsizeMinor;

  // Get layer dependent text flags ===================
  textflags::TextFlags textFlags = context->airportTextFlags();
  textflags::TextFlags textFlagsMinor = context->airportTextFlagsMinor();

  // Add airport symbols on top of diagrams ===========================
  for(int i = 0; i < visibleAirports.size(); i++)
  {
    const MapAirport& airport = visibleAirports.at(i).getAirport();
    const QPointF& pt = visibleAirports.at(i).getPoint();
    float x = static_cast<float>(pt.x());
    float y = static_cast<float>(pt.y());
    bool minor = airport.isMinor();

    if(context->objCount())
      return;

    if(!context->routeProcIdMap.contains(airport.getRef()))
    {
      if(context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_RUNWAY) &&
         airport.longestRunwayLength >= RUNWAY_OVERVIEW_MIN_LENGTH_FEET &&
         context->mapLayer->isAirportOverviewRunway() && !context->mapLayer->isAirportDiagramRunway() &&
         !airport.closed() && !airport.waterOnly())
        // Draw simplified runway lines if big enough and not water or closed - runways with white fill but not if already drawn for route
        drawAirportSymbolOverview(airport, x, y, minor ? symsizeMinor : symsize);
      // More detailed symbol will be drawn by the route or log painter - skip here
      else
        drawAirportSymbol(airport, x, y, minor ? symsizeMinor : symsize);

      context->szFont(context->textSizeAirport * (minor ? airportSoftFontScale : airportFontScale));
      symbolPainter->drawAirportText(context->painter, airport, x, y, context->dispOptsAirport, minor ? textFlagsMinor : textFlags,
                                     minor ? apMinorSymSize : apSymSize, context->mapLayer->isAirportDiagram(),
                                     context->mapLayerText->getMaxTextLengthAirport());
    }
  }
  context->endTimer("Airport");
}

void MapPainterAirport::collectVisibleAirports(QVector<AirportPaintData>& visibleAirports)
{
  const static QMargins MARGINS(100, 10, 10, 10);
  QSet<QString> visibleAirportIds;
  visibleAirports.clear();

  if((!context->objectTypes.testFlag(map::AIRPORT) || !context->mapLayer->isAirport()) &&
     (!context->mapLayer->isAirportDiagramRunway()) && context->routeProcIdMap.isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);

  // Get airports from cache/database for the bounding rectangle and add them to the map
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  bool overflow = false;
  const QList<MapAirport> *airportCache = nullptr;

  // Get airports from map display cache if enabled in toolbar/menu and layer
  if(context->objectTypes.testFlag(map::AIRPORT) && context->mapLayer->isAirport())
    airportCache = queries->getMapQuery()->getAirports(curBox, context->mapLayer, context->lazyUpdate, context->objectTypes, overflow);
  context->setQueryOverflow(overflow);

  // Collect departure, destination and alternate airports from flight plan for potential diagram painting ================
  QVector<MapAirport> airports;
  QSet<int> routeAirportIds;

  // Add airports from route
  if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN))
  {
    if(context->route->getDepartureAirportLeg().isAirport())
    {
      airports.append(context->route->getDepartureAirportLeg().getAirport());
      routeAirportIds.insert(airports.constLast().id);
    }
    if(context->route->getDestinationAirportLeg().isAirport())
    {
      airports.append(context->route->getDestinationAirportLeg().getAirport());
      routeAirportIds.insert(airports.constLast().id);
    }

    // Add alternates only to index if visible
    if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE))
    {
      for(const map::MapAirport& ap : context->route->getAlternateAirports())
      {
        airports.append(ap);
        routeAirportIds.insert(airports.constLast().id);
      }
    }
  }

  // Merge flight plan airports with other visible airports
  if(airportCache != nullptr)
  {
    for(const MapAirport& airport : *airportCache)
    {
      if(!routeAirportIds.contains(airport.id))
        airports.append(airport);
    }
  }

  // Use margins for text placed on the right side of the object to avoid disappearing at the left screen border
  int minRunwayLength = context->mimimumRunwayLengthFt; // GUI setting

  // Collect all airports that are visible ===========================
  for(const MapAirport& airport : airports)
  {
    // Either part of the route or enabled in the actions/menus/toolbar
    if(airport.isVisible(context->objectTypes, minRunwayLength, context->mapLayer) || context->routeProcIdMap.contains(airport.getRef()))
    {
      float x, y;
      bool hidden;
      bool visibleOnMap = wToSBuf(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), MARGINS, &hidden);

      if(!hidden)
      {
        if(!visibleOnMap && context->mapLayer->isAirportOverviewRunway())
          // Check bounding rect for visibility if relevant - not for point symbols
          visibleOnMap = airport.bounding.overlaps(context->viewportRect);

        if(visibleOnMap)
        {
          visibleAirports.append(AirportPaintData(airport, x, y));
          visibleAirportIds.insert(airport.ident);
        }
      }
    }
  }

  using namespace std::placeholders;
  std::sort(visibleAirports.begin(), visibleAirports.end(), std::bind(&MapPainter::sortAirportFunction, this, _1, _2));
}

void MapPainterAirport::drawAirportDiagramBackground(const map::MapAirport& airport)
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

  if(context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_RUNWAY))
  {
    // Get all runways for this airport ========================================
    // Calculate all runway screen coordinates
    QVector<RunwayPaintData> runwayPaintData;
    runwayCoords(runwayPaintData, queries->getAirportQuerySim()->getRunways(airport.id), false /* overview */);

    // Draw white background ===================================================
    // For runways
    for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
    {
      // Not for water runways
      if(!paintData.getRunway().isWater())
      {
        painter->translate(paintData.getCenter());
        painter->rotate(paintData.getRunway().heading);
        painter->drawRect(paintData.getOutline());
        painter->resetTransform();
      }
    }
  }

  // Taxi only for full diagram and not the runway overview ==================
  if(context->mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI))
  {
    // For taxipaths
    const QList<MapTaxiPath> *taxipaths = queries->getAirportQuerySim()->getTaxiPaths(airport.id);
    for(const MapTaxiPath& taxipath : *taxipaths)
    {
      bool visible;
      QPoint start = wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible);
      QPoint end = wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible);
      painter->drawLine(start, end);
    }
  }

  // Apron only for full diagram and not the runway overview ==================
  if(context->mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_APRON))
  {
    // For aprons
    const QList<MapApron> *aprons = queries->getAirportQuerySim()->getAprons(airport.id);
    for(const MapApron& apron : *aprons)
    {
      // FSX/P3D geometry
      if(!apron.vertices.isEmpty())
        drawFsApron(apron);
      if(!apron.geometry.boundary.isEmpty())
        drawXplaneApron(apron, true /* draw fast */);
    }
  }
}

void MapPainterAirport::drawFsApron(const map::MapApron& apron)
{
  QVector<QPoint> apronPoints;
  bool visible;
  for(const Pos& pos : apron.vertices)
    apronPoints.append(wToS(pos, DEFAULT_WTOS_SIZE, &visible));
  context->painter->QPainter::drawPolygon(apronPoints.data(), apronPoints.size());
}

void MapPainterAirport::drawXplaneApron(const map::MapApron& apron, bool fast)
{
  // Create the apron boundary or get it from the cache for this zoom distance
  QPainterPath boundaryPath = mapPaintWidget->getApronGeometryCache()->getApronGeometry(apron, context->zoomDistanceMeter, fast);

  if(!boundaryPath.isEmpty())
    context->painter->drawPath(boundaryPath);
}

void MapPainterAirport::drawAirportDiagram(const map::MapAirport& airport)
{
  const static QMargins MARGINS(2, 2, 2, 2);
  const static QMargins MARGINS_SMALL(30, 30, 30, 30);

  Marble::GeoPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(painter);
  bool fast = context->drawFast;

  const MapLayer *mapLayerEffective = context->mapLayerEffective;
  const MapLayer *mapLayer = context->mapLayer;

  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setFont(context->defaultFont);

  QVector<RunwayPaintData> runwayPaintData;
  if(context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_RUNWAY))
  {
    // ===================================================================================
    // Get all runways for this airport and calculate all runway screen coordinates
    runwayCoords(runwayPaintData, queries->getAirportQuerySim()->getRunways(airport.id), false /* overview */);

    // Draw runway shoulders (X-Plane) ==================================================
    if(!fast && mapLayer->isAirportDiagram())
    {
      painter->setPen(Qt::NoPen);
      for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
      {
        if(!paintData.getRunway().shoulder.isEmpty() && !paintData.getRunway().isWater()) // Do not draw shoulders for water runways
        {
          painter->translate(paintData.getCenter());
          painter->rotate(paintData.getRunway().heading);

          double width = paintData.getRect().width() / 4.;
          painter->setBrush(mapcolors::colorForSurface(paintData.getRunway().shoulder));
          painter->drawRect(paintData.getRect().marginsAdded(QMarginsF(width, 0., width, 0.)));
          painter->resetTransform();
        }
      }
    }
  }

  // Draw aprons ==================================================
  if(mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_APRON))
  {
    painter->setBackground(Qt::transparent);
    const QList<MapApron> *aprons = queries->getAirportQuerySim()->getAprons(airport.id);

    for(const MapApron& apron : *aprons)
    {
      // Draw aprons a bit darker so we can see the taxiways
      QColor col = mapcolors::colorForSurface(apron.surface);
      col = col.darker(110);

      painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));
      painter->setBrush(QBrush(col));

      // FSX/P3D geometry
      if(!apron.vertices.isEmpty())
        drawFsApron(apron);

      // X-Plane geometry
      if(!apron.geometry.boundary.isEmpty())
        drawXplaneApron(apron, context->drawFast);
    }
  }

  // Draw taxiways ==================================================
  if(mapLayer->isAirportDiagram() &&
     (context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI) || context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI_LINE) ||
      context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI_NAME)))
  {
    painter->setBackgroundMode(Qt::OpaqueMode);
    QVector<QPoint> startPts, endPts;
    QVector<int> pathThickness;

    // Collect coordinates first ===========================================
    const QList<MapTaxiPath> *taxipaths = queries->getAirportQuerySim()->getTaxiPaths(airport.id);
    for(const MapTaxiPath& taxipath : *taxipaths)
    {
      bool visible;
      // Do not do any clipping here
      startPts.append(wToS(taxipath.start, DEFAULT_WTOS_SIZE, &visible));
      endPts.append(wToS(taxipath.end, DEFAULT_WTOS_SIZE, &visible));

      if(taxipath.width > 0 && taxipath.width < MAX_TAXI_WIDTH_FT)
        pathThickness.append(std::max(2, scale->getPixelIntForFeet(taxipath.width)));
      else
        // Path is either broken or special X-Plane case where width is not given for path
        pathThickness.append(0);
    }

    if(context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI))
    {
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
        }
      }

      // Draw real taxiways ===========================================
      for(int i = 0; i < taxipaths->size(); i++)
      {
        const MapTaxiPath& taxipath = taxipaths->at(i);
        if(!taxipath.closed && pathThickness.at(i) > 0)
        {
          painter->setPen(QPen(mapcolors::colorForSurface(taxipath.surface), pathThickness.at(i), Qt::SolidLine, Qt::RoundCap));
          painter->drawLine(startPts.at(i), endPts.at(i));
        }
      }
    } // if(context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI))

    // Draw center lines - also for X-Plane on the pavement ===========================================
    if(!fast && mapLayerEffective->isAirportDiagramDetail() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI_LINE))
    {
      for(int i = 0; i < taxipaths->size(); i++)
      {
        painter->setPen(mapcolors::taxiwayLinePen);
        painter->drawLine(startPts.at(i), endPts.at(i));
      }
    }

    // Draw taxiway names ==================================================
    if(!fast && mapLayerEffective->isAirportDiagramDetail() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI_NAME))
    {
      context->szFont(context->textSizeAirportTaxiway);

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

      QVector<MapTaxiPath> pathsToLabel;
      QList<MapTaxiPath> paths;
      const QStringList keys = map.uniqueKeys();
      for(const QString& taxiname : keys)
      {
        paths = map.values(taxiname);
        pathsToLabel.clear();

        // Simplified text placement - take first, last and middle name for a path
        if(!paths.isEmpty())
          pathsToLabel.append(paths.constFirst());
        if(paths.size() > 2)
          pathsToLabel.append(paths.at(paths.size() / 2));
        pathsToLabel.append(paths.constLast());

        for(const MapTaxiPath& taxipath : pathsToLabel)
        {
          bool visibleStart, visibleEnd;
          QPointF start = wToSF(taxipath.start, DEFAULT_WTOS_SIZE, &visibleStart);
          QPointF end = wToSF(taxipath.end, DEFAULT_WTOS_SIZE, &visibleEnd);

          if(visibleStart && visibleEnd)
          {
            QLineF line(start, end);

            if(line.length() > TAXIWAY_TEXT_MIN_LENGTH)
            {
              // Only draw if segment is longer than 15 pixels
              float x = static_cast<float>(line.center().x());
              float y = static_cast<float>(line.center().y());
              symbolPainter->textBoxF(painter, {taxiname}, mapcolors::taxiwayNameColor, x, y, textatt::CENTER, 255,
                                      mapcolors::taxiwayNameBackgroundColor);
            }
          }
        }
      } // for(QString taxiname : map.keys())
    } // if(!fast && mapLayerEffective->isAirportDiagramDetail() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI_NAME))
  } // if(mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_TAXI))

  // Draw runway overrun and blast pads ==================================================
  if(!fast && mapLayer->isAirportDiagram())
  {
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 1, Qt::SolidLine, Qt::FlatCap));
    for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
    {
      const QRectF& rect = paintData.getRect();
      const map::MapRunway& runway = paintData.getRunway();

      painter->translate(paintData.getCenter());
      painter->rotate(runway.heading);

      painter->setBackground(mapcolors::colorForSurface(runway.surface));

      // Draw overrun areas =========================
      if(runway.primaryOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.primaryOverrun), runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->drawRect(QRectF(rect.left(), rect.bottom(), rect.width(), offs));
      }

      if(runway.secondaryOverrun > 0)
      {
        int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.secondaryOverrun), runway.heading);
        painter->setBrush(mapcolors::runwayOverrunBrush);
        painter->drawRect(QRectF(rect.left(), rect.top() - offs, rect.width(), offs));
      }

      // Draw blast pads =========================
      if(runway.primaryBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.primaryBlastPad), runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->drawRect(QRectF(rect.left(), rect.bottom(), rect.width(), offs));
      }

      if(runway.secondaryBlastPad > 0)
      {
        int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.secondaryBlastPad), runway.heading);
        painter->setBrush(mapcolors::runwayBlastpadBrush);
        painter->drawRect(QRectF(rect.left(), rect.top() - offs, rect.width(), offs));
      }

      painter->resetTransform();
    } // for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
  } // if(!fast && runways != nullptr)

  // Draw runways ===========================================================
  if(!runwayPaintData.isEmpty())
  {
    // Draw black runway outlines ==========================================
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 3, Qt::SolidLine, Qt::FlatCap));
    painter->setBrush(Qt::NoBrush);

    for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
    {
      // No outline for water
      if(!paintData.getRunway().isWater())
      {
        painter->translate(paintData.getCenter());
        painter->rotate(paintData.getRunway().heading);
        painter->drawRect(paintData.getRect().marginsAdded(MARGINS));
        painter->resetTransform();
      }
    }

    // Draw runway surface ==========================================
    for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
    {
      QColor col = mapcolors::colorForSurface(paintData.getRunway().surface);

      painter->translate(paintData.getCenter());
      painter->rotate(paintData.getRunway().heading);

      painter->setBrush(col);
      painter->setPen(QPen(col, 1, Qt::SolidLine, Qt::FlatCap));
      painter->drawRect(paintData.getRect());
      painter->resetTransform();
    }

    // Draw runway offset thresholds =====================================================
    if(!fast)
    {
      painter->setBackgroundMode(Qt::TransparentMode);
      for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
      {
        const MapRunway& runway = paintData.getRunway();

        QColor colThreshold = mapcolors::colorForSurface(runway.surface).value() < 220 ?
                              mapcolors::runwayOffsetColor : mapcolors::runwayOffsetColorDark;
        painter->setBrush(colThreshold);

        if(runway.primaryOffset > 0.f || runway.secondaryOffset > 0.f)
        {
          const QRectF& rect = paintData.getRect();

          painter->translate(paintData.getCenter());
          painter->rotate(runway.heading);

          if(runway.primaryOffset > 0.f)
          {
            int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.primaryOffset), runway.heading);

            // Draw solid boundary to runway
            painter->setPen(QPen(colThreshold, 3, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(QPointF(rect.left(), rect.bottom() - offs), QPointF(rect.right(), rect.bottom() - offs));

            // Draw dashed line
            painter->setPen(QPen(colThreshold, 3, Qt::DashLine, Qt::FlatCap));
            painter->drawLine(QPointF(0, rect.bottom()), QPointF(0, rect.bottom() - offs));
          }

          if(runway.secondaryOffset > 0)
          {
            int offs = scale->getPixelIntForFeet(atools::roundToInt(runway.secondaryOffset), runway.heading);

            // Draw solid boundary to runway
            painter->setPen(QPen(colThreshold, 3, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(QPointF(rect.left(), rect.top() + offs), QPointF(rect.right(), rect.top() + offs));

            // Draw dashed line
            painter->setPen(QPen(colThreshold, 3, Qt::DashLine, Qt::FlatCap));
            painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.top() + offs));
          }
          painter->resetTransform();
        } // if(runway.primaryOffset > 0.f || runway.secondaryOffset > 0.f)
      } // for(int i = 0; i < runwayCenters.size(); i++)
      painter->setBackgroundMode(Qt::OpaqueMode);
    } // if(!fast)
  }

  // Draw parking, fuel and tower ===========================================================
  if(mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_PARKING))
  {
    // Add to index indicating that tooltips for parking or helipads are needed
    context->shownDetailAirportIds->insert(airport.id);

    // Approximate needed margins by largest parking diameter to avoid parking circles dissappearing on the screen borders
    int size = scale->getPixelIntForFeet(200);
    QMargins margins(size, size, size, size);

    // Collect parking information to avoid duplicate calculation
    struct Parking
    {
      Parking(float xParam, float yParam, float wParam, float hParam)
        : x(xParam), y(yParam), width(wParam), height(hParam)
      {
      }

      bool isValid()const
      {
        return width > 0.f && height > 0.f;
      }

      float x, y, width, height;
    };

    QVector<Parking> parkingList;

    const QList<MapParking> *parkings = queries->getAirportQuerySim()->getParkingsForAirport(airport.id);
    for(const MapParking& parking : *parkings)
    {
      float x = 0.f, y = 0.f;
      double w = 0., h = 0.;
      if(wToSBuf(parking.position, x, y, margins))
      {
        int radius = parking.getRadius();

        if(radius == 0)
          continue;

        // Calculate approximate screen width and height
        w = scale->getPixelForFeet(radius, 90.f);
        h = scale->getPixelForFeet(radius, 0.f);

        painter->setPen(QPen(mapcolors::colorOutlineForParkingType(parking.type), 2, Qt::SolidLine, Qt::FlatCap));
        painter->setBrush(mapcolors::colorForParkingType(parking.type));
        painter->drawEllipse(QPointF(x, y), w, h);

        if(!fast)
        {
          if(parking.jetway)
            // Draw second ring for jetway
            painter->drawEllipse(QPointF(x, y), w * 3. / 4., h * 3. / 4.);

          if(parking.heading < map::INVALID_HEADING_VALUE)
          {
            // Draw heading tick mark
            painter->translate(QPointF(x, y));
            painter->rotate(parking.heading);
            painter->drawLine(QPointF(0., h * 2. / 3.), QPointF(0., h));
            painter->resetTransform();
          }
        }
      }

      // Always add an entry to keep in sync with parkings
      parkingList.append(Parking(x, y, static_cast<float>(w), static_cast<float>(h)));
    } // for(const MapParking& parking : *parkings)

    // Draw helipads ==================================================
    const QList<MapHelipad> *helipads = queries->getAirportQuerySim()->getHelipads(airport.id);
    if(!helipads->isEmpty())
    {
      for(const MapHelipad& helipad : *helipads)
      {
        float x, y;
        if(wToSBuf(helipad.position, x, y, margins))
        {
          int w = scale->getPixelIntForFeet(helipad.width, 90) / 2;
          int h = scale->getPixelIntForFeet(helipad.length, 0) / 2;
          SymbolPainter::drawHelipadSymbol(painter, helipad, x, y, w, h, fast);
        }
      }
    }

    // Draw tower ==================================================
    if(airport.towerCoords.isValid())
    {
      float x, y;
      if(wToSBuf(airport.towerCoords, x, y, MARGINS_SMALL))
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

        double w = scale->getPixelForMeter(10.f, 90);
        double h = scale->getPixelForMeter(10.f, 0);
        painter->drawEllipse(QPointF(x, y), w < 6. ? 6. : w, h < 6. ? 6. : h);
      }
    }

    painter->setBackgroundMode(Qt::TransparentMode);

    // Draw parking, fuel and tower texts ===========================================================
    painter->setFont(context->defaultFont);
    QFontMetricsF metrics(painter->font());
    if(!fast && mapLayerEffective->isAirportDiagramDetail())
    {
      for(int i = 0; i < parkingList.size(); i++)
      {
        const Parking& parkingSpot = parkingList.at(i);
        if(parkingSpot.isValid())
        {
          const MapParking& parking = parkings->at(i);

          // Use different text pen for better readability depending on background
          painter->setPen(QPen(mapcolors::colorTextForParkingType(parking.type), 2, Qt::SolidLine, Qt::FlatCap));

          // Get possibly truncated parking name but not for lowest layer
          QString text = parkingNameForSize(parking, mapLayerEffective->isAirportDiagramDetail3() ? 0.f : parkingSpot.width * 2.2f);
          if(!text.isEmpty())
            painter->drawText(QPointF(parkingSpot.x - metrics.horizontalAdvance(text) / 2., parkingSpot.y + metrics.ascent() / 2.), text);
        }
      } // for(const MapParking& parking : *parkings)

      // Draw tower T ==========================================
      if(airport.towerCoords.isValid())
      {
        float x, y;
        if(wToSBuf(airport.towerCoords, x, y, MARGINS_SMALL))
        {
          QString text = mapLayerEffective->isAirportDiagramDetail3() ? tr("Tower") : tr("T", "Tower short name");
          painter->setPen(QPen(mapcolors::towerTextColor, 2, Qt::SolidLine, Qt::FlatCap));
          painter->drawText(QPointF(x - metrics.horizontalAdvance(text) / 2., y + metrics.ascent() / 2.), text);
        }
      }
    } // if(!fast && mapLayer->isAirportDiagramDetail())
  } // if(mapLayer->isAirportDiagram() && context->dOptAp(optsd::ITEM_AIRPORT_DETAIL_PARKING))

  // Draw runway texts ===========================================================
  if(!fast && !runwayPaintData.isEmpty())
  {
    const static QMarginsF RUNWAY_DIMENSION_MARGINS(4., 0., 4., 0.);
    painter->setBackgroundMode(Qt::TransparentMode);
    painter->setPen(QPen(mapcolors::runwayDimsTextColor, 3, Qt::SolidLine, Qt::FlatCap));

    if(mapLayer->isAirportDiagram())
    {
      context->szFont(context->textSizeAirportRunway * 1.5f);
      QFontMetricsF rwMetrics(painter->font());

      QVector<double> runwayTextLengths;

      // Draw dimensions at runway side ===========================================================
      for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
      {
        const MapRunway& runway = paintData.getRunway();
        const QRectF& runwayRect = paintData.getRect();

        QString runwayText = QString::number(Unit::distShortFeetF(runway.length), 'f', 0);

        if(runway.width > 8.f)
          // Skip dummy lines where the runway is done by photo scenery or similar
          runwayText.append(tr(" x ") % QString::number(Unit::distShortFeetF(runway.width), 'f', 0));
        runwayText.append(" " % Unit::getUnitShortDistStr());

        // Add light indicator
        if(!runway.edgeLight.isEmpty())
          runwayText.append(tr(" / L"));

        if(map::surfaceValid(runway.surface))
        {
          // Draw only if valid
          const QString& surface = map::surfaceName(runway.surface);
          if(!surface.isEmpty())
            runwayText.append(tr(" / ") % surface);
        }

        // Truncate text to runway length
        runwayText = rwMetrics.elidedText(runwayText, Qt::ElideRight,
                                          runwayRect.height() - RUNWAY_DIMENSION_MARGINS.left() - RUNWAY_DIMENSION_MARGINS.right());

        painter->translate(paintData.getCenter());
        painter->rotate(runway.heading > 180.f ? runway.heading + 90.f : runway.heading - 90.f);

        // Draw semi-transparent rectangle behind text
        QRectF textBackRect = rwMetrics.boundingRect(runwayText);
        textBackRect = textBackRect.marginsAdded(RUNWAY_DIMENSION_MARGINS);

        // Remember width to exclude end arrows or not
        runwayTextLengths.append(textBackRect.width() > runwayRect.height() ? runwayRect.height() : textBackRect.width());

        double textx = -textBackRect.width() / 2., texty = -runwayRect.width() / 2.;
        textBackRect.moveTo(textx, texty - textBackRect.height() - 5.);
        painter->fillRect(textBackRect, mapcolors::runwayTextBackgroundColor);

        // Draw runway length x width / L / surface
        painter->drawText(QPointF(textx + RUNWAY_DIMENSION_MARGINS.left(), texty - rwMetrics.descent() - 5.), runwayText);
        painter->resetTransform();
      }

      // Draw runway heading numbers with arrows at ends ========================================================================
      const static QMarginsF RUNWAY_HEADING_MARGINS(2., 0., 2., 0.);
      context->szFont(context->textSizeAirportRunway);
      QFontMetricsF rwHdgMetrics(painter->font());

      for(int i = 0; i < runwayPaintData.size(); i++)
      {
        const RunwayPaintData& paintData = runwayPaintData.at(i);

        QString textPrim;
        QString textSec;

        float rotate;
        bool forceBoth = std::abs(airport.magvar) > 90.f;
        const MapRunway& runway = paintData.getRunway();
        if(runway.heading > 180.f)
        {
          // This case is rare (eg. LTAI) - probably primary in the wrong place
          rotate = runway.heading + 90.f;
          textPrim = tr("%1 ").arg(TextPointer::getPointerRight()) %
                     formatter::courseTextFromTrue(opposedCourseDeg(runway.heading), airport.magvar, false /* magBold */,
                                                   false /* magBig */, false /* trueSmall */, true /* narrow */, forceBoth);

          textSec = formatter::courseTextFromTrue(runway.heading, airport.magvar, false /* magBold */, false /* magBig */,
                                                  false /* trueSmall */, true /* narrow */, forceBoth) %
                    tr(" %1").arg(TextPointer::getPointerLeft());
        }
        else
        {
          rotate = runway.heading - 90.f;
          textPrim = tr("%1 ").arg(TextPointer::getPointerRight()) %
                     formatter::courseTextFromTrue(runway.heading, airport.magvar, false /* magBold */, false /* magBig */,
                                                   false /* trueSmall */, true /* narrow */, forceBoth);
          textSec = formatter::courseTextFromTrue(opposedCourseDeg(runway.heading), airport.magvar, false /* magBold */,
                                                  false /* magBig */, false /* trueSmall */, true /* narrow */, forceBoth) %
                    tr(" %1").arg(TextPointer::getPointerLeft());
        }

        // FreeType on Windows has a bug where the arrows break calculation of the text size
        QString textPrimMetrics(textPrim), textSecMetrics(textSec);
#ifdef Q_OS_WIN
        if(OptionData::instance().getFlags().testFlag(opts::GUI_FREETYPE_FONT_ENGINE))
        {
          textPrimMetrics.replace(TextPointer::getPointerRight(), ">");
          textSecMetrics.replace(TextPointer::getPointerLeft(), "<");
        }
#endif
        QRectF textRectPrim = rwHdgMetrics.boundingRect(textPrimMetrics).marginsAdded(RUNWAY_HEADING_MARGINS);
        textRectPrim.setHeight(rwHdgMetrics.height());

        QRectF textRectSec = rwHdgMetrics.boundingRect(textSecMetrics).marginsAdded(RUNWAY_HEADING_MARGINS);
        textRectSec.setHeight(rwHdgMetrics.height());

        // If all texts fit along the runway side draw heading
        const QRectF& runwayRect = paintData.getRect();
        if(textRectPrim.width() + textRectSec.width() + runwayTextLengths.at(i) < runwayRect.height())
        {
          painter->translate(paintData.getCenter()); // Center to 0,0
          painter->rotate(rotate); // Rotate runway to horizontal layout

          textRectPrim.moveTo(-runwayRect.height() / 2., -runwayRect.width() / 2. - textRectPrim.height() - 5.);
          painter->fillRect(textRectPrim, mapcolors::runwayTextBackgroundColor);
          painter->drawText(QPointF(-runwayRect.height() / 2. + RUNWAY_HEADING_MARGINS.left(),
                                    -runwayRect.width() / 2. - rwHdgMetrics.descent() - 5.), textPrim);

          textRectSec.moveTo(runwayRect.height() / 2. - textRectSec.width(), -runwayRect.width() / 2. - textRectSec.height() - 5.);
          painter->fillRect(textRectSec, mapcolors::runwayTextBackgroundColor);
          painter->drawText(QPointF(runwayRect.height() / 2. - textRectSec.width() + RUNWAY_HEADING_MARGINS.left(),
                                    -runwayRect.width() / 2. - rwHdgMetrics.descent() - 5.), textSec);
          painter->resetTransform();
        }
      } // for(int i = 0; i < runwayPaintData.size(); i++)
    } // if(mapLayer->isAirportDiagram())

    // Draw runway numbers at end ========================================================================
    const static QMarginsF RUNWAY_NUMBER_MARGINS(2., 0., 2., 0.);

    context->szFont(context->textSizeAirportRunway * (mapLayer->isAirportDiagram() ? 1.8f : 1.2f));
    painter->setPen(QPen(mapcolors::runwayOutlineColor, 2., Qt::SolidLine, Qt::FlatCap));
    QFontMetricsF runwayTextMetrics(painter->font());
    QMargins margins(20, 20, 20, 20);

    for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
    {
      const MapRunway& runway = paintData.getRunway();
      float x, y;
      if(wToSBuf(runway.primaryPosition, x, y, margins))
      {
        painter->translate(x, y);
        painter->rotate(runway.heading);

        // Calculate background rectangle with margins
        QRectF textRect = runwayTextMetrics.boundingRect(runway.primaryName);
        textRect = textRect.marginsAdded(RUNWAY_NUMBER_MARGINS);
        textRect.moveTo(-textRect.width() / 2., 4.);

        painter->fillRect(textRect, mapcolors::runwayTextBackgroundColor);
        painter->drawText(QPointF(-textRect.width() / 2. + RUNWAY_NUMBER_MARGINS.left(), runwayTextMetrics.ascent() + 4.),
                          runway.primaryName);

        if(runway.primaryClosed)
        {
          // Cross out runway number
          double radius = std::max(textRect.width(), textRect.height()) / 3.;
          double centerX = textRect.center().x();
          double centerY = textRect.center().y();
          painter->drawLine(QPointF(centerX - radius, centerY - radius), QPointF(centerX + radius, centerY + radius));
          painter->drawLine(QPointF(centerX + radius, centerY - radius), QPointF(centerX - radius, centerY + radius));
        }
        painter->resetTransform();
      }

      if(wToSBuf(runway.secondaryPosition, x, y, margins))
      {
        painter->translate(x, y);
        painter->rotate(runway.heading + 180.f);

        // Calculate background rectangle with margins
        QRectF textRect = runwayTextMetrics.boundingRect(runway.secondaryName);
        textRect = textRect.marginsAdded(RUNWAY_NUMBER_MARGINS);
        textRect.moveTo(-textRect.width() / 2., 4.);

        painter->fillRect(textRect, mapcolors::runwayTextBackgroundColor);
        painter->drawText(QPointF(-textRect.width() / 2. + RUNWAY_NUMBER_MARGINS.left(), runwayTextMetrics.ascent() + 4.),
                          runway.secondaryName);

        if(runway.secondaryClosed)
        {
          // Cross out runway number
          double radius = std::max(textRect.width(), textRect.height()) / 3.;
          double centerX = textRect.center().x();
          double centerY = textRect.center().y();
          painter->drawLine(QPointF(centerX - radius, centerY - radius), QPointF(centerX + radius, centerY + radius));
          painter->drawLine(QPointF(centerX + radius, centerY - radius), QPointF(centerX - radius, centerY + radius));
        }
        painter->resetTransform();
      }
    } // for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
  } // if(!fast && !runwayPaintData.isEmpty())
}

void MapPainterAirport::drawAirportSymbolOverview(const map::MapAirport& airport, float x, float y, float symsize)
{
  Marble::GeoPainter *painter = context->painter;

  // Draw only for airports with a runway longer than 8000 feet otherwise use symbol
  atools::util::PainterContextSaver saver(painter);

  QColor apColor = mapcolors::colorForAirport(airport);
  painter->setBackgroundMode(Qt::OpaqueMode);

  // Get all runways longer than 4000 feet
  QVector<RunwayPaintData> runwayPaintData;
  runwayCoords(runwayPaintData, queries->getMapQuery()->getRunwaysForOverview(airport.id), true /* overview */);

  // Draw outline in airport color (magenta or green depending on tower)
  bool runwayDrawn = false; // Use small icon if runways are visible
  painter->setBrush(QBrush(apColor));
  painter->setPen(QPen(QBrush(apColor), 1, Qt::SolidLine, Qt::FlatCap));

  for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
  {
    if(paintData.getRect().height() > 10)
    {
      painter->translate(paintData.getCenter());
      painter->rotate(paintData.getRunway().heading);
      painter->drawRect(paintData.getRect());
      painter->resetTransform();
      runwayDrawn = true;
    }
  }

  // Draw white center lines
  painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), 1, Qt::SolidLine, Qt::FlatCap));
  painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  for(const RunwayPaintData& paintData : qAsConst(runwayPaintData))
  {
    if(paintData.getRect().height() > 10)
    {
      painter->translate(paintData.getCenter());
      painter->rotate(paintData.getRunway().heading);
      painter->drawRect(paintData.getInner());
      painter->resetTransform();
      runwayDrawn = true;
    }
  }

  // Draw small symbol on top to find a clickspot
  symbolPainter->drawAirportSymbol(context->painter, airport, x, y,
                                   runwayDrawn ? 10.f : symsize, // Draw small icon only if runways are visible
                                   false /* isAirportDiagram */, context->drawFast,
                                   context->flags2.testFlag(opts2::MAP_AIRPORT_HIGHLIGHT_ADDON));
}

/* Draws the airport symbol. This is not drawn if the airport is drawn using runway overview */
void MapPainterAirport::drawAirportSymbol(const map::MapAirport& airport, float x, float y, float size)
{
  symbolPainter->drawAirportSymbol(context->painter, airport, x, y, size, context->mapLayer->isAirportDiagramRunway(), context->drawFast,
                                   context->flags2.testFlag(opts2::MAP_AIRPORT_HIGHLIGHT_ADDON));
}

void MapPainterAirport::runwayCoords(QVector<RunwayPaintData>& runwayPaintData, const QList<MapRunway> *runways, bool overview)
{
  if(runways != nullptr)
  {
    for(const map::MapRunway& runway : *runways)
    {
      Rect bounding(runway.primaryPosition);
      // bounding.extend(runway.secondaryPosition);
      QSize size = scale->getScreeenSizeForRect(bounding);

      // Get the two endpoints as screen coords
      float xr1, yr1, xr2, yr2;
      wToS(runway.primaryPosition, xr1, yr1, size);
      wToS(runway.secondaryPosition, xr2, yr2, size);

      // Get the center point as screen coords
      float xc, yc;
      wToS(runway.position, xc, yc, size);

      // Get an approximation of the runway length on the screen
      double length = atools::geo::simpleDistance(xr1, yr1, xr2, yr2);

      // Get an approximation of the runway width on the screen minimum six feet
      double width = overview ? RUNWAY_OVERVIEW_WIDTH_PIX :
                     scale->getPixelForFeet(std::max(runway.width, RUNWAY_MIN_WIDTH_FT), runway.heading + 90.f);

      double backgroundSize = scale->getPixelForMeter(AIRPORT_DIAGRAM_BACKGROUND_METER);

      QRectF outline(-width - backgroundSize, -length / 2. - backgroundSize,
                     width + backgroundSize * 2., length + backgroundSize * 2.);
      QRectF rect(-width / 2., -length / 2., width, length);
      QRectF inner(-width / 6., -length / 2. + 2., width - 4., length - 4.);

      runwayPaintData.append(RunwayPaintData(runway, QPointF(xc, yc), rect, outline, inner));
    }
  }
}

QString MapPainterAirport::parkingReplaceKeywords(QString parkingName, bool erase)
{
  const auto& replacements = map::parkingKeywords();
  parkingName.replace('_', ' ');
  // parkingName.replace('-', ' ');

  if(erase)
  {
    for(const auto& str : replacements)
      parkingName.remove(str.first);
  }
  else
  {
    for(const auto& str : replacements)
      parkingName.replace(str.first, str.second);
  }
  return parkingName.simplified();
}

QString MapPainterAirport::parkingCompressDigits(const QString& parkingName)
{
  QStringList strings = parkingName.split(' ');
  for(int i = 0; i < strings.size(); i++)
  {
    QString& str = strings[i];

    if(i > 0)
    {
      bool ok;
      str.toUInt(&ok);

      if(ok)
      {
        QString& prev = strings[i - 1];
        str = prev % str;
        prev.clear();
      }
    }
  }
  strings.removeAll(QString());
  return strings.join(' ');
}

QString MapPainterAirport::parkingExtractNumber(const QString& parkingName)
{
  QString number;
  for(int i = parkingName.size() - 1; i >= 0; i--)
  {
    QChar c = parkingName.at(i);
    if(c.isDigit())
      number.prepend(c);
    else if(!number.isEmpty())
      break;
  }
  return number.simplified();
}

QString MapPainterAirport::parkingNameForSize(const map::MapParking& parking, float width)
{
  QString text;
  if(parking.number != -1)
    // FSX/P3D style names =========
    text = map::parkingName(parking.name) % " " % QLocale().toString(parking.number) % parking.suffix;
  else if(!parking.name.isEmpty())
    // X-Plane style names =========
    text = parking.name;

  // Try to extract and/or shorten keywords and numbers =============
  QString shortText = text;
  if(width > 0.f)
  {
    QFontMetricsF metrics(context->painter->font());
    if(metrics.horizontalAdvance(shortText) > width)
    {
      // "Gate A 11" to "G A 11"
      shortText = parkingReplaceKeywords(text, false);

      if(metrics.horizontalAdvance(shortText) > width)
      {
        // "Gate A 11" to "A 11"
        shortText = parkingReplaceKeywords(text, true);

        if(metrics.horizontalAdvance(shortText) > width)
        {
          // "A 11" to "A11"
          shortText = parkingCompressDigits(shortText);

          if(metrics.horizontalAdvance(shortText) > width)
            // "A11" to "11"
            shortText = parkingExtractNumber(shortText);
        }
      }
    }

    if(shortText.isEmpty())
      // Nothing found elide original text
      shortText = atools::elidedText(metrics, text, Qt::ElideRight, width);
    else if(metrics.horizontalAdvance(shortText) > width)
      // Shortened but still too long
      shortText = atools::elidedText(metrics, shortText, Qt::ElideRight, width);

    shortText = shortText.simplified();

    // Clear if only a dot or ellipse
    if(shortText == atools::elidedStrDot() || shortText == atools::elidedStrDots())
      shortText.clear();

    return shortText;
  }
  else
    return atools::elideTextShort(text, 30);
}
