/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/mappaintermark.h"

#include "mapgui/mapwidget.h"
#include "navapp.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "query/airspacequery.h"
#include "geo/calculations.h"
#include "common/symbolpainter.h"
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

MapPainterMark::MapPainterMark(MapWidget *mapWidget, MapScale *mapScale)
  : MapPainter(mapWidget, mapScale)
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
  if(wToS(mapWidget->getSearchMarkPos(), x, y))
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
  if(wToS(mapWidget->getHomePos(), x, y))
  {
    painter->setPen(mapcolors::homeBackPen);
    painter->setBrush(mapcolors::homeFillColor);

    int size = context->sz(context->symbolSizeNavaid, 10);

    painter->drawRect(x - (size / 2), y - (size / 2), size, size);

    QPolygon roof;
    roof << QPoint(x, y - size) << QPoint(x + size - 3, y - 3) << QPoint(x - size + 3, y - 3);
    painter->drawConvexPolygon(roof);
    painter->drawPoint(x, y);
  }
}

/* Draw rings around objects that are selected on the search or flight plan tables */
void MapPainterMark::paintHighlights(PaintContext *context)
{
  // Draw hightlights from the search result view ------------------------------------------
  const MapSearchResult& highlightResults = mapWidget->getSearchHighlights();
  int size = context->sz(context->symbolSizeAirport, 6);

  QList<Pos> positions;

  for(const MapAirport& ap : highlightResults.airports)
    positions.append(ap.position);

  for(const MapWaypoint& wp : highlightResults.waypoints)
    positions.append(wp.position);

  for(const MapVor& vor : highlightResults.vors)
    positions.append(vor.position);

  for(const MapNdb& ndb : highlightResults.ndbs)
    positions.append(ndb.position);

  for(const MapUserpoint& user : highlightResults.userpoints)
    positions.append(user.position);

  for(const MapAirspace& airspace: highlightResults.airspaces)
    positions.append(airspace.bounding.getCenter());

  for(const atools::fs::sc::SimConnectAircraft& aircraft: highlightResults.onlineAircraft)
    positions.append(aircraft.getPosition());

  GeoPainter *painter = context->painter;
  if(context->mapLayerEffective->isAirport())
    size = context->sz(context->symbolSizeAirport,
                       std::max(size, context->mapLayerEffective->getAirportSymbolSize()));

  QPen outerPen(QBrush(mapcolors::highlightBackColor), size / 3 + 2, Qt::SolidLine, Qt::FlatCap);
  QPen innerPen(QBrush(mapcolors::highlightColor), size / 3, Qt::SolidLine, Qt::FlatCap);

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

  // Draw boundary for selected online network airspaces ------------------------------------------
  for(const MapAirspace& airspace: highlightResults.airspaces)
  {
    if(airspace.online)
    {
      const LineString *airspaceGeometry = airspaceQueryOnline->getAirspaceGeometry(airspace.id);
      if(airspaceGeometry != nullptr)
      {
        if(context->viewportRect.overlaps(airspace.bounding))
        {
          if(context->objCount())
            return;

          // qDebug() << airspace.getId() << airspace.name;

          Marble::GeoDataLinearRing linearRing;
          linearRing.setTessellate(true);

          for(const Pos& pos : *airspaceGeometry)
            linearRing.append(Marble::GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));

          if(!context->drawFast)
          {
            // Draw black background for outline
            painter->setPen(outerPen);
            painter->drawPolygon(linearRing);
            painter->setPen(innerPen);
          }
          painter->drawPolygon(linearRing);
        }
      }
    }
  }

  // Draw hightlights from the approach selection ------------------------------------------
  const proc::MapProcedureLeg& leg = mapWidget->getProcedureLegHighlights();

  positions.clear();

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
  const QList<int>& routeHighlightResults = mapWidget->getRouteHighlights();
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
}

