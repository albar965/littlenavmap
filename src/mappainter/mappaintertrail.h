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

#ifndef LITTLENAVMAP_MAPPAINTERTRACK_H
#define LITTLENAVMAP_MAPPAINTERTRACK_H

#include "mappainter/mappaintervehicle.h"

/*
 * Draws the simulator user aircraft track
 */
class MapPainterTrail :
  public MapPainterVehicle
{
public:
  MapPainterTrail(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterTrail() override;

  virtual void render() override;

};

#endif // LITTLENAVMAP_MAPPAINTERTRACK_H
