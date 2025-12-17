/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPPAINTERAIRPORT_H
#define LITTLENAVMAP_MAPPAINTERAIRPORT_H

#include "mappainter/mappainter.h"

class SymbolPainter;

namespace map {

struct MapParking;
struct MapAirport;
struct MapApron;
struct MapRunway;
}

class AirportPaintData;
class RunwayPaintData;

/*
 * Draws airport symbols, runway overview and complete airport diagram. Airport details are also drawn for
 * the flight plan.
 */
class MapPainterAirport :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterAirport(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterAirport() override;

  /* Needs call of collectVisibleAirports() before */
  virtual void render() override;

private:
  /* Pre-calculates visible airports for render() and fills visibleAirports.
   * visibleAirportIds gets all idents of shown airports */
  void collectVisibleAirports(QList<AirportPaintData>& visibleAirports);

  void drawAirportSymbol(const map::MapAirport& airport, float x, float y, float size);

  /* Draws the full airport diagram including runway, taxiways, apron, parking and more */
  void drawAirportDiagram(const map::MapAirport& airport);

  /* Draws the full white airport diagram background including runway, taxiways, apron, parking and more */
  void drawAirportDiagramBackground(const map::MapAirport& airport);

  /* Draw airport runway overview as in VFR maps (runways with white fill) */
  void drawAirportSymbolOverview(const map::MapAirport& airport, float x, float y, float symsize);

  /* Get all runways for this airport and calculate all runway screen coordinates */
  void runwayCoords(QList<RunwayPaintData>& runwayPaintData, const QList<map::MapRunway> *runways, bool overview);

  /* Draw simple FSX/P3D aprons */
  void drawFsApron(const map::MapApron& apron);
  void drawXplaneApron(const map::MapApron& apron, bool fast);
  QString parkingNameForSize(const map::MapParking& parking, float width);

  /* Replace or erase parking keywords */
  QString parkingReplaceKeywords(QString parkingName, bool erase);

  /* Remove space between prefix and digits */
  QString parkingCompressDigits(const QString& parkingName);

  /* Extract a single number */
  QString parkingExtractNumber(const QString& parkingName);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
