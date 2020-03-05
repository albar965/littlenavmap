/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "common/coordinateconverter.h"
#include "geo/pos.h"
#include "geo/calculations.h"
#include "geo/rect.h"
#include "common/maptypes.h"

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

  if(distance != lastDistance ||
     viewportParams->centerLatitude() != lastCenterLatYRad ||
     viewportParams->centerLongitude() != lastCenterLonXRad ||
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
      converter.wToS(center.endpoint(1000.f, compassDirection).normalize(), screenEndX, screenEndY);

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
    Marble::GeoDataCoordinates coords(position.getLonX(), position.getLatY(), 0., GeoDataCoordinates::Degree);
    bool globeHidesPoint = false, globeHidesPointEnd = false;
    double x = 0., y = 0., xEnd = 0., yEnd = 0.;
    bool visible = viewport->screenCoordinates(coords, x, y, globeHidesPoint);

    if(globeHidesPoint || !visible)
      // Not visible
      return map::INVALID_COURSE_VALUE;

    // Use a tenth of the zoom distance which results in a screen length of 15 - 30 pixel
    // Calculate endpoint in global coordinate system
    Pos end = position.endpoint(zoomDistanceMeter / 10.f, angle);
    end.normalize();

    // Calculate screen coordinates of endpoint
    Marble::GeoDataCoordinates endcoords(end.getLonX(), end.getLatY(), 0., GeoDataCoordinates::Degree);
    viewport->screenCoordinates(endcoords, xEnd, yEnd, globeHidesPointEnd);

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

int MapScale::getPixelIntForMeter(float meter, float directionDeg) const
{
  return static_cast<int>(std::round(getPixelForMeter(meter, directionDeg)));
}

int MapScale::getPixelIntForFeet(int feet, float directionDeg) const
{
  return getPixelIntForMeter(atools::geo::feetToMeter(static_cast<float>(feet)), directionDeg);
}

int MapScale::getPixelIntForNm(float nm, float directionDeg) const
{
  return getPixelIntForMeter(atools::geo::nmToMeter(nm), directionDeg);
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
  float pixelPerKm = lowerScale + (upperScale - lowerScale) * (directionDeg - lowerDeg) /
                     (upperDeg - lowerDeg);

  return pixelPerKm * meter / 1000.f;
}

float MapScale::getPixelForFeet(int feet, float directionDeg) const
{
  return getPixelForMeter(atools::geo::feetToMeter(static_cast<float>(feet)), directionDeg);
}

float MapScale::getPixelForNm(float nm, float directionDeg) const
{
  return getPixelForMeter(atools::geo::nmToMeter(nm), directionDeg);
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
