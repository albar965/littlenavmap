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

#ifndef SYMBOLPAINTER_H
#define SYMBOLPAINTER_H

#include <QColor>

class QPainter;

namespace maptypes {
struct MapAirport;

struct MapWaypoint;

struct MapVor;

struct MapNdb;

struct MapMarker;

}

class SymbolPainter
{
public:
  SymbolPainter();

  void drawAirportSymbol(QPainter *painter, const maptypes::MapAirport& ap, int x, int y, int size,
                         bool isAirportDiagram, bool fast);

  void drawWaypointSymbol(QPainter *painter, const maptypes::MapWaypoint& wp, int x, int y, int size,
                          bool fast);
  void drawVorSymbol(QPainter *painter, const maptypes::MapVor& vor, int x, int y, int size, bool fast,
                     int largeSize);
  void drawNdbSymbol(QPainter *painter, const maptypes::MapNdb& ndb, int x, int y, int size, bool fast);
  void drawMarkerSymbol(QPainter *painter, const maptypes::MapMarker& marker, int x, int y, int size,
                        bool fast);

private:
};

#endif // SYMBOLPAINTER_H
