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

#include "mapwidgetflags.h"

#include <QDebug>

namespace ms {

QStringList mapMouseStateToString(const ms::MouseStates& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(DRAG_NONE);
  ATOOLS_FLAGS_TO_STR(DRAG_DIST_PRE);
  ATOOLS_FLAGS_TO_STR(DRAG_DIST_NEW_TO);
  ATOOLS_FLAGS_TO_STR(DRAG_DIST_CHANGE_FROM);
  ATOOLS_FLAGS_TO_STR(DRAG_DIST_CHANGE_TO);
  ATOOLS_FLAGS_TO_STR(DRAG_ROUTE_LEG);
  ATOOLS_FLAGS_TO_STR(DRAG_ROUTE_POINT);
  ATOOLS_FLAGS_TO_STR(DRAG_USER_POINT);
  ATOOLS_FLAGS_TO_STR(DRAG_HOLDING);
  ATOOLS_FLAGS_TO_STR(DRAG_RANGE);
  ATOOLS_FLAGS_TO_STR(DRAG_CANCEL);
  ATOOLS_FLAGS_TO_STR_END;
}

ATOOLS_DEFINE_DEBUG_OPERATORS_FOR_FLAGS(MouseStates, MouseState, mapMouseStateToString)

} // namespace mapwin
