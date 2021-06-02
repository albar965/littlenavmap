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

#ifndef LNM_ROUTEFLAGS_H
#define LNM_ROUTEFLAGS_H

#include <QFlags>

namespace rf {

/* Options for Route::adjustedToOptions */
enum RouteAdjustOption
{
  NONE = 0,
  SAVE_APPROACH_WP = 1 << 0, /* Save approach as waypoints and remove approach information. */
  SAVE_SIDSTAR_WP = 1 << 1, /* Save SID and STAR as waypoints and remove procedure information. */
  SAVE_AIRWAY_WP = 1 << 2, /* Remove airway information and save waypoints only. */
  REPLACE_CUSTOM_WP = 1 << 3, /* Replace custom approach with user defined waypoints */
  REMOVE_ALTERNATE = 1 << 4, /* Remove all alternate legs. */
  REMOVE_TRACKS = 1 << 5, /* Empty track name to force direct */
  FIX_CIRCLETOLAND = 1 << 7, /* Add a dummy best guess runway for circle-to-land approaches for X-Plane */
  FIX_PROC_ENTRY_EXIT = 1 << 8, /* Add any removed procedure entry and exit points back
                                 * if they are attached to an airway. */
  FIX_PROC_ENTRY_EXIT_ALWAYS = 1 << 9, /* Always add any removed procedure entry and exit points back */
  SAVE_MSFS = 1 << 10, /* Insert all SID/STAR waypoints and add procedure information for each waypoint.
                        * Add approach information to last waypoint/destination.
                        * Also adds airport information to waypoints from simulator database.  */
  SAVE_LNMPLN = 1 << 11, /* Ignore menu options to save procedures or airways as waypoints */

  ISG_USER_WP_NAMES = 1 << 12, /* Modified user waypoint names for ISG */

  /* Export adjust options for most export formats */
  DEFAULT_OPTS = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | FIX_PROC_ENTRY_EXIT,

  /* Do not add waypoint and procedure entry or exit back. */
  DEFAULT_OPTS_PROC = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS,

  /* Always add entry and exit waypoints for procedures. This is used for formats not supporting procedures. */
  DEFAULT_OPTS_NO_PROC = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | FIX_PROC_ENTRY_EXIT_ALWAYS,

  /* LNMPLN save and load format. Does not mangle anything. */
  DEFAULT_OPTS_LNMPLN = FIX_PROC_ENTRY_EXIT | SAVE_LNMPLN,

  /* LNMPLN save selected legs as plan. */
  DEFAULT_OPTS_LNMPLN_SAVE_SELECTED = DEFAULT_OPTS_LNMPLN | rf::REMOVE_ALTERNATE,

  /* Option for RouteStringWriter used to generate a route description */
  DEFAULT_OPTS_ROUTESTRING = FIX_PROC_ENTRY_EXIT,

  /* Microsoft Flight Simulator 2020 */
  DEFAULT_OPTS_MSFS = DEFAULT_OPTS | SAVE_MSFS,

  /* Export adjust options for XP11 and old FMS3 */
  DEFAULT_OPTS_FMS3 = rf::DEFAULT_OPTS_NO_PROC,
  DEFAULT_OPTS_FMS11 = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS | rf::FIX_CIRCLETOLAND,

  /* Garmin GPX */
  DEFAULT_OPTS_GPX = rf::DEFAULT_OPTS | rf::SAVE_AIRWAY_WP | rf::SAVE_SIDSTAR_WP | rf::SAVE_APPROACH_WP
};

Q_DECLARE_FLAGS(RouteAdjustOptions, rf::RouteAdjustOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(rf::RouteAdjustOptions);

}

#endif // LNM_ROUTEFLAGS_H
