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

#include "profileoptionflags.h"

namespace optsp {

/* All available options for loops */
const QList<optsp::DisplayOptionProfile> ALL_OPTIONS({optsp::PROFILE_LABELS_DISTANCE, optsp::PROFILE_LABELS_MAG_COURSE,
                                                      optsp::PROFILE_LABELS_TRUE_COURSE, optsp::PROFILE_LABELS_RELATED,
                                                      optsp::PROFILE_FP_DIST, optsp::PROFILE_FP_MAG_COURSE,
                                                      optsp::PROFILE_FP_TRUE_COURSE, optsp::PROFILE_FP_VERTICAL_ANGLE,
                                                      optsp::PROFILE_FP_ALT_RESTRICTION, optsp::PROFILE_FP_SPEED_RESTRICTION,
                                                      optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK, optsp::PROFILE_GROUND,
                                                      optsp::PROFILE_SAFE_ALTITUDE, optsp::PROFILE_LEG_SAFE_ALTITUDE,
                                                      optsp::PROFILE_LABELS_ALT, optsp::PROFILE_TOOLTIP,
                                                      optsp::PROFILE_HIGHLIGHT, optsp::PROFILE_AIRCRAFT_ACTUAL_ALTITUDE,
                                                      optsp::PROFILE_AIRCRAFT_INDICATED_ALTITUDE,
                                                      optsp::PROFILE_AIRCRAFT_VERT_SPEED, optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT,
                                                      optsp::PROFILE_HEADER_DIST_TIME_TO_DEST,
                                                      optsp::PROFILE_HEADER_DIST_TIME_TO_TOD,
                                                      optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION,
                                                      optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE

                                                     });

const optsp::DisplayOptionsProfile DEFAULT_OPTIONS = optsp::PROFILE_LABELS_DISTANCE | optsp::PROFILE_LABELS_RELATED |
                                                     optsp::PROFILE_FP_MAG_COURSE | optsp::PROFILE_FP_VERTICAL_ANGLE |
                                                     optsp::PROFILE_FP_ALT_RESTRICTION | optsp::PROFILE_FP_SPEED_RESTRICTION |
                                                     optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK | optsp::PROFILE_GROUND |
                                                     optsp::PROFILE_SAFE_ALTITUDE | optsp::PROFILE_LEG_SAFE_ALTITUDE |
                                                     optsp::PROFILE_LABELS_ALT | optsp::PROFILE_TOOLTIP | optsp::PROFILE_HIGHLIGHT |
                                                     optsp::PROFILE_AIRCRAFT_ACTUAL_ALTITUDE | optsp::PROFILE_AIRCRAFT_VERT_SPEED |
                                                     optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT |
                                                     optsp::PROFILE_HEADER_DIST_TIME_TO_DEST |
                                                     optsp::PROFILE_HEADER_DIST_TIME_TO_TOD |
                                                     optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION |
                                                     optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE;

}
