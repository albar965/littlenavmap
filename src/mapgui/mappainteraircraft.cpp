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

#include "mapgui/mappainteraircraft.h"

#include "mapgui/mapwidget.h"
#include "navapp.h"
#include "mapgui/maplayer.h"
#include "util/paintercontextsaver.h"
#include "geo/calculations.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

const int NUM_CLOSEST_AI_LABELS = 5;
const int DIST_METER_CLOSEST_AI_LABELS = atools::geo::nmToMeter(20);

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

  const atools::fs::sc::SimConnectUserAircraft& userAircraft = mapWidget->getUserAircraft();
  const atools::geo::Pos& pos = userAircraft.getPosition();

  if(NavApp::isConnected() || mapWidget->getUserAircraft().isDebug())
  {
    // Draw AI aircraft
    if(context->objectTypes & map::AIRCRAFT_AI && context->mapLayer->isAiAircraftLarge())
    {
      typedef std::pair<const SimConnectAircraft *, float> AiDistType;
      QVector<AiDistType> aiSorted;

      for(const SimConnectAircraft& ac : mapWidget->getAiAircraft())
        aiSorted.append(std::make_pair(&ac, pos.distanceMeterTo(ac.getPosition())));

      std::sort(aiSorted.begin(), aiSorted.end(), [](const AiDistType& ai1,
                                                     const AiDistType& ai2) -> bool
      {
        // returns ​true if the first argument is less than (i.e. is ordered before) the second.
        return ai1.second > ai2.second;
      });

      int num = aiSorted.size();
      for(const AiDistType& adt : aiSorted)
      {
        const SimConnectAircraft& ac = *adt.first;

        if(ac.getCategory() != atools::fs::sc::BOAT &&
           (ac.getModelRadiusCorrected() * 2 > layer::LARGE_AIRCRAFT_SIZE || context->mapLayer->isAiAircraftSmall()) &&
           (!ac.isOnGround() || context->mapLayer->isAiAircraftGround()))
        {
          paintAiVehicle(context, ac, --num < NUM_CLOSEST_AI_LABELS && adt.second < DIST_METER_CLOSEST_AI_LABELS);
        }
      }
    }

    if(context->objectTypes.testFlag(map::AIRCRAFT))
    {
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
