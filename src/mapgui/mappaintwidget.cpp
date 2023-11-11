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

#include "mapgui/mappaintwidget.h"

#include "common/aircrafttrail.h"
#include "common/constants.h"
#include "common/mapresult.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "mapgui/aprongeometrycache.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/mapthemehandler.h"
#include "mappainter/mappaintlayer.h"
#include "marble/ViewportParams.h"
#include "app/navapp.h"
#include "query/airwayquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "query/waypointquery.h"
#include "query/waypointtrackquery.h"
#include "settings/settings.h"

#include <QPainter>
#include <QJsonDocument>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QFile>

#include <marble/MarbleLocale.h>
#include <marble/MarbleModel.h>
#include <marble/AbstractFloatItem.h>

// KM
const static double MINIMUM_DISTANCE_KM = 0.05;
const static double MAXIMUM_DISTANCE_KM = 6000.;
const static int MAXIMUM_ZOOM = 1120;

/* Do not show anything above this zoom distance except user features */
constexpr float DISTANCE_CUT_OFF_LIMIT_MERCATOR_KM = 10000.f;
constexpr float DISTANCE_CUT_OFF_LIMIT_SPHERICAL_KM = 8000.f;

// Placemark files to remove or add
const static QStringList PLACEMARK_FILES_CACHE({
  "baseplacemarks.cache", "boundaryplacemarks.cache", "cityplacemarks.cache", "elevplacemarks.cache",
  "otherplacemarks.cache"});

// Additional maps refer to KML files get rid of these too
const static QStringList PLACEMARK_FILES_KML({
  "baseplacemarks.kml", "boundaryplacemarks.kml", "cityplacemarks.kml", "elevplacemarks.kml", "otherplacemarks.kml"});

using namespace Marble;
using atools::geo::Rect;
using atools::geo::Pos;

MapPaintWidget::MapPaintWidget(QWidget *parent, bool visible)
  : Marble::MarbleWidget(parent), visibleWidget(visible)
{
  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_MAPWIDGET_DEBUG, false).toBool();

  aircraftTrail = new AircraftTrail;
  aircraftTrailLogbook = new AircraftTrail;

  // Set the map quality to gain speed while moving
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);
  setAnimationsEnabled(false);

  unitsUpdated();

  paintLayer = new MapPaintLayer(this);
  addLayer(paintLayer);

  screenIndex = new MapScreenIndex(this, paintLayer);

  const OptionData& options = OptionData::instance();

  setSunShadingDimFactor(static_cast<double>(options.getDisplaySunShadingDimFactor()) / 100.);
  avoidBlurredMap = options.getFlags2() & opts2::MAP_AVOID_BLURRED_MAP;

  setFont(options.getMapFont());

  // Initialize the X-Plane apron geometry cache
  apronGeometryCache = new ApronGeometryCache();
  apronGeometryCache->setViewportParams(viewport());

  mapQuery = new MapQuery(NavApp::getDatabaseSim(), NavApp::getDatabaseNav(), NavApp::getDatabaseUser());
  mapQuery->initQueries();

  // Set up airway queries =====================
  airwayTrackQuery = new AirwayTrackQuery(new AirwayQuery(NavApp::getDatabaseNav(), false),
                                          new AirwayQuery(NavApp::getDatabaseTrack(), true));
  airwayTrackQuery->initQueries();

  // Set up waypoint queries =====================
  waypointTrackQuery = new WaypointTrackQuery(new WaypointQuery(NavApp::getDatabaseNav(), false),
                                              new WaypointQuery(NavApp::getDatabaseTrack(), true));
  waypointTrackQuery->initQueries();

  paintLayer->initQueries();
}

MapPaintWidget::~MapPaintWidget()
{
  removeLayer(paintLayer);

  // Have to delete manually since classes can be copied and does not delete in destructor
  airwayTrackQuery->deleteChildren();
  ATOOLS_DELETE_LOG(airwayTrackQuery);

  waypointTrackQuery->deleteChildren();
  ATOOLS_DELETE_LOG(waypointTrackQuery);

  ATOOLS_DELETE_LOG(paintLayer);
  ATOOLS_DELETE_LOG(screenIndex);
  ATOOLS_DELETE_LOG(aircraftTrail);
  ATOOLS_DELETE_LOG(aircraftTrailLogbook);
  ATOOLS_DELETE_LOG(apronGeometryCache);
  ATOOLS_DELETE_LOG(mapQuery);
}

