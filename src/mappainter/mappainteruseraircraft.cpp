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

#include "mappainter/mappainteruseraircraft.h"

#include "app/navapp.h"
#include "mapgui/mapfunctions.h"
#include "mapgui/mappaintwidget.h"
#include "mappainter/paintcontext.h"
#include "online/onlinedatacontroller.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;

MapPainterUserAircraft::MapPainterUserAircraft(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainterVehicle(mapWidget, mapScale, paintContext)
{
}

MapPainterUserAircraft::~MapPainterUserAircraft()
{

}

void MapPainterUserAircraft::render()
{
  atools::util::PainterContextSaver saver(context->painter);
  const SimConnectUserAircraft& userAircraft = mapPaintWidget->getUserAircraft();

  if(context->objectTypes & map::AIRCRAFT_ALL)
  {
    // Draw user aircraft ====================================================================
    if(context->objectTypes.testFlag(map::AIRCRAFT))
    {
      // Use higher accuracy - falls back to normal position if not set
      atools::geo::PosD pos = userAircraft.getPositionD();
      if(pos.isValid())
      {
        bool hidden = false;
        double x, y;
        if(wToS(pos, x, y, DEFAULT_WTOS_SIZE, &hidden))
        {
          if(!hidden)
          {
            paintTurnPath(userAircraft);
            paintUserAircraft(userAircraft, static_cast<float>(x), static_cast<float>(y));
          }
        }
      }
    }
  } // if(context->objectTypes & map::AIRCRAFT_ALL)

  // Wind display depends only on option
  if(context->paintWindHeader && context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_WIND_POINTER) && userAircraft.isValid())
    paintWindPointer(userAircraft, context->screenRect.width() / 2.f, 2.f);
}
