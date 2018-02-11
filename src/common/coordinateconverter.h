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

#ifndef LITTLENAVMAP_COORDINATECONVERTER_H
#define LITTLENAVMAP_COORDINATECONVERTER_H

#include <marble/GeoDataCoordinates.h>

#include <QPoint>
#include <QSize>

namespace Marble {
class ViewportParams;
}

namespace atools {
namespace geo {
class Pos;
class Line;
}
}

/*
 * Converter for screen and world coordinates.
 */
class CoordinateConverter
{
public:
  CoordinateConverter(const Marble::ViewportParams *viewportParams);
  ~CoordinateConverter();

  /* Default size (100x100) for the screen object. Needed to find the repeating pattern for the
   *  Mercator projection. */
  const static QSize DEFAULT_WTOS_SIZE;

  /*
   * Test if a coordinate is visible
   * @param coords world coordinates
   * @param size estimated screen size for Mercator projection
   * @param isHidden if not null will indicate if object is hidden behind globe
   * @return true if coordinate is visible and not hidden
   */
  bool isVisible(const atools::geo::Pos& coords, const QSize& size = DEFAULT_WTOS_SIZE,
                 bool *isHidden = nullptr) const;

  /* @return true if position is hidden behind globe */
  bool isHidden(const atools::geo::Pos& coords) const;

  /*
   * Convert world to screen coordinates
   * @param coords world coordinates
   * @param size estimated screen size for Mercator projection
   * @param isVisible if not null will indicate if object is visible on screen
   * @param isHidden if not null will indicate if object is hidden behind globe
   * @return screen point which is null if coordinate is not visible or hidden
   */
  QPoint wToS(const Marble::GeoDataCoordinates& coords,
              const QSize& size = DEFAULT_WTOS_SIZE, bool *visible = nullptr, bool *isHidden = nullptr) const;

  /*
   * Convert world to screen coordinates for GeoDataCoordinates
   * @param coords world coordinates
   * @param x resulting screen coordinate
   * @param y resulting screen coordinate
   * @param size estimated screen size for Mercator projection
   * @param isHidden if not null will indicate if object is hidden behind globe
   * @return true if coordinate is visible and not hidden
   */
  bool wToS(const Marble::GeoDataCoordinates& coords, double& x, double& y,
            const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;

  bool wToS(const Marble::GeoDataCoordinates& coords, int& x, int& y,
            const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;

  QPoint wToS(const atools::geo::Pos& coords, const QSize& size = DEFAULT_WTOS_SIZE,
              bool *visible = nullptr, bool *isHidden = nullptr) const;
  QPointF wToSF(const atools::geo::Pos& coords, const QSize& size = DEFAULT_WTOS_SIZE,
                bool *visible = nullptr, bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Pos& coords, int& x, int& y, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Pos& coords, float& x, float& y, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Pos& coords, double& x, double& y, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Line& coords, QLineF& line, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const;

  bool sToW(int x, int y, Marble::GeoDataCoordinates& coords) const;

  /* Converte screen to world coordinates */
  atools::geo::Pos sToW(int x, int y) const;

  atools::geo::Pos sToW(const QPoint& point) const;
  atools::geo::Pos sToW(const QPointF& point) const;

  /* Shortcuts for more readable code */
  static Q_DECL_CONSTEXPR Marble::GeoDataCoordinates::Unit DEG = Marble::GeoDataCoordinates::Degree;
  static Q_DECL_CONSTEXPR Marble::GeoDataCoordinates::BearingType INITBRG =
    Marble::GeoDataCoordinates::InitialBearing;
  static Q_DECL_CONSTEXPR Marble::GeoDataCoordinates::BearingType FINALBRG =
    Marble::GeoDataCoordinates::FinalBearing;

private:
  bool wToSInternal(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size,
                    bool *isHidden) const;

  const Marble::ViewportParams *viewport;

};

#endif // LITTLENAVMAP_COORDINATECONVERTER_H
