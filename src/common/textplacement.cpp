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

#include "common/coordinateconverter.h"
#include "common/textplacement.h"

#include "geo/line.h"
#include "geo/calculations.h"
#include "geo/linestring.h"

#include <QPainter>

using atools::geo::Line;
using atools::geo::Pos;

TextPlacement::TextPlacement(QPainter *painterParam, CoordinateConverter *coordinateConverter)
  : painter(painterParam), converter(coordinateConverter), arrowRight(tr(" ►")), arrowLeft(tr("◄ "))
{

}

void TextPlacement::calculateTextPositions(const atools::geo::LineString& points)
{
  visibleStartPoints.resize(points.size() + 1);

  for(int i = 0; i < points.size(); i++)
  {
    int x1 = 0, y1 = 0;
    bool visibleStart = points.at(i).isValid() ? converter->wToS(points.at(i), x1, y1) : false;
    visibleStartPoints.setBit(i, visibleStart);
    startPoints.append(QPointF(x1, y1));
  }
}

void TextPlacement::calculateTextAlongLines(const QVector<atools::geo::Line>& lines, const QStringList& routeTexts)
{
  visibleStartPoints.resize(lines.size() + 1);

  QFontMetrics metrics = painter->fontMetrics();
  int x1, y1, x2, y2;
  for(int i = 0; i < lines.size(); i++)
  {
    const Line& line = lines.at(i);

    if(!fast)
    {
      converter->wToS(line.getPos1(), x1, y1);
      converter->wToS(line.getPos2(), x2, y2);

      int lineLength = atools::geo::simpleDistance(x1, y1, x2, y2);

      if(line.isValid() && lineLength > MIN_LENGTH_FOR_TEXT)
      {
        // Build text
        QString text = routeTexts.at(i);

        int textw = painter->fontMetrics().width(text);
        if(textw > lineLength)
          // Limit text length to line for elide
          textw = lineLength;

        int xt, yt;
        float brg;
        if(findTextPos(lines.at(i).getPos2(), lines.at(i).getPos1(), textw, metrics.height(), xt, yt, &brg))
        {
          textCoords.append(QPoint(xt, yt));
          textBearing.append(brg);
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
        textBearing.append(0.f);
        texts.append(QString());
        textLineLengths.append(0);
        if(!colors.isEmpty())
          colors2.append(QColor());
      }
    }
  }

  if(!lines.isEmpty())
  {
    // Add last point
    const Line& line = lines.last();
    converter->wToS(line.getPos2(), x2, y2);

    if(!colors.isEmpty())
      colors2.append(colors.at(lines.size() - 1));
  }
}

void TextPlacement::drawTextAlongOneLine(const QString& text, float bearing,
                                         const QPointF& textCoord,
                                         bool bothVisible, int textLineLength)
{
  if(!text.isEmpty())
  {
    QString newText(text);
    // Cut text right or left depending on direction
    Qt::TextElideMode elide = Qt::ElideRight;
    float rotate;
    if(bearing < 180.)
    {
      if(!arrowRight.isEmpty())
        newText += arrowRight;
      elide = Qt::ElideLeft;
      rotate = bearing - 90.f;
    }
    else
    {
      // Text is flipped upside down for readability
      if(!arrowLeft.isEmpty())
        newText = arrowLeft + newText;
      elide = Qt::ElideRight;
      rotate = bearing + 90.f;
    }

    // Draw text
    QFontMetricsF metrics = painter->fontMetrics();

    // Both points are visible - cut text for full line length
    if(bothVisible)
      newText = metrics.elidedText(newText, elide, textLineLength);

    float yoffset;
    if(textOnTopOfLine || bearing >= 180.)
      // Keep all texts north
      yoffset = -metrics.descent() - lineWidth / 2.f - 2.f;
    else
      yoffset = metrics.ascent() + lineWidth / 2.f + 2.f;

    painter->translate(textCoord.x(), textCoord.y());
    painter->rotate(rotate);

    QPointF textPos(-metrics.width(newText) / 2.f, yoffset);
    painter->drawText(textPos, newText);
    painter->resetTransform();
  }
}

void TextPlacement::drawTextAlongLines()
{
  if(!fast)
  {
    // Draw text with direction arrow along lines
    int i = 0;
    for(const QPointF& textCoord : textCoords)
    {
      if(!colors2.isEmpty() && colors2.at(i).isValid())
        painter->setPen(colors2.at(i));

      drawTextAlongOneLine(texts.at(i), textBearing.at(i), textCoord,
                           visibleStartPoints.testBit(i) && visibleStartPoints.testBit(i + 1),
                           textLineLengths.at(i));
      i++;
    }
  }
}

bool TextPlacement::findTextPos(const Pos& pos1, const Pos& pos2,
                                int textWidth, int textHeight, int& x, int& y, float *bearing)
{
  return findTextPos(pos1, pos2, pos1.distanceMeterTo(pos2), textWidth, textHeight, x, y, bearing);
}

bool TextPlacement::findTextPos(const Pos& pos1, const Pos& pos2, float distanceMeter,
                                int textWidth, int textHeight, int& x, int& y, float *bearing)
{
  if(!pos1.isValid() || !pos2.isValid())
    return false;

  int size = std::max(textWidth, textHeight);
  Pos center = pos1.interpolate(pos2, distanceMeter, 0.5);
  bool visible = converter->wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - size / 2, y - size / 2, size, size)))
  {
    // Center point is already visible
    if(bearing != nullptr)
    {
      // Calculate bearing at the center point
      float xtp1, ytp1, xtp2, ytp2;
      converter->wToS(pos1.interpolate(pos2, 0.5f - FIND_TEXT_POS_STEP), xtp1, ytp1);
      converter->wToS(pos1.interpolate(pos2, 0.5f + FIND_TEXT_POS_STEP), xtp2, ytp2);
      QLineF lineF(xtp1, ytp1, xtp2, ytp2);

      *bearing = static_cast<float>(atools::geo::normalizeCourse(-lineF.angle() + 270.f));
    }
    return true;
  }
  else
  {
    int x1, y1, x2, y2;
    // Check for 50 positions along the line starting below and above the center position
    for(float i = 0.; i <= 0.5; i += FIND_TEXT_POS_STEP)
    {
      center = pos1.interpolate(pos2, distanceMeter, 0.5f - i);
      visible = converter->wToS(center, x1, y1);
      if(visible && painter->window().contains(QRect(x1 - size / 2, y1 - size / 2, size, size)))
      {
        // Point is visible - return
        if(bearing != nullptr)
        {
          float xtp1, ytp1, xtp2, ytp2;
          converter->wToS(pos1.interpolate(pos2, 0.5f - i - FIND_TEXT_POS_STEP), xtp1, ytp1);
          converter->wToS(pos1.interpolate(pos2, 0.5f - i + FIND_TEXT_POS_STEP), xtp2, ytp2);
          QLineF lineF(xtp1, ytp1, xtp2, ytp2);
          *bearing = static_cast<float>(atools::geo::normalizeCourse(-lineF.angle() + 270.f));
        }
        x = x1;
        y = y1;
        return true;
      }

      center = pos1.interpolate(pos2, distanceMeter, 0.5f + i);
      visible = converter->wToS(center, x2, y2);
      if(visible && painter->window().contains(QRect(x2 - size / 2, y2 - size / 2, size, size)))
      {
        // Point is visible - return
        if(bearing != nullptr)
        {
          float xtp1, ytp1, xtp2, ytp2;
          converter->wToS(pos1.interpolate(pos2, 0.5f + i - FIND_TEXT_POS_STEP), xtp1, ytp1);
          converter->wToS(pos1.interpolate(pos2, 0.5f + i + FIND_TEXT_POS_STEP), xtp2, ytp2);
          QLineF lineF(xtp1, ytp1, xtp2, ytp2);
          *bearing = static_cast<float>(atools::geo::normalizeCourse(-lineF.angle() + 270.f));
        }
        x = x2;
        y = y2;
        return true;
      }
    }
  }
  return false;
}

bool TextPlacement::findTextPosRhumb(const Pos& pos1, const Pos& pos2,
                                     float distanceMeter, int textWidth, int textHeight, int& x, int& y)
{
  if(!pos1.isValid() || !pos2.isValid())
    return false;

  Pos center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5);
  bool visible = converter->wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
    return true;
  else
  {
    // Check for 50 positions along the line starting below and above the center position
    for(float i = 0.; i <= 0.5; i += FIND_TEXT_POS_STEP)
    {
      center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5f - i);
      visible = converter->wToS(center, x, y);
      if(visible &&
         painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
        return true;

      center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5f + i);
      visible = converter->wToS(center, x, y);
      if(visible &&
         painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
        return true;
    }
  }
  return false;
}

void TextPlacement::clearLineTextData()
{
  textCoords.clear();
  textBearing.clear();
  texts.clear();
  textLineLengths.clear();
  startPoints.clear();
  visibleStartPoints.clear();
  colors2.clear();
}
