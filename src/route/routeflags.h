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

#ifndef LNM_ROUTEFLAGS_H
#define LNM_ROUTEFLAGS_H

#include "util/flags.h"

#include <limits>

namespace legindex {
/* Special index values for adding legs */
enum LegIndex
{
  PREPEND = -2,
  AUTO = -1,
  APPEND = std::numeric_limits<int>::max()
};

}

namespace rf {

/* Options for Route::adjustedToOptions */
enum RouteAdjustOption : quint32
{
  NONE = 0,
  SAVE_APPROACH_WP = 1 << 0, /* Save approach as waypoints and remove approach information. */
  SAVE_SID_WP = 1 << 1, /* Save SID as waypoints and remove procedure information. */
  SAVE_STAR_WP = 1 << 2, /* As above for STAR. */
  SAVE_AIRWAY_WP = 1 << 3, /* Remove airway information and save waypoints only. */
  REPLACE_CUSTOM_WP = 1 << 4, /* Replace custom approach with user defined waypoints */
  REMOVE_ALTERNATE = 1 << 5, /* Remove all alternate legs. */
  REMOVE_TRACKS = 1 << 6, /* Empty track name to force direct */
  FIX_CIRCLETOLAND = 1 << 7, /* Add a dummy best guess runway for circle-to-land approaches for X-Plane */

  ADD_PROC_ENTRY_EXIT_AIRWAY = 1 << 8, /* Add any removed procedure entry and exit points back
                                        * *if* they are attached to an airway. */
  ADD_PROC_ENTRY_EXIT = 1 << 9, /* Always add any removed procedure entry and exit points back */

  SAVE_MSFS = 1 << 10, /* Insert all SID/STAR waypoints and add procedure information for each waypoint.
                        * Add approach information to last waypoint/destination.
                        * Also adds airport information to waypoints from simulator database.  */
  SAVE_LNMPLN = 1 << 11, /* Ignore menu options to save procedures or airways as waypoints */

  ISG_USER_WP_NAMES = 1 << 12, /* Modified user waypoint names for ISG */

  XPLANE_REPLACE_AIRPORT_IDENTS = 1 << 13, /* Replace airport idents in waypoint list only
                                            * with either ICAO, FAA, IATA or local ones */

  REMOVE_RUNWAY_PROC = 1 << 14, /* Remove runway extensions and their waypoints */
  REMOVE_MISSED = 1 << 15, /* Remove missed approach */
  REMOVE_CUSTOM_DEPART = 1 << 16, /* Remove first runway waypoint from selected departure runway for X-Plane */
  SAVE_KEEP_INVALID_START = 1 << 17, /* Do not remove parking/start position names which were not resolved against database */
  RESTRICTIONS_TO_REMARKS = 1 << 18, /* Put procedure restrictions into the remarks section when converting procedures to waypoints */
  REMOVE_ALL_PROCEDURES = 1 << 19, /* Remove all procedures and do not save their waypoints */

  SAVE_MSFS_2024 = 1 << 20,

  /* Flag combinations =========================================================================== */

  /* Export adjust options for most export formats */
  DEFAULT_OPTS = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | rf::ADD_PROC_ENTRY_EXIT_AIRWAY,

  /* Do not add waypoint and procedure entry or exit back. */
  DEFAULT_OPTS_PROC = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS,

  /* Always add entry and exit waypoints for procedures. This is used for formats not supporting procedures. */
  DEFAULT_OPTS_NO_PROC = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | ADD_PROC_ENTRY_EXIT,

  /* Export adjust options for Garmin GFP formats */
  DEFAULT_OPTS_GFP = rf::DEFAULT_OPTS | rf::REMOVE_RUNWAY_PROC,
  DEFAULT_OPTS_GFP_NO_PROC = rf::DEFAULT_OPTS_NO_PROC | rf::REMOVE_RUNWAY_PROC,

  DEFAULT_OPTS_FLP = rf::REPLACE_CUSTOM_WP | rf::REMOVE_TRACKS | rf::ADD_PROC_ENTRY_EXIT_AIRWAY | rf::REMOVE_RUNWAY_PROC,
  DEFAULT_OPTS_FLP_MSFS_CRJ = rf::REPLACE_CUSTOM_WP | rf::REMOVE_TRACKS | ADD_PROC_ENTRY_EXIT | SAVE_AIRWAY_WP |
                              rf::REMOVE_RUNWAY_PROC,

  /* LNMPLN save and load format. Does not mangle anything. */
  DEFAULT_OPTS_LNMPLN = rf::ADD_PROC_ENTRY_EXIT_AIRWAY | rf::SAVE_LNMPLN | rf::SAVE_KEEP_INVALID_START,

  /* LNMPLN save selected legs as plan. */
  DEFAULT_OPTS_LNMPLN_SAVE_SELECTED = rf::DEFAULT_OPTS_LNMPLN | rf::REMOVE_ALTERNATE | rf::SAVE_KEEP_INVALID_START,

  /* Option for RouteStringWriter used to generate a route description */
  DEFAULT_OPTS_ROUTESTRING = rf::ADD_PROC_ENTRY_EXIT_AIRWAY,

  /* Microsoft Flight Simulator 2020 */
  DEFAULT_OPTS_MSFS = rf::DEFAULT_OPTS | rf::SAVE_MSFS | rf::REMOVE_CUSTOM_DEPART,

  /* Microsoft Flight Simulator 2020 */
  DEFAULT_OPTS_MSFS_2024 = rf::DEFAULT_OPTS | rf::SAVE_MSFS_2024 | rf::REMOVE_CUSTOM_DEPART,

  /* FSX and P3D - remove first point of departure */
  DEFAULT_OPTS_FSX_P3D = rf::DEFAULT_OPTS_NO_PROC | rf::REMOVE_CUSTOM_DEPART,

  /* PMS50 GTN750 */
  DEFAULT_OPTS_PMS50 = rf::DEFAULT_OPTS_MSFS,

  /* Export adjust options for XP11 and old FMS3 */
  DEFAULT_OPTS_FMS3 = rf::DEFAULT_OPTS_NO_PROC,
  DEFAULT_OPTS_FMS11 = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | rf::FIX_CIRCLETOLAND |
                       rf::XPLANE_REPLACE_AIRPORT_IDENTS | rf::REMOVE_CUSTOM_DEPART,
  DEFAULT_OPTS_CIVA_FMS = rf::DEFAULT_OPTS_NO_PROC | rf::REMOVE_ALTERNATE | rf::REMOVE_RUNWAY_PROC | rf::REMOVE_MISSED,

  /* Remove all procedures for FlightFactor T7 but keep entry and exit points */
  DEFAULT_OPTS_FMS_T7 = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | rf::FIX_CIRCLETOLAND |
                        rf::XPLANE_REPLACE_AIRPORT_IDENTS | rf::REMOVE_CUSTOM_DEPART | rf::REMOVE_ALL_PROCEDURES |
                        rf::ADD_PROC_ENTRY_EXIT,

  /* Garmin GPX */
  DEFAULT_OPTS_GPX = rf::DEFAULT_OPTS | rf::SAVE_AIRWAY_WP | rf::SAVE_SID_WP | rf::SAVE_STAR_WP | rf::SAVE_APPROACH_WP
};

ATOOLS_DECLARE_FLAGS_32(RouteAdjustOptions, rf::RouteAdjustOption)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(rf::RouteAdjustOptions)

}

#endif // LNM_ROUTEFLAGS_H
