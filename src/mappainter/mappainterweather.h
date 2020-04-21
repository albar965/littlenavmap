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

#ifndef LITTLENAVMAP_MAPPAINTERWEATHER_H
#define LITTLENAVMAP_MAPPAINTERWEATHER_H

#include "mappainter/mappainter.h"

namespace atools {
namespace fs {
namespace weather {
class Metar;
}
}
}

struct PaintAirportType;

/*
 * Draws airport weather symbols.
 */
class MapPainterWeather :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterWeather(MapPaintWidget *mapPaintWidget, MapScale *mapScale);
  virtual ~MapPainterWeather() override;

  virtual void render(PaintContext *context) override;

private:
  void drawAirportWeather(PaintContext *context, const atools::fs::weather::Metar& metar,
                          float x, float y);

};

#endif // LITTLENAVMAP_MAPPAINTERWEATHER_H