void MapPaintWidget::copySettings(const MapPaintWidget& other)
{
  paintLayer->copySettings(*other.paintLayer);
  screenIndex->copy(*other.screenIndex);

  // Copy all MarbleWidget settings - some on demand to avoid overhead
  if(projection() != Marble::Mercator)
    setProjection(Marble::Mercator);

  if(mapThemeId() != other.mapThemeId())
    setTheme(other.mapThemeId(), other.currentThemeId);

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
  currentThemeId = other.currentThemeId;
  *aircraftTrail = *other.aircraftTrail;
  *aircraftTrailLogbook = *other.aircraftTrailLogbook;
  searchMarkPos = other.searchMarkPos;
  homePos = other.homePos;
  homeDistance = other.homeDistance;
  kmlFilePaths = other.kmlFilePaths;
  avoidBlurredMap = other.avoidBlurredMap;

  if(size() != other.size())
    resize(other.size());
}

void MapPaintWidget::copyView(const MapPaintWidget& other)
{
  currentViewBoundingBox = other.viewport()->viewLatLonAltBox();
}

void MapPaintWidget::setTheme(const QString& themePath, const QString& themeId)
{
  // themeId: "google-maps-def",  themePath: "earth/google-maps-def/google-maps-def.dgml" or full path
  qDebug() << Q_FUNC_INFO << "setting map theme to" << themeId << themePath;

  currentThemeId = themeId;

  setThemeInternal(themePath);
}

bool MapPaintWidget::noRender() const
{
  return paintLayer->noRender();
}

void MapPaintWidget::setThemeInternal(const QString& themePath)
{
  // Ignore any overlay state signals the widget sends while switching theme
  ignoreOverlayUpdates = true;

  updateThemeUi(currentThemeId);

  setMapThemeId(themePath);
  setShowClouds(false);

  if(NavApp::getMapThemeHandler()->hasPlacemarks(currentThemeId))
    // Add placemark files again - ignored if already loaded
    addPlacemarks();
  else
    // Need to remove the placemark files since they are shown randomly on online maps
    removePlacemarks();

  updateMapObjectsShown();

  ignoreOverlayUpdates = false;

  // Show or hide overlays again
  overlayStateFromMenu();
}

void MapPaintWidget::removePlacemarks()
{
  MarbleModel *m = model();
  // Need to remove the placemark files since they are shown randomly on online maps
  for(const QString& file : PLACEMARK_FILES_CACHE)
    m->removeGeoData(file);

  for(const QString& file : PLACEMARK_FILES_KML)
    m->removeGeoData(file);
}

void MapPaintWidget::addPlacemarks()
{
  MarbleModel *m = model();

  // Add placemark files again - ignored if already loaded
  for(const QString& file : PLACEMARK_FILES_CACHE)
    m->addGeoDataFile(file);
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
  const OptionData& options = OptionData::instance();

  // Pass API keys or tokens to map
  setKeys(NavApp::getMapThemeHandler()->getMapThemeKeysHash());

  setFont(options.getMapFont());

  unitsUpdated();

  // Updated sun shadow and force a tile refresh by changing the show status again
  setSunShadingDimFactor(static_cast<double>(options.getDisplaySunShadingDimFactor()) / 100.);
  setShowSunShading(showSunShading());
  avoidBlurredMap = options.getFlags2() & opts2::MAP_AVOID_BLURRED_MAP;

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
  if(paintLayer->getShownMapDisplayTypes().testFlag(map::AIRPORT_WEATHER))
    update();

  updateMapVisibleUi();
}

void MapPaintWidget::windDisplayUpdated()
{
  if(paintLayer->getShownMapDisplayTypes().testFlag(map::WIND_BARBS) ||
     paintLayer->getShownMapDisplayTypes().testFlag(map::WIND_BARBS_ROUTE))
    update();

  updateMapVisibleUi();
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
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << show;
#endif

  // Enable all POI stuff
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
  setShowTerrain(show);
}

