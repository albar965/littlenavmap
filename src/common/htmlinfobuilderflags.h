/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPHTMLINFOBUILDERFLAGS_H
#define LITTLENAVMAP_MAPHTMLINFOBUILDERFLAGS_H

namespace pid {
/*
 * Id number values are used to save settings for data field visibility in aircraft progress tab.
 *
 * ALLIDS in aircraftprogressconfig.cpp has to be updated together with this enumeration.
 * Maximum is HtmlBuilder::MAX_ID (512)
 */
enum ProgressConfId
{
  /* Leave gaps in number ranges for future extensions */
  DATE_TIME_REAL = 10,
  LOCAL_TIME_REAL,
  DATE_TIME,
  LOCAL_TIME,
  FLOWN,

  // "Destination" =================================
  DEST_DIST_TIME_ARR = 20,
  DEST_FUEL,
  DEST_GROSS_WEIGHT,

  // "Top of descent" =================================
  TOD_DIST_TIME_ARR = 40,
  TOD_FUEL,
  TOD_TO_DESTINATION,

  // "Top of climb" =================================
  TOC_DIST_TIME_ARR = 60,
  TOC_FUEL,
  TOC_FROM_DESTINATION,

  // "Next waypoint" =================================
  NEXT_LEG_TYPE = 80,
  NEXT_INSTRUCTIONS,
  NEXT_RELATED,
  NEXT_RESTRICTION,
  NEXT_DIST_TIME_ARR,
  NEXT_ALTITUDE,
  NEXT_FUEL,
  NEXT_COURSE_TO_WP,
  NEXT_LEG_COURSE,
  NEXT_HEADING,
  NEXT_CROSS_TRACK_DIST,
  NEXT_REMARKS,
  NEXT_COURSE_FROM_VOR,
  NEXT_COURSE_TO_VOR,

  // "Aircraft" =================================
  AIRCRAFT_HEADING = 100,
  AIRCRAFT_TRACK,
  AIRCRAFT_FUEL_FLOW,
  AIRCRAFT_FUEL,
  AIRCRAFT_GROSS_WEIGHT,
  AIRCRAFT_ENDURANCE,
  AIRCRAFT_ICE,

  // "Altitude" =================================
  ALT_INDICATED = 120,
  ALT_ACTUAL,
  ALT_ABOVE_GROUND,
  ALT_GROUND_ELEVATION,
  ALT_AUTOPILOT_ALT,

  // Other units than default
  ALT_INDICATED_OTHER,
  ALT_ACTUAL_OTHER,

  // "Speed" =================================
  SPEED_INDICATED = 140,
  SPEED_GROUND,
  SPEED_TRUE,
  SPEED_MACH,
  SPEED_VERTICAL,

  // Other units than default
  SPEED_INDICATED_OTHER,
  SPEED_GROUND_OTHER,
  SPEED_VERTICAL_OTHER,

  // "Descent Path" =================================
  DESCENT_DEVIATION = 160,
  DESCENT_ANGLE_SPEED,
  DESCENT_VERT_ANGLE_NEXT,

  // "Environment" =================================
  ENV_WIND_DIR_SPEED = 180,
  ENV_TAT,
  ENV_SAT,
  ENV_ISA_DEV,
  ENV_SEA_LEVEL_PRESS,
  ENV_CONDITIONS,
  ENV_VISIBILITY,
  ENV_DENSITY_ALTITUDE,

  // "Position" =================================
  POS_COORDINATES = 200,

  LAST = POS_COORDINATES, /* Last field to determine bit array size */

  /* Maximum is HtmlBuilder::MAX_ID (512) ==================================== */
};

}

#endif // LITTLENAVMAP_MAPHTMLINFOBUILDERFLAGS_H
