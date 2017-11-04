/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/mappainteraircraft.h"

#include "mapgui/mapwidget.h"
#include "navapp.h"
#include "mapgui/maplayer.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

MapPainterAircraft::MapPainterAircraft(MapWidget *mapWidget, MapScale *mapScale)
  : MapPainterVehicle(mapWidget, mapScale)
{

}

MapPainterAircraft::~MapPainterAircraft()
{

}

void MapPainterAircraft::render(PaintContext *context)
{
  if(!context->objectTypes.testFlag(map::AIRCRAFT) &&
     !context->objectTypes.testFlag(map::AIRCRAFT_AI) &&
     !context->objectTypes.testFlag(map::AIRCRAFT_TRACK))
    // If actions are unchecked return
    return;

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver);

  if(context->objectTypes.testFlag(map::AIRCRAFT_TRACK))
    paintTrack(context);

  if(NavApp::isConnected() || mapWidget->getUserAircraft().isDebug())
  {
    // Draw AI aircraft
    if(context->objectTypes & map::AIRCRAFT_AI && context->mapLayer->isAiAircraftLarge())
    {
      for(const SimConnectAircraft& ac : mapWidget->getAiAircraft())
      {
        if(ac.getCategory() != atools::fs::sc::BOAT &&
           (ac.getModelRadiusCorrected() * 2 > layer::LARGE_AIRCRAFT_SIZE || context->mapLayer->isAiAircraftSmall()) &&
           (!ac.isOnGround() || context->mapLayer->isAiAircraftGround()))
          paintAiVehicle(context, ac);
      }
    }

    if(context->objectTypes.testFlag(map::AIRCRAFT))
    {
      const atools::fs::sc::SimConnectUserAircraft& userAircraft = mapWidget->getUserAircraft();
      const atools::geo::Pos& pos = userAircraft.getPosition();

      if(pos.isValid())
      {
        if(context->dOpt(opts::ITEM_USER_AIRCRAFT_WIND_POINTER))
          paintWindPointer(context, userAircraft, context->painter->device()->width() / 2, 0);

        bool hidden = false;
        float x, y;
        if(wToS(pos, x, y, DEFAULT_WTOS_SIZE, &hidden))
        {
          if(!hidden)
            paintUserAircraft(context, userAircraft, x, y);
        }
      }
    }
  }
}
