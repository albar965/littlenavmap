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

#include "mapgui/mappainter.h"

#include "mapgui/mapscale.h"
#include "navapp.h"
#include "common/symbolpainter.h"
#include "geo/calculations.h"
#include "mapgui/mapwidget.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

void PaintContext::szFont(float scale) const
{
  QFont font = painter->font();
  if(font.pixelSize() == -1)
  {
    double size = szF(scale, defaultFont.pointSizeF());
    if(atools::almostNotEqual(size, font.pointSizeF()))
    {
      font.setPointSizeF(size);
      painter->setFont(font);
    }
  }
  else
  {
    int size = static_cast<int>(std::round(sz(scale, defaultFont.pixelSize())));
    if(size != defaultFont.pixelSize())
    {
      font.setPixelSize(size);
      painter->setFont(font);
    }
  }
}

// =================================================
MapPainter::MapPainter(MapWidget *parentMapWidget, MapScale *mapScale)
  : CoordinateConverter(parentMapWidget->viewport()), mapWidget(parentMapWidget), scale(mapScale)
{
  mapQuery = NavApp::getMapQuery();
  airspaceQuery = NavApp::getAirspaceQuery();
  airspaceQueryOnline = NavApp::getAirspaceQueryOnline();
  airportQuery = NavApp::getAirportQuerySim();
  symbolPainter = new SymbolPainter();
}

MapPainter::~MapPainter()
{
  delete symbolPainter;
}

void MapPainter::paintCircle(GeoPainter *painter, const Pos& centerPos, float radiusNm, bool fast,
                             int& xtext, int& ytext)
{
  QRect vpRect(painter->viewport());

  // Calculate the number of points to use depending in screen resolution
  int pixel = scale->getPixelIntForMeter(nmToMeter(radiusNm));
  int numPoints = std::min(std::max(pixel / (fast ? 20 : 2), CIRCLE_MIN_POINTS), CIRCLE_MAX_POINTS);

  float radiusMeter = nmToMeter(radiusNm);

  int step = 360 / numPoints;
  int x1, y1, x2 = -1, y2 = -1;
  xtext = -1;
  ytext = -1;

  QVector<int> xtexts;
  QVector<int> ytexts;

  // Use north endpoint of radius as start position
  Pos startPoint = centerPos.endpoint(radiusMeter, 0).normalize();
  Pos p1 = startPoint;
  bool hidden1 = true, hidden2 = true;
  bool visible1 = wToS(p1, x1, y1, DEFAULT_WTOS_SIZE, &hidden1);

  bool ringVisible = false, lastVisible = false;
  GeoDataLineString ellipse;
  ellipse.setTessellate(true);
  // Draw ring segments and collect potential text positions
  for(int i = 0; i <= 360; i += step)
  {
    // Line segment from p1 to p2
    Pos p2 = centerPos.endpoint(radiusMeter, i).normalize();

    bool visible2 = wToS(p2, x2, y2, DEFAULT_WTOS_SIZE, &hidden2);

    QRect rect(QPoint(x1, y1), QPoint(x2, y2));
    rect = rect.normalized();
    // Avoid points or flat rectangles (lines)
    rect.adjust(-1, -1, 1, 1);

    // Current line is visible (most likely)
    bool nowVisible = rect.intersects(vpRect);

    if(lastVisible || nowVisible)
      // Last line or this one are visible add coords
      ellipse.append(GeoDataCoordinates(p1.getLonX(), p1.getLatY(), 0, DEG));

    if(lastVisible && !nowVisible)
    {
      // Not visible anymore draw previous line segment
      painter->drawPolyline(ellipse);
      ellipse.clear();
    }

    if(lastVisible || nowVisible)
    {
      // At least one segment of the ring is visible
      ringVisible = true;

      if((visible1 || visible2) && !hidden1 && !hidden2)
      {
        if(visible1 && visible2)
        {
          // Remember visible positions for the text (center of the line segment)
          xtexts.append((x1 + x2) / 2);
          ytexts.append((y1 + y2) / 2);
        }
      }
    }
    x1 = x2;
    y1 = y2;
    visible1 = visible2;
    hidden1 = hidden2;
    p1 = p2;
    lastVisible = nowVisible;
  }

  if(ringVisible)
  {
    if(!ellipse.isEmpty())
    {
      // Last one always needs closing the circle
      ellipse.append(GeoDataCoordinates(startPoint.getLonX(), startPoint.getLatY(), 0, DEG));
      painter->drawPolyline(ellipse);
    }

    if(!xtexts.isEmpty() && !ytexts.isEmpty())
    {
      // Take the position at one third of the visible text points to avoid half hidden texts
      xtext = xtexts.at(xtexts.size() / 3);
      ytext = ytexts.at(ytexts.size() / 3);
    }
    else
    {
      xtext = -1;
      ytext = -1;
    }
  }
}

