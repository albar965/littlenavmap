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

#ifndef LITTLENAVMAP_MAPPAINTERMSA_H
#define LITTLENAVMAP_MAPPAINTERMSA_H

#include "mappainter/mappainter.h"

namespace map {
struct MapAirportMsa;
}

/*
 * Draws the airport MSA backdrop circles and details sector diagram.
 */
class MapPainterMsa :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterMsa(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterMsa() override;

  virtual void render() override;

private:
  void drawMsaSymbol(const map::MapAirportMsa& airportMsa, float x, float y, bool fast);

};

#endif // LITTLENAVMAP_MAPPAINTERMSA_H
