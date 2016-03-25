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

#ifndef MAPSCALE_H
#define MAPSCALE_H

#include <QVector>

#include <marble/ViewportParams.h>

namespace atools {
namespace geo {
class Pos;
}
}

class MapScale
{
public:
  MapScale();

  bool update(Marble::ViewportParams *viewportParams, double distance);

  float getPixelForMeter(float meter, float directionDeg = 45.f) const;
  float getPixelForFeet(int feet, float directionDeg = 45.f) const;
  int getPixelIntForMeter(float meter, float directionDeg = 45.f) const;
  int getPixelIntForFeet(int feet, float directionDeg = 45.f) const;

  float getDegreePerPixel(int px) const;

private:
  friend QDebug operator<<(QDebug out, const MapScale& scale);

  Marble::ViewportParams *viewport;
  double lastDistance = 0., lastCenterLonX = 0., lastCenterLatY = 0.;
  Marble::Projection lastProjection = Marble::VerticalPerspective; // never used

  /* Screen pixel per km for eight directions */
  QVector<double> scales;
};

#endif // MAPSCALE_H
