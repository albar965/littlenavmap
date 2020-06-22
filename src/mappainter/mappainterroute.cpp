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

#include "mappainter/mappainterroute.h"

#include "mapgui/mapwidget.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "common/proctypes.h"
#include "geo/calculations.h"
#include "route/routecontroller.h"
#include "mapgui/mapscale.h"
#include "navapp.h"
#include "util/paintercontextsaver.h"
#include "weather/windreporter.h"
#include "common/textplacement.h"
#include "route/routealtitudeleg.h"

#include <QBitArray>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using proc::MapProcedureLeg;
using proc::MapProcedureLegs;
using map::PosCourse;
using atools::contains;
namespace ageo = atools::geo;

MapPainterRoute::MapPainterRoute(MapPaintWidget *mapWidget, MapScale *mapScale, const Route *routeParam)
  : MapPainter(mapWidget, mapScale), route(routeParam)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::render(PaintContext *context)
{
  // Draw route including approaches
  if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN))
    paintRoute(context);

  // Draw the approach preview if any selected in the search tab
  proc::MapProcedureLeg lastLegPoint;
  if(context->mapLayer->isApproach())
    paintProcedure(lastLegPoint, context,
                   mapPaintWidget->getProcedureHighlight(), 0, mapcolors::routeProcedurePreviewColor,
                   true /* preview */);

  if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN) &&
     context->objectDisplayTypes.testFlag(map::FLIGHTPLAN_TOC_TOD) &&
     context->mapLayer->isRouteTextAndDetail())
    paintTopOfDescentAndClimb(context);
}

QString MapPainterRoute::buildLegText(const PaintContext *context, const RouteLeg& leg)
{
  return buildLegText(context, leg.getDistanceTo(), leg.getCourseToMag(), leg.getCourseToTrue());
}

QString MapPainterRoute::buildLegText(const PaintContext *context, float dist, float courseGcMag, float courseGcTrue)
{
  if(context->distance > layer::DISTANCE_CUT_OFF_LIMIT)
    return QString();

  QStringList texts;

  if(context->dOptRoute(optsd::ROUTE_DISTANCE) && dist < map::INVALID_DISTANCE_VALUE / 2.f)
    texts.append(Unit::distNm(dist, true /*addUnit*/, 20, true /*narrow*/));

  bool magGc = context->dOptRoute(optsd::ROUTE_MAG_COURSE_GC) && courseGcMag < map::INVALID_COURSE_VALUE / 2.f;
  bool trueGc = context->dOptRoute(optsd::ROUTE_TRUE_COURSE_GC) && courseGcTrue < map::INVALID_COURSE_VALUE / 2.f;

  QString courseGcMagStr = QString::number(ageo::normalizeCourse(courseGcMag), 'f', 0);
  QString courseGcTrueStr = QString::number(ageo::normalizeCourse(courseGcTrue), 'f', 0);

  if(magGc && trueGc && courseGcMagStr == courseGcTrueStr)
    // True and mag course are equal - combine
    texts.append(courseGcMagStr + tr("°M/T"));
  else
  {
    if(magGc)
      texts.append(courseGcMagStr + tr("°M"));
    if(trueGc)
      texts.append(courseGcTrueStr + tr("°T"));
  }

  return texts.join(tr(" / "));
}

