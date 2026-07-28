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

#ifndef LITTLENAVMAP_MAPINVALID_H
#define LITTLENAVMAP_MAPINVALID_H

#include <limits>

namespace map {

/* Value for invalid/not found/not applicable distances */
constexpr static float INVALID_COURSE_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_HEADING_VALUE = INVALID_COURSE_VALUE;
constexpr static float INVALID_ANGLE_VALUE = INVALID_COURSE_VALUE;
constexpr static float INVALID_DISTANCE_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_ALTITUDE_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_SPEED_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_TIME_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_WEIGHT_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_VOLUME_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_LON_LAT_VALUE = std::numeric_limits<float>::max();
constexpr static float INVALID_METAR_VALUE = std::numeric_limits<float>::max(); // Same as in metarparser.h
constexpr static int INVALID_INDEX_VALUE = std::numeric_limits<int>::max();
constexpr static float INVALID_MAGVAR = 9999.f;

} // namespace map

#endif // LITTLENAVMAP_MAPINVALID_H
