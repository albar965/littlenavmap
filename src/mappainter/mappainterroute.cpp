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

#include "mappainter/mappainterroute.h"

#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/proctypes.h"
#include "common/symbolpainter.h"
#include "common/textplacement.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "app/navapp.h"
#include "route/route.h"
#include "route/routealtitudeleg.h"
#include "util/paintercontextsaver.h"
#include "weather/windreporter.h"

#include <QPainterPath>
#include <QStringBuilder>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using atools::contains;
using map::MapType;
using map::PosCourse;
using proc::MapProcedureLeg;
using proc::MapProcedureLegs;
namespace ageo = atools::geo;

MapPainterRoute::MapPainterRoute(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::render()
{
  // Clear before collecting duplicates
  routeProcIdMap.clear();

  context->startTimer("Route");
  // Draw route including procedures =====================================
  if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN))
  {
    // Plan, procedures and departure position
    paintRoute();

    // Draw TOD and TOC markers ======================
    if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_TOC_TOD) && context->mapLayerRoute->isRouteTextAndDetail())
      paintTopOfDescentAndClimb();
  }

  // Draw the approach preview if any selected in the procedure search tab ========================
  if(context->mapLayerRoute->isApproach())
  {
    // Draw multi preview ===========================================================
    QSet<map::MapRef> procIdMapDummy; // No need for de-duplication pass dummy map in
    for(const proc::MapProcedureLegs& procedure : mapPaintWidget->getProcedureHighlights())
      paintProcedure(procIdMapDummy, procedure, 0, procedure.previewColor, true /* preview */, true /* previewAll */);
    procIdMapDummy.clear();

    // Draw selection highlight ===========================================================
    const proc::MapProcedureLegs& procedureHighlight = mapPaintWidget->getProcedureHighlight();
    if(!procedureHighlight.isEmpty())
      paintProcedure(procIdMapDummy, procedureHighlight, 0, procedureHighlight.previewColor,
                     true /* preview */, false /* previewAll */);
  }
  context->endTimer("Route");
}

QString MapPainterRoute::buildLegText(const RouteLeg& leg)
{
  if(mapPaintWidget->isDistanceCutOff())
    return QString();

  QStringList texts;
  if(context->dOptRoute(optsd::ROUTE_AIRWAY) && !leg.getAirwayName().isEmpty())
    texts.append(leg.getAirwayName());

  texts.append(leg.buildLegText(context->dOptRoute(optsd::ROUTE_DISTANCE), context->dOptRoute(optsd::ROUTE_MAG_COURSE),
                                context->dOptRoute(optsd::ROUTE_TRUE_COURSE), true /* narrow */));

  return texts.join(tr(" / "));
}

QString MapPainterRoute::buildLegText(float distance, float courseMag, float courseTrue)
{
  if(mapPaintWidget->isDistanceCutOff())
    return QString();

  if(!context->dOptRoute(optsd::ROUTE_DISTANCE))
    distance = map::INVALID_DISTANCE_VALUE;

  if(!context->dOptRoute(optsd::ROUTE_MAG_COURSE))
    courseMag = map::INVALID_DISTANCE_VALUE;

  if(!context->dOptRoute(optsd::ROUTE_TRUE_COURSE))
    courseTrue = map::INVALID_DISTANCE_VALUE;

  return RouteLeg::buildLegText(distance, courseMag, courseTrue, true /* narrow */).join(tr(" / "));
}

void MapPainterRoute::paintRoute()
{
  const Route *route = context->route;
  if(route->isEmpty() || (route->size() >= 2 && !resolves(route->getBoundingRect())))
    return;

  atools::util::PainterContextSaver saver(context->painter);

  context->painter->setBrush(Qt::NoBrush);

  // Get active route leg
  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg = route->isActiveValid() ? route->getActiveLegIndex() : 0;
  if(activeRouteLeg == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO;
    return;
  }

  int passedRouteLeg = context->flags2.testFlag(opts2::MAP_ROUTE_DIM_PASSED) ? activeRouteLeg : 0;

  // Collect line text and geometry from the route
  QStringList routeTexts;
  QVector<Line> lines;
  bool drawAlternate = context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE);

  // Collect route - only coordinates and texts ===============================
  const RouteLeg& destLeg = route->getDestinationAirportLeg();
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->value(i);
    const RouteLeg& last = route->value(i - 1);

    if(leg.isAlternate())
    {
      if(drawAlternate)
      {
        routeTexts.append(buildLegText(leg));
        lines.append(Line(destLeg.getPosition(), leg.getPosition()));
      }
      else
      {
        // Add empty texts to skip drawing
        routeTexts.append(QString());
        lines.append(Line());
      }
    }
    else
    {
      // Draw if at least one is part of the route - also draw empty spaces between procedures
      if(last.isRoute() || leg.isRoute() || // Draw if any is route - also covers STAR to airport
         (last.getProcedureLeg().isAnyDeparture() && leg.getProcedureLeg().isAnyArrival()) || // empty space from SID to STAR, transition or approach
         (last.getProcedureLeg().isStar() && leg.getProcedureLeg().isArrival())) // empty space from STAR to transition or approach
      {
        if(i >= passedRouteLeg)
          routeTexts.append(buildLegText(leg));
        else
          // No texts for passed legs
          routeTexts.append(QString());

        lines.append(Line(last.getPosition(), leg.getPosition()));
      }
      else
      {
        // Text and lines are drawn by paintProcedure
        routeTexts.append(QString());
        lines.append(Line());
      }
    }
  }

  paintRouteInternal(routeTexts, lines, passedRouteLeg);

  // Draw procedures ==========================================================================
  if(context->mapLayerEffective->isApproach())
  {
    // Remember last point across procedures to avoid overlaying text
    if(!mapPaintWidget->isDistanceCutOff())
    {
      // Draw in reverse flying order to have close legs on top
      const QColor& flightplanProcedureColor = OptionData::instance().getFlightplanProcedureColor();

      // Keep de-duplication separate from approach and STAR to avoid lines overlaying symbols
      QSet<map::MapRef> idMap(routeProcIdMap);
      if(route->hasAnyApproachProcedure())
        paintProcedure(idMap, route->getApproachLegs(), route->getApproachLegsOffset(),
                       flightplanProcedureColor, false /* preview */, false /* previewAll */);

      if(route->hasAnyStarProcedure())
        paintProcedure(routeProcIdMap, route->getStarLegs(), route->getStarLegsOffset(), flightplanProcedureColor,
                       false /* preview */, false /* previewAll */);
      routeProcIdMap.unite(idMap);

      if(route->hasAnySidProcedure())
        paintProcedure(routeProcIdMap, route->getSidLegs(), route->getSidLegsOffset(), flightplanProcedureColor,
                       false /* preview */, false /* previewAll */);

    }
  }

  // Recommended navaids
  paintRecommended(passedRouteLeg, routeProcIdMap);

  // Draw highlight for departure parking, start, helipad or airport
  if(context->mapLayerRoute->isRouteTextAndDetail())
    paintStartParking();

#ifdef DEBUG_ROUTE_PAINT
  context->painter->save();
  context->painter->setPen(Qt::black);
  context->painter->setBrush(Qt::white);
  context->painter->setBackground(Qt::white);
  context->painter->setBackgroundMode(Qt::OpaqueMode);
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->at(i);
    const RouteLeg& last = route->at(i - 1);

    bool hiddenDummy;

    QLineF linef;
    wToS(Line(last.getPosition(), leg.getPosition()), linef, DEFAULT_WTOS_SIZE, &hiddenDummy);

    context->painter->drawText(linef.p1() + QPointF(-60, 20), "start " + QString::number(i) + " " +
                               (leg.isAnyProcedure() ? "P" : ""));
    context->painter->drawText(linef.p2() + QPointF(-60, -20), "end " + QString::number(i) + " " +
                               (leg.isAnyProcedure() ? "P" : ""));
  }
  context->painter->restore();
#endif

}

void MapPainterRoute::paintRecommended(int passedRouteLeg, QSet<map::MapRef>& idMap)
{
  // Margins for text at left (VOR), right (waypoints) and below (NDB)
  const static QMargins MARGINS(50, 10, 50, 20);

  const Route& route = NavApp::getRouteConst();
  for(int i = passedRouteLeg; i < route.size(); i++)
  {
    const RouteLeg& routeLeg = route.value(i);
    map::MapTypes type = routeLeg.getMapType();
    if(type == map::PROCEDURE)
    {
      float x = 0, y = 0;
      // Only show missed related if missed is shown
      if(!routeLeg.getProcedureLeg().isMissed() || context->objectTypes.testFlag(map::MISSED_APPROACH))
      {
        // Procedure recommended navaids drawn by route code
        const map::MapResult& recNavaids = routeLeg.getProcedureLeg().recNavaids;
        if(recNavaids.hasWaypoints())
        {
          const map::MapWaypoint& wp = recNavaids.waypoints.constFirst();

          // Do not draw related if it was drawn already as a part of a procedure
          if(!idMap.contains(wp.getRef()))
          {
            idMap.insert(wp.getRef());
            if(wToSBuf(wp.position, x, y, MARGINS))
            {
              paintWaypoint(x, y, wp, false);
              paintWaypointText(x, y, wp, true /* drawTextDetails */, textatt::ROUTE_BG_COLOR, nullptr);
            }
          }
        }

        if(recNavaids.hasVor())
        {
          const map::MapVor& vor = recNavaids.vors.constFirst();

          // Do not draw related if it was drawn already as a part of a procedure
          if(!idMap.contains(vor.getRef()))
          {
            idMap.insert(vor.getRef());
            if(wToSBuf(vor.position, x, y, MARGINS))
            {
              paintVor(x, y, vor, false);
              paintVorText(x, y, vor, true /* drawTextDetails */, textatt::ROUTE_BG_COLOR, nullptr);
            }
          }
        }

        if(recNavaids.hasNdb())
        {
          const map::MapNdb& ndb = recNavaids.ndbs.constFirst();

          // Do not draw related if it was drawn already as a part of a procedure
          if(!idMap.contains(ndb.getRef()))
          {
            idMap.insert(ndb.getRef());
            if(wToSBuf(ndb.position, x, y, MARGINS))
            {
              paintNdb(x, y, ndb, false);
              paintNdbText(x, y, ndb, true /* drawTextDetails */, textatt::ROUTE_BG_COLOR, nullptr);
            }
          }
        }
      } // if(!routeLeg.getProcedureLeg().isMissed() || context->objectTypes.testFlag(map::MISSED_APPROACH))
    } // if(type == map::PROCEDURE)
  } // for(int i = passedRouteLeg; i < route.size(); i++)
}

