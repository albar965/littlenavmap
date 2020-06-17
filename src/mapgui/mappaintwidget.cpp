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

#include "mapgui/mappaintwidget.h"

#include "navapp.h"
#include "mappainter/mappaintlayer.h"
#include "mapgui/mapscale.h"
#include "common/maptools.h"
#include "route/route.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/maplayersettings.h"
#include "common/unit.h"
#include "common/aircrafttrack.h"
#include "mapgui/aprongeometrycache.h"

#include <QPainter>
#include <QJsonDocument>

#include <marble/MarbleLocale.h>
#include <marble/MarbleModel.h>
#include <marble/AbstractFloatItem.h>

/* If width and height of a bounding rect are smaller than this: Use show point */
const float POS_IS_POINT_EPSILON = 0.0001f;

// KM
const static double MINIMUM_DISTANCE_KM = 0.1;
const static double MAXIMUM_DISTANCE_KM = 6000.;

using namespace Marble;
using atools::geo::Rect;
using atools::geo::Pos;

MapPaintWidget::MapPaintWidget(QWidget *parent, bool visible)
  : Marble::MarbleWidget(parent), visibleWidget(visible)
{
  aircraftTrack = new AircraftTrack;

  // Set the map quality to gain speed while moving
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);
  setAnimationsEnabled(false);

  unitsUpdated();

  paintLayer = new MapPaintLayer(this, NavApp::getMapQuery());
  addLayer(paintLayer);

  screenIndex = new MapScreenIndex(this, paintLayer);

  setSunShadingDimFactor(static_cast<double>(OptionData::instance().getDisplaySunShadingDimFactor()) / 100.);
  avoidBlurredMap = OptionData::instance().getFlags2() & opts2::MAP_AVOID_BLURRED_MAP;

  // Initialize the X-Plane apron geometry cache
  apronGeometryCache = new ApronGeometryCache();
  apronGeometryCache->setViewportParams(viewport());
}

MapPaintWidget::~MapPaintWidget()
{
  removeLayer(paintLayer);

  qDebug() << Q_FUNC_INFO << "delete paintLayer";
  delete paintLayer;

  qDebug() << Q_FUNC_INFO << "delete screenIndex";
  delete screenIndex;

  delete aircraftTrack;

  delete apronGeometryCache;
}

void MapPaintWidget::copySettings(const MapPaintWidget& other)
{
  paintLayer->copySettings(*other.paintLayer);
  screenIndex->copy(*other.screenIndex);

  // Copy all MarbleWidget settings - some on demand to avoid overhead
  if(projection() != other.projection())
    setProjection(other.projection());

  if(mapThemeId() != other.mapThemeId())
    setTheme(other.mapThemeId(), other.currentThemeIndex);

  // Unused options
  // setAnimationsEnabled(other.animationsEnabled());
  // setShowDebugPolygons(other.showDebugPolygons());
  // setShowRuntimeTrace(other.showRuntimeTrace());
  // setShowBackground(other.showBackground());
  // setShowFrameRate(other.showFrameRate());
  // setShowLakes(other.showLakes());
  // setShowRivers(other.showRivers());
  // setShowBorders(other.showBorders());
  // setShowOverviewMap(other.showOverviewMap());
  // setShowScaleBar(other.showScaleBar());
  // setShowCompass(other.showCompass());
  // setShowClouds(other.showClouds());
  // setShowCityLights(other.showCityLights());
  // setLockToSubSolarPoint(other.isLockedToSubSolarPoint());
  // setSubSolarPointIconVisible(other.isSubSolarPointIconVisible());
  // setShowAtmosphere(other.showAtmosphere());
  // setShowCrosshairs(other.showCrosshairs());
  // setShowRelief(other.showRelief());

  setShowSunShading(other.showSunShading());
  setShowGrid(other.showGrid());
  setShowPlaces(other.showPlaces());
  setShowCities(other.showCities());
  setShowTerrain(other.showTerrain());
  setShowOtherPlaces(other.showOtherPlaces());
  setShowIceLayer(other.showIceLayer());

  // Adapt cache settings if changed
  if(model()->persistentTileCacheLimit() != other.model()->persistentTileCacheLimit())
    model()->setPersistentTileCacheLimit(other.model()->persistentTileCacheLimit());
  if(volatileTileCacheLimit() != other.volatileTileCacheLimit())
    setVolatileTileCacheLimit(other.volatileTileCacheLimit());

  setSunShadingDimFactor(static_cast<double>(OptionData::instance().getDisplaySunShadingDimFactor()) / 100.);

  // Copy own/internal settings
  currentThemeIndex = other.currentThemeIndex;
  *aircraftTrack = *other.aircraftTrack;
  searchMarkPos = other.searchMarkPos;
  homePos = other.homePos;
  homeDistance = other.homeDistance;
  kmlFilePaths = other.kmlFilePaths;
  mapDetailLevel = other.mapDetailLevel;
  avoidBlurredMap = other.avoidBlurredMap;

  if(hillshading != other.hillshading)
  {
    hillshading = other.hillshading;
    setPropertyValue("hillshading", hillshading);
  }

  if(size() != other.size())
    resize(other.size());
}

