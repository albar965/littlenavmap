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

#ifndef MAPPAINTLAYER_H
#define MAPPAINTLAYER_H

#include <marble/LayerInterface.h>
#include <QPen>
#include "mapquery.h"
namespace Marble {
class MarbleWidget;
class GeoPainter;
class GeoSceneLayer;
class ViewportParams;
}

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class MapLayer;
class NavMapWidget;
class MapLayerSettings;

class MapPaintLayer :
  public Marble::LayerInterface
{
public:
  MapPaintLayer(NavMapWidget *widget, atools::sql::SqlDatabase *sqlDb);
  virtual ~MapPaintLayer();

  const MapAirport getAirportAtPos(int xs, int ys);

private:
  MapLayerSettings *layers = nullptr;

  // Implemented from LayerInterface
  virtual QStringList renderPosition() const override
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(Marble::GeoPainter *painter, Marble::ViewportParams *viewport,
                      const QString& renderPos = "NONE", Marble::GeoSceneLayer *layer = nullptr) override;

  QPen textBackgroundPen, textPen, markBackPen, markFillPen;
  void paintMark(Marble::GeoPainter *painter);

  void airportSymbol(Marble::GeoPainter *painter, const MapAirport& ap, int x, int y,
                     const MapLayer *mapLayer,
                     bool fast);

  bool worldToScreen(const atools::geo::Pos& coords, int& x, int& y);
  bool worldToScreen(const Marble::GeoDataCoordinates& coords, int& x, int& y);

  QColor toweredAirportColor, unToweredAirportColor, airportEmptyColor;

  QList<MapAirport> airports;
  NavMapWidget *navMapWidget;
  atools::sql::SqlDatabase *db;

  void textBox(Marble::GeoPainter *painter, const MapAirport& ap, const QStringList& texts,
               const QPen& pen, int x, int y);
  QColor& colorForAirport(const MapAirport& ap);

  bool screenToWorld(int x, int y, Marble::GeoDataCoordinates& coords);

  void airportDiagram(Marble::GeoPainter *painter, const MapAirport& ap, int x, int y);

  void runwayCoords(const QList<MapRunway>& rw, QList<QPoint>& centers, QList<QRect>& rects,
                    QList<QRect>& innerRects);

  void initLayers();

};

#endif // MAPPAINTLAYER_H
