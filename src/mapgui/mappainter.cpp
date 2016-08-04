/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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
#include "common/symbolpainter.h"
#include "geo/calculations.h"
#include "mapgui/mapwidget.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPainter::MapPainter(MapWidget *parentMapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : CoordinateConverter(parentMapWidget->viewport()), mapWidget(parentMapWidget), query(mapQuery),
    scale(mapScale)
{
  symbolPainter = new SymbolPainter();
}

MapPainter::~MapPainter()
{
  delete symbolPainter;
}

void MapPainter::setRenderHints(GeoPainter *painter)
{
  if(mapWidget->viewContext() == Marble::Still)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  }
  else if(mapWidget->viewContext() == Marble::Animation)
  {
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::TextAntialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  }
}

void MapPainter::paintCircle(GeoPainter *painter, const Pos& centerPos, int radiusNm, bool fast,
                             int& xtext, int& ytext)
{
  QRect vpRect(painter->viewport());

  // Calculate the number of points to use depending in screen resolution
  int pixel = scale->getPixelIntForMeter(nmToMeter(radiusNm));
  int numPoints = std::min(std::max(pixel / (fast ? 20 : 2), CIRCLE_MIN_POINTS), CIRCLE_MAX_POINTS);

  int radiusMeter = nmToMeter(radiusNm);

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

    QRect r(QPoint(x1, y1), QPoint(x2, y2));
    // Current line is visible (most likely)
    bool nowVisible = r.normalized().intersects(vpRect);

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

bool MapPainter::findTextPos(const Pos& pos1, const Pos& pos2, GeoPainter *painter,
                             int textWidth, int textHeight, int& x, int& y, float *bearing)
{
  return findTextPos(pos1, pos2, painter, pos1.distanceMeterTo(pos2), textWidth, textHeight, x, y, bearing);
}

bool MapPainter::findTextPos(const Pos& pos1, const Pos& pos2, GeoPainter *painter, float distanceMeter,
                             int textWidth, int textHeight, int& x, int& y, float *bearing)
{
  int size = std::max(textWidth, textHeight);
  Pos center = pos1.interpolate(pos2, distanceMeter, 0.5);
  bool visible = wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - size / 2, y - size / 2, size, size)))
  {
    // Center point is already visible
    if(bearing != nullptr)
    {
      // Calculate bearing at the center point
      float xtp1, ytp1, xtp2, ytp2;
      wToS(pos1.interpolate(pos2, 0.5f - FIND_TEXT_POS_STEP), xtp1, ytp1);
      wToS(pos1.interpolate(pos2, 0.5f + FIND_TEXT_POS_STEP), xtp2, ytp2);
      QLineF lineF(xtp1, ytp1, xtp2, ytp2);

      *bearing = static_cast<float>(normalizeCourse(-lineF.angle() + 270.f));
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
      visible = wToS(center, x1, y1);
      if(visible && painter->window().contains(QRect(x1 - size / 2, y1 - size / 2, size, size)))
      {
        // Point is visible - return
        if(bearing != nullptr)
        {
          float xtp1, ytp1, xtp2, ytp2;
          wToS(pos1.interpolate(pos2, 0.5f - i - FIND_TEXT_POS_STEP), xtp1, ytp1);
          wToS(pos1.interpolate(pos2, 0.5f - i + FIND_TEXT_POS_STEP), xtp2, ytp2);
          QLineF lineF(xtp1, ytp1, xtp2, ytp2);
          *bearing = static_cast<float>(normalizeCourse(-lineF.angle() + 270.f));
        }
        x = x1;
        y = y1;
        return true;
      }

      center = pos1.interpolate(pos2, distanceMeter, 0.5f + i);
      visible = wToS(center, x2, y2);
      if(visible && painter->window().contains(QRect(x2 - size / 2, y2 - size / 2, size, size)))
      {
        // Point is visible - return
        if(bearing != nullptr)
        {
          float xtp1, ytp1, xtp2, ytp2;
          wToS(pos1.interpolate(pos2, 0.5f + i - FIND_TEXT_POS_STEP), xtp1, ytp1);
          wToS(pos1.interpolate(pos2, 0.5f + i + FIND_TEXT_POS_STEP), xtp2, ytp2);
          QLineF lineF(xtp1, ytp1, xtp2, ytp2);
          *bearing = static_cast<float>(normalizeCourse(-lineF.angle() + 270.f));
        }
        x = x2;
        y = y2;
        return true;
      }
    }
  }
  return false;
}

bool MapPainter::findTextPosRhumb(const Pos& pos1, const Pos& pos2, GeoPainter *painter,
                                  float distanceMeter, int textWidth, int textHeight, int& x, int& y)
{
  Pos center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5);
  bool visible = wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
    return true;
  else
  {
    // Check for 50 positions along the line starting below and above the center position
    for(float i = 0.; i <= 0.5; i += FIND_TEXT_POS_STEP)
    {
      center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5f - i);
      visible = wToS(center, x, y);
      if(visible &&
         painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
        return true;

      center = pos1.interpolateRhumb(pos2, distanceMeter, 0.5f + i);
      visible = wToS(center, x, y);
      if(visible &&
         painter->window().contains(QRect(x - textWidth / 2, y - textHeight / 2, textWidth, textHeight)))
        return true;
    }
  }
  return false;
}
