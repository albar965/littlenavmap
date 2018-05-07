/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "online/onlinedatacontroller.h"
#include "mapgui/maplayer.h"
#include "navapp.h"

#include <QVector>

namespace mapfunc {

bool aircraftVisible(const atools::fs::sc::SimConnectAircraft& ac, const MapLayer *layer)
{
  bool show = true;
  if(ac.isOnGround())
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

} // namespace mapfunc