void MapPaintWidget::copyView(const MapPaintWidget& other)
{
  currentViewBoundingBox = other.viewport()->viewLatLonAltBox();
}

void MapPaintWidget::setTheme(const QString& theme, int index)
{
  qDebug() << "setting map theme to index" << index << theme;

  currentThemeIndex = map::MapThemeComboIndex(index);

  setThemeInternal(theme);
}

void MapPaintWidget::setThemeInternal(const QString& theme)
{
  // Ignore any overlay state signals the widget sends while switching theme
  ignoreOverlayUpdates = true;

  updateThemeUi(currentThemeIndex);

  if(currentThemeIndex < map::CUSTOM)
  {
    // Update theme specific options
    switch(currentThemeIndex)
    {
      case map::STAMENTERRAIN:
      case map::OPENSTREETMAP:
      case map::CARTODARK:
      case map::CARTOLIGHT:
      case map::SIMPLE:
      case map::PLAIN:
      case map::ATLAS:
      case map::CUSTOM:
        break;

      case map::OPENTOPOMAP:
        // Disable ugly useless ice polygons on OpenTopoMap
        // This actually does not work until the polygon files are deleted from the configuration
        setPropertyValue("ice", false);
        setShowIceLayer(false);
        break;

      case map::INVALID_THEME:
        qWarning() << "Invalid theme index" << currentThemeIndex;
        break;
    }
  }

  setMapThemeId(theme);
  updateMapObjectsShown();

  ignoreOverlayUpdates = false;

  // Show or hide overlays again
  overlayStateFromMenu();
}

void MapPaintWidget::unitsUpdated()
{
  switch(OptionData::instance().getUnitDist())
  {
    case opts::DIST_NM:
      MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
      break;
    case opts::DIST_KM:
      MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::MetricSystem);
      break;
    case opts::DIST_MILES:
      MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::ImperialSystem);
      break;
  }
}

void MapPaintWidget::optionsChanged()
{
  unitsUpdated();

  // Updated sun shadow and force a tile refresh by changing the show status again
  setSunShadingDimFactor(static_cast<double>(OptionData::instance().getDisplaySunShadingDimFactor()) / 100.);
  setShowSunShading(showSunShading());
  avoidBlurredMap = OptionData::instance().getFlags2() & opts2::MAP_AVOID_BLURRED_MAP;

  // reloadMap();
  updateCacheSizes();
  update();
}

void MapPaintWidget::styleChanged()
{
  update();
}

void MapPaintWidget::updateCacheSizes()
{
  quint64 volCacheKb = OptionData::instance().getCacheSizeMemoryMb() * 1000L;
  if(volCacheKb != volatileTileCacheLimit())
  {
    qDebug() << "Volatile cache to" << volCacheKb << "kb";
    setVolatileTileCacheLimit(volCacheKb);
  }

  quint64 persCacheKb = OptionData::instance().getCacheSizeDiskMb() * 1000L;
  if(persCacheKb != model()->persistentTileCacheLimit())
  {
    qDebug() << "Persistent cache to" << persCacheKb << "kb";
    model()->setPersistentTileCacheLimit(persCacheKb);
  }
}

void MapPaintWidget::setShowMapSunShading(bool show)
{
  setShowSunShading(show);
}

void MapPaintWidget::weatherUpdated()
{
  if(paintLayer->getShownMapObjects() | map::AIRPORT_WEATHER)
    update();
}

void MapPaintWidget::windUpdated()
{
  if(paintLayer->getShownMapObjectDisplayTypes() | map::WIND_BARBS ||
     paintLayer->getShownMapObjectDisplayTypes() | map::WIND_BARBS_ROUTE)
    update();
}

