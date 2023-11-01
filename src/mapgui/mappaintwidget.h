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

#ifndef LITTLENAVMAP_NAVMAPPAINTWIDGET_H
#define LITTLENAVMAP_NAVMAPPAINTWIDGET_H

#include "common/mapflags.h"
#include "geo/pos.h"

#include <marble/GeoDataLatLonAltBox.h>
#include <marble/MarbleWidget.h>

namespace map {
struct MapResult;
struct MapRef;
struct RangeMarker;
struct DistanceMarker;
struct PatternMarker;
struct HoldingMarker;
struct MsaMarker;
struct MapHolding;
struct MapAirportMsa;
struct MapAirspace;
struct MapAirway;
}

namespace atools {
namespace fs {
namespace sc {
class SimConnectUserAircraft;
class SimConnectData;
class SimConnectAircraft;
}
}

namespace geo {
class Rect;
}
namespace fs {
class SimConnectData;
}
}

class MainWindow;
class MapPaintLayer;
class MapScreenIndex;
class ApronGeometryCache;
class MapQuery;
class AirwayTrackQuery;
class WaypointTrackQuery;
class MapLayer;

namespace proc {
struct MapProcedureLeg;

struct MapProcedureLegs;

}

class AircraftTrail;

/*
 * Contains all functions to draw a map including background, flight plan, navaids and whatnot.
 * Independent of GUI. Can use any resolution independent of visible map window.
 * Usable for map image export and printing.
 *
 * Not fully independent of map widget since options are still used from dialog.
 *
 * Does not contain any UI and mouse/keyboard interaction.
 */
class MapPaintWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  explicit MapPaintWidget(QWidget *parent, bool visible);
  virtual ~MapPaintWidget() override;

  MapPaintWidget(const MapPaintWidget& other) = delete;
  MapPaintWidget& operator=(const MapPaintWidget& other) = delete;

  /* Copy all map display settings, projection, theme, highlights and marks from the other widget. */
  void copySettings(const MapPaintWidget& other);

  /* Copies the bounding rectangle to this one which will be centered on next resize. */
  void copyView(const MapPaintWidget& other);

  /* streamlined for webmapcontroller from showPosInternal(pos, distanceKm, doubleClick, false) */
  void showPosNotAdjusted(const atools::geo::Pos& pos, float distanceKm);

  /* Jump to position on the map using the given zoom distance.
   *  Keep current zoom if  distanceNm is INVALID_DISTANCE_VALUE.
   *  Use predefined zoom if distanceNm is 0 */
  void showPos(const atools::geo::Pos& pos, float distanceKm, bool doubleClick);

  /* Show the bounding rectangle on the map */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* streamlined for webmapcontroller from showRect(rect, false)
   *  constrainDistance determines whether map distance should be constrained by OptionData values */
  void showRectStreamlined(const atools::geo::Rect& rect, bool constrainDistance = true);

  /* Show user simulator aircraft. state is tool button state */
  void showAircraft(bool centerAircraftChecked);
  void showAircraftNow(bool);

  /* Update highlighted objects from search and route table */
  void changeSearchHighlights(const map::MapResult& newHighlights, bool updateAirspace, bool updateLogEntries);
  void changeRouteHighlights(const QList<int>& routeHighlight);

  /* Highlight a point along the route while mouse over in the profile window */
  void changeProfileHighlight(const atools::geo::Pos& pos);

  /* Update procedure highlights, update screen index and redraw map */
  void changeProcedureHighlight(const proc::MapProcedureLegs& procedure);
  void changeProcedureHighlights(const QVector<proc::MapProcedureLegs>& procedures);
  void changeProcedureLegHighlight(const proc::MapProcedureLeg& procedureLeg);

  /* Update route screen coordinate index */
  void routeChanged(bool geometryChanged);
  void routeAltitudeChanged(float);

  /* Stop showing aircraft position on map */
  void disconnectedFromSimulator();

  /* Clear previous aircraft track */
  void connectedToSimulator();

  /* Add a KML file to map display. The file will be restored on program startup */
  bool addKmlFile(const QString& kmlFile);

  /* Remove all KML files from map */
  void clearKmlFiles();

  const atools::geo::Pos& getSearchMarkPos() const
  {
    return searchMarkPos;
  }

  const atools::geo::Pos& getHomePos() const
  {
    return homePos;
  }

  /* Getters used by the painters */
  const map::MapResult& getSearchHighlights() const;

  /* Procedure preview. Delegates to map screen index. */
  const proc::MapProcedureLegs& getProcedureHighlight() const;
  const QVector<proc::MapProcedureLegs>& getProcedureHighlights() const;
  const proc::MapProcedureLeg& getProcedureLegHighlight() const;

  const QList<int>& getRouteHighlights() const;

  const QHash<int, map::RangeMarker>& getRangeMarks() const;
  const QHash<int, map::DistanceMarker>& getDistanceMarks() const;
  const QHash<int, map::PatternMarker>& getPatternsMarks() const;
  const QHash<int, map::HoldingMarker>& getHoldingMarks() const;
  const QHash<int, map::MsaMarker>& getMsaMarks() const;

  /* Get pointers to the wrapped map objects from holdings and MSA. */
  QList<map::MapHolding> getHoldingMarksFiltered() const;
  QList<map::MapAirportMsa> getMsaMarksFiltered() const;

  const atools::geo::Pos& getProfileHighlight() const;

  void clearSearchHighlights();

  /* true if any highlighting circles are to be drawn on the map */
  bool hasHighlights() const;

  const AircraftTrail& getAircraftTrail() const
  {
    return *aircraftTrail;
  }

  const AircraftTrail& getAircraftTrailLogbook() const
  {
    return *aircraftTrailLogbook;
  }

  int getAircraftTrailSize() const;

  /* Disconnect painter to avoid updates while no data is available */
  void preDatabaseLoad();

  /* Changes in options dialog */
  virtual void optionsChanged();

  /* GUI style has changed */
  void styleChanged();

  /* Update map */
  void postDatabaseLoad();

  /* Set map theme. * themeId: "google-maps-def",  themePath: "earth/google-maps-def/google-maps-def.dgml" or full path */
  void setTheme(const QString& themePath, const QString& themeId);

  /* Show points of interest and other labels for certain map themes */
  void setShowMapPois(bool show);

  /* Globe shadow */
  void setShowMapSunShading(bool show);
  void setSunShadingDateTime(const QDateTime& datetime);
  QDateTime getSunShadingDateTime() const;

  /* Define which airport or navaid types are shown on the map. Updates screen index on demand. */
  void setShowMapObject(map::MapTypes type, bool show);
  void setShowMapObjects(map::MapTypes type, map::MapTypes mask);
  void setShowMapObjectDisplay(map::MapDisplayTypes type, bool show);
  void setShowMapAirspaces(const map::MapAirspaceFilter& types);

  /* All currently set map display filters */
  map::MapTypes getShownMapTypes() const;
  map::MapDisplayTypes getShownMapDisplayTypes() const;
  const map::MapAirspaceFilter& getShownAirspaces() const;
  map::MapAirspaceFilter getShownAirspaceTypesByLayer() const;
  int getShownMinimumRunwayFt() const;

  /* User aircraft as shown on the map */
  const atools::fs::sc::SimConnectUserAircraft& getUserAircraft() const;
  const atools::fs::sc::SimConnectData& getSimConnectData() const;

  /* AI aircraft as shown on the map */
  const QVector<atools::fs::sc::SimConnectAircraft>& getAiAircraft() const;

  /* Get currently loaded KML file paths */
  const QStringList& getKmlFiles() const
  {
    return kmlFilePaths;
  }

  /* Update indexes for online network changes */
  void onlineClientAndAtcUpdated();

  /* Whole online network has changed */
  void onlineNetworkChanged();

  /* Redraw map to reflect weather changes */
  void weatherUpdated();

  /* Redraw map to reflect wind barb changes */
  void windDisplayUpdated();

  /* Current weather source for icon display */
  map::MapWeatherSource getMapWeatherSource() const;

  /* Airspaces and airways from the information window are kept in separate lists */
  void changeAirspaceHighlights(const QList<map::MapAirspace>& airspaces);
  void changeAirwayHighlights(const QList<QList<map::MapAirway> >& airways);

  /* Highlights from clicking "Map" in information window */
  const QList<map::MapAirspace>& getAirspaceHighlights() const;
  const QList<QList<map::MapAirway> >& getAirwayHighlights() const;

  /* Highlights from info window */
  void clearAirspaceHighlights();
  void clearAirwayHighlights();

  /* Avoids dark background when printing in night mode */
  void setPrinting(bool value)
  {
    printing = value;
  }

  bool isPrinting() const
  {
    return printing;
  }

  /* Create json document with coordinates for AviTab configuration. Returns empty string if map is out of bounds.
   *  Requires Mercator projection. */
  QString createAvitabJson();

  /* Override for MapWidget with empty implementation ========================== */
  virtual void jumpBackToAircraftCancel();

  /* Draw empty rect if not active to avoid graphic clutter */
  void setActive(bool value = true)
  {
    active = value;
  }

  /* Will keep the shown bounding rectangle on resize if true */
  void setKeepWorldRect(bool value = true)
  {
    keepWorldRect = value;
  }

  /* Adjust for more sharp images when centering rectangle on resize */
  void setAdjustOnResize(bool value = true)
  {
    adjustOnResize = value;
  }

  /* Try several iterations to show the given rectangele as precise as possible.
   * More CPU intense than other functions. */
  void centerRectOnMapPrecise(const Marble::GeoDataLatLonBox& rect, bool allowAdjust);
  void centerRectOnMapPrecise(const atools::geo::Rect& rect, bool allowAdjust);

  /* Resizes the widget if width and height are bigger than 0 and returns map content as pixmap. */
  QPixmap getPixmap(int width = -1, int height = -1);
  QPixmap getPixmap(const QSize& size);

  /* Prepare Marble widget drawing with a dummy paint event without drawing navaids */
  void prepareDraw(int width, int height);

  bool isAvoidBlurredMap() const
  {
    return avoidBlurredMap;
  }

  void setAvoidBlurredMap(bool value)
  {
    avoidBlurredMap = value;
  }

  /* Pos includes distance in km as altitude */
  atools::geo::Pos getCurrentViewCenterPos() const;
  atools::geo::Rect getCurrentViewRect() const;

  bool isNoNavPaint() const
  {
    return noNavPaint;
  }

  void setNoNavPaint(bool value)
  {
    noNavPaint = value;
  }

  bool isPaintCopyright() const
  {
    return paintCopyright;
  }

  void setPaintCopyright(bool value)
  {
    paintCopyright = value;
  }

  ApronGeometryCache *getApronGeometryCache();

  /* true if real map display widget - false if hidden for online services or other applications */
  bool isVisibleWidget() const
  {
    return visibleWidget;
  }

  /* Logbook display options have changed or new or edited logbook entry */
  void updateLogEntryScreenGeometry();

  /* For debugging functions */
  MapScreenIndex *getScreenIndex()
  {
    return screenIndex;
  }

  MapPaintLayer *getMapPaintLayer()
  {
    return paintLayer;
  }

  /* Saved bounding box from last zoom or scroll operation. Needed to detect view changes. */
  const Marble::GeoDataLatLonBox& getCurrentViewBoundingBox() const;

  /* Get map query with cached objects for this paint widget instance */
  MapQuery *getMapQuery() const
  {
    return mapQuery;
  }

  AirwayTrackQuery *getAirwayTrackQuery() const
  {
    return airwayTrackQuery;
  }

  WaypointTrackQuery *getWaypointTrackQuery() const
  {
    return waypointTrackQuery;
  }

  void postTrackLoad();

  /* No drawing at all and not map interactions except moving and zooming if true.
   * Limit depends on projection. */
  bool noRender() const;

  /* Print all layers to debug channel */
  void dumpMapLayers() const;

  const QVector<map::MapRef>& getRouteDrawnNavaidsConst() const;

  QVector<map::MapRef> *getRouteDrawnNavaids();

  const QString& getCurrentThemeId() const
  {
    return currentThemeId;
  }

  /* Too many objects on map */
  bool isPaintOverflow() const;

  /* Do not show anything above this zoom distance except user features */
  bool isDistanceCutOff() const;

