/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
class MapPainterMsa;
class MapPainterAirspace;
class MapPainterNav;
class MapPainterIls;
class MapPainterMark;
class MapPainterTop;
class MapPainterRoute;
class MapPainterAircraft;
class MapPainterTrail;
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
  MapPaintLayer(MapPaintWidget *widget);
  virtual ~MapPaintLayer() override;

  MapPaintLayer(const MapPaintLayer& other) = delete;
  MapPaintLayer& operator=(const MapPaintLayer& other) = delete;

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
  void setShowMapObject(map::MapTypes type, bool show);
  void setShowMapObjectDisplay(map::MapDisplayTypes type, bool show);
  void setShowAirspaces(map::MapAirspaceFilter types);
  void setShowMapObjects(map::MapTypes type, map::MapTypes mask);

  /* Changes the detail factor (range 5-15 default is 10 */
  void setDetailLevel(int level);

  int getDetailLevel() const
  {
    return detailLevel;
  }

  /* Get all shown map objects like airports, VOR, NDB, etc. */
  map::MapTypes getShownMapTypes() const
  {
    return objectTypes;
  }

  /* Additional types like wind barbs or minimum altitude grid */
  map::MapDisplayTypes getShownMapDisplayTypes() const
  {
    return objectDisplayTypes;
  }

  /* Adjusted by layer visibility */
  map::MapAirspaceFilter getShownAirspacesTypesByLayer() const;

  /* Flags for airspace having labels */
  map::MapAirspaceTypes getShownAirspaceTextsByLayer() const;

  const map::MapAirspaceFilter& getShownAirspaces() const
  {
    return airspaceTypes;
  }

  /* Get the map scale that allows simple distance approximations for screen coordinates */
  const MapScale *getMapScale() const
  {
    return mapScale;
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

  bool isObjectOverflow() const
  {
    return context.isObjectOverflow();
  }

  int getObjectCount() const
  {
    return context.getObjectCount();
  }

  bool isQueryOverflow() const
  {
    return context.isQueryOverflow();
  }

  void initQueries();
  void updateLayers();

  int getShownMinimumRunwayFt() const
  {
    return minimumRunwayLenghtFt;
  }

  void setShowMinimumRunwayFt(int value)
  {
    minimumRunwayLenghtFt = value;
  }

  /* No drawing at all and not map interactions except moving and zooming if true.
   * Limit depends on projection. */
  bool noRender() const;

  void dumpMapLayers() const;

  /* Airports actually drawn having parking spots which require tooltips and more */
  const QSet<int>& getShownDetailAirportIds() const
  {
    return shownDetailAirportIds;
  }

private:
  void initMapLayerSettings();

  /* Implemented from LayerInterface: We  draw above all but below user tools */
  virtual QStringList renderPosition() const override
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(Marble::GeoPainter *painter, Marble::ViewportParams *viewport,
                      const QString& renderPos = "NONE", Marble::GeoSceneLayer *layer = nullptr) override;

  /* Disable font anti-aliasing for default and painter font */
  void setNoAntiAliasFont(PaintContext *context);

  /* Restore normal font anti-aliasing for default and painter font */
  void resetNoAntiAliasFont(PaintContext *context);

  /* Map objects currently shown */
  map::MapTypes objectTypes = map::NONE;
  map::MapDisplayTypes objectDisplayTypes = map::DISPLAY_TYPE_NONE;
  map::MapAirspaceFilter airspaceTypes;
  map::MapWeatherSource weatherSource = map::WEATHER_SOURCE_SIMULATOR;
  map::MapSunShading sunShading = map::SUN_SHADING_SIMULATOR_TIME;

  /* Airports drawn having parking spots which require tooltips and more */
  QSet<int> shownDetailAirportIds;

  /* Value from toolbar */
  int minimumRunwayLenghtFt = 0;

  /* Default detail factor. Range is from 5 to 15 */
  int detailLevel = 10;

  bool databaseLoadStatus = false;

  PaintContext context;

  /* All painters */
  MapPainterAirport *mapPainterAirport;
  MapPainterMsa *mapPainterMsa;
  MapPainterAirspace *mapPainterAirspace;
  MapPainterNav *mapPainterNav;
  MapPainterIls *mapPainterIls;
  MapPainterMark *mapPainterMark;
  MapPainterRoute *mapPainterRoute;
  MapPainterAircraft *mapPainterAircraft;
  MapPainterTrail *mapPainterTrack;
  MapPainterTop *mapPainterTop;
  MapPainterShip *mapPainterShip;
  MapPainterUser *mapPainterUser;
  MapPainterAltitude *mapPainterAltitude;
  MapPainterWeather *mapPainterWeather;
  MapPainterWind *mapPainterWind;

  MapScale *mapScale = nullptr;
  MapLayerSettings *layers = nullptr;
  MapPaintWidget *mapPaintWidget = nullptr;
  const MapLayer *mapLayer = nullptr, *mapLayerRoute = nullptr, *mapLayerEffective = nullptr;
  bool verbose = false, verboseDraw = false;
  QFont::StyleStrategy savedFontStrategy, savedDefaultFontStrategy;

};

#endif // LITTLENAVMAP_MAPPAINTLAYER_H
