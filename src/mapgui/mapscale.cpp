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

#include "common/coordinateconverter.h"
#include "mapscale.h"
#include "geo/pos.h"
#include "geo/calculations.h"

#include <marble/ViewportParams.h>

#include <geo/rect.h>

using namespace Marble;
using namespace atools::geo;

MapScale::MapScale()
{

}

bool MapScale::update(ViewportParams *viewportParams, double distance)
{
  viewport = viewportParams;
  CoordinateConverter converter(viewportParams);

  if(distance != lastDistance ||
     viewport->centerLatitude() != lastCenterLatY || viewport->centerLongitude() !=
     lastCenterLonX || viewport->projection() != lastProjection)
  {
    lastDistance = distance;
    lastCenterLonX = viewportParams->centerLongitude();
    lastCenterLatY = viewportParams->centerLatitude();
    lastProjection = viewport->projection();

    scales.clear();
    Pos center(lastCenterLonX, lastCenterLatY);
    center.toDeg();
    double screenCenterX, screenCenterY;
    converter.wToS(center, screenCenterX, screenCenterY);

    for(int i = 0; i <= 360; i += 45)
    {
      double screenEndX, screenEndY;
      converter.wToS(center.endpoint(1000., i).normalize(), screenEndX, screenEndY);

      scales.append(sqrt((screenCenterX - screenEndX) * (screenCenterX - screenEndX) +
                         (screenCenterY - screenEndY) * (screenCenterY - screenEndY)));
    }
    return true;
  }
  return false;
}

QSize MapScale::getScreeenSizeForRect(const atools::geo::Rect& rect) const
{
  int topWidth = getPixelIntForMeter(rect.getTopLeft().distanceMeterTo(rect.getTopRight()), 90.f);
  int bottomWidth = getPixelIntForMeter(rect.getBottomLeft().distanceMeterTo(rect.getBottomRight()), 90.f);
  int height = getPixelIntForMeter(rect.getBottomCenter().distanceMeterTo(rect.getTopCenter()), 180);
  return QSize(std::max(topWidth, bottomWidth), height);
}

float MapScale::getDegreePerPixel(int px) const
{
  return static_cast<float>(toDegree(viewport->angularResolution()) * static_cast<double>(px));
}

int MapScale::getPixelIntForMeter(float meter, float directionDeg) const
{
  return static_cast<int>(std::round(getPixelForMeter(meter, directionDeg)));
}

int MapScale::getPixelIntForFeet(int feet, float directionDeg) const
{
  return getPixelIntForMeter(atools::geo::feetToMeter(static_cast<float>(feet)), directionDeg);
}

float MapScale::getPixelForMeter(float meter, float directionDeg) const
{
  while(directionDeg >= 360.f)
    directionDeg -= 360.f;
  while(directionDeg < 0.f)
    directionDeg += 360.f;

  int octant = static_cast<int>(directionDeg / 45);
  int lowerDeg = octant * 45;
  int upperDeg = (octant + 1) * 45;
  double lowerScale = scales.at(octant);
  double upperScale = scales.at(octant + 1);

  // Screen pixel per km
  double interpolatedScale = lowerScale + (upperScale - lowerScale) *
                             (directionDeg - lowerDeg) /
                             (upperDeg - lowerDeg);
  return static_cast<float>(interpolatedScale * static_cast<double>(meter) / 1000.);
}

float MapScale::getPixelForFeet(int feet, float directionDeg) const
{
  return getPixelForMeter(atools::geo::feetToMeter(static_cast<float>(feet)), directionDeg);
}

QDebug operator<<(QDebug out, const MapScale& scale)
{
  QDebugStateSaver saver(out);

  out << "Scale["
  << "lastDistance" << scale.lastDistance
  << "lastCenterLonX" << scale.lastCenterLonX
  << "lastCenterLatY" << scale.lastCenterLatY
  << scale.scales << "]";
  return out;
}
