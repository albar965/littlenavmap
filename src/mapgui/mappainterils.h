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

#ifndef MAPPAINTERILS_H
#define MAPPAINTERILS_H

#include "mapgui/mappainter.h"

#include "mapquery.h"

class SymbolPainter;

class MapPainterIls :
  public MapPainter
{
public:
  MapPainterIls(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale);
  virtual ~MapPainterIls();

  virtual void paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                     Marble::ViewportParams *viewport) override;

private:
  SymbolPainter *symbolPainter;
  void drawIlsSymbol(Marble::GeoPainter *painter, const MapIls& ils, int x, int y, const MapLayer* mapLayer,
                     bool fast);

};

#endif // MAPPAINTERAIRPORT_H
