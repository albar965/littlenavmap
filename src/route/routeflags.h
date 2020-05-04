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
  SAVE_SIDSTAR_WP = 1 << 1, /* Save SID and STAR as waypoints and remove approach information. */
  SAVE_AIRWAY_WP = 1 << 2, /* Save SID and STAR as waypoints and remove approach information. */
  REPLACE_CUSTOM_WP = 1 << 3, /* Replace custom approach with user defined waypoints */
  REMOVE_ALTERNATE = 1 << 4, /* Remove all alternate legs. */
  REMOVE_TRACKS = 1 << 5, /* Empty track name to force direct */
  FIX_AIRPORT_IDENT = 1 << 6, /* Replace X-Plane dummy airport idents with real ICAO ident if available. */
  FIX_CIRCLETOLAND = 1 << 7, /* Add a dummy best guess runway for circle-to-land approaches for X-Plane */

  /* Export adjust options for most export formats */
  DEFAULT_OPTS = rf::REPLACE_CUSTOM_WP | rf::REMOVE_ALTERNATE | rf::REMOVE_TRACKS,

  /* Export adjust options for XP11 and old FMS3 */
  DEFAULT_OPTS_FMS3 = rf::DEFAULT_OPTS | rf::FIX_AIRPORT_IDENT,
  DEFAULT_OPTS_FMS11 = rf::DEFAULT_OPTS | rf::FIX_AIRPORT_IDENT | rf::FIX_CIRCLETOLAND,
};

Q_DECLARE_FLAGS(RouteAdjustOptions, rf::RouteAdjustOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(rf::RouteAdjustOptions);

}

#endif // LNM_ROUTEFLAGS_H