/* Draw all rang rings. This includes the red rings and the radio navaid ranges. */
void MapPainterMark::paintRangeRings(const PaintContext *context)
{
  const QList<map::RangeMarker>& rangeRings = mapWidget->getRangeRings();
  GeoPainter *painter = context->painter;

  painter->setBrush(Qt::NoBrush);

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
  if(context->objectTypes & map::COMPASS_ROSE && mapWidget->distance() < MIN_VIEW_DISTANCE_COMPASS_ROSE_KM)
  {
    atools::util::PainterContextSaver saver(context->painter);
    Q_UNUSED(saver);

    Marble::GeoPainter *painter = context->painter;
    atools::geo::Pos pos = mapWidget->getUserAircraft().getPosition();

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

    radiusMeter = std::min(radiusMeter, atools::geo::nmToMeter(MAX_COMPASS_ROSE_RADIUS_NM));
    radiusMeter = std::max(radiusMeter, atools::geo::nmToMeter(MIN_COMPASS_ROSE_RADIUS_NM));

    float radiusNm = atools::geo::meterToNm(radiusMeter);

    painter->setBrush(Qt::NoBrush);
    float lineWidth = context->szF(context->thicknessCompassRose, 2);
    QPen rosePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen rosePenSmall(QBrush(mapcolors::compassRoseColor), lineWidth / 8.f, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    QPen headingLinePen(QBrush(mapcolors::compassRoseColor), lineWidth, Qt::DotLine, Qt::RoundCap,
                        Qt::MiterJoin);
    painter->setPen(rosePen);

    // Draw outer big circle
    int xt, yt;
    paintCircle(context->painter, pos, atools::geo::meterToNm(radiusMeter), context->drawFast, xt, yt);

    // Draw small center circle if no aircraft
    QPointF centerPoint = wToSF(pos);
    if(!centerPoint.isNull() && !hasAircraft)
      painter->drawEllipse(centerPoint, 5, 5);

    // Collect points for tick marks and labels
    float magVar = hasAircraft ? mapWidget->getUserAircraft().getMagVarDeg() : NavApp::getMagVar(pos);
    QVector<Pos> endpoints;
    QVector<QPointF> endpointsScreen;
    for(float angle = 0.f; angle < 360.f; angle += 5)
    {
      endpoints.append(pos.endpoint(radiusMeter, magVar + angle));
      endpointsScreen.append(wToSF(endpoints.last()));
    }

    // Draw tick marks ======================================================
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
        if(mapWidget->distance() < 3200 /* km */)
          drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.92f), endpoints.at(i)));
      }
      else if(mapWidget->distance() < 6400 /* km */) // 5 degree ticks
        drawLineStraight(context, Line(pos.interpolate(endpoints.at(i), radiusMeter, 0.95f), endpoints.at(i)));
    }

    painter->setBrush(QBrush(Qt::white));
    // Calculate and draw triangle for true north ======================================================
    Pos trueNorth = pos.endpoint(radiusMeter, 0);
    QPointF trueNorthPoint = wToSF(trueNorth);

    if(!trueNorthPoint.isNull())
      painter->drawPolygon(QPolygonF({trueNorthPoint, trueNorthPoint - QPointF(10, 20),
                                      trueNorthPoint - QPointF(-10, 20)}));

    // Aircraft track and heading line ======================================================
    // Convert to selected display unit
    float radiusUnit = Unit::distNmF(radiusNm);
    float stepsizeUnit = atools::calculateSteps(radiusUnit, 6.f);
    float stepsizeNm = Unit::rev(stepsizeUnit, Unit::distNmF);
    if(hasAircraft)
    {
      painter->setPen(rosePenSmall);
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
        paintCircle(context->painter, pos, i * stepsizeNm, context->drawFast, xt, yt);

      // Solid track line
      painter->setPen(rosePen);
      float trackTrue = mapWidget->getUserAircraft().getTrackDegTrue();
      Pos trueTrackPos = pos.endpoint(radiusMeter, trackTrue);
      drawLine(context, Line(pos, trueTrackPos));

      // Dotted heading line
      float headingTrue = mapWidget->getUserAircraft().getHeadingDegTrue();
      Pos trueHeadingPos = pos.endpoint(radiusMeter, headingTrue);
      painter->setPen(headingLinePen);
      drawLine(context, Line(pos, trueHeadingPos));
      painter->setPen(rosePen);
    }

    // Draw labels for four directions ======================================================
    context->szFont(1.4f);
    painter->setPen(mapcolors::compassRoseTextColor);

    for(int i = 0; i < 360 / 5; i++)
    {
      if((i % (90 / 5)) == 0)
      {
        if(mapWidget->distance() < 6400 /* km */)
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

    // Draw small 15 deg labels ======================================================
    if(mapWidget->distance() < 1600 /* km */)
    {
      // Reduce font size
      context->szFont(0.8f);
      for(int i = 0; i < 360 / 5; i++)
      {
        if((i % (15 / 5)) == 0 && !((i % (90 / 5)) == 0))
        {
          // All 15 deg but not at 90 deg boundaries
          symbolPainter->textBoxF(painter, {QString::number(i * 5)}, painter->pen(),
                                  endpointsScreen.at(i).x(), endpointsScreen.at(i).y(), textatt::CENTER);
        }
      }
    }

    // Aircraft track line end text and distance labels along track line ======================================================
    if(hasAircraft)
    {
      // Solid track line
      float trackTrue = mapWidget->getUserAircraft().getTrackDegTrue();

      // Distance labels along track line
      context->szFont(0.8f);
      for(float i = 1.f; i * stepsizeNm < radiusNm; i++)
      {
        QPointF s = wToSF(pos.endpoint(atools::geo::nmToMeter(i * stepsizeNm), trackTrue));
        if(!s.isNull())
          symbolPainter->textBoxF(painter, {Unit::distNm(i * stepsizeNm, true, true)}, painter->pen(),
                                  s.x(), s.y(), textatt::CENTER);
      }

      // Aircraft track label at end of track line ======================================================
      QPointF trueTrackTextPoint = wToSF(pos.endpoint(radiusMeter * 1.1f, trackTrue));
      if(!trueTrackTextPoint.isNull())
      {
        context->szFont(1.f);
        QString text =
          tr("%1°M").arg(QString::number(atools::roundToInt(mapWidget->getUserAircraft().getTrackDegMag())));
        symbolPainter->textBoxF(painter, {text, tr("TRK")}, painter->pen(), trueTrackTextPoint.x(),
                                trueTrackTextPoint.y(), textatt::CENTER | textatt::ROUTE_BG_COLOR);
      }
    }
  }
}

