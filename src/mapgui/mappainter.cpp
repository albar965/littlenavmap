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

#include "mappainter.h"
#include "mapscale.h"
#include "symbolpainter.h"
#include "common/mapcolors.h"
#include "geo/pos.h"
#include "geo/calculations.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

const int CIRCLE_MIN_POINTS = 16;
const int CIRCLE_MAX_POINTS = 72;

using namespace Marble;
using namespace atools::geo;

MapPainter::MapPainter(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale,
                       bool verboseMsg)
  : CoordinateConverter(marbleWidget->viewport()), widget(marbleWidget), query(mapQuery), scale(mapScale),
    verbose(verboseMsg)
{
  symbolPainter = new SymbolPainter();
}

MapPainter::~MapPainter()
{
  delete symbolPainter;
}

void MapPainter::setRenderHints(GeoPainter *painter)
{
  if(widget->viewContext() == Marble::Still)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  }
  else if(widget->viewContext() == Marble::Animation)
  {
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::TextAntialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  }
}

void MapPainter::paintCircle(GeoPainter *painter, const Pos& pos, int radiusNm, bool fast,
                             int& xtext, int& ytext)
{
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

  Pos p1 = pos.endpoint(radiusMeter, 0);
  bool h1 = true, h2 = true;
  bool v1 = wToS(p1, x1, y1, &h1);

  GeoDataLinearRing ellipse;
  ellipse.setTessellate(true);
  for(int i = 0; i <= 360; i += step)
  {
    Pos p2 = pos.endpoint(radiusMeter, i).normalize();
    ellipse.append(GeoDataCoordinates(p2.getLonX(), p2.getLatY(), 0, GeoDataCoordinates::Degree));

    bool v2 = wToS(p2, x2, y2, &h2);

    if((v1 || v2) && !h1 && !h2)
      if(v1 && v2)
      {
        // Remember visible positions for the text
        xtexts.append((x1 + x2) / 2);
        ytexts.append((y1 + y2) / 2);
      }
    x1 = x2;
    y1 = y2;
    v1 = v2;
    h1 = h2;
  }

  painter->drawPolygon(ellipse);

  if(!xtexts.isEmpty() && !ytexts.isEmpty())
  {
    // Take the position at one third
    xtext = xtexts.at(xtexts.size() / 3);
    ytext = ytexts.at(ytexts.size() / 3);
  }
  else
  {
    xtext = -1;
    ytext = -1;
  }
}

bool MapPainter::findTextPos(const Pos& pos1, const Pos& pos2, GeoPainter *painter,
                             int w, int h, int& x, int& y, float *bearing)
{
  return findTextPos(pos1, pos2, painter, pos1.distanceMeterTo(pos2), w, h, x, y, bearing);
}

bool MapPainter::findTextPos(const Pos& pos1, const Pos& pos2, GeoPainter *painter, float distance,
                             int w, int h, int& x, int& y, float *bearing)
{
  int size = std::max(w, h);
  Pos center = pos1.interpolate(pos2, distance, 0.5);
  bool visible = wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - size / 2, y - size / 2, size, size)))
  {
    if(bearing != nullptr)
    {
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
      center = pos1.interpolate(pos2, distance, 0.5f - i);
      visible = wToS(center, x1, y1);
      if(visible && painter->window().contains(QRect(x1 - size / 2, y1 - size / 2, size, size)))
      {
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

      center = pos1.interpolate(pos2, distance, 0.5f + i);
      visible = wToS(center, x2, y2);
      if(visible && painter->window().contains(QRect(x2 - size / 2, y2 - size / 2, size, size)))
      {
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

bool MapPainter::findTextPosRhumb(const Pos& pos1, const Pos& pos2,
                                  GeoPainter *painter, float distance, int w, int h, int& x, int& y)
{
  using namespace atools::geo;

  Pos center = pos1.interpolateRhumb(pos2, distance, 0.5);
  bool visible = wToS(center, x, y);
  if(visible && painter->window().contains(QRect(x - w / 2, y - h / 2, w, h)))
    return true;
  else
    // Check for 50 positions along the line starting below and above the center position
    for(float i = 0.; i <= 0.5; i += FIND_TEXT_POS_STEP)
    {
    center = pos1.interpolateRhumb(pos2, distance, 0.5f - i);
    visible = wToS(center, x, y);
    if(visible && painter->window().contains(QRect(x - w / 2, y - h / 2, w, h)))
      return true;

    center = pos1.interpolateRhumb(pos2, distance, 0.5f + i);
    visible = wToS(center, x, y);
    if(visible && painter->window().contains(QRect(x - w / 2, y - h / 2, w, h)))
      return true;
    }
  return false;
}
