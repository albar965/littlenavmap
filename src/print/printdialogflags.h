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

#ifndef LNM_PRINTDIALOGFLAGS_H
#define LNM_PRINTDIALOGFLAGS_H

#include "util/flags.h"

namespace prt {
enum PrintFlightplanOpt : quint32
{
  NONE = 0,
  DEPARTURE_OVERVIEW = 1 << 0,
  DEPARTURE_RUNWAYS = 1 << 1,
  DEPARTURE_RUNWAYS_SOFT = 1 << 2,
  DEPARTURE_RUNWAYS_DETAIL = 1 << 3,
  DEPARTURE_COM = 1 << 4,
  DEPARTURE_APPR = 1 << 5,
  DESTINATION_OVERVIEW = 1 << 6,
  DESTINATION_RUNWAYS = 1 << 7,
  DESTINATION_RUNWAYS_SOFT = 1 << 8,
  DESTINATION_RUNWAYS_DETAIL = 1 << 9,
  DESTINATION_COM = 1 << 10,
  DESTINATION_APPR = 1 << 11,
  FLIGHTPLAN = 1 << 12, /* Print flight plan */
  NEW_PAGE = 1 << 13, /* New page after each chapter */
  FUEL_REPORT = 1 << 14, /* Print fuel and aircraft performance report */
  HEADER = 1 << 15, /* Print a detailed header as on top of the flight plan dock */

  DEPARTURE_WEATHER = 1 << 16,
  DESTINATION_WEATHER = 1 << 17,

  DEPARTURE_ANY = DEPARTURE_OVERVIEW | DEPARTURE_RUNWAYS |
                  DEPARTURE_COM | DEPARTURE_APPR | DEPARTURE_WEATHER,
  DESTINATION_ANY = DESTINATION_OVERVIEW | DESTINATION_RUNWAYS |
                    DESTINATION_COM | DESTINATION_APPR | DESTINATION_WEATHER
};

ATOOLS_DECLARE_FLAGS_32(PrintFlightplanOpts, prt::PrintFlightplanOpt)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(prt::PrintFlightplanOpts)
}

#endif // LNM_PRINTDIALOGFLAGS_H
