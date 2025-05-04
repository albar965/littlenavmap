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

#ifndef LITTLENAVMAP_SYMBOLPAINTER_H
#define LITTLENAVMAP_SYMBOLPAINTER_H

#include "options/optiondata.h"

#include "common/mapflags.h"

#include <QColor>
#include <QIcon>
#include <QCoreApplication>
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
struct MapAirway;
struct MapHelipad;
struct MapAirportMsa;
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
  /* Create icons for tooltips, table views and more. Size is pixel. */
  static QIcon createAirportIcon(const map::MapAirport& airport, int size);
  static QIcon createAirportWeatherIcon(const atools::fs::weather::MetarParser& metar, int size);
  static QIcon createVorIcon(const map::MapVor& vor, int size, bool darkMap);
  static QIcon createNdbIcon(int size, bool darkMap);
  static QIcon createAirwayIcon(const map::MapAirway& airway, int size, bool darkMap);
  static QIcon createWaypointIcon(int size, const QColor& color = QColor());
  static QIcon createUserpointIcon(int size);
  static QIcon createProcedurePointIcon(int size);
  static QIcon createAirspaceIcon(const map::MapAirspace& airspace, int size, int lineThickness, int transparency);
  static QIcon createProcedurePreviewIcon(const QColor& color, int size);
  static QIcon createHelipadIcon(const map::MapHelipad& helipad, int size);
  static QIcon createAircraftTrailIcon(int size, const QPen& pen);

  /* Scale is not pixel size but a factor related for font size */
  static QIcon createAirportMsaIcon(const map::MapAirportMsa& airportMsa, const QFont& font, float symbolScale, int *actualSize = nullptr);

  /* Airport symbol. For airport diagram use a transparent text background */
  void drawAirportSymbol(QPainter *painter, const map::MapAirport& airport, float x, float y, float size,
                         bool isAirportDiagram, bool fast, bool addonHighlight);
  void drawAirportText(QPainter *painter, const map::MapAirport& airport, float x, float y,
                       optsd::DisplayOptionsAirport dispOpts, textflags::TextFlags flags, float size, bool diagram,
                       int maxTextLength, textatt::TextAttributes atts = textatt::NONE);

  /* Waypoint symbol. Can use a different color for invalid waypoints that were not found in the database */
  void drawWaypointSymbol(QPainter *painter, const QColor& col, float x, float y, float size, bool fill);

  /* Waypoint symbol. Can use a different color for invalid waypoints that were not found in the database */
  void drawAirportWeather(QPainter *painter, const atools::fs::weather::MetarParser& metar,
                          float x, float y, float size, bool windPointer, bool windBarbs, bool fast);

  /* Wind arrow */
  void drawWindPointer(QPainter *painter, float x, float y, float size, float dir);

  /* Draw large symbol with sectors and labels. MSA circle with bearings and altitude */
  void drawAirportMsa(QPainter *painter, const map::MapAirportMsa& airportMsa, float x, float y, float size, float symbolScale, bool header,
                      bool transparency, bool fast);

  /* Aircraft track */
  void drawTrackLine(QPainter *painter, float x, float y, int size, float dir);

  /* Waypoint texts have no background excepts for flight plan */
  void drawWaypointText(QPainter *painter, const map::MapWaypoint& wp, float x, float y,
                        textflags::TextFlags flags, float size, bool fill, textatt::TextAttributes atts = textatt::NONE,
                        const QStringList *addtionalText = nullptr);

  /* VOR with large size has a ring with compass ticks. For VORs part of the route the interior is filled.  */
  void drawVorSymbol(QPainter *painter, const map::MapVor& vor, float x, float y, float size, float sizeLarge, bool routeFill,
                     bool fast, bool darkMap);

  /* VOR texts have no background excepts for flight plan */
  void drawVorText(QPainter *painter, const map::MapVor& vor, float x, float y, textflags::TextFlags flags,
                   float size, bool fill, bool darkMap, textatt::TextAttributes atts = textatt::NONE,
                   const QStringList *addtionalText = nullptr);

  /* NDB with dotted rings or solid rings depending on size. For NDBs part of the route the interior is filled.  */
  void drawNdbSymbol(QPainter *painter, float x, float y, float size, bool routeFill, bool fast, bool darkMap);

  /* NDB texts have no background excepts for flight plan */
  void drawNdbText(QPainter *painter, const map::MapNdb& ndb, float x, float y, textflags::TextFlags flags,
                   float size, bool fill, bool darkMap, textatt::TextAttributes atts = textatt::NONE,
                   const QStringList *addtionalText = nullptr);

  void drawMarkerSymbol(QPainter *painter, const map::MapMarker& marker, float x, float y, float size, bool fast);

  static void drawHelipadSymbol(QPainter *painter, const map::MapHelipad& helipad, float x, float y, float w, float h, bool fast);

  /* User defined flight plan waypoint. Green rect. */
  void drawUserpointSymbol(QPainter *painter, float x, float y, float size, bool routeFill);

  /* Circle for approach points which are not navaids */
  void drawProcedureSymbol(QPainter *painter, float x, float y, float size, bool routeFill);

  /* Circle for flight plan waypoints */
  void drawLogbookPreviewSymbol(QPainter *painter, float x, float y, float size);

  /* Maltese cross to indicate FAF on the map and ring to indicate fly over*/
  void drawProcedureUnderlay(QPainter *painter, float x, float y, float size, bool flyover, bool faf);

  /* Flyover underlay */
  void drawProcedureFlyover(QPainter *painter, float x, float y, float size);

  /* Maltese cross to indicate FAF on the map */
  void drawProcedureFaf(QPainter *painter, float x, float y, float size);

  /* Draw a custom text box */
  void textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
               textatt::TextAttributes atts = textatt::NONE,
               int transparency = 255, const QColor& backgroundColor = QColor());
  void textBoxF(QPainter *painter, QStringList texts, QPen textPen, float x, float y,
                textatt::TextAttributes atts = textatt::NONE,
                int transparency = 255, const QColor& backgroundColor = QColor());

  /* Get dimensions of a custom text box */
  QRectF textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts);

  /* Upper level winds */
  void drawWindBarbs(QPainter *painter, float wind, float gust, float dir, float x, float y, float size,
                     bool windBarbs, bool altWind, bool route, bool fast) const;

  /* Move coordinates by size based on text placement attributes */
  void adjustPos(float& x, float& y, float size, textatt::TextAttributes atts);

private:
  QStringList airportTexts(optsd::DisplayOptionsAirport dispOpts, textflags::TextFlags flags,
                           const map::MapAirport& airport, int maxTextLength);
  const QPixmap *windPointerFromCache(int size);
  const QPixmap *trackLineFromCache(int size);

  QCache<int, QPixmap> windPointerPixmaps, trackLinePixmaps;
  static void prepareForIcon(QPainter& painter);

  void drawWindBarbs(QPainter *painter, const atools::fs::weather::MetarParser& parsedMetar, float x, float y,
                     float size, bool windBarbs, bool altWind, bool route, bool fast) const;

  QVector<int> calculateWindBarbs(float& lineLength, float lineWidth, float wind, bool useBarb50) const;
  void drawBarbFeathers(QPainter *painter, const QVector<int>& barbs, float lineLength, float barbLength5,
                        float barbLength10, float barbLength50, float barbStep) const;

  static int airportMsaSize(QPainter *painter, const map::MapAirportMsa& airportMsa, float sizeFactor, bool drawDetails);

};

#endif // LITTLENAVMAP_SYMBOLPAINTER_H