void MapPainter::drawLineString(const PaintContext *context, const Marble::GeoDataLineString& linestring)
{
  GeoDataLineString ls;
  ls.setTessellate(true);
  for(int i = 1; i < linestring.size(); i++)
  {
    ls.clear();
    ls << linestring.at(i - 1) << linestring.at(i);
    context->painter->drawPolyline(ls);
  }
}

void MapPainter::drawLineStraight(const PaintContext *context, const atools::geo::Line& line)
{
  double x1, y1, x2, y2;
  bool visible1 = wToS(line.getPos1(), x1, y1);

  bool visible2 = wToS(line.getPos2(), x2, y2);

  if(visible1 || visible2)
    context->painter->drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

void MapPainter::drawLineString(const PaintContext *context, const atools::geo::LineString& linestring)
{
  GeoDataLineString ls;
  ls.setTessellate(true);
  for(int i = 1; i < linestring.size(); i++)
  {
    ls.clear();
    ls << GeoDataCoordinates(linestring.at(i - 1).getLonX(), linestring.at(i - 1).getLatY(), 0, DEG)
       << GeoDataCoordinates(linestring.at(i).getLonX(), linestring.at(i).getLatY(), 0, DEG);
    context->painter->drawPolyline(ls);
  }
}

void MapPainter::drawLine(const PaintContext *context, const atools::geo::Line& line)
{
  if(line.isValid())
  {
    GeoDataLineString ls;
    ls.setTessellate(true);
    ls << GeoDataCoordinates(line.getPos1().getLonX(), line.getPos1().getLatY(), 0, DEG)
       << GeoDataCoordinates(line.getPos2().getLonX(), line.getPos2().getLatY(), 0, DEG);
    context->painter->drawPolyline(ls);
  }
}

void MapPainter::paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& center, bool left)
{
  QRectF arcRect;
  float startAngle, spanAngle;
  atools::geo::arcFromPoints(QLineF(p1, p2), center, left, &arcRect, &startAngle, &spanAngle);

  painter->drawArc(arcRect, atools::roundToInt(-startAngle * 16.), atools::roundToInt(spanAngle * 16.));
}

void MapPainter::paintHoldWithText(QPainter *painter, float x, float y, float direction,
                                   float lengthNm, float minutes, bool left,
                                   const QString& text, const QString& text2,
                                   const QColor& textColor, const QColor& textColorBackground)
{
  // Scale to total length given in the leg
  // length = 2 * p + 2 * PI * p / 2
  // p = length / (2 + PI)
  // Straight segments are segmentLength long and circle diameter is pixel/2
  // Minimum 3.5

  float segmentLength;
  if(minutes > 0.f)
    // 3.5 nm per minute
    segmentLength = minutes * 3.5f;
  else if(lengthNm > 0.f)
    segmentLength = lengthNm;
  else
    segmentLength = 3.5f;

  float pixel = scale->getPixelForNm(segmentLength);

  QRectF arc1, arc2;
  float angle1, span1, angle2, span2;
  QPainterPath path;
  if(!left)
  {
    // Turn right in the hold
    arc1 = QRectF(0, -pixel * 0.25f, pixel * 0.5f, pixel * 0.5f);
    angle1 = 180.f;
    span1 = -180.f;
    arc2 = QRectF(0, 0 + pixel * 0.75f, pixel * 0.5f, pixel * 0.5f);
    angle2 = 0.f;
    span2 = -180.f;
  }
  else
  {
    // Turn left in the hold
    arc1 = QRectF(-pixel * 0.5f, -pixel * 0.25f, pixel * 0.5f, pixel * 0.5f);
    angle1 = 0.f;
    span1 = 180.f;
    arc2 = QRectF(-pixel * 0.5f, 0.f + pixel * 0.75f, pixel * 0.5f, pixel * 0.5f);
    angle2 = 180.f;
    span2 = 180.f;
  }

  path.arcTo(arc1, angle1, span1);
  path.arcTo(arc2, angle2, span2);
  path.closeSubpath();

  // translate to orgin of hold (navaid or waypoint) and rotate
  painter->translate(x, y);
  painter->rotate(direction);

  // Draw hold
  painter->drawPath(path);

  if(!text.isEmpty() || !text2.isEmpty())
  {
    float lineWidth = painter->pen().widthF();
    // Move to first text position
    painter->translate(0, pixel / 2);
    painter->rotate(direction < 180.f ? 270 : 90);

    painter->save();
    painter->setPen(textColor);
    painter->setBackground(textColorBackground);

    QFontMetrics metrics = painter->fontMetrics();
    if(!text.isEmpty())
    {
      // text pointing to origin
      QString str = metrics.elidedText(text, Qt::ElideRight, pixel);
      int w1 = metrics.width(str);
      painter->drawText(-w1 / 2, -lineWidth - 3, str);
    }

    if(!text2.isEmpty())
    {
      // text on other side to origin
      QString str = metrics.elidedText(text2, Qt::ElideRight, pixel);
      int w2 = metrics.width(str);

      if(direction < 180.f)
        painter->translate(0, left ? -pixel / 2 : pixel / 2);
      else
        painter->translate(0, left ? pixel / 2 : -pixel / 2);
      painter->drawText(-w2 / 2, -lineWidth - 3, str);
    }
    painter->restore();
  }

  painter->resetTransform();
}

