/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/mappaintermark.h"

#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "perf/aircraftperfcontroller.h"
#include "common/mapcolors.h"
#include "common/formatter.h"
#include "geo/calculations.h"
#include "util/roundedpolygon.h"
#include "common/symbolpainter.h"
#include "geo/rect.h"
#include "atools.h"
#include "common/symbolpainter.h"
#include "airspace/airspacecontroller.h"
#include "common/unit.h"
#include "route/routeleg.h"
#include "route/route.h"
#include "util/paintercontextsaver.h"
#include "common/textplacement.h"
#include "fs/userdata/logdatamanager.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/GeoPainter.h>

#include <QPainterPath>
#include <QStringBuilder>

const float MAX_COMPASS_ROSE_RADIUS_NM = 500.f;
const float MIN_COMPASS_ROSE_RADIUS_NM = 0.2f;
const double MIN_VIEW_DISTANCE_COMPASS_ROSE_KM = 6400.;

#ifdef DEBUG_INFORMATION_MEASUREMENT
const int DISTMARK_DEG_PRECISION = 3;
#else
const int DISTMARK_DEG_PRECISION = 0;
#endif

using namespace Marble;
using namespace map;
namespace ageo = atools::geo;

MapPainterMark::MapPainterMark(MapPaintWidget *mapWidgetParam, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidgetParam, mapScale, paintContext)
{
  magTrueSuffix = tr("°M/T");
  magSuffix = tr("°M");
  trueSuffix = tr("°T");
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::render()
{
  atools::util::PainterContextSaver saver(context->painter);

  paintMark();
  paintHome();

  // Traffic patterns
  if(context->objectTypes.testFlag(map::MARK_PATTERNS))
    paintPatternMarks();

  if(context->objectTypes.testFlag(map::MARK_HOLDING))
    paintHoldingMarks(mapPaintWidget->getHoldingMarksFiltered(), true /* user */, context->drawFast);

  // Airport MSA set by user
  if(context->objectTypes.testFlag(map::MARK_MSA))
    paintMsaMarks(mapPaintWidget->getMsaMarksFiltered(), true /* user */, context->drawFast);

  // All range rings
  if(context->objectTypes.testFlag(map::MARK_RANGE))
    paintRangeMarks();

  // Measurement lines
  if(context->objectTypes.testFlag(map::MARK_DISTANCE))
    paintDistanceMarks();

  paintCompassRose();
  paintEndurance();
  paintSelectedAltitudeRange();

  paintHighlights();
  paintRouteDrag();
  paintUserpointDrag();

#ifdef DEBUG_AIRWAY_PAINT
  context->painter->setPen(QPen(QColor(0, 0, 255, 50), 10, Qt::SolidLine, Qt::RoundCap));
  MapScreenIndex *screenIndex = mapPaintWidget->getScreenIndex();
  screenIndex->updateAirwayScreenGeometry(mapPaintWidget->getCurrentViewBoundingBox());
  QList<std::pair<int, QLine> > airwayLines = screenIndex->getAirwayLines();
  for(std::pair<int, QLine> pair: airwayLines)
    context->painter->drawLine(pair.second);
#endif
}

void MapPainterMark::paintMark()
{
  GeoPainter *painter = context->painter;

  int x, y;
  if(wToS(mapPaintWidget->getSearchMarkPos(), x, y))
  {
    context->painter->setPen(mapcolors::searchCenterBackPen);
    drawCross(painter, x, y, context->sz(context->symbolSizeAirport, 10));

    context->painter->setPen(mapcolors::searchCenterFillPen);
    drawCross(painter, x, y, context->sz(context->symbolSizeAirport, 8));
  }
}

void MapPainterMark::paintHome()
{
  GeoPainter *painter = context->painter;

  int x, y;
  if(wToS(mapPaintWidget->getHomePos(), x, y))
  {
    int size = atools::roundToInt(context->szF(context->textSizeRangeUserFeature, 24));

    if(x < ageo::INVALID_INT / 2 && y < ageo::INVALID_INT / 2)
    {
      QPixmap pixmap;
      getPixmap(pixmap, ":/littlenavmap/resources/icons/homemap.svg", size);
      painter->drawPixmap(QPoint(x - size / 2, y - size / 2), pixmap);
    }
  }
}

void MapPainterMark::paintHighlights()
{
  bool transparent = context->flags2.testFlag(opts2::MAP_HIGHLIGHT_TRANSPARENT);
  const MapLayer *mapLayer = context->mapLayer;
  GeoPainter *painter = context->painter;

  // Draw highlight from the search result view =====================================================
  const MapResult& highlightResultsSearch = mapPaintWidget->getSearchHighlights();

  // Get airport entries from log to avoid rings around log entry airports
  QSet<int> logIds;
  for(const MapLogbookEntry& entry : highlightResultsSearch.logbookEntries)
  {
    if(entry.departurePos.isValid())
      logIds.insert(entry.departure.id);
    if(entry.destinationPos.isValid())
      logIds.insert(entry.destination.id);
  }

  // Draw boundary for selected online network airspaces =====================================================
  for(const MapAirspace& airspace: highlightResultsSearch.airspaces)
  {
    if(airspace.hasValidGeometry())
      paintAirspace(airspace);
  }

  // Draw boundary for airspaces highlighted in the information window =======================================
  for(const MapAirspace& airspace: mapPaintWidget->getAirspaceHighlights())
  {
    if(airspace.hasValidGeometry())
      paintAirspace(airspace);
  }

  // Draw airways highlighted in the information window =====================================================
  for(const QList<MapAirway>& airwayFull : mapPaintWidget->getAirwayHighlights())
    paintAirwayList(airwayFull);

  for(const QList<MapAirway>& airwayFull : mapPaintWidget->getAirwayHighlights())
    paintAirwayTextList(airwayFull);

  float alpha = transparent ? (1.f - context->transparencyHighlight) : 1.f;

  // Selected logbook entries ------------------------------------------
  if(highlightResultsSearch.hasLogEntries())
    paintLogEntries(highlightResultsSearch.logbookEntries);

  // ====================================================================
  // Collect all highlight rings for positions =============
  // Feature position and size (not radius)
  QVector<std::pair<ageo::Pos, float> > positionSizeList;
  for(const MapAirport& ap : highlightResultsSearch.airports)
  {
    // Do not add if already drawn by logbook preview
    if(!logIds.contains(ap.id))
      positionSizeList.append(std::make_pair(ap.position, context->szF(context->symbolSizeAirport, mapLayer->getAirportSymbolSize())));
  }

  for(const MapWaypoint& wp : highlightResultsSearch.waypoints)
    positionSizeList.append(std::make_pair(wp.position, context->szF(context->symbolSizeNavaid, mapLayer->getWaypointSymbolSize())));

  for(const MapVor& vor : highlightResultsSearch.vors)
    positionSizeList.append(std::make_pair(vor.position, context->szF(context->symbolSizeNavaid, mapLayer->getVorSymbolSize())));

  for(const MapNdb& ndb : highlightResultsSearch.ndbs)
    positionSizeList.append(std::make_pair(ndb.position, context->szF(context->symbolSizeNavaid, mapLayer->getNdbSymbolSize())));

  for(const MapUserpoint& user : highlightResultsSearch.userpoints)
    positionSizeList.append(std::make_pair(user.position, context->szF(context->symbolSizeUserpoint, mapLayer->getUserPointSymbolSize())));

  for(const map::MapOnlineAircraft& aircraft: highlightResultsSearch.onlineAircraft)
  {
    float size = std::max(context->sz(context->symbolSizeAircraftAi, mapLayer->getAiAircraftSize()),
                          scale->getPixelIntForFeet(aircraft.getAircraft().getModelSize()));
    positionSizeList.append(std::make_pair(aircraft.getPosition(), size * 0.75f));
  }

  // ====================================================================
  // Draw all highlight rings for collected positions and sizes =============
  QColor highlightColor = OptionData::instance().getHighlightSearchColor();
  painter->setBrush(transparent ? QBrush(mapcolors::adjustAlphaF(highlightColor, alpha)) : QBrush(Qt::NoBrush));
  for(const std::pair<ageo::Pos, float>& posSize : positionSizeList)
  {
    float x, y;
    if(wToS(posSize.first, x, y))
    {
      // Adjust to highlight size in options
      float size = context->szF(context->symbolSizeHighlight, posSize.second);

      // Make radius a bit bigger and set minimum
      float radius = std::max(size / 2.f * 1.4f, 6.f);

      if(!context->drawFast && !transparent)
      {
        // Draw black background for outline
        painter->setPen(QPen(mapcolors::highlightBackColor, radius / 3. + 2., Qt::SolidLine, Qt::FlatCap));
        painter->drawEllipse(QPointF(x, y), radius, radius);
      }
      painter->setPen(QPen(mapcolors::adjustAlphaF(highlightColor, alpha), transparent ? 1. : radius / 3., Qt::SolidLine, Qt::FlatCap));
      painter->drawEllipse(QPointF(x, y), radius, radius);
    }
  }

  // Draw highlights from the approach fix selection =====================================================
  const QColor outerColorProc(mapcolors::highlightBackColor);
  const QColor innerColorProc(mapcolors::adjustAlphaF(mapcolors::highlightProcedureColor, alpha));
  painter->setBrush(transparent ? QBrush(mapcolors::adjustAlphaF(mapcolors::highlightProcedureColor, alpha)) : QBrush(Qt::NoBrush));

  // Use one radius for all procedure fixes and flight plan waypoints
  float radius = context->szF(context->symbolSizeAirport, transparent ? 3.f : 6.f);
  if(mapLayer->isAirport())
    radius = std::max(radius, static_cast<float>(mapLayer->getAirportSymbolSize()));

  // Adjust to highlight size in options
  radius = context->szF(context->symbolSizeHighlight, radius);

  const proc::MapProcedureLeg& leg = mapPaintWidget->getProcedureLegHighlight();
  if(leg.isValid())
  {
    // Recommended fixes =========================
    if(leg.recFixPos.isValid())
    {
      float ellipseSize = radius / 2.f;

      float x, y;
      if(wToS(leg.recFixPos, x, y))
      {
        // Draw recommended fix with a thin small circle
        if(!context->drawFast && !transparent)
        {
          painter->setPen(QPen(outerColorProc, radius / 5 + 2));
          painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
        }
        painter->setPen(QPen(innerColorProc, transparent ? 0 : radius / 5));
        painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
      }
    }

    // Procedure fixes =========================
    if(leg.line.isValid())
    {
      float ellipseSize = radius;

      float x, y;
      if(wToS(leg.line.getPos1(), x, y))
      {
        if(!leg.line.isPoint() || leg.procedureTurnPos.isValid())
          ellipseSize /= 2;

        // Draw start point of the leg using a smaller circle
        if(!context->drawFast && !transparent)
        {
          painter->setPen(QPen(outerColorProc, radius / 3 + 2));
          painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
        }
        painter->setPen(QPen(innerColorProc, transparent ? 0 : radius / 3));
        painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
      }

      ellipseSize = radius;
      if(!leg.line.isPoint())
      {
        if(wToS(leg.line.getPos2(), x, y))
        {
          // Draw end point of the leg using a larger circle
          if(!context->drawFast && !transparent)
          {
            painter->setPen(QPen(outerColorProc, radius / 3 + 2));
            painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
          }
          painter->setPen(QPen(innerColorProc, transparent ? 0 : radius / 3));
          painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
        }
      }

      if(leg.procedureTurnPos.isValid())
      {
        if(wToS(leg.procedureTurnPos, x, y))
        {
          // Draw turn position of the procedure turn
          if(!context->drawFast && !transparent)
          {
            painter->setPen(QPen(outerColorProc, radius / 3 + 2));
            painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
          }
          painter->setPen(QPen(innerColorProc, transparent ? 0 : radius / 3));
          painter->drawEllipse(QPointF(x, y), ellipseSize, ellipseSize);
        }
      }
    }
  }

  // Draw highlights from the flight plan view =====================================================
  if(mapLayer->isAirport())
    radius = std::max(radius, static_cast<float>(mapLayer->getAirportSymbolSize()));

  if(transparent)
    radius *= 0.75f;

  const QList<int>& routeHighlightResults = mapPaintWidget->getRouteHighlights();
  QVector<atools::geo::Pos> positions;
  for(int idx : routeHighlightResults)
  {
    const RouteLeg& routeLeg = NavApp::getRouteConst().value(idx);
    positions.append(routeLeg.getPosition());
  }

  QColor routeHighlightColor = OptionData::instance().getHighlightFlightplanColor();
  const QPen outerPenRoute(mapcolors::routeHighlightBackColor, radius / 3. + 2., Qt::SolidLine, Qt::FlatCap);
  const QPen innerPenRoute(mapcolors::adjustAlphaF(routeHighlightColor, alpha), transparent ? 1. : radius / 3., Qt::SolidLine, Qt::FlatCap);
  painter->setBrush(transparent ? QBrush(mapcolors::adjustAlphaF(routeHighlightColor, alpha)) : QBrush(Qt::NoBrush));
  painter->setPen(innerPenRoute);
  for(const ageo::Pos& pos : positions)
  {
    float x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast && !transparent)
      {
        painter->setPen(outerPenRoute);
        painter->drawEllipse(QPointF(x, y), radius, radius);
        painter->setPen(innerPenRoute);
      }
      painter->drawEllipse(QPointF(x, y), radius, radius);
    }
  }

  // Draw highlight from the elevation profile view =====================================================
  QColor profileHighlightColor = OptionData::instance().getHighlightProfileColor();
  const QPen outerPenProfile(mapcolors::profileHighlightBackColor, radius / 3. + 2., Qt::SolidLine, Qt::FlatCap);
  const QPen innerPenProfile(mapcolors::adjustAlphaF(profileHighlightColor, alpha), transparent ? 1. : radius / 3., Qt::SolidLine,
                             Qt::FlatCap);
  painter->setBrush(transparent ? QBrush(mapcolors::adjustAlphaF(profileHighlightColor, alpha)) : QBrush(Qt::NoBrush));
  const ageo::Pos& pos = mapPaintWidget->getProfileHighlight();
  painter->setPen(innerPenProfile);
  if(pos.isValid())
  {
    float x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast && !transparent)
      {
        painter->setPen(outerPenProfile);
        painter->drawEllipse(QPointF(x, y), radius, radius);
        painter->setPen(innerPenProfile);
      }
      painter->drawEllipse(QPointF(x, y), radius, radius);
    }
  }
}