void MapPainterRoute::paintRouteInternal(QStringList routeTexts, QVector<Line> lines, int passedRouteLeg)
{
  const static QMargins MARGINS(100, 100, 100, 100);

  const Route *route = context->route;
  Marble::GeoPainter *painter = context->painter;
  const OptionData& od = OptionData::instance();
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBackground(od.getFlightplanOutlineColor());
  painter->setBrush(Qt::NoBrush);

  // Draw route lines ==========================================================================
  float outerlinewidth = context->szF(context->thicknessFlightplan, 7);
  float innerlinewidth = context->szF(context->thicknessFlightplan, 4);

  int destAptIdx = route->getDestinationAirportLegIndex();

  if(!lines.isEmpty())
  {
    // Do not draw a line from runway end to airport center
    if(route->hasAnyApproachProcedure() && destAptIdx != map::INVALID_INDEX_VALUE)
    {
      lines[destAptIdx - 1] = Line();
      routeTexts[destAptIdx - 1].clear();
    }

    // Do not draw a line from airport center to runway end
    if(route->hasAnySidProcedure())
    {
      lines.first() = Line();
      routeTexts.first().clear();
    }

    bool transparent = context->flags2.testFlag(opts2::MAP_ROUTE_TRANSPARENT);
    float alpha = transparent ? (1.f - context->transparencyFlightplan) : 1.f;
    float lineWidth = transparent ? outerlinewidth : innerlinewidth;
    bool drawAlternate = context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE);

    const QPen routePen(mapcolors::adjustAlphaF(od.getFlightplanColor(), alpha), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen routeOutlinePen(od.getFlightplanOutlineColor(), outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen routeAlternateOutlinePen(od.getFlightplanOutlineColor(), outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QPen routePassedPen(mapcolors::adjustAlphaF(od.getFlightplanPassedSegmentColor(), alpha), lineWidth,
                              Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    int alternateOffset = route->getAlternateLegsOffset();

    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      int passed = passedRouteLeg > 0 ? passedRouteLeg - 1 : 0;
      // Draw gray line for passed legs
      painter->setPen(routePassedPen);
      for(int i = 0; i < passed; i++)
        drawLine(painter, lines.at(i));

      if(!transparent)
      {
        if(alternateOffset != map::INVALID_INDEX_VALUE && drawAlternate)
        {
          painter->setPen(routeAlternateOutlinePen);
          for(int idx = alternateOffset; idx < alternateOffset + route->getNumAlternateLegs(); idx++)
            drawLine(painter, lines.at(idx - 1));
        }

        // Draw background for legs ahead
        painter->setPen(routeOutlinePen);
        for(int i = passed; i < route->getDestinationAirportLegIndex(); i++)
          drawLine(painter, lines.at(i));
      }

      // Draw center line for legs ahead
      painter->setPen(routePen);
      for(int i = passed; i < destAptIdx; i++)
        drawLine(painter, lines.at(i));

      // Draw center line for alternates all from destination airport to each alternate
      if(alternateOffset != map::INVALID_INDEX_VALUE && drawAlternate)
      {
        mapcolors::adjustPenForAlternate(painter);
        for(int idx = alternateOffset; idx < alternateOffset + route->getNumAlternateLegs(); idx++)
          drawLine(painter, lines.at(idx - 1));
      }
    }

    if(route->isActiveValid() && context->flags2.testFlag(opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE))
    {
      int activeRouteLeg = route->getActiveLegIndex();

      // Draw active leg on top of all others to keep it visible ===========================
      if(!transparent)
      {
        painter->setPen(routeOutlinePen);
        drawLine(painter, lines.at(activeRouteLeg - 1));
      }

      painter->setPen(QPen(mapcolors::adjustAlphaF(flightplanActiveColor(), alpha), lineWidth,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      drawLine(painter, lines.at(activeRouteLeg - 1));
    }
  }
  context->szFont(context->textSizeFlightplan * context->mapLayerRoute->getRouteFontScale());

  // Collect coordinates for text placement and lines first ============================
  LineString positions;
  for(int i = 0; i < route->size(); i++)
    positions.append(route->value(i).getPosition());

  painter->save();
  if(!context->flags2.testFlag(opts2::MAP_ROUTE_TEXT_BACKGROUND))
    painter->setBackgroundMode(Qt::TransparentMode);
  else
    painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setBackground(mapcolors::routeTextBackgroundColor);
  painter->setPen(mapcolors::routeTextColor);

  if(context->mapLayerRoute->isRouteTextAndDetail())
  {
    // Do not draw text on the color/black outline - on center only if transparent line or text background selected
    bool textOnLineCenter = context->flags2.testFlag(opts2::MAP_ROUTE_TEXT_BACKGROUND) ||
                            context->flags2.testFlag(opts2::MAP_ROUTE_TRANSPARENT);

    // Use a text placement configuration without screen buffer to have labels moving correctly
    TextPlacement textPlacement(painter, this, context->screenRect);
    textPlacement.setMinLengthForText(painter->fontMetrics().averageCharWidth() * 2);
    textPlacement.setDrawFast(context->drawFast);
    textPlacement.setLineWidth(outerlinewidth);
    textPlacement.setTextOnLineCenter(textOnLineCenter);
    textPlacement.calculateTextPositions(positions);
    textPlacement.calculateTextAlongLines(lines, routeTexts);
    textPlacement.drawTextAlongLines();
  }

  // ================================================================================
  // Separate text placement object with screen buffer to avoid navaids popping out at screen edges
  TextPlacement textPlacementBuf(painter, this, context->screenRect.marginsAdded(MARGINS));
  textPlacementBuf.setMinLengthForText(painter->fontMetrics().averageCharWidth() * 2);
  textPlacementBuf.setDrawFast(context->drawFast);
  textPlacementBuf.setLineWidth(outerlinewidth);
  textPlacementBuf.calculateTextPositions(positions);
  bool drawAlternate = context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE);

  QBitArray visibleStartPointsBuf = textPlacementBuf.getVisibleStartPoints();
  for(int i = 0; i < route->size(); i++)
  {
    // Make all approach points except the last one invisible to avoid text and symbol overlay over approach
    // Also clear hidden alternates
    const RouteLeg& leg = route->getLegAt(i);
    if(leg.isAnyProcedure() || (leg.isAlternate() && !drawAlternate))
      visibleStartPointsBuf.clearBit(i);
  }

  for(int i = 0; i < route->size(); i++)
  {
    // Do not draw text for passed waypoints
    if(i + 1 < passedRouteLeg)
      visibleStartPointsBuf.clearBit(i);
  }
  // Draw initial and final course arrows but not for VORs
  if(context->mapLayerRoute->isRouteTextAndDetail2())
    paintInboundOutboundTexts(textPlacementBuf, passedRouteLeg, false /* vor */);

  painter->restore();
  painter->setBackgroundMode(Qt::OpaqueMode);

  // Draw airport and navaid symbols
  drawSymbols(visibleStartPointsBuf, textPlacementBuf.getStartPoints(), false /* preview */);

  // Draw symbol text
  drawRouteSymbolText(visibleStartPointsBuf, textPlacementBuf.getStartPoints());

  if(context->mapLayerRoute->isRouteTextAndDetail2())
  {
    // Draw initial and final course arrows but VOR only to have text over VOR symbols
    painter->save();
    if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
      painter->setBackgroundMode(Qt::TransparentMode);
    else
      painter->setBackgroundMode(Qt::OpaqueMode);

    painter->setBackground(mapcolors::routeTextBackgroundColor);
    painter->setPen(mapcolors::routeTextColor);
    paintInboundOutboundTexts(textPlacementBuf, passedRouteLeg, true /* vor */);
    painter->restore();
  }

  if(context->objectDisplayTypes.testFlag(map::WIND_BARBS_ROUTE) && NavApp::getWindReporter()->hasAnyWindData())
    paintWindBarbs(visibleStartPointsBuf, textPlacementBuf.getStartPoints());
}

float MapPainterRoute::sizeForRouteType(const MapLayer *layer, const RouteLeg& leg)
{
  map::MapTypes type = leg.getMapType();
  if(type == map::PROCEDURE)
  {
    const MapProcedureLeg& procedureLeg = leg.getProcedureLeg();
    if(procedureLeg.navaids.hasVor())
      type = map::VOR;
    else if(procedureLeg.navaids.hasNdb())
      type = map::NDB;
    else if(procedureLeg.navaids.hasWaypoints())
      type = map::WAYPOINT;
  }

  if(type == map::AIRPORT)
    return context->szF(context->symbolSizeAirport, layer->getAirportSymbolSize());
  else if(type == map::NDB)
    return context->szF(context->symbolSizeNavaid, layer->getNdbSymbolSize()) * 1.2f;
  else if(type == map::VOR)
    return context->szF(context->symbolSizeNavaid, std::max(layer->getVorSymbolSizeRoute(), layer->getVorSymbolSizeLarge()));

  // Use waypoint as fallback
  return context->szF(context->symbolSizeNavaid, layer->getWaypointSymbolSize());
}

void MapPainterRoute::paintInboundOutboundTexts(const TextPlacement& textPlacement, int passedRouteLeg, bool vor)
{
  const static QMargins MARGINS(100, 100, 100, 100); // Avoid pop out at screen borders
  const Route *route = context->route;
  Marble::GeoPainter *painter = context->painter;

  // Collect options
  bool magCourse = context->dOptRoute(optsd::ROUTE_INITIAL_FINAL_MAG_COURSE);
  bool trueCourse = context->dOptRoute(optsd::ROUTE_INITIAL_FINAL_TRUE_COURSE);

  if(!magCourse && !trueCourse)
    // Nothing in options selected
    return;

  // Make text a bit smaller
  context->szFont(context->textSizeFlightplan * 0.85f * context->mapLayerRoute->getRouteFontScale());

  QFontMetricsF metrics = painter->fontMetrics();
  float arrowWidth = textPlacement.getArrowWidth(); // Arrows added in text placement

  int lastDepartureLeg = route->getLastIndexOfDepartureProcedure();
  int lastRouteLeg = route->getDestinationIndexBeforeProcedure();
  for(int i = 1; i < route->size(); i++)
  {
    if(i < passedRouteLeg)
      continue;

    // Leg where where the outbound text from last waypoint and inbound text to leg's waypoint is drawn
    const RouteLeg& curLeg = route->value(i);
    const RouteLeg& lastLeg = route->value(i - 1);

    if(vor && !curLeg.isCalibratedVor() && !lastLeg.isCalibratedVor())
      continue;

    if(i <= lastDepartureLeg || i > lastRouteLeg)
      continue;

    // Get first and last positions as well as visibility ============================
    bool firstHidden, lastHidden;
    QPointF first1, last1;
    bool firstVisible1 = wToSBuf(lastLeg.getPosition(), first1, MARGINS, &firstHidden);
    bool lastVisible1 = wToSBuf(curLeg.getPosition(), last1, MARGINS, &lastHidden);

    if(firstHidden || lastHidden)
      // Return if any behind the globe
      continue;

    // Get final course and build text - Inbound to this curLeg navaid =====================
    float inboundTrueBearing, inboundMagBearing;
    route->getInboundCourse(i, inboundMagBearing, inboundTrueBearing);
    QString inboundText = formatter::courseTextNarrow(magCourse ? inboundMagBearing : map::INVALID_COURSE_VALUE,
                                                      trueCourse ? inboundTrueBearing : map::INVALID_COURSE_VALUE);

    // Get initial course and build text - Outbound from lastLeg navaid =====================
    float outboundTrueBearing, outboundMagBearing;
    route->getOutboundCourse(i, outboundMagBearing, outboundTrueBearing);
    QString outboundText = formatter::courseTextNarrow(magCourse ? outboundMagBearing : map::INVALID_COURSE_VALUE,
                                                       trueCourse ? outboundTrueBearing : map::INVALID_COURSE_VALUE);

    if(outboundText.isEmpty() && inboundText.isEmpty())
      continue;

    // Extend towards the center to calculate rotation later - needed for spherical projection ============================
    Line line(lastLeg.getPosition(), curLeg.getPosition());
    QPointF first2, last2;
    float lengthToMeter = atools::geo::nmToMeter(curLeg.getDistanceTo());
    float fraction = scale->getMeterPerPixel() * 20.f / lengthToMeter; // Calculate fraction for roughly 20 pixels distance
    wToSBuf(line.interpolate(lengthToMeter, fraction), first2, MARGINS, &firstHidden);
    wToSBuf(line.interpolate(lengthToMeter, 1.f - fraction), last2, MARGINS, &lastHidden);

    if(firstHidden || lastHidden)
      // Return if any behind the globe
      continue;

#ifdef DEBUG_INFORMATION_BEARING
    if(!outboundText.isEmpty())
      outboundText += "[O" + QString::number(i) + curLeg.getIdent() + "]";
    if(!inboundText.isEmpty())
      inboundText += "[I" + QString::number(i) + curLeg.getIdent() + "]";
#endif

    // Get text line lengths
    QLineF outboundTextLine(first1, first2), inboundTextLine(last1, last2);
    double lineLength = QLineF(outboundTextLine.p1(), inboundTextLine.p1()).length();
    double outboundTextLength = metrics.horizontalAdvance(outboundText) + arrowWidth;
    double inboundTextLength = metrics.horizontalAdvance(inboundText) + arrowWidth;

    // Check if texts fit along line
    if(outboundTextLength + inboundTextLength < lineLength * (vor ? 0.75 : 0.5))
    {
      // Get maximum symbol size to avoid overlap with text
      // Outbound from lastLeg navaid  =================================
      if(!outboundText.isEmpty() && vor == lastLeg.isCalibratedVor() && firstVisible1)
      {

        // Draw blue if related to VOR
        painter->setPen(lastLeg.isCalibratedVor() ? mapcolors::vorSymbolColor : mapcolors::routeTextColorGray);

        // Move p2 by setting length to get accurate text center - sizeForRouteType is distance to navaid
        outboundTextLine.setLength(outboundTextLength + sizeForRouteType(context->mapLayerRoute, lastLeg) + 5.);

        // Draw texts
        float rotate = static_cast<float>(atools::geo::angleFromQt(outboundTextLine.angle()));
        textPlacement.drawTextAlongOneLine(outboundText, rotate, outboundTextLine.center(), 10000);
      }

      // Inbound to curLeg navaid  ==================================
      if(!inboundText.isEmpty() && vor == curLeg.isCalibratedVor() && lastVisible1)
      {
        // Draw blue if related to VOR
        painter->setPen(curLeg.isCalibratedVor() ? mapcolors::vorSymbolColor : mapcolors::routeTextColorGray);

        // Move p2 by setting length to get accurate text center - sizeForRouteType is distance to navaid
        inboundTextLine.setLength(inboundTextLength + sizeForRouteType(context->mapLayerRoute, curLeg) + 5.);

        // Rotate + 180 since line is from end to interpolated end
        float rotate = static_cast<float>(atools::geo::angleFromQt(inboundTextLine.angle() + 180.));
        textPlacement.drawTextAlongOneLine(inboundText, rotate, inboundTextLine.center(), 10000);
      }
    } // if(outboundTextLength + inboundTextLength < lineLength * 0.75)
  } // for(int i = 1; i < route->size(); i++)
}

void MapPainterRoute::paintTopOfDescentAndClimb()
{
  const static QMargins MARGINS(50, 10, 10, 10);
  const textatt::TextAttributes TEXT_ATTS = textatt::ROUTE_BG_COLOR | textatt::PLACE_BELOW_RIGHT;
  const Route *route = context->route;
  if(route->getSizeWithoutAlternates() >= 2)
  {
    atools::util::PainterContextSaver saver(context->painter);

    float width = context->szF(context->symbolSizeNavaid, 3.f);
    float radius = context->szF(context->symbolSizeNavaid, 6.f);

    context->painter->setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
    context->painter->setBrush(Qt::transparent);
    context->szFont(context->textSizeFlightplan * context->mapLayerRoute->getRouteFontScale());

    int activeLegIndex = route->getActiveLegIndex();

    // Use margins for text placed on the right side of the object to avoid disappearing at the left screen border
    bool drawTextDetails = context->mapLayerRoute->isApproachText();
    // Draw the top of climb circle and text ======================
    if(!(context->flags2.testFlag(opts2::MAP_ROUTE_DIM_PASSED)) ||
       activeLegIndex == map::INVALID_INDEX_VALUE || route->getTopOfClimbLegIndex() > activeLegIndex - 1)
    {
      Pos pos = route->getTopOfClimbPos();
      if(pos.isValid())
      {
        float x, y;
        if(wToSBuf(pos, x, y, MARGINS))
        {
          context->painter->drawEllipse(QPointF(x, y), radius, radius);

          QStringList toc;
          toc.append(tr("TOC"));
          if(context->mapLayerRoute->isAirportRouteInfo())
            toc.append(Unit::distNm(route->getTopOfClimbDistance()));

          paintText(mapcolors::routeTextColor, x, y, radius * 2.f, drawTextDetails, toc, TEXT_ATTS);
        }
      }
    }

    // Draw the top of descent circle and text ======================
    if(!(context->flags2.testFlag(opts2::MAP_ROUTE_DIM_PASSED)) ||
       activeLegIndex == map::INVALID_INDEX_VALUE || route->getTopOfDescentLegIndex() > activeLegIndex - 1)
    {
      Pos pos = route->getTopOfDescentPos();
      if(pos.isValid())
      {
        float x, y;
        if(wToSBuf(pos, x, y, MARGINS))
        {
          context->painter->drawEllipse(QPointF(x, y), radius, radius);

          QStringList tod;
          tod.append(tr("TOD"));
          if(context->mapLayerRoute->isAirportRouteInfo())
            tod.append(Unit::distNm(route->getTopOfDescentFromDestination()));

          paintText(mapcolors::routeTextColor, x, y, radius * 2.f, drawTextDetails, tod, TEXT_ATTS);
        }
      }
    }
  }
}

/* Draw approaches and transitions selected in the tree view */
void MapPainterRoute::paintProcedure(QSet<map::MapRef>& idMap, const proc::MapProcedureLegs& legs, int legsRouteOffset, const QColor& color,
                                     bool preview, bool previewAll)
{
  if(legs.isEmpty() || !legs.bounding.overlaps(context->viewportRect))
    return;

  QPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(context->painter);
  const Route *route = context->route;
  const OptionData& od = OptionData::instance();

  float outerlinewidth = context->szF(context->thicknessFlightplan, 7);
  float innerlinewidth = context->szF(context->thicknessFlightplan, 4);
  bool transparent = context->flags2.testFlag(opts2::MAP_ROUTE_TRANSPARENT);

  float lineWidth = transparent ? outerlinewidth : innerlinewidth;
  if(previewAll)
    // Use smaller lines for multi preview
    lineWidth /= 2.f;
  float alpha = transparent ? (1.f - context->transparencyFlightplan) : 1.f;

  context->painter->setBackgroundMode(Qt::OpaqueMode);
  context->painter->setBackground(mapcolors::adjustAlphaF(Qt::white, alpha));
  context->painter->setBrush(Qt::NoBrush);

  // Get active approach leg
  bool activeValid = route->isActiveValid() && !preview && !previewAll;
  int activeProcLeg = activeValid ? route->getActiveLegIndex() - legsRouteOffset : 0;
  int passedProcLeg = context->flags2.testFlag(opts2::MAP_ROUTE_DIM_PASSED) ? activeProcLeg : 0;

  // Draw black background ========================================

  // Keep a stack of last painted geometry since some functions need access back into the history
  QVector<QLineF> lastLines({QLineF()});
  QVector<QLineF> lastActiveLines({QLineF()});

  if(!transparent)
  {
    painter->setPen(QPen(od.getFlightplanOutlineColor(), outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for(int i = 0; i < legs.size(); i++)
    {
      // Do not draw background for passed legs but calculate lastLine
      bool draw = (i >= passedProcLeg) || !activeValid || preview;
      if(legs.at(i).isCircleToLand() || legs.at(i).isStraightIn() || legs.at(i).isVectors() || legs.at(i).isManual())
        // Do not draw outline for circle-to-land approach legs
        draw = false;

      paintProcedureSegment(legs, i, lastLines, nullptr, true /* no text */, previewAll, draw);
    }
  }

  const QPen missedPen(mapcolors::adjustAlphaF(color, alpha), lineWidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen apprPen(mapcolors::adjustAlphaF(color, alpha), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  const QPen missedActivePen(mapcolors::adjustAlphaF(flightplanActiveProcColor(), alpha), lineWidth,
                             Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen procedureActivePen(mapcolors::adjustAlphaF(flightplanActiveProcColor(), alpha), lineWidth,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen routePassedPen(mapcolors::adjustAlphaF(od.getFlightplanPassedSegmentColor(), alpha), lineWidth,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  lastLines.clear();
  lastLines.append(QLineF());
  QVector<DrawText> drawTextLines, lastActiveDrawTextLines;
  drawTextLines.fill({Line(), false, false}, legs.size());
  lastActiveDrawTextLines = drawTextLines;

  // Draw segments and collect text placement information in drawTextLines ========================================
  // Need to set font since it is used by drawHold
  context->szFont(context->textSizeFlightplan * context->mapLayerRoute->getRouteFontScale());

  // Paint legs ====================================================
  bool noText = context->drawFast;
  for(int i = 0; i < legs.size(); i++)
  {
    if(i < passedProcLeg && activeValid && !preview)
    {
      noText = true;
      painter->setPen(routePassedPen);
    }
    else
    {
      noText = context->drawFast;
      if(legs.at(i).isMissed())
        painter->setPen(missedPen);
      else
        painter->setPen(apprPen);
    }

    if(i == activeProcLeg)
    {
      // Create snapshot of vectors for drawing the active one on top of others
      lastActiveLines = lastLines;
      lastActiveDrawTextLines = drawTextLines;
    }

    if(legs.at(i).isCircleToLand() || legs.at(i).isStraightIn())
      // Use dashed line for CTL or straight in
      mapcolors::adjustPenForCircleToLand(painter);
    else if(legs.at(i).isVectors())
      mapcolors::adjustPenForVectors(painter);
    else if(legs.at(i).isManual())
      mapcolors::adjustPenForManual(painter);

    // Do not draw active - just fill the geometry arrays
    bool draw = !activeValid || activeProcLeg != i;

    // Paint segment
    paintProcedureSegment(legs, i, lastLines, &drawTextLines, noText, previewAll, draw);
  }

  // Paint active on top of others ====================================================
  if(!preview && activeValid && activeProcLeg >= 0 && route->isActiveProcedure() &&
     atools::inRange(0, legs.size() - 1, activeProcLeg))
  {
    // Use pen for active leg
    painter->setPen(legs.at(activeProcLeg).isMissed() ? missedActivePen : procedureActivePen);

    if(legs.at(activeProcLeg).isCircleToLand() || legs.at(activeProcLeg).isStraightIn())
      // Use dashed line for CTL or straight in
      mapcolors::adjustPenForCircleToLand(painter);
    else if(legs.at(activeProcLeg).isVectors())
      mapcolors::adjustPenForVectors(painter);
    else if(legs.at(activeProcLeg).isManual())
      mapcolors::adjustPenForManual(painter);

    paintProcedureSegment(legs, activeProcLeg, lastActiveLines, &lastActiveDrawTextLines, noText, previewAll, true /* draw */);
  }

  // Draw text along lines only on low zoom factors ========
  if((!preview && context->mapLayerRoute->isRouteTextAndDetail()) || (preview && context->mapLayerRoute->isApproachText()))
  {
    if(!context->drawFast)
    {
      // Build text strings for drawing along the line ===========================
      QStringList approachTexts;
      QVector<Line> lines;
      QVector<QColor> textColors;

      for(int i = 0; i < legs.size(); i++)
      {
        const proc::MapProcedureLeg& leg = legs.at(i);

        if((i < passedProcLeg && activeValid && !preview) || (previewAll && leg.isMissed()))
        {
          // No texts for passed legs and missed approach legs in mult preview
          approachTexts.append(QString());
          lines.append(leg.line);
          textColors.append(Qt::transparent);
        }
        else
        {
          float dist = map::INVALID_DISTANCE_VALUE, courseMag = map::INVALID_COURSE_VALUE, courseTrue = map::INVALID_COURSE_VALUE;
          if(!previewAll) // No line texts for multi preview
          {
            if(drawTextLines.at(i).distance && context->dOptRoute(optsd::ROUTE_DISTANCE))
              dist = leg.calculatedDistance;

            if(drawTextLines.at(i).course && leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE && !leg.noCalcCourseDisplay())
            {
              if(context->dOptRoute(optsd::ROUTE_MAG_COURSE))
                // Use same values for mag - does not make a difference at the small values in procedures
                courseMag = leg.calculatedMagCourse();

              if(context->dOptRoute(optsd::ROUTE_TRUE_COURSE))
                courseTrue = leg.calculatedTrueCourse;
            }

            if(leg.noDistanceDisplay())
              dist = map::INVALID_DISTANCE_VALUE;
            approachTexts.append(buildLegText(dist, courseMag, courseTrue));
          }
          else
            approachTexts.append(QString() /*legs.approachFixIdent*/);

          lines.append(leg.line);
          textColors.append(leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::routeProcedureTextColor);
        }
      }

      if(!context->flags2.testFlag(opts2::MAP_ROUTE_TEXT_BACKGROUND))
        painter->setBackgroundMode(Qt::TransparentMode);
      else
        painter->setBackgroundMode(Qt::OpaqueMode);

      // Draw text along lines ====================================================
      painter->setBackground(previewAll ? QColor(Qt::transparent) : mapcolors::routeTextBackgroundColor);
      if(previewAll)
        // Make the font larger for better arrow visibility in multi preview
        context->szFont(context->textSizeFlightplan * 1.5f * context->mapLayerRoute->getRouteFontScale());

      QVector<Line> textLines;
      LineString positions;

      for(const DrawText& dt : qAsConst(drawTextLines))
      {
        textLines.append(dt.line);
        positions.append(dt.line.getPos1());
      }

      // Do not draw text on the color/black outline - on center only if transparent line or text background selected
      bool textOnLineCenter = context->flags2.testFlag(opts2::MAP_ROUTE_TEXT_BACKGROUND) ||
                              context->flags2.testFlag(opts2::MAP_ROUTE_TRANSPARENT);

      TextPlacement textPlacement(painter, this, context->screenRect);
      textPlacement.setMinLengthForText(painter->fontMetrics().averageCharWidth() * 2);
      textPlacement.setArrowForEmpty(previewAll); // Arrow for empty texts
      textPlacement.setTextOnLineCenter(textOnLineCenter);
      textPlacement.setDrawFast(context->drawFast);
      textPlacement.setTextOnTopOfLine(false); // Allow text below line to avoid cluttering up procedures
      textPlacement.setLineWidth(outerlinewidth);
      textPlacement.setColors(textColors);
      textPlacement.calculateTextPositions(positions);
      textPlacement.calculateTextAlongLines(textLines, approachTexts);
      textPlacement.drawTextAlongLines();
    }

    context->szFont(context->textSizeFlightplan * context->mapLayerRoute->getRouteFontScale());
  }

  // Texts and navaid icons ====================================================
  // Drawn from destination to departure/aircraft
  for(int i = legs.size() - 1; i >= 0; i--)
  {
    int routeIndex = legsRouteOffset + i;
    if(preview || previewAll)
      // No route available for any preview
      routeIndex = -1;

    // No text labels for passed points but always for preview
    int diff = 1; // Need to draw one more for overlapping legs
    bool drawText = i + diff >= passedProcLeg || !activeValid || preview;
    bool drawPoint = i + diff >= passedProcLeg || !activeValid || preview || previewAll;

    if(!context->mapLayerRoute->isApproachText())
      drawText = false;

    if(previewAll)
    {
      // Only endpoint labels for SID in preview
      if(legs.mapType & proc::PROCEDURE_SID_ALL && i < legs.size() - 1)
        drawText = false;

      // Only start- and endpoint labels for STAR in preview
      if(legs.mapType & proc::PROCEDURE_STAR_ALL && (i > 0 && i < legs.size() - 1))
        drawText = false;

      // Only startpoint labels for approaches and transitions in preview
      if(legs.mapType & proc::PROCEDURE_APPROACH_ALL && i > 0)
        drawText = false;
    }

    if(drawPoint)
      paintProcedurePoint(idMap, legs, i, routeIndex, preview, previewAll, drawText);
  } // for(int i = legs.size() - 1; i >= 0; i--)
}

void MapPainterRoute::paintProcedureSegment(const proc::MapProcedureLegs& legs, int index, QVector<QLineF>& lastLines,
                                            QVector<DrawText> *drawTextLines, bool noText, bool previewAll, bool draw)
{
  const static QMargins MARGINS(50, 50, 50, 50);
  const proc::MapProcedureLeg& leg = legs.at(index);

  if(previewAll && leg.isMissed())
    return;

  if(leg.isMissed() && !(context->objectTypes & map::MISSED_APPROACH))
    return;

  if(!leg.line.isValid())
    return;

  const proc::MapProcedureLeg *prevLeg = index > 0 ? &legs.at(index - 1) : nullptr;

  QSize size = scale->getScreeenSizeForRect(legs.bounding);

  // Use visible dummy here since we need to call the method that also returns coordinates outside the screen
  QLineF line;
  bool hidden;
  wToS(leg.line, line, size, &hidden);

  if(leg.disabled)
    return;

  if(leg.isInitialFix())
  {
    // Nothing to do here
    lastLines.append(line);
    return;
  }

  if(leg.type == proc::START_OF_PROCEDURE &&
     // START_OF_PROCEDURE is an actual leg for departure where it connects runway and initial fix
     !(leg.mapType & proc::PROCEDURE_DEPARTURE))
  {
    // Nothing to do here
    lastLines.append(line);
    return;
  }

  QPainter *painter = context->painter;
  // ===========================================================
  // Draw lines simplified if no point is hidden to avoid weird things at high zoom factors in spherical projection
  if(!context->mapLayerRoute->isApproachDetail())
  {
    bool visible1, visible2, hidden1, hidden2;
    wToS(leg.line.getPos1(), DEFAULT_WTOS_SIZE, &visible1, &hidden1);
    wToS(leg.line.getPos2(), DEFAULT_WTOS_SIZE, &visible2, &hidden2);

    if(!hidden1 && !hidden2)
    {
      // QLineF simpleLine(lastLine.p2(), line.p1());
      if(draw)
      {
        if(!lastLines.constLast().p2().isNull() && !line.p1().isNull())
          drawLine(painter, lastLines.constLast().p2(), line.p1());
        if(!line.isNull())
          drawLine(painter, line);
      }
      lastLines.append(line);

      if(drawTextLines != nullptr)
        // Disable all drawing
        (*drawTextLines)[index] = DrawText(leg.line, false, false);
    }
    return;
  }

  QPointF interceptPoint = wToS(leg.interceptPos, size, &hidden);

  bool showDistance = !leg.noDistanceDisplay();

  // ===========================================================
  if(contains(leg.type, {proc::ARC_TO_FIX, proc::CONSTANT_RADIUS_ARC}))
  {
    if(line.length() > 2 && leg.recFixPos.isValid())
    {
      if(draw)
      {
        QPointF point = wToS(leg.recFixPos, size, &hidden);
        if(leg.correctedArc)
        {
          // Arc with stub
          drawLine(painter, line.p1(), interceptPoint);
          paintArc(context->painter, interceptPoint, line.p2(), point, leg.turnDirection == "L");
        }
        else
          paintArc(context->painter, line.p1(), line.p2(), point, leg.turnDirection == "L");
      }
    }
    else
    {
      if(draw)
        drawLine(painter, line);
      lastLines.append(line);

      if(drawTextLines != nullptr)
        // Can draw a label along the line
        (*drawTextLines)[index] = DrawText(leg.line, showDistance, true);
    }

    lastLines.append(line);
  }
  // ===========================================================
  else if(contains(leg.type, {proc::COURSE_TO_ALTITUDE,
                              proc::COURSE_TO_FIX,
                              proc::CUSTOM_APP_RUNWAY,
                              proc::CUSTOM_DEP_END,
                              proc::DIRECT_TO_FIX,
                              proc::VECTORS,
                              proc::FIX_TO_ALTITUDE,
                              proc::TRACK_TO_FIX,
                              proc::TRACK_FROM_FIX_FROM_DISTANCE,
                              proc::FROM_FIX_TO_MANUAL_TERMINATION,
                              proc::HEADING_TO_ALTITUDE_TERMINATION,
                              proc::HEADING_TO_MANUAL_TERMINATION,
                              proc::COURSE_TO_INTERCEPT,
                              proc::HEADING_TO_INTERCEPT,
                              proc::COURSE_TO_RADIAL_TERMINATION,
                              proc::HEADING_TO_RADIAL_TERMINATION}))
  {
    if(
#ifndef DEBUG_DRAW_ALWAYS_BOW
      contains(leg.turnDirection, {"R", "L"}) &&
#endif
      prevLeg != nullptr /*&& !prevLeg->isInitialFix() && prevLeg->type != proc::START_OF_PROCEDURE*/)
    {
      float lineDist = static_cast<float>(QLineF(lastLines.constLast().p2(), line.p1()).length());
      if(!lastLines.constLast().p2().isNull() && lineDist > 2.f)
      {
        // Lines are not connected which can happen if a CF follows after a FD or similar
        paintProcedureBow(prevLeg, lastLines, painter, line, leg, interceptPoint, draw);

        if(drawTextLines != nullptr)
        {
          if(leg.interceptPos.isValid())
            (*drawTextLines)[index] = DrawText(Line(leg.interceptPos, leg.line.getPos2()), false, true);
          else
            // Can draw a label along the line with course but not distance
            (*drawTextLines)[index] = DrawText(leg.line, false, true);
        }
      }
      else
      {
        // Lines are connected but a turn direction is given
        // Draw a small arc if a turn direction is given

        // lastLines gets the full line added and nextLine is the line for text
        QLineF nextLine = paintProcedureTurn(lastLines, line, leg, painter, interceptPoint, draw);

        Pos p1 = sToW(nextLine.p1());
        Pos p2 = sToW(nextLine.p2());

        if(drawTextLines != nullptr)
        {
          // Can draw a label along the line with course but not distance
          if(leg.interceptPos.isValid())
            (*drawTextLines)[index] = DrawText(Line(leg.interceptPos, leg.line.getPos2()), true, true);
          else
            (*drawTextLines)[index] = DrawText(Line(p1, p2), showDistance, true);
        }
      }
    }
    else
    {
      // No turn direction or both

      if(leg.interceptPos.isValid())
      {
        // Intercept a CF leg
        if(draw)
        {
          drawLine(painter, line.p1(), interceptPoint);
          drawLine(painter, interceptPoint, line.p2());
        }
        lastLines.append(QLineF(interceptPoint, line.p2()));

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = DrawText(Line(leg.interceptPos, leg.line.getPos2()), showDistance, true);
      }
      else
      {
        if(draw)
        {
          if(!lastLines.constLast().p2().isNull() && QLineF(lastLines.constLast().p2(), line.p1()).length() > 2)
          {
            if(!(prevLeg != nullptr && (prevLeg->isCircleToLand() || prevLeg->isStraightIn()) && leg.isMissed()))
              // Close any gaps in the lines but not for circle-to-land legs
              drawLine(painter, lastLines.constLast().p2(), line.p1());
          }

          drawLine(painter, line);
        }
        lastLines.append(line);

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = DrawText(leg.line, showDistance, true);
      }
    }
  }
  // ===========================================================
  else if(contains(leg.type, {proc::COURSE_TO_RADIAL_TERMINATION,
                              proc::HEADING_TO_RADIAL_TERMINATION,
                              proc::COURSE_TO_DME_DISTANCE,
                              proc::START_OF_PROCEDURE,
                              proc::HEADING_TO_DME_DISTANCE_TERMINATION,
                              proc::TRACK_FROM_FIX_TO_DME_DISTANCE,
                              proc::DIRECT_TO_RUNWAY,
                              proc::CUSTOM_DEP_RUNWAY,
                              proc::CIRCLE_TO_LAND,
                              proc::STRAIGHT_IN}))
  {
    if(draw)
      drawLine(painter, line);
    lastLines.append(line);

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = DrawText(leg.line, showDistance, true);
  }
  // ===========================================================
  else if(contains(leg.type, {proc::HOLD_TO_ALTITUDE,
                              proc::HOLD_TO_FIX,
                              proc::HOLD_TO_MANUAL_TERMINATION}))
  {
    QString holdText, holdText2;

    float trueCourse = leg.legTrueCourse();

    if(!noText)
    {
      holdText = QString::number(leg.course, 'f', 0) % (leg.trueCourse ? tr("°T") : tr("°M"));

      if(trueCourse < 180.f)
        holdText = holdText % tr(" ►");
      else
        holdText = tr("◄ ") % holdText;

      if(leg.time > 0.f)
        holdText2.append(QString::number(leg.time, 'g', 2) % tr("min"));
      else if(leg.distance > 0.f)
        holdText2 = Unit::distNm(leg.distance, true /*addUnit*/, 20, true /*narrow*/);
      else
        holdText2 = tr("1min");

      if(trueCourse < 180.f)
        holdText2 = tr("◄ ") % holdText2;
      else
        holdText2 = holdText2 % tr(" ►");
    }

    if(draw)
      paintHoldWithText(painter, static_cast<float>(line.x2()), static_cast<float>(line.y2()),
                        trueCourse, leg.distance, leg.time, leg.turnDirection == "L",
                        holdText, holdText2,
                        leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::routeProcedureTextColor,
                        mapcolors::routeTextBackgroundColor);
  }
  // ===========================================================
  else if(leg.type == proc::PROCEDURE_TURN)
  {
    QString text;

    float trueCourse = leg.legTrueCourse();
    if(!noText)
    {
      text = QString::number(leg.course, 'f', 0) % (leg.trueCourse ? tr("°T") : tr("°M")) % tr("/1min");
      if(trueCourse < 180.f)
        text = text % tr(" ►");
      else
        text = tr("◄ ") % text;
    }

    float px, py;
    wToSBuf(leg.procedureTurnPos, px, py, size, MARGINS, &hidden);

    if(draw && !hidden)
    {
      paintProcedureTurnWithText(painter, px, py,
                                 trueCourse, leg.distance,
                                 leg.turnDirection == "L", &lastLines.last(), text,
                                 leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::
                                 routeProcedureTextColor,
                                 mapcolors::routeTextBackgroundColor);

      drawLine(painter, line.p1(), QPointF(px, py));
    }

    if(drawTextLines != nullptr && !hidden)
      // Can draw a label along the line
      (*drawTextLines)[index] = DrawText(Line(leg.line.getPos1(), leg.procedureTurnPos), showDistance, true);
  }
}

void MapPainterRoute::paintProcedureBow(const proc::MapProcedureLeg *prevLeg, QVector<QLineF>& lastLines, QPainter *painter, QLineF line,
                                        const proc::MapProcedureLeg& leg, const QPointF& intersectPoint, bool draw)
{
  if(!prevLeg->line.getPos2().isValid() || !leg.line.getPos1().isValid())
    return;

  bool firstMissed = !prevLeg->isMissed() && leg.isMissed() && (prevLeg->isCircleToLand() || prevLeg->isStraightIn());
  Pos prevPos;
  QPointF lastP1, lastP2;
  bool prevCircle = false;
  if(firstMissed)
  {
    // This is the first missed approach leg after a CTL or straight in
    // Adjust geometry so that missed begins at the MAP
    prevPos = prevLeg->line.getPos1();

    if(lastLines.size() >= 2)
    {
      lastP1 = lastLines.at(lastLines.size() - 2).p1();
      lastP2 = lastLines.at(lastLines.size() - 2).p2();
    }
  }
  else
  {
    prevPos = prevLeg->line.getPos2();
    if(prevLeg->isCircular() && prevLeg->geometry.size() > 1)
    {
      // Consider circle geometry for legs with a simplified geometry
      bool visible = false, hidden = false;
      lastP1 = wToS(prevLeg->geometry.at(prevLeg->geometry.size() - 2), DEFAULT_WTOS_SIZE, &visible, &hidden);
      if(hidden)
        // Ignore calculated point behind globe
        lastP1 = lastLines.constLast().p1();
      else
        prevCircle = true;

      lastP2 = lastLines.constLast().p2();
    }
    else
    {
      lastP1 = lastLines.constLast().p1();
      lastP2 = lastLines.constLast().p2();
    }
  }

  Pos nextPos;
  QLineF lineDraw;
  if(leg.interceptPos.isValid() && !intersectPoint.isNull())
  {
    // Next leg is a intercept leg - adjust geometry
    nextPos = leg.interceptPos;
    lineDraw = QLineF(line.p2(), intersectPoint);
  }
  else
  {
    nextPos = leg.line.getPos1();
    lineDraw = QLineF(line.p2(), line.p1());
  }

  // Calculate distance to control points
  float dist = prevPos.distanceMeterTo(nextPos);

  if(dist < ageo::Pos::INVALID_VALUE)
  {
    // Shorten the next line around 1 NM to get a better curve
    // Use a value less than 1 nm to avoid flickering on 1 nm legs
    // This will start the curve after the first point of the next leg
    float oneNmPixel = scale->getPixelForNm(0.95f);
    if(lineDraw.length() > oneNmPixel * 2.f)
      lineDraw.setLength(lineDraw.length() - oneNmPixel);
    lineDraw.setPoints(lineDraw.p2(), lineDraw.p1());

    dist = dist * 3 / 4;

    // Calculate bezier control points by extending the last and next line
    QLineF ctrl1(lastP1, lastP2);
    QLineF ctrl2(lineDraw.p2(), lineDraw.p1());
    if(prevCircle)
    {
      // Use shorter lines if previous leg is circular to get a smoother display
      ctrl1.setLength(ctrl1.length() + scale->getPixelForMeter(dist / 2.f));
      ctrl2.setLength(ctrl2.length() + scale->getPixelForMeter(dist / 2.f));
    }
    else
    {
      ctrl1.setLength(ctrl1.length() + scale->getPixelForMeter(dist));
      ctrl2.setLength(ctrl2.length() + scale->getPixelForMeter(dist));
    }

#ifdef DEBUG_APPROACH_PAINT
    {
      atools::util::PainterContextSaver context(painter);
      painter->setPen(Qt::black);
      painter->drawLine(ctrl1);
      painter->drawLine(ctrl2);
    }
#endif

    // Draw a bow connecting the two lines
    if(draw)
    {
      QPainterPath path;
      path.moveTo(lastP2);
      path.cubicTo(ctrl1.p2(), ctrl2.p2(), lineDraw.p1());
      painter->drawPath(path);

      drawLine(painter, lineDraw);
    }
    lastLines.append(lineDraw);
  }
}

QLineF MapPainterRoute::paintProcedureTurn(QVector<QLineF>& lastLines, QLineF line, const proc::MapProcedureLeg& leg,
                                           QPainter *painter, const QPointF& intersectPoint, bool draw)
{
  QPointF endPos = line.p2();
  if(leg.interceptPos.isValid())
    endPos = intersectPoint;

  const QLineF& lastLine = lastLines.constLast();

  // The returned value represents the number of degrees you need to add to this
  // line to make it have the same angle as the given line, going counter-clockwise.
  double angleToLastRev = line.angleTo(lastLine);

  // Calculate the angle difference between last and current line
  double diff = angleAbsDiff(normalizeCourse(angleFromQt(lastLine.angle())), normalizeCourse(angleFromQt(line.angle())));

  // Use a bigger extension (modify course more) if the course is a 180 (e.g. 90 and returning to 270)
  float extension = atools::almostEqual(diff, 0., 1.) || atools::almostEqual(diff, 180., 1.) ? 1.f : 0.5f;

  // Calculate the start position of the next line and leave space for the arc
  QLineF arc(line.p1(), QPointF(line.x2(), line.y2() /* + 100.*/));
  arc.setLength(scale->getPixelForNm(extension));
  if(leg.turnDirection == "R")
    arc.setAngle(angleToQt(angleFromQt(QLineF(lastLine.p2(), lastLine.p1()).angle()) + angleToLastRev / 2.) + 180.f);
  else
    arc.setAngle(angleToQt(angleFromQt(QLineF(lastLine.p2(), lastLine.p1()).angle()) + angleToLastRev / 2.));

  // Calculate bezier control points by extending the last and next line
  QLineF ctrl1(lastLine.p1(), lastLine.p2()), ctrl2(endPos, arc.p2());
  ctrl1.setLength(ctrl1.length() + scale->getPixelForNm(extension / 2.f));
  ctrl2.setLength(ctrl2.length() + scale->getPixelForNm(extension / 2.f));

  // Draw the arc
  if(draw)
  {
    QPainterPath path;
    path.moveTo(arc.p1());
    path.cubicTo(ctrl1.p2(), ctrl2.p2(), arc.p2());
    painter->drawPath(path);
  }

  // Draw the next line
  QLineF nextLine(arc.p2(), endPos);
  if(draw)
  {
    drawLine(painter, nextLine);

    if(leg.interceptPos.isValid())
      // Add line from intercept leg
      drawLine(painter, endPos, line.p2());
  }

  // Full line for drawing
  lastLines.append(line);

#ifdef DEBUG_APPROACH_PAINT
  {
    atools::util::PainterContextSaver saver(context->painter);
    painter->setPen(QPen(Qt::blue, 4.f, Qt::DashLine));
    drawLine(painter, nextLine);

    painter->setPen(QPen(Qt::magenta, 4.f, Qt::DashLine));
    drawLine(painter, line);

    painter->setPen(QPen(Qt::cyan, 4.f, Qt::DashLine));
    drawLine(painter, arc);
  }
#endif

  // Line for text
  return nextLine;
}

void MapPainterRoute::paintProcedurePoint(QSet<map::MapRef>& idMap, const proc::MapProcedureLegs& legs, int index, int routeIndex,
                                          bool preview, bool previewAll, bool drawTextFlag)
{
  static const QVector<proc::ProcedureLegType> CALCULATED_END_POS_TYPES(
    {proc::COURSE_TO_ALTITUDE,
     proc::COURSE_TO_DME_DISTANCE,
     proc::COURSE_TO_INTERCEPT,
     proc::COURSE_TO_RADIAL_TERMINATION,
     // proc::HOLD_TO_FIX, proc::HOLD_TO_MANUAL_TERMINATION, proc::HOLD_TO_ALTITUDE,
     proc::FIX_TO_ALTITUDE,
     proc::TRACK_FROM_FIX_TO_DME_DISTANCE,
     proc::HEADING_TO_ALTITUDE_TERMINATION,
     proc::HEADING_TO_DME_DISTANCE_TERMINATION,
     proc::HEADING_TO_INTERCEPT,
     proc::HEADING_TO_RADIAL_TERMINATION,
     proc::TRACK_FROM_FIX_FROM_DISTANCE,
     proc::FROM_FIX_TO_MANUAL_TERMINATION,
     proc::HEADING_TO_MANUAL_TERMINATION});
  const static QMargins MARGINS(50, 10, 50, 20);

  const proc::MapProcedureLeg& leg = legs.at(index);

  // Debugging code for drawing ================================================
#ifdef DEBUG_APPROACH_PAINT

  {
    QColor col(255, 0, 0, 50);

    bool hiddenDummy;
    QSize size = scale->getScreeenSizeForRect(legs.bounding);

    QLineF line, holdLine;
    wToS(leg.line, line, size, &hiddenDummy);
    wToS(leg.holdLine, holdLine, size, &hiddenDummy);
    QPainter *painter = context->painter;

    if(leg.disabled)
      painter->setPen(Qt::red);

    QPointF intersectPoint = wToS(leg.interceptPos, DEFAULT_WTOS_SIZE, &hiddenDummy);

    painter->save();
    painter->setPen(QPen(Qt::blue, 3.));
    painter->drawEllipse(line.p1(), 20, 10);
    painter->drawEllipse(line.p2(), 10, 20);
    painter->drawText(line.x1() - 60, line.y1() + (index % 3) * 10, "Start " + proc::procedureLegTypeShortStr(
                        leg.type) + " " + QString::number(index));

    painter->drawText(line.x2() - 60, line.y2() + (index % 3) * 10 + 20, "End " + proc::procedureLegTypeShortStr(
                        leg.type) + " " + QString::number(index));
    if(!intersectPoint.isNull())
    {
      painter->drawEllipse(intersectPoint, 30, 30);
      painter->drawText(intersectPoint.x() - 60, intersectPoint.y() + (index % 3) * 10 + 30,
                        proc::procedureLegTypeShortStr(leg.type) + " " + QString::number(index));
    }

    painter->setPen(QPen(col, 20));
    for(int i = 0; i < leg.geometry.size() - 1; i++)
    {
      QPoint pt1 = wToS(leg.geometry.at(i), size, &hiddenDummy);
      QPoint pt2 = wToS(leg.geometry.at(i + 1), size, &hiddenDummy);

      drawLine(painter, pt1, pt2);
    }

    painter->setPen(QPen(QColor(0, 0, 255, 100), 5));
    drawLine(painter, holdLine);

    painter->setBackground(Qt::transparent);
    painter->setPen(QPen(QColor(0, 255, 0, 255), 3, Qt::DotLine));
    drawLine(painter, line);

    painter->setPen(QPen(col, 10));
    for(int i = 0; i < leg.geometry.size(); i++)
      painter->drawPoint(wToS(leg.geometry.at(i)));
    painter->restore();
  }
#endif

  if(previewAll && leg.isMissed())
    return;

  if(leg.isMissed() && !(context->objectTypes & map::MISSED_APPROACH))
    // If missed is hidden on route do not display it
    return;

  if(leg.disabled)
    return;

  bool drawText = context->mapLayerRoute->isApproachText() && drawTextFlag;
  bool drawTextDetails = !previewAll && drawTextFlag && context->mapLayerRoute->isApproachTextDetails();
  bool drawUnderlay = drawText && !previewAll && context->mapLayerRoute->isApproachDetail();

  proc::MapAltRestriction altRestr(leg.altRestriction);
  proc::MapSpeedRestriction speedRestr(leg.speedRestriction);
  bool lastInTransition = false;

  if(index < legs.size() - 1 &&
     leg.type != proc::HEADING_TO_INTERCEPT && leg.type != proc::COURSE_TO_INTERCEPT &&
     leg.type != proc::HOLD_TO_ALTITUDE && leg.type != proc::HOLD_TO_FIX &&
     leg.type != proc::HOLD_TO_MANUAL_TERMINATION)
    // Do not paint the last point in the transition since it overlaps with the approach
    // But draw the intercept and hold text
    lastInTransition = leg.isTransition() && legs.at(index + 1).isApproach();

  if(index < legs.size() - 1 && lastInTransition)
  {
    // Do not draw transitions from this last transition leg - merge and draw them for the first approach leg
    altRestr = proc::MapAltRestriction();
    speedRestr = proc::MapSpeedRestriction();
  }

  // Get text placement sector based on inbound and outbound leg courses
  textatt::TextAttributes textPlacementAtts = textatt::ROUTE_BG_COLOR;
  if(!preview && !previewAll)
    textPlacementAtts = textPlacementAttributes(routeIndex);

  QStringList texts, restrTexts;
#ifdef DEBUG_INFORMATION_PROCEDURE_INDEX
  texts.append(QString("#%1 ").arg(index + legsRouteOffset));
#endif

  float x = 0, y = 0;

  // Margins for text at left (VOR), right (waypoints) and below (NDB)
  float defaultOverflySize = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());
  if(leg.mapType == proc::PROCEDURE_SID && index == 0)
  {
    // All legs with a calculated end point - runway =====================
    if(wToSBuf(leg.line.getPos1(), x, y, MARGINS))
    {
      texts.append("RW" % legs.runwayEnd.name);

      if(drawText)
      {
        proc::MapAltRestriction altRestriction;
        altRestriction.descriptor = proc::MapAltRestriction::AT;
        altRestriction.alt1 = legs.runwayEnd.getAltitude();
        altRestriction.alt2 = 0.f;
        restrTexts.append(proc::altRestrictionTextNarrow(altRestriction));
      }

      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, defaultOverflySize);
      paintProcedurePoint(x, y, false /* preview */);
      if(drawText)
      {
        texts.append(restrTexts);
        paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
        restrTexts.clear();
        texts.clear();
      }
    }

    // =================================
    return;
  }

  // Manual and altitude terminated legs that have a calculated position needing extra text
  if(contains(leg.type, {proc::COURSE_TO_ALTITUDE,
                         proc::COURSE_TO_DME_DISTANCE,
                         proc::COURSE_TO_INTERCEPT,
                         proc::COURSE_TO_RADIAL_TERMINATION,
                         proc::HOLD_TO_FIX,
                         proc::HOLD_TO_MANUAL_TERMINATION,
                         proc::HOLD_TO_ALTITUDE,
                         proc::FIX_TO_ALTITUDE,
                         proc::TRACK_FROM_FIX_TO_DME_DISTANCE,
                         proc::HEADING_TO_ALTITUDE_TERMINATION,
                         proc::HEADING_TO_DME_DISTANCE_TERMINATION,
                         proc::HEADING_TO_INTERCEPT,
                         proc::HEADING_TO_RADIAL_TERMINATION,
                         proc::TRACK_FROM_FIX_FROM_DISTANCE,
                         proc::FROM_FIX_TO_MANUAL_TERMINATION,
                         proc::HEADING_TO_MANUAL_TERMINATION}))
  {
    if(lastInTransition)
      // =================================
      return;

    // All legs with a calculated end point
    if(wToSBuf(leg.line.getPos2(), x, y, MARGINS))
    {
      if(drawText)
      {
        texts.append(leg.displayText);
        restrTexts.append(proc::altRestrictionTextNarrow(altRestr));
        restrTexts.append(proc::speedRestrictionTextNarrow(speedRestr));
      }

      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, defaultOverflySize);
      paintProcedurePoint(x, y, false);

      if(drawText)
      {
        texts.append(restrTexts);
        paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
        restrTexts.clear();
        texts.clear();
      }
    }
  }
  else if(leg.type == proc::START_OF_PROCEDURE)
  {
    if(wToSBuf(leg.line.getPos1(), x, y, MARGINS))
      paintProcedurePoint(x, y, false);
  }
  else if(leg.type == proc::COURSE_TO_FIX || leg.type == proc::CUSTOM_APP_RUNWAY || leg.type == proc::CUSTOM_DEP_END)
  {
    if(index == 0)
    {
      if(wToSBuf(leg.line.getPos1(), x, y, MARGINS))
      {
        if(drawUnderlay)
          paintProcedureUnderlay(leg, x, y, defaultOverflySize);
        paintProcedurePoint(x, y, false);
      }
    }
    else if(leg.interceptPos.isValid())
    {
      if(wToSBuf(leg.interceptPos, x, y, MARGINS))
      {
        // Draw intercept comment - no altitude restriction and no underlay there
        texts.append(leg.displayText);

        paintProcedurePoint(x, y, false);
        if(drawText)
          paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
      }

      // Clear text from "intercept" and add restrictions
      if(drawText)
      {
        texts.clear();
        restrTexts.clear();
        restrTexts.append(proc::altRestrictionTextNarrow(altRestr));
        restrTexts.append(proc::speedRestrictionTextNarrow(speedRestr));
      }
    }
    else
    {
      if(lastInTransition)
        // =========================================================
        return;

      if(drawText)
      {
        texts.append(leg.displayText);
        restrTexts.append(proc::altRestrictionTextNarrow(altRestr));
        restrTexts.append(proc::speedRestrictionTextNarrow(speedRestr));
      }
    }
  }
  else
  {
    if(lastInTransition)
      // =========================================================
      return;

    texts.append(leg.displayText);
    if(drawText)
    {
      restrTexts.append(proc::altRestrictionTextNarrow(altRestr));
      restrTexts.append(proc::speedRestrictionTextNarrow(speedRestr));
    }
  }

  // Merge restriction texts only for flight plan - not for previews
  if(drawText && routeIndex != -1)
  {
    // Loop moves from prevLeg to leg to nextLeg to destination
    // Legs are drawn reversed from route.size() (destination) to 0 (departure)
    // +1 is closer to the destination
    // Loop moves from prevLeg to leg to nextLeg to destination
    const proc::MapProcedureLeg& prevLeg = context->route->getLegAt(routeIndex - 1).getProcedureLeg();

    // Merge texts between approach and transition fixes ========================================
    if(prevLeg.isAlmostEqual(leg) && prevLeg.isTransition() && leg.isApproach())
    {
      // Merge restriction texts from last transition and this approach leg
      // Transition point is never drawn since method returns above
      restrTexts.append(proc::altRestrictionTextNarrow(prevLeg.altRestriction));
      restrTexts.append(proc::speedRestrictionTextNarrow(prevLeg.speedRestriction));
    }
    else
    {
      // Kittila (EFKT) Arrive using STAR EVIM2A and ILS-Z BENUG
      // Index moving in direction ^
      // . 0      ^ EFKE;Departure
      // . 1      ^ OLNOP
      // . 2      ^ EVIMI;STAR EVIM2A;Initial fix;A 10.000
      // . 3 prev ^ XIVPA;STAR EVIM2A;Track to fix;A 3.800                  >>>>>>>>>> OMIT
      // . 4 leg  ^ XIVPA (IAF);Approach ILS-Z BENUG;Initial fix;GS 2.500   >>>>>>>>>> DRAW
      // . 5 next ^ BENUG (FAF);Approach ILS-Z BENUG;Course to fix;A 2.500
      // . 6      ^ RW34 (MAP);Approach ILS-Z BENUG;Course to fix;703

      // Merge restriction label texts when transitioning from SID to approach transition, for example
      if(leg.isAlmostEqual(prevLeg) && !contains(prevLeg.type, CALCULATED_END_POS_TYPES))
      {
        // Add restrictions from last point to text
        restrTexts.append(proc::altRestrictionTextNarrow(prevLeg.altRestriction));
        restrTexts.append(proc::speedRestrictionTextNarrow(prevLeg.speedRestriction));
      }

      // Omit drawing of double labels when transitioning from SID to approach transition, for example
      const proc::MapProcedureLeg& nextLeg = context->route->getLegAt(routeIndex + 1).getProcedureLeg();
      if(nextLeg.isAlmostEqual(leg) && !contains(nextLeg.type, CALCULATED_END_POS_TYPES))
        // Draw symbol but not labels
        drawText = false;
    }
  }

  // Add restriction texts to additional texts beside navaids
  texts.append(restrTexts);

  // Remove duplicates and empty strings
  texts.removeAll(QString());
  texts.removeDuplicates();

  const map::MapResult& navaids = leg.navaids;
  float symbolSizeWaypoint = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());

  if(!navaids.waypoints.isEmpty() && wToSBuf(navaids.waypoints.constFirst().position, x, y, MARGINS))
  {
    const map::MapWaypoint& wp = navaids.waypoints.constFirst();
    if(!idMap.contains(wp.getRef()))
    {
      idMap.insert(wp.getRef());
      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, symbolSizeWaypoint);
      paintWaypoint(x, y, wp, false);
      if(drawText)
        paintWaypointText(x, y, wp, drawTextDetails, textPlacementAtts, &texts);
    }
  }
  else if(!navaids.vors.isEmpty() && wToSBuf(navaids.vors.constFirst().position, x, y, MARGINS))
  {

    const map::MapVor& vor = navaids.vors.constFirst();
    if(!idMap.contains(vor.getRef()))
    {
      idMap.insert(vor.getRef());
      float symbolSizeVor = context->sz(context->symbolSizeNavaid, context->mapLayerRoute->getVorSymbolSize());
      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, symbolSizeVor);
      paintVor(x, y, vor, false);
      if(drawText)
        paintVorText(x, y, vor, drawTextDetails, textPlacementAtts, &texts);
    }
  }
  else if(!navaids.ndbs.isEmpty() && wToSBuf(navaids.ndbs.constFirst().position, x, y, MARGINS))
  {

    const map::MapNdb& ndb = navaids.ndbs.constFirst();
    if(!idMap.contains(ndb.getRef()))
    {
      idMap.insert(ndb.getRef());
      float symbolSizeNdb = context->sz(context->symbolSizeNavaid, context->mapLayerRoute->getNdbSymbolSize());
      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, symbolSizeNdb);
      paintNdb(x, y, ndb, false);
      if(drawText)
        paintNdbText(x, y, ndb, drawTextDetails, textPlacementAtts, &texts);
    }
  }
  else if(!navaids.ils.isEmpty() && wToSBuf(navaids.ils.constFirst().position, x, y, MARGINS))
  {
    const map::MapIls& ils = navaids.ils.constFirst();
    if(!idMap.contains(ils.getRef()))
    {
      idMap.insert(ils.getRef());
      texts.append(leg.fixIdent);
      if(drawUnderlay)
        paintProcedureUnderlay(leg, x, y, symbolSizeWaypoint);
      paintProcedurePoint(x, y, false);
    }
    if(drawText)
      paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
  }
  else if(!navaids.runwayEnds.isEmpty() && wToSBuf(navaids.runwayEnds.constFirst().position, x, y, MARGINS))
  {
    texts.prepend(leg.fixIdent);
    if(drawUnderlay)
      paintProcedureUnderlay(leg, x, y, symbolSizeWaypoint);
    paintProcedurePoint(x, y, false);
    if(drawText)
      paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
  }
  else if(!leg.fixIdent.isEmpty() && wToSBuf(leg.fixPos, x, y, MARGINS))
  {
    // Custom IF case
    texts.prepend(leg.fixIdent);
    if(drawUnderlay)
      paintProcedureUnderlay(leg, x, y, symbolSizeWaypoint);
    paintProcedurePoint(x, y, false);
    if(drawText)
      paintProcedurePointText(x, y, drawTextDetails, textPlacementAtts, texts);
  }

  // Draw wind barbs for SID and STAR (not approaches) =======================================
  const Route *route = context->route;
  if(!preview && !previewAll && (leg.isAnySid() || leg.isAnyStar()))
  {
    if(context->objectDisplayTypes.testFlag(map::WIND_BARBS_ROUTE) && NavApp::getWindReporter()->hasAnyWindData())
    {
      int routeIdx = index;

      // Convert from procedure index to route leg index
      if(leg.isAnySid())
        routeIdx += route->getSidLegsOffset();
      else if(leg.isAnyStar())
        routeIdx += route->getStarLegsOffset();

      // Do not draw wind barbs for approaches
      const RouteAltitudeLeg& altLeg = route->getAltitudeLegAt(routeIdx);
      if(altLeg.getLineString().getPos2().getAltitude() > map::MIN_WIND_BARB_ALTITUDE)
        paintWindBarbAtWaypoint(altLeg.getWindSpeed(), altLeg.getWindDirection(), x, y);
    }
  }
}

void MapPainterRoute::paintAirport(float x, float y, const map::MapAirport& obj)
{
  context->routeDrawnNavaids->append(obj.getRef());
  float size = context->szF(context->symbolSizeAirport, context->mapLayerRoute->getAirportSymbolSize());
  symbolPainter->drawAirportSymbol(context->painter, obj, x, y, size, false, false,
                                   context->flags2.testFlag(opts2::MAP_AIRPORT_HIGHLIGHT_ADDON));
}

void MapPainterRoute::paintAirportText(float x, float y, const map::MapAirport& obj, textatt::TextAttributes atts)
{
  float size = context->szF(context->symbolSizeAirport, context->mapLayerRoute->getAirportSymbolSize());
  symbolPainter->drawAirportText(context->painter, obj, x, y, context->dispOptsAirport,
                                 context->airportTextFlagsRoute(true /* drawAsRoute */, false /* draw as log */), size,
                                 context->mapLayerRoute->isAirportDiagram(),
                                 context->mapLayerRoute->getMaxTextLengthAirport(), atts);
}

void MapPainterRoute::paintWaypoint(float x, float y, const map::MapWaypoint& obj, bool preview)
{
  context->routeDrawnNavaids->append(obj.getRef());
  paintWaypoint(QColor(), x, y, preview);
}

void MapPainterRoute::paintWaypoint(const QColor& col, float x, float y, bool preview)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());
  size = std::max(size, 8.f);

  symbolPainter->drawWaypointSymbol(context->painter, col, x, y, size, !preview);
}

void MapPainterRoute::paintWaypointText(float x, float y, const map::MapWaypoint& obj, bool drawTextDetails,
                                        textatt::TextAttributes atts, const QStringList *additionalText)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;

  if(!drawTextDetails && additionalText != nullptr && !additionalText->isEmpty())
    // Show ellipsis instead of additional texts if these are not empty
    flags |= textflags::ELLIPSE_IDENT;

  if(context->mapLayerRoute->isWaypointRouteName())
    flags |= textflags::IDENT;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawWaypointText(context->painter, obj, x, y, flags, size, fill, atts, additionalText);
}

