/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPPAINTERILS_H
#define LITTLENAVMAP_MAPPAINTERILS_H

#include "mappainter/mappainter.h"

class SymbolPainter;

namespace map {
struct MapIls;

}

/*
 * Draws the ILS feathers and text.
 */
class MapPainterIls :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterIls(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterIls() override;

  virtual void render() override;

private:
  /* Fixed value that is used when writing the database. See atools::fs::db::IlsWriter */
  static Q_DECL_CONSTEXPR int FEATHER_LEN_NM = 9;
  static Q_DECL_CONSTEXPR int MIN_LENGTH_FOR_TEXT = 40;

  void drawIlsSymbol(const map::MapIls& ils, bool fast);

};

#endif // LITTLENAVMAP_MAPPAINTERILS_H