map::MapWeatherSource MapPaintWidget::getMapWeatherSource() const
{
  return paintLayer->getWeatherSource();
}

QDateTime MapPaintWidget::getSunShadingDateTime() const
{
  return model()->clockDateTime();
}

void MapPaintWidget::setSunShadingDateTime(const QDateTime& datetime)
{
  if(std::abs(datetime.toSecsSinceEpoch() - model()->clockDateTime().toSecsSinceEpoch()) > 300)
  {
    // Update only if difference more than 5 minutes
    model()->setClockDateTime(datetime);
    update();
  }
}

void MapPaintWidget::setShowMapPois(bool show)
{
  // Enable all POI stuff
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
  setShowTerrain(show);
}

void MapPaintWidget::setShowMapFeatures(map::MapObjectTypes type, bool show)
{
  bool curShow = (paintLayer->getShownMapObjects() & type) == type;
  paintLayer->setShowMapObjects(type, show);

  // Update screen coordinate caches if display options have changed

  if(type & map::AIRWAY_ALL && show != curShow)
    screenIndex->updateAirwayScreenGeometry(getCurrentViewBoundingBox());

  if(type & map::AIRSPACE && show != curShow)
    screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());

  if(type & map::ILS && show != curShow)
    screenIndex->updateIlsScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::setShowMapFeaturesDisplay(map::MapObjectDisplayTypes type, bool show)
{
  bool curShow = (paintLayer->getShownMapObjectDisplayTypes() & type) == type;
  paintLayer->setShowMapObjectsDisplay(type, show);

  // Update screen coordinate cache if display options have changed
  if(type & map::LOGBOOK_ALL && show != curShow)
    screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::setShowMapAirspaces(map::MapAirspaceFilter types)
{
  paintLayer->setShowAirspaces(types);
  updateMapVisibleUi();
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::updateMapVisibleUi() const
{
  // No-op
}

void MapPaintWidget::updateMapObjectsShown()
{
  // No-op
}

bool MapPaintWidget::checkPos(const atools::geo::Pos&)
{
  return true;
  // No-op
}

map::MapObjectTypes MapPaintWidget::getShownMapFeatures() const
{
  return paintLayer->getShownMapObjects();
}

map::MapObjectDisplayTypes MapPaintWidget::getShownMapFeaturesDisplay() const
{
  return paintLayer->getShownMapObjectDisplayTypes();
}

map::MapAirspaceFilter MapPaintWidget::getShownAirspaces() const
{
  return paintLayer->getShownAirspaces();
}

map::MapAirspaceFilter MapPaintWidget::getShownAirspaceTypesByLayer() const
{
  return paintLayer->getShownAirspacesTypesByLayer();
}

ApronGeometryCache *MapPaintWidget::getApronGeometryCache()
{
  return apronGeometryCache;
}

QString MapPaintWidget::getMapCopyright() const
{
  static const QString OSM("© OpenStreetMap contributors");
  static const QString OTM("© OpenStreetMap / OpenTopoMap contributors");
  static const QString NONE;

  switch(currentThemeIndex)
  {
    case map::OPENTOPOMAP:
      return OTM;

    case map::OPENSTREETMAP:
    case map::STAMENTERRAIN:
    case map::CARTOLIGHT:
    case map::CARTODARK:
      return OSM;

    case map::SIMPLE:
    case map::PLAIN:
    case map::ATLAS:
    case map::CUSTOM:
    case map::INVALID_THEME:
      break;
  }
  return NONE;
}

void MapPaintWidget::preDatabaseLoad()
{
  jumpBackToAircraftCancel();
  cancelDragAll();
  databaseLoadStatus = true;
  apronGeometryCache->clear();
  paintLayer->preDatabaseLoad();
}

void MapPaintWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  paintLayer->postDatabaseLoad();
  screenIndex->updateAllGeometry(getCurrentViewBoundingBox());
  update();
  updateMapVisibleUi();
}

// {
// "calibration": {
// "latitude1": 51.8425,
// "latitude2": 44.831,
// "longitude1": -8.4895,
// "longitude2": -0.7134,
// "x1": 0.1722338204592902,
// "x2": 0.541660487940095,
// "y1": 0.11333186101295643,
// "y2": 0.9629301675667267
// }
// }
QString MapPaintWidget::createAvitabJson()
{
  CoordinateConverter conv(viewport());
  Pos topLeft = conv.sToW(rect().topLeft());
  Pos bottomRight = conv.sToW(rect().bottomRight());

  if(topLeft.isValid() && bottomRight.isValid())
  {
    QJsonObject calibration;
    calibration.insert("latitude1", topLeft.getLatY());
    calibration.insert("latitude2", bottomRight.getLatY());
    calibration.insert("longitude1", topLeft.getLonX());
    calibration.insert("longitude2", bottomRight.getLonX());
    calibration.insert("x1", 0.);
    calibration.insert("x2", 1.);
    calibration.insert("y1", 0.);
    calibration.insert("y2", 1.);

    QJsonObject calibrationObj;
    calibrationObj.insert("calibration", calibration);

    QJsonDocument doc;
    doc.setObject(calibrationObj);
    return doc.toJson();
  }
  return QString();
}

void MapPaintWidget::centerRectOnMapPrecise(const atools::geo::Rect& rect, bool allowAdjust)
{
  centerRectOnMapPrecise(Marble::GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                                                  GeoDataCoordinates::Degree), allowAdjust);
}