void MapPainterRoute::paintVor(float x, float y, const map::MapVor& obj, bool preview)
{
  context->routeDrawnNavaids->append(obj.getRef());
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getVorSymbolSizeRoute());
  float sizeLarge = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getVorSymbolSizeLarge());
  symbolPainter->drawVorSymbol(context->painter, obj, x, y, size, sizeLarge, !preview, false /* fast */);
}

void MapPainterRoute::paintVorText(float x, float y, const map::MapVor& obj, bool drawTextDetails, textatt::TextAttributes atts,
                                   const QStringList *additionalText)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getVorSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;

  if(!drawTextDetails && additionalText != nullptr && !additionalText->isEmpty())
    // Show ellipsis instead of additional texts if these are not empty
    flags |= textflags::ELLIPSE_IDENT;

  // Use more more detailed VOR text for flight plan
  if(context->mapLayerRoute->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayerRoute->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawVorText(context->painter, obj, x, y, flags, size, fill, atts, additionalText);
}

void MapPainterRoute::paintNdb(float x, float y, const map::MapNdb& obj, bool preview)
{
  context->routeDrawnNavaids->append(obj.getRef());
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getNdbSymbolSize());
  size = std::max(size, 8.f);
  symbolPainter->drawNdbSymbol(context->painter, x, y, size, !preview, false);
}

