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

#include "mappainter/mappaintermsa.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "util/paintercontextsaver.h"
#include "common/symbolpainter.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using map::MapAirportMsa;

MapPainterMsa::MapPainterMsa(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterMsa::~MapPainterMsa()
{
}

void MapPainterMsa::render()
{
  if(context->mapLayer->isAirportMsa()) // Enabled by layer / zoom factor
  {
    const GeoDataLatLonBox& curBox = context->viewport->viewLatLonAltBox();

    float x, y;
    if(context->objectTypes.testFlag(map::AIRPORT_MSA)) // Enabled by checkbox in view
    {
      // Get drawing objects and cache them
      bool overflow = false;
      const QList<MapAirportMsa> *msaList = mapQuery->getAirportMsa(curBox, context->mapLayer, context->lazyUpdate, overflow);
      context->setQueryOverflow(overflow);

      if(msaList != nullptr)
      {
        atools::util::PainterContextSaver saver(context->painter);

        for(const MapAirportMsa& msa : *msaList)
        {
          bool visible = wToS(msa.position, x, y /*, scale->getScreeenSizeForRect(msa.bounding)*/);

          if(!visible)
            // Check bounding rect for visibility
            visible = msa.bounding.overlaps(context->viewportRect);

          if(visible)
          {
            if(context->objCount())
              return;

            drawMsaSymbol(msa, x, y, context->drawFast);
          }
        }
      }
    }
  }
}

void MapPainterMsa::drawMsaSymbol(const map::MapAirportMsa& airportMsa, float x, float y, bool fast)
{
  atools::util::PainterContextSaver saver(context->painter);

  Marble::GeoPainter *painter = context->painter;

  int size = 0;
  float scale = 0.f;
  if(!context->mapLayer->isAirportMsaDetails())
  {
    if(airportMsa.navType == map::AIRPORT)
      size = context->sz(context->symbolSizeAirport, context->mapLayer->getAirportSymbolSize());
    else if(airportMsa.navType == map::VOR)
      size = context->sz(context->symbolSizeNavaid, context->mapLayer->getVorSymbolSize());
    else if(airportMsa.navType == map::NDB)
      size = context->sz(context->symbolSizeNavaid, context->mapLayer->getNdbSymbolSize());
    else if(airportMsa.navType == map::WAYPOINT)
      size = context->sz(context->symbolSizeNavaid, context->mapLayer->getWaypointSymbolSize());
    size = std::max(size, 6);
  }
  else
    scale = context->mapLayer->getAirportMsaSymbolScale();

  // Draw the full symbol with all sectors
  symbolPainter->drawAirportMsa(painter, airportMsa, x, y, size * 2, scale, true /* header */, true /* transparency */, fast);
}
