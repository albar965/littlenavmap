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

class MapScale
{
private:
  static Q_DECL_CONSTEXPR float DEFAULT_ANGLE = 45.f;

public:
  MapScale();

  bool update(Marble::ViewportParams *viewportParams, double distance);

  float getPixelForMeter(float meter, float directionDeg = DEFAULT_ANGLE) const;
  float getPixelForFeet(int feet, float directionDeg = DEFAULT_ANGLE) const;
  int getPixelIntForMeter(float meter, float directionDeg = DEFAULT_ANGLE) const;
  int getPixelIntForFeet(int feet, float directionDeg = DEFAULT_ANGLE) const;

  float getDegreePerPixel(int px) const;
  QSize getScreeenSizeForRect(const atools::geo::Rect& rect) const;

  bool isValid() const
  {
    return !scales.isEmpty();
  }

private:
  friend QDebug operator<<(QDebug out, const MapScale& scale);

  Marble::ViewportParams *viewport;
  double lastDistance = 0., lastCenterLonX = 0., lastCenterLatY = 0.;
  Marble::Projection lastProjection = Marble::VerticalPerspective; // never used

  /* Screen pixel per km for eight directions */
  QVector<double> scales;
};

#endif // LITTLENAVMAP_MAPSCALE_H
