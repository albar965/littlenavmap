/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

// #define DEBUG_APPROACH_PAINT

using namespace Marble;
using namespace atools::geo;
using maptypes::MapApproachLeg;
using maptypes::MapApproachLegs;
using atools::contains;

MapPainterRoute::MapPainterRoute(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                                 RouteController *controller)
  : MapPainter(mapWidget, mapQuery, mapScale), routeController(controller)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::render(PaintContext *context)
{
  if(context->objectTypes.testFlag(maptypes::ROUTE))
  {
    setRenderHints(context->painter);

    atools::util::PainterContextSaver saver(context->painter);
    Q_UNUSED(saver);
    paintRoute(context);
  }

  if(context->mapLayer->isAirport())
    paintApproachPreview(context, mapWidget->getApproachHighlight());
}

void MapPainterRoute::paintRoute(const PaintContext *context)
{
  const RouteMapObjectList& routeMapObjects = routeController->getRouteMapObjects();

  if(routeMapObjects.isEmpty())
    return;

  context->painter->setBrush(Qt::NoBrush);

  // Use a layer that is independent of the declutter factor
  if(!routeMapObjects.isEmpty() && context->mapLayerEffective->isAirportDiagram())
  {
    // Draw start position or parking circle into the airport diagram
    const RouteMapObject& first = routeMapObjects.at(0);
    if(first.getMapObjectType() == maptypes::AIRPORT)
    {
      int size = 100;

      Pos startPos;
      if(routeMapObjects.hasDepartureParking())
      {
        startPos = first.getDepartureParking().position;
        size = first.getDepartureParking().radius;
      }
      else if(routeMapObjects.hasDepartureStart())
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

  // Check if there is any magnetic variance on the route
  bool foundMagvarObject = false;
  for(const RouteMapObject& obj : routeMapObjects)
  {
    // Route contains correct magvar if any of these objects were found
    if(obj.getMapObjectType() == maptypes::AIRPORT || obj.getMapObjectType() == maptypes::VOR ||
       obj.getMapObjectType() == maptypes::NDB || obj.getMapObjectType() == maptypes::WAYPOINT)
      foundMagvarObject = true;
  }

  // Collect line text and geometry from the route
  QStringList routeTexts;
  QVector<Line> lines;
  GeoDataLineString linestring;
  linestring.setTessellate(true);
  for(int i = 0; i < routeMapObjects.size(); i++)
  {
    const RouteMapObject& obj = routeMapObjects.at(i);
    if(i > 0)
    {
      // Build text
      routeTexts.append(Unit::distNm(obj.getDistanceTo(), true /*addUnit*/, 20, true /*narrow*/) + tr(" / ") +
                        QString::number(obj.getCourseToRhumb(), 'f', 0) +
                        (foundMagvarObject ? tr("°M") : tr("°T")));

      lines.append(Line(routeMapObjects.at(i - 1).getPosition(), obj.getPosition()));
    }
    linestring.append(GeoDataCoordinates(obj.getPosition().getLonX(), obj.getPosition().getLatY(), 0, DEG));
  }

  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);

  // Draw lines separately to avoid omission in mercator near anti meridian
  // Draw outer line
  context->painter->setPen(QPen(mapcolors::routeOutlineColor, outerlinewidth, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
  drawLineString(context, linestring);

  const Pos& pos = mapWidget->getUserAircraft().getPosition();

  // Get active route leg
  int activeLeg = -1;
  if(pos.isValid())
  {
    float cross;
    activeLeg = routeMapObjects.getNearestLegIndex(pos, cross);
  }

  // Draw innner line
  context->painter->setPen(QPen(OptionData::instance().getFlightplanColor(), innerlinewidth,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  GeoDataLineString ls;
  ls.setTessellate(true);
  for(int i = 1; i < linestring.size(); i++)
  {
    ls.clear();
    ls << linestring.at(i - 1) << linestring.at(i);

    if(activeLeg != -1 && i == activeLeg)
    {
      context->painter->setPen(QPen(OptionData::instance().getFlightplanActiveSegmentColor(), innerlinewidth,
                                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      context->painter->drawPolyline(ls);
      context->painter->setPen(QPen(OptionData::instance().getFlightplanColor(), innerlinewidth,
                                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }
    else
      context->painter->drawPolyline(ls);
  }

  context->szFont(context->textSizeFlightplan * 1.1f);

  // Collect coordinates for text placement and lines first
  TextPlacement textPlacement(context->painter, this);
  textPlacement.setDrawFast(context->drawFast);
  textPlacement.setLineWidth(outerlinewidth);
  textPlacement.calculateTextAlongLines(lines, routeTexts);
  context->painter->save();
  context->painter->setBackgroundMode(Qt::OpaqueMode);
  context->painter->setBackground(mapcolors::routeTextBackgroundColor);
  context->painter->setPen(mapcolors::routeTextColor);
  textPlacement.drawTextAlongLines();
  context->painter->restore();

  context->szFont(context->textSizeFlightplan);
  // ================================================================================

  // Draw airport and navaid symbols
  drawSymbols(context, routeMapObjects, textPlacement.getVisibleStartPoints(), textPlacement.getStartPoints());

  // Draw symbol text
  drawSymbolText(context, routeMapObjects, textPlacement.getVisibleStartPoints(), textPlacement.getStartPoints());

  if(routeMapObjects.size() >= 2)
  {
    // Draw the top of descent circle and text
    QPoint pt = wToS(routeMapObjects.getTopOfDescent());
    if(!pt.isNull())
    {
      float width = context->sz(context->thicknessFlightplan, 3);
      int radius = atools::roundToInt(context->sz(context->thicknessFlightplan, 6));

      context->painter->setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
      context->painter->drawEllipse(pt, radius, radius);

      QStringList tod;
      tod.append(tr("TOD"));
      if(context->mapLayer->isAirportRouteInfo())
        tod.append(Unit::distNm(routeMapObjects.getTopOfDescentFromDestination()));

      symbolPainter->textBox(context->painter, tod, QPen(mapcolors::routeTextColor),
                             pt.x() + radius, pt.y() + radius,
                             textatt::ROUTE_BG_COLOR | textatt::BOLD, 255);
    }
  }
}

/* Draw approaches and transitions selected in the tree view */
void MapPainterRoute::paintApproachPreview(const PaintContext *context, const maptypes::MapApproachLegs& legs)
{
  if(legs.isEmpty() || !legs.bounding.overlaps(context->viewportRect))
    return;

  // Draw white background ========================================
  float outerlinewidth = context->sz(context->thicknessFlightplan, 7);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);
  QLineF lastLine;

  context->painter->setPen(QPen(mapcolors::routeApproachPreviewOutlineColor, outerlinewidth,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(int i = 0; i < legs.size(); i++)
    paintApproachSegment(context, legs, i, lastLine, nullptr, true);

  context->painter->setBackgroundMode(Qt::OpaqueMode);
  context->painter->setBackground(Qt::white);

  QPen missedPen(mapcolors::routeApproachPreviewColor, innerlinewidth, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
  QPen apprPen(mapcolors::routeApproachPreviewColor, innerlinewidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  lastLine = QLineF();
  QVector<DrawText> drawTextLines;
  drawTextLines.fill({Line(), false, false}, legs.size());

  // Draw segments and collect text placement information in drawTextLines ========================================
  // Need to set font since it is used by drawHold
  context->szFont(context->textSizeFlightplan * 1.1);
  for(int i = 0; i < legs.size(); i++)
  {
    if(legs.isMissed(i))
      context->painter->setPen(missedPen);
    else
      context->painter->setPen(apprPen);
    paintApproachSegment(context, legs, i, lastLine, &drawTextLines, false);
  }

  // Build text strings for along the line ===========================
  QStringList approachTexts;
  QVector<Line> lines;
  QVector<QColor> textColors;
  for(int i = 0; i < legs.size(); i++)
  {
    const maptypes::MapApproachLeg& leg = legs.at(i);
    lines.append(leg.line);

    QString approachText;

    if(drawTextLines.at(i).distance)
    {
      float dist = leg.calculatedDistance;
      if(leg.type == maptypes::PROCEDURE_TURN)
        dist /= 2.f;

      approachText.append(Unit::distNm(dist, true /*addUnit*/, 20, true /*narrow*/));
    }

    if(drawTextLines.at(i).course)
    {
      if(!approachText.isEmpty())
        approachText.append(tr("/"));
      approachText += (QString::number(atools::geo::normalizeCourse(leg.calculatedTrueCourse - leg.magvar), 'f', 0) +
                       tr("°M"));
    }

    approachTexts.append(approachText);

    textColors.append(leg.missed ? mapcolors::routeApproachMissedTextColor : mapcolors::routeApproachTextColor);
  }

  // Draw text along lines ====================================================
  context->painter->setBackground(mapcolors::routeTextBackgroundColor);

  QVector<Line> textLines;
  for(const DrawText& dt : drawTextLines)
    textLines.append(dt.line);

  TextPlacement textPlacement(context->painter, this);
  textPlacement.setDrawFast(context->drawFast);
  textPlacement.setTextOnTopOfLine(false);
  textPlacement.setLineWidth(outerlinewidth);
  textPlacement.setColors(textColors);
  textPlacement.calculateTextAlongLines(textLines, approachTexts);
  textPlacement.drawTextAlongLines();

  context->szFont(context->textSizeFlightplan);

  // Texts and navaid icons ====================================================
  for(int i = 0; i < legs.size(); i++)
    paintApproachPoints(context, legs, i);
}

void MapPainterRoute::paintApproachSegment(const PaintContext *context, const maptypes::MapApproachLegs& legs,
                                           int index, QLineF& lastLine, QVector<DrawText> *drawTextLines,
                                           bool background)
{
  if((!(context->objectTypes & maptypes::APPROACH_TRANSITION) && legs.isTransition(index)) ||
     (!(context->objectTypes & maptypes::APPROACH) && legs.isApproach(index)) ||
     (!(context->objectTypes & maptypes::APPROACH_MISSED) && legs.isMissed(index)))
    return;

  const maptypes::MapApproachLeg& leg = legs.at(index);

  const maptypes::MapApproachLeg *prevLeg = index > 0 ? &legs.at(index - 1) : nullptr;

  QSize size = scale->getScreeenSizeForRect(legs.bounding);

  if(!leg.line.isValid())
  {
    // qWarning() << "leg line" << index << leg.line << "is invalid";
    return;
  }

  // Use visible dummy here since we need to call the method that also returns coordinates outside the screen
  QLineF line;
  bool hiddenDummy;
  wToS(leg.line, line, size, &hiddenDummy);

  if(leg.disabled)
    return;

  if(leg.type == maptypes::INITIAL_FIX) // Nothing to do here
  {
    lastLine = line;
    return;
  }

  QPointF point = wToS(leg.recFixPos, size, &hiddenDummy);
  QPointF intersectPoint = wToS(leg.interceptPos, size, &hiddenDummy);

  QPainter *painter = context->painter;

  bool showDistance = !contains(leg.type, {maptypes::COURSE_TO_ALTITUDE,
                                           maptypes::FIX_TO_ALTITUDE,
                                           maptypes::FROM_FIX_TO_MANUAL_TERMINATION,
                                           maptypes::HEADING_TO_ALTITUDE_TERMINATION,
                                           maptypes::HEADING_TO_MANUAL_TERMINATION});

  // ===========================================================
  if(contains(leg.type, {maptypes::ARC_TO_FIX, maptypes::CONSTANT_RADIUS_ARC}))
  {
    if(line.length() > 2)
      paintArc(context->painter, line.p1(), line.p2(), point, leg.turnDirection == "L");
    lastLine = line;
  }
  // ===========================================================
  else if(contains(leg.type, {maptypes::COURSE_TO_ALTITUDE,
                              maptypes::COURSE_TO_FIX,
                              maptypes::DIRECT_TO_FIX,
                              maptypes::FIX_TO_ALTITUDE,
                              maptypes::TRACK_TO_FIX,
                              maptypes::TRACK_FROM_FIX_FROM_DISTANCE,
                              maptypes::FROM_FIX_TO_MANUAL_TERMINATION,
                              maptypes::HEADING_TO_ALTITUDE_TERMINATION,
                              maptypes::HEADING_TO_MANUAL_TERMINATION}))
  {
    if((leg.turnDirection == "R" || leg.turnDirection == "L") &&
       prevLeg != nullptr && prevLeg->type != maptypes::INITIAL_FIX)
    {
      float lineDist = QLineF(lastLine.p2(), line.p1()).length();
      if(!lastLine.p2().isNull() && lineDist > 2)
      {
        // Lines are not connected which can happen if a CF follows after a FD or similar
        QLineF lineDraw(line.p2(), line.p1());

        // Shorten the next line to get a better curve - use a value less than 1 nm to avoid flickering on 1 nm legs
        float oneNmPixel = scale->getPixelForNm(0.95f);
        if(lineDraw.length() > oneNmPixel * 2.f)
          lineDraw.setLength(lineDraw.length() - oneNmPixel);
        lineDraw.setPoints(lineDraw.p2(), lineDraw.p1());

        // Calculate distance to control points
        float dist = prevLeg->line.getPos2().distanceMeterToRhumb(leg.line.getPos1()) * 3 / 4;

        // Calculate bezier control points by extending the last and next line
        QLineF ctrl1(lastLine.p1(), lastLine.p2());
        ctrl1.setLength(ctrl1.length() + scale->getPixelForMeter(dist));
        QLineF ctrl2(lineDraw.p2(), lineDraw.p1());
        ctrl2.setLength(ctrl2.length() + scale->getPixelForMeter(dist));

        // Draw a bow connecting the two lines
        QPainterPath path;
        path.moveTo(lastLine.p2());
        path.cubicTo(ctrl1.p2(), ctrl2.p2(), lineDraw.p1());
        painter->drawPath(path);

        painter->drawLine(lineDraw);
        lastLine = lineDraw;

        if(drawTextLines != nullptr)
          // Can draw a label along the line with course but not distance
          (*drawTextLines)[index] = {leg.line, false, true};
      }
      else
      {
        // Lines are connected but a turn direction is given
        // Draw a small arc if a turn direction is given

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
        QLineF ctrl1(lastLine.p1(), lastLine.p2()), ctrl2(line.p2(), arc.p2());
        ctrl1.setLength(ctrl1.length() + scale->getPixelForNm(.5f));
        ctrl2.setLength(ctrl2.length() + scale->getPixelForNm(.5f));

        // Draw the arc
        QPainterPath path;
        path.moveTo(arc.p1());
        path.cubicTo(ctrl1.p2(), ctrl2.p2(), arc.p2());
        painter->drawPath(path);

        // Draw the next line
        QLineF nextLine(arc.p2(), line.p2());
        painter->drawLine(nextLine);

        lastLine = nextLine.toLine();

        Pos p1 = sToW(nextLine.p1());
        Pos p2 = sToW(nextLine.p2());

        if(drawTextLines != nullptr)
          // Can draw a label along the line with course but not distance
          (*drawTextLines)[index] = {Line(p1, p2), showDistance, true};
      }
    }
    else
    {
      // No turn direction or both

      if(leg.interceptPos.isValid())
      {
        // Intercept a CF leg
        painter->drawLine(line.p1(), intersectPoint);
        painter->drawLine(intersectPoint, line.p2());
        lastLine = QLineF(intersectPoint, line.p2());

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = {Line(leg.interceptPos, leg.line.getPos2()), showDistance, true};
      }
      else
      {
        if(!lastLine.p2().isNull() && QLineF(lastLine.p2(), line.p1()).length() > 2)
          // Close any gaps in the lines
          painter->drawLine(lastLine.p2(), line.p1());

        painter->drawLine(line);
        lastLine = line;

        if(drawTextLines != nullptr)
          // Can draw a label along the line
          (*drawTextLines)[index] = {leg.line, showDistance, true};
      }
    }
  }
  // ===========================================================
  else if(contains(leg.type, {maptypes::COURSE_TO_RADIAL_TERMINATION,
                              maptypes::HEADING_TO_RADIAL_TERMINATION,
                              maptypes::COURSE_TO_DME_DISTANCE,
                              maptypes::HEADING_TO_DME_DISTANCE_TERMINATION,
                              maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE,
                              maptypes::COURSE_TO_INTERCEPT,
                              maptypes::HEADING_TO_INTERCEPT}))
  {
    painter->drawLine(line);
    lastLine = line;

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = {leg.line, showDistance, true};
  }
  // ===========================================================
  else if(contains(leg.type, {maptypes::HOLD_TO_ALTITUDE,
                              maptypes::HOLD_TO_FIX,
                              maptypes::HOLD_TO_MANUAL_TERMINATION}))
  {
    QString holdText, holdText2;

    float trueCourse = leg.legTrueCourse();

    if(!background)
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

    paintHoldWithText(painter, line.x2(), line.y2(), trueCourse, leg.distance, leg.time, leg.turnDirection == "L",
                      holdText, holdText2,
                      leg.missed ? mapcolors::routeApproachMissedTextColor : mapcolors::routeApproachTextColor,
                      mapcolors::routeTextBackgroundColor);
  }
  // ===========================================================
  else if(leg.type == maptypes::PROCEDURE_TURN)
  {
    QString text;

    float trueCourse = leg.legTrueCourse();
    if(!background)
    {
      text = QString::number(leg.course, 'f', 0) + (leg.trueCourse ? tr("°T") : tr("°M")) + tr("/1min");
      if(trueCourse < 180.f)
        text = text + tr(" ►");
      else
        text = tr("◄ ") + text;
    }

    QPointF pc = wToS(leg.procedureTurnPos, size, &hiddenDummy);

    paintProcedureTurnWithText(painter, pc.x(), pc.y(), trueCourse, leg.distance,
                               leg.turnDirection == "L", &lastLine, text,
                               leg.missed ? mapcolors::routeApproachMissedTextColor : mapcolors::routeApproachTextColor,
                               mapcolors::routeTextBackgroundColor);

    painter->drawLine(line.p1(), pc);

    if(drawTextLines != nullptr)
      // Can draw a label along the line
      (*drawTextLines)[index] = {Line(leg.line.getPos1(), leg.procedureTurnPos), showDistance, true};
  }

#ifdef DEBUG_APPROACH_PAINT
  painter->save();
  painter->setPen(Qt::black);
  painter->drawEllipse(line.p1(), 20, 10);
  painter->drawEllipse(line.p2(), 10, 20);
  painter->drawEllipse(intersectPoint, 30, 30);
  painter->drawText(line.x1() - 40, line.y1() + 40,
                    "Start " + maptypes::approachLegTypeShortStr(leg.type) + " " + QString::number(index));
  painter->drawText(line.x2() - 40, line.y2() + 60,
                    "End" + maptypes::approachLegTypeShortStr(leg.type) + " " + QString::number(index));
  painter->drawText(intersectPoint.x() - 40, intersectPoint.y() + 20, leg.type + " " + QString::number(index));
  painter->restore();
#endif
}

void MapPainterRoute::paintApproachPoints(const PaintContext *context, const maptypes::MapApproachLegs& legs,
                                          int index)
{
  if((!(context->objectTypes & maptypes::APPROACH_TRANSITION) && legs.isTransition(index)) ||
     (!(context->objectTypes & maptypes::APPROACH) && legs.isApproach(index)) ||
     (!(context->objectTypes & maptypes::APPROACH_MISSED) && legs.isMissed(index)))
    return;

  const maptypes::MapApproachLeg& leg = legs.at(index);

  if(leg.disabled)
    return;

  bool lastInTransition = false;
  if(index < legs.size() - 1 &&
     leg.type != maptypes::HEADING_TO_INTERCEPT && leg.type != maptypes::COURSE_TO_INTERCEPT)
    // Do not paint the last point in the transition since it overlaps with the approach
    lastInTransition = legs.isTransition(index) &&
                       legs.isApproach(index + 1) && context->objectTypes & maptypes::APPROACH;

  int x = 0, y = 0;

  QStringList texts;
  // Manual and altitude terminated legs that have a calculated position needing extra text
  if(contains(leg.type, {maptypes::COURSE_TO_ALTITUDE,
                         maptypes::COURSE_TO_DME_DISTANCE,
                         maptypes::COURSE_TO_INTERCEPT,
                         maptypes::COURSE_TO_RADIAL_TERMINATION,
                         maptypes::HOLD_TO_FIX,
                         maptypes::HOLD_TO_MANUAL_TERMINATION,
                         maptypes::HOLD_TO_ALTITUDE,
                         maptypes::FIX_TO_ALTITUDE,
                         maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE,
                         maptypes::HEADING_TO_ALTITUDE_TERMINATION,
                         maptypes::HEADING_TO_DME_DISTANCE_TERMINATION,
                         maptypes::HEADING_TO_INTERCEPT,
                         maptypes::HEADING_TO_RADIAL_TERMINATION,
                         maptypes::TRACK_FROM_FIX_FROM_DISTANCE,
                         maptypes::FROM_FIX_TO_MANUAL_TERMINATION,
                         maptypes::HEADING_TO_MANUAL_TERMINATION}))
  {
    if(lastInTransition)
      return;

    // All legs with a calculated end point
    if(wToS(leg.line.getPos2(), x, y))
    {
      texts.append(leg.displayText);
      texts.append(maptypes::restrictionText(leg.altRestriction));
      paintApproachpoint(context, x, y);
      paintText(context, mapcolors::routeApproachPointColor, x, y, texts);
      texts.clear();
    }
  }
  else if(leg.type == maptypes::COURSE_TO_FIX)
  {
    if(wToS(leg.interceptPos, x, y))
    {
      // Draw intercept comment - no altitude restriction there
      texts.append(leg.displayText);
      paintApproachpoint(context, x, y);
      paintText(context, mapcolors::routeApproachPointColor, x, y, texts);
      texts.clear();
    }
  }
  else
  {
    if(lastInTransition)
      return;

    texts.append(leg.displayText);
    texts.append(maptypes::restrictionText(leg.altRestriction));
  }

  const maptypes::MapSearchResult& navaids = leg.navaids;

  if(!navaids.waypoints.isEmpty() && wToS(navaids.waypoints.first().position, x, y))
  {
    paintWaypoint(context, QColor(), x, y);
    paintWaypointText(context, x, y, navaids.waypoints.first(), &texts);
  }
  else if(!navaids.vors.isEmpty() && wToS(navaids.vors.first().position, x, y))
  {
    paintVor(context, x, y, navaids.vors.first());
    paintVorText(context, x, y, navaids.vors.first(), &texts);
  }
  else if(!navaids.ndbs.isEmpty() && wToS(navaids.ndbs.first().position, x, y))
  {
    paintNdb(context, x, y);
    paintNdbText(context, x, y, navaids.ndbs.first(), &texts);
  }
  else if(!navaids.ils.isEmpty() && wToS(navaids.ils.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    paintApproachpoint(context, x, y);
    paintText(context, mapcolors::routeApproachPointColor, x, y, texts);
  }
  else if(!navaids.runwayEnds.isEmpty() && wToS(navaids.runwayEnds.first().position, x, y))
  {
    texts.append(leg.fixIdent);
    paintApproachpoint(context, x, y);
    paintText(context, mapcolors::routeApproachPointColor, x, y, texts);
  }
}

void MapPainterRoute::paintAirport(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj)
{
  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
  symbolPainter->drawAirportSymbol(context->painter, obj, x, y, size, false, false);
}

void MapPainterRoute::paintAirportText(const PaintContext *context, int x, int y,
                                       const maptypes::MapAirport& obj)
{
  int size = context->sz(context->symbolSizeAirport, context->mapLayerEffective->getAirportSymbolSize());
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT;

  // Use more more detailed text for flight plan
  if(context->mapLayer->isAirportRouteInfo())
    flags |= textflags::NAME | textflags::INFO;

  symbolPainter->drawAirportText(context->painter, obj, x, y, context->dispOpts, flags, size,
                                 context->mapLayerEffective->isAirportDiagram());
}

void MapPainterRoute::paintVor(const PaintContext *context, int x, int y, const maptypes::MapVor& obj)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
  symbolPainter->drawVorSymbol(context->painter, obj, x, y,
                               size, false, false,
                               context->mapLayerEffective->isVorLarge() ? size * 5 : 0);
}

void MapPainterRoute::paintVorText(const PaintContext *context, int x, int y, const maptypes::MapVor& obj,
                                   const QStringList *addtionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  // Use more more detailed VOR text for flight plan
  if(context->mapLayer->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawVorText(context->painter, obj, x, y, flags, size, true, addtionalText);
}

void MapPainterRoute::paintNdb(const PaintContext *context, int x, int y)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
  symbolPainter->drawNdbSymbol(context->painter, x, y, size, true, false);
}

void MapPainterRoute::paintNdbText(const PaintContext *context, int x, int y, const maptypes::MapNdb& obj,
                                   const QStringList *addtionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  // Use more more detailed NDB text for flight plan
  if(context->mapLayer->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawNdbText(context->painter, obj, x, y, flags, size, true, addtionalText);
}

void MapPainterRoute::paintWaypoint(const PaintContext *context, const QColor& col, int x, int y)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawWaypointSymbol(context->painter, col, x, y, size, true, false);
}

void MapPainterRoute::paintWaypointText(const PaintContext *context, int x, int y,
                                        const maptypes::MapWaypoint& obj,
                                        const QStringList *addtionalText)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(context->mapLayer->isWaypointRouteName())
    flags |= textflags::IDENT;

  symbolPainter->drawWaypointText(context->painter, obj, x, y, flags, size, true, addtionalText);
}

/* paint intermediate approach point */
void MapPainterRoute::paintApproachpoint(const PaintContext *context, int x, int y)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawApproachSymbol(context->painter, x, y, size, true, false);
}

/* Paint user defined waypoint */
void MapPainterRoute::paintUserpoint(const PaintContext *context, int x, int y)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
  symbolPainter->drawUserpointSymbol(context->painter, x, y, size, true, false);
}

/* Draw text with light yellow background for flight plan */
void MapPainterRoute::paintText(const PaintContext *context, const QColor& color, int x, int y,
                                const QStringList& texts)
{
  int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());

  if(!texts.isEmpty() && context->mapLayer->isWaypointRouteName())
    symbolPainter->textBox(context->painter, texts, color,
                           x + size / 2 + 2,
                           y, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
}

void MapPainterRoute::drawSymbols(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                                  const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = pt.x();
      int y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          // name and region not found in database
          paintWaypoint(context, mapcolors::routeInvalidPointColor, x, y);
          break;
        case maptypes::USER:
          paintUserpoint(context, x, y);
          break;
        case maptypes::AIRPORT:
          paintAirport(context, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVor(context, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdb(context, x, y);
          break;
        case maptypes::WAYPOINT:
          paintWaypoint(context, QColor(), x, y);
          break;
      }
    }
    i++;
  }
}

void MapPainterRoute::drawSymbolText(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                                     const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints)
{
  int i = 0;
  for(const QPointF& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = pt.x();
      int y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          paintText(context, mapcolors::routeInvalidPointColor, x, y, {obj.getIdent()});
          break;
        case maptypes::USER:
          paintText(context, mapcolors::routeUserPointColor, x, y, {obj.getIdent()});
          break;
        case maptypes::AIRPORT:
          paintAirportText(context, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVorText(context, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdbText(context, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypointText(context, x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }
}
