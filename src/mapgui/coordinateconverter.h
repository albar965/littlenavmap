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

#ifndef COORDINATECONVERTER_H
#define COORDINATECONVERTER_H

#include <QPoint>

namespace Marble {
class ViewportParams;
class GeoDataCoordinates;
}

namespace atools {
namespace geo {
class Pos;
}
}

class CoordinateConverter
{
public:
  CoordinateConverter(const Marble::ViewportParams *viewportParams);

  QPoint wToS(const Marble::GeoDataCoordinates& coords, bool *visible, bool *isHidden = nullptr) const;
  QPoint wToS(const atools::geo::Pos& coords, bool *visible = nullptr, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::Pos& coords, int& x, int& y, bool *isHidden = nullptr) const;

  bool wToS(const Marble::GeoDataCoordinates& coords, int& x, int& y, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::Pos& coords, float& x, float& y, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::Pos& coords, double& x, double& y, bool *isHidden = nullptr) const;

  bool sToW(int x, int y, Marble::GeoDataCoordinates& coords) const;
  atools::geo::Pos sToW(int x, int y) const;
  atools::geo::Pos sToW(const QPoint& point) const;

  bool isHidden(const atools::geo::Pos& coords) const;

private:
  const Marble::ViewportParams *viewport;
};

#endif // COORDINATECONVERTER_H
