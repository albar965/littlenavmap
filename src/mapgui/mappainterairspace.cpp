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

#include "mapgui/mappainterairspace.h"

#include "common/mapcolors.h"
#include "util/paintercontextsaver.h"
#include "route/route.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "query/airspacequery.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/ViewportParams.h>

#include <QElapsedTimer>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterAirspace::MapPainterAirspace(MapWidget *mapWidget, MapScale *mapScale,
                                       const Route *routeParam)
  : MapPainter(mapWidget, mapScale), route(routeParam)
{
}

MapPainterAirspace::~MapPainterAirspace()
{

}

void MapPainterAirspace::render(PaintContext *context)
{
  if(!context->mapLayer->isAirspace() ||
     !(context->objectTypes.testFlag(map::AIRSPACE) || context->objectTypes.testFlag(map::AIRSPACE_ONLINE)))
    return;

  // Get online and offline airspace and merge then into one list =============
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  QList<const MapAirspace *> airspaces;

  if(context->objectTypes.testFlag(map::AIRSPACE))
  {
    // qDebug() << Q_FUNC_INFO << "NON ONLINE";
    const QList<MapAirspace> *airspacesNav =
      airspaceQuery->getAirspaces(curBox, context->mapLayer, context->airspaceFilterByLayer,
                                  route->getCruisingAltitudeFeet(),
                                  context->viewContext == Marble::Animation);
    if(airspacesNav != nullptr)
    {
      for(const MapAirspace& airspace : *airspacesNav)
        airspaces.append(&airspace);
    }
  }

  if(context->objectTypes.testFlag(map::AIRSPACE_ONLINE))
  {
    // qDebug() << Q_FUNC_INFO << "ONLINE";
    const QList<MapAirspace> *airspacesOnline =
      airspaceQueryOnline->getAirspaces(curBox, context->mapLayer, context->airspaceFilterByLayer,
                                        route->getCruisingAltitudeFeet(),
                                        context->viewContext == Marble::Animation);
    if(airspacesOnline != nullptr)
    {
      for(const MapAirspace& airspace : *airspacesOnline)
        airspaces.append(&airspace);
    }
  }

  if(!airspaces.isEmpty())
  {
    Marble::GeoPainter *painter = context->painter;
    atools::util::PainterContextSaver saver(painter);
    Q_UNUSED(saver);

    painter->setBackgroundMode(Qt::TransparentMode);

    for(const MapAirspace *airspace : airspaces)
    {
      if(!(airspace->type & context->airspaceFilterByLayer.types))
        continue;

      if(context->viewportRect.overlaps(airspace->bounding))
      {
        if(context->objCount())
          return;

        // qDebug() << airspace.getId() << airspace.name;

        Marble::GeoDataLinearRing linearRing;
        linearRing.setTessellate(true);

        painter->setPen(mapcolors::penForAirspace(*airspace));

        if(!context->drawFast)
          painter->setBrush(mapcolors::colorForAirspaceFill(*airspace));

        const LineString *lines =
          (airspace->online ? airspaceQueryOnline : airspaceQuery)->getAirspaceGeometry(airspace->id);

        for(const Pos& pos : *lines)
          linearRing.append(Marble::GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));

        painter->drawPolygon(linearRing);
      }
    }
  }
}
