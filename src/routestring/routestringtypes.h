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

#ifndef LNM_ROUTESTRINGTYPES_H
#define LNM_ROUTESTRINGTYPES_H

#include <QFlags>
#include <QStringList>

namespace rs {

/* Do not change order since it is used to save to options */
enum RouteStringOption
{
  NONE = 0,

  /* Writing options when converting flight plan to string ====================== */
  DCT = 1 << 0, /* Add DCT */
  START_AND_DEST = 1 << 1, /* Add departure and/or destination ident */
  ALT_AND_SPEED = 1 << 2, /* Add altitude and speed restriction */
  SID_STAR = 1 << 3, /* Add sid, star and transition names if selected */
  SID_STAR_GENERIC = 1 << 4, /* Always add SID and STAR keyword */
  GFP = 1 << 5, /* Produce Garmin flight plan format using special coordinate format */
  NO_AIRWAYS = 1 << 6, /* Add all waypoints instead of from/airway/to triplet */

  SID_STAR_SPACE = 1 << 7, /* Separate SID/STAR and transition with space. Not ATS compliant. */
  RUNWAY = 1 << 8, /* Add departure runway if available. Not ATS compliant. */
  APPROACH = 1 << 9, /* Add approach ARINC name and transition after destination. Not ATS compliant. */
  FLIGHTLEVEL = 1 << 10, /* Append flight level at end of string. Not ATS compliant. */

  GFP_COORDS = 1 << 11, /* Suffix all navaids with coordinates for new GFP format */
  USR_WPT = 1 << 12, /* User waypoints for all navaids to avoid locked waypoints from Garmin */
  SKYVECTOR_COORDS = 1 << 13, /* Skyvector coordinate format */
  NO_FINAL_DCT = 1 << 14, /* omit last DCT for Flight Factor export */

  ALTERNATES = 1 << 15, /* treat last airport as alternate */

  /* Reading options when converting string to flight plan ====================== */
  READ_ALTERNATES = 1 << 16, /* Read any consecutive list of airports at the end of the string as alternates */
  READ_NO_AIRPORTS = 1 << 17, /* Do not look for first and last entry as airport. */
  READ_MATCH_WAYPOINTS = 1 << 18, /* Match coordinate formats to nearby waypoints. */

  /* Writing options when converting flight plan to string ====================== */
  NO_TRACKS = 1 << 19, /* Do not use NAT, PACOTS or AUSOTS. */

  DEFAULT_OPTIONS = START_AND_DEST | ALT_AND_SPEED | SID_STAR | ALTERNATES | READ_ALTERNATES
};

Q_DECLARE_FLAGS(RouteStringOptions, RouteStringOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(rs::RouteStringOptions);

/* Remove all invalid characters and simplify string */
QStringList cleanRouteString(const QString& string);

} // namespace rs

#endif // LNM_ROUTESTRINGTYPES_H