void MapPaintWidget::centerRectOnMapPrecise(const Marble::GeoDataLatLonBox& rect, bool allowAdjust)
{
  // Marble zooms out too far - start by doing this
  centerOn(rect, false /* animated */);
  int zoomIterations = 0;

  double north = rect.north(GeoDataCoordinates::Degree), south = rect.south(GeoDataCoordinates::Degree),
         east = rect.east(GeoDataCoordinates::Degree), west = rect.west(GeoDataCoordinates::Degree);

  if(!visibleWidget)
    qDebug() << Q_FUNC_INFO << "intial zoom" << zoom() << "zoom step" << zoomStep();

  // Zoom in deeper
  zoomIn();
  zoomIn();

  // Now zoom out step by step until all points are visible - max 100 iterations
  qreal x, y;
  int step = 1;
  while((!screenCoordinates(west, north, x, y) || !screenCoordinates(east, north, x, y) ||
         !screenCoordinates(east, south, x, y) || !screenCoordinates(west, south, x, y)) &&
        (zoomIterations < 500) && (zoom() < maximumZoom()))
  {
    if(!visibleWidget)
      qDebug() << Q_FUNC_INFO << "zoom" << zoom()
               << "Viewport" << viewport()->viewLatLonAltBox().toString(GeoDataCoordinates::Degree);

    zoomViewBy(-step);
    zoomIterations++;
  }

  if(!visibleWidget)
    qDebug() << Q_FUNC_INFO << "final zoom" << zoom();

  // Fix blurryness or zoom one out after correcting by zooming in
  if((allowAdjust && avoidBlurredMap) || zoomIterations > 0)
    adjustMapDistance();
}

void MapPaintWidget::prepareDraw(int width, int height)
{
  // Issue a single drawing event to the Marble widget for initialization
  // No navaids are painted for performance reasons
  noNavPaint = true;
  getPixmap(width, height);
  noNavPaint = false;
}

QPixmap MapPaintWidget::getPixmap(int width, int height)
{
  if(width > 0 && height > 0)
    // Resize if needed - resizeEvent will be called in grab
    resize(width, height);

  return grab();
}

QPixmap MapPaintWidget::getPixmap(const QSize& size)
{
  return getPixmap(size.width(), size.height());
}

atools::geo::Pos MapPaintWidget::getCurrentViewCenterPos() const
{
  return atools::geo::Pos(centerLongitude(), centerLatitude(), distance());
}

atools::geo::Rect MapPaintWidget::getCurrentViewRect() const
{
  const GeoDataLatLonBox& box = getCurrentViewBoundingBox();
  return atools::geo::Rect(box.west(GeoDataCoordinates::Degree), box.north(GeoDataCoordinates::Degree),
                           box.east(GeoDataCoordinates::Degree), box.south(GeoDataCoordinates::Degree));
}

void MapPaintWidget::centerRectOnMap(const Marble::GeoDataLatLonBox& rect, bool allowAdjust)
{
  centerRectOnMap(Rect(rect.west(GeoDataCoordinates::Degree), rect.north(GeoDataCoordinates::Degree),
                       rect.east(GeoDataCoordinates::Degree), rect.south(GeoDataCoordinates::Degree)), allowAdjust);
}

