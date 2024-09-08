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

#ifndef LNM_MAPWIDGETFLAGS_H
#define LNM_MAPWIDGETFLAGS_H

#include "util/flags.h"

namespace mw {
/* State of click, drag and drop actions on the map */
enum MouseState : quint32
{
  NONE = 0, /* Nothing */

  DRAG_DIST_NEW_END = 1 << 0, /* A new distance measurement line is dragged moving the endpoint */
  DRAG_DIST_CHANGE_START = 1 << 1, /* A present distance measurement line is changed dragging the origin */
  DRAG_DIST_CHANGE_END = 1 << 2, /* A present distance measurement line is changed dragging the endpoint */

  DRAG_ROUTE_LEG = 1 << 3, /* Changing a flight plan leg by adding a new point */
  DRAG_ROUTE_POINT = 1 << 4, /* Changing the flight plan by replacing a present waypoint */

  DRAG_USER_POINT = 1 << 5, /* Moving a userpoint around */

  DRAG_POST = 1 << 6, /* Mouse released - all done */
  DRAG_POST_MENU = 1 << 7, /* A menu is opened after selecting multiple objects.
                            * Avoid cancelling all drag when loosing focus */
  DRAG_POST_CANCEL = 1 << 8, /* Right mousebutton clicked - cancel all actions */

  /* Used to check if any interaction is going on */
  DRAG_ALL = mw::DRAG_DIST_NEW_END | mw::DRAG_DIST_CHANGE_START | mw::DRAG_DIST_CHANGE_END |
             mw::DRAG_ROUTE_LEG | mw::DRAG_ROUTE_POINT |
             mw::DRAG_USER_POINT
};

ATOOLS_DECLARE_FLAGS_32(MouseStates, MouseState)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(mw::MouseStates)
}

#endif // LNM_MAPWIDGETFLAGS_H
