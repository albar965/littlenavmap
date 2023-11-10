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
#include "route/route.h"
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
    const atools::geo::Rect& bounding = aircraftTrail.getBounding();

    // Have to do separate check for single point rect which appears right after deleting the trail
    if(!aircraftTrail.isEmpty() && (resolves(bounding) || (bounding.isPoint() && context->viewportRect.overlaps(bounding))))
    {
#ifdef DEBUG_DRAW_TRACK
      {
        atools::util::PainterContextSaver saver(context->painter);
        context->painter->setPen(QPen(Qt::blue, 2));
        int i = 0;
        for(const AircraftTrailPos& pos : aircraftTrail)
          drawText(context->painter, pos.getPosition(), QString::number(i++), 0.f, 0.f);
      }

#endif

      float maxAltitude = aircraftTrail.getMaxAltitude();
      // Use flight plan cruise as max altitude if valid
      if(context->route->getSizeWithoutAlternates() > 2)
        maxAltitude = std::max(context->route->getCruiseAltitudeFt(), maxAltitude);

      atools::util::PainterContextSaver saver(context->painter);
      const QVector<atools::geo::LineString> lineStrings = aircraftTrail.getLineStrings(mapPaintWidget->getUserAircraft().getPosition());
      paintAircraftTrail(lineStrings, aircraftTrail.getMinAltitude(), maxAltitude);
    }
  }
}
