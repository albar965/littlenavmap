/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef MAPPAINTERAIRPORT_H
#define MAPPAINTERAIRPORT_H

#include "mapgui/mappainter.h"
#include "common/symbolpainter.h"

class SymbolPainter;

namespace maptypes {
struct MapAirport;

struct MapRunway;

}

class MapPainterAirport :
  public MapPainter
{
public:
  MapPainterAirport(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                    bool verboseMsg);
  virtual ~MapPainterAirport();

  virtual void render(const PaintContext *context) override;

private:
  void drawAirportSymbol(const PaintContext *context, const maptypes::MapAirport& ap, int x, int y, bool fast);
  void drawAirportDiagram(const PaintContext *context, const maptypes::MapAirport& airport, bool fast);
  void drawAirportSymbolOverview(const PaintContext *context, const maptypes::MapAirport& ap, bool fast);
  void runwayCoords(const QList<maptypes::MapRunway> *rw, QList<QPoint> *centers, QList<QRect> *rects,
                    QList<QRect> *innerRects, QList<QRect> *backRects);
  QString parkingName(const QString& name);

};

#endif // MAPPAINTERAIRPORT_H
