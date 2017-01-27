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

#include <QBitArray>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using maptypes::MapApproachLeg;
using maptypes::MapApproachLegList;

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
  if(!context->objectTypes.testFlag(maptypes::ROUTE))
    return;

  setRenderHints(context->painter);

  atools::util::PainterContextSaver saver(context->painter);

  paintRoute(context);
  paintApproachPreview(context, mapWidget->getTransitionHighlight(), mapWidget->getApproachHighlight());
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

  // Collect coordinates for text placement and lines first
  QList<QPoint> textCoords;
  QList<float> textBearing;
  QStringList texts;
  QList<int> textLineLengths;

  // Collect start and end points of legs and visibility
  QList<QPoint> startPoints;
  QBitArray visibleStartPoints(routeMapObjects.size(), false);
  GeoDataLineString linestring;
  linestring.setTessellate(true);

  for(int i = 0; i < routeMapObjects.size(); i++)
  {
    const RouteMapObject& obj = routeMapObjects.at(i);
    linestring.append(GeoDataCoordinates(obj.getPosition().getLonX(), obj.getPosition().getLatY(), 0, DEG));

    int x, y;
    visibleStartPoints.setBit(i, wToS(obj.getPosition(), x, y));

    if(i > 0 && !context->drawFast)
    {
      int lineLength = simpleDistance(x, y, startPoints.at(i - 1).x(), startPoints.at(i - 1).y());
      if(lineLength > MIN_LENGTH_FOR_TEXT)
      {
        // Build text
        QString text(Unit::distNm(obj.getDistanceTo(), true /*addUnit*/, 10, true /*narrow*/) + tr(" / ") +
                     QString::number(obj.getCourseToRhumb(), 'f', 0) +
                     (foundMagvarObject ? tr("°M") : tr("°T")));

        int textw = context->painter->fontMetrics().width(text);
        if(textw > lineLength)
          // Limit text length to line for elide
          textw = lineLength;

        int xt, yt;
        float brg;
        Pos p1(obj.getPosition()), p2(routeMapObjects.at(i - 1).getPosition());

        if(findTextPos(p1, p2, context->painter,
                       textw, context->painter->fontMetrics().height(), xt, yt, &brg))
        {
          textCoords.append(QPoint(xt, yt));
          textBearing.append(brg);
          texts.append(text);
          textLineLengths.append(lineLength);
        }
      }
      else
      {
        // No text - append all dummy values
        textCoords.append(QPoint());
        textBearing.append(0.f);
        texts.append(QString());
        textLineLengths.append(0);
      }
    }
    startPoints.append(QPoint(x, y));
  }

  float outerlinewidth = context->sz(context->thicknessFlightplan, 6);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 4);

  // Draw lines separately to avoid omission in mercator near anti meridian
  // Draw outer line
  context->painter->setPen(QPen(mapcolors::routeOutlineColor, outerlinewidth, Qt::SolidLine,
                                Qt::RoundCap, Qt::RoundJoin));
  drawLineString(context, linestring);

  const Pos& pos = mapWidget->getUserAircraft().getPosition();

  int leg = -1;
  if(pos.isValid())
  {
    float cross;
    leg = routeMapObjects.getNearestLegIndex(pos, cross);
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

    if(leg != -1 && i == leg)
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

  context->szFont(context->textSizeFlightplan);

  if(!context->drawFast)
  {
    // Draw text with direction arrow along lines
    context->painter->setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::FlatCap));
    int i = 0;
    for(const QPoint& textCoord : textCoords)
    {
      QString text = texts.at(i);
      if(!text.isEmpty())
      {
        // Cut text right or left depending on direction
        Qt::TextElideMode elide = Qt::ElideRight;
        float rotate, brg = textBearing.at(i);
        if(brg < 180.)
        {
          text += tr(" ►");
          elide = Qt::ElideLeft;
          rotate = brg - 90.f;
        }
        else
        {
          text = tr("◄ ") + text;
          elide = Qt::ElideRight;
          rotate = brg + 90.f;
        }

        // Draw text
        QFontMetrics metrics = context->painter->fontMetrics();

        // Both points are visible - cut text for full line length
        if(visibleStartPoints.testBit(i) && visibleStartPoints.testBit(i + 1))
          text = metrics.elidedText(text, elide, textLineLengths.at(i));

        context->painter->translate(textCoord.x(), textCoord.y());
        context->painter->rotate(rotate);
        context->painter->drawText(-metrics.width(text) / 2, -metrics.descent() - outerlinewidth / 2, text);
        context->painter->resetTransform();
      }
      i++;
    }
  }

  // Draw airport and navaid symbols
  drawSymbols(context, routeMapObjects, visibleStartPoints, startPoints);

  // Draw symbol text
  drawSymbolText(context, routeMapObjects, visibleStartPoints, startPoints);

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

      symbolPainter->textBox(context->painter, tod, QPen(Qt::black),
                             pt.x() + radius, pt.y() + radius,
                             textatt::ROUTE_BG_COLOR | textatt::BOLD, 255);
    }
  }
}