void MapPainterRoute::paintNdbText(float x, float y, const map::MapNdb& obj, bool drawTextDetails, textatt::TextAttributes atts,
                                   const QStringList *additionalText)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getNdbSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;

  if(!drawTextDetails && additionalText != nullptr && !additionalText->isEmpty())
    // Show ellipsis instead of additional texts if these are not empty
    flags |= textflags::ELLIPSE_IDENT;

  // Use more more detailed NDB text for flight plan
  if(context->mapLayerRoute->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayerRoute->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawNdbText(context->painter, obj, x, y, flags, size * 1.5f, fill, atts, additionalText);
}

/* paint intermediate approach point */
void MapPainterRoute::paintProcedurePoint(float x, float y, bool preview)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getProcedurePointSymbolSize());
  symbolPainter->drawProcedureSymbol(context->painter, x, y, size, !preview);
}

void MapPainterRoute::paintProcedurePointText(float x, float y, bool drawTextDetails, textatt::TextAttributes atts,
                                              const QStringList& texts)
{
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getProcedurePointSymbolSize());
  float lineWidth = std::max(size / 5.f, 2.0f);
  paintText(mapcolors::routeProcedurePointColor, x + lineWidth + 2.f, y, size * 1.5f, drawTextDetails, texts, atts);
}

