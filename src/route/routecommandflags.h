/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_ROUTECOMMANDFLAGS_H
#define LITTLENAVMAP_ROUTECOMMANDFLAGS_H

namespace rctype {
/* Command type used to merge undo actions */
enum RouteCmdType
{
  EDIT = -1, /* Unmergeable edit */
  DELETE = 0, /* Waypoint(s) deleted in table */
  MOVE = 1, /* Waypoint(s) moved in table */
  ALTITUDE = 2, /* Altitude changed in spin box */
  REVERSE = 4, /* Route reverse action */
  REMARKS = 5 /* Route remarks edited */
};

}

#endif // LITTLENAVMAP_ROUTECOMMANDFLAGS_H
