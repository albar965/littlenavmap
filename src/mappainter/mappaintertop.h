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

#ifndef LNM_MAPPAINTERTOP_H
#define LNM_MAPPAINTERTOP_H

#include "mappainter/mappainter.h"

/*
 * Draws navigation items and OSM copyright on top of all map features.
 */
class MapPainterTop :
  public MapPainter
{
public:
  MapPainterTop(MapPaintWidget *mapWidgetParam, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterTop() override;

  virtual void render() override;

private:
  /* Highlight click/touch areas */
  void drawTouchMarks(int lineSize, int areaSize);
  void drawTouchRegions(int areaSize);

  /* Draw navigation icons into the corners */
  void drawTouchIcons(int iconSize);

  /* Paint message into the right bottom corner */
  void paintCopyright();

};

#endif // LNM_MAPPAINTERTOP_H
