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

#ifndef LNM_GEO_MARBLECONVERTER_H
#define LNM_GEO_MARBLECONVERTER_H

#include "marble/GeoDataCoordinates.h"

namespace atools {
namespace geo {

class PosD;
class Line;
class LineString;
class Pos;
class Rect;
}
}

namespace Marble {
class GeoDataLinearRing;
class GeoDataLineString;
class GeoDataLatLonBox;
}

/* Conversion from the cumbersome Marble coordinates to atools coordinates and back.
 * Do not transfer or convert altitude. Uses always degrees and tesselation enabled.  */
namespace mconvert {

/* Point coordinates */
Marble::GeoDataCoordinates toGdc(const atools::geo::Pos& coords);
Marble::GeoDataCoordinates toGdc(const atools::geo::PosD& coords);
Marble::GeoDataCoordinates toGdc(float lonX, float latY);
Marble::GeoDataCoordinates toGdc(double lonX, double latY);

/* Rectangle */
Marble::GeoDataLatLonBox toGdc(const atools::geo::Rect& coords);

/* Line */
Marble::GeoDataLineString toGdcStr(const atools::geo::Line& coords);
Marble::GeoDataLineString toGdcStr(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2);

/* Linestring */
Marble::GeoDataLineString toGdcStr(const atools::geo::LineString& coords);
Marble::GeoDataLinearRing toGdcRing(const atools::geo::LineString& linestring);

atools::geo::Rect fromGdc(const Marble::GeoDataLatLonBox& coords);
atools::geo::Pos fromGdc(const Marble::GeoDataCoordinates& coords);
const atools::geo::LineString fromGdcStr(const Marble::GeoDataLineString& coords);

/* Shortcut for more readable code */
constexpr Marble::GeoDataCoordinates::Unit DEG = Marble::GeoDataCoordinates::Degree;

} // namespace marbleconverter

#endif // LNM_GEO_MARBLECONVERTER_H