void MapPaintWidget::updateGeometryIndex(map::MapTypes oldTypes, map::MapDisplayTypes oldDisplayTypes, int oldMinRunwayLength)
{
  // Update screen coordinate caches if display options have changed
  map::MapTypes types = getShownMapTypes();
  map::MapDisplayTypes displayTypes = getShownMapDisplayTypes();
  int minRunwayLength = getShownMinimumRunwayFt();

  if(((types& map::AIRWAY_ALL) != (oldTypes & map::AIRWAY_ALL)) || types.testFlag(map::TRACK) || oldTypes.testFlag(map::TRACK))
    screenIndex->updateAirwayScreenGeometry(getCurrentViewBoundingBox());

  if(types.testFlag(map::AIRSPACE) != oldTypes.testFlag(map::AIRSPACE))
    screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());

  if(minRunwayLength != oldMinRunwayLength || // Airport visibility also changes ILS
     (types& map::AIRPORT_ALL_AND_ADDON) != (oldTypes & map::AIRPORT_ALL_AND_ADDON) || // ILS are disabled with airports
     (types.testFlag(map::ILS) != oldTypes.testFlag(map::ILS)) ||
     (displayTypes.testFlag(map::GLS) != oldDisplayTypes.testFlag(map::GLS)) ||
     (displayTypes.testFlag(map::FLIGHTPLAN) != oldDisplayTypes.testFlag(map::FLIGHTPLAN)))
    screenIndex->updateIlsScreenGeometry(getCurrentViewBoundingBox());

  if(types.testFlag(map::MISSED_APPROACH) != oldTypes.testFlag(map::MISSED_APPROACH) ||
     displayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE) != oldDisplayTypes.testFlag(map::FLIGHTPLAN_ALTERNATE) ||
     (displayTypes.testFlag(map::FLIGHTPLAN) != oldDisplayTypes.testFlag(map::FLIGHTPLAN)))
    screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());

  // Update screen coordinate cache if display options have changed
  if((displayTypes& map::LOGBOOK_ALL) != (oldDisplayTypes & map::LOGBOOK_ALL))
    screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::dumpMapLayers() const
{
  paintLayer->dumpMapLayers();
}

const QVector<map::MapRef>& MapPaintWidget::getRouteDrawnNavaidsConst() const
{
  return screenIndex->getRouteDrawnNavaidsConst();
}

QVector<map::MapRef> *MapPaintWidget::getRouteDrawnNavaids()
{
  return screenIndex->getRouteDrawnNavaids();
}

bool MapPaintWidget::isPaintOverflow() const
{
  return paintLayer->isObjectOverflow() || paintLayer->isQueryOverflow();
}

bool MapPaintWidget::isDistanceCutOff() const
{
  if(projection() == Marble::Spherical)
    return distance() > DISTANCE_CUT_OFF_LIMIT_SPHERICAL_KM;
  else
    return distance() > DISTANCE_CUT_OFF_LIMIT_MERCATOR_KM;
}

void MapPaintWidget::setShowMapObjects(map::MapTypes type, map::MapTypes mask)
{
  paintLayer->setShowMapObjects(type, mask);
}

void MapPaintWidget::setShowMapObject(map::MapTypes type, bool show)
{
  paintLayer->setShowMapObject(type, show);
}

void MapPaintWidget::setShowMapObjectDisplay(map::MapDisplayTypes type, bool show)
{
  paintLayer->setShowMapObjectDisplay(type, show);
}

void MapPaintWidget::setShowMapAirspaces(const map::MapAirspaceFilter& types)
{
  paintLayer->setShowAirspaces(types);
  updateMapVisibleUi();
  screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
}

void MapPaintWidget::updateMapVisibleUi() const
{
  // No-op
}

void MapPaintWidget::updateMapVisibleUiPostDatabaseLoad() const
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

map::MapTypes MapPaintWidget::getShownMapTypes() const
{
  return paintLayer->getShownMapTypes();
}

int MapPaintWidget::getShownMinimumRunwayFt() const
{
  return paintLayer->getShownMinimumRunwayFt();
}

map::MapDisplayTypes MapPaintWidget::getShownMapDisplayTypes() const
{
  return paintLayer->getShownMapDisplayTypes();
}

const map::MapAirspaceFilter& MapPaintWidget::getShownAirspaces() const
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

void MapPaintWidget::preDatabaseLoad()
{
  jumpBackToAircraftCancel();
  cancelDragAll();
  databaseLoadStatus = true;
  apronGeometryCache->clear();
  paintLayer->preDatabaseLoad();
  mapQuery->deInitQueries();
  airwayTrackQuery->deInitQueries();
  waypointTrackQuery->deInitQueries();
}

void MapPaintWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;

  // Update screen index after next paint event
  screenIndexUpdateReqired = true;

  // Reload track into database to catch changed waypoint ids
  airwayTrackQuery->initQueries();
  waypointTrackQuery->initQueries();
  mapQuery->initQueries();
  paintLayer->postDatabaseLoad();
  update();
  updateMapVisibleUiPostDatabaseLoad();
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

  if(verbose)
    qDebug() << Q_FUNC_INFO << "initial zoom" << zoom() << "zoom step" << zoomStep();

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
    if(verbose)
      qDebug() << Q_FUNC_INFO << "zoom" << zoom()
               << "Viewport" << viewport()->viewLatLonAltBox().toString(GeoDataCoordinates::Degree);

    zoomViewBy(-step);
    zoomIterations++;
  }

  if(verbose)
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
  if(!rect.isPoint(POS_IS_POINT_EPSILON_DEG) &&
     std::abs(rect.getWidthDegree()) < 180.f &&
     std::abs(rect.getHeightDegree()) < 180.f &&
     std::abs(rect.getWidthDegree()) > POS_IS_POINT_EPSILON_DEG &&
     std::abs(rect.getHeightDegree()) > POS_IS_POINT_EPSILON_DEG)
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

    if(std::abs(rect.getWidthDegree()) < 180.f && std::abs(rect.getHeightDegree()) < 180.f)
      setDistanceToMap(MINIMUM_DISTANCE_KM, allowAdjust);
    else
      setDistanceToMap(MAXIMUM_DISTANCE_KM, allowAdjust);
  }

  if(zoom() < MAXIMUM_ZOOM)
    setZoom(MAXIMUM_ZOOM);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distance NM" << atools::geo::kmToNm(distance()) << "zoom" << zoom();
#endif
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
  setDistance(atools::minmax(MINIMUM_DISTANCE_KM, MAXIMUM_DISTANCE_KM, distanceKm));

  if(allowAdjust && avoidBlurredMap)
    adjustMapDistance();
}

void MapPaintWidget::showPosNotAdjusted(const atools::geo::Pos& pos, float distanceKm)
{
  Pos normPos = pos.normalized();
  centerOn(normPos.getLonX(), normPos.getLatY());
  setDistance(atools::minmax(MINIMUM_DISTANCE_KM, MAXIMUM_DISTANCE_KM, static_cast<double>(distanceKm)));
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
  jumpBackToAircraftStart();

  if(atools::almostEqual(distanceKm, 0.f))
    // Use distance depending on double click
    distanceKm = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                               OptionData::instance().getMapZoomShowClick() :
                                               OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

  centerPosOnMap(pos);
  if(distanceKm < map::INVALID_DISTANCE_VALUE)
    setDistanceToMap(distanceKm, allowAdjust);
}

void MapPaintWidget::showRectStreamlined(const atools::geo::Rect& rect, bool constrainDistance)
{
  if(rect.isPoint(POS_IS_POINT_EPSILON_DEG))
    showPosNotAdjusted(rect.getTopLeft(), 0.f);
  else
  {
    float w = std::abs(rect.getWidthDegree());
    float h = std::abs(rect.getHeightDegree());

    if(atools::almostEqual(w, 0.f, POS_IS_POINT_EPSILON_DEG))
      // Workaround for marble not being able to center certain lines
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest() - h / 2, rect.getNorth(), rect.getEast() + h / 2, rect.getSouth()));
    else if(atools::almostEqual(h, 0.f, POS_IS_POINT_EPSILON_DEG))
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest(), rect.getNorth() + w / 2, rect.getEast(), rect.getSouth() - w / 2));
    else
      // Center on rectangle
      centerRectOnMap(rect);

    if(constrainDistance)
    {
      float distanceKm = atools::geo::nmToKm(Unit::rev(OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

      if(distance() < distanceKm)
        setDistanceToMap(distanceKm);
    }
  }
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
  jumpBackToAircraftStart();

  float w = std::abs(rect.getWidthDegree());
  float h = std::abs(rect.getHeightDegree());

  if(!rect.isValid())
  {
    qWarning() << Q_FUNC_INFO << "invalid rectangle";
    return;
  }

  if(rect.isPoint(POS_IS_POINT_EPSILON_DEG))
    showPos(rect.getTopLeft(), 0.f, doubleClick);
  else
  {
    if(atools::almostEqual(w, 0.f, POS_IS_POINT_EPSILON_DEG))
      // Workaround for marble not being able to center certain lines
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest() - h / 2, rect.getNorth(), rect.getEast() + h / 2, rect.getSouth()));
    else if(atools::almostEqual(h, 0.f, POS_IS_POINT_EPSILON_DEG))
      // Turn rect into a square
      centerRectOnMap(Rect(rect.getWest(), rect.getNorth() + w / 2, rect.getEast(), rect.getSouth() - w / 2));
    else
      // Center on rectangle
      centerRectOnMap(rect);

    float distanceKm = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                                     OptionData::instance().getMapZoomShowClick() :
                                                     OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

    if(distance() < distanceKm)
      setDistanceToMap(distanceKm);
  }
}

