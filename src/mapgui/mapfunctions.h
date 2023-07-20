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

#ifndef LNM_MAPFUNCTIONS_H
#define LNM_MAPFUNCTIONS_H

namespace atools {
namespace geo {
class Pos;
}
namespace fs {
namespace sc {
class SimConnectAircraft;
}
}
}

class MapLayer;

namespace mapfunc {
/*
 * Common functions used by paint layers and screen index
 */

/* True if aircraft is visible for current layer and aicraft properties */
bool aircraftVisible(const atools::fs::sc::SimConnectAircraft& ac, const MapLayer *layer, bool hideAiOnGround);

/* True if wind barb is shown at confluence point */
bool windBarbVisible(const atools::geo::Pos& pos, const MapLayer *layer, bool sphericalProjection);

} // namespace mapfunc

#endif // LNM_MAPFUNCTIONS_H
