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

#include "common/coordinateconverter.h"
#include "common/textplacement.h"

#include "geo/line.h"
#include "common/mapflags.h"
#include "geo/calculations.h"
#include "geo/linestring.h"

#ifdef DEBUG_TEXPLACEMENT_PAINT
#include "util/paintercontextsaver.h"
#endif

#include <QPainter>
#include <QStringBuilder>

using atools::geo::Line;
using atools::geo::LineString;
using atools::geo::Pos;

TextPlacement::TextPlacement(QPainter *painterParam, const CoordinateConverter *coordinateConverter,
                             const QRect& screenRectParam)
  : painter(painterParam), converter(coordinateConverter)
{
  arrowRight = tr(" ►");
  arrowLeft = tr("◄ ");
  sectionSeparator = tr(" / ");
  screenRect = screenRectParam;
}

void TextPlacement::calculateTextPositions(const atools::geo::LineString& points)
{
  visibleStartPoints.resize(points.size() + 1);

  for(int i = 0; i < points.size(); i++)
  {
    int x1 = 0, y1 = 0;
    bool hidden = false;
    bool visibleStart = points.at(i).isValid() ?
                        converter->wToS(points.at(i), x1, y1, CoordinateConverter::DEFAULT_WTOS_SIZE, &hidden) : false;

    if(!visibleStart && !screenRect.isNull() && !hidden)
      // Not visible - try the (extended) screen rectangle if not hidden behind the globe
      visibleStart = screenRect.contains(x1, y1);

    visibleStartPoints.setBit(i, visibleStart);
    startPoints.append(QPointF(x1, y1));
  }
}

void TextPlacement::calculateTextAlongLines(const QVector<atools::geo::Line>& lines, const QStringList& lineTexts)
{
  visibleStartPoints.resize(lines.size() + 1);

  QFontMetricsF metrics(painter->font());
  float x1, y1, x2, y2;
  for(int i = 0; i < lines.size(); i++)
  {
    const Line& line = lines.at(i);

    if(!fast)
    {
      converter->wToS(line.getPos1(), x1, y1);
      converter->wToS(line.getPos2(), x2, y2);

      float lineLength = atools::geo::simpleDistanceF(x1, y1, x2, y2);

      if(line.isValid() && lineLength > minLengthForText)
      {
        // Build temporary elided text to get the right length - use any arrow
        QString text = elideText(lineTexts.at(i), arrowLeft, metrics, lineLength);

        float xt, yt, brg;
        if(findTextPos(lines.at(i), lines.at(i).lengthMeter(), horizontalAdvance(text, metrics),
                       static_cast<float>(metrics.height()), maximumPoints, xt, yt, &brg))
        {
          textCoords.append(QPointF(xt, yt));
          textBearings.append(brg);
          texts.append(text);
          textLineLengths.append(lineLength);
          if(!colors.isEmpty())
            colors2.append(colors.at(i));
        }
      }
      else
      {
        // No text - append all dummy values
        textCoords.append(QPointF());
        textBearings.append(0.f);
        texts.append(QString());
        textLineLengths.append(0.f);
        if(!colors.isEmpty())
          colors2.append(QColor());
      }
    }
  }

  if(!lines.isEmpty())
  {
    // Add last point
    const Line& line = lines.constLast();
    converter->wToS(line.getPos2(), x2, y2);

    if(!colors.isEmpty())
      colors2.append(colors.at(lines.size() - 1));
  }
}

void TextPlacement::calculateTextAlongLine(const atools::geo::Line& line, const QString& lineText)
{
  calculateTextAlongLines({line}, {lineText});
}

float TextPlacement::horizontalAdvance(const QString& text, const QFontMetricsF& metrics) const
{
  double maxAdvance = 0.;
  const QStringList txts = text.split('\n');
  for(const QString& txt : txts)
    maxAdvance = std::max(maxAdvance, metrics.horizontalAdvance(txt));
  return static_cast<float>(maxAdvance);
}

QString TextPlacement::elideText(const QString& text, const QString& arrow, const QFontMetricsF& metrics, float lineLength) const
{
  QStringList txts = text.split('\n');
  for(QString& txt : txts)
    txt = metrics.elidedText(txt, Qt::ElideRight, lineLength - metrics.horizontalAdvance(arrow) - metrics.height() * 2);
  return txts.join('\n');
}

