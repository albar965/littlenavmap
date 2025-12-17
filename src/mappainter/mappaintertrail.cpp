/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "fs/sc/simconnectuseraircraft.h"
#include "geo/aircrafttrail.h"
#include "mapgui/mappaintwidget.h"
#include "route/route.h"
#include "util/paintercontextsaver.h"

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
    const atools::geo::Pos& aircraftPos = mapPaintWidget->getUserAircraft().getPosition();
    const AircraftTrail& aircraftTrail = mapPaintWidget->getAircraftTrail();
    atools::geo::Rect bounding = aircraftTrail.getBounding();
    bounding.extend(aircraftPos); // Add aircraft since this is not neccesarily a part of the trail

    // Have to do separate check for single point rect which appears right after deleting the trail
    if(!aircraftTrail.isEmpty() && (resolves(bounding) || (bounding.isPoint() && context->viewportRect.overlaps(bounding))))
    {
#ifdef DEBUG_DRAW_TRAIL
      {
        atools::util::PainterContextSaver saver(context->painter);
        context->painter->setPen(QPen(Qt::blue, 2));

        for(int i = 0; i < aircraftTrail.size(); i++)
          drawText(context->painter, aircraftTrail.at(i).getPosition(), QString::number(i), true /* topCorner */, true /* leftCorner */);

        context->painter->setPen(QPen(Qt::red, 2));
        int lineStringIndex = 0;
        for( const atools::geo::LineString& linestring : aircraftTrail.getLineStrings())
        {
          int posIndex = 0;
          for(const atools::geo::Pos& pos : linestring)
            drawText(context->painter, pos, QString::number(lineStringIndex) + "/" + QString::number(posIndex++),
                     false /* topCorner */, false /* leftCorner */);
          lineStringIndex++;
        }
      }

#endif

      float maxAltitude = aircraftTrail.getMaxAltitude();
      // Use flight plan cruise as max altitude if valid
      if(context->route->getSizeWithoutAlternates() > 2)
        maxAltitude = std::max(context->route->getCruiseAltitudeFt(), maxAltitude);

      context->startTimer("Aircraft Trail");
      atools::util::PainterContextSaver saver(context->painter);
      paintAircraftTrail(aircraftTrail.getLineStrings(), aircraftTrail.getMinAltitude(), maxAltitude, aircraftPos);
      context->endTimer("Aircraft Trail");
    }
  }
}
