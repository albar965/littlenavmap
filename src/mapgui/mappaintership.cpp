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

#include "mapgui/mappaintership.h"

#include "navapp.h"
#include "mapgui/mapwidget.h"
#include "mapgui/maplayer.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

MapPainterShip::MapPainterShip(MapWidget *mapWidget, MapScale *mapScale)
  : MapPainterVehicle(mapWidget, mapScale)
{

}

MapPainterShip::~MapPainterShip()
{
}

void MapPainterShip::render(PaintContext *context)
{
  if(!context->objectTypes.testFlag(map::AIRCRAFT_AI_SHIP))
    // If actions are unchecked return
    return;

  if(NavApp::isConnected() || mapWidget->getUserAircraft().isDebug())
  {
    // Draw AI ships first
    if(context->objectTypes & map::AIRCRAFT_AI_SHIP && context->mapLayer->isAiShipLarge())
    {
      atools::util::PainterContextSaver saver(context->painter);
      Q_UNUSED(saver);

      for(const SimConnectAircraft& ac : mapWidget->getAiAircraft())
      {
        if(ac.getCategory() == atools::fs::sc::BOAT &&
           (ac.getModelRadiusCorrected() * 2 > layer::LARGE_SHIP_SIZE || context->mapLayer->isAiShipSmall()))
          paintAiVehicle(context, ac, false /* force label */);
      }
    }
  }
}