void TextPlacement::drawTextAlongOneLine(QString text, float bearing, const QPointF& textCoord, float textLineLength, Side side,
                                         float offset) const
{
  if(!text.isEmpty() || arrowForEmpty)
  {
    // Cut text right or left depending on direction
    float rotate;
    QString arrow;
    if(bearing < 180.f)
    {
      arrow = arrowRight;
      rotate = bearing - 90.f;
    }
    else
    {
      // Text is flipped upside down for readability
      arrow = arrowLeft;
      rotate = bearing + 90.f;
    }

    // Draw text
    QFontMetricsF metrics(painter->font());

    // Elide each line in text - considers \n and returns them too
    text = elideText(text, arrow, metrics, textLineLength);

    // Split separated text into lines
    QStringList txts = text.split('\n');

    // Swap sections if needed ============================================================
    if(bearing >= 180.f)
    {
      // Need to swap all sections separated by backslash due to rotation
      for(QString& txt : txts)
      {
        if(txt.contains('\\'))
          txt = txt.section('\\', 1, 1).trimmed() % sectionSeparator % txt.section('\\', 0, 0).trimmed();
      }
    }
    else
    {
      // No swap needed - replace backslash with separator
      for(QString& txt : txts)
        txt.replace('\\', sectionSeparator);
    }

    // Add arrow to center line ================================
    int mid = txts.size() / 2;
    if(bearing < 180.)
      txts[mid].append(arrow);
    else
      txts[mid] = arrow % txts.at(mid);

    // Remove all too short texts which contain only "..." but keep arrows
    txts.erase(std::remove_if(txts.begin(), txts.end(), [&arrow](const QString& txt)->bool {
      return txt.trimmed().size() <= 1 && txt != arrow;
    }), txts.end());

    if(!txts.isEmpty())
    {
      // Calculate offset ======================================
      // Text allowed below line
      double yoffsetBelow = lineWidth / 2. + offset + metrics.ascent();
      // Keep all texts north - positive moves down
      double yoffsetAbove = -(lineWidth / 2. + offset + metrics.height() * txts.size() - metrics.ascent());
      double yoffset = 0.; // Negative moves up

      if(side == CENTER || textOnLineCenter)
        yoffset = -metrics.height() * txts.size() / 2. + metrics.ascent();
      else if(side != NONE)
        yoffset = (side == LEFT && bearing >= 180.) || (side == RIGHT && bearing < 180.) ? yoffsetBelow : yoffsetAbove;
      else
        yoffset = textOnTopOfLine || bearing >= 180. ? yoffsetAbove : yoffsetBelow;

      painter->translate(textCoord.x(), textCoord.y());
      painter->rotate(rotate);

      for(int i = 0; i < txts.size(); i++)
      {
        // Add space at start and end to avoid letters touching the border
        QString txt = ' ' % txts.at(i) % ' ';
        painter->drawText(QPointF(-horizontalAdvance(txt, metrics) / 2.f, yoffset + i * metrics.height()), txt);
      }

      painter->resetTransform();
    }
  }
}

void TextPlacement::drawTextAlongLines()
{
  if(!fast)
  {
    // Draw text with direction arrow along lines
    int i = 0;
    for(const QPointF& textCoord : qAsConst(textCoords))
    {
      if(!colors2.isEmpty() && colors2.at(i).isValid())
        painter->setPen(colors2.at(i));

      drawTextAlongOneLine(texts.at(i), textBearings.at(i), textCoord, textLineLengths.at(i));
      i++;
    }
  }
}

bool TextPlacement::findTextPos(const Line& line, float distanceMeter, float textWidth, float textHeight, int maxPoints,
                                float& x, float& y, float *bearing) const
{
  float brg = 0.f;

  // Try with three points first and do not allow partial visibility. Finds only the center point.
  bool retval = findTextPosInternal(line, distanceMeter, textWidth, textHeight, 3, false /* allowPartial */, x, y, brg);

  if(!retval)
    // Nothing found for the center line try again
    retval = findTextPosInternal(line, distanceMeter, textWidth, textHeight, maxPoints, true /* allowPartial */, x, y, brg);

  if(bearing != nullptr)
    *bearing = brg;

  return retval;
}

