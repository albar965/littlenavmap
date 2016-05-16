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
#include <QIcon>

class QPainter;
class QPen;

namespace Marble {
class GeoPainter;
}

namespace maptypes {
struct MapAirport;

struct MapWaypoint;

struct MapVor;

struct MapNdb;

struct MapMarker;

}

namespace textflags {
enum TextFlag
{
  NONE = 0x00,
  IDENT = 0x01,
  TYPE = 0x02,
  FREQ = 0x04,
  NAME = 0x08,
  MORSE = 0x10,
  INFO = 0x20,
  ROUTE_TEXT = 0x40,
  ABS_POS = 0x80,
  ALL = 0xff
};

Q_DECLARE_FLAGS(TextFlags, TextFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(textflags::TextFlags);
}

namespace textatt {
enum TextAttribute
{
  NONE = 0x00,
  BOLD = 0x01,
  ITALIC = 0x02,
  UNDERLINE = 0x04,
  RIGHT = 0x08,
  LEFT = 0x10,
  CENTER = 0x20,
  ROUTE_BG_COLOR = 0x40
};

Q_DECLARE_FLAGS(TextAttributes, TextAttribute);
Q_DECLARE_OPERATORS_FOR_FLAGS(TextAttributes);
}

class SymbolPainter
{
public:
  SymbolPainter();

  QIcon createAirportIcon(const maptypes::MapAirport& airport, int size);
  QIcon createVorIcon(const maptypes::MapVor& vor, int size);
  QIcon createNdbIcon(const maptypes::MapNdb& ndb, int size);
  QIcon createWaypointIcon(const maptypes::MapWaypoint& waypoint, int size);
  QIcon createUserpointIcon(int size);

  void drawAirportSymbol(QPainter *painter, const maptypes::MapAirport& ap, int x, int y, int size,
                         bool isAirportDiagram, bool fast);

  void drawWaypointSymbol(QPainter *painter, const maptypes::MapWaypoint& wp, const QColor& col, int x, int y,
                          int size, bool fill, bool fast);
  void drawVorSymbol(QPainter *painter, const maptypes::MapVor& vor, int x, int y, int size, bool routeFill,
                     bool fast, int largeSize);
  void drawNdbSymbol(QPainter *painter, const maptypes::MapNdb& ndb, int x, int y, int size, bool routeFill,
                     bool fast);
  void drawMarkerSymbol(QPainter *painter, const maptypes::MapMarker& marker, int x, int y, int size,
                        bool fast);

  void textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
               textatt::TextAttributes atts = textatt::NONE, int transparency = 255);

  void drawWaypointText(QPainter *painter, const maptypes::MapWaypoint& wp, int x, int y,
                        textflags::TextFlags flags, int size, bool fill, bool fast);

  void drawVorText(QPainter *painter, const maptypes::MapVor& vor, int x, int y, textflags::TextFlags flags,
                   int size, bool fill, bool fast);

  void drawNdbText(QPainter *painter, const maptypes::MapNdb& ndb, int x, int y, textflags::TextFlags flags,
                   int size, bool fill, bool fast);

  void drawAirportText(QPainter *painter, const maptypes::MapAirport& airport, int x, int y,
                       textflags::TextFlags flags, int size, bool diagram, bool fill,
                       bool fast);

  void drawUserpointSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast);

  void drawAircraftSymbol(QPainter *painter, int x, int y, int size);

private:
  QStringList airportTexts(textflags::TextFlags flags, const maptypes::MapAirport& airport);

  QColor iconBackground;
};

#endif // SYMBOLPAINTER_H
