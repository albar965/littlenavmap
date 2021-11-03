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
 */
enum ProgressConfId
{
  DATE_TIME_REAL,
  LOCAL_TIME_REAL,
  DATE_TIME,
  LOCAL_TIME,
  FLOWN,

  // "Destination" =================================
  DEST_DIST_TIME_ARR,
  DEST_FUEL,
  DEST_GROSS_WEIGHT,

  // "Top of descent" =================================
  TOD_DIST_TIME_ARR,
  TOD_FUEL,
  TOD_TO_DESTINATION,

  // "Top of climb" =================================
  TOC_DIST_TIME_ARR,
  TOC_FUEL,
  TOC_TO_DESTINATION,

  // "Next waypoint" =================================
  NEXT_LEG_TYPE,
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

  // "Aircraft" =================================
  AIRCRAFT_HEADING,
  AIRCRAFT_TRACK,
  AIRCRAFT_FUEL_FLOW,
  AIRCRAFT_FUEL,
  AIRCRAFT_GROSS_WEIGHT,
  AIRCRAFT_ENDURANCE,
  AIRCRAFT_ICE,

  // "Altitude" =================================
  ALT_INDICATED,
  ALT_ACTUAL,
  ALT_ABOVE_GROUND,
  ALT_GROUND_ELEVATION,

  // "Speed" =================================
  SPEED_INDICATED,
  SPEED_GROUND,
  SPEED_TRUE,
  SPEED_MACH,
  SPEED_VERTICAL,

  // "Descent Path" =================================
  DESCENT_DEVIATION,
  DESCENT_ANGLE_SPEED,

  // "Environment" =================================
  ENV_WIND_DIR_SPEED,
  ENV_TAT,
  ENV_SAT,
  ENV_ISA_DEV,
  ENV_SEA_LEVEL_PRESS,
  ENV_CONDITIONS,
  ENV_VISIBILITY,

  // "Position" =================================
  POS_COORDINATES,
};

}

#endif // LITTLENAVMAP_MAPHTMLINFOBUILDERFLAGS_H