float TextPlacement::getArrowWidth() const
{
  QFontMetricsF metrics(painter->font());
  return static_cast<float>(std::max(metrics.horizontalAdvance(arrowLeft), metrics.horizontalAdvance(arrowRight)) +
                            metrics.horizontalAdvance(' '));
}

int TextPlacement::findClosestInternal(const QVector<int>& fullyVisibleValid, const QVector<int>& pointsIdxValid, const QPolygonF& points,
                                       const QVector<QPointF>& neighbors) const
{
  int closestIndex = -1;
  double closestDist = map::INVALID_DISTANCE_VALUE;
  for(const QPointF& close : neighbors)
  {
    for(int i : fullyVisibleValid)
    {
      QPointF pt = points.at(pointsIdxValid.at(i));
      double dist = QLineF(pt, close).length();
      if(dist < closestDist)
      {
        closestDist = dist;
        closestIndex = i;
      }
    }
  }
  if(closestIndex != -1)
    return closestIndex;
  else
    return -1;
}

bool TextPlacement::findTextPosInternal(const Line& line, float distanceMeter, float textWidth, float textHeight, int numPoints,
                                        bool allowPartial, float& x, float& y, float& bearing) const
{
#ifdef DEBUG_TEXPLACEMENT_PAINT
  atools::util::PainterContextSaver saver(painter);

  painter->setPen(QPen(Qt::blue, 0.5));
#endif

  if(!line.isValid())
    return false;

  // Split line into a number of positions ============================================
  LineString positions;
  line.interpolatePoints(distanceMeter, numPoints - 1, positions);
  positions.append(line.getPos2());

  // Collect positions which are not hidden ============================================
  QPolygonF points;
  double lineLength = 0.;
  bool firstVisible = false, lastVisible = false;
  for(int i = 0; i < positions.size(); i++)
  {
    const Pos& pos = positions.at(i);
    bool hidden = false;
    float xt, yt;
    bool visible = converter->wToS(pos, xt, yt, QSize(0, 0), &hidden);
    if(!hidden)
    {
      if(!points.isEmpty())
        lineLength += QLineF(QPointF(xt, yt), points.constLast()).length();
      points.append(QPointF(xt, yt));

      // Remember visibility for first and last point
      if(i == 0)
        firstVisible = visible;
      else if(i == positions.size() - 1)
        lastVisible = visible;
    }
  }

  if(!points.isEmpty())
  {
    double textWidth2 = textWidth / 2., textHeight2 = textHeight / 2.; // Half width and height
    double square2 = std::max(textWidth2, textHeight2);
    const QPointF square(square2, square2);
    double fromStart = 0., toEnd = lineLength;
    QRectF screenRectF = screenRect;

    // Calculate bearing and do some first rough filtering ============================================
    QVector<int> pointsIdxValid; // Index into "points" of fully or partially visible points
    QVector<double> bearingsValid; // Same size vector as above
    for(int i = 0; i < points.size(); i++)
    {
      const QPointF& pt = points.at(i);

      if(i > 0)
      {
        // Adjust distance to endpoints
        double l = QLineF(points.at(i - 1), pt).length();
        fromStart += l;
        toEnd -= l;
      }

      // Check if text is getting too close to the line ends to avoid overshoot
      bool touchingStart = fromStart < textWidth / 2.f + textHeight;
      bool touchingEnd = toEnd < textWidth / 2.f + textHeight;

      // Text fits if not too close to the ends and if roughly visible by checking the squared rectangle
      if((!touchingStart && !touchingEnd) && !pt.isNull() && screenRectF.intersects(QRectF(pt - square, pt + square)))
      {
        QLineF textLine;
        if(i == 0)
          // Use first and second point for bearing calculation
          textLine.setPoints(pt, points.at(1));
        else if(i == points.size() - 1)
          // Use second last and last point for bearing calculation
          textLine.setPoints(points.at(points.size() - 2), pt);
        else
          // Use previous and next point for bearing calculation
          textLine.setPoints(points.at(i - 1), points.at(i + 1));

        bearingsValid.append(atools::geo::normalizeCourse(-textLine.angle()));
        pointsIdxValid.append(i);
      }
    }

    if(!pointsIdxValid.isEmpty())
    {
      // Now to precise filtering using the rotated text polygons ============================================
      double margin = textHeight * 2.;
      QRectF screenRectSmall = screenRectF.marginsRemoved(QMarginsF(margin, margin, margin, margin));
      QPolygonF screenPolygon({screenRect.topLeft(), screenRect.topRight(), screenRect.bottomRight(), screenRect.bottomLeft(),
                               screenRect.topLeft()});

      QVector<int> fullyVisibleValid /* Index into "pointsIdxValid". Fully within screen rect */,
                   partiallyVisibleValid /* Index into "pointsIdxValid". Only touching screen rect */;
      QMatrix matrix;
      for(int i = 0; i < pointsIdxValid.size(); i++)
      {
        const QPointF& pt = points.at(pointsIdxValid.at(i));

        // Build the text rectangle as polygon ===================================
        QPolygonF textPolygon({QPointF(pt.x() - textWidth2, pt.y() - textHeight2), QPointF(pt.x() + textWidth2, pt.y() - textHeight2),
                               QPointF(pt.x() + textWidth2, pt.y() + textHeight2),
                               QPointF(pt.x() - textWidth2, pt.y() + textHeight2), QPointF(pt.x() - textWidth2, pt.y() - textHeight2)});

        // Translate, rotate and translate text rectangle back ======================================
        matrix.translate(pt.x(), pt.y()).rotate(bearingsValid.at(i)).translate(-pt.x(), -pt.y());
        textPolygon = matrix.map(textPolygon);
        matrix.reset();

        // Check full visibility ================
        if(screenRectSmall.contains(textPolygon.boundingRect()))
        {
          fullyVisibleValid.append(i);
#ifdef DEBUG_TEXPLACEMENT_PAINT
          painter->setPen(QPen(Qt::red, 0.8));
          painter->drawPolyline(textPolygon);
          painter->drawText(textPolygon.constFirst(), QString("ptIdx %1 i %2").arg(ptIdx).arg(i));
#endif
        }
        // Check partial visibility ================
        else if(screenPolygon.intersects(textPolygon) && allowPartial)
        {
          partiallyVisibleValid.append(i);
#ifdef DEBUG_TEXPLACEMENT_PAINT
          painter->setPen(QPen(Qt::blue, 0.8, Qt::DotLine));
          painter->drawPolyline(textPolygon);
          painter->drawText(textPolygon.constFirst(), QString("ptIdx %1 i %2").arg(ptIdx).arg(i));
#endif
        }
      }

      // Select best points from full or partial visible lists ============================================
      int foundIndexValid = -1;
      if(!fullyVisibleValid.isEmpty())
        // There are fully visible points - get one closest to the center point
        foundIndexValid = findClosestInternal(fullyVisibleValid, pointsIdxValid, points, {points.at(points.size() / 2)});
      else if(!partiallyVisibleValid.isEmpty())
      {
        // There are only partially visible points ====================================
        if(!firstVisible && !lastVisible)
          // Neither first nor last are visible - get one closest to the center point
          foundIndexValid = findClosestInternal(partiallyVisibleValid, pointsIdxValid, points, {points.at(points.size() / 2)});
        else
        {
          // Get one point closest to either last or first
          QVector<QPointF> nearest;
          if(firstVisible)
            nearest.append(points.constFirst());
          if(lastVisible)
            nearest.append(points.constLast());
          foundIndexValid = findClosestInternal(partiallyVisibleValid, pointsIdxValid, points, nearest);
        }
      }

      if(foundIndexValid != -1)
      {
        // Calculate line heading - NOT text rotation
        bearing = static_cast<float>(atools::geo::normalizeCourse(bearingsValid.at(foundIndexValid) + 90.));

        const QPointF& pt = points.at(pointsIdxValid.at(foundIndexValid));
        x = static_cast<float>(pt.x());
        y = static_cast<float>(pt.y());
        return true;
      }
    } // if(!pointsIdxValid.isEmpty())
  } // if(!points.isEmpty())

  return false;
}

void TextPlacement::clearLineTextData()
{
  textCoords.clear();
  textBearings.clear();
  texts.clear();
  textLineLengths.clear();
  startPoints.clear();
  visibleStartPoints.clear();
  colors2.clear();
}