void MapPaintWidget::showAircraftNow(bool)
{
  qDebug() << Q_FUNC_INFO;

  if(screenIndex->getUserAircraft().isFullyValid())
    showPos(screenIndex->getUserAircraft().getPosition(), 0.f, false /* doubleClick */);
}

void MapPaintWidget::showAircraft(bool centerAircraftChecked)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;

  if(!(OptionData::instance().getFlags2() & opts2::ROUTE_NO_FOLLOW_ON_MOVE) || centerAircraftChecked)
  {
    // Keep old behavior if jump back to aircraft is disabled

    // Adapt the menu item status if this method was not called by the action
    updateShowAircraftUi(centerAircraftChecked);

    if(centerAircraftChecked && screenIndex->getUserAircraft().isFullyValid())
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
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(geometryChanged)
  {
    cancelDragAll();
    screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());
  }
  screenIndex->updateIlsScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::routeAltitudeChanged(float)
{
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
  screenIndex->clearSimData();
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
  for(const QString& file : qAsConst(kmlFilePaths))
    model()->removeGeoData(file);
  kmlFilePaths.clear();
}

const atools::geo::Pos& MapPaintWidget::getProfileHighlight() const
{
  return screenIndex->getProfileHighlight();
}

void MapPaintWidget::clearSearchHighlights()
{
  screenIndex->changeSearchHighlights(map::MapResult());

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

int MapPaintWidget::getAircraftTrailSize() const
{
  return aircraftTrail->size();
}

const map::MapResult& MapPaintWidget::getSearchHighlights() const
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

const proc::MapProcedureLeg& MapPaintWidget::getProcedureLegHighlight() const
{
  return screenIndex->getProcedureLegHighlight();
}

const QVector<proc::MapProcedureLegs>& MapPaintWidget::getProcedureHighlights() const
{
  return screenIndex->getProcedureHighlights();
}

void MapPaintWidget::changeProcedureHighlights(const QVector<proc::MapProcedureLegs>& procedures)
{
#ifdef DEBUG_INFORMATION_PROC_HIGHLIGHT
  qDebug() << Q_FUNC_INFO << procedures;
#endif

  cancelDragAll();
  screenIndex->setProcedureHighlights(procedures);
  screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());
  update();
}

const proc::MapProcedureLegs& MapPaintWidget::getProcedureHighlight() const
{
  return screenIndex->getProcedureHighlight();
}

void MapPaintWidget::changeProcedureHighlight(const proc::MapProcedureLegs& procedure)
{
#ifdef DEBUG_INFORMATION_PROC_HIGHLIGHT
  qDebug() << Q_FUNC_INFO << procedure;
#endif

  cancelDragAll();
  screenIndex->setProcedureHighlight(procedure);
  screenIndex->updateRouteScreenGeometry(getCurrentViewBoundingBox());
  update();
}