void MapPainterRoute::paintProcedureUnderlay(const proc::MapProcedureLeg& leg, float x, float y, float size)
{
  // Ring to indicate fly over
  // Maltese cross to indicate FAF on the map
  symbolPainter->drawProcedureUnderlay(context->painter, x, y, size, leg.flyover, leg.malteseCross);
}

void MapPainterRoute::paintUserpoint(float x, float y, const map::MapUserpointRoute& obj, bool preview)
{
  context->routeDrawnNavaids->append(obj.getRef());
  float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());
  size = std::max(size, 8.f);
  symbolPainter->drawUserpointSymbol(context->painter, x, y, size, !preview);
}

void MapPainterRoute::paintText(const QColor& color, float x, float y, float size, bool drawTextDetails, QStringList texts,
                                textatt::TextAttributes atts)
{
  texts.removeDuplicates();
  texts.removeAll(QString());

  // Move position according to text placement
  symbolPainter->adjustPos(x, y, size, atts);

  if(!drawTextDetails)
  {
    if(texts.isEmpty())
      // Show ellipsis only if text is empty
      texts.append(tr("…", "Dots used indicate additional text in map"));
    else
    {
      // Truncate long texts like "intercept ..." and add ellipsis to ignore additional texts
      texts.first() = texts.constFirst().mid(0, 5) % (tr("…", "Dots used indicate additional text in map"));
      texts = texts.mid(0, 1);
    }
  }

  int transparency = 255;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    transparency = 0;

  if(!texts.isEmpty() && context->mapLayerRoute->isWaypointRouteName())
    symbolPainter->textBoxF(context->painter, texts, color, x, y, atts, transparency);
}