void MapPaintWidget::centerRectOnMap(const atools::geo::Rect& rect, bool allowAdjust)
{
  if(!rect.isPoint(POS_IS_POINT_EPSILON) &&
     rect.getWidthDegree() < 180.f &&
     rect.getHeightDegree() < 180.f &&
     rect.getWidthDegree() > POS_IS_POINT_EPSILON &&
     rect.getHeightDegree() > POS_IS_POINT_EPSILON)
  {
    // Make rectangle slightly bigger to avoid waypoints hiding at the window corners
    Rect scaled(rect);
    scaled.scale(1.075f, 1.075f);

    double north = scaled.getNorth(), south = scaled.getSouth(), east = scaled.getEast(), west = scaled.getWest();
    GeoDataLatLonBox box(north, south, east, west, GeoDataCoordinates::Degree);

    // Center rectangle first
    centerOn(box, false /* animated */);

    // Correct zoom - zoom out until all points are visible ==========================
    // Needed since Marble does zoom correctly
    qreal x, y;
    // Limit iterations to avoid endless loop
    int zoomIterations = 0;
    while((!screenCoordinates(west, north, x, y) || !screenCoordinates(east, south, x, y)) &&
          (zoomIterations < 10) && (zoom() < maximumZoom()))
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "out distance NM" << atools::geo::kmToNm(distance());
#endif
      zoomOut();
      zoomIterations++;
    }

    // Correct zoom - zoom in until at least one point is not visible ==========================
    zoomIterations = 0;
    while(screenCoordinates(west, north, x, y) && screenCoordinates(east, south, x, y) &&
          (zoomIterations < 10) && (zoom() > minimumZoom()))
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "out distance NM" << atools::geo::kmToNm(distance());
#endif
      zoomIn();
      zoomIterations++;
    }

    // Fix blurryness or zoom one out after correcting by zooming in
    if((allowAdjust && avoidBlurredMap) || zoomIterations > 0)
      adjustMapDistance();
  }
  else
  {
    // Rect is a point or otherwise malformed
    centerPosOnMap(rect.getCenter());

    if(rect.getWidthDegree() < 180.f &&
       rect.getHeightDegree() < 180.f)
      setDistanceToMap(MINIMUM_DISTANCE_KM, allowAdjust);
    else
      setDistanceToMap(MAXIMUM_DISTANCE_KM, allowAdjust);
  }
}

void MapPaintWidget::adjustMapDistance()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "Adjusted view zoom FROM" << zoom() << "distance"
           << atools::geo::kmToNm(distance()) << "NM" << distance() << "KM";
#endif

  // Zoom out to next step to get a sharper map display
  zoomOut(Marble::Instant);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "Adjusted view zoom TO" << zoom() << "distance"
           << atools::geo::kmToNm(distance()) << "NM" << distance() << "KM";
#endif
}

void MapPaintWidget::centerPosOnMap(const atools::geo::Pos& pos)
{
  if(pos.isValid())
  {
    Pos normPos = pos.normalized();
    centerOn(normPos.getLonX(), normPos.getLatY(), false /* animated */);
  }
}

void MapPaintWidget::setDistanceToMap(double distanceKm, bool allowAdjust)
{
  distanceKm = std::min(std::max(distanceKm, MINIMUM_DISTANCE_KM / 2.), MAXIMUM_DISTANCE_KM);

  // KM
  setDistance(distanceKm);

  if(allowAdjust && avoidBlurredMap)
    adjustMapDistance();
}

void MapPaintWidget::showPosNotAdjusted(const atools::geo::Pos& pos, float distanceKm, bool doubleClick)
{
  if(checkPos(pos))
    showPosInternal(pos, distanceKm, doubleClick, false /* allow adjust */);
}

void MapPaintWidget::showPos(const atools::geo::Pos& pos, float distanceKm, bool doubleClick)
{
  if(checkPos(pos))
    showPosInternal(pos, distanceKm, doubleClick, true /* allow adjust */);
}

