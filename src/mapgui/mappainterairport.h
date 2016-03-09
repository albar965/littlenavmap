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

#ifndef MAPPAINTERAIRPORT_H
#define MAPPAINTERAIRPORT_H

#include "mapgui/mappainter.h"

#include "mapquery.h"

class SymbolPainter;

class MapPainterAirport :
  public MapPainter
{
public:
  MapPainterAirport(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale);
  virtual ~MapPainterAirport();

  virtual void paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                     Marble::ViewportParams *viewport) override;

  MapAirport getAirportAtPos(int xs, int ys);

private:
  QColor colorForSurface(const QString& surface);
  QColor& colorForAirport(const MapAirport& ap);

  QColor airportDetailBackColor, taxiwayNameColor, runwayOutlineColor, runwayOffsetColor, parkingOutlineColor,
         helipadOutlineColor, activeTowerColor, activeTowerOutlineColor, inactiveTowerColor,
         inactiveTowerOutlineColor, darkParkingTextColor, brightParkingTextColor, towerTextColor,
         runwayDimsTextColor, airportSymbolFillColor, transparentTextBoxColor, textBoxColor;

  void airportSymbol(Marble::GeoPainter *painter, const MapAirport& ap, int x, int y,
                     const MapLayer *mapLayer,
                     bool fast);
  void textBox(Marble::GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
               const QPen& pen, int x, int y, bool transparent);

  void airportDiagram(const MapLayer *mapLayer, Marble::GeoPainter *painter, const MapAirport& airport);

  void runwayCoords(const QList<MapRunway> *rw, QList<QPoint> *centers, QList<QRect> *rects,
                    QList<QRect> *innerRects, QList<QRect> *backRects);

  void airportSymbolOverview(Marble::GeoPainter *painter, const MapAirport& ap, const MapLayer *mapLayer,
                             bool fast);

  QList<MapAirport> airports;

  QString parkingName(const QString& name);

  QColor colorForParkingType(const QString& type);
  QStringList airportTexts(const MapLayer *mapLayer, const MapAirport& airport);

  SymbolPainter *symbolPainter;
};

#endif // MAPPAINTERAIRPORT_H