void MapPainter::paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm,
                                            bool left, QLineF *extensionLine, const QString& text,
                                            const QColor& textColor, const QColor& textColorBackground)
{
  // One minute = 3.5 nm
  float pixel = scale->getPixelForFeet(atools::roundToInt(atools::geo::nmToFeet(3.f)));

  float course;
  if(left)
    // Turn right and then turn 180 deg left
    course = turnHeading - 45.f;
  else
    // Turn left and then turn 180 deg right
    course = turnHeading + 45.f;

  QLineF extension = QLineF(x, y, x + 400.f, y);
  extension.setAngle(angleToQt(course));
  extension.setLength(scale->getPixelForNm(distanceNm, angleFromQt(extension.angle())));

  if(extensionLine != nullptr)
    // Return course
    *extensionLine = QLineF(extension.p2(), extension.p1());

  // Turn segment
  QLineF turnSegment = QLineF(x, y, x + pixel, y);
  float turnCourse;
  if(left)
    turnCourse = course + 45.f;
  else
    turnCourse = course - 45.f;
  turnSegment.setAngle(angleToQt(turnCourse));

  if(!text.isEmpty())
  {
    float lineWidth = painter->pen().widthF();

    painter->save();
    painter->setPen(textColor);
    painter->setBackground(textColorBackground);
    QFontMetrics metrics = painter->fontMetrics();
    QString str = metrics.elidedText(text, Qt::ElideRight, turnSegment.length());
    int w1 = metrics.width(str);

    painter->translate((turnSegment.x1() + turnSegment.x2()) / 2, (turnSegment.y1() + turnSegment.y2()) / 2);
    painter->rotate(turnCourse < 180.f ? turnCourse - 90.f : turnCourse + 90.f);
    painter->drawText(-w1 / 2, -lineWidth - 3, str);
    painter->resetTransform();
    painter->restore();
  }

  // 180 deg turn arc
  QLineF arc = QLineF(turnSegment.x2(), turnSegment.y2(), turnSegment.x2() + pixel / 2., turnSegment.y2());
  if(left)
    arc.setAngle(angleToQt(course - 45.f));
  else
    arc.setAngle(angleToQt(course + 45.f));

  // Return from turn arc
  QLineF returnSegment(turnSegment);
  returnSegment.setP1(arc.p2());
  returnSegment.setP2(QPointF(turnSegment.x1() - (arc.x1() - arc.x2()), turnSegment.y1() - (arc.y1() - arc.y2())));

  // Calculate intersection with extension to get the end point
  QPointF intersect;
  bool intersects = extension.intersect(returnSegment, &intersect) != QLineF::NoIntersection;
  if(intersects)
    returnSegment.setP2(intersect);
  // Make return segment a bit shorter than turn segment
  returnSegment.setLength(returnSegment.length() * 0.8);

  painter->drawLine(turnSegment);
  paintArc(painter, arc.p1(), arc.p2(), arc.pointAt(0.5), left);
  painter->drawLine(returnSegment.p1(), returnSegment.p2());

  // Calculate arrow for return segment
  QLineF arrow(returnSegment.p2(), returnSegment.p1());
  arrow.setLength(scale->getPixelForNm(0.15f, angleFromQt(returnSegment.angle())));

  QPolygonF poly;
  poly << arrow.p2() << arrow.p1();
  if(left)
    arrow.setAngle(angleToQt(turnCourse - 15.f));
  else
    arrow.setAngle(angleToQt(turnCourse + 15.f));
  poly << arrow.p2();

  painter->save();
  QPen pen = painter->pen();
  pen.setCapStyle(Qt::SquareCap);
  pen.setJoinStyle(Qt::MiterJoin);
  painter->setPen(pen);
  painter->drawPolygon(poly);
  painter->restore();
}