void MapPaintWidget::showPosInternal(const atools::geo::Pos& pos, float distanceKm, bool doubleClick, bool allowAdjust)
{
#if DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << pos << distanceKm << doubleClick;
#endif

  if(!pos.isValid())
  {
    qWarning() << Q_FUNC_INFO << "Invalid position";
    return;
  }

  hideTooltip();
  showAircraft(false);
  jumpBackToAircraftStart(true /* save distance too */);

  if(distanceKm == 0.f)
    // Use distance depending on double click
    distanceKm = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                               OptionData::instance().getMapZoomShowClick() :
                                               OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

  centerPosOnMap(pos);
  if(distanceKm < map::INVALID_DISTANCE_VALUE)
    setDistanceToMap(distanceKm, allowAdjust);
}

void MapPaintWidget::showRect(const atools::geo::Rect& rect, bool doubleClick)
{
#if DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << rect << doubleClick;
#endif

  if(!checkPos(rect.getCenter()))
    return;

  hideTooltip();
  showAircraft(false);
  jumpBackToAircraftStart(true /* save distance too */);

  float w = rect.getWidthDegree();
  float h = rect.getHeightDegree();

  if(!rect.isValid())
  {
    qWarning() << Q_FUNC_INFO << "invalid rectangle";
    return;
  }

  if(rect.isPoint(POS_IS_POINT_EPSILON))
    showPos(rect.getTopLeft(), 0.f, doubleClick);
  else
  {
    if(atools::almostEqual(w, 0.f, POS_IS_POINT_EPSILON))
      // Workaround for marble not being able to center certain lines
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest() - h / 2, rect.getNorth(), rect.getEast() + h / 2, rect.getSouth()));
    else if(atools::almostEqual(h, 0.f, POS_IS_POINT_EPSILON))
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest(), rect.getNorth() + w / 2, rect.getEast(), rect.getSouth() - w / 2));
    else
      // Center on rectangle
      centerRectOnMap(Rect(rect.getWest(), rect.getNorth(), rect.getEast(), rect.getSouth()));

    float distanceKm = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                                     OptionData::instance().getMapZoomShowClick() :
                                                     OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

    if(distance() < distanceKm)
      setDistanceToMap(distanceKm);
  }
}

void MapPaintWidget::showAircraft(bool centerAircraftChecked)
{
  qDebug() << Q_FUNC_INFO;

  if(!(OptionData::instance().getFlags2() & opts2::ROUTE_NO_FOLLOW_ON_MOVE) || centerAircraftChecked)
  {
    // Keep old behavior if jump back to aircraft is disabled

    // Adapt the menu item status if this method was not called by the action
    updateShowAircraftUi(centerAircraftChecked);

    if(centerAircraftChecked && screenIndex->getUserAircraft().isValid())
      centerPosOnMap(screenIndex->getUserAircraft().getPosition());
  }
}

void MapPaintWidget::changeRouteHighlights(const QList<int>& routeHighlight)
{
  screenIndex->setRouteHighlights(routeHighlight);
  update();
}

void MapPaintWidget::routeChanged(bool geometryChanged)
{
  qDebug() << Q_FUNC_INFO;

  if(geometryChanged)
  {
    cancelDragAll();
    screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());
  }
  update();
}

void MapPaintWidget::routeAltitudeChanged(float altitudeFeet)
{
  Q_UNUSED(altitudeFeet);

  if(databaseLoadStatus)
    return;

  qDebug() << Q_FUNC_INFO;
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  jumpBackToAircraftCancel();
  update();
}

void MapPaintWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  // Clear all data on disconnect
  screenIndex->updateSimData(atools::fs::sc::SimConnectData());
  updateMapVisibleUi();
  jumpBackToAircraftCancel();
  update();
}

bool MapPaintWidget::addKmlFile(const QString& kmlFile)
{
  if(loadKml(kmlFile, true))
  {
    // Add to the list of files that will be reloaded on startup
    kmlFilePaths.append(kmlFile);
    // Successfully loaded
    return true;
  }
  else
    // Loading failed
    return false;
}

void MapPaintWidget::clearKmlFiles()
{
  for(const QString& file : kmlFilePaths)
    model()->removeGeoData(file);
  kmlFilePaths.clear();
}

const atools::geo::Pos& MapPaintWidget::getProfileHighlight() const
{
  return screenIndex->getProfileHighlight();
}

