/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "navapp.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "common/formatter.h"
#include "query/airspacequery.h"
#include "geo/calculations.h"
#include "util/roundedpolygon.h"
#include "common/symbolpainter.h"
#include "geo/rect.h"
#include "atools.h"
#include "common/constants.h"
#include "common/unit.h"
#include "route/routeleg.h"
#include "route/route.h"
#include "route/routecontroller.h"
#include "util/paintercontextsaver.h"
#include "common/textplacement.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/GeoPainter.h>

const float MAX_COMPASS_ROSE_RADIUS_NM = 500.f;
const float MIN_COMPASS_ROSE_RADIUS_NM = 2.f;
const double MIN_VIEW_DISTANCE_COMPASS_ROSE_KM = 6400.;

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterMark::MapPainterMark(MapPaintWidget *mapWidgetParam, MapScale *mapScale)
  : MapPainter(mapWidgetParam, mapScale)
{
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::render(PaintContext *context)
{
  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver);

  paintHighlights(context);
  paintMark(context);
  paintHome(context);
  paintTrafficPatterns(context);
  paintRangeRings(context);
  paintDistanceMarkers(context);
  paintCompassRose(context);

  paintRouteDrag(context);
  paintUserpointDrag(context);
}

/* Draw black yellow cross for search distance marker */
void MapPainterMark::paintMark(const PaintContext *context)
{
  int x, y;
  if(wToS(mapPaintWidget->getSearchMarkPos(), x, y))
  {
    int size = context->sz(context->symbolSizeAirport, 10);
    int size2 = context->sz(context->symbolSizeAirport, 8);

    context->painter->setPen(mapcolors::markBackPen);

    context->painter->drawLine(x, y - size, x, y + size);
    context->painter->drawLine(x - size, y, x + size, y);

    context->painter->setPen(mapcolors::markFillPen);
    context->painter->drawLine(x, y - size2, x, y + size2);
    context->painter->drawLine(x - size2, y, x + size2, y);
  }
}

/* Paint the center of the home position */
void MapPainterMark::paintHome(const PaintContext *context)
{
  GeoPainter *painter = context->painter;

  int x, y;
  if(wToS(mapPaintWidget->getHomePos(), x, y))
  {
    int size = atools::roundToInt(context->szF(context->textSizeRangeDistance, 24));

    if(x < INVALID_INT / 2 && y < INVALID_INT / 2)
    {
      QPixmap pixmap = QIcon(":/littlenavmap/resources/icons/homemap.svg").pixmap(QSize(size, size));
      painter->drawPixmap(QPoint(x - size / 2, y - size / 2), pixmap);
    }
  }
}

