/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include <QPoint>
#include <QPolygonF>
#include <QSize>
#include <QList>

class QPolygonF;

class QLineF;

class QRectF;
namespace Marble {

class GeoDataCoordinates;
class ViewportParams;
class GeoDataLineString;
class GeoDataLatLonBox;
}

namespace atools {
namespace geo {
class Pos;
class PosD;
class Line;
class Rect;
class LineString;
}
}

/*
 * Converter for screen and world coordinates.
 */
class CoordinateConverter
{
public:
  CoordinateConverter(const Marble::ViewportParams *viewportParams)
    : viewport(viewportParams)
  {
  }

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

  /* Make width or height 1 pixel if null. Zero width or heigh rectangles do not overlap with anything else */
  static QRectF correctBounding(const QRectF& boundingRect);
  static QRect correctBounding(const QRect& boundingRect);

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
  QPointF wToSF(const Marble::GeoDataCoordinates& coords,
                const QSize& size = DEFAULT_WTOS_SIZE, bool *visible = nullptr, bool *isHidden = nullptr) const;

  /* Reliable world to screen conversion. Polygons are already divided into segments.
   *  Result might contain duplicates for Mercator projection. */
  QList<QPolygonF> wToS(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2) const;
  QList<QPolygonF> wToS(const atools::geo::Line& line) const;
  QList<QPolygonF> wToS(const atools::geo::LineString& linestring) const;
  QList<QPolygonF> wToS(const Marble::GeoDataLineString& line) const;

  /*
   * Convert world to screen coordinates for GeoDataCoordinates
   * @param coords world coordinates
   * @param x resulting screen coordinate
   * @param y resulting screen coordinate
   * @param size estimated screen size for Mercator projection
   * @param isHidden if not null will indicate if object is hidden behind globe
   * @return true if coordinate is visible and not hidden
   */
  bool wToS(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const
  {
    return wToSInternal(coords, x, y, size, isHidden);
  }

  bool wToS(const Marble::GeoDataCoordinates& coords, int& x, int& y, const QSize& size = DEFAULT_WTOS_SIZE,
            bool *isHidden = nullptr) const;

  QPoint wToS(const atools::geo::Pos& coords, const QSize& size = DEFAULT_WTOS_SIZE, bool *visible = nullptr,
              bool *isHidden = nullptr) const;
  QPointF wToSF(const atools::geo::Pos& coords, const QSize& size = DEFAULT_WTOS_SIZE, bool *visible = nullptr,
                bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Pos& coords, int& x, int& y, const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::Pos& coords, float& x, float& y, const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::Pos& coords, double& x, double& y, const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;
  bool wToS(const atools::geo::PosD& coords, double& x, double& y, const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;

  bool wToS(const atools::geo::Line& coords, QLineF& line, const QSize& size = DEFAULT_WTOS_SIZE, bool *isHidden = nullptr) const;

  bool sToW(int x, int y, atools::geo::Pos& pos) const;

  /* Converte screen to world coordinates */
  atools::geo::Pos sToW(int x, int y) const;

  atools::geo::Pos sToW(const QPoint& point) const;
  atools::geo::Pos sToW(const QPointF& point) const;

  /* Get screen polygons for given line string. Polygons are cleaned up from duplicates and split at anti-meridian.
   * Free with releasePolygons() */
  const QList<QPolygonF *> createPolygons(const atools::geo::LineString& linestring, const QRectF& screenRect) const;

  void releasePolygons(const QList<QPolygonF *>& polygons) const
  {
    qDeleteAll(polygons);
  }

  const QList<QPolygonF *> createPolylines(const atools::geo::LineString& linestring, const QRectF& screenRect, bool splitLongLines) const
  {
    return createPolylinesInternal(linestring, screenRect, splitLongLines);
  }

  void releasePolylines(const QList<QPolygonF *>& polylines) const
  {
    qDeleteAll(polylines);
  }

  /*  Determines whether a geographical feature is big enough so that it should
   * represent a single point on the screen. Additionally checks if feature bounding rect overlaps viewport. */
  bool resolves(const Marble::GeoDataLatLonBox& box) const;
  bool resolves(const Marble::GeoDataCoordinates& coord1, const Marble::GeoDataCoordinates& coord2) const;
  bool resolves(const atools::geo::Rect& rect) const;
  bool resolves(const atools::geo::Line& line) const;

  bool wToSPoints(const atools::geo::Pos& pos, QList<double>& x, double& y, const QSize& size, bool *isHidden) const;
  bool wToSPoints(const atools::geo::Pos& pos, QList<float>& x, float& y, const QSize& size, bool *isHidden) const;
  bool wToSPoints(const Marble::GeoDataCoordinates& coords, QList<double>& x, double& y, const QSize& size, bool *isHidden) const;

private:
  bool wToSInternal(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size, bool *isHidden) const;

  const QList<QPolygonF *> createPolygonsInternal(const atools::geo::LineString& linestring, const QRectF& screenRect) const;
  const QList<QPolygonF *> createPolylinesInternal(const atools::geo::LineString& linestring, const QRectF& screenRect,
                                                     bool splitLongLines) const;

  const Marble::ViewportParams *viewport;
};

#endif // LITTLENAVMAP_COORDINATECONVERTER_H
