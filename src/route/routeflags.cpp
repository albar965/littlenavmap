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

#include "route/routeflags.h"

#include <QDebug>

namespace rf {

QStringList mapDisplayTypeToString(const rf::RouteAdjustOptions& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(NONE);
  ATOOLS_FLAGS_TO_STR(SAVE_APPROACH_WP);
  ATOOLS_FLAGS_TO_STR(SAVE_SID_WP);
  ATOOLS_FLAGS_TO_STR(SAVE_STAR_WP);
  ATOOLS_FLAGS_TO_STR(SAVE_AIRWAY_WP);
  ATOOLS_FLAGS_TO_STR(REPLACE_CUSTOM_WP);
  ATOOLS_FLAGS_TO_STR(REMOVE_ALTERNATE);
  ATOOLS_FLAGS_TO_STR(REMOVE_TRACKS);
  ATOOLS_FLAGS_TO_STR(FIX_CIRCLETOLAND);
  ATOOLS_FLAGS_TO_STR(ADD_PROC_ENTRY_EXIT_AIRWAY);
  ATOOLS_FLAGS_TO_STR(ADD_PROC_ENTRY_EXIT);
  ATOOLS_FLAGS_TO_STR(SAVE_MSFS);
  ATOOLS_FLAGS_TO_STR(SAVE_LNMPLN);
  ATOOLS_FLAGS_TO_STR(ISG_USER_WP_NAMES);
  ATOOLS_FLAGS_TO_STR(XPLANE_REPLACE_AIRPORT_IDENTS);
  ATOOLS_FLAGS_TO_STR(REMOVE_RUNWAY_PROC);
  ATOOLS_FLAGS_TO_STR(REMOVE_MISSED);
  ATOOLS_FLAGS_TO_STR(REMOVE_CUSTOM_DEPART);
  ATOOLS_FLAGS_TO_STR(SAVE_KEEP_INVALID_START);
  ATOOLS_FLAGS_TO_STR(RESTRICTIONS_TO_REMARKS);
  ATOOLS_FLAGS_TO_STR(REMOVE_ALL_PROCEDURES);
  ATOOLS_FLAGS_TO_STR(SAVE_MSFS_2024);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, rf::RouteAdjustOption type)
{
  out << rf::RouteAdjustOptions(type);
  return out;
}

QDebug operator<<(QDebug out, const rf::RouteAdjustOptions& type)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << mapDisplayTypeToString(type).join("|");
  return out;
}

} // namespace rf
