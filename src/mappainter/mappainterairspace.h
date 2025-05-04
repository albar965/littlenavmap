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

#ifndef LITTLENAVMAP_MAPPAINTERAIRSPACE_H
#define LITTLENAVMAP_MAPPAINTERAIRSPACE_H

#include "mappainter/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;

/*
 * Paints all airspaces/boundaries.
 */
class MapPainterAirspace :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainterAirspace)

public:
  MapPainterAirspace(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterAirspace() override;

  virtual void render() override;

};

#endif // LITTLENAVMAP_MAPPAINTERAIRSPACE_H
