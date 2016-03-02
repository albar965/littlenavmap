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

class NavMapWidget;

class MapPaintLayer :
  public Marble::LayerInterface
{
public:
  MapPaintLayer(NavMapWidget *widget, atools::sql::SqlDatabase *sqlDb);

  // Implemented from LayerInterface
  virtual QStringList renderPosition() const override
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(Marble::GeoPainter *painter, Marble::ViewportParams *viewport,
                      const QString& renderPos = "NONE", Marble::GeoSceneLayer *layer = nullptr) override;

  const MapAirport *getAirportAtPos(int xs, int ys);

  void paintMark(Marble::ViewportParams *viewport, Marble::GeoPainter *painter);

  void airportSymbol(Marble::GeoPainter *painter, const MapAirport& ap, int size, int x, int y);

private:
  QPen textBackgroundPen, textPen, markBackPen, markFillPen;
  QColor toweredAirportColor, unToweredAirportColor, airportEmptyColor;

  QList<MapAirport> ap;
  NavMapWidget *navMapWidget;
  atools::sql::SqlDatabase *db;

  void textBox(Marble::ViewportParams *viewport, Marble::GeoPainter *painter, const QStringList& texts,
               const QPen& pen,
               int x,
               int y);
  QColor& colorForAirport(const MapAirport& ap);

};

#endif // MAPPAINTLAYER_H
