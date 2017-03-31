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

#include "mapgui/mappainterairspace.h"

#include "mapgui/mapwidget.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "atools.h"
#include "common/constants.h"
#include "util/paintercontextsaver.h"
#include "common/textplacement.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/GeoDataLinearRing.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterAirspace::MapPainterAirspace(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(mapWidget, mapQuery, mapScale)
{
}

MapPainterAirspace::~MapPainterAirspace()
{

}

void MapPainterAirspace::render(PaintContext *context)
{
  if(!context->mapLayer->isAirspace())
    return;

  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  const QList<MapAirspace> *airspaces = query->getAirspaces(curBox, context->mapLayer, context->airspaceTypes,
                                                            context->viewContext == Marble::Animation);
  if(airspaces != nullptr)
  {
    Marble::GeoPainter *painter = context->painter;
    atools::util::PainterContextSaver saver(painter);
    Q_UNUSED(saver);

    painter->setBackgroundMode(Qt::TransparentMode);

    for(const MapAirspace& airspace : *airspaces)
    {
      Marble::GeoDataLinearRing linearRing;
      linearRing.setTessellate(true);

      painter->setPen(mapcolors::penForAirspace(airspace));
      painter->setBrush(mapcolors::colorForAirspaceFill(airspace));

      for(const Pos& pos : airspace.lines)
        linearRing.append(Marble::GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));

      painter->drawPolygon(linearRing);
    }
  }
}