/* Draw approaches and transitions selected in the tree view */
void MapPainterRoute::paintApproachPreview(const PaintContext *context,
                                           const maptypes::MapApproachLegList& transition,
                                           const maptypes::MapApproachLegList& approach)
{
  if(((transition.legs.isEmpty() || !transition.bounding.overlaps(context->viewportRect))) &&
     (approach.legs.isEmpty() || !approach.bounding.overlaps(context->viewportRect)))
    return;

  // Draw white background ========================================
  float outerlinewidth = context->sz(context->thicknessFlightplan, 9);
  float innerlinewidth = context->sz(context->thicknessFlightplan, 3);

  context->painter->setPen(QPen(mapcolors::routeApproachPreviewOutlineColor, outerlinewidth,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if(context->objectTypes & context->objectTypes & maptypes::APPROACH_TRANSITION)
  {
    for(int i = 0; i < transition.legs.size(); ++i)
      paintApproachSegment(context, transition, &approach, i);
  }

  for(int i = 0; i < approach.legs.size(); ++i)
  {
    if((approach.legs.at(i).missed && context->objectTypes & maptypes::APPROACH_MISSED) ||
       (!approach.legs.at(i).missed && context->objectTypes & maptypes::APPROACH))
      paintApproachSegment(context, approach, &transition, i);
  }

  context->painter->setPen(QPen(mapcolors::routeApproachPreviewColor, innerlinewidth,
                                Qt::DotLine, Qt::RoundCap, Qt::RoundJoin));
  // Missed Approach below others ===========================================================
  if(context->objectTypes & context->objectTypes & maptypes::APPROACH_MISSED)
  {
    for(int i = 0; i < approach.legs.size(); ++i)
    {
      if(approach.legs.at(i).missed)
        paintApproachSegment(context, approach, &transition, i);
    }
  }

  context->painter->setPen(QPen(mapcolors::routeApproachPreviewColor, innerlinewidth,
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  // Transition  ===========================================================
  if(context->objectTypes & context->objectTypes & maptypes::APPROACH_TRANSITION)
  {
    for(int i = 0; i < transition.legs.size(); ++i)
      paintApproachSegment(context, transition, &approach, i);
  }

  // Approach ===========================================================
  if(context->objectTypes & context->objectTypes & maptypes::APPROACH)
  {
    for(int i = 0; i < approach.legs.size(); ++i)
    {
      if(!approach.legs.at(i).missed)
        paintApproachSegment(context, approach, &transition, i);
      else
        break;
    }
  }

  // Texts ====================================================
  // Do not paint the last point in the transition since it overlaps with the approach
  if(context->objectTypes & context->objectTypes & maptypes::APPROACH_MISSED)
  {
    for(const MapApproachLeg& leg : approach.legs)
    {
      if(leg.missed)
        paintApproachPoint(context, leg);
    }
  }

  if(context->objectTypes & context->objectTypes & maptypes::APPROACH_TRANSITION)
  {
    for(int i = 0; i < transition.legs.size() - 1; ++i)
      paintApproachPoint(context, transition.legs.at(i));
  }

  if(context->objectTypes & context->objectTypes & maptypes::APPROACH)
  {
    for(const MapApproachLeg& leg : approach.legs)
    {
      if(!leg.missed)
        paintApproachPoint(context, leg);
      else
        break;
    }
  }
}

void setAngle(QLineF& line, float angle)
{
  line.setAngle(-(angle - 90.));
}

float getAngle(const QLineF& line)
{
  return atools::geo::normalizeCourse(line.angle() + 90.);
}

void MapPainterRoute::paintApproachSegment(const PaintContext *context,
                                           const maptypes::MapApproachLegList& legs,
                                           const maptypes::MapApproachLegList *nextLegs, int index)
{
  const maptypes::MapApproachLeg& leg = legs.legs.at(index);
  const maptypes::MapApproachLeg *prevLeg = nullptr;
  if(index > 0)
    prevLeg = &legs.legs.at(index - 1);

  // const maptypes::MapApproachLeg *nextLeg = nullptr;
  // float nextCourse = 999.f;
  // if(index < legs.legs.size() - 1)
  // {
  // nextLeg = &legs.legs.at(index + 1);
  // nextCourse = nextLeg->course;
  // }
  // else
  // {
  // if(nextLegs != nullptr && !nextLegs->legs.isEmpty())
  // {
  // if(nextLegs->legs.first().type == "IF")
  // {
  // nextLeg = &nextLegs->legs.first();
  // if(nextLegs->legs.size() > 1)
  // nextCourse = nextLegs->legs.at(1).course;
  // }
  // else
  // {
  // nextLeg = &nextLegs->legs.first();
  // nextCourse = nextLegs->legs.first().course;
  // }
  // }
  // }

  int fixX = 0, fixY = 0, prevX = 0, prevY = 0, recX = 0, recY = 0;
  bool valid = leg.displayPos.isValid(),
       prevValid = prevLeg != nullptr ? prevLeg->displayPos.isValid() : false,
       recValid = leg.recFixPos.isValid();

  QSize size = scale->getScreeenSizeForRect(legs.bounding);
  // Use visible dummy here since we need to call the method that also returns coordinates outside the screen
  bool hiddenDummy;
  if(valid)
    wToS(leg.displayPos, fixX, fixY, size, &hiddenDummy);
  if(prevValid)
    wToS(prevLeg->displayPos, prevX, prevY, size, &hiddenDummy);
  if(recValid)
    wToS(leg.recFixPos, recX, recY, size, &hiddenDummy);

  QPainter *painter = context->painter;

  // if(leg.type == "IF")   // Initial fix - Nothing to do here
  if(leg.type == "AF" || // Arc to fix
     leg.type == "RF") // Constant radius arc
  {
    if(valid && prevValid)
      paintArc(context->painter, prevX, prevY, fixX, fixY, recX, recY, leg.turnDirection == "L");
  }
  else if(leg.type == "CA" ||  // Course to altitude - point prepared by ApproachQuery
          leg.type == "CF" || // Course to fix
          leg.type == "DF" || // Direct to fix
          leg.type == "FA" || // Fix to altitude - point prepared by ApproachQuery
          leg.type == "TF" || // Track to fix
          leg.type == "FC" || // Track from fix from distance
          leg.type == "FM" || // From fix to manual termination
          leg.type == "VA" || // Heading to altitude termination
          leg.type == "VM") // Heading to manual termination
  {
    if(valid && prevValid)
      painter->drawLine(fixX, fixY, prevX, prevY);
  }
  else if(leg.type == "CD" ||  // Course to DME distance
          leg.type == "VD") // Heading to DME distance termination
  {
    // TODO
    if(valid && prevValid)
      painter->drawLine(fixX, fixY, prevX, prevY);
  }
  else if(leg.type == "CI" ||  // Course to intercept
          leg.type == "VI") // Heading to intercept
  {
  }
  else if(leg.type == "CR" ||  // Course to radial termination
          leg.type == "VR") // Heading to radial termination
  {
  }
  else if(leg.type == "FD")   // Track from fix to DME distance
  {
    // TODO - not correct
    if(valid && prevValid)
      painter->drawLine(fixX, fixY, prevX, prevY);
  }
  else if(leg.type == "HA" ||  // Hold to altitude
          leg.type == "HF" || // Hold to fix
          leg.type == "HM") // Hold to manual termination
  {
    // Length of the straight segments - scale to about total length given in the leg
    float pixel = scale->getPixelForFeet(
      atools::roundToInt(atools::geo::nmToFeet(std::max(leg.dist, 3.5f) / 3.3f)));

    QPainterPath path;
    if(leg.turnDirection == "R")
    {
      path.arcTo(QRectF(0, -pixel * 0.25f, pixel * 0.5f, pixel * 0.5f), 180, -180);
      path.arcTo(QRectF(0, 0 + pixel * 0.75f, pixel * 0.5f, pixel * 0.5f), 0, -180);
    }
    else
    {
      path.arcTo(QRectF(-pixel * 0.5f, -pixel * 0.25f, pixel * 0.5f, pixel * 0.5f), 0, 180);
      path.arcTo(QRectF(-pixel * 0.5f, 0 + pixel * 0.75f, pixel * 0.5f, pixel * 0.5f), 180, 180);
    }
    path.closeSubpath();

    painter->translate(fixX, fixY);
    painter->rotate(leg.course + leg.magvar);

    painter->drawPath(path);
    painter->resetTransform();
  }
  else if(leg.type == "PI")   // Procedure turn
  {
    float pixel = scale->getPixelForFeet(atools::roundToInt(atools::geo::nmToFeet(3.5f)));

    float course;
    // if(nextCourse != 999.f)
    // course = atools::geo::opposedCourseDeg(nextCourse + leg.magvar);
    // else

    if(leg.turnDirection == "L")
      course = leg.course + leg.magvar - 45;
    else
      course = leg.course + leg.magvar + 45;

    QLineF ext = QLineF(fixX, fixY, fixX + 400, fixY);
    setAngle(ext, course);
    ext.setLength(scale->getPixelForNm(static_cast<int>(leg.dist), getAngle(ext)));

    QLineF turn = QLineF(ext.x2(), ext.y2(), ext.x2() + pixel, ext.y2());
    float turnCourse;
    if(leg.turnDirection == "L")
      turnCourse = course + 45.f;
    else
      turnCourse = course - 45.f;
    setAngle(turn, turnCourse);

    QLineF arrow(turn.p2(), turn.p1());
    arrow.setLength(scale->getPixelForMeter(1000, getAngle(arrow)));

    QPolygonF poly;
    poly << arrow.p2() << arrow.p1();

    if(leg.turnDirection == "L")
      setAngle(arrow, opposedCourseDeg(turnCourse) - 15);
    else
      setAngle(arrow, opposedCourseDeg(turnCourse) + 15);

    context->painter->save();
    context->painter->setBrush(mapcolors::routeApproachPreviewColor);
    painter->setBrush(mapcolors::routeApproachPreviewColor);
    QPen pen = painter->pen();
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(pen);
    poly << arrow.p2();
    painter->drawPolygon(poly);
    context->painter->restore();

    // QLineF arc = QLineF(turn.x2(), turn.y2(), turn.x2() + pixel / 2., turn.y2());
    // if(leg.turnDirection == "L")
    // setAngle(arc, course - 45.f);
    // else if(leg.turnDirection == "R")
    // setAngle(arc, course + 45.f);

    // QLineF ret(turn);
    // ret.setP1(arc.p2());
    // ret.setP2(QPointF(turn.x1() - (arc.x1() - arc.x2()), turn.y1() - (arc.y1() - arc.y2())));

    // QPointF intersect;
    // bool intersects = ext.intersect(ret, &intersect) != QLineF::NoIntersection;
    // if(intersects)
    // ret.setP2(intersect);

    painter->drawLine(ext);
    painter->drawLine(turn);
    // painter->drawLine(arrow);

    // paintArc(context->painter, arc.p1(), arc.p2(), arc.pointAt(0.5), leg.turnDirection == "L");
    // painter->drawLine(ret.p1(), ret.p2());
    // if(intersects)
    // painter->drawLine(ret.p2(), turn.p1());
    // else
    // painter->drawLine(ret.p1(), turn.p1());
  }

  // if(valid && prevValid)
  // {
  // painter->drawText((fixX + prevX) / 2, (fixY + prevY) / 2, QString::number(index));
  // }
}

void MapPainterRoute::paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& p0,
                               bool left)
{
  paintArc(painter, p1.x(), p1.y(), p2.x(), p2.y(), p0.x(), p0.y(), left);
}

void MapPainterRoute::paintArc(QPainter *painter, const QPoint& p1, const QPoint& p2, const QPoint& p0,
                               bool left)
{
  paintArc(painter, p1.x(), p1.y(), p2.x(), p2.y(), p0.x(), p0.y(), left);
}

void MapPainterRoute::paintArc(QPainter *painter, float x1, float y1, float x2, float y2, float x0, float y0,
                               bool left)
{
  // center of the circle is (x0, y0) and that the arc contains your two points (x1, y1) and (x2, y2).
  // Then the radius is: r=sqrt((x1-x0)(x1-x0) + (y1-y0)(y1-y0)).
  float r = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  float x = x0 - r;
  float y = y0 - r;
  float width = 2.f * r;
  float height = 2.f * r;
  float startAngle = atools::geo::normalizeCourse(atools::geo::toDegree(std::atan2(y1 - y0, x1 - x0)));
  float endAngle = atools::geo::normalizeCourse(atools::geo::toDegree(std::atan2(y2 - y0, x2 - x0)));

  float span = 0.f;
  if(left)
  {
    if(startAngle > endAngle)
      span = startAngle - endAngle;
    else
      span = 360.f - endAngle + startAngle;

    painter->drawArc(x, y, width, height, -startAngle * 16.f, span * 16.f);
  }
  else
  {
    // negative values mean clockwise
    if(endAngle > startAngle)
      span = endAngle - startAngle;
    else
      span = 360.f - startAngle + endAngle;

    painter->drawArc(x, y, width, height, -startAngle * 16.f, -span * 16.f);
  }
}

void MapPainterRoute::paintApproachPoint(const PaintContext *context, const MapApproachLeg& leg)
{
  int x = 0, y = 0;

  QStringList texts;
  // Manual and altitude terminated legs
  if((leg.type == "CA" ||
      leg.type == "FA" ||
      leg.type == "FD" ||
      leg.type == "VA" ||
      leg.type == "FC" ||
      leg.type == "FM" ||
      leg.type == "VM") && wToS(leg.displayPos, x, y))
  {
    texts.append(leg.displayText);
    texts.append(maptypes::restrictionText(leg.altRestriction));
    paintUserpoint(context, x, y);
    paintText(context, mapcolors::routeUserPointColor, x, y, texts);
    texts.clear();
  }
  else
  {
    texts.append(maptypes::restrictionText(leg.altRestriction));
  }

  if(leg.waypoint.position.isValid() && wToS(leg.waypoint.position, x, y))
  {
    paintWaypoint(context, QColor(), x, y);
    paintWaypointText(context, x, y, leg.waypoint, &texts);
  }
  else if(leg.ndb.position.isValid() && wToS(leg.ndb.position, x, y))
  {
    paintNdb(context, x, y);
    paintNdbText(context, x, y, leg.ndb, &texts);
  }
  else if(leg.vor.position.isValid() && wToS(leg.vor.position, x, y))
  {
    paintVor(context, x, y, leg.vor);
    paintVorText(context, x, y, leg.vor, &texts);
  }
  else if(leg.ils.position.isValid() && wToS(leg.ils.position, x, y))
  {
    texts.append(leg.fixIdent);
    paintUserpoint(context, x, y);
    paintText(context, mapcolors::routeUserPointColor, x, y, texts);
  }
  else if(leg.runwayEnd.position.isValid() && wToS(leg.runwayEnd.position, x, y))
  {
    texts.append(leg.fixIdent);
    paintUserpoint(context, x, y);
    paintText(context, mapcolors::routeUserPointColor, x, y, texts);
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
                                  const QBitArray& visibleStartPoints, const QList<QPoint>& startPoints)
{
  int i = 0;
  for(const QPoint& pt : startPoints)
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
                                     const QBitArray& visibleStartPoints, const QList<QPoint>& startPoints)
{
  int i = 0;
  for(const QPoint& pt : startPoints)
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
