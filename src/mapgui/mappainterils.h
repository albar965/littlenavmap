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

#ifndef LITTLENAVMAP_MAPPAINTERILS_H
#define LITTLENAVMAP_MAPPAINTERILS_H

#include "mapgui/mappainter.h"

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
  MapPainterIls(MapWidget *mapWidget, MapScale *mapScale);
  virtual ~MapPainterIls();

  virtual void render(PaintContext *context) override;

private:
  /* Fixed value that is used when writing the database. See atools::fs::db::IlsWriter */
  static Q_DECL_CONSTEXPR int FEATHER_LEN_NM = 9;
  static Q_DECL_CONSTEXPR int MIN_LENGHT_FOR_TEXT = 40;

  void drawIlsSymbol(const PaintContext *context, const map::MapIls& ils);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
