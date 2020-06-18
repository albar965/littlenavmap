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

#ifndef LITTLENAVMAP_MAPPAINTLAYER_H
#define LITTLENAVMAP_MAPPAINTLAYER_H

#include "mappainter/mappainter.h"

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
class MapPainterAirspace;
class MapPainterNav;
class MapPainterIls;
class MapPainterMark;
class MapPainterTop;
class MapPainterRoute;
class MapPainterAircraft;
class MapPainterTrack;
class MapPainterShip;
class MapPainterUser;
class MapPainterAltitude;
class MapPainterWeather;
class MapPainterWind;
class MapPaintWidget;

/*
 * Implements the Marble layer interface that paints upon the Marble map. Contains all painter instances
 * and calls them in order for each paint event.
 */
class MapPaintLayer :
  public Marble::LayerInterface
{
public:
  MapPaintLayer(MapPaintWidget *widget, MapQuery *mapQueries);
  virtual ~MapPaintLayer() override;

  void copySettings(const MapPaintLayer& other);

  /* Sets databaseLoadStatus to avoid painting while database is offline */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Get the current map layer for the zoom distance and detail level */
  const MapLayer *getMapLayer() const
  {
    return mapLayer;
  }

  /* Get the current map layer for the zoom distance. This layer is independent of any detail level changes */
  const MapLayer *getMapLayerEffective() const
  {
    return mapLayerEffective;
  }

  /* Set the flags for map objects on or off depending on value show. Does not repaint */
  void setShowMapObjects(map::MapObjectTypes type, bool show);
  void setShowMapObjectsDisplay(map::MapObjectDisplayTypes type, bool show);
  void setShowAirspaces(map::MapAirspaceFilter types);

  /* Changes the detail factor (range 5-15 default is 10 */
  void setDetailFactor(int factor);

  int getDetailFactor() const
  {
    return detailFactor;
  }

  /* Get all shown map objects like airports, VOR, NDB, etc. */
  map::MapObjectTypes getShownMapObjects() const
  {
    return objectTypes;
  }

  /* Additional types like wind barbs or minimum altitude grid */
  map::MapObjectDisplayTypes getShownMapObjectDisplayTypes() const
  {
    return objectDisplayTypes;
  }

  /* Adjusted by layer visibility */
  map::MapAirspaceFilter getShownAirspacesTypesByLayer() const;

  map::MapAirspaceFilter getShownAirspaces() const
  {
    return airspaceTypes;
  }

  /* Get the map scale that allows simple distance approximations for screen coordinates */
  const MapScale *getMapScale() const
  {
    return mapScale;
  }

  int getOverflow() const
  {
    return overflow;
  }

  map::MapWeatherSource getWeatherSource() const
  {
    return weatherSource;
  }

  void setWeatherSource(const map::MapWeatherSource& value)
  {
    weatherSource = value;
  }

  map::MapSunShading getSunShading() const
  {
    return sunShading;
  }

  void setSunShading(const map::MapSunShading& value)
  {
    sunShading = value;
  }

private:
  void initMapLayerSettings();
  void updateLayers();

  /* Implemented from LayerInterface: We  draw above all but below user tools */
  virtual QStringList renderPosition() const override
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(Marble::GeoPainter *painter, Marble::ViewportParams *viewport,
                      const QString& renderPos = "NONE", Marble::GeoSceneLayer *layer = nullptr) override;

  /* Map objects currently shown */
  map::MapObjectTypes objectTypes = map::NONE;
  map::MapObjectDisplayTypes objectDisplayTypes = map::DISPLAY_TYPE_NONE;
  map::MapAirspaceFilter airspaceTypes;
  map::MapWeatherSource weatherSource = map::WEATHER_SOURCE_SIMULATOR;
  map::MapSunShading sunShading = map::SUN_SHADING_SIMULATOR_TIME;

  /* Default detail factor. Range is from 5 to 15 */
  int detailFactor = 10;

  bool databaseLoadStatus = false;

  /* All painters */
  MapPainterAirport *mapPainterAirport;
  MapPainterAirspace *mapPainterAirspace;
  MapPainterNav *mapPainterNav;
  MapPainterIls *mapPainterIls;
  MapPainterMark *mapPainterMark;
  MapPainterRoute *mapPainterRoute;
  MapPainterAircraft *mapPainterAircraft;
  MapPainterTrack *mapPainterTrack;
  MapPainterTop *mapPainterTop;
  MapPainterShip *mapPainterShip;
  MapPainterUser *mapPainterUser;
  MapPainterAltitude *mapPainterAltitude;
  MapPainterWeather *mapPainterWeather;
  MapPainterWind *mapPainterWind;

  /* Database source */
  MapQuery *mapQuery = nullptr;

  MapScale *mapScale = nullptr;
  MapLayerSettings *layers = nullptr;
  MapPaintWidget *mapWidget = nullptr;
  const MapLayer *mapLayer = nullptr, *mapLayerEffective = nullptr;
  int overflow = 0;

};

#endif // LITTLENAVMAP_MAPPAINTLAYER_H
