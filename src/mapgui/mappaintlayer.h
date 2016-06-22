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

#ifndef LITTLENAVMAP_MAPPAINTLAYER_H
#define LITTLENAVMAP_MAPPAINTLAYER_H

#include "mapgui/mappainter.h"

#include <QPen>

#include <marble/LayerInterface.h>

namespace Marble {
class GeoPainter;
class GeoSceneLayer;
class ViewportParams;
}

class MapPainter;
class MapLayer;
class MapWidget;
class MapLayerSettings;
class MapScale;
class MapPainterAirport;
class MapPainterNav;
class MapPainterIls;
class MapPainterMark;
class MapPainterRoute;
class MapPainterAircraft;

class MapPaintLayer :
  public Marble::LayerInterface
{
public:
  MapPaintLayer(MapWidget *widget, MapQuery *mapQueries);
  virtual ~MapPaintLayer();

  void preDatabaseLoad();
  void postDatabaseLoad();

  const MapLayer *getMapLayer() const
  {
    return mapLayer;
  }

  const MapLayer *getMapLayerEffective() const
  {
    return mapLayerEffective;
  }

  void setShowMapFeatures(maptypes::MapObjectTypes type, bool show);
  void setDetailFactor(int factor);

  maptypes::MapObjectTypes getShownMapFeatures() const
  {
    return objectTypes;
  }

  const MapScale *getMapScale() const
  {
    return mapScale;
  }

  void routeChanged();

private:
  void initLayers();
  void updateLayers();

  // Implemented from LayerInterface
  virtual QStringList renderPosition() const override
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(Marble::GeoPainter *painter, Marble::ViewportParams *viewport,
                      const QString& renderPos = "NONE", Marble::GeoSceneLayer *layer = nullptr) override;

  /* Do not show anything at all above this limit */
  static Q_DECL_CONSTEXPR float DISTANCE_CUT_OFF_LIMIT = 4000.f;

  QSet<ForcePaintType> forcePaint;

  maptypes::MapObjectTypes objectTypes;
  int detailFactor = 10;

  bool databaseLoadStatus = false;
  MapPainterAirport *mapPainterAirport;
  MapPainterNav *mapPainterNav;
  MapPainterIls *mapPainterIls;
  MapPainterMark *mapPainterMark;
  MapPainterRoute *mapPainterRoute;
  MapPainterAircraft *mapPainterAircraft;

  MapQuery *mapQuery = nullptr;
  MapScale *mapScale = nullptr;
  MapLayerSettings *layers = nullptr;
  MapWidget *mapWidget = nullptr;
  const MapLayer *mapLayer = nullptr, *mapLayerEffective = nullptr;

};

#endif // LITTLENAVMAP_MAPPAINTLAYER_H