void MapPainterRoute::paintRoute(const PaintContext *context)
{
  if(route->isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);

  context->painter->setBrush(Qt::NoBrush);

  if(context->mapLayer->isRouteTextAndDetail())
    drawStartParking(context);

  // Get active route leg
  bool activeValid = route->isActiveValid();
  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg = activeValid ? route->getActiveLegIndex() : 0;
  if(activeRouteLeg == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO;
    return;
  }

  int passedRouteLeg = context->flags2 & opts2::MAP_ROUTE_DIM_PASSED ? activeRouteLeg : 0;

  // Collect line text and geometry from the route
  QStringList routeTexts;
  QVector<Line> lines;

  // Collect route - only coordinates and texts ===============================
  const RouteLeg& destLeg = route->getDestinationAirportLeg();
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->value(i);
    const RouteLeg& last = route->value(i - 1);

    if(leg.isAlternate())
    {
      routeTexts.append(buildLegText(context, leg));
      lines.append(Line(destLeg.getPosition(), leg.getPosition()));
    }
    else
    {
      // Draw if at least one is part of the route - also draw empty spaces between procedures
      if(last.isRoute() || leg.isRoute() || // Draw if any is route - also covers STAR to airport
         (last.getProcedureLeg().isAnyDeparture() && leg.getProcedureLeg().isAnyArrival()) || // empty space from SID to STAR, transition or approach
         (last.getProcedureLeg().isStar() && leg.getProcedureLeg().isArrival())) // empty space from STAR to transition or approach
      {
        if(i >= passedRouteLeg)
          routeTexts.append(buildLegText(context, leg));
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

  drawRouteInternal(context, routeTexts, lines, passedRouteLeg);

  // Draw procedures ==========================================================================
  // Remember last point across procedures to avoid overlaying text

  if(context->mapLayer->isApproach())
  {
    proc::MapProcedureLeg lastLegPoint;
    if(context->distance < layer::DISTANCE_CUT_OFF_LIMIT)
    {
      const QColor& flightplanProcedureColor = OptionData::instance().getFlightplanProcedureColor();
      if(route->hasAnyApproachProcedure())
        paintProcedure(lastLegPoint, context, route->getApproachLegs(), route->getApproachLegsOffset(),
                       flightplanProcedureColor, false /* preview */);

      if(route->hasAnyStarProcedure())
        paintProcedure(lastLegPoint, context, route->getStarLegs(), route->getStarLegsOffset(),
                       flightplanProcedureColor, false /* preview */);

      if(route->hasAnySidProcedure())
        paintProcedure(lastLegPoint, context, route->getSidLegs(), route->getSidLegsOffset(),
                       flightplanProcedureColor, false /* preview */);
    }
  }

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

void MapPainterRoute::drawRouteInternal(const PaintContext *context, QStringList routeTexts, QVector<Line> lines,
                                        int passedRouteLeg)
{
  Marble::GeoPainter *painter = context->painter;
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBackground(mapcolors::routeOutlineColor);
  painter->setBrush(Qt::NoBrush);

  // Draw route lines ==========================================================================
  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);

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

    const OptionData& od = OptionData::instance();
    QPen routePen(od.getFlightplanColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen routeOutlinePen(mapcolors::routeOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen routeAlternateOutlinePen(mapcolors::routeAlternateOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap,
                                  Qt::RoundJoin);
    QPen routePassedPen(od.getFlightplanPassedSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin);
    int alternateOffset = route->getAlternateLegsOffset();

    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      // Draw gray line for passed legs
      painter->setPen(routePassedPen);
      for(int i = 0; i < passedRouteLeg; i++)
        drawLine(painter, lines.at(i));

      painter->setPen(routeAlternateOutlinePen);
      if(alternateOffset != map::INVALID_INDEX_VALUE)
      {
        for(int idx = alternateOffset; idx < alternateOffset + route->getNumAlternateLegs(); idx++)
          drawLine(painter, lines.at(idx - 1));
      }

      // Draw background for legs ahead
      painter->setPen(routeOutlinePen);
      for(int i = passedRouteLeg; i < route->getDestinationAirportLegIndex(); i++)
        drawLine(painter, lines.at(i));

      // Draw center line for legs ahead
      painter->setPen(routePen);
      for(int i = passedRouteLeg; i < destAptIdx; i++)
        drawLine(painter, lines.at(i));

      // Draw center line for alternates all from destination airport to each alternate
      if(alternateOffset != map::INVALID_INDEX_VALUE)
      {
        mapcolors::adjustPenForAlternate(painter);
        for(int idx = alternateOffset; idx < alternateOffset + route->getNumAlternateLegs(); idx++)
          drawLine(painter, lines.at(idx - 1));
      }
    }

    if(route->isActiveValid())
    {
      int activeRouteLeg = route->getActiveLegIndex();

      // Draw active leg on top of all others to keep it visible ===========================
      painter->setPen(routeOutlinePen);
      drawLine(painter, lines.at(activeRouteLeg - 1));

      painter->setPen(QPen(OptionData::instance().getFlightplanActiveSegmentColor(), innerlinewidth,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      drawLine(painter, lines.at(activeRouteLeg - 1));
    }
  }
  context->szFont(context->textSizeFlightplan);

  // Collect coordinates for text placement and lines first ============================
  LineString positions;
  for(int i = 0; i < route->size(); i++)
    positions.append(route->value(i).getPosition());

  TextPlacement textPlacement(painter, this);
  textPlacement.setDrawFast(context->drawFast);
  textPlacement.setLineWidth(outerlinewidth);
  textPlacement.calculateTextPositions(positions);
  textPlacement.calculateTextAlongLines(lines, routeTexts);
  painter->save();
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    painter->setBackgroundMode(Qt::TransparentMode);
  else
    painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setBackground(mapcolors::routeTextBackgroundColor);
  painter->setPen(mapcolors::routeTextColor);

  if(context->mapLayer->isRouteTextAndDetail())
    textPlacement.drawTextAlongLines();
  painter->restore();
  painter->setBackgroundMode(Qt::OpaqueMode);

  // ================================================================================

  QBitArray visibleStartPoints = textPlacement.getVisibleStartPoints();
  for(int i = 0; i < route->size(); i++)
  {
    // Make all approach points except the last one invisible to avoid text and symbol overlay over approach
    if(route->value(i).isAnyProcedure())
      visibleStartPoints.clearBit(i);
  }

  // Draw airport and navaid symbols
  drawSymbols(context, visibleStartPoints, textPlacement.getStartPoints(), false /* preview */);

  for(int i = 0; i < route->size(); i++)
  {
    // Do not draw text for passed waypoints
    if(i + 1 < passedRouteLeg)
      visibleStartPoints.clearBit(i);
  }

  // Draw symbol text
  drawRouteSymbolText(context, visibleStartPoints, textPlacement.getStartPoints());

  if(context->objectDisplayTypes.testFlag(map::WIND_BARBS_ROUTE) &&
     (NavApp::getWindReporter()->hasWindData() || NavApp::getWindReporter()->isWindManual()))
    drawWindBarbs(context, visibleStartPoints, textPlacement.getStartPoints());
}

void MapPainterRoute::paintTopOfDescentAndClimb(const PaintContext *context)
{
  if(route->getSizeWithoutAlternates() >= 2)
  {
    atools::util::PainterContextSaver saver(context->painter);

    float width = context->sz(context->symbolSizeNavaid, 3);
    int radius = atools::roundToInt(context->sz(context->symbolSizeNavaid, 6));

    context->painter->setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
    context->painter->setBrush(Qt::transparent);
    context->szFont(context->textSizeFlightplan);

    int transparency = 255;
    if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
      transparency = 0;

    int activeLegIndex = route->getActiveLegIndex();

    // Draw the top of climb circle and text ======================
    if(!(OptionData::instance().getFlags2() & opts2::MAP_ROUTE_DIM_PASSED) ||
       activeLegIndex == map::INVALID_INDEX_VALUE || route->getTopOfClimbLegIndex() > activeLegIndex - 1)
    {
      Pos pos = route->getTopOfClimbPos();
      if(pos.isValid())
      {
        QPoint pt = wToS(pos);
        if(!pt.isNull())
        {
          context->painter->drawEllipse(pt, radius, radius);

          QStringList toc;
          toc.append(tr("TOC"));
          if(context->mapLayer->isAirportRouteInfo())
            toc.append(Unit::distNm(route->getTopOfClimbDistance()));

          symbolPainter->textBox(context->painter, toc, QPen(mapcolors::routeTextColor),
                                 pt.x() + radius, pt.y() + radius,
                                 textatt::ROUTE_BG_COLOR, transparency);
        }
      }
    }

    // Draw the top of descent circle and text ======================
    if(!(OptionData::instance().getFlags2() & opts2::MAP_ROUTE_DIM_PASSED) ||
       activeLegIndex == map::INVALID_INDEX_VALUE || route->getTopOfDescentLegIndex() > activeLegIndex - 1)
    {
      Pos pos = route->getTopOfDescentPos();
      if(pos.isValid())
      {
        QPoint pt = wToS(pos);
        if(!pt.isNull())
        {
          context->painter->drawEllipse(pt, radius, radius);

          QStringList tod;
          tod.append(tr("TOD"));
          if(context->mapLayer->isAirportRouteInfo())
            tod.append(Unit::distNm(route->getTopOfDescentFromDestination()));

          symbolPainter->textBox(context->painter, tod, QPen(mapcolors::routeTextColor),
                                 pt.x() + radius, pt.y() + radius,
                                 textatt::ROUTE_BG_COLOR, transparency);
        }
      }
    }
  }
}

/* Draw approaches and transitions selected in the tree view */
void MapPainterRoute::paintProcedure(proc::MapProcedureLeg& lastLegPoint, const PaintContext *context,
                                     const proc::MapProcedureLegs& legs, int legsRouteOffset, const QColor& color,
                                     bool preview)
{
  if(legs.isEmpty() || !legs.bounding.overlaps(context->viewportRect))
    return;

  QPainter *painter = context->painter;
  atools::util::PainterContextSaver saver(context->painter);

  context->painter->setBackgroundMode(Qt::OpaqueMode);
  context->painter->setBackground(Qt::white);
  context->painter->setBrush(Qt::NoBrush);

  // Get active approach leg
  bool activeValid = route->isActiveValid();
  int activeProcLeg = activeValid ? route->getActiveLegIndex() - legsRouteOffset : 0;
  int passedProcLeg = context->flags2 & opts2::MAP_ROUTE_DIM_PASSED ? activeProcLeg : 0;

  // Draw black background ========================================
  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);

  // Keep a stack of last painted geometry since some functions need access back into the history
  QVector<QLineF> lastLines({QLineF()});
  QVector<QLineF> lastActiveLines({QLineF()});

  painter->setPen(QPen(mapcolors::routeProcedureOutlineColor, outerlinewidth,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(int i = 0; i < legs.size(); i++)
  {
    // Do not draw background for passed legs but calculate lastLine
    bool draw = (i >= passedProcLeg) || !activeValid || preview;
    if(legs.at(i).isCircleToLand() || legs.at(i).isStraightIn() || legs.at(i).isVectors() || legs.at(i).isManual())
      // Do not draw outline for circle-to-land approach legs
      draw = false;

    paintProcedureSegment(context, legs, i, lastLines, nullptr, true /* no text */, preview, draw);
  }

  QPen missedPen(color, innerlinewidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  QPen apprPen(color, innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  const OptionData& od = OptionData::instance();
  QPen missedActivePen(od.getFlightplanActiveSegmentColor(), innerlinewidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  QPen apprActivePen(od.getFlightplanActiveSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  QPen routePassedPen(od.getFlightplanPassedSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  lastLines.clear();
  lastLines.append(QLineF());
  QVector<DrawText> drawTextLines, lastActiveDrawTextLines;
  drawTextLines.fill({Line(), false, false}, legs.size());
  lastActiveDrawTextLines = drawTextLines;

  // Draw segments and collect text placement information in drawTextLines ========================================
  // Need to set font since it is used by drawHold
  context->szFont(context->textSizeFlightplan * 1.1f);

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
    paintProcedureSegment(context, legs, i, lastLines, &drawTextLines, noText, preview, draw);
  }

  // Paint active on top of others ====================================================
  if(!preview && activeValid && activeProcLeg >= 0 && route->isActiveProcedure())
  {
    // Use pen for active leg
    painter->setPen(legs.at(activeProcLeg).isMissed() ? missedActivePen : apprActivePen);

    if(legs.at(activeProcLeg).isCircleToLand() || legs.at(activeProcLeg).isStraightIn())
      // Use dashed line for CTL or straight in
      mapcolors::adjustPenForCircleToLand(painter);
    else if(legs.at(activeProcLeg).isVectors())
      mapcolors::adjustPenForVectors(painter);
    else if(legs.at(activeProcLeg).isManual())
      mapcolors::adjustPenForManual(painter);

    paintProcedureSegment(context, legs, activeProcLeg, lastActiveLines, &lastActiveDrawTextLines, noText, preview,
                          true /* draw */);
  }

  // Draw text along lines only on low zoom factors ========
  if(context->mapLayer->isApproachText())
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

        if(i < passedProcLeg && activeValid && !preview)
        {
          // No texts for passed legs
          approachTexts.append(QString());
          lines.append(leg.line);
          textColors.append(Qt::transparent);
        }
        else
        {
          float dist = map::INVALID_DISTANCE_VALUE;
          if(drawTextLines.at(i).distance && context->dOptRoute(optsd::ROUTE_DISTANCE))
            dist = leg.calculatedDistance;

          float courseMag = map::INVALID_COURSE_VALUE, courseTrue = map::INVALID_COURSE_VALUE;
          if(drawTextLines.at(i).course)
          {
            if(leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
            {
              if(context->dOptRoute(optsd::ROUTE_MAG_COURSE_GC))
                // Use same values for mag - does not make a difference at the small values in procedures
                courseMag = atools::geo::normalizeCourse(leg.calculatedTrueCourse - leg.magvar);
            }

            if(context->dOptRoute(optsd::ROUTE_MAG_COURSE_GC))
              courseTrue = leg.calculatedTrueCourse;
          }

          approachTexts.append(buildLegText(context, dist, courseMag, courseTrue));
          lines.append(leg.line);
          textColors.append(leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::routeProcedureTextColor);
        }
      }

      // Draw text along lines ====================================================
      painter->setBackground(mapcolors::routeTextBackgroundColor);

      QVector<Line> textLines;
      LineString positions;

      for(const DrawText& dt : drawTextLines)
      {
        textLines.append(dt.line);
        positions.append(dt.line.getPos1());
      }

      TextPlacement textPlacement(painter, this);
      textPlacement.setDrawFast(context->drawFast);
      textPlacement.setTextOnTopOfLine(false);
      textPlacement.setLineWidth(outerlinewidth);
      textPlacement.setColors(textColors);
      textPlacement.calculateTextPositions(positions);
      textPlacement.calculateTextAlongLines(textLines, approachTexts);
      textPlacement.drawTextAlongLines();
    }

    context->szFont(context->textSizeFlightplan);
  }

  // Texts and navaid icons ====================================================
  for(int i = legs.size() - 1; i >= 0; i--)
  {
    // No text labels for passed points
    bool drawText = i + 1 >= passedProcLeg || !activeValid || preview;
    if(!context->mapLayer->isApproachText())
      drawText = false;
    paintProcedurePoint(lastLegPoint, context, legs, i, preview, drawText);
  }
}

void MapPainterRoute::paintProcedureSegment(const PaintContext *context, const proc::MapProcedureLegs& legs,
                                            int index, QVector<QLineF>& lastLines, QVector<DrawText> *drawTextLines,
                                            bool noText, bool preview, bool draw)
{
  const proc::MapProcedureLeg& leg = legs.at(index);

  if(!preview && leg.isMissed() && !(context->objectTypes & map::MISSED_APPROACH))
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

  if(leg.type == proc::INITIAL_FIX)
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
  if(!context->mapLayer->isApproachDetail())
  {
    bool visible1, visible2, hidden1, hidden2;
    wToS(leg.line.getPos1(), DEFAULT_WTOS_SIZE, &visible1, &hidden1);
    wToS(leg.line.getPos2(), DEFAULT_WTOS_SIZE, &visible2, &hidden2);

    if(!hidden1 && !hidden2)
    {
      // QLineF simpleLine(lastLine.p2(), line.p1());
      if(draw)
      {
        if(!lastLines.last().p2().isNull() && !line.p1().isNull())
          drawLine(painter, lastLines.last().p2(), line.p1());
        if(!line.isNull())
          drawLine(painter, line);
      }
      lastLines.append(line);

      if(drawTextLines != nullptr)
        // Disable all drawing
        (*drawTextLines)[index] = {leg.line, false, false};
    }
    return;
  }

  QPointF intersectPoint = wToS(leg.interceptPos, size, &hidden);

  bool showDistance = !leg.noDistanceDisplay();

  // ===========================================================
  if(contains(leg.type, {proc::ARC_TO_FIX, proc::CONSTANT_RADIUS_ARC}))
  {
    if(line.length() > 2 && leg.recFixPos.isValid())
    {
      if(draw)
      {
        QPointF point = wToS(leg.recFixPos, size, &hidden);
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
        (*drawTextLines)[index] = {leg.line, showDistance, true};
    }

    lastLines.append(line);
  }
  // ===========================================================
  else if(contains(leg.type, {proc::COURSE_TO_ALTITUDE,
                              proc::COURSE_TO_FIX,
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
    if(contains(leg.turnDirection, {"R", "L"}) &&
       prevLeg != nullptr && prevLeg->type != proc::INITIAL_FIX && prevLeg->type != proc::START_OF_PROCEDURE)
    {
      float lineDist = static_cast<float>(QLineF(lastLines.last().p2(), line.p1()).length());
      if(!lastLines.last().p2().isNull() && lineDist > 2.f)
      {
        // Lines are not connected which can happen if a CF follows after a FD or similar
        paintProcedureBow(prevLeg, lastLines, painter, line, leg, intersectPoint, draw);

        if(drawTextLines != nullptr)
        {
          if(leg.interceptPos.isValid())
            (*drawTextLines)[index] = {Line(leg.interceptPos, leg.line.getPos2()), false, true};
          else
            // Can draw a label along the line with course but not distance
            (*drawTextLines)[index] = {leg.line, false, true};
        }
      }
      else
      {
        // Lines are connected but a turn direction is given
        // Draw a small arc if a turn direction is given

        // lastLines gets the full line added and nextLine is the line for text
        QLineF nextLine = paintProcedureTurn(lastLines, line, leg, painter, intersectPoint, draw);

        Pos p1 = sToW(nextLine.p1());
        Pos p2 = sToW(nextLine.p2());

        if(drawTextLines != nullptr)
        {
          // Can draw a label along the line with course but not distance
          if(leg.interceptPos.isValid())
            (*drawTextLines)[index] = {Line(leg.interceptPos, leg.line.getPos2()), true, true};
          else
            (*drawTextLines)[index] = {Line(p1, p2), showDistance, true};
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
          drawLine(painter, line.p1(), intersectPoint);
          drawLine(painter, intersectPoint, line.p2());
        }
        lastLines.append(QLineF(intersectPoint, line.p2()));

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = {Line(leg.interceptPos, leg.line.getPos2()), showDistance, true};
      }
      else
      {
        if(draw)
        {
          if(!lastLines.last().p2().isNull() && QLineF(lastLines.last().p2(), line.p1()).length() > 2)
          {
            if(!(prevLeg != nullptr && (prevLeg->isCircleToLand() || prevLeg->isStraightIn()) && leg.isMissed()))
              // Close any gaps in the lines but not for circle-to-land legs
              drawLine(painter, lastLines.last().p2(), line.p1());
          }

          drawLine(painter, line);
        }
        lastLines.append(line);

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = {leg.line, showDistance, true};
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
                              proc::CIRCLE_TO_LAND,
                              proc::STRAIGHT_IN}))
  {
    if(draw)
      drawLine(painter, line);
    lastLines.append(line);

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = {leg.line, showDistance, true};
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
      holdText = QString::number(leg.course, 'f', 0) + (leg.trueCourse ? tr("°T") : tr("°M"));

      if(trueCourse < 180.f)
        holdText = holdText + tr(" ►");
      else
        holdText = tr("◄ ") + holdText;

      if(leg.time > 0.f)
        holdText2 += QString::number(leg.time, 'g', 2) + tr("min");
      else if(leg.distance > 0.f)
        holdText2 = Unit::distNm(leg.distance, true /*addUnit*/, 20, true /*narrow*/);
      else
        holdText2 = tr("1min");

      if(trueCourse < 180.f)
        holdText2 = tr("◄ ") + holdText2;
      else
        holdText2 = holdText2 + tr(" ►");
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
      text = QString::number(leg.course, 'f', 0) + (leg.trueCourse ? tr("°T") : tr("°M")) + tr("/1min");
      if(trueCourse < 180.f)
        text = text + tr(" ►");
      else
        text = tr("◄ ") + text;
    }

    QPointF pc = wToS(leg.procedureTurnPos, size, &hidden);

    if(draw)
      paintProcedureTurnWithText(painter, static_cast<float>(pc.x()), static_cast<float>(pc.y()),
                                 trueCourse, leg.distance,
                                 leg.turnDirection == "L", &lastLines.last(), text,
                                 leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::
                                 routeProcedureTextColor,
                                 mapcolors::routeTextBackgroundColor);

    if(draw)
      drawLine(painter, line.p1(), pc);

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = {Line(leg.line.getPos1(), leg.procedureTurnPos), showDistance, true};
  }
}

void MapPainterRoute::paintProcedureBow(const proc::MapProcedureLeg *prevLeg, QVector<QLineF>& lastLines,
                                        QPainter *painter,
                                        QLineF line, const proc::MapProcedureLeg& leg, const QPointF& intersectPoint,
                                        bool draw)
{
  if(!prevLeg->line.getPos2().isValid() || !leg.line.getPos1().isValid())
    return;

  bool firstMissed = !prevLeg->isMissed() && leg.isMissed() && (prevLeg->isCircleToLand() || prevLeg->isStraightIn());
  Pos prevPos;
  QPointF lastP1, lastP2;
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
    lastP1 = lastLines.last().p1();
    lastP2 = lastLines.last().p2();
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
    // Shorten the next line to get a better curve - use a value less than 1 nm to avoid flickering on 1 nm legs
    float oneNmPixel = scale->getPixelForNm(0.95f);
    if(lineDraw.length() > oneNmPixel * 2.f)
      lineDraw.setLength(lineDraw.length() - oneNmPixel);
    lineDraw.setPoints(lineDraw.p2(), lineDraw.p1());

    dist = dist * 3 / 4;

    // Calculate bezier control points by extending the last and next line
    QLineF ctrl1(lastP1, lastP2);
    ctrl1.setLength(ctrl1.length() + scale->getPixelForMeter(dist));
    QLineF ctrl2(lineDraw.p2(), lineDraw.p1());
    ctrl2.setLength(ctrl2.length() + scale->getPixelForMeter(dist));

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

  const QLineF& lastLine = lastLines.last();

  // The returned value represents the number of degrees you need to add to this
  // line to make it have the same angle as the given line, going counter-clockwise.
  double angleToLastRev = line.angleTo(QLineF(lastLine.p1(), lastLine.p2()));

  // Calculate the start position of the next line and leave space for the arc
  QLineF arc(line.p1(), QPointF(line.x2(), line.y2() + 100.));
  arc.setLength(scale->getPixelForNm(1.f));
  if(leg.turnDirection == "R")
    arc.setAngle(angleToQt(angleFromQt(QLineF(lastLine.p2(),
                                              lastLine.p1()).angle()) + angleToLastRev / 2.) + 180.f);
  else
    arc.setAngle(angleToQt(angleFromQt(QLineF(lastLine.p2(), lastLine.p1()).angle()) + angleToLastRev / 2.));

  // Calculate bezier control points by extending the last and next line
  QLineF ctrl1(lastLine.p1(), lastLine.p2()), ctrl2(endPos, arc.p2());
  ctrl1.setLength(ctrl1.length() + scale->getPixelForNm(.5f));
  ctrl2.setLength(ctrl2.length() + scale->getPixelForNm(.5f));

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

  // Line for text
  return nextLine;
}

void MapPainterRoute::paintProcedurePoint(proc::MapProcedureLeg& lastLegPoint, const PaintContext *context,
                                          const proc::MapProcedureLegs& legs, int index, bool preview,
                                          bool drawTextFlag)
{
  const proc::MapProcedureLeg& leg = legs.at(index);
  bool drawText = context->mapLayer->isApproachText() && drawTextFlag;

  // Debugging code for drawing ================================================
#ifdef DEBUG_APPROACH_PAINT
  bool hiddenDummy;
  QSize size = scale->getScreeenSizeForRect(legs.bounding);

  QLineF line, holdLine;
  wToS(leg.line, line, size, &hiddenDummy);
  wToS(leg.holdLine, holdLine, size, &hiddenDummy);
  QPainter *painter = context->painter;

  if(leg.disabled)
  {
    painter->setPen(Qt::red);
  }

  QPointF intersectPoint = wToS(leg.interceptPos, DEFAULT_WTOS_SIZE, &hiddenDummy);

  painter->save();
  painter->setPen(Qt::black);
  painter->drawEllipse(line.p1(), 20, 10);
  painter->drawEllipse(line.p2(), 10, 20);
  painter->drawText(line.x1() - 40, line.y1() + 40,
                    "Start " + proctypes::ProcedureLegTypeShortStr(leg.type) + " " + QString::number(index));

  painter->drawText(line.x2() - 40, line.y2() + 60,
                    "End " + proctypes::ProcedureLegTypeShortStr(leg.type) + " " + QString::number(index));
  if(!intersectPoint.isNull())
  {
    painter->drawEllipse(intersectPoint, 30, 30);
    painter->drawText(intersectPoint.x() - 40, intersectPoint.y() + 20,
                      proctypes::ProcedureLegTypeShortStr(leg.type) + " " + QString::number(index));
  }

  painter->setPen(QPen(Qt::darkBlue, 2));
  for(int i = 0; i < leg.geometry.size() - 1; i++)
  {
    QPoint pt1 = wToS(leg.geometry.at(i), size, &hiddenDummy);
    QPoint pt2 = wToS(leg.geometry.at(i + 1), size, &hiddenDummy);

    drawLine(painter, pt1, pt2);
  }

  painter->setPen(QPen(Qt::darkBlue, 10));
  drawLine(painter, holdLine);

  painter->setPen(QPen(Qt::darkBlue, 7));
  for(int i = 0; i < leg.geometry.size(); i++)
    painter->drawPoint(wToS(leg.geometry.at(i)));
  painter->restore();
#endif

  if(!preview && leg.isMissed() && !(context->objectTypes & map::MISSED_APPROACH))
    // If missed is hidden on route do not display it
    return;

  if(leg.disabled)
    return;

  int defaultOverflySize = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());

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

  if(index < legs.size() - 1 && leg.isTransition() && legs.at(index + 1).isApproach())
  {
    // Do not draw transtitions from this last transition leg - merge and draw them for the first approach leg
    altRestr = proc::MapAltRestriction();
    speedRestr = proc::MapSpeedRestriction();
  }

  QStringList texts;
  // if(index > 0 && legs.isApproach(index) &&
  // legs.isTransition(index - 1) && context->objectTypes & proctypes::APPROACH &&
  // context->objectTypes & proctypes::APPROACH_TRANSITION)
  //// Merge display text to get any text from a preceding transition point
  // texts.append(legs.at(index - 1).displayText);

  int x = 0, y = 0;

  if(leg.mapType == proc::PROCEDURE_SID && index == 0)
  {
    // All legs with a calculated end point
    if(wToS(leg.line.getPos1(), x, y))
    {
      texts.append("RW" + legs.runwayEnd.name);

      proc::MapAltRestriction altRestriction;
      altRestriction.descriptor = proc::MapAltRestriction::AT;
      altRestriction.alt1 = legs.runwayEnd.getPosition().getAltitude();
      altRestriction.alt2 = 0.f;

      texts.append(proc::altRestrictionTextNarrow(altRestriction));
      if(drawText)
        paintProcedureUnderlay(context, leg, x, y, defaultOverflySize);
      paintProcedurePoint(context, x, y, false);
      if(drawText)
        paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
      texts.clear();
    }
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
      return;

    // All legs with a calculated end point
    if(wToS(leg.line.getPos2(), x, y))
    {
      texts.append(leg.displayText);
      texts.append(proc::altRestrictionTextNarrow(altRestr));
      texts.append(proc::speedRestrictionTextNarrow(speedRestr));
      if(drawText)
        paintProcedureUnderlay(context, leg, x, y, defaultOverflySize);
      paintProcedurePoint(context, x, y, false);
      if(drawText)
        paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
      texts.clear();
    }
  }
  else if(leg.type == proc::START_OF_PROCEDURE)
  {
    if(wToS(leg.line.getPos1(), x, y))
      paintProcedurePoint(context, x, y, false);
  }
  else if(leg.type == proc::COURSE_TO_FIX)
  {
    if(index == 0)
    {
      if(wToS(leg.line.getPos1(), x, y))
      {
        if(drawText)
          paintProcedureUnderlay(context, leg, x, y, defaultOverflySize);
        paintProcedurePoint(context, x, y, false);
      }
    }
    else if(leg.interceptPos.isValid())
    {
      if(wToS(leg.interceptPos, x, y))
      {
        // Draw intercept comment - no altitude restriction there
        texts.append(leg.displayText);
        if(drawText)
          paintProcedureUnderlay(context, leg, x, y, defaultOverflySize);
        paintProcedurePoint(context, x, y, false);
        if(drawText)
          paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
      }

      // Clear text from "intercept" and add restrictions
      texts.clear();
      texts.append(proc::altRestrictionTextNarrow(altRestr));
      texts.append(proc::speedRestrictionTextNarrow(speedRestr));
    }
    else
    {
      if(lastInTransition)
        return;

      texts.append(leg.displayText);
      texts.append(proc::altRestrictionTextNarrow(altRestr));
      texts.append(proc::speedRestrictionTextNarrow(speedRestr));
    }
  }
  else
  {
    if(lastInTransition)
      return;

    texts.append(leg.displayText);
    texts.append(proc::altRestrictionTextNarrow(altRestr));
    texts.append(proc::speedRestrictionTextNarrow(speedRestr));
  }

  // Merge restricions between overlapping fixes across different procedures
  if(lastLegPoint.isValid() && lastLegPoint.fixPos.almostEqual(leg.fixPos) &&
     lastLegPoint.fixIdent == leg.fixIdent && lastLegPoint.fixRegion == leg.fixRegion &&
     // Do not merge text from legs which are labeled at calculated end position
     !contains(lastLegPoint.type, {proc::COURSE_TO_ALTITUDE,
                                   proc::COURSE_TO_DME_DISTANCE,
                                   proc::COURSE_TO_INTERCEPT,
                                   proc::COURSE_TO_RADIAL_TERMINATION,
                                   // proc::HOLD_TO_FIX,
                                   // proc::HOLD_TO_MANUAL_TERMINATION,
                                   // proc::HOLD_TO_ALTITUDE,
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
    // Add restrictions from last point to text
    texts.append(proc::altRestrictionTextNarrow(lastLegPoint.altRestriction));
    texts.append(proc::speedRestrictionTextNarrow(lastLegPoint.speedRestriction));
  }

  // Merge texts between approach and transition fixes ========================================
  if(index > 0)
  {
    const proc::MapProcedureLeg& nextLeg = legs.at(index - 1);

    if(nextLeg.isTransition() && leg.isApproach() && nextLeg.fixPos.almostEqual(leg.fixPos) &&
       nextLeg.fixIdent == leg.fixIdent && nextLeg.fixRegion == leg.fixRegion)
    {
      // Merge restriction texts from last transition and this approach leg
      texts.append(proc::altRestrictionTextNarrow(nextLeg.altRestriction));
      texts.append(proc::speedRestrictionTextNarrow(nextLeg.speedRestriction));
    }
  }

  // Remove duplicates and empty strings
  texts.removeDuplicates();
  texts.removeAll(QString());

  const map::MapSearchResult& navaids = leg.navaids;
  int symbolSize = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());

  if(!navaids.waypoints.isEmpty() && wToS(navaids.waypoints.first().position, x, y))
  {
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintWaypoint(context, QColor(), x, y, false);
    if(drawText)
      paintWaypointText(context, x, y, navaids.waypoints.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.vors.isEmpty() && wToS(navaids.vors.first().position, x, y))
  {
    symbolSize = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintVor(context, x, y, navaids.vors.first(), false);
    if(drawText)
      paintVorText(context, x, y, navaids.vors.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.ndbs.isEmpty() && wToS(navaids.ndbs.first().position, x, y))
  {
    symbolSize = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintNdb(context, x, y, false);
    if(drawText)
      paintNdbText(context, x, y, navaids.ndbs.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.ils.isEmpty() && wToS(navaids.ils.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintProcedurePoint(context, x, y, false);
    if(drawText)
      paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
  }
  else if(!navaids.runwayEnds.isEmpty() && wToS(navaids.runwayEnds.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintProcedurePoint(context, x, y, false);
    if(drawText)
      paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
  }
  else if(!leg.fixIdent.isEmpty() && wToS(leg.fixPos, x, y))
  {
    // Custom IF case
    texts.append(leg.fixIdent);
    if(drawText)
      paintProcedureUnderlay(context, leg, x, y, symbolSize);
    paintProcedurePoint(context, x, y, false);
    if(drawText)
      paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
  }

  // Draw left aligned texts like intercept or manual =====================================
  // if(!texts.isEmpty() && drawTextDetail)
  // paintText(context, mapcolors::routeProcedurePointColor, x - symbolSize * 3 / 2, y, texts,
  // true /* draw as route */, textatt::RIGHT);

  // Draw wind barbs for SID and STAR (not approaches) =======================================
  if(!preview && (leg.isSid() || leg.isStar()))
  {
    if(context->objectDisplayTypes.testFlag(map::WIND_BARBS_ROUTE) &&
       (NavApp::getWindReporter()->hasWindData() || NavApp::getWindReporter()->isWindManual()))
    {
      int routeIdx = index;

      // Convert from procedure index to route leg index
      if(leg.isSid())
        routeIdx += route->getSidLegsOffset();
      else if(leg.isStar())
        routeIdx += route->getStarLegsOffset();

      // Do not draw wind barbs for approaches
      const RouteAltitudeLeg& altLeg = route->getAltitudeLegAt(routeIdx);
      if(altLeg.getLineString().getPos2().getAltitude() > map::MIN_WIND_BARB_ALTITUDE)
        drawWindBarbAtWaypoint(context, altLeg.getWindSpeed(), altLeg.getWindDirection(), x, y);
    }
  }

  // Remember last painted leg for next procedure painter
  lastLegPoint = leg;
}

void MapPainterRoute::paintAirport(const PaintContext *context, int x, int y, const map::MapAirport& obj)
{
  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
  symbolPainter->drawAirportSymbol(context->painter, obj, x, y, size, false, false);
}

void MapPainterRoute::paintAirportText(const PaintContext *context, int x, int y, bool drawAsRoute,
                                       const map::MapAirport& obj)
{
  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
  symbolPainter->drawAirportText(context->painter, obj, x, y, context->dispOpts,
                                 context->airportTextFlagsRoute(drawAsRoute, false /* draw as log */), size,
                                 context->mapLayerEffective->isAirportDiagram(),
                                 context->mapLayer->getMaxTextLengthAirport());
}

void MapPainterRoute::paintVor(const PaintContext *context, int x, int y, const map::MapVor& obj, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
  symbolPainter->drawVorSymbol(context->painter, obj, x, y,
                               size, !preview, false,
                               context->mapLayerEffective->isVorLarge() ? size * 5 : 0);
}

void MapPainterRoute::paintVorText(const PaintContext *context, int x, int y, const map::MapVor& obj, bool drawAsRoute,
                                   const QStringList *additionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
  textflags::TextFlags flags = textflags::NONE;

  if(drawAsRoute)
    flags |= textflags::ROUTE_TEXT;

  // Use more more detailed VOR text for flight plan
  if(context->mapLayer->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawVorText(context->painter, obj, x, y, flags, size, fill, additionalText);
}

void MapPainterRoute::paintNdb(const PaintContext *context, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
  symbolPainter->drawNdbSymbol(context->painter, x, y, size, !preview, false);
}

void MapPainterRoute::paintNdbText(const PaintContext *context, int x, int y, const map::MapNdb& obj, bool drawAsRoute,
                                   const QStringList *additionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
  textflags::TextFlags flags = textflags::NONE;

  if(drawAsRoute)
    flags |= textflags::ROUTE_TEXT;

  // Use more more detailed NDB text for flight plan
  if(context->mapLayer->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawNdbText(context->painter, obj, x, y, flags, size, fill, additionalText);
}

void MapPainterRoute::paintWaypoint(const PaintContext *context, const QColor& col, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawWaypointSymbol(context->painter, col, x, y, size, !preview);
}

void MapPainterRoute::paintWaypointText(const PaintContext *context, int x, int y,
                                        const map::MapWaypoint& obj, bool drawAsRoute,
                                        const QStringList *additionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  textflags::TextFlags flags = textflags::NONE;

  if(drawAsRoute)
    flags |= textflags::ROUTE_TEXT;

  if(context->mapLayer->isWaypointRouteName())
    flags |= textflags::IDENT;

  bool fill = true;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawWaypointText(context->painter, obj, x, y, flags, size, fill, additionalText);
}

/* paint intermediate approach point */
void MapPainterRoute::paintProcedurePoint(const PaintContext *context, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawProcedureSymbol(context->painter, x, y, size + 3, !preview);
}

void MapPainterRoute::paintProcedureUnderlay(const PaintContext *context, const proc::MapProcedureLeg& leg,
                                             int x, int y, int size)
{
  // Ring to indicate fly over
  // Maltese cross to indicate FAF on the map
  symbolPainter->drawProcedureUnderlay(context->painter, x, y, size, leg.flyover, leg.malteseCross);
}

/* Paint user defined waypoint */
void MapPainterRoute::paintUserpoint(const PaintContext *context, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawUserpointSymbol(context->painter, x, y, size, !preview);
}

/* Draw text with light yellow background for flight plan */
void MapPainterRoute::paintText(const PaintContext *context, const QColor& color, int x, int y,
                                const QStringList& texts, bool drawAsRoute, textatt::TextAttributes atts)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());

  if(drawAsRoute)
    atts |= textatt::ROUTE_BG_COLOR;

  int transparency = 255;
  if(!(context->flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    transparency = 0;

  if(!texts.isEmpty() && context->mapLayer->isWaypointRouteName())
    symbolPainter->textBox(context->painter, texts, color, x + size / 2 + 2, y, atts, transparency);
}

void MapPainterRoute::drawSymbols(const PaintContext *context,
                                  const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints, bool preview)
{
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = atools::roundToInt(pt.x());
      int y = atools::roundToInt(pt.y());
      const RouteLeg& obj = route->value(i);
      map::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case map::INVALID:
          // name and region not found in database
          paintWaypoint(context, mapcolors::routeInvalidPointColor, x, y, preview);
          break;
        case map::USERPOINTROUTE:
          paintUserpoint(context, x, y, preview);
          break;
        case map::AIRPORT:
          paintAirport(context, x, y, obj.getAirport());
          break;
        case map::VOR:
          paintVor(context, x, y, obj.getVor(), preview);
          break;
        case map::NDB:
          paintNdb(context, x, y, preview);
          break;
        case map::WAYPOINT:
          paintWaypoint(context, QColor(), x, y, preview);
          break;
      }
    }
    i++;
  }
}

void MapPainterRoute::drawWindBarbs(const PaintContext *context,
                                    const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  if(!route->hasAltitudeLegs() || !route->hasValidProfile())
    return;

  QPointF lastPt;
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(i > route->getNumAltitudeLegs() - 1)
      break;

    if(visibleStartPoints.testBit(i))
    {
      bool distOk = lastPt.isNull() || (pt - lastPt).manhattanLength() > 50;

      const RouteAltitudeLeg& altLeg = route->getAltitudeLegAt(i);
      if(altLeg.getLineString().getPos2().getAltitude() > map::MIN_WIND_BARB_ALTITUDE && !altLeg.isMissed() &&
         !altLeg.isAlternate() && distOk)
      {
        drawWindBarbAtWaypoint(context, altLeg.getWindSpeed(), altLeg.getWindDirection(),
                               static_cast<float>(pt.x()), static_cast<float>(pt.y()));
        lastPt = pt;
      }
    }
    i++;
  }
}

void MapPainterRoute::drawRouteSymbolText(const PaintContext *context,
                                          const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = atools::roundToInt(pt.x());
      int y = atools::roundToInt(pt.y());
      const RouteLeg& obj = route->value(i);
      map::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case map::INVALID:
          paintText(context, mapcolors::routeInvalidPointColor, x, y, {obj.getIdent()}, true /* draw as route */);
          break;
        case map::USERPOINTROUTE:
          paintText(context, mapcolors::routeUserPointColor, x, y,
                    {atools::elideTextShort(obj.getIdent(), context->mapLayer->getMaxTextLengthAirport())},
                    true /* draw as route */);
          break;
        case map::AIRPORT:
          paintAirportText(context, x, y, true /* draw as route */, obj.getAirport());
          break;
        case map::VOR:
          paintVorText(context, x, y, obj.getVor(), true /* draw as route */);
          break;
        case map::NDB:
          paintNdbText(context, x, y, obj.getNdb(), true /* draw as route */);
          break;
        case map::WAYPOINT:
          paintWaypointText(context, x, y, obj.getWaypoint(), true /* draw as route */);
          break;
      }
    }
    i++;
  }
}

void MapPainterRoute::drawStartParking(const PaintContext *context)
{
  // Use a layer that is independent of the declutter factor
  if(!route->isEmpty() && context->mapLayerEffective->isAirportDiagram())
  {
    // Draw start position or parking circle into the airport diagram
    const RouteLeg& first = route->value(0);
    if(first.getMapObjectType() == map::AIRPORT)
    {
      int size = 25;

      Pos startPos;
      if(route->hasDepartureParking())
      {
        startPos = first.getDepartureParking().position;
        size = first.getDepartureParking().radius;
      }
      else if(route->hasDepartureStart())
        startPos = first.getDepartureStart().position;

      if(startPos.isValid())
      {
        QPoint pt = wToS(startPos);

        if(!pt.isNull())
        {
          // At least 10 pixel size - unaffected by scaling
          int w = std::max(10, scale->getPixelIntForFeet(size, 90));
          int h = std::max(10, scale->getPixelIntForFeet(size, 0));

          context->painter->setPen(QPen(mapcolors::routeOutlineColor, 9,
                                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          context->painter->drawEllipse(pt, w, h);
          context->painter->setPen(QPen(OptionData::instance().getFlightplanColor(), 5,
                                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          context->painter->drawEllipse(pt, w, h);
        }
      }
    }
  }
}

void MapPainterRoute::drawWindBarbAtWaypoint(const PaintContext *context, float windSpeed, float windDir,
                                             float x, float y)
{
  if(!route->hasAltitudeLegs() || !route->hasValidProfile())
    return;

  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getWindBarbsSymbolSize());
  symbolPainter->drawWindBarbs(context->painter, windSpeed, 0.f /* gust */, windDir, x - 5, y - 5, size,
                               true /* barbs */, true /* alt wind */, true /* route */, context->drawFast);
}