signals:
  /* Emitted whenever the result exceeds the limit clause in the queries */
  void resultTruncated();

  /* Update action state in main window (disabled/enabled) */
  void updateActionStates();

  /* Aircraft track was pruned and needs to be updated */
  void aircraftTrackPruned();

  void shownMapFeaturesChanged(map::MapTypes types);

  /* Search center has changed by context menu */
  void searchMarkChanged(const atools::geo::Pos& mark);

protected:
  /* Internal zooming and centering. Zooms one step out to get a sharper map display if allowAdjust is true */
  void centerPosOnMap(const atools::geo::Pos& pos);
  void setDistanceToMap(double dist, bool allowAdjust = true);
  void centerRectOnMap(const atools::geo::Rect& rect, bool allowAdjust = true);
  void centerRectOnMap(const Marble::GeoDataLatLonBox& rect, bool allowAdjust);

  const MapScreenIndex *getScreenIndexConst() const
  {
    return screenIndex;
  }

  void showPosInternal(const atools::geo::Pos& pos, float distanceKm, bool doubleClick, bool allowAdjust);

  /* Zoom out once to get a sharp map display */
  void adjustMapDistance();

  bool loadKml(const QString& filename, bool center);

  /* Set cache size from option data */
  void updateCacheSizes();

  /* Overrides for MapWidget with empty implementation that are used to update GUI elements ================ */
  /* Show or hide overlays depending on menu state - default is all off */
  virtual void overlayStateFromMenu();

  /* Send visibility of overlays to menu items - default is no-op */
  virtual void overlayStateToMenu();

  /* Start jump back time - default is no-op */
  virtual void jumpBackToAircraftStart();

  /* Cancel all drag and drop operations - default is no-op */
  virtual void cancelDragAll();

  /* Remove tooltip - default is no-op */
  virtual void hideTooltip();

  /* Create history entry for new map position - default is no-op */
  virtual void handleHistory();

  /* Get sun shading from menu state - default is from internal value */
  virtual map::MapSunShading sunShadingFromUi();

  /* Get weather from menu state - default is from internal value */
  virtual map::MapWeatherSource weatherSourceFromUi();

  /* Update buttons based on current theme - default is no-op */
  virtual void updateThemeUi(const QString&);

  /* Update buttons for show/center aircraft - default is no-op */
  virtual void updateShowAircraftUi(bool);

  /* Update toolbar state for visible features - default is no-op */
  virtual void updateMapVisibleUi() const;
  virtual void updateMapVisibleUiPostDatabaseLoad() const;

  /* Update internal values for visible map objects based on menus - default is no-op */
  virtual void updateMapObjectsShown();

  /* Check if position can be displayed and show a warning if not (near poles in Mercator, e.g.).
   * returns true if position is ok. */
  virtual bool checkPos(const atools::geo::Pos&);

  virtual void resizeEvent(QResizeEvent *event) override;

  void updateGeometryIndex(map::MapTypes oldTypes, map::MapDisplayTypes oldDisplayTypes, int oldMinRunwayLength);

  /* If width and height of a bounding rect are smaller than this: Use show point */
  static constexpr float POS_IS_POINT_EPSILON_DEG = 0.0001f;
  static constexpr float MIN_ZOOM_RECT_DIAMETER_KM = 0.04f;
  static constexpr float MAX_ZOOM_RECT_DIAMETER_KM = 1000.f;

  /* Caches complex X-Plane apron geometry as objects in screen coordinates for faster painting. */
  ApronGeometryCache *apronGeometryCache;

  /* Keep the the overlays for the GUI widget from updating */
  bool ignoreOverlayUpdates = false;

  /* Values used to check if view has changed */
  Marble::GeoDataLatLonBox currentViewBoundingBox;

  /* Radius search center and home position */
  atools::geo::Pos searchMarkPos, homePos;
  double homeDistance = 0.;

  /* Loaded KML file paths */
  QStringList kmlFilePaths;

  MapPaintLayer *paintLayer;

  /* Do not draw while database is unavailable */
  bool databaseLoadStatus = false;

  /* Widget is shown */
  bool active = false;

  /* Keep the visible world rectangle when resizing - used in resize event */
  bool keepWorldRect = false;

  /* Keep the visible world rectangle when resizing and zoom out one step to keep image sharp */
  bool adjustOnResize = false;

  /* Zoom one step out to avoid blurred maps */
  bool avoidBlurredMap = false;

  /* true if real window/widget */
  bool visibleWidget = false;

  /* verbose logging */
  bool verbose = false;

  /* Dummy paint cycle without any navigation stuff. Just used to initialize Marble */
  bool noNavPaint = false;

  /* Paint copyright note into image */
  bool paintCopyright = true;

  /* Update screen index after painting */
  bool screenIndexUpdateReqired = false;

  /* Map theme id like "google-maps-def". */
  QString currentThemeId;

  /* Trail/track of user aircraft */
  AircraftTrail *aircraftTrail = nullptr, *aircraftTrailLogbook = nullptr;

  /* Skip the first unneeded render event after mouse events */
  bool skipRender = false;

private:
  /* Set map theme and adjust properties accordingly. themePath is the full path to the DGML */
  void setThemeInternal(const QString& themePath);

  /* Override widget events */
  virtual void paintEvent(QPaintEvent *paintEvent) override;

  void unitsUpdated();

  /*  Add placemark files for offline maps */
  void addPlacemarks();

  /* Need to remove the placemark files since they are shown randomly on online maps */
  void removePlacemarks();

  /* Keeps geographical objects as index in screen coordinates */
  MapScreenIndex *screenIndex = nullptr;

  MapQuery *mapQuery = nullptr;
  AirwayTrackQuery *airwayTrackQuery = nullptr;
  WaypointTrackQuery *waypointTrackQuery = nullptr;

  /* Current zoom value (NOT distance) */
  int currentZoom = -1;

  /* Avoids dark background when printing in night mode */
  bool printing = false;

  /* true if inside paint event - avoids crashes due to nested calls */
  bool painting = false;
};



#endif // LITTLENAVMAP_NAVMAPPAINTWIDGET_H
