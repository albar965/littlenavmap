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

#ifndef LNM_ROUTELABELFLAGS_H
#define LNM_ROUTELABELFLAGS_H

#include "util/flags.h"

namespace routelabel {

/* Flag which defines header and footer content. */
enum LabelFlag : quint32
{
  HEADER_AIRPORTS = 1 << 0,
  HEADER_DEPARTURE = 1 << 1,
  HEADER_ARRIVAL = 1 << 2,
  HEADER_RUNWAY_TAKEOFF = 1 << 3,
  HEADER_RUNWAY_TAKEOFF_WIND = 1 << 4,
  HEADER_RUNWAY_LAND = 1 << 5,
  HEADER_RUNWAY_LAND_WIND = 1 << 6,
  HEADER_DISTTIME = 1 << 7,
  FOOTER_SELECTION = 1 << 8,
  FOOTER_ERROR = 1 << 9,
  FOOTER_ERROR_MINOR = 1 << 10,

  /* Default when loading config first time */
  LABEL_ALL = HEADER_AIRPORTS | HEADER_DEPARTURE | HEADER_ARRIVAL | HEADER_RUNWAY_TAKEOFF | HEADER_RUNWAY_TAKEOFF_WIND |
              HEADER_RUNWAY_LAND | HEADER_RUNWAY_LAND_WIND | HEADER_DISTTIME | FOOTER_SELECTION | FOOTER_ERROR | FOOTER_ERROR_MINOR
};

ATOOLS_DECLARE_FLAGS_32(LabelFlags, routelabel::LabelFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(routelabel::LabelFlags)
} // namespace label

#endif // LNM_ROUTELABELFLAGS_H
