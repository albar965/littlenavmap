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

#include "mappainter/mappaintertrail.h"

#include "app/navapp.h"
#include "common/aircrafttrail.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "mapgui/mappaintwidget.h"
#include "util/paintercontextsaver.h"
#include "geo/linestring.h"

#include <marble/GeoPainter.h>

using atools::fs::sc::SimConnectAircraft;

MapPainterTrail::MapPainterTrail(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainterVehicle(mapWidget, mapScale, paintContext)
{

}

MapPainterTrail::~MapPainterTrail()
{

}

void MapPainterTrail::render()
{
  if(context->objectTypes.testFlag(map::AIRCRAFT_TRAIL))
  {
    const AircraftTrail& aircraftTrail = NavApp::getAircraftTrail();
    if(!aircraftTrail.isEmpty() && resolves(aircraftTrail.getBounding()))
    {
      atools::util::PainterContextSaver saver(context->painter);
      const QVector<atools::geo::LineString> lineStrings = aircraftTrail.getLineStrings(mapPaintWidget->getUserAircraft().getPosition());
      paintAircraftTrail(lineStrings, aircraftTrail.getMinAltitude(), aircraftTrail.getMaxAltitude());
    }
  }
}
