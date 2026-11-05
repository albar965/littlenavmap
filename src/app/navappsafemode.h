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

#ifndef LNM_GUI_APPLICATION_SAFE_MODE_H
#define LNM_GUI_APPLICATION_SAFE_MODE_H

#include "util/flags.h"

namespace safemode {

/* Defines which features should be skipped loading after a detected crash */
enum SafeModeFlag
{
  SAFE_MODE_NONE = 0,
  SAFE_MODE_ALL = 1 << 0, /* Reset all settings in .ini except databases */
  SAFE_MODE_LAYOUT = 1 << 1, /* Window layout back to default */
  SAFE_MODE_ROUTE_PERF_TRAIL = 1 << 2, /* Flight plan, aircraft perf, KML and trail */
  SAFE_MODE_MAP_MARKERS = 1 << 3, /* Map settings and markers */
  SAFE_MODE_INFO_SEARCH = 1 << 4 /* Information and search parameters */
};

ATOOLS_DECLARE_FLAGS_32(SafeModeFlags, safemode::SafeModeFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(SafeModeFlags)
ATOOLS_DECLARE_DEBUG_OPERATORS_FOR_FLAGS(SafeModeFlags, safemode::SafeModeFlag)

} // namespace safemode

#endif // LNM_GUI_APPLICATION_SAFE_MODE_H
