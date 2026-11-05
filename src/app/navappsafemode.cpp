/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "app/navappsafemode.h"

#include <QDebug>

namespace safemode {

QStringList safeModeToString(const SafeModeFlags& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(SAFE_MODE_NONE);
  ATOOLS_FLAGS_TO_STR(SAFE_MODE_ALL);
  ATOOLS_FLAGS_TO_STR(SAFE_MODE_LAYOUT);
  ATOOLS_FLAGS_TO_STR(SAFE_MODE_ROUTE_PERF_TRAIL);
  ATOOLS_FLAGS_TO_STR(SAFE_MODE_MAP_MARKERS);
  ATOOLS_FLAGS_TO_STR(SAFE_MODE_INFO_SEARCH);
  ATOOLS_FLAGS_TO_STR_END;
}

ATOOLS_DEFINE_DEBUG_OPERATORS_FOR_FLAGS(SafeModeFlags, SafeModeFlag, safeModeToString);

} // namespace safemode