void MapPaintWidget::changeProcedureLegHighlight(const proc::MapProcedureLeg& procedureLeg)
{
  screenIndex->setProcedureLegHighlight(procedureLeg);
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

void MapPaintWidget::changeSearchHighlights(const map::MapResult& newHighlights, bool updateAirspace, bool updateLogEntries)
{
  screenIndex->changeSearchHighlights(newHighlights);
  if(updateLogEntries)
    screenIndex->updateLogEntryScreenGeometry(getCurrentViewBoundingBox());
  if(updateAirspace)
    screenIndex->updateAirspaceScreenGeometry(getCurrentViewBoundingBox());
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
  const QList<AbstractFloatItem *> floats = floatItems();
  for(AbstractFloatItem *overlay : floats)
  {
    overlay->setVisible(false);
    overlay->hide();
  }
}

void MapPaintWidget::overlayStateToMenu()
{
  // No-op
}

void MapPaintWidget::jumpBackToAircraftStart()
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

void MapPaintWidget::postTrackLoad()
{
  waypointTrackQuery->clearCache();
  airwayTrackQuery->clearCache();

  waypointTrackQuery->initQueries();
  airwayTrackQuery->initQueries();
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

void MapPaintWidget::updateThemeUi(const QString&)
{
  // No-op
}

void MapPaintWidget::updateShowAircraftUi(bool)
{
  // No-op
}

const QList<int>& MapPaintWidget::getRouteHighlights() const
{
  return screenIndex->getRouteHighlights();
}

const QHash<int, map::RangeMarker>& MapPaintWidget::getRangeMarks() const
{
  return screenIndex->getRangeMarks();
}

const QHash<int, map::DistanceMarker>& MapPaintWidget::getDistanceMarks() const
{
  return screenIndex->getDistanceMarks();
}

const QHash<int, map::PatternMarker>& MapPaintWidget::getPatternsMarks() const
{
  return screenIndex->getPatternMarks();
}

const QHash<int, map::HoldingMarker>& MapPaintWidget::getHoldingMarks() const
{
  return screenIndex->getHoldingMarks();
}

const QHash<int, map::MsaMarker>& MapPaintWidget::getMsaMarks() const
{
  return screenIndex->getMsaMarks();
}

QList<map::MapHolding> MapPaintWidget::getHoldingMarksFiltered() const
{
  QList<map::MapHolding> retval;
  const QHash<int, map::HoldingMarker>& marks = screenIndex->getHoldingMarks();
  for(auto it = marks.constBegin(); it != marks.constEnd(); ++it)
    retval.append(it.value().holding);
  return retval;
}

QList<map::MapAirportMsa> MapPaintWidget::getMsaMarksFiltered() const
{
  QList<map::MapAirportMsa> retval;
  const QHash<int, map::MsaMarker>& marks = screenIndex->getMsaMarks();
  for(auto it = marks.constBegin(); it != marks.constEnd(); ++it)
    retval.append(it.value().msa);
  return retval;
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
  if(verbose)
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
  if(verbose)
  {
    // Avoid excessive logging on visible widget
    qDebug() << Q_FUNC_INFO << "Viewport" << getCurrentViewBoundingBox().toString(GeoDataCoordinates::Degree);
    qDebug() << Q_FUNC_INFO << "currentViewBoundingBox" << currentViewBoundingBox.toString(GeoDataCoordinates::Degree);
    qDebug() << Q_FUNC_INFO << "skipRender" << skipRender << "painting" << painting
             << "noRender" << noRender() << "mapCoversViewport" << viewport()->mapCoversViewport();
  }

  if(skipRender)
  {
    // Skip unneeded rendering after single mouseclick
    skipRender = false;

    // Exit paint only if the map covers the whole view.
    // Spurious paint events are triggered if the coverage is not complete (e.g. black background around the globe)
    if(viewport()->mapCoversViewport())
      return;
  }

  if(!painting)
  {
    painting = true;
    if(!active)
    {
      // No map yet - clear area
      QPainter painter(this);
      painter.fillRect(paintEvent->rect(), QGuiApplication::palette().color(QPalette::Window));
    }
    else
    {
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

      if(screenIndexUpdateReqired)
      {
        screenIndexUpdateReqired = false;
        changed = true;
      }

      // Erase map window to avoid black rectangle but do a dummy draw call to have everything initialized
      MarbleWidget::paintEvent(paintEvent);

      if(!NavApp::isMainWindowVisible())
        QPainter(this).fillRect(paintEvent->rect(), QGuiApplication::palette().color(QPalette::Window));

      if(changed)
      {
        // Major change - update index and visible objects
        updateMapVisibleUi();
        screenIndex->updateAllGeometry(currentViewBoundingBox);
      }

      if(paintLayer->isObjectOverflow() || paintLayer->isQueryOverflow())
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO
                 << "isObjectOverflow" << paintLayer->isObjectOverflow()
                 << "getObjectCount" << paintLayer->getObjectCount()
                 << "isQueryOverflow" << paintLayer->isQueryOverflow();
#endif

        // Passed by queued connection - execute later in event loop
        emit resultTruncated();
      }
    } // if(!active) ... else ...

    painting = false;
  } // if(!painting)
  else
    qWarning() << Q_FUNC_INFO << "Recursive call to paint";
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
