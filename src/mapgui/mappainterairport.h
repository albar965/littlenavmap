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

#ifndef LITTLENAVMAP_MAPPAINTERAIRPORT_H
#define LITTLENAVMAP_MAPPAINTERAIRPORT_H

#include "mapgui/mappainter.h"

class SymbolPainter;

namespace maptypes {
struct MapAirport;

struct MapRunway;

}

class MapPainterAirport :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterAirport(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                    bool verboseMsg);
  virtual ~MapPainterAirport();

  virtual void render(const PaintContext *context) override;

private:
  void drawAirportSymbol(const PaintContext *context, const maptypes::MapAirport& ap, int x, int y);
  void drawAirportDiagram(const PaintContext *context, const maptypes::MapAirport& airport, bool fast);
  void drawAirportSymbolOverview(const PaintContext *context, const maptypes::MapAirport& ap);
  void runwayCoords(const QList<maptypes::MapRunway> *rw, QList<QPoint> *centers, QList<QRect> *rects,
                    QList<QRect> *innerRects, QList<QRect> *backRects);
  QString parkingName(const QString& name);

  static Q_DECL_CONSTEXPR int RUNWAY_HEADING_FONT_SIZE = 12;
  static Q_DECL_CONSTEXPR int RUNWAY_TEXT_FONT_SIZE = 16;
  static Q_DECL_CONSTEXPR int RUNWAY_NUMBER_FONT_SIZE = 20;
};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