void MapPainterRoute::drawSymbols(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints, bool preview)
{
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      float x = static_cast<float>(pt.x());
      float y = static_cast<float>(pt.y());
      const RouteLeg& leg = context->route->value(i);
      map::MapTypes type = leg.getMapType();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
      switch(type)
      {
        case map::INVALID:
          // name and region not found in database
          paintWaypoint(mapcolors::routeInvalidPointColor, x, y, preview);
          break;

        case map::USERPOINTROUTE:
          paintUserpoint(x, y, leg.getUserpointRoute(), preview);
          break;

        case map::AIRPORT:
          paintAirport(x, y, leg.getAirport());
          break;

        case map::VOR:
          paintVor(x, y, leg.getVor(), preview);
          break;

        case map::NDB:
          paintNdb(x, y, leg.getNdb(), preview);
          break;

        case map::WAYPOINT:
          paintWaypoint(x, y, leg.getWaypoint(), preview);
          break;
      }
#pragma GCC diagnostic pop
    }
    i++;
  }
}

void MapPainterRoute::paintWindBarbs(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  const Route *route = context->route;

  if(!route->hasAltitudeLegs() || !route->isValidProfile())
    return;

  QPointF lastPt;
  int routeIndex = 0;
  for(const QPointF& pt : startPoints)
  {
    if(routeIndex > route->getNumAltitudeLegs() - 1)
      break;

    if(visibleStartPoints.testBit(routeIndex))
    {
      bool distOk = lastPt.isNull() || (pt - lastPt).manhattanLength() > 50;

      const RouteAltitudeLeg& altLeg = route->getAltitudeLegAt(routeIndex);
      if(altLeg.getLineString().getPos2().getAltitude() > map::MIN_WIND_BARB_ALTITUDE && !altLeg.isMissed() &&
         !altLeg.isAlternate() && distOk &&
         routeIndex > route->getDepartureAirportLegIndex() && routeIndex < route->getDestinationAirportLegIndex())
      {
        paintWindBarbAtWaypoint(altLeg.getWindSpeed(), altLeg.getWindDirection(), static_cast<float>(pt.x()), static_cast<float>(pt.y()));
        lastPt = pt;
      }
    }
    routeIndex++;
  }
}