void MapPainterMark::paintLogEntries(const QList<map::MapLogbookEntry>& entries)
{
  const static QMargins MARGINS(120, 10, 10, 10);

  GeoPainter *painter = context->painter;
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBackground(Qt::black);
  painter->setBrush(Qt::NoBrush);
  context->szFont(context->textSizeFlightplan);

  float minAltitude = std::numeric_limits<float>::max(), maxAltitude = std::numeric_limits<float>::min();
  // Collect visible feature parts ==========================================================================
  atools::fs::userdata::LogdataManager *logdataManager = NavApp::getLogdataManager();
  QVector<const MapLogbookEntry *> visibleLogEntries, allLogEntries;
  ageo::LineString visibleRouteGeometries;
  QStringList visibleRouteTexts;
  QVector<ageo::LineString> visibleTrailGeometries;
  bool showRouteAndTrail = entries.size() == 1;

  for(const MapLogbookEntry& logEntry : entries)
  {
    // All selected for airport drawing
    allLogEntries.append(&logEntry);

    // All which have visible geometry
    if(resolves(logEntry.bounding()))
      visibleLogEntries.append(&logEntry);

    // Show details only if one entry is selected - only direct connection for more than one selection
    if(showRouteAndTrail)
    {
      // Get cached data
      const atools::fs::gpx::GpxData *gpxData = logdataManager->getGpxData(logEntry.id);

      // Geometry might be null in case of cache overflow
      // Geometry has to be copied since cache in LogDataManager might remove it any time
      if(gpxData != nullptr)
      {
        // Flight plan =========================================================
        if(!gpxData->flightplan.isEmpty() && context->objectDisplayTypes.testFlag(map::LOGBOOK_ROUTE) && resolves(gpxData->flightplanRect))
        {
          for(const atools::fs::pln::FlightplanEntry& entry : gpxData->flightplan)
          {
            visibleRouteGeometries.append(entry.getPosition());
            visibleRouteTexts.append(entry.getIdent());
          }
        }

        // Trail =========================================================
        // Limit number of visible tracks
        if(!gpxData->trails.isEmpty() && context->objectDisplayTypes.testFlag(map::LOGBOOK_TRACK) && resolves(gpxData->trailRect))
        {
          maxAltitude = std::max(maxAltitude, gpxData->maxTrailAltitude);
          minAltitude = std::min(minAltitude, gpxData->minTrailAltitude);

          for(const atools::fs::gpx::TrailPoints& points : gpxData->trails)
          {
            if(!points.isEmpty())
            {
              ageo::LineString lineString;
              for(const atools::fs::gpx::TrailPoint& point : points)
                lineString.append(point.pos.asPos());

              if(resolves(lineString.boundingRect()))
                visibleTrailGeometries.append(lineString);
            }
          }
        }
      }
      break;
    }
  }

  // Draw route ==========================================================================
  if(context->objectDisplayTypes & map::LOGBOOK_ROUTE && !visibleRouteGeometries.isEmpty())
  {
    float outerlinewidth = context->szF(context->thicknessFlightplan, 7);
    float innerlinewidth = context->szF(context->thicknessFlightplan, 4);
    bool transparent = context->flags2.testFlag(opts2::MAP_ROUTE_TRANSPARENT);
    float alpha = transparent ? (1.f - context->transparencyFlightplan) : 1.f;
    float lineWidth = transparent ? outerlinewidth : innerlinewidth;
    float symbolSize = context->szF(context->thicknessFlightplan, 10);
    QColor routeLogEntryColor = mapcolors::adjustAlphaF(mapcolors::routeLogEntryColor, alpha);
    QColor routeLogEntryOutlineColor = mapcolors::adjustAlphaF(mapcolors::routeLogEntryOutlineColor, alpha);

    painter->setPen(QPen(routeLogEntryOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    // Draw outline for all selected entries ===============
    // Draw one route
    drawPolyline(painter, visibleRouteGeometries);

    // Draw line for all selected entries ===============
    // Use a lighter pen for the flight plan legs ======================================
    QPen routePen(routeLogEntryColor, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    routePen.setColor(routeLogEntryColor.lighter(130));
    painter->setPen(routePen);
    drawPolyline(painter, visibleRouteGeometries);

    if(!mapPaintWidget->isDistanceCutOff())
    {
      for(int i = 0; i < visibleRouteGeometries.size(); i++)
      {
        // Draw waypoint symbols and text for route preview =========
        const QString& name = visibleRouteTexts.at(i);
        float x, y;
        if(wToS(visibleRouteGeometries.at(i), x, y))
        {
          symbolPainter->drawLogbookPreviewSymbol(context->painter, x, y, symbolSize);

          if(context->mapLayer->isWaypointRouteName())
            symbolPainter->textBoxF(context->painter, {name}, routeLogEntryOutlineColor, x + symbolSize / 2 + 2, y, textatt::LOG_BG_COLOR);
        }
      }
    }

    if(!mapPaintWidget->isDistanceCutOff())
    {
      painter->setPen(QPen(routeLogEntryOutlineColor, (outerlinewidth - lineWidth) / 2., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter->setBrush(Qt::white);
      QPolygonF arrow = buildArrow(outerlinewidth);
      for(int i = 1; i < visibleRouteGeometries.size(); i++)
        // Draw waypoint symbols and text for route preview =========
        paintArrowAlongLine(painter, ageo::Line(visibleRouteGeometries.at(i),
                                                visibleRouteGeometries.at(i - 1)), arrow, 0.5f /* pos*/, 40.f /* minLengthPx */);
    }
  }

  // Draw track ==========================================================================
  if(!mapPaintWidget->isDistanceCutOff())
  {
    if(context->objectDisplayTypes.testFlag(map::LOGBOOK_TRACK) && !visibleTrailGeometries.isEmpty())
    {
      // Use a darker pen for the trail but same style as normal trail ======================================
      QPen trackPen = mapcolors::aircraftTrailPen(context->sz(context->thicknessTrail, 2));
      trackPen.setColor(mapcolors::routeLogEntryColor.darker(200));
      painter->setPen(trackPen);

      paintAircraftTrail(visibleTrailGeometries, minAltitude, maxAltitude);
    }
  }

  // Draw direct connection ==========================================================================
  if(context->objectDisplayTypes & map::LOGBOOK_DIRECT)
  {
    // Use smaller measurement line thickness for this direct connection
    float outerlinewidth = context->szF(context->thicknessUserFeature, 7) * 0.6f;
    float innerlinewidth = context->szF(context->thicknessUserFeature, 4) * 0.6f;
    QPen directPen(mapcolors::routeLogEntryColor, innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen directOutlinePen(mapcolors::routeLogEntryOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    float size = context->szF(context->symbolSizeAirport, context->mapLayer->getAirportSymbolSize());

    QVector<ageo::LineString> geo;
    for(const MapLogbookEntry *entry : visibleLogEntries)
      geo.append(entry->lineString());

    // Outline
    float circleSize = size;
    painter->setPen(directOutlinePen);
    for(const ageo::LineString& line : geo)
    {
      if(line.size() > 1)
        drawPolyline(painter, line);
      else
        drawCircle(painter, line.getPos1(), circleSize);
    }

    // Center line
    painter->setPen(directPen);
    for(const ageo::LineString& line : geo)
    {
      if(line.size() > 1)
        drawPolyline(painter, line);
      else
        drawCircle(painter, line.getPos1(), circleSize);
    }

    // Draw line text ==========================================================================
    if(!mapPaintWidget->isDistanceCutOff())
    {
      context->szFont(context->textSizeRangeUserFeature);
      painter->setBackground(mapcolors::routeTextBackgroundColor);
      painter->setPen(mapcolors::routeTextColor);
      for(const MapLogbookEntry *entry : visibleLogEntries)
      {
        // Text for one line
        const ageo::LineString positions = entry->lineString();

        TextPlacement textPlacement(context->painter, this, context->screenRect);
        textPlacement.setDrawFast(context->drawFast);
        textPlacement.setLineWidth(outerlinewidth);
        textPlacement.calculateTextPositions(positions);

        QStringList text;
        text.append(tr("%1 to %2").arg(entry->departureIdent).arg(entry->destinationIdent));
        // text.append(atools::elideTextShort(entry->aircraftType, 5));
        // text.append(atools::elideTextShort(entry->aircraftRegistration, 7));
        if(entry->distanceGcNm > 0.f)
          text.append(Unit::distNm(entry->distanceGcNm, true /* unit */, 20, true /* narrow */));
        text.removeAll(QString());

        if(positions.size() >= 2)
        {
          textPlacement.calculateTextAlongLines({positions.toLine()}, {text.join(tr(","))});
          textPlacement.drawTextAlongLines();
        }
      }
    }
  }

  // Draw airport symbols and text always ==========================================================================
  if(!mapPaintWidget->isDistanceCutOff())
  {
    float x, y;
    textflags::TextFlags flags = context->airportTextFlagsRoute(false /* draw as route */, true /* draw as log */);
    float size = context->szF(context->symbolSizeAirport, context->mapLayer->getAirportSymbolSize());
    context->szFont(context->textSizeFlightplan);

    QSet<int> airportIds;
    for(const MapLogbookEntry *entry : allLogEntries)
    {
      // Check if already drawn
      if(!airportIds.contains(entry->departure.id))
      {
        if(entry->departure.isValid() ?
           wToSBuf(entry->departure.position, x, y, MARGINS) : // Use valid airport position
           wToSBuf(entry->departurePos, x, y, MARGINS)) // Use recorded position
        {
          symbolPainter->drawAirportSymbol(context->painter, entry->departure, x, y, size, false, context->drawFast,
                                           context->flags2.testFlag(opts2::MAP_AIRPORT_HIGHLIGHT_ADDON));
          symbolPainter->drawAirportText(context->painter, entry->departure, x, y, context->dispOptsAirport, flags, size,
                                         context->mapLayer->isAirportDiagram(),
                                         context->mapLayer->getMaxTextLengthAirport());
        }
        airportIds.insert(entry->departure.id);
      }

      if(!airportIds.contains(entry->destination.id))
      {
        if(entry->destination.isValid() ?
           wToSBuf(entry->destination.position, x, y, MARGINS) :
           wToSBuf(entry->destinationPos, x, y, MARGINS))
        {
          symbolPainter->drawAirportSymbol(context->painter, entry->destination, x, y, size, false, context->drawFast,
                                           context->flags2.testFlag(opts2::MAP_AIRPORT_HIGHLIGHT_ADDON));
          symbolPainter->drawAirportText(context->painter, entry->destination, x, y, context->dispOptsAirport, flags, size,
                                         context->mapLayer->isAirportDiagram(),
                                         context->mapLayer->getMaxTextLengthAirport());
        }
        airportIds.insert(entry->destination.id);
      }
    }
  }
}

void MapPainterMark::paintAirwayList(const QList<map::MapAirway>& airwayList)
{
  Marble::GeoPainter *painter = context->painter;

  // Collect all waypoints from airway list ===========================
  ageo::LineString linestring;
  if(!airwayList.isEmpty())
    linestring.append(airwayList.constFirst().from);
  for(const map::MapAirway& airway : airwayList)
  {
    if(airway.isValid())
      linestring.append(airway.to);
  }

  // Outline =================
  float lineWidth = context->szF(context->thicknessUserFeature, 5.f);
  QPen outerPen(mapcolors::highlightBackColor, lineWidth, Qt::SolidLine, Qt::RoundCap);
  painter->setPen(outerPen);
  drawPolyline(painter, linestring);

  // Inner line ================
  QPen innerPen(mapcolors::airwayVictorColor, lineWidth);
  innerPen.setWidthF(lineWidth * 0.5);
  innerPen.setColor(innerPen.color().lighter(150));
  painter->setPen(innerPen);
  drawPolyline(painter, linestring);

  // Arrows ================
  QPolygonF arrow = buildArrow(lineWidth);
  context->painter->setPen(QPen(mapcolors::highlightBackColor, lineWidth / 3., Qt::SolidLine, Qt::RoundCap));
  context->painter->setBrush(Qt::white);
  for(const map::MapAirway& airway : airwayList)
  {
    if(airway.direction != map::DIR_BOTH)
    {
      ageo::Line arrLine = airway.direction != map::DIR_FORWARD ? ageo::Line(airway.from, airway.to) : ageo::Line(airway.to, airway.from);
      paintArrowAlongLine(context->painter, arrLine, arrow, 0.3f);
    }
  }

  // Draw waypoint triangles =============================================
  for(const ageo::Pos& pos : qAsConst(linestring))
  {
    QPointF pt = wToS(pos);
    if(!pt.isNull())
    {
      // Draw a triangle
      double radius = lineWidth * 0.8;
      QPolygonF polygon;
      polygon << QPointF(pt.x(), pt.y() - (radius * 1.2)) << QPointF(pt.x() + radius, pt.y() + radius)
              << QPointF(pt.x() - radius, pt.y() + radius);

      painter->drawConvexPolygon(polygon);
    }
  }
}

void MapPainterMark::paintAirwayTextList(const QList<map::MapAirway>& airwayList)
{
  context->szFont(context->textSizeRangeUserFeature);

  for(const map::MapAirway& airway : airwayList)
  {
    if(airway.isValid())
    {
      QPen innerPen = mapcolors::colorForAirwayOrTrack(airway);

      // Draw text  at center position of a line
      int x, y;
      ageo::Pos center = airway.bounding.getCenter();
      bool visible1, hidden1, visible2, hidden2;
      QPoint p1 = wToS(airway.from, DEFAULT_WTOS_SIZE, &visible1, &hidden1);
      QPoint p2 = wToS(airway.to, DEFAULT_WTOS_SIZE, &visible2, &hidden2);

      // Draw if not behind the globe and sufficient distance
      if((p1 - p2).manhattanLength() > 40)
      {
        if(wToS(center, x, y))
        {
          if(!hidden1 && !hidden2)
            symbolPainter->textBoxF(context->painter, {airway.name}, innerPen, x, y, textatt::CENTER);
        }
      }
    }
  }
}

void MapPainterMark::paintAirspace(const map::MapAirspace& airspace)
{
  Marble::GeoPainter *painter = context->painter;
  const OptionData& optionData = OptionData::instance();

  float lineWidth = context->szF(context->thicknessUserFeature, 5);

  QPen outerPen(mapcolors::highlightBackColor, lineWidth, Qt::SolidLine, Qt::FlatCap);

  // Make boundary pen the same color as airspace boundary without transparency
  QPen innerPen = mapcolors::penForAirspace(airspace, optionData.getDisplayThicknessAirspace());
  innerPen.setWidthF(lineWidth * 0.5);
  QColor c = innerPen.color();
  c.setAlpha(255);
  innerPen.setColor(c);

  painter->setBrush(mapcolors::colorForAirspaceFill(airspace, optionData.getDisplayTransparencyAirspace()));
  context->szFont(context->textSizeRangeUserFeature);

  if(context->viewportRect.overlaps(airspace.bounding))
  {
    if(context->objCount())
      return;

    const atools::geo::LineString *lineString = NavApp::getAirspaceController()->getAirspaceGeometry(airspace.combinedId());

    if(lineString != nullptr)
    {
      const QVector<QPolygonF *> polygons = createPolygons(*lineString, context->screenRect);

      if(!context->drawFast)
      {
        // Draw black background for outline
        painter->setPen(outerPen);
        for(const QPolygonF *polygon : polygons)
          painter->drawPolygon(*polygon);
        painter->setPen(innerPen);
      }

      for(const QPolygonF *polygon : polygons)
        painter->drawPolygon(*polygon);

      QPointF center = polygons.constFirst()->boundingRect().center();
      symbolPainter->textBoxF(painter, {map::airspaceNameMap(airspace, 20, true, true, true, true, true)}, innerPen,
                              static_cast<float>(center.x()), static_cast<float>(center.y()), textatt::CENTER);

      releasePolygons(polygons);
    }
  }
}

void MapPainterMark::paintRangeMarks()
{
  atools::util::PainterContextSaver saver(context->painter);
  const QList<map::RangeMarker>& rangeRings = mapPaintWidget->getRangeMarks().values();
  GeoPainter *painter = context->painter;

  context->szFont(context->textSizeRangeUserFeature);

  float lineWidth = context->szF(context->thicknessUserFeature, 3);

  for(const map::RangeMarker& rings : rangeRings)
  {
    // Get the biggest ring to check visibility
    auto maxRingIter = std::max_element(rings.ranges.constBegin(), rings.ranges.constEnd());

    if(maxRingIter != rings.ranges.end())
    {
      float maxRadiusNm = *maxRingIter;

      ageo::Rect rect(rings.position, ageo::nmToMeter(maxRadiusNm), true /* fast */);
      if(context->viewportRect.overlaps(rect) || maxRadiusNm > 2000.f)
      {
        // Ring is visible - the rest of the visibility check is done in paintCircle

        painter->setPen(QPen(QBrush(rings.color), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

        bool centerVisible;
        QPointF center = wToSF(rings.position, DEFAULT_WTOS_SIZE, &centerVisible);
        if(centerVisible)
        {
          // Draw small center point
          painter->setBrush(Qt::white);
          painter->drawEllipse(center, 4., 4.);
        }

        if(resolves(rect))
        {
          painter->setBrush(Qt::NoBrush);

          // Draw all rings
          for(float radius : rings.ranges)
          {
            QPoint textPos;
            paintCircle(painter, rings.position, radius, context->drawFast, &textPos);
            if(!textPos.isNull())
            {
              // paintCircle found a text position - draw text
              painter->setPen(mapcolors::rangeRingTextColor);

              QStringList texts;

              if(!rings.text.isEmpty())
                texts.append(rings.text);

              // Build narrow text manually
              if(radius > 0.f)
                texts.append(tr("%1%2").arg(QLocale(QLocale::C).toString(Unit::distNmF(radius), 'g', 6)).arg(Unit::getUnitDistStr()));

              textPos.ry() += painter->fontMetrics().height() / 2 - painter->fontMetrics().descent();
              symbolPainter->textBox(painter, texts, painter->pen(), textPos.x(), textPos.y(), textatt::CENTER);

              painter->setPen(QPen(QBrush(rings.color), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
            }
          }
        }
      }
    }
  }
}

void MapPainterMark::paintSelectedAltitudeRange()
{
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = mapPaintWidget->getUserAircraft();
  if(context->objectDisplayTypes & map::AIRCRAFT_SELECTED_ALT_RANGE && userAircraft.isFlying())
  {
    ageo::Pos pos = mapPaintWidget->getUserAircraft().getPosition();
    if(pos.isValid())
    {
      if( // Aircraft climbing and set altitude is above current
        (userAircraft.getIndicatedAltitudeFt() < userAircraft.getAltitudeAutopilotFt() &&
         userAircraft.getVerticalSpeedFeetPerMin() > 50.f) ||
        // Aircraft descending and set altitude is below current
        (userAircraft.getIndicatedAltitudeFt() > userAircraft.getAltitudeAutopilotFt() &&
         userAircraft.getVerticalSpeedFeetPerMin() < -50.f))
      {
        float minutesPerOneFt = std::abs(1.f / userAircraft.getVerticalSpeedFeetPerMin());
        float minutesForDiffAlt = std::abs(userAircraft.getAltitudeAutopilotFt() - userAircraft.getIndicatedAltitudeFt()) * minutesPerOneFt;
        float rangeDistanceNm = ageo::feetToNm(minutesForDiffAlt * std::abs(ageo::nmToFeet(userAircraft.getGroundSpeedKts()) / 60.f));

        Marble::GeoPainter *painter = context->painter;
        atools::util::PainterContextSaver saver(painter);
        if(rangeDistanceNm > 0.5f && rangeDistanceNm < 1000.f)
        {
          if(scale->getPixelForNm(rangeDistanceNm, userAircraft.getTrackDegTrue()) > 16)
          {
            float lineWidth = context->szF(context->thicknessUserFeature, mapcolors::markSelectedAltitudeRangePen.width());
            painter->setPen(mapcolors::adjustWidth(mapcolors::markSelectedAltitudeRangePen, lineWidth));
            painter->setBrush(Qt::NoBrush);
            context->szFont(context->textSizeRangeUserFeature);

            // Draw arc
            float arcAngleStart = ageo::normalizeCourse(userAircraft.getTrackDegTrue() - 45.f);
            float arcAngleEnd = ageo::normalizeCourse(userAircraft.getTrackDegTrue() + 45.f);
            paintArc(painter, pos, rangeDistanceNm, arcAngleStart, arcAngleEnd, context->drawFast);
          }
        }
      }
    }
  }
}

void MapPainterMark::paintEndurance()
{
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = mapPaintWidget->getUserAircraft();
  if(context->objectDisplayTypes & map::AIRCRAFT_ENDURANCE && userAircraft.isFlying())
  {
    ageo::Pos pos = mapPaintWidget->getUserAircraft().getPosition();
    if(pos.isValid())
    {
      // Get endurance
      float enduranceHours = 0.f, enduranceNm = 0.f;
      NavApp::getAircraftPerfController()->getEnduranceAverage(enduranceHours, enduranceNm);
      if(enduranceNm < map::INVALID_DISTANCE_VALUE)
      {
        Marble::GeoPainter *painter = context->painter;
        atools::util::PainterContextSaver saver(painter);
        bool labelAtCenter;
        QPoint textPos;
        bool visibleCenter = false;
        if(enduranceNm > 1.f)
        {
          float lineWidth = context->szF(context->thicknessUserFeature, mapcolors::markEndurancePen.width());
          painter->setPen(mapcolors::adjustWidth(mapcolors::markEndurancePen, lineWidth));
          painter->setBrush(Qt::NoBrush);
          context->szFont(context->textSizeRangeUserFeature);

          // Draw circle and get a text placement position
          paintCircle(painter, pos, enduranceNm, context->drawFast, &textPos);
          visibleCenter = true;
          labelAtCenter = false;
        }
        else
        {
          visibleCenter = wToS(pos, textPos.rx(), textPos.ry());
          labelAtCenter = true;
        }

        if(visibleCenter && !textPos.isNull())
        {
          // paintCircle found a text position - draw text
          painter->setPen(mapcolors::rangeRingTextColor);

          textatt::TextAttributes atts = textatt::CENTER;

          const Route& route = NavApp::getRouteConst();
          if(route.getSizeWithoutAlternates() <= 1 || route.getActiveLegIndexCorrected() == map::INVALID_INDEX_VALUE)
          {
            // Show error colors only for free flight
            if(enduranceHours < 0.5f)
              atts |= textatt::ERROR_COLOR;
            else if(enduranceHours < 0.75f)
              atts |= textatt::WARNING_COLOR;
          }
          else if(enduranceHours < 0.1f)
            // Show error color even with flight plan if fuel gets really low
            atts |= textatt::ERROR_COLOR;

          QStringList texts;
          // if(!userAircraft.getAirplaneRegistration().isEmpty())
          // texts.append(atools::elideTextShort(userAircraft.getAirplaneRegistration(), 15));
          // else if(!userAircraft.getAirplaneTitle().isEmpty())
          // texts.append(atools::elideTextShort(userAircraft.getAirplaneTitle(), 15));
          // if(!userAircraft.getAirplaneModel().isEmpty())
          // texts.append(userAircraft.getAirplaneModel());

          texts.append(Unit::distNm(enduranceNm, true, 5, true));

          if(enduranceHours < map::INVALID_TIME_VALUE)
            texts.append(formatter::formatMinutesHoursLong(enduranceHours));

          if(labelAtCenter)
          {
            int size = std::max(context->sz(context->symbolSizeAircraftUser, 32), scale->getPixelIntForFeet(userAircraft.getModelSize()));
            textPos.ry() -= size * 2;
          }
          else
            textPos.ry() += painter->fontMetrics().height() / 2 - painter->fontMetrics().descent();

          symbolPainter->textBox(painter, texts, painter->pen(), textPos.x(), textPos.y(), atts);
        }
      }
    }
  }
}

void MapPainterMark::paintCompassRose()
{
  if(context->objectDisplayTypes & map::COMPASS_ROSE && mapPaintWidget->distance() < MIN_VIEW_DISTANCE_COMPASS_ROSE_KM)
  {
    atools::util::PainterContextSaver saver(context->painter);
    const OptionData& od = OptionData::instance();

    Marble::GeoPainter *painter = context->painter;
    const atools::fs::sc::SimConnectUserAircraft& aircraft = mapPaintWidget->getUserAircraft();
    ageo::Pos pos = aircraft.getPosition();

    // Use either aircraft position or viewport center
    QRect viewport = painter->viewport();
    bool hasAircraft = pos.isValid() && context->objectDisplayTypes & map::COMPASS_ROSE_ATTACH;
    if(!hasAircraft)
      pos = sToW(viewport.center());

    // Get vertical and horizontal dimensions and calculate size
    ageo::Line horiz(context->viewportRect.getLeftCenter(), context->viewportRect.getRightCenter());
    ageo::Line vert(context->viewportRect.getTopCenter(), context->viewportRect.getBottomCenter());

    float h = horiz.lengthMeter();
    float v = vert.lengthMeter();
    if(h < 1000.f)
      h = v;

    float radiusMeter = std::min(h, v) / 2.f * 0.8f;

    radiusMeter = std::min(radiusMeter, ageo::nmToMeter(MAX_COMPASS_ROSE_RADIUS_NM));
    radiusMeter = std::max(radiusMeter, ageo::nmToMeter(MIN_COMPASS_ROSE_RADIUS_NM));

    float radiusNm = ageo::meterToNm(radiusMeter);

    painter->setBrush(Qt::NoBrush);
    float lineWidth = context->szF(context->thicknessCompassRose, 2);
    QPen rosePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen rosePenSmall(QBrush(mapcolors::compassRoseColor), lineWidth / 4.f, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen headingLinePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::DotLine, Qt::RoundCap, Qt::MiterJoin);
    painter->setPen(rosePen);

    // Draw outer big circle ==================================================
    paintCircle(context->painter, pos, ageo::meterToNm(radiusMeter), context->drawFast, nullptr);

    // Draw small center circle if no aircraft ===================================
    QPointF centerPoint = wToSF(pos);
    if(!centerPoint.isNull() && !hasAircraft)
      painter->drawEllipse(centerPoint, 5, 5);

    // Collect points for tick marks and labels
    float magVar = 0.f;
    if(!context->dOptRose(optsd::ROSE_TRUE_HEADING))
      magVar = hasAircraft ? aircraft.getMagVarDeg() : NavApp::getMagVar(pos);

    QVector<ageo::Pos> endpoints;
    QVector<QPointF> endpointsScreen;
    for(float angle = 0.f; angle < 360.f; angle += 5)
    {
      endpoints.append(pos.endpoint(radiusMeter, magVar + angle));
      endpointsScreen.append(wToSF(endpoints.constLast()));
    }

    // Draw tick marks ======================================================
    if(context->dOptRose(optsd::ROSE_DEGREE_MARKS))
    {
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (90 / 5)) == 0) // 90 degree ticks
        {
          drawLineStraight(painter, ageo::Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.8f), endpoints.at(i)));
        }
        else if((i % (45 / 5)) == 0) // 45 degree ticks
          drawLineStraight(painter, ageo::Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.84f), endpoints.at(i)));
        else if((i % (10 / 5)) == 0) // 10 degree ticks
        {
          if(mapPaintWidget->distance() < 3200 /* km */)
            drawLineStraight(painter, ageo::Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.92f), endpoints.at(i)));
        }
        else if(mapPaintWidget->distance() < 6400 /* km */) // 5 degree ticks
          drawLineStraight(painter, ageo::Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.95f), endpoints.at(i)));
      }
    }

    // Calculate and draw triangle for true north ======================================================
    painter->setBrush(QBrush(Qt::white));
    ageo::Pos trueNorth = pos.endpoint(radiusMeter, 0);
    QPointF trueNorthPoint = wToSF(trueNorth);

    if(!trueNorthPoint.isNull())
      painter->drawPolygon(QPolygonF({trueNorthPoint, trueNorthPoint - QPointF(10, 20), trueNorthPoint - QPointF(-10, 20)}));

    // Aircraft track and heading line ======================================================
    // Convert to selected display unit
    float stepsizeUnit = atools::calculateSteps(Unit::distNmF(radiusNm), 6.5f);
    float stepsizeNm = Unit::rev(stepsizeUnit, Unit::distNmF);
    painter->setPen(rosePenSmall);

    // Draw distance circles =======================================================
    painter->setBrush(Qt::NoBrush);
    if(context->dOptRose(optsd::ROSE_RANGE_RINGS))
    {
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
        paintCircle(context->painter, pos, i * stepsizeNm, context->drawFast, nullptr);
    }
    painter->setPen(rosePen);

    if(hasAircraft)
    {
      // Solid track line
      painter->setPen(rosePen);
      if(context->dOptRose(optsd::ROSE_TRACK_LINE))
      {
        float trackTrue = aircraft.getTrackDegTrue();
        ageo::Pos trueTrackPos = pos.endpoint(radiusMeter, trackTrue);
        drawLine(painter, ageo::Line(pos, trueTrackPos));
      }

      // Dotted heading line
      if(context->dOptRose(optsd::ROSE_HEADING_LINE))
      {
        float headingTrue = aircraft.getHeadingDegTrue();
        ageo::Pos trueHeadingPos = pos.endpoint(radiusMeter, headingTrue);
        painter->setPen(headingLinePen);
        drawLine(painter, ageo::Line(pos, trueHeadingPos));
      }
      painter->setPen(rosePen);
    }

    // Draw labels for four directions ======================================================
    context->szFont(context->textSizeCompassRose * 1.4f);
    painter->setPen(mapcolors::compassRoseTextColor);

    if(context->dOptRose(optsd::ROSE_DIR_LABELS))
    {
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (90 / 5)) == 0)
        {
          if(mapPaintWidget->distance() < 6400 /* km */)
          {
            if(!endpointsScreen.at(i).isNull())
            {
              QString text;
              if(i == 0)
                text = tr("N", "North");
              else if(i == 90 / 5)
                text = tr("E", "East");
              else if(i == 180 / 5)
                text = tr("S", "South");
              else if(i == 270 / 5)
                text = tr("W", "West");

              symbolPainter->textBoxF(painter, {text}, painter->pen(),
                                      static_cast<float>(endpointsScreen.at(i).x()),
                                      static_cast<float>(endpointsScreen.at(i).y()), textatt::CENTER);
            }
          }
        }
      }
    }

    // Draw small 15 deg labels ======================================================
    if(mapPaintWidget->distance() < 1600 /* km */ && context->dOptRose(optsd::ROSE_DEGREE_LABELS))
    {
      // Reduce font size
      context->szFont(context->textSizeCompassRose * 0.8f);
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (15 / 5)) == 0 && (!context->dOptRose(optsd::ROSE_DIR_LABELS) || !((i % (90 / 5)) == 0)))
        {
          // All 15 deg but not at 90 deg boundaries
          symbolPainter->textBoxF(painter, {QString::number(i * 5)}, painter->pen(),
                                  static_cast<float>(endpointsScreen.at(i).x()),
                                  static_cast<float>(endpointsScreen.at(i).y()), textatt::CENTER);
        }
      }
    }

    // Aircraft track line end text and distance labels along track line ======================================================
    float trackTrue = 0.f;
    if(hasAircraft)
      // Solid track line
      trackTrue = aircraft.getTrackDegTrue();

    // Distance labels along track line
    context->szFont(context->textSizeCompassRose * 0.8f);
    if(context->dOptRose(optsd::ROSE_RANGE_RINGS))
    {
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
      {
        QPointF s = wToSF(pos.endpoint(ageo::nmToMeter(i * stepsizeNm), trackTrue));
        if(!s.isNull())
        {
          float dist = i * stepsizeNm;
          QString text = tr("%1%2").arg(QLocale(QLocale::C).toString(Unit::distNmF(dist), 'g', 6)).arg(Unit::getUnitDistStr());
          symbolPainter->textBoxF(painter, {text}, painter->pen(), static_cast<float>(s.x()), static_cast<float>(s.y()), textatt::CENTER);
        }
      }
    }

    if(hasAircraft)
    {
      const Route& route = NavApp::getRouteConst();

      if(route.getSizeWithoutAlternates() > 1 && aircraft.isFlying())
      {
        bool isCorrected = false;
        int activeLegCorrected = route.getActiveLegIndexCorrected(&isCorrected);
        if(activeLegCorrected != map::INVALID_INDEX_VALUE)
        {
          // Draw crab angle if flight plan is available ========================

          // If approaching an initial fix use corrected version
          int activeLeg = route.getActiveLegIndex();
          const RouteLeg& routeLeg = activeLeg != map::INVALID_INDEX_VALUE && isCorrected ?
                                     route.value(activeLeg) : route.value(activeLegCorrected);

          float courseToWptTrue = map::INVALID_COURSE_VALUE;
          if((routeLeg.isRoute() || !routeLeg.getProcedureLeg().isCircular()) && routeLeg.getPosition().isValid())
            courseToWptTrue = aircraft.getPosition().angleDegTo(routeLeg.getPosition());

          if(context->dOptRose(optsd::ROSE_CRAB_ANGLE) && courseToWptTrue < map::INVALID_COURSE_VALUE)
          {
            // Crab angle is the amount of correction an aircraft must be turned into the wind in order to maintain the desired course.
            float crabAngle = ageo::windCorrectedHeading(aircraft.getWindSpeedKts(), aircraft.getWindDirectionDegT(),
                                                         courseToWptTrue, aircraft.getTrueAirspeedKts());

            ageo::Pos crabPos = pos.endpoint(radiusMeter, crabAngle);
            painter->setPen(rosePen);
            painter->setBrush(OptionData::instance().getFlightplanActiveSegmentColor());

            QPointF crabScreenPos = wToSF(crabPos);
            painter->drawEllipse(crabScreenPos, lineWidth * 3, lineWidth * 3);
          }

          if(context->dOptRose(optsd::ROSE_NEXT_WAYPOINT) && courseToWptTrue < map::INVALID_COURSE_VALUE)
          {
            // Draw small line to show course to next waypoint ========================
            ageo::Pos endPt = pos.endpoint(radiusMeter, courseToWptTrue);
            ageo::Line crsLine(pos.interpolate(endPt, radiusMeter, 0.92f), endPt);
            painter->setPen(QPen(od.getFlightplanOutlineColor(), context->sz(context->thicknessFlightplan, 7),
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawLineStraight(painter, crsLine);

            painter->setPen(QPen(OptionData::instance().getFlightplanActiveSegmentColor(), context->sz(context->thicknessFlightplan, 4),
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawLineStraight(painter, crsLine);
          }
        }
      }

      // Aircraft track label at end of track line ======================================================
      if(context->dOptRose(optsd::ROSE_TRACK_LABEL))
      {
        QPointF trueTrackTextPoint = wToSF(pos.endpoint(radiusMeter * 1.1f, trackTrue));
        if(!trueTrackTextPoint.isNull())
        {
          painter->setPen(mapcolors::compassRoseTextColor);
          context->szFont(context->textSizeCompassRose);

          QString text;
          if(context->dOptRose(optsd::ROSE_TRUE_HEADING))
            text = tr("%1°T").arg(QString::number(atools::roundToInt(aircraft.getTrackDegTrue())));
          else
            text = tr("%1°M").arg(QString::number(atools::roundToInt(aircraft.getTrackDegMag())));

          symbolPainter->textBoxF(painter, {text, tr("TRK")}, painter->pen(), static_cast<float>(trueTrackTextPoint.x()),
                                  static_cast<float>(trueTrackTextPoint.y()), textatt::CENTER | textatt::ROUTE_BG_COLOR);
        }
      }
    }
  }
}

QStringList MapPainterMark::distanceMarkText(const map::DistanceMarker& marker, bool drawFast) const
{
  float initTrue = marker.from.initialBearing(marker.to);
  float finalTrue = marker.from.finalBearing(marker.to);
  float distanceMeter = marker.getDistanceMeter();

  QString finalMagText;
  if(marker.flags.testFlag(map::DIST_MARK_MAGVAR))
    finalMagText = QString::number(ageo::normalizeCourse(finalTrue - marker.magvar), 'f', DISTMARK_DEG_PRECISION);
  else
    finalMagText = QString::number(ageo::normalizeCourse(finalTrue -
                                                         NavApp::getMagVar(marker.to, marker.magvar)), 'f', DISTMARK_DEG_PRECISION);

  QString textStr, radialStr, distStr, magTrueStr, trueStr;
  // Center labels ==============================================================
  // Text label - VOR, airport, etc. ==========================================================
  bool textLabel = context->dOptMeasurement(optsd::MEASUREMENT_LABEL) && !marker.text.isEmpty();
  if(textLabel)
    textStr = marker.text;

  // Draw radial number always after label of separate in next line ====================================================
  if(context->dOptMeasurement(optsd::MEASUREMENT_RADIAL) && marker.flags.testFlag(map::DIST_MARK_RADIAL))
    radialStr = proc::radialText(ageo::normalizeCourse(initTrue - marker.magvar));

  // Distance ==========================================================
  if(context->dOptMeasurement(optsd::MEASUREMENT_DIST) && distanceMeter < INVALID_DISTANCE_VALUE)
    distStr = Unit::distLongShortMeter(distanceMeter, tr("/"), true /* addUnit */, true /* narrow */);

  // Mag (/ true) Course on separate line ==========================================================
  QString initMagText = QString::number(ageo::normalizeCourse(initTrue - marker.magvar), 'f', DISTMARK_DEG_PRECISION);
  QString initTrueText = QString::number(initTrue, 'f', DISTMARK_DEG_PRECISION);
  QString finalTrueText = QString::number(finalTrue, 'f', DISTMARK_DEG_PRECISION);
  if(context->dOptMeasurement(optsd::MEASUREMENT_TRUE) && context->dOptMeasurement(optsd::MEASUREMENT_MAG) &&
     initTrueText == initMagText && finalTrueText == finalMagText)
  {
    // drawFast: Avoid flickering course while dragging line
    if(drawFast || initTrueText == finalTrueText)
      magTrueStr = initTrueText % magTrueSuffix;
    else
      // Use backslash for separation to allow text placement to swap on direction
      magTrueStr = initTrueText % magTrueSuffix % '\\' % finalTrueText % magTrueSuffix;
  }
  else
  {
    if(context->dOptMeasurement(optsd::MEASUREMENT_MAG))
    {
      if(drawFast || initMagText == finalMagText)
        magTrueStr = initMagText % magSuffix;
      else
        // Use backslash for separation to allow text placement to swap on direction
        magTrueStr = initMagText % magSuffix % '\\' % finalMagText % magSuffix;
    }
    if(context->dOptMeasurement(optsd::MEASUREMENT_TRUE))
    {
      if(drawFast || initTrueText == finalTrueText)
        trueStr = initTrueText % trueSuffix;
      else
        // Use backslash for separation to allow text placement to swap on direction
        trueStr = initTrueText % trueSuffix % '\\' % finalTrueText % trueSuffix;
    }
  }

  // Build text =========================================
  QStringList texts;
  texts.append(magTrueStr);

  if(magTrueStr.isEmpty() || trueStr.isEmpty())
    // Two lines so far - add label and radial in one line and distance in another line
    texts << atools::strJoin({textStr, radialStr}, tr(" / ")) << distStr;
  else
    // Label, radial and distance in one line between the course labels
    texts << atools::strJoin({textStr, radialStr, distStr}, tr(" / "));

  texts.append(trueStr);
  texts.removeAll(QString());

#ifdef DEBUG_INFORMATION_MEASUREMENT
  texts.append("[" + QString::number(distanceMeter, 'f', 0) + " m]");
  QPointF p1 = wToSF(marker.from);
  QPointF p2 = wToSF(marker.to);
  QLineF linef(p1, p2);
  texts.append("[" + QString::number(linef.length(), 'f', 0) + " px]");
#endif
  return texts;
}

void MapPainterMark::paintDistanceMarks()
{
  atools::util::PainterContextSaver saver(context->painter);
  GeoPainter *painter = context->painter;
  context->szFont(context->textSizeRangeMeasurement);

  const QList<map::DistanceMarker>& distanceMarkers = mapPaintWidget->getDistanceMarks().values();

  // Sort markers into a list of pointers where the last one is the one currently edited and drawn on top
  QList<const map::DistanceMarker *> markers;
  for(const map::DistanceMarker& marker : distanceMarkers)
  {
    if(marker.id == context->currentDistanceMarkerId)
      markers.append(&marker);
    else
      markers.prepend(&marker);
  }

  float lineWidth = context->szF(context->thicknessMeasurement, 3);
  const QColor measurementColor = OptionData::instance().getMeasurementColor();
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setBackground(mapcolors::distanceMarkerTextBackgroundColor);

  for(const map::DistanceMarker *marker : markers)
  {
    // Get color from marker or default
    QColor color = marker->color.isValid() ? marker->color : measurementColor;
    painter->setPen(QPen(color, lineWidth * 0.5, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));

    float symbolSize = lineWidth * 1.7f;
    float x, y;
    // Draw ellipse at start point ==================
    if(wToS(marker->from, x, y))
    {
      painter->setBrush(Qt::white);
      painter->drawEllipse(QPointF(x, y), symbolSize, symbolSize);
    }

    // Draw cross at end point =======================
    if(wToS(marker->to, x, y))
    {
      painter->drawLine(QPointF(x - symbolSize, y), QPointF(x + symbolSize, y));
      painter->drawLine(QPointF(x, y - symbolSize), QPointF(x, y + symbolSize));
    }

    if(resolves(ageo::Line(marker->from, marker->to)))
    {
      // Draw great circle line ========================================================
      painter->setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
      drawLine(painter, ageo::Line(marker->from, marker->to));

      // Build '\n' separated texts =============================
      QStringList texts =
        distanceMarkText(*marker, marker->id == context->currentDistanceMarkerId && context->viewContext == Marble::Animation);

      if(marker->from != marker->to)
      {
        painter->setPen(mapcolors::distanceMarkerTextColor);
        TextPlacement textPlacement(context->painter, this, context->screenRect);
        textPlacement.setArrowForEmpty(true);
        textPlacement.setMinLengthForText(painter->fontMetrics().averageCharWidth() * 2);
        textPlacement.setDrawFast(context->drawFast);
        textPlacement.setLineWidth(lineWidth);
        textPlacement.setTextOnLineCenter(true);
        textPlacement.calculateTextAlongLine(ageo::Line(marker->from, marker->to), texts.join('\n'));
        textPlacement.drawTextAlongLines();
      }
    }
  } // for(const map::DistanceMarker& marker : distanceMarkers)
}

void MapPainterMark::paintPatternMarks()
{
  atools::util::PainterContextSaver saver(context->painter);
  GeoPainter *painter = context->painter;
  const QList<PatternMarker>& patterns = mapPaintWidget->getPatternsMarks().values();
  float lineWidth = context->szF(context->thicknessUserFeature, 3);
  context->szFont(context->textSizeRangeUserFeature);

  TextPlacement textPlacement(painter, this, context->screenRect);
  textPlacement.setLineWidth(lineWidth);
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setBackground(Qt::white);

  QPolygonF arrow = buildArrow(lineWidth * 2.3f);

  // . B---------Downwind---------------C
  // . a                                r
  // . s                                o
  // . e--Final--==Runway==--Departure--s
  for(const PatternMarker& pattern : patterns)
  {
    bool visibleOrigin, hiddenOrigin;
    QPointF originPoint = wToS(pattern.getPosition(), DEFAULT_WTOS_SIZE, &visibleOrigin, &hiddenOrigin);
    if(hiddenOrigin)
      continue;

    // All distances in meter
    float runwayLength = ageo::feetToMeter(pattern.runwayLength);
    float parallelDist = ageo::nmToMeter(pattern.downwindParallelDistance);
    float departureDist = pattern.base45Degree ? ageo::nmToMeter(pattern.downwindParallelDistance) + runwayLength :
                          ageo::nmToMeter(pattern.departureDistance);
    float finalDist = ageo::nmToMeter(pattern.base45Degree ? pattern.downwindParallelDistance : pattern.finalDistance);

    if(context->mapLayer->isApproach() && scale->getPixelForNm(finalDist) > 5.f)
    {
      // Turn point base to final - extend from runway
      ageo::Pos baseToFinal = pattern.position.endpoint(finalDist, ageo::opposedCourseDeg(pattern.courseTrue));

      // Turn point downwind to base
      ageo::Pos downwindToBase = baseToFinal.endpoint(parallelDist, pattern.courseTrue + (pattern.turnRight ? 90.f : -90.f));

      // Turn point departure to crosswind
      ageo::Pos departureToCrosswind = pattern.position.endpoint(departureDist, pattern.courseTrue);

      // Turn point crosswind to downwind
      ageo::Pos crosswindToDownwind = departureToCrosswind.endpoint(parallelDist, pattern.courseTrue + (pattern.turnRight ? 90.f : -90.f));

      // Calculate bounding rectangle and check if it is at least partially visible
      ageo::Rect rect(baseToFinal);
      rect.extend(downwindToBase);
      rect.extend(departureToCrosswind);
      rect.extend(crosswindToDownwind);

      // Expand rect by approximately 2 NM
      rect.inflateMeter(ageo::nmToMeter(2.f), ageo::nmToMeter(2.f));

      if(resolves(rect))
      {
        // Entry at opposite runway threshold
        ageo::Pos downwindEntry = downwindToBase.endpoint(finalDist + runwayLength, pattern.courseTrue);

        bool visible, hidden;
        // Bail out if any points are hidden behind the globe
        QPointF baseFinalPoint = wToS(baseToFinal, DEFAULT_WTOS_SIZE, &visible, &hidden);
        if(hidden)
          continue;
        QPointF downwindBasePoint = wToS(downwindToBase, DEFAULT_WTOS_SIZE, &visible, &hidden);
        if(hidden)
          continue;
        QPointF upwindCrosswindPoint = wToS(departureToCrosswind, DEFAULT_WTOS_SIZE, &visible, &hidden);
        if(hidden)
          continue;
        QPointF crosswindDownwindPoint = wToS(crosswindToDownwind, DEFAULT_WTOS_SIZE, &visible, &hidden);
        if(hidden)
          continue;
        QPointF downwindEntryPoint = wToS(downwindEntry, DEFAULT_WTOS_SIZE, &visible, &hidden);
        if(hidden)
          continue;
        bool drawDetails = QLineF(baseFinalPoint, crosswindDownwindPoint).length() > 50.;

        // Calculate polygon rounding in pixels =======================
        float pixelForNm = scale->getPixelForNm(pattern.downwindParallelDistance, pattern.courseTrue + 90.f);
        atools::util::RoundedPolygon polygon(pixelForNm / 3.f, {originPoint, upwindCrosswindPoint, crosswindDownwindPoint,
                                                                downwindBasePoint, baseFinalPoint});

        QLineF downwind(crosswindDownwindPoint, downwindBasePoint);
        QLineF upwind(originPoint, upwindCrosswindPoint);
        float angle = static_cast<float>(ageo::angleFromQt(downwind.angle()));
        float oppositeAngle = static_cast<float>(ageo::opposedCourseDeg(ageo::angleFromQt(downwind.angle())));

        if(pattern.showEntryExit && context->mapLayer->isApproachText())
        {
          // Draw a line below to fill the gap because of round edges
          painter->setBrush(Qt::white);
          painter->setPen(QPen(pattern.color, context->szF(context->thicknessUserFeature, 3), Qt::DashLine));
          drawLine(painter, upwind);

          // Straight out exit for pattern =======================
          QPointF exitStraight =
            wToS(departureToCrosswind.endpoint(ageo::nmToMeter(1.f), oppositeAngle), DEFAULT_WTOS_SIZE, &visible, &hidden);
          drawLine(painter, upwind.p2(), exitStraight);

          // 45 degree exit for pattern =======================
          QPointF exit45Deg = wToS(departureToCrosswind.endpoint(ageo::nmToMeter(1.f), oppositeAngle + (pattern.turnRight ? 45.f : -45.f)),
                                   DEFAULT_WTOS_SIZE, &visible, &hidden);
          drawLine(painter, upwind.p2(), exit45Deg);

          // Entry to downwind
          QPointF entry = wToS(downwindEntry.endpoint(ageo::nmToMeter(1.f), oppositeAngle + (pattern.turnRight ? 45.f : -45.f)),
                               DEFAULT_WTOS_SIZE, &visible, &hidden);
          drawLine(painter, downwindEntryPoint, entry);

          if(drawDetails)
          {
            // Draw arrows to all the entry and exit indicators ========================
            painter->setPen(QPen(pattern.color, context->szF(context->thicknessUserFeature, 2), Qt::SolidLine));
            paintArrowAlongLine(painter, QLineF(upwind.p2(), exitStraight), arrow, 0.95f);
            paintArrowAlongLine(painter, QLineF(upwind.p2(), exit45Deg), arrow, 0.95f);
            paintArrowAlongLine(painter, QLineF(entry, downwindEntryPoint), arrow, 0.05f);
          }
        }

        painter->setPen(QPen(pattern.color, lineWidth));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(polygon.getPainterPath());

        if(drawDetails && context->mapLayer->isApproachText())
        {
          // Text for downwind leg =======================================
          QLineF final (baseFinalPoint, originPoint);
          QPointF center = downwind.center();
          QString text = tr("%1/%2").
                         arg(Unit::altFeet(pattern.position.getAltitude(), true /* addUnit */, true /* narrow */, 10.f /* round */)).
                         arg(formatter::courseTextFromTrue(ageo::opposedCourseDeg(pattern.courseTrue), pattern.magvar,
                                                           false /* magBold */, false /* magBig */,
                                                           false /* trueSmall */, true /* narrow */));

          painter->setBrush(Qt::white);
          textPlacement.drawTextAlongOneLine(text, angle, center, atools::roundToInt(downwind.length()));

          // Text for final leg =======================================
          text = tr("RW%1/%2").
                 arg(pattern.runwayName).
                 arg(formatter::courseTextFromTrue(pattern.courseTrue, pattern.magvar, false /* magBold */, false /* magBig */,
                                                   false /* trueSmall */, true /* narrow */));
          textPlacement.drawTextAlongOneLine(text, oppositeAngle, final.pointAt(0.60), atools::roundToInt(final.length()));

          // Draw arrows on legs =======================================
          // Set a lighter fill color for arrows

          painter->setBrush(pattern.color.lighter(300));
          painter->setPen(QPen(pattern.color, painter->pen().widthF() * 0.66));

          // Two arrows on downwind leg
          paintArrowAlongLine(painter, downwind, arrow, 0.75f);
          paintArrowAlongLine(painter, downwind, arrow, 0.25f);

          // Base leg
          paintArrowAlongLine(painter, QLineF(downwindBasePoint, baseFinalPoint), arrow);

          // Final leg
          paintArrowAlongLine(painter, final, arrow, 0.30f);

          // Upwind leg
          paintArrowAlongLine(painter, upwind, arrow);

          // Crosswind leg
          paintArrowAlongLine(painter, QLineF(upwindCrosswindPoint, crosswindDownwindPoint), arrow);
        }
      }
    }

    if(visibleOrigin)
    {
      // Draw ellipse at touchdown point - independent of zoom factor
      painter->setPen(QPen(pattern.color, lineWidth));
      painter->setBrush(Qt::white);
      painter->drawEllipse(originPoint, lineWidth * 2.f, lineWidth * 2.f);
    }
  }
}

void MapPainterMark::paintUserpointDrag()
{
  // Get screen position an pixmap
  QPoint cur;
  QPixmap pixmap;
  MapWidget *mapWidget = dynamic_cast<MapWidget *>(mapPaintWidget);
  if(mapWidget != nullptr)
    mapWidget->getUserpointDragPoints(cur, pixmap);

  if(!cur.isNull() && mapPaintWidget->rect().contains(cur) && !pixmap.isNull())
    context->painter->drawPixmap(cur.x() - pixmap.width() / 2, cur.y() - pixmap.height() / 2, pixmap);
}

/* Draw route dragging/moving lines */
void MapPainterMark::paintRouteDrag()
{
  ageo::LineString fixed;
  QPoint cur;

  MapWidget *mapWidget = dynamic_cast<MapWidget *>(mapPaintWidget);
  if(mapWidget != nullptr)
    mapWidget->getRouteDragPoints(fixed, cur);

  if(!cur.isNull())
  {
    ageo::Pos curGeo;
    if(sToW(cur.x(), cur.y(), curGeo))
    {
      context->painter->setPen(QPen(mapcolors::mapDragColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      // Draw lines from current mouse position to all fixed points which can be waypoints or several alternates
      for(const ageo::Pos& pos : qAsConst(fixed))
        drawLine(context->painter, ageo::Line(curGeo, pos));
    }
  }
}
