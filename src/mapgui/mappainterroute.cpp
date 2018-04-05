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

#include "mapgui/mappainterroute.h"

#include "mapgui/mapwidget.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "route/routecontroller.h"
#include "mapgui/mapscale.h"
#include "util/paintercontextsaver.h"
#include "common/textplacement.h"

#include <QBitArray>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using proc::MapProcedureLeg;
using proc::MapProcedureLegs;
using map::PosCourse;
using atools::contains;

MapPainterRoute::MapPainterRoute(MapWidget *mapWidget, MapScale *mapScale, const Route *routeParam)
  : MapPainter(mapWidget, mapScale), route(routeParam)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::render(PaintContext *context)
{
  // Draw route including approaches
  if(context->objectTypes.testFlag(map::FLIGHTPLAN))
    paintRoute(context);

  // Draw the approach preview if any selected in the search tab
  proc::MapProcedureLeg lastLegPoint;
  if(context->mapLayer->isApproach())
    paintProcedure(lastLegPoint, context, mapWidget->getProcedureHighlight(), 0, mapcolors::routeProcedurePreviewColor,
                   true /* preview */);

  if(context->objectTypes & map::FLIGHTPLAN && OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
    paintTopOfDescent(context);
}

void MapPainterRoute::paintRoute(const PaintContext *context)
{
  if(route->isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver);

  context->painter->setBrush(Qt::NoBrush);

  drawStartParking(context);

  // Collect line text and geometry from the route
  QStringList routeTexts;
  QVector<Line> lines;

  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);

  // qDebug() << "AL" << route->getActiveLegIndex();
  // qDebug() << "RS" << route->size();

  // Get active route leg
  bool activeValid = route->isActiveValid();
  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg = activeValid ? route->getActiveLegIndex() : 0;
  int passedRouteLeg = context->flags2 & opts::MAP_ROUTE_DIM_PASSED ? activeRouteLeg : 0;

  // Collect route only coordinates and texts ===============================
  for(int i = 1; i < route->size(); i++)
  {
    const RouteLeg& leg = route->at(i);
    const RouteLeg& last = route->at(i - 1);

    // Draw if at least one is part of the route - also draw empty spaces between procedures
    if(
      last.isRoute() || leg.isRoute() || // Draw if any is route - also covers STAR to airport
      (last.getProcedureLeg().isAnyDeparture() && leg.getProcedureLeg().isAnyArrival()) || // empty space from SID to STAR, transition or approach
      (last.getProcedureLeg().isStar() && leg.getProcedureLeg().isArrival()) // empty space from STAR to transition or approach
      )
    {
      if(i >= passedRouteLeg)
        routeTexts.append(Unit::distNm(leg.getDistanceTo(), true /*addUnit*/, 20, true /*narrow*/) + tr(" / ") +
                          QString::number(leg.getCourseToRhumbMag(), 'f', 0) + tr("°M"));
      else
        // No texts for passed legs
        routeTexts.append(QString());

      lines.append(Line(last.getPosition(), leg.getPosition()));
    }
    else
    {
      // Text and lines are drawn by paintApproach
      routeTexts.append(QString());
      lines.append(Line());
    }
  }

  QPainter *painter = context->painter;

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBackground(mapcolors::routeOutlineColor);
  painter->setBrush(Qt::NoBrush);

  if(!lines.isEmpty()) // Do not draw a line from airport to runway end
  {
    if(route->hasAnyArrivalProcedure())
    {
      lines.last() = Line();
      routeTexts.last().clear();
    }
    if(route->hasAnyDepartureProcedure())
    {
      lines.first() = Line();
      routeTexts.first().clear();
    }

    const OptionData& od = OptionData::instance();
    QPen routePen(od.getFlightplanColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen routeOutlinePen(mapcolors::routeOutlineColor, outerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen routePassedPen(od.getFlightplanPassedSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin);

    // Draw gray line for passed legs
    painter->setPen(routePassedPen);
    for(int i = 0; i < passedRouteLeg; i++)
      drawLine(context, lines.at(i));

    // Draw background for legs ahead
    painter->setPen(routeOutlinePen);
    for(int i = passedRouteLeg; i < lines.size(); i++)
      drawLine(context, lines.at(i));

    // Draw center line for legs ahead
    painter->setPen(routePen);
    for(int i = passedRouteLeg; i < lines.size(); i++)
      drawLine(context, lines.at(i));

    if(activeValid)
    {
      // Draw active leg on top of all others to keep it visible
      painter->setPen(routeOutlinePen);
      drawLine(context, lines.at(activeRouteLeg - 1));

      painter->setPen(QPen(OptionData::instance().getFlightplanActiveSegmentColor(), innerlinewidth,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      drawLine(context, lines.at(activeRouteLeg - 1));
    }
  }

  context->szFont(context->textSizeFlightplan * 1.1f);

  // Collect coordinates for text placement and lines first
  LineString positions;
  for(int i = 0; i < route->size(); i++)
    positions.append(route->at(i).getPosition());

  TextPlacement textPlacement(painter, this);
  textPlacement.setDrawFast(context->drawFast);
  textPlacement.setLineWidth(outerlinewidth);
  textPlacement.calculateTextPositions(positions);
  textPlacement.calculateTextAlongLines(lines, routeTexts);
  painter->save();
  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
    painter->setBackgroundMode(Qt::TransparentMode);
  else
    painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setBackground(mapcolors::routeTextBackgroundColor);
  painter->setPen(mapcolors::routeTextColor);
  textPlacement.drawTextAlongLines();
  painter->restore();
  painter->setBackgroundMode(Qt::OpaqueMode);

  context->szFont(context->textSizeFlightplan);
  // ================================================================================

  QBitArray visibleStartPoints = textPlacement.getVisibleStartPoints();
  for(int i = 0; i < route->size(); i++)
  {
    // Make all approach points except the last one invisible to avoid text and symbol overlay over approach
    if(route->at(i).isAnyProcedure())
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

  // Remember last point across procedures to avoid overlaying text
  proc::MapProcedureLeg lastLegPoint;

  // Draw arrival and departure procedures ============================
  const QColor& flightplanProcedureColor = OptionData::instance().getFlightplanProcedureColor();
  if(route->hasAnyDepartureProcedure())
    paintProcedure(lastLegPoint, context, route->getDepartureLegs(), route->getDepartureLegsOffset(),
                   flightplanProcedureColor, false /* preview */);

  if(route->hasAnyStarProcedure())
    paintProcedure(lastLegPoint, context, route->getStarLegs(), route->getStarLegsOffset(),
                   flightplanProcedureColor, false /* preview */);

  if(route->hasAnyArrivalProcedure())
    paintProcedure(lastLegPoint, context, route->getArrivalLegs(), route->getArrivalLegsOffset(),
                   flightplanProcedureColor, false /* preview */);

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

void MapPainterRoute::paintTopOfDescent(const PaintContext *context)
{
  if(route->size() >= 2)
  {
    atools::util::PainterContextSaver saver(context->painter);
    Q_UNUSED(saver);

    // Draw the top of descent circle and text
    QPoint pt = wToS(route->getTopOfDescent());
    if(!pt.isNull())
    {
      float width = context->sz(context->thicknessFlightplan, 3);
      int radius = atools::roundToInt(context->sz(context->thicknessFlightplan, 6));

      context->painter->setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
      context->painter->drawEllipse(pt, radius, radius);

      QStringList tod;
      tod.append(tr("TOD"));
      if(context->mapLayer->isAirportRouteInfo())
        tod.append(Unit::distNm(route->getTopOfDescentFromDestination()));

      int transparency = 255;
      if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
        transparency = 0;

      symbolPainter->textBox(context->painter, tod, QPen(mapcolors::routeTextColor),
                             pt.x() + radius, pt.y() + radius,
                             textatt::ROUTE_BG_COLOR | textatt::BOLD, transparency);
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
  Q_UNUSED(saver);

  context->painter->setBackgroundMode(Qt::OpaqueMode);
  context->painter->setBackground(Qt::white);
  context->painter->setBrush(Qt::NoBrush);

  // Get active approach leg
  bool activeValid = route->isActiveValid();
  int activeProcLeg = activeValid ? route->getActiveLegIndex() - legsRouteOffset : 0;
  int passedProcLeg = context->flags2 & opts::MAP_ROUTE_DIM_PASSED ? activeProcLeg : 0;

  // Draw black background ========================================
  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);
  QLineF lastLine, lastActiveLine;

  painter->setPen(QPen(mapcolors::routeProcedurePreviewOutlineColor, outerlinewidth,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(int i = 0; i < legs.size(); i++)
  {
    // Do not draw background for passed legs but calculate lastLine
    bool draw = i >= passedProcLeg || !activeValid || preview;
    paintApproachSegment(context, legs, i, lastLine, nullptr, true /* no text */, preview, draw);
  }

  QPen missedPen(color, innerlinewidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  QPen apprPen(color, innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  const OptionData& od = OptionData::instance();
  QPen missedActivePen(od.getFlightplanActiveSegmentColor(), innerlinewidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  QPen apprActivePen(od.getFlightplanActiveSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  QPen routePassedPen(od.getFlightplanPassedSegmentColor(), innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  lastLine = QLineF();
  QVector<DrawText> drawTextLines;
  drawTextLines.fill({Line(), false, false}, legs.size());

  // Draw segments and collect text placement information in drawTextLines ========================================
  // Need to set font since it is used by drawHold
  context->szFont(context->textSizeFlightplan * 1.1f);

  // Paint legs
  for(int i = 0; i < legs.size(); i++)
  {
    bool noText = context->drawFast;
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
      // Remember for drawing the active one
      lastActiveLine = lastLine;

    paintApproachSegment(context, legs, i, lastLine, &drawTextLines, noText, preview, true /* draw */);
  }

  // Paint active leg if in range for this procedure
  if(!preview && activeValid && activeProcLeg >= 0 && activeProcLeg < legs.size())
  {
    painter->setPen(legs.at(activeProcLeg).isMissed() ? missedActivePen : apprActivePen);
    paintApproachSegment(context, legs, activeProcLeg, lastActiveLine, &drawTextLines, context->drawFast, preview,
                         true /* draw */);
  }

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
        QString approachText;
        if(drawTextLines.at(i).distance)
          approachText.append(Unit::distNm(leg.calculatedDistance, true /*addUnit*/, 20, true /*narrow*/));

        if(drawTextLines.at(i).course)
        {
          if(leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
          {
            if(!approachText.isEmpty())
              approachText.append(tr("/"));
            approachText +=
              (QString::number(atools::geo::normalizeCourse(leg.calculatedTrueCourse - leg.magvar), 'f', 0) +
               tr("°M"));
          }
        }

        approachTexts.append(approachText);
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

  // Texts and navaid icons ====================================================
  for(int i = 0; i < legs.size(); i++)
  {
    // No text labels for passed points
    bool drawText = i + 1 >= passedProcLeg || !activeValid || preview;
    paintApproachPoint(lastLegPoint, context, legs, i, preview, drawText);
  }
}

void MapPainterRoute::paintApproachSegment(const PaintContext *context, const proc::MapProcedureLegs& legs,
                                           int index, QLineF& lastLine, QVector<DrawText> *drawTextLines,
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
  bool hiddenDummy;
  wToS(leg.line, line, size, &hiddenDummy);

  if(leg.disabled)
    return;

  if(leg.type == proc::INITIAL_FIX)
  {
    // Nothing to do here
    lastLine = line;
    return;
  }

  if(leg.type == proc::START_OF_PROCEDURE &&
     // START_OF_PROCEDURE is an actual leg for departure where it connects runway and initial fix
     !(leg.mapType & proc::PROCEDURE_DEPARTURE))
  {
    // Nothing to do here
    lastLine = line;
    return;
  }

  QPointF intersectPoint = wToS(leg.interceptPos, size, &hiddenDummy);

  QPainter *painter = context->painter;

  bool showDistance = !leg.noDistanceDisplay();

  // ===========================================================
  if(contains(leg.type, {proc::ARC_TO_FIX, proc::CONSTANT_RADIUS_ARC}))
  {
    if(line.length() > 2 && leg.recFixPos.isValid())
    {
      if(draw)
      {
        QPointF point = wToS(leg.recFixPos, size, &hiddenDummy);
        paintArc(context->painter, line.p1(), line.p2(), point, leg.turnDirection == "L");
      }
    }
    else
    {
      if(draw)
        painter->drawLine(line);
      lastLine = line;

      if(drawTextLines != nullptr)
        // Can draw a label along the line
        (*drawTextLines)[index] = {leg.line, showDistance, true};
    }

    lastLine = line;
  }
  // ===========================================================
  else if(contains(leg.type, {proc::COURSE_TO_ALTITUDE,
                              proc::COURSE_TO_FIX,
                              proc::DIRECT_TO_FIX,
                              proc::FIX_TO_ALTITUDE,
                              proc::TRACK_TO_FIX,
                              proc::TRACK_FROM_FIX_FROM_DISTANCE,
                              proc::FROM_FIX_TO_MANUAL_TERMINATION,
                              proc::HEADING_TO_ALTITUDE_TERMINATION,
                              proc::HEADING_TO_MANUAL_TERMINATION,
                              proc::COURSE_TO_INTERCEPT,
                              proc::HEADING_TO_INTERCEPT}))
  {
    if(contains(leg.turnDirection,
                {"R", "L" /*, "B"*/}) && prevLeg != nullptr && prevLeg->type != proc::INITIAL_FIX &&
       prevLeg->type != proc::START_OF_PROCEDURE)
    {
      float lineDist = static_cast<float>(QLineF(lastLine.p2(), line.p1()).length());
      if(!lastLine.p2().isNull() && lineDist > 2)
      {
        // Lines are not connected which can happen if a CF follows after a FD or similar
        paintApproachBow(prevLeg, lastLine, painter, line, leg, draw);

        if(drawTextLines != nullptr)
          // Can draw a label along the line with course but not distance
          (*drawTextLines)[index] = {leg.line, false, true};
      }
      else
      {
        // Lines are connected but a turn direction is given
        // Draw a small arc if a turn direction is given

        QLineF nextLine = paintApproachTurn(lastLine, line, leg, painter, intersectPoint, draw);

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
          painter->drawLine(line.p1(), intersectPoint);
          painter->drawLine(intersectPoint, line.p2());
        }
        lastLine = QLineF(intersectPoint, line.p2());

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = {Line(leg.interceptPos, leg.line.getPos2()), showDistance, true};
      }
      else
      {
        if(draw)
        {
          if(!lastLine.p2().isNull() && QLineF(lastLine.p2(), line.p1()).length() > 2)
            // Close any gaps in the lines
            painter->drawLine(lastLine.p2(), line.p1());

          painter->drawLine(line);
        }
        lastLine = line;

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
                              proc::DIRECT_TO_RUNWAY}))
  {
    if(draw)
      painter->drawLine(line);
    lastLine = line;

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

    QPointF pc = wToS(leg.procedureTurnPos, size, &hiddenDummy);

    if(draw)
      paintProcedureTurnWithText(painter, static_cast<float>(pc.x()), static_cast<float>(pc.y()),
                                 trueCourse, leg.distance,
                                 leg.turnDirection == "L", &lastLine, text,
                                 leg.missed ? mapcolors::routeProcedureMissedTextColor : mapcolors::
                                 routeProcedureTextColor,
                                 mapcolors::routeTextBackgroundColor);

    if(draw)
      painter->drawLine(line.p1(), pc);

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = {Line(leg.line.getPos1(), leg.procedureTurnPos), showDistance, true};
  }
}

void MapPainterRoute::paintApproachBow(const proc::MapProcedureLeg *prevLeg, QLineF& lastLine, QPainter *painter,
                                       QLineF line, const proc::MapProcedureLeg& leg, bool draw)
{
  if(!prevLeg->line.getPos2().isValid() || !leg.line.getPos1().isValid())
    return;

  QLineF lineDraw(line.p2(), line.p1());

  // Shorten the next line to get a better curve - use a value less than 1 nm to avoid flickering on 1 nm legs
  float oneNmPixel = scale->getPixelForNm(0.95f);
  if(lineDraw.length() > oneNmPixel * 2.f)
    lineDraw.setLength(lineDraw.length() - oneNmPixel);
  lineDraw.setPoints(lineDraw.p2(), lineDraw.p1());

  // Calculate distance to control points
  float dist = prevLeg->line.getPos2().distanceMeterToRhumb(leg.line.getPos1());

  if(dist < atools::geo::Pos::INVALID_VALUE)
  {
    dist = dist * 3 / 4;

    // Calculate bezier control points by extending the last and next line
    QLineF ctrl1(lastLine.p1(), lastLine.p2());
    ctrl1.setLength(ctrl1.length() + scale->getPixelForMeter(dist));
    QLineF ctrl2(lineDraw.p2(), lineDraw.p1());
    ctrl2.setLength(ctrl2.length() + scale->getPixelForMeter(dist));

    // Draw a bow connecting the two lines
    if(draw)
    {
      QPainterPath path;
      path.moveTo(lastLine.p2());
      path.cubicTo(ctrl1.p2(), ctrl2.p2(), lineDraw.p1());
      painter->drawPath(path);

      painter->drawLine(lineDraw);
    }
    lastLine = lineDraw;
  }
}

QLineF MapPainterRoute::paintApproachTurn(QLineF& lastLine, QLineF line, const proc::MapProcedureLeg& leg,
                                          QPainter *painter, QPointF intersectPoint, bool draw)
{
  QPointF endPos = line.p2();
  if(leg.interceptPos.isValid())
    endPos = intersectPoint;

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
    painter->drawLine(nextLine);

    if(leg.interceptPos.isValid())
      // Add line from intercept leg
      painter->drawLine(endPos, line.p2());
  }

  lastLine = nextLine.toLine();

  return nextLine;
}

void MapPainterRoute::paintApproachPoint(proc::MapProcedureLeg& lastLegPoint, const PaintContext *context,
                                         const proc::MapProcedureLegs& legs, int index, bool preview, bool drawText)
{
  const proc::MapProcedureLeg& leg = legs.at(index);
  bool drawTextDetail = context->mapLayer->isApproachTextAndDetail() && drawText;

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

    painter->drawLine(pt1, pt2);
  }

  painter->setPen(QPen(Qt::darkBlue, 10));
  painter->drawLine(holdLine);

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

  bool lastInTransition = false;
  if(index < legs.size() - 1 &&
     leg.type != proc::HEADING_TO_INTERCEPT && leg.type != proc::COURSE_TO_INTERCEPT &&
     leg.type != proc::HOLD_TO_ALTITUDE && leg.type != proc::HOLD_TO_FIX &&
     leg.type != proc::HOLD_TO_MANUAL_TERMINATION)
    // Do not paint the last point in the transition since it overlaps with the approach
    // But draw the intercept and hold text
    lastInTransition = legs.at(index).isTransition() &&
                       legs.at(index + 1).isApproach() && context->objectTypes & proc::PROCEDURE_APPROACH;

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
      if(leg.flyover && drawTextDetail)
        paintProcedureFlyover(context, x, y, defaultOverflySize);
      paintProcedurePoint(context, x, y, false);
      if(drawTextDetail)
        paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
      texts.clear();
    }
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
      texts.append(proc::altRestrictionTextNarrow(leg.altRestriction));
      texts.append(proc::speedRestrictionTextNarrow(leg.speedRestriction));
      if(leg.flyover && drawTextDetail)
        paintProcedureFlyover(context, x, y, defaultOverflySize);
      paintProcedurePoint(context, x, y, false);
      if(drawTextDetail)
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
        if(leg.flyover && drawTextDetail)
          paintProcedureFlyover(context, x, y, defaultOverflySize);
        paintProcedurePoint(context, x, y, false);
      }
    }
    else if(leg.interceptPos.isValid())
    {
      if(wToS(leg.interceptPos, x, y))
      {
        // Draw intercept comment - no altitude restriction there
        texts.append(leg.displayText);
        if(leg.flyover && drawTextDetail)
          paintProcedureFlyover(context, x, y, defaultOverflySize);
        paintProcedurePoint(context, x, y, false);
        if(drawTextDetail)
          paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
      }

      // Clear text from "intercept" and add restrictions
      texts.clear();
      texts.append(proc::altRestrictionTextNarrow(leg.altRestriction));
      texts.append(proc::speedRestrictionTextNarrow(leg.speedRestriction));
    }
    else
    {
      if(lastInTransition)
        return;

      texts.append(leg.displayText);
      texts.append(proc::altRestrictionTextNarrow(leg.altRestriction));
      texts.append(proc::speedRestrictionTextNarrow(leg.speedRestriction));
    }
  }
  else
  {
    if(lastInTransition)
      return;

    texts.append(leg.displayText);
    texts.append(proc::altRestrictionTextNarrow(leg.altRestriction));
    texts.append(proc::speedRestrictionTextNarrow(leg.speedRestriction));
  }

  // Merge restricions between overlapping fixes across procedures
  if(lastLegPoint.isValid() && lastLegPoint.fixPos.almostEqual(leg.fixPos) &&
     lastLegPoint.fixIdent == leg.fixIdent && lastLegPoint.fixRegion == leg.fixRegion)
  {
    // Add restrictions from last point to text
    texts.append(proc::altRestrictionTextNarrow(lastLegPoint.altRestriction));
    texts.append(proc::speedRestrictionTextNarrow(lastLegPoint.speedRestriction));
  }

  // Remove duplicates and empty strings
  texts.removeDuplicates();
  texts.removeAll(QString());

  const map::MapSearchResult& navaids = leg.navaids;

  if(!navaids.waypoints.isEmpty() && wToS(navaids.waypoints.first().position, x, y))
  {
    if(leg.flyover && drawTextDetail)
      paintProcedureFlyover(context, x, y,
                            context->sz(context->symbolSizeNavaid,
                                        context->mapLayerEffective->getWaypointSymbolSize()));
    paintWaypoint(context, QColor(), x, y, false);
    if(drawTextDetail)
      paintWaypointText(context, x, y, navaids.waypoints.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.vors.isEmpty() && wToS(navaids.vors.first().position, x, y))
  {
    if(leg.flyover && drawTextDetail)
      paintProcedureFlyover(context, x, y,
                            context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize()));
    paintVor(context, x, y, navaids.vors.first(), false);
    if(drawTextDetail)
      paintVorText(context, x, y, navaids.vors.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.ndbs.isEmpty() && wToS(navaids.ndbs.first().position, x, y))
  {
    if(leg.flyover && drawTextDetail)
      paintProcedureFlyover(context, x, y,
                            context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize()));
    paintNdb(context, x, y, false);
    if(drawTextDetail)
      paintNdbText(context, x, y, navaids.ndbs.first(), true /* draw as route */, &texts);
  }
  else if(!navaids.ils.isEmpty() && wToS(navaids.ils.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    if(leg.flyover && drawTextDetail)
      paintProcedureFlyover(context, x, y,
                            context->sz(context->symbolSizeNavaid,
                                        context->mapLayerEffective->getWaypointSymbolSize()));
    paintProcedurePoint(context, x, y, false);
    if(drawTextDetail)
      paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
  }
  else if(!navaids.runwayEnds.isEmpty() && wToS(navaids.runwayEnds.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    if(leg.flyover && drawTextDetail)
      paintProcedureFlyover(context, x, y,
                            context->sz(context->symbolSizeNavaid,
                                        context->mapLayerEffective->getWaypointSymbolSize()));
    paintProcedurePoint(context, x, y, false);
    if(drawTextDetail)
      paintText(context, mapcolors::routeProcedurePointColor, x, y, texts, true /* draw as route */);
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
  textflags::TextFlags flags = textflags::IDENT;

  if(drawAsRoute)
    flags |= textflags::ROUTE_TEXT;

  // Use more more detailed text for flight plan
  if(context->mapLayer->isAirportRouteInfo())
    flags |= textflags::NAME | textflags::INFO;

  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
    flags |= textflags::NO_BACKGROUND;

  symbolPainter->drawAirportText(context->painter, obj, x, y, context->dispOpts, flags, size,
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
  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
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
  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
  {
    flags |= textflags::NO_BACKGROUND;
    fill = false;
  }

  symbolPainter->drawNdbText(context->painter, obj, x, y, flags, size, fill, additionalText);
}

void MapPainterRoute::paintWaypoint(const PaintContext *context, const QColor& col, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawWaypointSymbol(context->painter, col, x, y, size, !preview, false);
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
  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
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
  symbolPainter->drawProcedureSymbol(context->painter, x, y, size + 3, !preview, false);
}

void MapPainterRoute::paintProcedureFlyover(const PaintContext *context, int x, int y, int size)
{
  symbolPainter->drawProcedureFlyover(context->painter, x, y, size + 14);
}

/* Paint user defined waypoint */
void MapPainterRoute::paintUserpoint(const PaintContext *context, int x, int y, bool preview)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawUserpointSymbol(context->painter, x, y, size, !preview, false);
}

/* Draw text with light yellow background for flight plan */
void MapPainterRoute::paintText(const PaintContext *context, const QColor& color, int x, int y,
                                const QStringList& texts, bool drawAsRoute)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());

  textatt::TextAttributes atts = textatt::BOLD;

  if(drawAsRoute)
    atts |= textatt::ROUTE_BG_COLOR;

  int transparency = 255;
  if(!(context->flags2 & opts::MAP_ROUTE_TEXT_BACKGROUND))
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
      const RouteLeg& obj = route->at(i);
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
      const RouteLeg& obj = route->at(i);
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
    const RouteLeg& first = route->at(0);
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
