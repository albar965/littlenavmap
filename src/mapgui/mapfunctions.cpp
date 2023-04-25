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

#include "mapgui/mapfunctions.h"

#include "fs/sc/simconnectaircraft.h"
#include "mapgui/maplayer.h"

#include <QVector>

namespace mapfunc {

bool aircraftVisible(const atools::fs::sc::SimConnectAircraft& ac, const MapLayer *layer, bool hideAiOnGround)
{
  bool show = true;
  if(ac.isOnGround() && hideAiOnGround)
    // Hide on ground if flag is set - otherwise use normal logic
    show &= layer->isAiAircraftGround();
  else
  {
    if(ac.isOnline())
      show &= layer->isOnlineAircraft();
    else if(ac.getModelRadiusCorrected() * 2 < layer::LARGE_AIRCRAFT_SIZE)
      show &= layer->isAiAircraftSmall();
    else
      show &= layer->isAiAircraftLarge();
  }
  return show;
}

bool windBarbVisible(const atools::geo::Pos& pos, const MapLayer *layer, bool sphericalProjection)
{
  if(layer->getWindBarbs() == 0)
    return false;

  float gridSpacing = static_cast<float>(layer->getWindBarbs());

  float gridX = gridSpacing;
  float gridY = gridSpacing;
  if(sphericalProjection)
  {
    // Adjust horizontal density at higher or lower latitudes
    float lat = std::abs(pos.getLatY());

    // Perfer multiple of five to snap to display globe grid
    if(lat > 89.f)
      gridX = 360.f; // Draw only one barb on all levels
    else if(lat > 88.f)
      gridX *= 15.f;
    else if(lat > 85.f)
      gridX *= 10.f;
    else if(lat > 75.f)
      gridX *= 5.f;
    else if(lat > 70.f)
      gridX *= 2.f;
  }

  // Return true if point is near a grid position
  return pos.nearGridLonLat(gridX, gridY);
}

} // namespace mapfunc