/* Draw great circle and rhumb line distance measurment lines */
void MapPainterMark::paintDistanceMarkers(const PaintContext *context)
{
  GeoPainter *painter = context->painter;
  QFontMetrics metrics = painter->fontMetrics();

  const QList<map::DistanceMarker>& distanceMarkers = mapWidget->getDistanceMarkers();
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

      double init = atools::geo::normalizeCourse(from.bearing(to, DEG, INITBRG));
      double final = atools::geo::normalizeCourse(from.bearing(to, DEG, FINALBRG));

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
      magBearing = atools::geo::normalizeCourse(magBearing);
      bearing = atools::geo::normalizeCourse(bearing);

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

void MapPainterMark::paintUserpointDrag(const PaintContext *context)
{
  // Get screen position an pixmap
  QPoint cur;
  QPixmap pixmap;
  mapWidget->getUserpointDragPoints(cur, pixmap);

  if(!cur.isNull() && mapWidget->rect().contains(cur) && !pixmap.isNull())
    context->painter->drawPixmap(cur.x() - pixmap.width() / 2, cur.y() - pixmap.height() / 2, pixmap);
}

/* Draw route dragging/moving lines */
void MapPainterMark::paintRouteDrag(const PaintContext *context)
{
  atools::geo::Pos from, to;
  QPoint cur;

  // Flight plan editing use always three points except at the end
  mapWidget->getRouteDragPoints(from, to, cur);
  if(!cur.isNull())
  {
    GeoDataCoordinates curGeo;
    if(sToW(cur.x(), cur.y(), curGeo))
    {
      GeoDataLineString linestring;
      linestring.setTessellate(true);

      if(from.isValid())
        linestring.append(GeoDataCoordinates(from.getLonX(), from.getLatY(), 0, DEG));
      linestring.append(GeoDataCoordinates(curGeo));
      if(to.isValid())
        linestring.append(GeoDataCoordinates(to.getLonX(), to.getLatY(), 0, DEG));

      if(linestring.size() > 1)
      {
        context->painter->setPen(QPen(mapcolors::mapDragColor, 3, Qt::SolidLine, Qt::RoundCap,
                                      Qt::RoundJoin));
        context->painter->drawPolyline(linestring);
      }
    }
  }
}
