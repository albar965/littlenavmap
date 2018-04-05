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

#ifndef LITTLENAVMAP_MAPSCALE_H
#define LITTLENAVMAP_MAPSCALE_H

#include <QVector>
#include <QSize>

 #include <marble/ViewportParams.h>

namespace atools {
namespace geo {
class Pos;
class Rect;
}
}

/*
 * Provides functionality to approximate a screen distance in pixel for the map.
 * Updates internal values for eight compass directions on zoom distance change
 * to avoid complex calculations during painting.
 */
class MapScale
{
public:
  MapScale();

  /* Update internal values */
  bool update(Marble::ViewportParams *viewport, double distance);

  /* Use if angle is unknown */
  static Q_DECL_CONSTEXPR float DEFAULT_ANGLE = 45.f;

  /* Convert length unit into screen pixels for the given direction. This approximation loses accuracy with
   *  higher zoom distances. */
  float getPixelForMeter(float meter, float directionDeg = DEFAULT_ANGLE) const;
  float getPixelForFeet(int feet, float directionDeg = DEFAULT_ANGLE) const;
  float getPixelForNm(float nm, float directionDeg = DEFAULT_ANGLE) const;

  int getPixelIntForMeter(float meter, float directionDeg = DEFAULT_ANGLE) const;
  int getPixelIntForFeet(int feet, float directionDeg = DEFAULT_ANGLE) const;
  int getPixelIntForNm(float nm, float directionDeg = DEFAULT_ANGLE) const;

  /*Get an approximation in screen pixes for the given coordinate rectangle */
  QSize getScreeenSizeForRect(const atools::geo::Rect& rect) const;

  /* @return true if initialized */
  bool isValid() const
  {
    return !scales.isEmpty();
  }

private:
  friend QDebug operator<<(QDebug out, const MapScale& scale);

  /* Store some values that can be used to check if the view has changed */
  Marble::Projection lastProjection = Marble::VerticalPerspective; // VerticalPerspective is never used
  double lastDistance = 0., lastCenterLonXRad = 0., lastCenterLatYRad = 0.;

  /* Screen pixel per km for eight directions */
  QVector<float> scales;
};

#endif // LITTLENAVMAP_MAPSCALE_H
