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

#include "mappainter/mappaintership.h"

#include "app/navapp.h"
#include "fs/sc/simconnectaircraft.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

MapPainterShip::MapPainterShip(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainterVehicle(mapWidget, mapScale, paintContext)
{

}

MapPainterShip::~MapPainterShip()
{
}

void MapPainterShip::render()
{
  const static QMargins MARGINS(100, 100, 100, 100);

  if(!context->objectTypes.testFlag(map::AIRCRAFT_AI_SHIP))
    // If actions are unchecked return
    return;

  if(NavApp::isConnected() || mapPaintWidget->getUserAircraft().isDebug())
  {
    // Draw AI ships first
    if(context->objectTypes & map::AIRCRAFT_AI_SHIP && context->mapLayer->isAiShipLarge())
    {
      atools::util::PainterContextSaver saver(context->painter);
      bool hidden = false;
      float x, y;

      for(const SimConnectAircraft& ac : mapPaintWidget->getAiAircraft())
      {
        if(ac.isAnyBoat() && (ac.getModelRadiusCorrected() * 2 > layer::LARGE_SHIP_SIZE || context->mapLayer->isAiShipSmall()))
        {
          if(wToSBuf(ac.getPosition(), x, y, MARGINS, &hidden))
          {
            if(!hidden)
              paintAiVehicle(ac, x, y, false /* forceLabelNearby */);
          }
        }
      }
    }
  }
}
