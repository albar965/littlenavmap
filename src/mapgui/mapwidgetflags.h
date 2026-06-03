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

#ifndef LNM_MAPWIDGETFLAGS_H
#define LNM_MAPWIDGETFLAGS_H

#include "util/flags.h"

namespace ms {

/* State of click, drag and drop actions on the map */
enum MouseState : quint32
{
  // Also change in mapMouseStateToString()

  DRAG_NONE = 0, /* Nothing */

  DRAG_DIST_PRE = 1 << 0, /* Show cross cursor and wait for click and hold */
  DRAG_DIST_NEW_TO = 1 << 1, /* A new distance measurement line is dragged moving the endpoint */
  DRAG_DIST_CHANGE_FROM = 1 << 2, /* A present distance measurement line is changed dragging the origin */
  DRAG_DIST_CHANGE_TO = 1 << 3, /* A present distance measurement line is changed dragging the endpoint */

  DRAG_ROUTE_LEG = 1 << 4, /* Changing a flight plan leg by adding a new point */
  DRAG_ROUTE_POINT = 1 << 5, /* Changing the flight plan by replacing a present waypoint */

  DRAG_USER_POINT = 1 << 6, /* Moving a userpoint around */

  DRAG_HOLDING = 1 << 7, /* Moving a userpoint around */
  DRAG_RANGE = 1 << 8, /* Moving a userpoint around */

  DRAG_CANCEL = 1 << 9, /* Right mousebutton clicked or Esc clicked - cancel all actions.
                         * Delays action to avoid recognizing stray mouse clicks. */

  /* Any dragging of distance measurment lines */
  DRAG_DIST_ANY = DRAG_DIST_NEW_TO | DRAG_DIST_CHANGE_FROM | DRAG_DIST_CHANGE_TO,

  /* Used to check if any interaction is going on */
  DRAG_ANY = ms::DRAG_DIST_ANY |
             ms::DRAG_HOLDING | ms::DRAG_RANGE | ms::DRAG_USER_POINT |
             ms::DRAG_ROUTE_LEG | ms::DRAG_ROUTE_POINT,

};

ATOOLS_DECLARE_FLAGS_32(MouseStates, ms::MouseState)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(ms::MouseStates)
ATOOLS_DECLARE_DEBUG_OPERATORS_FOR_FLAGS(MouseStates, ms::MouseState)

} // namespace ms

Q_DECLARE_TYPEINFO(ms::MouseStates, Q_PRIMITIVE_TYPE);

#endif // LNM_MAPWIDGETFLAGS_H
