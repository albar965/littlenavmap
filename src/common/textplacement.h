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

#ifndef LITTLENAVMAP_TEXTPLACEMENT_H
#define LITTLENAVMAP_TEXTPLACEMENT_H

#include <QBitArray>
#include <QLineF>
#include <QList>
#include <QPoint>
#include <QApplication>
#include <QColor>
#include <QVector>

namespace atools {
namespace geo {
class Line;
class Pos;
}
}

class QPainter;
class CoordinateConverter;

class TextPlacement
{
  Q_DECLARE_TR_FUNCTIONS(TextPlacement)

public:
  TextPlacement(QPainter *painterParam, CoordinateConverter *coordinateConverter);

  /* Prepare for drawTextAlongLines and also fills data for getVisibleStartPoints and getStartPoints */
  void calculateTextAlongLines(const QVector<atools::geo::Line>& lines, const QStringList& routeTexts);

  /* Draw text after calling calculateTextAlongLines. Text is aligned with lines and kept in an
   * upwards readable position. Arrows are optionally added to end or start.
   *  the tree methods drawTextAlongLines, calculateTextAlongScreenLines and clearLineTextData work with a class state */
  void drawTextAlongLines();
  void clearLineTextData();

  void drawTextAlongOneLine(const QString& text, float bearing, const QPointF& textCoord,
                            bool bothVisible, int textLineLength);

  /* Find text position along a great circle route
   *  @param x,y resulting text position
   *  @param pos1,pos2 start and end coordinates of the line
   *  @param bearing text bearing at the returned position
   */
  bool findTextPos(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2,
                   int textWidth, int textHeight, int& x, int& y, float *bearing);

  /* Find text position along a great circle route
   *  @param x,y resulting text position
   *  @param pos1,pos2 start and end coordinates of the line
   *  @param bearing text bearing at the returned position
   *  @param distanceMeter distance between points
   */
  bool findTextPos(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2,
                   float distanceMeter, int textWidth, int textHeight, int& x, int& y, float *bearing);

  /* Find text position along a rhumb line route
   *  @param x,y resulting text position
   *  @param pos1,pos2 start and end coordinates of the line
   *  @param distanceMeter distance between points
   */
  bool findTextPosRhumb(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2,
                        float distanceMeter, int textWidth, int textHeight,
                        int& x, int& y);

  const QBitArray& getVisibleStartPoints() const
  {
    return visibleStartPoints;
  }

  const QList<QPointF>& getStartPoints() const
  {
    return startPoints;
  }

  void setArrowRight(const QString& value)
  {
    arrowRight = value;
  }

  void setArrowLeft(const QString& value)
  {
    arrowLeft = value;
  }

  void setDrawFast(bool value)
  {
    fast = value;
  }

  void setTextOnTopOfLine(bool value)
  {
    textOnTopOfLine = value;
  }

  void setLineWidth(float value)
  {
    lineWidth = value;
  }

  void setColors(const QVector<QColor>& value)
  {
    colors = value;
  }

private:
  QList<QPointF> textCoords;
  QList<float> textBearing;
  QStringList texts;
  QList<int> textLineLengths;

  // Collect start and end points of legs and visibility
  QList<QPointF> startPoints;
  QBitArray visibleStartPoints;

  /* Evaluate 50 text placement positions along line */
  const float FIND_TEXT_POS_STEP = 0.02f;

  /* Minimum pixel length for a line to get a text label */
  static Q_DECL_CONSTEXPR int MIN_LENGTH_FOR_TEXT = 80;

  bool fast = false, textOnTopOfLine = true;
  QPainter *painter = nullptr;
  CoordinateConverter *converter = nullptr;
  QString arrowRight, arrowLeft;
  float lineWidth = 10.f;
  QVector<QColor> colors;
  QVector<QColor> colors2;
};

#endif // LITTLENAVMAP_TEXTPLACEMENT_H
