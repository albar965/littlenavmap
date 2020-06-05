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

#ifndef LITTLENAVMAP_SYMBOLPAINTER_H
#define LITTLENAVMAP_SYMBOLPAINTER_H

#include "options/optiondata.h"

#include "common/mapflags.h"

#include <QColor>
#include <QIcon>
#include <QApplication>
#include <QCache>

namespace atools {
namespace fs {
namespace weather {
class Metar;
class MetarParser;
}
}
}

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
  QIcon createAirportWeatherIcon(const atools::fs::weather::Metar& metar, int size);
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
                       optsd::DisplayOptions dispOpts, textflags::TextFlags flags, int size, bool diagram,
                       int maxTextLength);

  /* Waypoint symbol. Can use a different color for invalid waypoints that were not found in the database */
  void drawWaypointSymbol(QPainter *painter, const QColor& col, int x, int y, int size, bool fill);

  /* Waypoint symbol. Can use a different color for invalid waypoints that were not found in the database */
  void drawAirportWeather(QPainter *painter, const atools::fs::weather::Metar& metar,
                          float x, float y, float size, bool windPointer, bool windBarbs, bool fast);

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
  void drawUserpointSymbol(QPainter *painter, int x, int y, int size, bool routeFill);

  /* Circle for approach points which are not navaids */
  void drawProcedureSymbol(QPainter *painter, int x, int y, int size, bool routeFill);

  /* Circle for flight plan waypoints */
  void drawLogbookPreviewSymbol(QPainter *painter, int x, int y, int size);

  /* Maltese cross to indicate FAF on the map and ring to indicate fly over*/
  void drawProcedureUnderlay(QPainter *painter, int x, int y, int size, bool flyover, bool faf);

  /* Flyover underlay */
  void drawProcedureFlyover(QPainter *painter, int x, int y, int size);

  /* Maltese cross to indicate FAF on the map */
  void drawProcedureFaf(QPainter *painter, int x, int y, int size);

  /* Draw a custom text box */
  void textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
               textatt::TextAttributes atts = textatt::NONE,
               int transparency = 255, const QColor& backgroundColor = QColor());
  void textBoxF(QPainter *painter, const QStringList& texts, const QPen& textPen, float x, float y,
                textatt::TextAttributes atts = textatt::NONE,
                int transparency = 255, const QColor& backgroundColor = QColor());

  /* Get dimensions of a custom text box */
  QRect textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts);

  /* Upper level winds */
  void drawWindBarbs(QPainter *painter, float wind, float gust, float dir, float x, float y, float size,
                     bool windBarbs, bool altWind, bool route, bool fast) const;

private:
  QStringList airportTexts(optsd::DisplayOptions dispOpts, textflags::TextFlags flags,
                           const map::MapAirport& airport, int maxTextLength);
  const QPixmap *windPointerFromCache(int size);
  const QPixmap *trackLineFromCache(int size);

  QCache<int, QPixmap> windPointerPixmaps, trackLinePixmaps;
  void prepareForIcon(QPainter& painter);

  void drawWindBarbs(QPainter *painter, const atools::fs::weather::MetarParser& parsedMetar, float x, float y,
                     float size, bool windBarbs, bool altWind, bool route, bool fast) const;

  QVector<int> calculateWindBarbs(float& lineLength, float lineWidth, float wind, bool useBarb50) const;
  void drawBarbFeathers(QPainter *painter, const QVector<int>& barbs, float lineLength, float barbLength5,
                        float barbLength10, float barbLength50, float barbStep) const;

};

#endif // LITTLENAVMAP_SYMBOLPAINTER_H
