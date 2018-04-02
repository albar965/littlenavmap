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

#ifndef LITTLENAVMAP_SYMBOLPAINTER_H
#define LITTLENAVMAP_SYMBOLPAINTER_H

#include "options/optiondata.h"

#include <QColor>
#include <QIcon>
#include <QApplication>
#include <QCache>

class QPainter;
class QPen;

namespace Marble {
class GeoPainter;
}

namespace map {
struct MapAirport;

struct MapWaypoint;

struct MapVor;

struct MapNdb;

struct MapMarker;

struct MapAirspace;

}

namespace textflags {
/* Flags that determine what information is added to an icon */
enum TextFlag
{
  NONE = 0x0000,
  IDENT = 0x0001, /* Draw airport or navaid ICAO ident */
  TYPE = 0x0002, /* Draw navaid type (HIGH, MEDIUM, TERMINAL, HH, H, etc.) */
  FREQ = 0x0004, /* Draw navaid frequency */
  NAME = 0x0008,
  MORSE = 0x0010, /* Draw navaid morse code */
  INFO = 0x0020, /* Additional airport information like tower frequency, etc. */
  ROUTE_TEXT = 0x0040, /* Object is part of route */
  ABS_POS = 0x0080, /* Use absolute text positioning */
  NO_BACKGROUND = 0x0100, /* No background */
  ALL = 0x00ff
};

Q_DECLARE_FLAGS(TextFlags, TextFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(textflags::TextFlags);
}

namespace textatt {
/* Low level text attributes for custom text boxes */
enum TextAttribute
{
  NONE = 0x00,
  BOLD = 0x01,
  ITALIC = 0x02,
  UNDERLINE = 0x04,
  OVERLINE = 0x08,
  RIGHT = 0x10,
  LEFT = 0x20,
  CENTER = 0x40,
  ROUTE_BG_COLOR = 0x80 /* Use light yellow background for route objects */
};

Q_DECLARE_FLAGS(TextAttributes, TextAttribute);
Q_DECLARE_OPERATORS_FOR_FLAGS(TextAttributes);
}

/*
 * Draws all kind of map symbols and texts into an icon or a QPainter. Icons can change shape depending on size.
 * Separate functions are available for texts/captions.
 * An additional parameter "fast" is used to draw icons with less details while scrolling the map.
 * Instead of using a text collision detection text are placed on different sides of the symbols.
 */
class SymbolPainter
{
  Q_DECLARE_TR_FUNCTIONS(SymbolPainter)

public:
  /*
   * @param backgroundColor used for tooltips of table view icons
   */
  SymbolPainter();
  ~SymbolPainter();

  /* Create icons for tooltips, table views and more. Size is pixel. */
  QIcon createAirportIcon(const map::MapAirport& airport, int size);
  QIcon createVorIcon(const map::MapVor& vor, int size);
  QIcon createNdbIcon(int size);
  QIcon createWaypointIcon(int size, const QColor& color = QColor());
  QIcon createUserpointIcon(int size);
  QIcon createProcedurePointIcon(int size);
  QIcon createAirspaceIcon(const map::MapAirspace& airspace, int size);

  /* Airport symbol. For airport diagram use a transparent text background */
  void drawAirportSymbol(QPainter *painter, const map::MapAirport& airport, float x, float y, int size,
                         bool isAirportDiagram, bool fast);
  void drawAirportText(QPainter *painter, const map::MapAirport& airport, float x, float y,
                       opts::DisplayOptions dispOpts, textflags::TextFlags flags, int size, bool diagram,
                       int maxTextLength);

  /* Waypoint symbol. Can use a different color for invalid waypoints that were not found in the database */
  void drawWaypointSymbol(QPainter *painter, const QColor& col, int x, int y, int size, bool fill, bool fast);

  /* Wind arrow */
  void drawWindPointer(QPainter *painter, float x, float y, int size, float dir);

  /* Aircraft track */
  void drawTrackLine(QPainter *painter, float x, float y, int size, float dir);

  /* Waypoint texts have no background excepts for flight plan */
  void drawWaypointText(QPainter *painter, const map::MapWaypoint& wp, int x, int y,
                        textflags::TextFlags flags, int size, bool fill,
                        const QStringList *addtionalText = nullptr);

  /* VOR with large size has a ring with compass ticks. For VORs part of the route the interior is filled.  */
  void drawVorSymbol(QPainter *painter, const map::MapVor& vor, int x, int y, int size, bool routeFill,
                     bool fast, int largeSize);

  /* VOR texts have no background excepts for flight plan */
  void drawVorText(QPainter *painter, const map::MapVor& vor, int x, int y, textflags::TextFlags flags,
                   int size, bool fill, const QStringList *addtionalText = nullptr);

  /* NDB with dotted rings or solid rings depending on size. For NDBs part of the route the interior is filled.  */
  void drawNdbSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast);

  /* NDB texts have no background excepts for flight plan */
  void drawNdbText(QPainter *painter, const map::MapNdb& ndb, int x, int y, textflags::TextFlags flags,
                   int size, bool fill, const QStringList *addtionalText = nullptr);

  void drawMarkerSymbol(QPainter *painter, const map::MapMarker& marker, int x, int y, int size,
                        bool fast);

  /* User defined flight plan waypoint */
  void drawUserpointSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast);

  /* Circle for approach points which are not navaids */
  void drawProcedureSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast);

  /* Flyover underlay */
  void drawProcedureFlyover(QPainter *painter, int x, int y, int size);

  /* Simulator aircraft symbol. Only used for HTML display */
  void drawAircraftSymbol(QPainter *painter, int x, int y, int size, bool onGround);

  /* Draw a custom text box */
  void textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
               textatt::TextAttributes atts = textatt::NONE, int transparency = 255);
  void textBoxF(QPainter *painter, const QStringList& texts, const QPen& textPen, float x, float y,
                textatt::TextAttributes atts = textatt::NONE, int transparency = 255);

  /* Get dimensions of a custom text box */
  QRect textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts);

private:
  QStringList airportTexts(opts::DisplayOptions dispOpts, textflags::TextFlags flags,
                           const map::MapAirport& airport, int maxTextLength);
  const QPixmap *windPointerFromCache(int size);
  const QPixmap *trackLineFromCache(int size);

  QCache<int, QPixmap> windPointerPixmaps, trackLinePixmaps;
  void prepareForIcon(QPainter& painter);

};

#endif // LITTLENAVMAP_SYMBOLPAINTER_H
