/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/mappainter.h"

#include "app/navapp.h"
#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/maptools.h"
#include "common/maptypes.h"
#include "common/symbolpainter.h"
#include "common/textplacement.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/GeoPainter.h>

#include <QPixmapCache>
#include <QPainterPath>
#include <QStringBuilder>

using namespace Marble;
using namespace atools::geo;
using atools::roundToInt;

AirportPaintData::AirportPaintData(const map::MapAirport& ap, float x, float y)
  : airport(new map::MapAirport(ap)), point(x, y)
{

}

AirportPaintData::AirportPaintData(const AirportPaintData& other)
  : airport(new map::MapAirport)
{
  this->operator=(other);
}

AirportPaintData::AirportPaintData()
  : airport(new map::MapAirport)
{
}

AirportPaintData::~AirportPaintData()
{
  delete airport;
}

AirportPaintData& AirportPaintData::operator=(const AirportPaintData& other)
{
  *airport = *other.airport;
  point = other.point;
  return *this;
}

void PaintContext::szFont(float scale) const
{
  mapcolors::scaleFont(painter, scale, &defaultFont);
}

textflags::TextFlags PaintContext::airportTextFlags() const
{
  // Build and draw airport text
  textflags::TextFlags textflags = textflags::NONE;

  if(mapLayerText->isAirportInfo())
    textflags = textflags::INFO;

  if(mapLayerText->isAirportIdent())
    textflags |= textflags::IDENT;

  if(mapLayerText->isAirportName())
    textflags |= textflags::NAME;

  if(!flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}

textflags::TextFlags PaintContext::airportTextFlagsMinor() const
{
  // Build and draw airport text
  textflags::TextFlags textflags = textflags::NONE;

  if(mapLayerText->isAirportMinorInfo())
    textflags = textflags::INFO;

  if(mapLayerText->isAirportMinorIdent())
    textflags |= textflags::IDENT;

  if(mapLayerText->isAirportMinorName())
    textflags |= textflags::NAME;

  if(!flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}

textflags::TextFlags PaintContext::airportTextFlagsRoute(bool drawAsRoute, bool drawAsLog) const
{
  // Show ident always on route
  textflags::TextFlags textflags = textflags::IDENT;

  if(drawAsRoute)
    textflags |= textflags::ROUTE_TEXT;

  if(drawAsLog)
    textflags |= textflags::LOG_TEXT;

  // Use more more detailed text for flight plan
  if(mapLayerRouteText->isAirportRouteInfo())
    textflags |= textflags::NAME | textflags::INFO;

  if(!(flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}

// =================================================
MapPainter::MapPainter(MapPaintWidget *parentMapWidget, MapScale *mapScale, PaintContext *paintContext)
  : CoordinateConverter(parentMapWidget->viewport()), context(paintContext), mapPaintWidget(parentMapWidget), scale(mapScale)
{
  symbolPainter = new SymbolPainter();
}

MapPainter::~MapPainter()
{
  delete symbolPainter;
}

bool MapPainter::wToSBuf(const Pos& coords, int& x, int& y, QSize size, const QMargins& margins, bool *hidden) const
{
  float xf, yf;
  bool visible = wToSBuf(coords, xf, yf, size, margins, hidden);
  x = atools::roundToInt(xf);
  y = atools::roundToInt(yf);

  return visible;
}

bool MapPainter::wToSBuf(const Pos& coords, float& x, float& y, QSize size, const QMargins& margins, bool *hidden) const
{
  bool hid = false;
  bool visible = wToS(coords, x, y, size, &hid);

  if(hidden != nullptr)
    *hidden = hid;

  if(!visible && !hid)
    // Check additional visibility using the extended rectangle only if the object is not hidden behind the globe
    return context->screenRect.marginsAdded(margins).contains(atools::roundToInt(x), atools::roundToInt(y));

  return visible;
}

bool MapPainter::wToSBuf(const atools::geo::Pos& coords, QPointF& point, const QMargins& margins, bool *hidden) const
{
  float x, y;
  bool retval = wToSBuf(coords, x, y, DEFAULT_WTOS_SIZE, margins, hidden);
  point.setX(x);
  point.setY(y);
  return retval;
}

void MapPainter::paintArc(GeoPainter *painter, const Pos& centerPos, float radiusNm, float angleDegStart, float angleDegEnd,
                          bool fast) const
{
  if(radiusNm > atools::geo::EARTH_CIRCUMFERENCE_METER / 4.f)
    return;

  // Calculate the number of points to use depending on screen resolution
  int pixel = scale->getPixelIntForMeter(nmToMeter(radiusNm));
  int numPoints = std::min(std::max(pixel / (fast ? 20 : 2), CIRCLE_MIN_POINTS), CIRCLE_MAX_POINTS);

  float radiusMeter = nmToMeter(radiusNm);

  int step = 360 / numPoints;
  int x1, y1, x2 = -1, y2 = -1;

  // Use north endpoint of radius as start position
  Pos startPoint = centerPos.endpoint(radiusMeter, angleDegStart);
  Pos p1 = startPoint;
  bool hidden1 = true, hidden2 = true;
  wToS(p1, x1, y1, DEFAULT_WTOS_SIZE, &hidden1);

  bool ringVisible = false, lastVisible = false;
  LineString ellipse;
  if(angleDegEnd < angleDegStart)
    angleDegEnd += 360.f;

  for(float angle = angleDegStart; angle <= angleDegEnd; angle += step)
  {
    // Line segment from p1 to p2
    Pos p2 = centerPos.endpoint(radiusMeter, atools::geo::normalizeCourse(angle));

    wToS(p2, x2, y2, DEFAULT_WTOS_SIZE, &hidden2);

    QRect rect(QPoint(x1, y1), QPoint(x2, y2));
    rect = rect.normalized();
    // Avoid points or flat rectangles (lines)
    rect.adjust(-1, -1, 1, 1);

    // Current line is visible (most likely)
    bool nowVisible = rect.intersects(painter->viewport());

    if(lastVisible || nowVisible)
      // Last line or this one are visible add coords
      ellipse.append(p1);

    if(lastVisible && !nowVisible)
    {
      // Not visible anymore draw previous line segment
      drawPolyline(painter, ellipse);
      ellipse.clear();
    }

    if(lastVisible || nowVisible)
      // At least one segment of the arc is visible
      ringVisible = true;

    x1 = x2;
    y1 = y2;
    hidden1 = hidden2;
    p1 = p2;
    lastVisible = nowVisible;
  }

  if(ringVisible && !ellipse.isEmpty())
  {
    ellipse.append(centerPos.endpoint(radiusMeter, angleDegEnd));
    drawPolyline(painter, ellipse);
  }
}

void MapPainter::paintCircle(GeoPainter *painter, const Pos& centerPos, float radiusNm, bool fast, QPoint *textPos) const
{
  if(radiusNm > atools::geo::EARTH_CIRCUMFERENCE_METER / 4.f)
    return;

  if(radiusNm < 1.f || atools::geo::meterToNm(context->zoomDistanceMeter) < 5.f)
    // Use different method to draw circles with small radius to avoid distortion because of rounding errors
    // This one ignores spherical shape and projection at low zoom distances
    paintCircleSmallInternal(painter, centerPos, radiusNm, fast, textPos);
  else
    // Draw large circles with correct shape in projection
    paintCircleLargeInternal(painter, centerPos, radiusNm, fast, textPos);
}

void MapPainter::paintCircleSmallInternal(GeoPainter *painter, const Pos& centerPos, float radiusNm, bool fast, QPoint *textPos) const
{
  Q_UNUSED(fast)

  // Get pixel size for line from center to north
  int pixel = scale->getPixelIntForMeter(nmToMeter(radiusNm), 0.f);

  bool visible, hidden;
  QPoint pt = wToS(centerPos, QSize(pixel * 3, pixel * 3), &visible, &hidden);

  if(!hidden)
  {
    // Rectangle for the circle
    QRect rect(pt.x() - pixel, pt.y() - pixel, pixel * 2, pixel * 2);

    if(context->screenRect.intersects(rect))
    {
      // Draw simple circle
      painter->drawEllipse(pt, pixel, pixel);
      if(textPos != nullptr)
      {
        // Check each circle octant for a visible text position
        for(int angle = 0; angle <= 360; angle += 45)
        {
          // Create a line and rotate P2 clockwise
          QLineF line(pt.x(), pt.y(), pt.x(), pt.y() - pixel);
          line.setAngle(atools::geo::angleToQt(angle));

          if(context->screenRect.contains(atools::roundToInt(line.p2().x()), atools::roundToInt(line.p2().y())))
            *textPos = line.p2().toPoint();
        }
      }
    }
  }
}

void MapPainter::paintCircleLargeInternal(GeoPainter *painter, const Pos& centerPos, float radiusNm, bool fast, QPoint *textPos) const
{
  // Calculate the number of points to use depending on screen resolution
  int pixel = scale->getPixelIntForMeter(nmToMeter(radiusNm));
  int numPoints = std::min(std::max(pixel / (fast ? 20 : 2), CIRCLE_MIN_POINTS), CIRCLE_MAX_POINTS);

  float radiusMeter = nmToMeter(radiusNm);

  int step = 360 / numPoints;
  int x1, y1, x2 = -1, y2 = -1;
  if(textPos != nullptr)
    *textPos = QPoint(0, 0);

  QVector<int> xtexts;
  QVector<int> ytexts;

  // Use north endpoint of radius as start position
  Pos startPoint = centerPos.endpoint(radiusMeter, 0);
  Pos p1 = startPoint;
  bool hidden1 = true, hidden2 = true;
  bool visible1 = wToS(p1, x1, y1, DEFAULT_WTOS_SIZE, &hidden1);

  bool ringVisible = false, lastVisible = false;
  LineString ellipse;
  // Draw ring segments and collect potential text positions
  for(int i = step; i <= 360; i += step)
  {
    // Line segment from p1 to p2
    Pos p2 = centerPos.endpoint(radiusMeter, i);

    bool visible2 = wToS(p2, x2, y2, DEFAULT_WTOS_SIZE, &hidden2);

    QRect rect(QPoint(x1, y1), QPoint(x2, y2));
    rect = rect.normalized();
    // Avoid points or flat rectangles (lines)
    rect.adjust(-1, -1, 1, 1);

    // Current line is visible (most likely)
    bool nowVisible = rect.intersects(painter->viewport());

    if(lastVisible || nowVisible)
      // Last line or this one are visible add coords
      ellipse.append(p1);

    if(lastVisible && !nowVisible)
    {
      // Not visible anymore draw previous line segment
      drawPolyline(painter, ellipse);
      ellipse.clear();
    }

    if(lastVisible || nowVisible)
    {
      // At least one segment of the ring is visible
      ringVisible = true;

      if(textPos != nullptr)
      {
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
      ellipse.append(startPoint);
      drawPolyline(painter, ellipse);
    }

    if(textPos != nullptr)
    {
      if(!xtexts.isEmpty() && !ytexts.isEmpty())
        // Take the position at one third of the visible text points to avoid half hidden texts
        *textPos = QPoint(xtexts.at(xtexts.size() / 3), ytexts.at(ytexts.size() / 3));
      else
        *textPos = QPoint(0, 0);
    }
  }
}

void MapPainter::drawLineStraight(Marble::GeoPainter *painter, const atools::geo::Line& line) const
{
  double x1, y1, x2, y2;
  bool visible1 = wToS(line.getPos1(), x1, y1);
  bool visible2 = wToS(line.getPos2(), x2, y2);

  if(visible1 || visible2)
    drawLine(painter, QPointF(x1, y1), QPointF(x2, y2));
}

void MapPainter::drawLine(QPainter *painter, const QLineF& line) const
{
  static const QMarginsF MARGINS(1., 1., 1., 1.);
  QRectF rect(line.p1(), line.p2());
  // Add margins to avoid null width and height which will not intersect with viewport
  rect = rect.normalized().marginsAdded(MARGINS);

  if(atools::geo::lineValid(line) && QRectF(painter->viewport()).intersects(rect))
  {
    // if(line.intersect(QLineF(rect.topLeft(), rect.topRight()), nullptr) == QLineF::BoundedIntersection ||
    // line.intersect(QLineF(rect.topRight(), rect.bottomRight()), nullptr) == QLineF::BoundedIntersection ||
    // line.intersect(QLineF(rect.bottomRight(), rect.bottomLeft()), nullptr) == QLineF::BoundedIntersection ||
    // line.intersect(QLineF(rect.bottomLeft(), rect.topLeft()), nullptr) == QLineF::BoundedIntersection)
    painter->drawLine(line);
  }
}

void MapPainter::drawCircle(Marble::GeoPainter *painter, const atools::geo::Pos& center, float radius) const
{
  QPointF pt = wToSF(center);
  if(!pt.isNull())
    painter->drawEllipse(pt, radius, radius);
}

void MapPainter::drawText(Marble::GeoPainter *painter, const Pos& pos, const QString& text, bool topCorner, bool leftCorner) const
{
  QPoint pt = wToS(pos);
  if(!pt.isNull())
  {
    QFontMetrics metrics = painter->fontMetrics();
    pt.setX(leftCorner ? pt.x() : pt.x() - metrics.horizontalAdvance(text));
    pt.setY(topCorner ? pt.y() + metrics.ascent() : pt.y() - metrics.descent());
    painter->drawText(pt, text);
  }
}

void MapPainter::drawCross(Marble::GeoPainter *painter, int x, int y, int size) const
{
  painter->drawLine(x, y - size, x, y + size);
  painter->drawLine(x - size, y, x + size, y);
}

void MapPainter::drawPolyline(Marble::GeoPainter *painter, const atools::geo::LineString& linestring) const
{
  QVector<QPolygonF *> polygons = createPolylines(linestring, context->screenRect, true /* splitLongLines */);
  drawPolylines(painter, polygons);
  releasePolylines(polygons);
}

void MapPainter::drawPolylines(Marble::GeoPainter *painter, const QVector<QPolygonF *>& polygons) const
{
  for(const QPolygonF *polygon : polygons)
    drawPolyline(painter, *polygon);
}

void MapPainter::drawPolyline(Marble::GeoPainter *painter, const QPolygonF& polygon) const
{
  painter->drawPolyline(polygon);

#ifdef DEBUG_INFORMATION_PAINT_POLYGON
  {
    atools::util::PainterContextSaver save(painter);
    painter->setPen(Qt::black);
    for(int i = 0; i < polygon.size(); i++)
    {
      QPointF pt = polygon.at(i);
      painter->drawEllipse(pt, 3, 3);

      if((i % 4) == 0)
        pt.rx() += 10;
      else if((i % 4) == 1)
        pt.rx() -= 10;
      else if((i % 4) == 2)
        pt.ry() += 10;
      else if((i % 4) == 3)
        pt.ry() -= 10;
      painter->drawText(pt, QString::number(i));
    }
  }
#endif
}

void MapPainter::drawPolygon(Marble::GeoPainter *painter, const atools::geo::LineString& linestring) const
{
  QVector<QPolygonF *> polygons = createPolygons(linestring, context->screenRect);
  drawPolygons(painter, polygons);
  releasePolygons(polygons);
}

void MapPainter::drawPolygons(Marble::GeoPainter *painter, const QVector<QPolygonF *>& polygons) const
{
  for(const QPolygonF *polygon : polygons)
    drawPolygon(painter, *polygon);
}

void MapPainter::drawPolygon(Marble::GeoPainter *painter, const QPolygonF& polygon) const
{
  if(!polygon.isEmpty())
    painter->drawPolygon(polygon, Qt::OddEvenFill);

#ifdef DEBUG_INFORMATION_PAINT_POLYGON
  {
    atools::util::PainterContextSaver save(painter);
    painter->setPen(Qt::black);
    for(int i = 0; i < polygon.size(); i++)
    {
      QPointF pt = polygon.at(i);
      painter->drawEllipse(pt, 3, 3);

      if((i % 4) == 0)
        pt.rx() += 10;
      else if((i % 4) == 1)
        pt.rx() -= 10;
      else if((i % 4) == 2)
        pt.ry() += 10;
      else if((i % 4) == 3)
        pt.ry() -= 10;
      painter->drawText(pt, QString::number(i));
    }
  }
#endif
}

void MapPainter::drawLine(Marble::GeoPainter *painter, const atools::geo::Line& line, bool forceDraw) const
{
  LineString linestring(line.getPos1(), line.getPos2());

  // Move latitude values slightly up and down to workaround Marble drawing straight lines
  maptools::correctLatY(linestring, false /* polygon */);

  QVector<QPolygonF *> polygons = createPolylines(linestring, context->screenRect, true /* splitLongLines */);
  if(!polygons.isEmpty())
  {
    drawPolylines(painter, polygons);
    releasePolylines(polygons);
  }
  else if(forceDraw)
  {
    // Avoid disappearing line segments due to Marble

    bool visible1, visible2, hidden1, hidden2;
    QPointF pt1 = wToSF(line.getPos1(), DEFAULT_WTOS_SIZE, &visible1, &hidden1);
    QPointF pt2 = wToSF(line.getPos2(), DEFAULT_WTOS_SIZE, &visible2, &hidden2);
    if(!hidden1 && !hidden2)
      drawLine(context->painter, pt1, pt2);
  }
}

void MapPainter::paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& center, bool left) const
{
  QRectF arcRect;
  float startAngle, spanAngle;
  atools::geo::arcFromPoints(QLineF(p1, p2), center, left, &arcRect, &startAngle, &spanAngle);

  painter->drawArc(arcRect, atools::roundToInt(-startAngle * 16.), atools::roundToInt(spanAngle * 16.));
}

void MapPainter::paintHoldWithText(QPainter *painter, float x, float y, float direction,
                                   float lengthNm, float minutes, bool left,
                                   const QString& text, const QString& text2,
                                   const QColor& textColor, const QColor& textColorBackground,
                                   QVector<float> inboundArrows, QVector<float> outboundArrows) const
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

  // Build the rectangles that are used to draw the arcs ====================
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

  // Draw hold ============================================================
  // translate to orgin of hold (navaid or waypoint) and rotate
  painter->translate(x, y);
  painter->rotate(direction);

  // Draw hold
  painter->setBrush(Qt::transparent);
  painter->drawPath(path);

  // Draw arrows if requested ==================================================
  if(!inboundArrows.isEmpty() || !outboundArrows.isEmpty())
  {
    painter->save();
    // Calculate arrow size
    float arrowSize = static_cast<float>(painter->pen().widthF() * 2.3);

    // Use a lighter brush for fill and a thinner pen for lines
    painter->setBrush(painter->pen().color().lighter(300));
    painter->setPen(QPen(painter->pen().color(), painter->pen().widthF() * 0.66));

    if(!inboundArrows.isEmpty())
    {
      QPolygonF arrow = buildArrow(static_cast<float>(arrowSize));
      QLineF inboundLeg(0., pixel, 0., 0.);

      // 0,0 = origin and 0,pixel = start of inbound
      // Drawn an arrow for each position
      for(float pos : inboundArrows)
        painter->drawPolygon(arrow.translated(inboundLeg.pointAt(pos)));
    }

    if(!outboundArrows.isEmpty())
    {
      // Mirror y axis for left turn legs - convert arrow pointing up to pointing  down
      float leftScale = left ? -1.f : 1.f;
      QPolygonF arrowMirror = buildArrow(arrowSize, true);
      QLineF outboundLeg(pixel * 0.5f * leftScale, 0., pixel * 0.5f * leftScale, pixel);

      // 0,0 = origin and 0,pixel = start of inbound
      // Drawn an arrow for each position
      for(float pos : outboundArrows)
        painter->drawPolygon(arrowMirror.translated(outboundLeg.pointAt(pos)));
    }
    painter->restore();
  }

  if(!text.isEmpty() || !text2.isEmpty())
  {
    float lineWidth = static_cast<float>(painter->pen().widthF());
    // Move to first text position
    painter->translate(0, pixel / 2);
    painter->rotate(direction < 180.f ? 270. : 90.);

    painter->save();
    painter->setPen(textColor);
    painter->setBrush(textColorBackground);
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBackground(textColorBackground);

    QFontMetrics metrics = painter->fontMetrics();
    if(!text.isEmpty())
    {
      // text pointing to origin
      QString str = metrics.elidedText(text, Qt::ElideRight, roundToInt(pixel));
      int w1 = metrics.horizontalAdvance(str);
      painter->drawText(-w1 / 2, roundToInt(-lineWidth - 3), str);
    }

    if(!text2.isEmpty())
    {
      // text on other side to origin
      QString str = metrics.elidedText(text2, Qt::ElideRight, roundToInt(pixel));
      int w2 = metrics.horizontalAdvance(str);

      if(direction < 180.f)
        painter->translate(0, left ? -pixel / 2 : pixel / 2);
      else
        painter->translate(0, left ? pixel / 2 : -pixel / 2);
      painter->drawText(-w2 / 2, roundToInt(-lineWidth - 3), str);
    }
    painter->restore();
  }
  painter->resetTransform();
}

void MapPainter::paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm,
                                            bool left, QLineF *extensionLine, const QString& text,
                                            const QColor& textColor, const QColor& textColorBackground) const
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
  extension.setLength(scale->getPixelForNm(distanceNm, static_cast<float>(angleFromQt(extension.angle()))));

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
    float lineWidth = static_cast<float>(painter->pen().widthF());

    painter->save();
    painter->setPen(textColor);
    painter->setBackground(textColorBackground);
    QFontMetrics metrics = painter->fontMetrics();
    QString str = metrics.elidedText(text, Qt::ElideRight, roundToInt(turnSegment.length()));
    int w1 = metrics.horizontalAdvance(str);

    painter->translate((turnSegment.x1() + turnSegment.x2()) / 2, (turnSegment.y1() + turnSegment.y2()) / 2);
    painter->rotate(turnCourse < 180.f ? turnCourse - 90.f : turnCourse + 90.f);
    painter->drawText(-w1 / 2, roundToInt(-lineWidth - 3), str);
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
  arrow.setLength(scale->getPixelForNm(0.15f, static_cast<float>(angleFromQt(returnSegment.angle()))));

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

void MapPainter::paintAircraftTrail(const QVector<LineString>& lineStrings, float minAlt, float maxAlt,
                                    const atools::geo::Pos& aircraftPos) const
{
  if(!lineStrings.isEmpty())
  {
    // Add aircraft position to avoid gap to current position
    Line lastToAircraft;
    if(aircraftPos.isValid() && !lineStrings.constLast().isEmpty())
      lastToAircraft = Line(lineStrings.constLast().constLast(), aircraftPos);

    if(OptionData::instance().getFlags().testFlag(opts::MAP_TRAIL_GRADIENT))
    {
      // Draw black or white background as one single line ==========================
      context->painter->setPen(mapcolors::aircraftTrailPenOuter(context->szF(context->thicknessTrail, 2.f)));
      for(const LineString& lineString : lineStrings)
        drawPolyline(context->painter, lineString);

      if(lastToAircraft.isValid())
        drawLine(context->painter, lastToAircraft);

      // Split linestring and draw single line segments using different colors for gradient ==========================
      for(const LineString& lineString : lineStrings)
      {
        for(int i = 0; i < lineString.size() - 1; i++)
        {
          Line line(lineString.at(i), lineString.at(i + 1));

          // Average altitude between start and end point
          float alt = static_cast<float>((line.getPos1().getAltitude() + line.getPos2().getAltitude()) / 2.);

          context->painter->setPen(mapcolors::aircraftTrailPen(context->szF(context->thicknessTrail, 2.f), minAlt, maxAlt, alt));

          // Draw line and force drawing also for very short segments
          drawLine(context->painter, line, true /* forceDraw */);
        }
      }

      if(lastToAircraft.isValid())
        drawLine(context->painter, lastToAircraft);
    }
    else
    {
      // Styled pen - no gradient ==================================
      context->painter->setPen(mapcolors::aircraftTrailPen(context->szF(context->thicknessTrail, 2.f)));
      for(const LineString& lineString : lineStrings)
        drawPolyline(context->painter, lineString);

      if(lastToAircraft.isValid())
        drawLine(context->painter, lastToAircraft);
    }
  }
}

QPolygonF MapPainter::buildArrow(float size, bool downwards) const
{
  if(downwards)
    // Pointing downwards
    return QPolygonF({QPointF(0., size), QPointF(size, -size), QPointF(0., -size / 2.), QPointF(-size, -size)});
  else
    // Point up
    return QPolygonF({QPointF(0., -size), QPointF(size, size), QPointF(0., size / 2.), QPointF(-size, size)});
}

void MapPainter::paintArrowAlongLine(QPainter *painter, const atools::geo::Line& line, const QPolygonF& arrow, float pos,
                                     float minLengthPx) const
{
  bool visible, hidden;
  QPointF pt = wToSF(line.interpolate(pos), DEFAULT_WTOS_SIZE, &visible, &hidden);

  if(visible && !hidden)
  {
    bool draw = true;
    if(minLengthPx > 0.f)
    {
      QLineF lineF;
      wToS(line, lineF, DEFAULT_WTOS_SIZE, &hidden);

      draw = !hidden && lineF.length() > minLengthPx;
    }

    if(draw)
    {
      painter->translate(pt);
      painter->rotate(atools::geo::opposedCourseDeg(line.angleDeg()));
      painter->drawPolygon(arrow);
      painter->resetTransform();
    }
  }
}

void MapPainter::paintArrowAlongLine(QPainter *painter, const QLineF& line, const QPolygonF& arrow, float pos) const
{
  painter->translate(line.pointAt(pos));
  painter->rotate(atools::geo::angleFromQt(line.angle()));
  painter->drawPolygon(arrow);
  painter->resetTransform();
}

bool MapPainter::sortAirportFunction(const AirportPaintData& airportPaintData1, const AirportPaintData& airportPaintData2)
{
  // returns ​true if the first argument is less than (i.e. is ordered before) the second.
  // ">" puts true behind
  const OptionData& od = OptionData::instance();

  // Put add-on on top if any add-on filter is set
  bool addonFlag = context->objectTypes.testFlag(map::AIRPORT_ADDON_ZOOM) ||
                   context->objectTypes.testFlag(map::AIRPORT_ADDON_ZOOM_FILTER);

  bool empty3dFlag = od.getFlags2().testFlag(opts2::MAP_EMPTY_AIRPORTS_3D) &&
                     NavApp::getCurrentSimulatorDb() != atools::fs::FsPaths::XPLANE_12;
  bool emptyFlag = od.getFlags().testFlag(opts::MAP_EMPTY_AIRPORTS);
  int priority1 = airportPaintData1.getAirport().paintPriority(addonFlag, emptyFlag, empty3dFlag);
  int priority2 = airportPaintData2.getAirport().paintPriority(addonFlag, emptyFlag, empty3dFlag);

  if(priority1 == priority2)
    return airportPaintData1.getAirport().id < airportPaintData2.getAirport().id;
  else
    // Smaller priority: Draw first below all other. Higher priority: Draw last on top of other
    return priority1 < priority2;
}

void MapPainter::initQueries()
{
  queries = mapPaintWidget->getQueries();
}

void MapPainter::getPixmap(QPixmap& pixmap, const QString& resource, int size) const
{
  QPixmap *pixmapPtr = QPixmapCache::find(resource % "_" % QString::number(size));
  if(pixmapPtr == nullptr)
  {
    pixmap = QIcon(resource).pixmap(QSize(size, size));
    QPixmapCache::insert(pixmap);
  }
  else
    pixmap = *pixmapPtr;
}

void MapPainter::paintMsaMarks(const QList<map::MapAirportMsa>& airportMsa, bool user, bool drawFast) const
{
  Q_UNUSED(user)

  if(airportMsa.isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);
  GeoPainter *painter = context->painter;

  for(const map::MapAirportMsa& msa : airportMsa)
  {
    float x, y;
    bool msaVisible = wToS(msa.position, x, y, scale->getScreeenSizeForRect(msa.bounding));

    if(!msaVisible)
      // Check bounding rect for visibility
      msaVisible = msa.bounding.overlaps(context->viewportRect);

    if(msaVisible)
    {
      if(context->objCount())
        return;

      // Use width and style from pen but override transparency
      QColor gridCol = context->darkMap ? mapcolors::msaDiagramLinePenDark.color() : mapcolors::msaDiagramLinePen.color();
      gridCol.setAlphaF(atools::minmax(0., 1., 1. - context->transparencyAirportMsa));
      QPen pen = context->darkMap ? mapcolors::msaDiagramLinePenDark : mapcolors::msaDiagramLinePen;
      pen.setColor(gridCol);
      context->painter->setPen(pen);

      // Fill color for circle
      painter->setBrush(context->darkMap ? mapcolors::msaDiagramFillColorDark : mapcolors::msaDiagramFillColor);
      drawPolygon(painter, msa.geometry);

      TextPlacement textPlacement(painter, this, context->screenRect);
      QVector<atools::geo::Line> lines;
      QStringList texts;

      if(!drawFast)
      {
        // Skip lines if restriction is full circle
        if(msa.altitudes.size() > 1)
        {
          // Draw sector bearing lines and collect geometry and texts for placement =========================
          for(int i = 0; i < msa.bearingEndPositions.size(); i++)
          {
            texts.append(tr("%1%2").arg(atools::geo::normalizeCourse(msa.bearings.value(i))).arg(msa.trueBearing ? tr("°T") : tr("°M")));

            atools::geo::Line line(msa.bearingEndPositions.value(i), msa.position);
            lines.append(line);
            drawLine(painter, line);
          }
        }

        // Calculate font size from radius
        float fontSize = scale->getPixelForNm(msa.radius) / 8.f * context->textSizeAirportMsa;

        if(msa.altitudes.size() == 1)
          // Larger font for full circle restriction
          fontSize *= 2.f;

        if(fontSize > 4.f)
        {
          // Do not use transparency but override from options
          QColor textCol = context->darkMap ? mapcolors::msaDiagramNumberColorDark : mapcolors::msaDiagramNumberColor;
          textCol.setAlphaF(atools::minmax(0., 1., 1. - context->transparencyAirportMsa));
          context->painter->setPen(textCol);

          QFont font = context->painter->font();
          font.setPixelSize(atools::roundToInt(fontSize));
          context->painter->setFont(font);

          // Draw altitude labels ===================================================================
          for(int i = 0; i < msa.altitudes.size(); i++)
          {
            const atools::geo::Pos labelPos = msa.labelPositions.value(i);

            float xp, yp;
            bool visible = wToS(labelPos, xp, yp, scale->getScreeenSizeForRect(msa.bounding));

            if(visible)
            {
              QString text = Unit::altFeet(msa.altitudes.at(i), true /* addUnit */, true /* narrow */);
              QSizeF txtsize = painter->fontMetrics().boundingRect(text).size();
              painter->drawText(QPointF(xp - txtsize.width() / 2., yp + txtsize.height() / 2.), text);
            }
          }
        }
      }

      {
        atools::util::PainterContextSaver saverCenter(painter);

        painter->setFont(context->defaultFont);
        context->szFont(context->textSizeAirportMsa);

        painter->setPen(context->darkMap ? mapcolors::msaDiagramLinePenDark : mapcolors::msaDiagramLinePen);
        painter->setBrush(Qt::white);
        painter->setBackground(Qt::white);
        painter->setBackgroundMode(Qt::OpaqueMode);

        // Draw bearing labels ==========================================================================
        textPlacement.calculateTextAlongLines(lines, texts);
        textPlacement.drawTextAlongLines();

        // Draw small center circle ===================================================================
        drawCircle(painter, msa.position, 4);
      }
    }
  }
}

void MapPainter::paintHoldingMarks(const QList<map::MapHolding>& holdings, const MapLayer *layer, const MapLayer *layerText, bool user,
                                   bool drawFast, bool darkMap) const
{
  if(holdings.isEmpty())
    return;

  atools::util::PainterContextSaver saver(context->painter);
  GeoPainter *painter = context->painter;

  bool detail = layerText->isHoldingInfo();
  bool detail2 = layerText->isHoldingInfo2();

  QColor backColor = user || context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND ? QColor(Qt::white) : QColor(Qt::transparent);

  if(user)
    context->szFont(context->textSizeRangeUserFeature);
  else
    context->szFont(context->textSizeNavaid);

  QColor holdingColor = mapcolors::holdingColor.lighter(darkMap ? 250 : 100);

  for(const map::MapHolding& holding : holdings)
  {
    bool visible, hidden;
    QPointF pt = wToS(holding.getPosition(), DEFAULT_WTOS_SIZE, &visible, &hidden);
    if(hidden)
      continue;

    QColor color = user ? holding.color : holdingColor;

    float dist = holding.distance();
    float distPixel = scale->getPixelForNm(dist);
    float lineWidth = user ? context->szF(context->thicknessUserFeature, 3) : (detail2 ? 2.5f : 1.5f);

    if(layer->isApproach() && distPixel > 10.f)
    {
      // Calculcate approximate rectangle
      Rect rect(holding.position, atools::geo::nmToMeter(dist) * 2.f, true /* fast */);

      if(context->viewportRect.overlaps(rect))
      {
        painter->setPen(QPen(color, lineWidth, Qt::SolidLine));

        QStringList inboundText, outboundText;
        if(detail && !drawFast)
        {
          if(detail2)
          {
            // Text for inbound leg =======================================
            inboundText.append(formatter::courseTextFromTrue(holding.courseTrue, holding.magvar, false /* magBold */, false /* magBig */,
                                                             false /* trueSmall */, true /* narrow */));

            if(holding.time > 0.f)
              inboundText.append(tr("%1min").arg(QString::number(holding.time, 'g', 2)));
            if(holding.length > 0.f)
              inboundText.append(Unit::distNm(holding.length, true /* addUnit */, 1, true /* narrow */));
          }

          if(!holding.navIdent.isEmpty())
            inboundText.append(holding.navIdent);

          if(detail2)
          {
            // Text for outbound leg =======================================
            outboundText.append(formatter::courseTextFromTrue(opposedCourseDeg(holding.courseTrue), holding.magvar,
                                                              false /* magBold */, false /* magBig */, false /* trueSmall */,
                                                              true /* narrow */));

            if(user)
            {
              if(holding.speedKts > 0.f)
                outboundText.append(Unit::speedKts(holding.speedKts, true /* addUnit */, true /* narrow */));
              outboundText.append(Unit::altFeet(holding.position.getAltitude(), true /* addUnit */, true /* narrow */));
            }
            else
            {
              if(holding.speedLimit > 0.f)
                outboundText.append(Unit::speedKts(holding.speedLimit, true /* addUnit */, true /* narrow */));

              if(holding.minAltititude > 0.f)
                outboundText.append(tr("A%1").arg(Unit::altFeet(holding.minAltititude, true /* addUnit */,
                                                                true /* narrow */)));
              if(holding.maxAltititude > 0.f)
                outboundText.append(tr("B%2").arg(Unit::altFeet(holding.maxAltititude, true /* addUnit */,
                                                                true /* narrow */)));
            }
          }
        }

        paintHoldWithText(context->painter, static_cast<float>(pt.x()), static_cast<float>(pt.y()),
                          holding.courseTrue, dist, 0.f, holding.turnLeft,
                          inboundText.join(tr("/")), outboundText.join(tr("/")), color, backColor,
                          detail && !drawFast ? QVector<float>({0.80f}) : QVector<float>() /* inbound arrows */,
                          detail && !drawFast ? QVector<float>({0.80f}) : QVector<float>() /* outbound arrows */);
      } // if(context->viewportRect.overlaps(rect))
    } // if(context->mapLayer->isApproach() && scale->getPixelForNm(hold.distance()) > 10.f)

    if(visible /* && (detail || !enroute)*/)
    {
      // Draw triangle at hold fix - independent of zoom factor
      float radius = lineWidth * 2.5f;
      painter->setPen(QPen(color, lineWidth));
      painter->setBrush(backColor);
      painter->drawConvexPolygon(QPolygonF({QPointF(pt.x(), pt.y() - radius),
                                            QPointF(pt.x() + radius / 1.4, pt.y() + radius / 1.4),
                                            QPointF(pt.x() - radius / 1.4, pt.y() + radius / 1.4)}));
    }
  } // for(const map::Hold& hold : holds)
}
