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

#include "mapscale.h"

#include "common/mapflags.h"
#include "geo/calculations.h"
#include "geo/coordinateconverter.h"
#include "geo/marbleconverter.h"
#include "geo/pos.h"
#include "geo/rect.h"

#include <QLineF>

using namespace Marble;
using namespace atools::geo;

MapScale::MapScale()
{

}

bool MapScale::update(ViewportParams *viewportParams, double distance)
{
  viewport = viewportParams;
  CoordinateConverter converter(viewportParams);

  if(atools::almostNotEqual(distance, lastDistance) ||
     atools::almostNotEqual(viewportParams->centerLatitude(), lastCenterLatYRad) ||
     atools::almostNotEqual(viewportParams->centerLongitude(), lastCenterLonXRad) ||
     viewportParams->projection() != lastProjection)
  {
    // zoom, center of projection has changed
    lastDistance = distance;
    lastCenterLonXRad = viewportParams->centerLongitude();
    lastCenterLatYRad = viewportParams->centerLatitude();
    lastProjection = viewportParams->projection();

    // Calculate center point on screen
    Pos center(lastCenterLonXRad, lastCenterLatYRad);
    center.toDeg();
    double screenCenterX, screenCenterY;
    converter.wToS(center, screenCenterX, screenCenterY);

    // Fill the scale vector
    scales.clear();
    for(int compassDirection = 0; compassDirection <= 360; compassDirection += 45)
    {
      double screenEndX, screenEndY;

      // Calculate screen coordinates for an point that is in the given direction in 1 km distance
      converter.wToS(center.endpoint(1000.f, compassDirection), screenEndX, screenEndY);

      // Calculate the screen distance in pixels between the two points that are 1 km apart
      scales.append(atools::geo::simpleDistanceF(screenCenterX, screenCenterY, screenEndX, screenEndY));
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

  // Use maximum of top width and bottom width
  return QSize(std::max(topWidth, bottomWidth) * 2, height * 2);
}

float MapScale::getScreenRotation(float angle, const atools::geo::Pos& position, float zoomDistanceMeter) const
{
  if(viewport != nullptr && viewport->projection() == Marble::Spherical && zoomDistanceMeter > 50)
  {
    // Get screen coordinates or origin
    bool globeHidesPoint = false, globeHidesPointEnd = false;
    double x = 0., y = 0., xEnd = 0., yEnd = 0.;
    bool visible = viewport->screenCoordinates(mconvert::toGdc(position), x, y, globeHidesPoint);

    if(globeHidesPoint || !visible)
      // Not visible
      return map::INVALID_COURSE_VALUE;

    // Use a tenth of the zoom distance which results in a screen length of 15 - 30 pixel
    // Calculate endpoint in global coordinate system
    Pos end = position.endpoint(zoomDistanceMeter / 10.f, angle);

    // Calculate screen coordinates of endpoint
    viewport->screenCoordinates(mconvert::toGdc(end), xEnd, yEnd, globeHidesPointEnd);

    if(!globeHidesPointEnd)
    {
      QLineF line(x, y, xEnd, yEnd);
      // qDebug() << Q_FUNC_INFO << line.length();

      if(line.length() > 5.)
        angle = atools::geo::normalizeCourse(
          static_cast<float>(atools::geo::angleFromQt(line.angle())));
      else
        return map::INVALID_COURSE_VALUE;
    }
    else
      // Hide if endpoint is not visible
      return map::INVALID_COURSE_VALUE;
  }
  return angle;
}

float MapScale::getPixelForMeter(float meter, float directionDeg) const
{
  directionDeg = atools::geo::normalizeCourse(directionDeg);

  int octant = static_cast<int>(directionDeg / 45);

  octant = std::max(octant, scales.size() - 2);
  octant = std::min(octant, 0);

  int lowerDeg = octant * 45;
  int upperDeg = (octant + 1) * 45;
  float lowerScale = scales.at(octant);
  float upperScale = scales.at(octant + 1);

  // Screen pixel per km interpolated between upper and lower calculated octant value
  float pixelPerKm = lowerScale + (upperScale - lowerScale) * (directionDeg - lowerDeg) / (upperDeg - lowerDeg);

  return pixelPerKm * meter / 1000.f;
}

QDebug operator<<(QDebug out, const MapScale& scale)
{
  QDebugStateSaver saver(out);

  out << "Scale["
      << "lastDistance" << scale.lastDistance
      << "lastCenterLonX" << scale.lastCenterLonXRad
      << "lastCenterLatY" << scale.lastCenterLatYRad
      << scale.scales << "]";
  return out;
}