void MapPaintWidget::clearSearchHighlights()
{
  screenIndex->changeSearchHighlights(map::MapSearchResult());

  screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::clearAirspaceHighlights()
{
  screenIndex->changeAirspaceHighlights(QList<map::MapAirspace>());
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::clearAirwayHighlights()
{
  screenIndex->changeAirwayHighlights(QList<QList<map::MapAirway> >());
  screenIndex->updateAirwayScreenGeometry(getCurrentViewBoundingBox());
  update();
}

bool MapPaintWidget::hasHighlights() const
{
  return !screenIndex->getSearchHighlights().isEmpty() || !screenIndex->getAirspaceHighlights().isEmpty() ||
         !screenIndex->getAirwayHighlights().isEmpty();
}

bool MapPaintWidget::hasTrackPoints() const
{
  return !aircraftTrack->isEmpty();
}

const map::MapSearchResult& MapPaintWidget::getSearchHighlights() const
{
  return screenIndex->getSearchHighlights();
}

const QList<map::MapAirspace>& MapPaintWidget::getAirspaceHighlights() const
{
  return screenIndex->getAirspaceHighlights();
}

const QList<QList<map::MapAirway> >& MapPaintWidget::getAirwayHighlights() const
{
  return screenIndex->getAirwayHighlights();
}

const proc::MapProcedureLeg& MapPaintWidget::getProcedureLegHighlights() const
{
  return screenIndex->getApproachLegHighlights();
}

const proc::MapProcedureLegs& MapPaintWidget::getProcedureHighlight() const
{
  return screenIndex->getProcedureHighlight();
}

void MapPaintWidget::changeApproachHighlight(const proc::MapProcedureLegs& approach)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << approach;
#endif

  cancelDragAll();
  screenIndex->getProcedureHighlight() = approach;
  screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());
  update();
}

/* Also clicked airspaces in the info window */
void MapPaintWidget::changeAirspaceHighlights(const QList<map::MapAirspace>& airspaces)
{
  screenIndex->changeAirspaceHighlights(airspaces);
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
  update();
}