/* Draw rings around objects that are selected on the search or flight plan tables */
void MapPainterMark::paintHighlights(PaintContext *context)
{
  // Draw hightlights from the search result view ------------------------------------------
  const MapSearchResult& highlightResultsSearch = mapPaintWidget->getSearchHighlights();
  int size = context->sz(context->symbolSizeAirport, 6);

  // Get airport entries from log to avoid rings around log entry airports
  QSet<int> logIds;
  for(const MapLogbookEntry& entry : highlightResultsSearch.logbookEntries)
  {
    if(entry.departurePos.isValid())
      logIds.insert(entry.departure.id);
    if(entry.destinationPos.isValid())
      logIds.insert(entry.destination.id);
  }

  QList<Pos> positions;
  for(const MapAirport& ap : highlightResultsSearch.airports)
  {
    if(!logIds.contains(ap.id))
      positions.append(ap.position);
  }

  for(const MapWaypoint& wp : highlightResultsSearch.waypoints)
    positions.append(wp.position);

  for(const MapVor& vor : highlightResultsSearch.vors)
    positions.append(vor.position);

  for(const MapNdb& ndb : highlightResultsSearch.ndbs)
    positions.append(ndb.position);

  for(const MapUserpoint& user : highlightResultsSearch.userpoints)
    positions.append(user.position);

  // for(const MapAirspace& airspace: highlightResultsSearch.airspaces)
  // positions.append(airspace.bounding.getCenter());

  // for(const MapAirspace& airspace: mapWidget->getAirspaceHighlights())
  // positions.append(airspace.bounding.getCenter());

  for(const atools::fs::sc::SimConnectAircraft& aircraft: highlightResultsSearch.onlineAircraft)
    positions.append(aircraft.getPosition());

  // Draw boundary for selected online network airspaces ------------------------------------------
  for(const MapAirspace& airspace: highlightResultsSearch.airspaces)
    paintAirspace(context, airspace, size);

  // Draw boundary for airspaces higlighted in the information window ------------------------------------------
  for(const MapAirspace& airspace: mapPaintWidget->getAirspaceHighlights())
    paintAirspace(context, airspace, size);

  // Selected logbook entries ------------------------------------------
  paintLogEntries(context, highlightResultsSearch.logbookEntries);

  GeoPainter *painter = context->painter;
  if(context->mapLayerEffective->isAirport())
    size = context->sz(context->symbolSizeAirport,
                       std::max(size, context->mapLayerEffective->getAirportSymbolSize()));

  QPen outerPen(mapcolors::highlightBackColor, size / 3. + 2., Qt::SolidLine, Qt::FlatCap);
  QPen innerPen(mapcolors::highlightColor, size / 3., Qt::SolidLine, Qt::FlatCap);

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::highlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const Pos& pos : positions)
  {
    int x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast)
      {
        // Draw black background for outline
        painter->setPen(outerPen);
        painter->drawEllipse(QPoint(x, y), size, size);
        painter->setPen(innerPen);
      }
      painter->drawEllipse(QPoint(x, y), size, size);
    }
  }

  // Draw hightlights from the approach selection ------------------------------------------
  const proc::MapProcedureLeg& leg = mapPaintWidget->getProcedureLegHighlights();

  if(leg.recFixPos.isValid())
  {
    int ellipseSize = size / 2;

    int x, y;
    if(wToS(leg.recFixPos, x, y))
    {
      // Draw recommended fix with a thin small circle
      if(!context->drawFast)
      {
        painter->setPen(QPen(mapcolors::highlightBackColor, size / 5 + 2));
        painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
        painter->setPen(QPen(mapcolors::highlightApproachColor, size / 5));
      }
      painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
    }
  }

  if(leg.line.isValid())
  {
    int ellipseSize = size;

    int x, y;
    if(wToS(leg.line.getPos1(), x, y))
    {
      if(!leg.line.isPoint() || leg.procedureTurnPos.isValid())
        ellipseSize /= 2;

      // Draw start point of the leg using a smaller circle
      if(!context->drawFast)
      {
        painter->setPen(QPen(mapcolors::highlightBackColor, size / 3 + 2));
        painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
        painter->setPen(QPen(mapcolors::highlightApproachColor, size / 3));
      }
      painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
    }

    ellipseSize = size;
    if(!leg.line.isPoint())
    {
      if(wToS(leg.line.getPos2(), x, y))
      {
        // Draw end point of the leg using a larger circle
        if(!context->drawFast)
        {
          painter->setPen(QPen(mapcolors::highlightBackColor, size / 3 + 2));
          painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
          painter->setPen(QPen(mapcolors::highlightApproachColor, size / 3));
        }
        painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
      }
    }

    if(leg.procedureTurnPos.isValid())
    {
      if(wToS(leg.procedureTurnPos, x, y))
      {
        // Draw turn position of the procedur turn
        if(!context->drawFast)
        {
          painter->setPen(QPen(mapcolors::highlightBackColor, size / 3 + 2));
          painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
          painter->setPen(QPen(mapcolors::highlightApproachColor, size / 3));
        }
        painter->drawEllipse(QPoint(x, y), ellipseSize, ellipseSize);
      }
    }
  }

  if(context->mapLayerEffective->isAirport())
    size = std::max(size, context->mapLayerEffective->getAirportSymbolSize());

  // Draw hightlights from the flight plan view ------------------------------------------
  const QList<int>& routeHighlightResults = mapPaintWidget->getRouteHighlights();
  positions.clear();
  for(int idx : routeHighlightResults)
  {
    const RouteLeg& routeLeg = NavApp::getRouteConst().at(idx);
    positions.append(routeLeg.getPosition());
  }

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::routeHighlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const Pos& pos : positions)
  {
    int x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast)
      {
        painter->setPen(QPen(QBrush(mapcolors::routeHighlightBackColor), size / 3 + 2, Qt::SolidLine,
                             Qt::FlatCap));
        painter->drawEllipse(QPoint(x, y), size, size);
        painter->setPen(QPen(QBrush(mapcolors::routeHighlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
      }
      painter->drawEllipse(QPoint(x, y), size, size);
    }
  }

  // Draw hightlight from the elevation profile view ------------------------------------------
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::profileHighlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  const Pos& pos = mapPaintWidget->getProfileHighlight();
  if(pos.isValid())
  {
    int x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast)
      {
        painter->setPen(QPen(QBrush(mapcolors::profileHighlightBackColor), size / 3 + 2, Qt::SolidLine,
                             Qt::FlatCap));
        painter->drawEllipse(QPoint(x, y), size, size);
        painter->setPen(QPen(QBrush(mapcolors::profileHighlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
      }
      painter->drawEllipse(QPoint(x, y), size, size);
    }
  }

}

void MapPainterMark::paintLogEntries(PaintContext *context, const QList<map::MapLogbookEntry>& entries)
{
  QPainter *painter = context->painter;
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBackground(mapcolors::routeOutlineColor);
  painter->setBrush(Qt::NoBrush);

  float outerlinewidth = context->sz(context->thicknessRangeDistance, 7) * 0.6f;
  float innerlinewidth = context->sz(context->thicknessRangeDistance, 4) * 0.6f;
  QPen routePen(mapcolors::routeLogEntryColor, innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  QPen routeOutlinePen(mapcolors::routeLogEntryOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin);

  // Draw connecting lines ==========================================================================
  QVector<const MapLogbookEntry *> visibleLogEntries;
  for(const MapLogbookEntry& entry : entries)
  {
    if(context->viewportRect.overlaps(entry.bounding()))
      visibleLogEntries.append(&entry);
  }

  painter->setPen(routeOutlinePen);
  for(const MapLogbookEntry *entry : visibleLogEntries)
    drawLine(context, Line(entry->departurePos, entry->destinationPos));

  painter->setPen(routePen);
  for(const MapLogbookEntry *entry : visibleLogEntries)
    drawLine(context, Line(entry->departurePos, entry->destinationPos));

  // Draw line text ==========================================================================
  context->szFont(context->textSizeRangeDistance);
  painter->setBackground(mapcolors::routeTextBackgroundColor);
  painter->setPen(mapcolors::routeTextColor);
  for(const MapLogbookEntry *entry : visibleLogEntries)
  {
    // Text for one line
    LineString positions(entry->departurePos, entry->destinationPos);

    TextPlacement textPlacement(context->painter, this);
    textPlacement.setDrawFast(context->drawFast);
    textPlacement.setLineWidth(outerlinewidth);
    textPlacement.calculateTextPositions(positions);

    QStringList text;
    text.append(tr("%1 to %2").arg(entry->departureIdent).arg(entry->destinationIdent));
    // text.append(atools::elideTextShort(entry->aircraftType, 5));
    // text.append(atools::elideTextShort(entry->aircraftRegistration, 7));
    if(entry->distanceGc > 0.f)
      text.append(Unit::distNm(entry->distanceGc, true /* unit */, 20, true /* narrow */));
    text.removeAll(QString());

    textPlacement.calculateTextAlongLines({Line(entry->departurePos, entry->destinationPos)}, {text.join(tr(","))});
    textPlacement.drawTextAlongLines();
  }

  // Draw airport symbols and text ==========================================================================
  float x, y;
  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
  textflags::TextFlags flags = context->airportTextFlagsRoute(false /* draw as route */, true /* draw as log */);

  QSet<int> airportIds;
  for(const MapLogbookEntry *entry : visibleLogEntries)
  {
    if(!airportIds.contains(entry->departure.id) && wToS(entry->departure.position, x, y))
    {
      symbolPainter->drawAirportSymbol(context->painter, entry->departure, x, y, size, false, context->drawFast);
      symbolPainter->drawAirportText(context->painter, entry->departure, x, y, context->dispOpts, flags, size,
                                     context->mapLayerEffective->isAirportDiagram(),
                                     context->mapLayer->getMaxTextLengthAirport());
      airportIds.insert(entry->departure.id);
    }

    if(!airportIds.contains(entry->destination.id) && wToS(entry->destination.position, x, y))
    {
      symbolPainter->drawAirportSymbol(context->painter, entry->destination, x, y, size, false, context->drawFast);
      symbolPainter->drawAirportText(context->painter, entry->destination, x, y, context->dispOpts, flags, size,
                                     context->mapLayerEffective->isAirportDiagram(),
                                     context->mapLayer->getMaxTextLengthAirport());
      airportIds.insert(entry->destination.id);
    }
  }
}

void MapPainterMark::paintAirspace(PaintContext *context, const map::MapAirspace& airspace, int size)
{
  const LineString *airspaceGeometry = nullptr;
  if(airspace.online)
    airspaceGeometry = airspaceQueryOnline->getAirspaceGeometry(airspace.id);
  else
    airspaceGeometry = airspaceQuery->getAirspaceGeometry(airspace.id);
  Marble::GeoPainter *painter = context->painter;

  QPen outerPen(mapcolors::highlightBackColor, size / 3. + 2., Qt::SolidLine, Qt::FlatCap);
  QPen innerPen = mapcolors::penForAirspace(airspace);
  innerPen.setWidthF(size / 3.);
  QColor c = innerPen.color();
  c.setAlpha(255);
  innerPen.setColor(c);

  painter->setBrush(mapcolors::colorForAirspaceFill(airspace));

  if(airspaceGeometry != nullptr)
  {
    if(context->viewportRect.overlaps(airspace.bounding))
    {
      if(context->objCount())
        return;

      Marble::GeoDataLinearRing linearRing;
      linearRing.setTessellate(true);

      for(const Pos& pos : *airspaceGeometry)
        linearRing.append(Marble::GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));
      GeoDataCoordinates center = linearRing.latLonAltBox().center();

      if(!context->drawFast)
      {
        // Draw black background for outline
        painter->setPen(outerPen);
        painter->drawPolygon(linearRing);
        painter->setPen(innerPen);
      }
      painter->drawPolygon(linearRing);

      int x, y;
      if(wToS(center, x, y))
      {
        QStringList texts;
        texts << (airspace.online ? airspace.name : formatter::capNavString(airspace.name))
              << map::airspaceTypeToString(airspace.type);
        if(!airspace.restrictiveDesignation.isEmpty())
          texts << (airspace.restrictiveType + "-" + airspace.restrictiveDesignation);

        symbolPainter->textBoxF(painter, {texts}, innerPen, x, y, textatt::CENTER);
      }
    }
  }
}

/* Draw all rang rings. This includes the red rings and the radio navaid ranges. */
void MapPainterMark::paintRangeRings(const PaintContext *context)
{
  const QList<map::RangeMarker>& rangeRings = mapPaintWidget->getRangeRings();
  GeoPainter *painter = context->painter;

  painter->setBrush(Qt::NoBrush);
  context->szFont(context->textSizeRangeDistance);

  float lineWidth = context->szF(context->thicknessRangeDistance, 3);

  for(const map::RangeMarker& rings : rangeRings)
  {
    // Get the biggest ring to check visibility
    QVector<int>::const_iterator maxRingIter = std::max_element(rings.ranges.begin(), rings.ranges.end());

    if(maxRingIter != rings.ranges.end())
    {
      int maxRadiusNm = *maxRingIter;

      Rect rect(rings.center, nmToMeter(maxRadiusNm));

      if(context->viewportRect.overlaps(rect) /*&& !fast*/)
      {
        // Ring is visible - the rest of the visibility check is done in paintCircle

        // Select color according to source
        QColor color = mapcolors::rangeRingColor, textColor = mapcolors::rangeRingTextColor;
        if(rings.type == map::VOR)
        {
          color = mapcolors::vorSymbolColor;
          textColor = mapcolors::vorSymbolColor;
        }
        else if(rings.type == map::NDB)
        {
          color = mapcolors::ndbSymbolColor;
          textColor = mapcolors::ndbSymbolColor;
        }

        painter->setPen(QPen(QBrush(color), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

        bool centerVisible;
        QPoint center = wToS(rings.center, DEFAULT_WTOS_SIZE, &centerVisible);
        if(centerVisible)
        {
          // Draw small center point
          painter->setPen(QPen(QBrush(color), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
          painter->drawEllipse(center, 4, 4);
        }

        // Draw all rings
        for(int radius : rings.ranges)
        {
          int xt, yt;
          paintCircle(painter, rings.center, radius, context->drawFast, xt, yt);

          if(xt != -1 && yt != -1)
          {
            // paintCirle found a text position - draw text
            painter->setPen(textColor);

            QString txt;
            if(rings.text.isEmpty())
              txt = Unit::distNm(radius);
            else
              txt = rings.text;

            xt -= painter->fontMetrics().width(txt) / 2;
            yt += painter->fontMetrics().height() / 2 - painter->fontMetrics().descent();

            symbolPainter->textBox(painter, {txt}, painter->pen(), xt, yt);
            painter->setPen(QPen(QBrush(color), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
          }
        }
      }
    }
  }
}

/* Draw a compass rose for the user aircraft with tick marks. */
void MapPainterMark::paintCompassRose(const PaintContext *context)
{
  if(context->objectTypes & map::COMPASS_ROSE && mapPaintWidget->distance() < MIN_VIEW_DISTANCE_COMPASS_ROSE_KM)
  {
    atools::util::PainterContextSaver saver(context->painter);
    Q_UNUSED(saver);

    Marble::GeoPainter *painter = context->painter;
    const atools::fs::sc::SimConnectUserAircraft& aircraft = mapPaintWidget->getUserAircraft();
    Pos pos = aircraft.getPosition();

    // Use either aircraft position or viewport center
    QRect viewport = painter->viewport();
    bool hasAircraft = pos.isValid();
    if(!hasAircraft)
      pos = sToW(viewport.center());

    // Get vertical and horizontal dimensions and calculate size
    Line horiz(context->viewportRect.getLeftCenter(), context->viewportRect.getRightCenter());
    Line vert(context->viewportRect.getTopCenter(), context->viewportRect.getBottomCenter());

    float h = horiz.lengthMeter();
    float v = vert.lengthMeter();
    if(h < 1000.f)
      h = v;

    float radiusMeter = std::min(h, v) / 2.f * 0.8f;

    radiusMeter = std::min(radiusMeter, nmToMeter(MAX_COMPASS_ROSE_RADIUS_NM));
    radiusMeter = std::max(radiusMeter, nmToMeter(MIN_COMPASS_ROSE_RADIUS_NM));

    float radiusNm = meterToNm(radiusMeter);

    painter->setBrush(Qt::NoBrush);
    float lineWidth = context->szF(context->thicknessCompassRose, 2);
    QPen rosePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen rosePenSmall(QBrush(mapcolors::compassRoseColor), lineWidth / 4.f, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen headingLinePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::DotLine, Qt::RoundCap,
                        Qt::MiterJoin);
    painter->setPen(rosePen);

    // Draw outer big circle
    int xt, yt;
    paintCircle(context->painter, pos, meterToNm(radiusMeter), context->drawFast, xt, yt);

    // Draw small center circle if no aircraft
    QPointF centerPoint = wToSF(pos);
    if(!centerPoint.isNull() && !hasAircraft)
      painter->drawEllipse(centerPoint, 5, 5);

    // Collect points for tick marks and labels
    float magVar = hasAircraft ? aircraft.getMagVarDeg() : NavApp::getMagVar(pos);
    QVector<Pos> endpoints;
    QVector<QPointF> endpointsScreen;
    for(float angle = 0.f; angle < 360.f; angle += 5)
    {
      endpoints.append(pos.endpoint(radiusMeter, magVar + angle).normalize());
      endpointsScreen.append(wToSF(endpoints.last()));
    }

    // Draw tick marks ======================================================
    if(context->dOptRose(opts::ROSE_DEGREE_MARKS))
    {
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (90 / 5)) == 0) // 90 degree ticks
        {
          drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.8f), endpoints.at(i)));
        }
        else if((i % (45 / 5)) == 0) // 45 degree ticks
          drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.84f), endpoints.at(i)));
        else if((i % (10 / 5)) == 0) // 10 degree ticks
        {
          if(mapPaintWidget->distance() < 3200 /* km */)
            drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.92f), endpoints.at(i)));
        }
        else if(mapPaintWidget->distance() < 6400 /* km */) // 5 degree ticks
          drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.95f), endpoints.at(i)));
      }
    }

    painter->setBrush(QBrush(Qt::white));
    // Calculate and draw triangle for true north ======================================================
    Pos trueNorth = pos.endpoint(radiusMeter, 0).normalize();
    QPointF trueNorthPoint = wToSF(trueNorth);

    if(!trueNorthPoint.isNull())
      painter->drawPolygon(QPolygonF({trueNorthPoint, trueNorthPoint - QPointF(10, 20),
                                      trueNorthPoint - QPointF(-10, 20)}));

    // Aircraft track and heading line ======================================================
    // Convert to selected display unit
    float radiusUnit = Unit::distNmF(radiusNm);
    float stepsizeUnit = atools::calculateSteps(radiusUnit, 6.f);
    float stepsizeNm = Unit::rev(stepsizeUnit, Unit::distNmF);
    painter->setPen(rosePenSmall);

    // Draw distance circles =======================================================
    if(context->dOptRose(opts::ROSE_RANGE_RINGS))
    {
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
        paintCircle(context->painter, pos, i * stepsizeNm, context->drawFast, xt, yt);
    }
    painter->setPen(rosePen);

    if(hasAircraft)
    {
      // Solid track line
      painter->setPen(rosePen);
      if(context->dOptRose(opts::ROSE_TRACK_LINE))
      {
        float trackTrue = aircraft.getTrackDegTrue();
        Pos trueTrackPos = pos.endpoint(radiusMeter, trackTrue).normalize();
        drawLine(context, Line(pos, trueTrackPos));
      }

      // Dotted heading line
      if(context->dOptRose(opts::ROSE_HEADING_LINE))
      {
        float headingTrue = aircraft.getHeadingDegTrue();
        Pos trueHeadingPos = pos.endpoint(radiusMeter, headingTrue).normalize();
        painter->setPen(headingLinePen);
        drawLine(context, Line(pos, trueHeadingPos));
      }
      painter->setPen(rosePen);
    }

    // Draw labels for four directions ======================================================
    context->szFont(context->textSizeCompassRose * 1.4f);
    painter->setPen(mapcolors::compassRoseTextColor);

    if(context->dOptRose(opts::ROSE_DIR_LABLES))
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
                                      endpointsScreen.at(i).x(), endpointsScreen.at(i).y(), textatt::CENTER);
            }
          }
        }
      }
    }

    // Draw small 15 deg labels ======================================================
    if(mapPaintWidget->distance() < 1600 /* km */ && context->dOptRose(opts::ROSE_DEGREE_LABELS))
    {
      // Reduce font size
      context->szFont(context->textSizeCompassRose * 0.8f);
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (15 / 5)) == 0 && (!context->dOptRose(opts::ROSE_DIR_LABLES) || !((i % (90 / 5)) == 0)))
        {
          // All 15 deg but not at 90 deg boundaries
          symbolPainter->textBoxF(painter, {QString::number(i * 5)}, painter->pen(),
                                  endpointsScreen.at(i).x(), endpointsScreen.at(i).y(), textatt::CENTER);
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
    if(context->dOptRose(opts::ROSE_RANGE_RINGS))
    {
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
      {
        QPointF s = wToSF(pos.endpoint(nmToMeter(i * stepsizeNm), trackTrue).normalize());
        if(!s.isNull())
          symbolPainter->textBoxF(painter, {Unit::distNm(i * stepsizeNm, true, 20, true)}, painter->pen(),
                                  static_cast<float>(s.x()), static_cast<float>(s.y()), textatt::CENTER);
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
                                     route.at(activeLeg) : route.at(activeLegCorrected);

          float courseToWptTrue = map::INVALID_COURSE_VALUE;
          if((routeLeg.isRoute() || !routeLeg.getProcedureLeg().isCircular()) && routeLeg.getPosition().isValid())
            courseToWptTrue = aircraft.getPosition().angleDegTo(routeLeg.getPosition());

          if(context->dOptRose(opts::ROSE_CRAB_ANGLE) && courseToWptTrue < map::INVALID_COURSE_VALUE)
          {
            // Crab angle is the amount of correction an aircraft must be turned into the wind in order to maintain the desired course.
            float crabAngle = windCorrectedHeading(aircraft.getWindSpeedKts(), aircraft.getWindDirectionDegT(),
                                                   courseToWptTrue, aircraft.getTrueAirspeedKts());

            Pos crabPos = pos.endpoint(radiusMeter, crabAngle).normalize();
            painter->setPen(rosePen);
            painter->setBrush(OptionData::instance().getFlightplanActiveSegmentColor());

            QPointF crabScreenPos = wToSF(crabPos);
            painter->drawEllipse(crabScreenPos, lineWidth * 3, lineWidth * 3);
          }

          if(context->dOptRose(opts::ROSE_NEXT_WAYPOINT) && courseToWptTrue < map::INVALID_COURSE_VALUE)
          {
            // Draw small line to show course to next waypoint ========================
            Pos endPt = pos.endpoint(radiusMeter, courseToWptTrue).normalize();
            Line crsLine(pos.interpolate(endPt, radiusMeter, 0.92f), endPt);
            painter->setPen(QPen(mapcolors::routeOutlineColor, context->sz(context->thicknessFlightplan, 7),
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawLineStraight(context, crsLine);

            painter->setPen(QPen(OptionData::instance().getFlightplanActiveSegmentColor(),
                                 context->sz(context->thicknessFlightplan, 4),
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawLineStraight(context, crsLine);
          }
        }
      }

      // Aircraft track label at end of track line ======================================================
      if(context->dOptRose(opts::ROSE_TRACK_LABEL))
      {
        QPointF trueTrackTextPoint = wToSF(pos.endpoint(radiusMeter * 1.1f, trackTrue).normalize());
        if(!trueTrackTextPoint.isNull())
        {
          painter->setPen(mapcolors::compassRoseTextColor);
          context->szFont(context->textSizeCompassRose);
          QString text =
            tr("%1°M").arg(QString::number(atools::roundToInt(aircraft.getTrackDegMag())));
          symbolPainter->textBoxF(painter, {text, tr("TRK")}, painter->pen(), trueTrackTextPoint.x(),
                                  trueTrackTextPoint.y(), textatt::CENTER | textatt::ROUTE_BG_COLOR);
        }
      }
    }
  }
}

/* Draw great circle and rhumb line distance measurement lines */
void MapPainterMark::paintDistanceMarkers(const PaintContext *context)
{
  GeoPainter *painter = context->painter;
  context->szFont(context->textSizeRangeDistance);
  QFontMetrics metrics = painter->fontMetrics();

  const QList<map::DistanceMarker>& distanceMarkers = mapPaintWidget->getDistanceMarkers();
  float lineWidth = context->szF(context->thicknessRangeDistance, 3);
  TextPlacement textPlacement(context->painter, this);

  for(const map::DistanceMarker& m : distanceMarkers)
  {
    // Get color from marker
    painter->setPen(QPen(m.color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

    const int SYMBOL_SIZE = 5;
    int x, y;
    if(wToS(m.from, x, y))
      // Draw ellipse at start point
      painter->drawEllipse(QPoint(x, y), SYMBOL_SIZE, SYMBOL_SIZE);

    if(wToS(m.to, x, y))
    {
      // Draw cross at end point
      painter->drawLine(x - SYMBOL_SIZE, y, x + SYMBOL_SIZE, y);
      painter->drawLine(x, y - SYMBOL_SIZE, x, y + SYMBOL_SIZE);
    }

    painter->setPen(QPen(m.color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
    if(!m.isRhumbLine)
    {
      // Draw great circle line
      float distanceMeter = m.from.distanceMeterTo(m.to);

      GeoDataCoordinates from(m.from.getLonX(), m.from.getLatY(), 0, DEG);
      GeoDataCoordinates to(m.to.getLonX(), m.to.getLatY(), 0, DEG);

      // Draw line
      GeoDataLineString line;
      line.append(from);
      line.append(to);
      line.setTessellate(true);
      painter->drawPolyline(line);

      // Build and draw text
      QStringList texts;
      if(!m.text.isEmpty())
        texts.append(m.text);

      double init = normalizeCourse(from.bearing(to, DEG, INITBRG));
      double final = normalizeCourse(from.bearing(to, DEG, FINALBRG));

      if(atools::almostEqual(init, final, 1.))
        texts.append(QString::number(init, 'f', 0) + tr("°T"));
      else
        texts.append(QString::number(init, 'f', 0) + tr("°T ► ") + QString::number(final, 'f', 0) + tr("°T"));

      if(Unit::getUnitDist() == opts::DIST_KM && Unit::getUnitShortDist() == opts::DIST_SHORT_METER &&
         distanceMeter < 6000)
        texts.append(QString::number(distanceMeter, 'f', 0) + Unit::getUnitShortDistStr());
      else
      {
        texts.append(Unit::distMeter(distanceMeter));
        if(distanceMeter < 6000)
          // Add feet to text for short distances
          texts.append(Unit::distShortMeter(distanceMeter));
      }

      if(m.from != m.to)
      {
        int xt = -1, yt = -1;
        if(textPlacement.findTextPos(m.from, m.to, distanceMeter, metrics.width(texts.at(0)),
                                     metrics.height() * 2, xt, yt, nullptr))
          symbolPainter->textBox(painter, texts, painter->pen(), xt, yt, textatt::BOLD | textatt::CENTER);
      }
    }
    else
    {
      // Draw a rhumb line with constant course
      float bearing = m.from.angleDegToRhumb(m.to);
      float magBearing = bearing - m.magvar;
      magBearing = normalizeCourse(magBearing);
      bearing = normalizeCourse(bearing);

      float distanceMeter = m.from.distanceMeterToRhumb(m.to);

      // Approximate the needed number of line segments
      int pixel = scale->getPixelIntForMeter(distanceMeter);
      int numPoints = std::min(std::max(pixel / (context->drawFast ? 200 : 20), 4), 72);

      Pos p1 = m.from, p2;

      // Draw line segments
      for(float d = 0.f; d < distanceMeter; d += distanceMeter / numPoints)
      {
        p2 = m.from.endpointRhumb(d, bearing);
        GeoDataLineString line;
        line.append(GeoDataCoordinates(p1.getLonX(), p1.getLatY(), 0, DEG));
        line.append(GeoDataCoordinates(p2.getLonX(), p2.getLatY(), 0, DEG));
        painter->drawPolyline(line);
        p1 = p2;
      }

      // Draw rest
      p2 = m.from.endpointRhumb(distanceMeter, bearing);
      GeoDataLineString line;
      line.append(GeoDataCoordinates(p1.getLonX(), p1.getLatY(), 0, DEG));
      line.append(GeoDataCoordinates(p2.getLonX(), p2.getLatY(), 0, DEG));
      painter->drawPolyline(line);

      // Build and draw text
      QStringList texts;
      if(!m.text.isEmpty())
        texts.append(m.text);

      QString bearingText = QString::number(bearing, 'f', 0);
      QString magBearingText = QString::number(magBearing, 'f', 0);
      if(bearingText == magBearingText)
        texts.append(bearingText + tr("°M/T"));
      else
        texts.append(magBearingText + tr("°M") + "/" + bearingText + tr("°T"));

      if(Unit::getUnitDist() == opts::DIST_KM && Unit::getUnitShortDist() == opts::DIST_SHORT_METER &&
         distanceMeter < 6000)
        texts.append(QString::number(distanceMeter, 'f', 0) + Unit::getUnitShortDistStr());
      else
      {
        texts.append(Unit::distMeter(distanceMeter));
        if(distanceMeter < 6000)
          texts.append(Unit::distShortMeter(distanceMeter));
      }

      if(m.from != m.to)
      {
        int xt = -1, yt = -1;
        if(textPlacement.findTextPosRhumb(m.from, m.to, distanceMeter, metrics.width(texts.at(0)),
                                          metrics.height() * 2, xt, yt))
          symbolPainter->textBox(painter, texts,
                                 painter->pen(), xt, yt, textatt::ITALIC | textatt::BOLD | textatt::CENTER);
      }
    }
  }
}

void MapPainterMark::paintTrafficPatterns(const PaintContext *context)
{
  if(!context->mapLayer->isApproach())
    return;

  atools::util::PainterContextSaver saver(context->painter);
  GeoPainter *painter = context->painter;
  const QList<TrafficPattern>& patterns = mapPaintWidget->getTrafficPatterns();
  float lineWidth = context->szF(context->thicknessRangeDistance, 3);
  context->szFont(context->textSizeRangeDistance);

  TextPlacement textPlacement(painter, this);
  textPlacement.setLineWidth(lineWidth);
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setBackground(Qt::white);

  QPolygonF arrow = buildArrow(lineWidth * 2.3f);

  for(const TrafficPattern& pattern : patterns)
  {
    float finalDistance = pattern.base45Degree ? pattern.downwindDistance : pattern.baseDistance;

    // Turn point base to final
    Pos baseFinal = pattern.position.endpoint(nmToMeter(finalDistance),
                                              opposedCourseDeg(pattern.heading)).normalize();

    // Turn point downwind to base
    Pos downwindBase = baseFinal.endpoint(nmToMeter(pattern.downwindDistance),
                                          pattern.heading + (pattern.turnRight ? 90.f : -90.f)).normalize();

    // Turn point upwind to crosswind
    Pos upwindCrosswind = pattern.position.endpoint(nmToMeter(finalDistance) + feetToMeter(pattern.runwayLength),
                                                    pattern.heading).normalize();

    // Turn point crosswind to downwind
    Pos crosswindDownwind = upwindCrosswind.endpoint(nmToMeter(pattern.downwindDistance),
                                                     pattern.heading + (pattern.turnRight ? 90.f : -90.f)).normalize();

    // Calculate bounding rectangle and check if it is at least partially visible
    Rect rect(baseFinal);
    rect.extend(downwindBase);
    rect.extend(upwindCrosswind);
    rect.extend(crosswindDownwind);

    if(context->viewportRect.overlaps(rect))
    {
      // Entry at opposite runway threshold
      Pos downwindEntry = downwindBase.endpoint(nmToMeter(finalDistance) + feetToMeter(
                                                  pattern.runwayLength), pattern.heading).normalize();

      // Bail out if any points are hidden behind the globe
      bool visible, hidden;
      QPointF originPoint = wToS(pattern.getPosition(), DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      QPointF baseFinalPoint = wToS(baseFinal, DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      QPointF downwindBasePoint = wToS(downwindBase, DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      QPointF upwindCrosswindPoint = wToS(upwindCrosswind, DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      QPointF crosswindDownwindPoint = wToS(crosswindDownwind, DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      QPointF downwindEntryPoint = wToS(downwindEntry, DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        continue;
      bool drawDetails = QLineF(baseFinalPoint, crosswindDownwindPoint).length() > 50.;

      // Calculate polygon rounding in pixels =======================
      float pixelForNm = scale->getPixelForNm(pattern.downwindDistance, pattern.heading + 90.f);
      atools::util::RoundedPolygon polygon(pixelForNm / 3.f,
                                           {originPoint, upwindCrosswindPoint, crosswindDownwindPoint,
                                            downwindBasePoint, baseFinalPoint});

      QLineF downwind(crosswindDownwindPoint, downwindBasePoint);
      QLineF upwind(originPoint, upwindCrosswindPoint);
      float angle = static_cast<float>(angleFromQt(downwind.angle()));
      float oppAngle = static_cast<float>(opposedCourseDeg(angleFromQt(downwind.angle())));

      if(pattern.showEntryExit && context->mapLayer->isApproachTextAndDetail())
      {
        // Draw a line below to fill the gap because of round edges
        painter->setBrush(Qt::white);
        painter->setPen(QPen(pattern.color, context->szF(context->thicknessRangeDistance, 3), Qt::DashLine));
        drawLine(painter, upwind);

        // Straight out exit for pattern =======================
        QPointF exitStraight =
          wToS(upwindCrosswind.endpoint(nmToMeter(1.f), oppAngle).normalize(),
               DEFAULT_WTOS_SIZE, &visible, &hidden);
        drawLine(painter, upwind.p2(), exitStraight);

        // 45 degree exit for pattern =======================
        QPointF exit45Deg =
          wToS(upwindCrosswind.endpoint(nmToMeter(1.f), oppAngle + (pattern.turnRight ? 45.f : -45.f)).normalize(),
               DEFAULT_WTOS_SIZE, &visible, &hidden);
        drawLine(painter, upwind.p2(), exit45Deg);

        // Entry to downwind
        QPointF entry =
          wToS(downwindEntry.endpoint(nmToMeter(1.f), oppAngle + (pattern.turnRight ? 45.f : -45.f)).normalize(),
               DEFAULT_WTOS_SIZE, &visible, &hidden);
        drawLine(painter, downwindEntryPoint, entry);

        if(drawDetails)
        {
          // Draw arrows to all the entry and exit indicators ========================
          painter->setPen(QPen(pattern.color, context->szF(context->thicknessRangeDistance, 2), Qt::SolidLine));
          paintArrowAlongLine(painter, QLineF(upwind.p2(), exitStraight), arrow, 0.95f);
          paintArrowAlongLine(painter, QLineF(upwind.p2(), exit45Deg), arrow, 0.95f);
          paintArrowAlongLine(painter, QLineF(entry, downwindEntryPoint), arrow, 0.05f);
        }
      }

      painter->setPen(QPen(pattern.color, lineWidth));
      painter->setBrush(Qt::NoBrush);
      painter->drawPath(polygon.getPainterPath());

      if(drawDetails && context->mapLayer->isApproachTextAndDetail())
      {
        // Text for downwind leg =======================================
        QLineF final (baseFinalPoint, originPoint);
        QPointF center = downwind.center();
        QString text = QString("%1/%2°M").
                       arg(Unit::altFeet(pattern.position.getAltitude(), true, true)).
                       arg(QLocale().toString(
                             opposedCourseDeg(normalizeCourse(pattern.heading - pattern.magvar)), 'f', 0));

        painter->setBrush(Qt::white);
        textPlacement.drawTextAlongOneLine(text, angle, center, atools::roundToInt(downwind.length()),
                                           true /* both visible */);

        // Text for final leg =======================================
        text = QString("RW%1/%2°M").
               arg(pattern.runwayName).
               arg(QLocale().toString(normalizeCourse(pattern.heading - pattern.magvar), 'f', 0));
        textPlacement.drawTextAlongOneLine(text, oppAngle, final.pointAt(0.60), atools::roundToInt(final.length()),
                                           true /* both visible */);

        // Draw arrows on legs =======================================
        // Set a lighter fill color for arros
        painter->setBrush(pattern.color.lighter(190));
        painter->setPen(QPen(pattern.color, context->szF(context->thicknessRangeDistance, 2)));

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

        // Draw ellipse at touchdown point
        painter->drawEllipse(originPoint, lineWidth * 2.f, lineWidth * 2.f);
      }
    }
  }
}

void MapPainterMark::paintUserpointDrag(const PaintContext *context)
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
void MapPainterMark::paintRouteDrag(const PaintContext *context)
{
  LineString fixed;
  QPoint cur;

  MapWidget *mapWidget = dynamic_cast<MapWidget *>(mapPaintWidget);
  if(mapWidget != nullptr)
    mapWidget->getRouteDragPoints(fixed, cur);

  if(!cur.isNull())
  {
    GeoDataCoordinates curGeo;
    if(sToW(cur.x(), cur.y(), curGeo))
    {
      context->painter->setPen(QPen(mapcolors::mapDragColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      GeoDataLineString linestring;
      linestring.setTessellate(true);

      // Draw lines from current mouse position to all fixed points which can be waypoints or several alternates
      for(const Pos& pos : fixed)
      {
        linestring.clear();
        linestring.append(GeoDataCoordinates(curGeo));
        linestring.append(GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));
        context->painter->drawPolyline(linestring);
      }
    }
  }
}
