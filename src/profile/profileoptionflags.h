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

#ifndef LNM_PROFILEOPTIONFLAGS_H
#define LNM_PROFILEOPTIONFLAGS_H

#include "util/flags.h"

namespace optsp {

/* Display options for elevation profile affecting line as well as top and left labels. Saved to settings. */
enum DisplayOptionProfile : quint32
{
  PROFILE_NONE = 0,

  /* General options */
  PROFILE_TOOLTIP = 1 << 1,
  PROFILE_HIGHLIGHT = 1 << 2,

  /* Labels */
  PROFILE_LABELS_ALT = 1 << 3,
  PROFILE_LABELS_DISTANCE = 1 << 4,
  PROFILE_LABELS_MAG_COURSE = 1 << 5,
  PROFILE_LABELS_TRUE_COURSE = 1 << 6,
  PROFILE_LABELS_RELATED = 1 << 7,

  /* Elevation Profile */
  PROFILE_GROUND = 1 << 8,
  PROFILE_SAFE_ALTITUDE = 1 << 9,
  PROFILE_LEG_SAFE_ALTITUDE = 1 << 10,

  /* Flight plan line options */
  PROFILE_FP_DIST = 1 << 11,
  PROFILE_FP_MAG_COURSE = 1 << 12,
  PROFILE_FP_TRUE_COURSE = 1 << 13,
  PROFILE_FP_VERTICAL_ANGLE = 1 << 14,
  PROFILE_FP_ALT_RESTRICTION = 1 << 15,
  PROFILE_FP_SPEED_RESTRICTION = 1 << 16,
  PROFILE_FP_ALT_RESTRICTION_BLOCK = 1 << 17,

  /* User aircraft labels */
  PROFILE_AIRCRAFT_ACTUAL_ALTITUDE = 1 << 18,
  PROFILE_AIRCRAFT_INDICATED_ALTITUDE = 1 << 25,
  PROFILE_AIRCRAFT_VERT_SPEED = 1 << 19,
  PROFILE_AIRCRAFT_VERT_ANGLE_NEXT = 1 << 20,

  /* ui->labelProfileInfo in ProfileWidget::updateHeaderLabel() */
  PROFILE_HEADER_DIST_TIME_TO_DEST = 1 << 21, /* Destination: 82 NM (0 h 15 m). */
  PROFILE_HEADER_DIST_TIME_TO_TOD = 1 << 22, /* Top of Descent: 69 NM (0 h 11 m). */
  PROFILE_HEADER_DESCENT_PATH_DEVIATION = 1 << 23, /* Descent Path: Deviation: 1,153 ft, above */
  PROFILE_HEADER_DESCENT_PATH_ANGLE = 1 << 24, /* ... Angle and Speed: -3.4°, -2,102 fpm */

  // Next is 1 << 26

  /* All drawn in the top label */
  PROFILE_TOP_ANY = PROFILE_LABELS_DISTANCE | PROFILE_LABELS_MAG_COURSE | PROFILE_LABELS_TRUE_COURSE | PROFILE_LABELS_RELATED,

  /* All drawn in the header label */
  PROFILE_HEADER_ANY = PROFILE_HEADER_DIST_TIME_TO_DEST | PROFILE_HEADER_DIST_TIME_TO_TOD | PROFILE_HEADER_DESCENT_PATH_DEVIATION |
                       PROFILE_HEADER_DESCENT_PATH_ANGLE,

  /* All drawn along the flight plan line */
  PROFILE_FP_ANY = PROFILE_FP_DIST | PROFILE_FP_MAG_COURSE | PROFILE_FP_TRUE_COURSE | PROFILE_FP_VERTICAL_ANGLE,
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsProfile, optsp::DisplayOptionProfile)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsp::DisplayOptionsProfile)

/* All available options for loops */
extern const QList<optsp::DisplayOptionProfile> ALL_OPTIONS;
extern const optsp::DisplayOptionsProfile DEFAULT_OPTIONS;
}

#endif // LNM_PROFILEOPTIONFLAGS_H