textatt::TextAttributes MapPainterRoute::textPlacementAttributes(int routeIndex)
{
  if(routeIndex == -1)
    return textatt::PLACE_LEFT;

  const RouteLeg *curLeg = &context->route->value(routeIndex);
  int nextIndex = routeIndex + 1;
  const RouteLeg *nextLeg = &context->route->getLegAt(nextIndex);

  float courseEndTrue = map::INVALID_COURSE_VALUE, courseStartTrue = map::INVALID_COURSE_VALUE;

  // Get course to the endpoint of the current leg
  courseEndTrue = curLeg->getGeometryEndCourse();

  // Do not look further if this is an alternate airport
  if(!curLeg->isAlternate())
  {
    // Try to skip initial fix and other legs without geometry or course
    if(!(courseEndTrue < map::INVALID_COURSE_VALUE) && !curLeg->isAlternate())
    {
      curLeg = &context->route->getLegAt(--routeIndex);
      courseEndTrue = curLeg->getGeometryEndCourse();
    }

    // Get outbound course for the waypoint at the end of this leg
    courseStartTrue = nextLeg->getGeometryStartCourse();
    if(!(courseStartTrue < map::INVALID_COURSE_VALUE))
    {
      // Skip initial fix or others without course or geometry
      nextLeg = &context->route->getLegAt(++nextIndex);
      courseStartTrue = nextLeg->getGeometryStartCourse();
    }
  }

  // Angle pointing towards text label - e.g. 270 puts the label to the left of the point
  float textAngle = 0.f;
  if(courseStartTrue < map::INVALID_COURSE_VALUE && courseEndTrue < map::INVALID_COURSE_VALUE)
  {
    // Reverse inbound to outbound
    courseEndTrue = atools::geo::opposedCourseDeg(courseEndTrue);

    // Calculate difference in degrees
    float diff = atools::geo::angleAbsDiffSign(courseEndTrue, courseStartTrue);

    // Pos Turn right
    // positive if angle1 < angle2 and negative if angle1 > angle2
    if(diff > 0.f)
      // courseEndTrue < courseStartTrue
      textAngle = atools::geo::opposedCourseDeg(courseEndTrue + std::abs(diff) / 2.f);
    else
      // courseEndTrue > courseStartTrue
      textAngle = atools::geo::opposedCourseDeg(courseStartTrue + std::abs(diff) / 2.f);
  }
  // Next two cases appear at end of lines like runways or alternates
  else if(courseStartTrue < map::INVALID_COURSE_VALUE)
    textAngle = atools::geo::opposedCourseDeg(courseStartTrue);
  else if(courseEndTrue < map::INVALID_COURSE_VALUE)
    textAngle = courseEndTrue;

#ifdef DEBUG_INFORMATION_TEXT_PLACEMENT
  qDebug() << Q_FUNC_INFO << "====" << "textAngle" << textAngle;
  qDebug() << Q_FUNC_INFO << "CUR" << curIndex << proc::procedureTypeText(curLeg->getProcedureType())
           << curLeg->getIdent() << courseEndTrue << "geo" << curLeg->getGeometry();
  qDebug() << Q_FUNC_INFO << "NEXT" << nextIndex << proc::procedureTypeText(nextLeg->getProcedureType())
           << nextLeg->getIdent() << courseStartTrue << "geo" << nextLeg->getGeometry();
#endif

  textatt::TextAttributes att;
  // 45 degree steps with sectors moving from 22.5°
  if(textAngle < 22.5f)
    att = textatt::PLACE_ABOVE;
  else if(textAngle < 67.5f)
    att = textatt::PLACE_ABOVE_RIGHT;
  else if(textAngle < 112.5f)
    att = textatt::PLACE_RIGHT;
  else if(textAngle < 157.5f)
    att = textatt::PLACE_BELOW_RIGHT;
  else if(textAngle < 202.5f)
    att = textatt::PLACE_BELOW;
  else if(textAngle < 247.5f)
    att = textatt::PLACE_BELOW_LEFT;
  else if(textAngle < 292.5f)
    att = textatt::PLACE_LEFT;
  else if(textAngle < 337.5f)
    att = textatt::PLACE_ABOVE_LEFT;
  else if(textAngle < 360.f)
    att = textatt::PLACE_ABOVE;
  else
    att = textatt::PLACE_LEFT;

  return att | textatt::ROUTE_BG_COLOR;
}