/* Also clicked airways in the info window */
void MapPaintWidget::changeAirwayHighlights(const QList<QList<map::MapAirway> >& airways)
{
  screenIndex->changeAirwayHighlights(airways);
  screenIndex->updateAirwayScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::updateLogEntryScreenGeometry()
{
  screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::changeSearchHighlights(const map::MapSearchResult& newHighlights)
{
  screenIndex->changeSearchHighlights(newHighlights);
  screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::changeProcedureLegHighlights(const proc::MapProcedureLeg *leg)
{
  screenIndex->setApproachLegHighlights(leg);
  update();
}

void MapPaintWidget::changeProfileHighlight(const atools::geo::Pos& pos)
{
  if(pos != screenIndex->getProfileHighlight())
  {
    screenIndex->setProfileHighlight(pos);
    update();
  }
}

void MapPaintWidget::overlayStateFromMenu()
{
  // Default implementation for hidden widget - disable all overlays
  for(AbstractFloatItem *overlay : floatItems())
  {
    overlay->setVisible(false);
    overlay->hide();
  }
}

void MapPaintWidget::overlayStateToMenu()
{
  // No-op
}

void MapPaintWidget::jumpBackToAircraftStart(bool)
{
  // No-op
}

void MapPaintWidget::jumpBackToAircraftCancel()
{
  // No-op
}

const GeoDataLatLonBox& MapPaintWidget::getCurrentViewBoundingBox() const
{
  return viewport()->viewLatLonAltBox();
}

void MapPaintWidget::cancelDragAll()
{
  // No-op
}

void MapPaintWidget::hideTooltip()
{
  // No-op
}

void MapPaintWidget::handleHistory()
{
  // No-op
}

map::MapSunShading MapPaintWidget::sunShadingFromUi()
{
  // Default get internal value instead of GUI
  return paintLayer->getSunShading();
}

map::MapWeatherSource MapPaintWidget::weatherSourceFromUi()
{
  // Default get internal value instead of GUI
  return paintLayer->getWeatherSource();
}

void MapPaintWidget::updateThemeUi(int index)
{
  // No-op
  Q_UNUSED(index);
}

void MapPaintWidget::updateShowAircraftUi(bool centerAircraftChecked)
{
  // No-op
  Q_UNUSED(centerAircraftChecked);
}

const QList<int>& MapPaintWidget::getRouteHighlights() const
{
  return screenIndex->getRouteHighlights();
}

const QList<map::RangeMarker>& MapPaintWidget::getRangeRings() const
{
  return screenIndex->getRangeMarks();
}

const QList<map::DistanceMarker>& MapPaintWidget::getDistanceMarkers() const
{
  return screenIndex->getDistanceMarks();
}

const QList<map::TrafficPattern>& MapPaintWidget::getTrafficPatterns() const
{
  return screenIndex->getTrafficPatterns();
}

QList<map::TrafficPattern>& MapPaintWidget::getTrafficPatterns()
{
  return screenIndex->getTrafficPatterns();
}

const QList<map::Hold>& MapPaintWidget::getHolds() const
{
  return screenIndex->getHolds();
}

QList<map::Hold>& MapPaintWidget::getHolds()
{
  return screenIndex->getHolds();
}

const atools::fs::sc::SimConnectUserAircraft& MapPaintWidget::getUserAircraft() const
{
  return screenIndex->getUserAircraft();
}

const atools::fs::sc::SimConnectData& MapPaintWidget::getSimConnectData() const
{
  return screenIndex->getSimConnectData();
}

const QVector<atools::fs::sc::SimConnectAircraft>& MapPaintWidget::getAiAircraft() const
{
  return screenIndex->getAiAircraft();
}

void MapPaintWidget::resizeEvent(QResizeEvent *event)
{
  if(!visibleWidget)
  {
    // Avoid excessive logging on visible widget
    qDebug() << Q_FUNC_INFO << "Viewport" << viewport()->viewLatLonAltBox().toString(GeoDataCoordinates::Degree);
    qDebug() << Q_FUNC_INFO << "currentViewBoundingBox" << currentViewBoundingBox.toString(GeoDataCoordinates::Degree);
    qDebug() << Q_FUNC_INFO << event->oldSize() << event->size();
  }

  // Let parent do its thing
  MarbleWidget::resizeEvent(event);

  if(keepWorldRect)
    // Keep visible rectangle if desired - CPU intense
    centerRectOnMapPrecise(currentViewBoundingBox, adjustOnResize);
}

void MapPaintWidget::paintEvent(QPaintEvent *paintEvent)
{
  if(!visibleWidget)
  {
    // Avoid excessive logging on visible widget
    qDebug() << Q_FUNC_INFO << "Viewport" << getCurrentViewBoundingBox().toString(GeoDataCoordinates::Degree);
    qDebug() << Q_FUNC_INFO << "currentViewBoundingBox" << currentViewBoundingBox.toString(GeoDataCoordinates::Degree);
  }

  if(!active)
  {
    // No map yet - clear area
    QPainter painter(this);
    painter.fillRect(paintEvent->rect(), QGuiApplication::palette().color(QPalette::Window));
    return;
  }

  bool changed = false;
  const GeoDataLatLonBox visibleLatLonBox = getCurrentViewBoundingBox();

#ifdef DEBUG_INFORMATION_PAINT
  qDebug() << Q_FUNC_INFO << "distance"
           << atools::geo::kmToNm(distance()) << "NM" << distance() << "KM zoom" << zoom()
           << "step" << zoomStep();
#endif

  if(viewContext() == Marble::Still && (zoom() != currentZoom || visibleLatLonBox != currentViewBoundingBox))
  {
    // This paint event has changed zoom or the visible bounding box
    currentZoom = zoom();
    currentViewBoundingBox = visibleLatLonBox;

    // qDebug() << "paintEvent map view has changed zoom" << currentZoom
    // << "distance" << distance() << " (" << meterToNm(distance() * 1000.) << " km)";

    handleHistory();

    changed = true;
  }

  MarbleWidget::paintEvent(paintEvent);

  if(changed)
  {
    // Major change - update index and visible objects
    updateMapVisibleUi();
    screenIndex->updateAllGeometry(currentViewBoundingBox);
  }

  if(paintLayer->getOverflow() > 0)
    emit resultTruncated(paintLayer->getOverflow());
}

bool MapPaintWidget::loadKml(const QString& filename, bool center)
{
  if(QFile::exists(filename))
  {
    model()->addGeoDataFile(filename, 0, center && OptionData::instance().getFlags() & opts::GUI_CENTER_KML);

    if(center)
      showAircraft(false);
    return true;
  }
  return false;
}

void MapPaintWidget::onlineClientAndAtcUpdated()
{
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  update();
}

void MapPaintWidget::onlineNetworkChanged()
{
  screenIndex->resetAirspaceOnlineScreenGeometry();
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  update();
}
