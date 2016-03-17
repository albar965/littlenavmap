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

#ifndef MAPPAINTERNAV_H
#define MAPPAINTERNAV_H

#include "mapgui/mappainter.h"

#include "mapquery.h"

class SymbolPainter;

class MapPainterNav :
  public MapPainter
{
public:
  MapPainterNav(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg);
  virtual ~MapPainterNav();

  virtual void paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                     Marble::ViewportParams *viewport, maptypes::MapObjectTypes objectTypes) override;

private:
  SymbolPainter *symbolPainter;

};

#endif // MAPPAINTERAIRPORT_H