void MapPainterRoute::drawRouteSymbolText(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  const Route *route = context->route;
  for(int i = 0; i < startPoints.size(); i++)
  {
    if(visibleStartPoints.testBit(i))
    {
      const QPointF& startPt(startPoints.at(i));
      float x = static_cast<float>(startPt.x());
      float y = static_cast<float>(startPt.y());
      const RouteLeg& curLeg = route->value(i);

      textatt::TextAttributes textPlacementAtts = textPlacementAttributes(i);

#ifdef DEBUG_INFORMATION_ROUTE_TEXT
      {
        atools::util::PainterContextSaver saver(context->painter);
        QLineF line(startPt, startPt - QPointF(0, 50));
        line.setAngle(atools::geo::angleToQt(textAngle));
        context->painter->setPen(QPen());
        context->painter->drawLine(line);
      }

      qDebug() << Q_FUNC_INFO << curLeg.getIdent() << "courseEndTrue" << courseEndTrue << "courseStartTrue" << courseStartTrue
               << "diff" << diff << "textAngle" << textAngle << (diff > 0.f ? "right" : "left");
#endif

      float size = context->szF(context->symbolSizeNavaid, context->mapLayerRoute->getWaypointSymbolSize());

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
      switch(curLeg.getMapType())
      {
        case map::INVALID:
          paintText(mapcolors::routeInvalidPointColor, x, y, size, true /* drawTextDetails */, {curLeg.getDisplayIdent()},
                    textPlacementAtts);
          break;

        case map::USERPOINTROUTE:
          paintText(mapcolors::routeUserPointColor, x, y, size, true /* drawTextDetails */,
                    {atools::elideTextShort(curLeg.getDisplayIdent(), context->mapLayerRoute->getMaxTextLengthAirport())},
                    textPlacementAtts);
          break;

        case map::AIRPORT:
          paintAirportText(x, y, curLeg.getAirport(), textPlacementAtts);
          break;

        case map::VOR:
          paintVorText(x, y, curLeg.getVor(), true /* drawTextDetails */, textPlacementAtts, nullptr);
          break;

        case map::NDB:
          paintNdbText(x, y, curLeg.getNdb(), true /* drawTextDetails */, textPlacementAtts, nullptr);
          break;

        case map::WAYPOINT:
          paintWaypointText(x, y, curLeg.getWaypoint(), true /* drawTextDetails */, textPlacementAtts, nullptr);
          break;

        default:
          break;
      }
#pragma GCC diagnostic pop
    }
  }
}

void MapPainterRoute::paintStartParking()
{
  const static QMargins MARGINS(20, 20, 20, 20);
  const Route *route = context->route;

  // Draw start position or parking circle into the airport diagram
  const RouteLeg& departureLeg = route->getDepartureAirportLeg();
  if(departureLeg.isValid() && departureLeg.getMapType() == map::AIRPORT)
  {
    // Use airport symbol as base size for default
    float radius = context->szF(context->symbolSizeAirport, context->mapLayerRoute->getAirportSymbolSize()) * 0.75f;
    float wradius = radius, hradius = radius;
    Pos startPos;
    if(route->hasDepartureParking())
    {
      // Parking ramp or gate ==================
      if(context->mapLayerRoute->isAirportDiagramDetail())
      {
        // Radius based on parking size
        startPos = departureLeg.getDepartureParking().position;
        radius = departureLeg.getDepartureParking().getRadius();

        // At least 10 pixel size - unaffected by scaling
        wradius = std::max(10.f, scale->getPixelForFeet(radius, 90));
        hradius = std::max(10.f, scale->getPixelForFeet(radius, 0));
      }
    }
    else if(route->hasDepartureStart())
    {
      // Start - runway, helipad or water ==================
      if(context->mapLayerRoute->isAirportDiagramDetail())
        startPos = departureLeg.getDepartureStart().position;
    }
    else
      // No departure set - use airport ==================
      startPos = departureLeg.getPosition();

    if(startPos.isValid())
    {
      float x, y;
      if(wToSBuf(startPos, x, y, MARGINS))
      {
        bool transparent = context->flags2.testFlag(opts2::MAP_HIGHLIGHT_TRANSPARENT);
        if(!transparent)
        {
          context->painter->setPen(QPen(Qt::black, 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          context->painter->drawEllipse(QPointF(x, y), wradius, hradius);
        }

        float alpha = transparent ? (1.f - context->transparencyHighlight) : 1.f;
        context->painter->setPen(QPen(mapcolors::adjustAlphaF(OptionData::instance().getFlightplanColor(), alpha),
                                      transparent ? 1.f : 5.f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        context->painter->setBrush(transparent ?
                                   QBrush(mapcolors::adjustAlphaF(OptionData::instance().getFlightplanColor(), alpha)) :
                                   QBrush(Qt::NoBrush));
        context->painter->drawEllipse(QPointF(x, y), wradius, hradius);
      }
    }
  }
}

void MapPainterRoute::paintWindBarbAtWaypoint(float windSpeed, float windDir, float x, float y)
{
  if(context->route->hasAltitudeLegs() && context->route->isValidProfile())
  {
    float size = context->szF(context->symbolSizeAirport, context->mapLayerRoute->getWindBarbsSymbolSize());
    symbolPainter->drawWindBarbs(context->painter, windSpeed, 0.f /* gust */, windDir, x - 5, y - 5, size,
                                 true /* barbs */, true /* alt wind */, true /* route */, context->drawFast);
  }
}

QColor MapPainterRoute::flightplanActiveColor() const
{
  if(context->flags2.testFlag(opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE))
    return OptionData::instance().getFlightplanActiveSegmentColor();
  else
    return OptionData::instance().getFlightplanColor();
}

QColor MapPainterRoute::flightplanActiveProcColor() const
{
  if(context->flags2.testFlag(opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE))
    return OptionData::instance().getFlightplanActiveSegmentColor();
  else
    return OptionData::instance().getFlightplanProcedureColor();
}
