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

#include "mappainter/mappaintertrack.h"

#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

MapPainterTrack::MapPainterTrack(MapPaintWidget *mapWidget, MapScale *mapScale)
  : MapPainterVehicle(mapWidget, mapScale)
{

}

MapPainterTrack::~MapPainterTrack()
{

}

void MapPainterTrack::render(PaintContext *context)
{
  if(!context->objectTypes.testFlag(map::AIRCRAFT_TRACK))
    // If actions are unchecked return
    return;

  atools::util::PainterContextSaver(context->painter);
  paintAircraftTrack(context);
}
