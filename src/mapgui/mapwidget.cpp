/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/mapwidget.h"

#include "options/optiondata.h"
#include "navapp.h"
#include "common/constants.h"
#include "mapgui/mappaintlayer.h"
#include "settings/settings.h"
#include "common/elevationprovider.h"
#include "gui/mainwindow.h"
#include "mapgui/mapscale.h"
#include "geo/calculations.h"
#include "common/maptools.h"
#include "common/mapcolors.h"
#include "connect/connectclient.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "route/route.h"
#include "route/routecontroller.h"
#include "atools.h"
#include "mapgui/mapquery.h"
#include "mapgui/maptooltip.h"
#include "common/symbolpainter.h"
#include "mapgui/mapscreenindex.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"
#include "util/htmlbuilder.h"
#include "mapgui/maplayersettings.h"
#include "common/unit.h"
#include "gui/widgetstate.h"
#include "gui/application.h"

#include <QContextMenuEvent>
#include <QToolTip>
#include <QRubberBand>
#include <QMessageBox>
#include <QPainter>

#include <marble/MarbleLocale.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/MarbleModel.h>
#include <marble/AbstractFloatItem.h>

// Default zoom distance if start position was not set (usually first start after installation */
const int DEFAULT_MAP_DISTANCE = 7000;

// Get elevation when mouse is still
const int ALTITUDE_UPDATE_TIMEOUT = 200;

// Update rates defined by delta values
const static QHash<opts::SimUpdateRate, MapWidget::SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
{
  // manhattanLengthDelta; headingDelta; speedDelta; altitudeDelta; timeDeltaMs;
  {
    opts::FAST, {0.5f, 1.f, 1.f, 1.f, 75}
  },
  {
    opts::MEDIUM, {1, 1.f, 10.f, 10.f, 250}
  },
  {
    opts::LOW, {2, 4.f, 10.f, 100.f, 550}
  }
});

using namespace Marble;
using atools::gui::MapPosHistoryEntry;
using atools::gui::MapPosHistory;
using atools::geo::Rect;
using atools::geo::Pos;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

MapWidget::MapWidget(MainWindow *parent)
  : Marble::MarbleWidget(parent), mainWindow(parent), mapQuery(NavApp::getMapQuery())
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  setMinimumSize(QSize(50, 50));

  // Event filter needed to disable some unwanted Marble default functionality
  installEventFilter(this);

  // Set the map quality to gain speed
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  // All nautical miles and feet for now
  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);

  // Avoid stuttering movements
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapTooltip = new MapTooltip(mainWindow);

  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  screenIndex = new MapScreenIndex(this, mapQuery, paintLayer);

  // Disable all unwante popups on mouse click
  MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);

  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();

  // "Compass" id "compass"
  // "License" id "license"
  // "Scale Bar" id "scalebar"
  // "Navigation" id "navigation"
  // "Overview Map" id "overviewmap"
  mapOverlays.insert("compass", mainWindow->getUi()->actionMapOverlayCompass);
  mapOverlays.insert("scalebar", mainWindow->getUi()->actionMapOverlayScalebar);
  mapOverlays.insert("navigation", mainWindow->getUi()->actionMapOverlayNavigation);
  mapOverlays.insert("overviewmap", mainWindow->getUi()->actionMapOverlayOverview);

  elevationDisplayTimer.setInterval(ALTITUDE_UPDATE_TIMEOUT);
  elevationDisplayTimer.setSingleShot(true);
  connect(&elevationDisplayTimer, &QTimer::timeout, this, &MapWidget::elevationDisplayTimerTimeout);
}

MapWidget::~MapWidget()
{
  qDebug() << Q_FUNC_INFO << "removeEventFilter";
  removeEventFilter(this);

  qDebug() << Q_FUNC_INFO << "delete paintLayer";
  delete paintLayer;

  qDebug() << Q_FUNC_INFO << "delete mapTooltip";
  delete mapTooltip;

  qDebug() << Q_FUNC_INFO << "delete screenIndex";
  delete screenIndex;
}

void MapWidget::setTheme(const QString& theme, int index)
{
  Q_UNUSED(index);
  qDebug() << "setting map theme to " << theme;

  Ui::MainWindow *ui = mainWindow->getUi();
  currentComboIndex = MapWidget::MapThemeComboIndex(index);

  if(index >= MapWidget::CUSTOM)
  {
    // Enable all buttons for custom maps
    ui->actionMapShowCities->setEnabled(true);
    ui->actionMapShowHillshading->setEnabled(true);
  }
  else
  {
    // Update theme specific options
    switch(index)
    {
      case MapWidget::STAMENTERRAIN:
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(false);
        break;
      case MapWidget::OPENTOPOMAP:
        setPropertyValue("ice", false);
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(false);
        break;
      case MapWidget::OPENSTREETMAPROADS:
      case MapWidget::OPENSTREETMAP:
      case MapWidget::CARTODARK:
      case MapWidget::CARTOLIGHT:
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(true);
        break;
      case MapWidget::SIMPLE:
      case MapWidget::PLAIN:
      case MapWidget::ATLAS:
        ui->actionMapShowCities->setEnabled(true);
        ui->actionMapShowHillshading->setEnabled(false);
        break;
      case MapWidget::INVALID:
        qWarning() << "Invalid theme index" << index;
        break;
    }
  }

  setMapThemeId(theme);
  updateMapObjectsShown();

  // atools::gui::Application::processEventsExtended();
  // ignoreOverlayUpdates = false;
  overlayStateFromMenu();
}

void MapWidget::optionsChanged()
{
  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();

  updateCacheSizes();
  update();
}

void MapWidget::updateCacheSizes()
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

void MapWidget::updateMapObjectsShown()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  setShowMapPois(ui->actionMapShowCities->isChecked() &&
                 (currentComboIndex == MapWidget::SIMPLE || currentComboIndex == MapWidget::PLAIN
                  || currentComboIndex == MapWidget::ATLAS));
  setShowGrid(ui->actionMapShowGrid->isChecked());

  setPropertyValue("hillshading", ui->actionMapShowHillshading->isChecked() &&
                   (currentComboIndex == MapWidget::OPENSTREETMAP ||
                    currentComboIndex == MapWidget::OPENSTREETMAPROADS ||
                    currentComboIndex == MapWidget::CARTODARK ||
                    currentComboIndex == MapWidget::CARTOLIGHT ||
                    currentComboIndex >= MapWidget::CUSTOM));

  setShowMapFeatures(map::MISSED_APPROACH, ui->actionInfoApproachShowMissedAppr->isChecked());

  setShowMapFeatures(map::AIRWAYV, ui->actionMapShowVictorAirways->isChecked());
  setShowMapFeatures(map::AIRWAYJ, ui->actionMapShowJetAirways->isChecked());

  setShowMapFeatures(map::AIRSPACE, getShownAirspaces() & map::AIRSPACE_ALL && ui->actionShowAirspaces->isChecked());

  setShowMapFeatures(map::FLIGHTPLAN, ui->actionMapShowRoute->isChecked());
  setShowMapFeatures(map::AIRCRAFT, ui->actionMapShowAircraft->isChecked());
  setShowMapFeatures(map::AIRCRAFT_TRACK, ui->actionMapShowAircraftTrack->isChecked());
  setShowMapFeatures(map::AIRCRAFT_AI, ui->actionMapShowAircraftAi->isChecked());
  setShowMapFeatures(map::AIRCRAFT_AI_SHIP, ui->actionMapShowAircraftAiBoat->isChecked());

  setShowMapFeatures(map::AIRPORT_HARD, ui->actionMapShowAirports->isChecked());
  setShowMapFeatures(map::AIRPORT_SOFT, ui->actionMapShowSoftAirports->isChecked());

  // Force addon airport independent of other settings or not
  setShowMapFeatures(map::AIRPORT_ADDON, ui->actionMapShowAddonAirports->isChecked());

  if(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS)
  {
    // Treat empty airports special
    setShowMapFeatures(map::AIRPORT_EMPTY, ui->actionMapShowEmptyAirports->isChecked());

    // Set the general airport flag if any airport is selected
    setShowMapFeatures(map::AIRPORT,
                       ui->actionMapShowAirports->isChecked() ||
                       ui->actionMapShowSoftAirports->isChecked() ||
                       ui->actionMapShowEmptyAirports->isChecked() ||
                       ui->actionMapShowAddonAirports->isChecked());
  }
  else
  {
    // Treat empty airports as all others
    setShowMapFeatures(map::AIRPORT_EMPTY, true);

    // Set the general airport flag if any airport is selected
    setShowMapFeatures(map::AIRPORT,
                       ui->actionMapShowAirports->isChecked() ||
                       ui->actionMapShowSoftAirports->isChecked() ||
                       ui->actionMapShowAddonAirports->isChecked());
  }

  setShowMapFeatures(map::VOR, ui->actionMapShowVor->isChecked());
  setShowMapFeatures(map::NDB, ui->actionMapShowNdb->isChecked());
  setShowMapFeatures(map::ILS, ui->actionMapShowIls->isChecked());
  setShowMapFeatures(map::WAYPOINT, ui->actionMapShowWp->isChecked());

  updateVisibleObjectsStatusBar();

  emit shownMapFeaturesChanged(paintLayer->getShownMapObjects());

  // Update widget
  update();
}

void MapWidget::setShowMapPois(bool show)
{
  // Enable all POI stuff
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
  setShowTerrain(show);
}

void MapWidget::setShowMapFeatures(map::MapObjectTypes type, bool show)
{
  paintLayer->setShowMapObjects(type, show);

  if(type & map::AIRWAYV || type & map::AIRWAYJ)
    screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);

  if(type & map::AIRSPACE)
    screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

void MapWidget::setShowMapAirspaces(map::MapAirspaceTypes types)
{
  paintLayer->setShowAirspaces(types);
  // setShowMapFeatures(map::AIRSPACE, types & map::AIRSPACE_ALL);
  updateVisibleObjectsStatusBar();
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

void MapWidget::setDetailLevel(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
  updateVisibleObjectsStatusBar();
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

map::MapObjectTypes MapWidget::getShownMapFeatures() const
{
  return paintLayer->getShownMapObjects();
}

map::MapAirspaceTypes MapWidget::getShownAirspaces() const
{
  return paintLayer->getShownAirspaces();
}

map::MapAirspaceTypes MapWidget::getShownAirspaceTypesByLayer() const
{
  return paintLayer->getShownAirspacesTypesByLayer();
}

void MapWidget::getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur)
{
  from = routeDragFrom;
  to = routeDragTo;
  cur = routeDragCur;
}

void MapWidget::preDatabaseLoad()
{
  cancelDragAll();
  databaseLoadStatus = true;
  paintLayer->preDatabaseLoad();
}

void MapWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  paintLayer->postDatabaseLoad();
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  screenIndex->updateRouteScreenGeometry(currentViewBoundingBox);
  update();
  updateVisibleObjectsStatusBar();
}

void MapWidget::historyNext()
{
  const MapPosHistoryEntry& entry = history.next();
  if(entry.isValid())
  {
    setDistance(entry.getDistance());
    centerOn(entry.getPos().getLonX(), entry.getPos().getLatY(), false);
    changedByHistory = true;
    mainWindow->setStatusMessage(tr("Map position history next."));
    showAircraft(false);
  }
}

void MapWidget::historyBack()
{
  const MapPosHistoryEntry& entry = history.back();
  if(entry.isValid())
  {
    setDistance(entry.getDistance());
    centerOn(entry.getPos().getLonX(), entry.getPos().getLatY(), false);
    changedByHistory = true;
    mainWindow->setStatusMessage(tr("Map position history back."));
    showAircraft(false);
  }
}

void MapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  writePluginSettings(*s.getQSettings());
  // Workaround to overviewmap storing absolute paths which will be invalid when moving program location
  s.remove("plugin_overviewmap/path_earth");
  s.remove("plugin_overviewmap/path_jupiter");
  s.remove("plugin_overviewmap/path_mars");
  s.remove("plugin_overviewmap/path_mercury");
  s.remove("plugin_overviewmap/path_moon");
  s.remove("plugin_overviewmap/path_neptune");
  s.remove("plugin_overviewmap/path_pluto");
  s.remove("plugin_overviewmap/path_saturn");
  s.remove("plugin_overviewmap/path_sky");
  s.remove("plugin_overviewmap/path_sun");
  s.remove("plugin_overviewmap/path_uranus");
  s.remove("plugin_overviewmap/path_venus");

  s.setValue(lnm::MAP_MARKLONX, searchMarkPos.getLonX());
  s.setValue(lnm::MAP_MARKLATY, searchMarkPos.getLatY());

  s.setValue(lnm::MAP_HOMELONX, homePos.getLonX());
  s.setValue(lnm::MAP_HOMELATY, homePos.getLatY());
  s.setValue(lnm::MAP_HOMEDISTANCE, homeDistance);
  s.setValue(lnm::MAP_KMLFILES, kmlFilePaths);

  s.setValue(lnm::MAP_DETAILFACTOR, mapDetailLevel);
  s.setValue(lnm::MAP_AIRSPACES, static_cast<int>(paintLayer->getShownAirspaces()));

  history.saveState(atools::settings::Settings::getConfigFilename(".history"));
  screenIndex->saveState();
  aircraftTrack.saveState();

  overlayStateToMenu();
  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : mapOverlays.values())
    state.save(action);
}

void MapWidget::resetSettingActionsToDefault()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->actionMapShowAirports->blockSignals(true);
  ui->actionMapShowAirports->setChecked(true);
  ui->actionMapShowAirports->blockSignals(false);
  ui->actionMapShowSoftAirports->blockSignals(true);
  ui->actionMapShowSoftAirports->setChecked(true);
  ui->actionMapShowSoftAirports->blockSignals(false);
  ui->actionMapShowEmptyAirports->blockSignals(true);
  ui->actionMapShowEmptyAirports->setChecked(true);
  ui->actionMapShowEmptyAirports->blockSignals(false);
  ui->actionMapShowAddonAirports->blockSignals(true);
  ui->actionMapShowAddonAirports->setChecked(true);
  ui->actionMapShowAddonAirports->blockSignals(false);
  ui->actionMapShowVor->blockSignals(true);
  ui->actionMapShowVor->setChecked(true);
  ui->actionMapShowVor->blockSignals(false);
  ui->actionMapShowNdb->blockSignals(true);
  ui->actionMapShowNdb->setChecked(true);
  ui->actionMapShowNdb->blockSignals(false);
  ui->actionMapShowWp->blockSignals(true);
  ui->actionMapShowWp->setChecked(true);
  ui->actionMapShowWp->blockSignals(false);
  ui->actionMapShowIls->blockSignals(true);
  ui->actionMapShowIls->setChecked(true);
  ui->actionMapShowIls->blockSignals(false);
  ui->actionMapShowVictorAirways->blockSignals(true);
  ui->actionMapShowVictorAirways->setChecked(false);
  ui->actionMapShowVictorAirways->blockSignals(false);
  ui->actionMapShowJetAirways->blockSignals(true);
  ui->actionMapShowJetAirways->setChecked(false);
  ui->actionMapShowJetAirways->blockSignals(false);
  ui->actionShowAirspaces->blockSignals(true);
  ui->actionShowAirspaces->setChecked(true);
  ui->actionShowAirspaces->blockSignals(false);
  ui->actionMapShowRoute->blockSignals(true);
  ui->actionMapShowRoute->setChecked(true);
  ui->actionMapShowRoute->blockSignals(false);
  ui->actionMapShowAircraft->blockSignals(true);
  ui->actionMapShowAircraft->setChecked(true);
  ui->actionMapShowAircraft->blockSignals(false);
  ui->actionMapAircraftCenter->blockSignals(true);
  ui->actionMapAircraftCenter->setChecked(true);
  ui->actionMapAircraftCenter->blockSignals(false);
  ui->actionMapShowAircraftAi->blockSignals(true);
  ui->actionMapShowAircraftAi->setChecked(true);
  ui->actionMapShowAircraftAi->blockSignals(false);
  ui->actionMapShowAircraftAiBoat->blockSignals(true);
  ui->actionMapShowAircraftAiBoat->setChecked(false);
  ui->actionMapShowAircraftAiBoat->blockSignals(false);
  ui->actionMapShowAircraftTrack->blockSignals(true);
  ui->actionMapShowAircraftTrack->setChecked(true);
  ui->actionMapShowAircraftTrack->blockSignals(false);
  ui->actionInfoApproachShowMissedAppr->blockSignals(true);
  ui->actionInfoApproachShowMissedAppr->setChecked(true);
  ui->actionInfoApproachShowMissedAppr->blockSignals(false);
}

void MapWidget::resetSettingsToDefault()
{
  paintLayer->setShowAirspaces(map::AIRSPACE_DEFAULT);
  mapDetailLevel = MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailLevel);
}

void MapWidget::restoreState()
{
  qDebug() << Q_FUNC_INFO;
  atools::settings::Settings& s = atools::settings::Settings::instance();

  readPluginSettings(*s.getQSettings());

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
    mapDetailLevel = s.valueInt(lnm::MAP_DETAILFACTOR, MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR);
  else
    mapDetailLevel = MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailLevel);

  if(s.contains(lnm::MAP_MARKLONX) && s.contains(lnm::MAP_MARKLATY))
    searchMarkPos = Pos(s.valueFloat(lnm::MAP_MARKLONX), s.valueFloat(lnm::MAP_MARKLATY));
  else
    searchMarkPos = Pos(0.f, 0.f);

  if(s.contains(lnm::MAP_HOMELONX) && s.contains(lnm::MAP_HOMELATY) && s.contains(lnm::MAP_HOMEDISTANCE))
  {
    homePos = Pos(s.valueFloat(lnm::MAP_HOMELONX), s.valueFloat(lnm::MAP_HOMELATY));
    homeDistance = s.valueFloat(lnm::MAP_HOMEDISTANCE);
  }
  else
  {
    // Looks like first start after installation
    homePos = Pos(0.f, 0.f);
    homeDistance = DEFAULT_MAP_DISTANCE;
  }

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_KML)
    kmlFilePaths = s.valueStrList(lnm::MAP_KMLFILES);
  screenIndex->restoreState();
  aircraftTrack.restoreState();

  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : mapOverlays.values())
    state.restore(action);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
    paintLayer->setShowAirspaces(static_cast<map::MapAirspaceTypes>(
                                   s.valueVar(lnm::MAP_AIRSPACES, map::AIRSPACE_DEFAULT).toInt()));
  else
    paintLayer->setShowAirspaces(map::AIRSPACE_DEFAULT);

  restoreHistoryState();
}

void MapWidget::restoreHistoryState()
{
  history.restoreState(atools::settings::Settings::getConfigFilename(".history"));
}

void MapWidget::showOverlays(bool show)
{
  for(const QString& name : mapOverlays.keys())
  {
    AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
    {
      bool showConfig = mapOverlays.value(name)->isChecked();

      overlay->blockSignals(true);

      if(show && showConfig)
      {
        // qDebug() << "showing float item" << overlay->name() << "id" << overlay->nameId();
        overlay->setVisible(true);
        overlay->show();
      }
      else
      {
        // qDebug() << "hiding float item" << overlay->name() << "id" << overlay->nameId();
        overlay->setVisible(false);
        overlay->hide();
      }
      overlay->blockSignals(false);
    }
  }
}

void MapWidget::overlayStateToMenu()
{
  qDebug() << Q_FUNC_INFO;
  for(const QString& name : mapOverlays.keys())
  {
    AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
    {
      QAction *menuItem = mapOverlays.value(name);
      menuItem->blockSignals(true);
      menuItem->setChecked(overlay->visible());
      menuItem->blockSignals(false);
    }
  }
}

void MapWidget::overlayStateFromMenu()
{
  qDebug() << Q_FUNC_INFO;

  for(const QString& name : mapOverlays.keys())
  {
    AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
    {
      bool show = mapOverlays.value(name)->isChecked();
      overlay->setVisible(show);
      if(show)
      {
        // qDebug() << "showing float item" << overlay->name() << "id" << overlay->nameId();
        setPropertyValue(overlay->nameId(), true);
        overlay->show();
      }
      else
      {
        // qDebug() << "hiding float item" << overlay->name() << "id" << overlay->nameId();
        setPropertyValue(overlay->nameId(), false);
        overlay->hide();
      }
    }
  }
}

void MapWidget::connectOverlayMenus()
{
  for(QAction *action : mapOverlays.values())
    connect(action, &QAction::toggled, this, &MapWidget::overlayStateFromMenu);

  for(const QString& name : mapOverlays.keys())
  {
    AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
      connect(overlay, &Marble::AbstractFloatItem::visibilityChanged, this, &MapWidget::overlayStateToMenu);
  }
}

void MapWidget::mainWindowShown()
{
  qDebug() << Q_FUNC_INFO;

  // Create a copy of KML files where all missing files will be removed from the recent list
  QStringList copyKml(kmlFilePaths);
  for(const QString& kml : kmlFilePaths)
  {
    if(!loadKml(kml, false /* center */))
      copyKml.removeAll(kml);
  }

  kmlFilePaths = copyKml;

  // Set cache sizes from option data. This is done later in the startup process to avoid disk trashing.
  updateCacheSizes();

  overlayStateFromMenu();
  connectOverlayMenus();
  emit searchMarkChanged(searchMarkPos);
}

void MapWidget::showSavedPosOnStartup()
{
  qDebug() << Q_FUNC_INFO;

  active = true;

  const MapPosHistoryEntry& currentPos = history.current();

  if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_ROUTE)
  {
    qDebug() << "Show Route" << NavApp::getRoute().getBoundingRect();
    if(!NavApp::getRoute().isFlightplanEmpty())
      showRect(NavApp::getRoute().getBoundingRect(), false);
    else
      showHome();
  }
  else if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_HOME)
    showHome();
  else if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_LAST)
  {
    if(currentPos.isValid())
    {
      qDebug() << "Show Last" << currentPos;
      centerOn(currentPos.getPos().getLonX(), currentPos.getPos().getLatY(), false);
      setDistance(currentPos.getDistance());
    }
    else
    {
      qDebug() << "Show 0,0" << currentPos;
      centerOn(0.f, 0.f, false);
      setDistance(DEFAULT_MAP_DISTANCE);
    }
  }
  history.activate();
}

void MapWidget::showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick)
{
  if(!pos.isValid())
  {
    qWarning() << Q_FUNC_INFO << "Invalid position";
    return;
  }

  hideTooltip();
  showAircraft(false);

  float dst = zoom;

  if(dst == 0.f)
    dst = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                        OptionData::instance().getMapZoomShowClick() :
                                        OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

  setDistance(dst);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void MapWidget::showRect(const atools::geo::Rect& rect, bool doubleClick)
{
  qDebug() << Q_FUNC_INFO << rect;

  hideTooltip();
  showAircraft(false);

  float w = rect.getWidthDegree();
  float h = rect.getHeightDegree();

  qDebug() << "rect w" << QString::number(w, 'f')
           << "h" << QString::number(h, 'f');

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
    {
      // Workaround for marble not being able to center certain lines
      // Turn rect into a square
      centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(),
                                rect.getEast() + h / 2,
                                rect.getWest() - h / 2,
                                GeoDataCoordinates::Degree), false);
    }
    else if(atools::almostEqual(h, 0.f, POS_IS_POINT_EPSILON))
    {
      centerOn(GeoDataLatLonBox(rect.getNorth() + w / 2,
                                rect.getSouth() - w / 2,
                                rect.getEast(),
                                rect.getWest(),
                                GeoDataCoordinates::Degree), false);
    }
    else
      centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                                GeoDataCoordinates::Degree), false);

    float dist = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                               OptionData::instance().getMapZoomShowClick() :
                                               OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

    if(distance() < dist)
      setDistance(dist);
  }
}

void MapWidget::showSearchMark()
{
  qDebug() << "NavMapWidget::showMark" << searchMarkPos;

  hideTooltip();
  showAircraft(false);

  if(searchMarkPos.isValid())
  {
    setDistance(atools::geo::nmToKm(Unit::rev(OptionData::instance().getMapZoomShowMenu(), Unit::distNmF)));
    centerOn(searchMarkPos.getLonX(), searchMarkPos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showAircraft(bool centerAircraftChecked)
{
  qDebug() << "NavMapWidget::showAircraft" << screenIndex->getUserAircraft().getPosition();

  // Adapt the menu item status if this method was not called by the action
  QAction *acAction = mainWindow->getUi()->actionMapAircraftCenter;
  if(acAction->isEnabled())
  {
    acAction->blockSignals(true);
    acAction->setChecked(centerAircraftChecked);
    acAction->blockSignals(false);
    qDebug() << "Aircraft center set to" << centerAircraftChecked;
  }

  if(centerAircraftChecked && screenIndex->getUserAircraft().getPosition().isValid())
    centerOn(screenIndex->getUserAircraft().getPosition().getLonX(),
             screenIndex->getUserAircraft().getPosition().getLatY(), false);
}

void MapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << homePos;

  hideTooltip();
  showAircraft(false);
  if(!atools::almostEqual(homeDistance, 0.))
    // Only center position is valid
    setDistance(homeDistance);

  if(homePos.isValid())
  {
    centerOn(homePos.getLonX(), homePos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing home position."));
  }
}

void MapWidget::changeSearchMark(const atools::geo::Pos& pos)
{
  searchMarkPos = pos;

  // Will update any active distance search
  emit searchMarkChanged(searchMarkPos);
  update();
  mainWindow->setStatusMessage(tr("Distance search center position changed."));
}

void MapWidget::changeHome()
{
  homePos = Pos(centerLongitude(), centerLatitude());
  homeDistance = distance();
  update();
  mainWindow->setStatusMessage(QString(tr("Changed home position.")));
}

void MapWidget::changeRouteHighlights(const QList<int>& routeHighlight)
{
  screenIndex->setRouteHighlights(routeHighlight);
  update();
}

void MapWidget::routeChanged(bool geometryChanged)
{
  qDebug() << Q_FUNC_INFO;

  if(geometryChanged)
  {
    cancelDragAll();
    screenIndex->updateRouteScreenGeometry(currentViewBoundingBox);
    update();
  }
}

void MapWidget::routeAltitudeChanged(float altitudeFeet)
{
  Q_UNUSED(altitudeFeet);

  if(databaseLoadStatus)
    return;

  qDebug() << Q_FUNC_INFO;
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  update();
}

void MapWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = simulatorData.getUserAircraft();
  if(databaseLoadStatus || !userAircraft.getPosition().isValid())
    return;

  screenIndex->updateSimData(simulatorData);
  const atools::fs::sc::SimConnectUserAircraft& lastUserAircraft = screenIndex->getLastUserAircraft();

  CoordinateConverter conv(viewport());
  QPointF curPos = conv.wToSF(userAircraft.getPosition());
  QPointF diff = curPos - conv.wToSF(lastUserAircraft.getPosition());

  bool wasEmpty = aircraftTrack.isEmpty();
  bool trackPruned = aircraftTrack.appendTrackPos(userAircraft.getPosition(),
                                                  userAircraft.getZuluTime(), userAircraft.isOnGround());

  if(trackPruned)
    emit aircraftTrackPruned();

  if(wasEmpty != aircraftTrack.isEmpty())
    // We have a track - update toolbar and menu
    emit updateActionStates();

  if(paintLayer->getShownMapObjects() & map::AIRCRAFT || paintLayer->getShownMapObjects() & map::AIRCRAFT_AI)
  {
    // Show aircraft is enabled
    bool centerAircraft = mainWindow->getUi()->actionMapAircraftCenter->isChecked();

    // Get delta values for update rate
    const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

    // Limit number of updates per second
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if(now - lastSimUpdateMs > deltas.timeDeltaMs)
    {
      lastSimUpdateMs = now;

      // Check if any AI aircraft are visible
      bool aiVisible = false;
      if(paintLayer->getShownMapObjects() & map::AIRCRAFT_AI)
      {
        for(const atools::fs::sc::SimConnectAircraft& ai : simulatorData.getAiAircraft())
        {
          if(currentViewBoundingBox.contains(
               Marble::GeoDataCoordinates(ai.getPosition().getLonX(), ai.getPosition().getLatY(), 0,
                                          Marble::GeoDataCoordinates::Degree)))
          {
            aiVisible = true;
            break;
          }
        }
      }

      using atools::almostNotEqual;
      if(!lastUserAircraft.getPosition().isValid() ||
         diff.manhattanLength() >= deltas.manhattanLengthDelta || // Screen position has changed
         almostNotEqual(lastUserAircraft.getHeadingDegMag(),
                        userAircraft.getHeadingDegMag(), deltas.headingDelta) || // Heading has changed
         almostNotEqual(lastUserAircraft.getIndicatedSpeedKts(),
                        userAircraft.getIndicatedSpeedKts(),
                        deltas.speedDelta) || // Speed has changed
         almostNotEqual(lastUserAircraft.getPosition().getAltitude(),
                        userAircraft.getPosition().getAltitude(),
                        deltas.altitudeDelta) || // Altitude has changed
         (curPos.isNull() && centerAircraft) || // Not visible on world map but centering required
         (!rect().contains(curPos.toPoint()) && centerAircraft) || // Not in screen rectangle but centering required
         aiVisible) // Paint always for visible AI
      {
        screenIndex->updateLastSimData(simulatorData);

        // Calculate the amount that has to be substracted from each side of the rectangle
        float boxFactor = (100.f - OptionData::instance().getSimUpdateBox()) / 100.f / 2.f;
        int dx = static_cast<int>(width() * boxFactor);
        int dy = static_cast<int>(height() * boxFactor);

        QRect widgetRect = rect();
        widgetRect.adjust(dx, dy, -dx, -dy);

        if(mouseState == mw::NONE && viewContext() == Marble::Still)
        {
          if((!widgetRect.contains(curPos.toPoint()) || // Aircraft out of box or ...
              OptionData::instance().getFlags() & opts::SIM_UPDATE_MAP_CONSTANTLY) && // ... update always
             centerAircraft) // Centering wanted
            centerOn(userAircraft.getPosition().getLonX(), userAircraft.getPosition().getLatY(), false);
          else
            update();
        }
      }
    }
  }
  else if(paintLayer->getShownMapObjects() & map::AIRCRAFT_TRACK)
  {
    // No aircraft but track - update track only
    if(!lastUserAircraft.getPosition().isValid() || diff.manhattanLength() > 4)
    {
      screenIndex->updateLastSimData(simulatorData);
      update();
    }
  }
}

void MapWidget::highlightProfilePoint(const atools::geo::Pos& pos)
{
  if(pos.isValid())
  {
    CoordinateConverter conv(viewport());
    int x, y;
    if(conv.wToS(pos, x, y))
    {
      if(profileRubberRect == nullptr)
        profileRubberRect = new QRubberBand(QRubberBand::Rectangle, this);

      const QRect& rubberRect = profileRubberRect->geometry();

      if(rubberRect.x() != x - 6 || rubberRect.y() != y - 6)
      {
        // Update only if position has changed
        profileRubberRect->setGeometry(x - 6, y - 6, 12, 12);
        profileRubberRect->show();
      }

      return;
    }
  }

  delete profileRubberRect;
  profileRubberRect = nullptr;
}

void MapWidget::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  // aircraftTrack.clearTrack();
  update();
}

void MapWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  // Clear all data on disconnect
  screenIndex->updateSimData(atools::fs::sc::SimConnectData());
  updateVisibleObjectsStatusBar();
  update();
}

bool MapWidget::addKmlFile(const QString& kmlFile)
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

void MapWidget::clearKmlFiles()
{
  for(const QString& file : kmlFilePaths)
    model()->removeGeoData(file);
  kmlFilePaths.clear();
}

const map::MapSearchResult& MapWidget::getSearchHighlights() const
{
  return screenIndex->getSearchHighlights();
}

const proc::MapProcedureLeg& MapWidget::getProcedureLegHighlights() const
{
  return screenIndex->getApproachLegHighlights();
}

const proc::MapProcedureLegs& MapWidget::getProcedureHighlight() const
{
  return screenIndex->getProcedureHighlight();
}

void MapWidget::changeApproachHighlight(const proc::MapProcedureLegs& approach)
{
  cancelDragAll();
  screenIndex->getProcedureHighlight() = approach;
  screenIndex->updateRouteScreenGeometry(currentViewBoundingBox);
  update();
}

void MapWidget::changeSearchHighlights(const map::MapSearchResult& positions)
{
  screenIndex->getSearchHighlights() = positions;
  update();
}

void MapWidget::changeProcedureLegHighlights(const proc::MapProcedureLeg *leg)
{
  screenIndex->setApproachLegHighlights(leg);
  update();
}

/* Update the flight plan from a drag and drop result. Show a menu if multiple objects are
 * found at the button release position. */
void MapWidget::updateRouteFromDrag(QPoint newPoint, mw::MouseStates state, int leg, int point)
{
  qDebug() << "End route drag" << newPoint << "state" << state << "leg" << leg << "point" << point;

  // Get all objects where the mouse button was released
  map::MapSearchResult result;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(newPoint.x(), newPoint.y(), screenSearchDistance, result, procPoints);

  CoordinateConverter conv(viewport());

  // Get objects from cache - already present objects will be skipped
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(), false,
                              paintLayer->getShownMapObjects() &
                              (map::AIRPORT_ALL | map::VOR | map::NDB | map::WAYPOINT),
                              newPoint.x(), newPoint.y(), screenSearchDistance, result);

  int totalSize = result.airports.size() + result.vors.size() + result.ndbs.size() + result.waypoints.size();

  int id = -1;
  map::MapObjectTypes type = map::NONE;
  Pos pos = atools::geo::EMPTY_POS;
  if(totalSize == 0)
  {
    // Nothing at the position - add userpoint
    qDebug() << "add userpoint";
    type = map::USER;
  }
  else if(totalSize == 1)
  {
    // Only one entry at the position - add single navaid without menu
    qDebug() << "add navaid";

    if(!result.airports.isEmpty())
    {
      id = result.airports.first().id;
      type = map::AIRPORT;
    }
    else if(!result.vors.isEmpty())
    {
      id = result.vors.first().id;
      type = map::VOR;
    }
    else if(!result.ndbs.isEmpty())
    {
      id = result.ndbs.first().id;
      type = map::NDB;
    }
    else if(!result.waypoints.isEmpty())
    {
      id = result.waypoints.first().id;
      type = map::WAYPOINT;
    }
  }
  else
  {
    // Avoid drag cancel when loosing focus
    mouseState |= mw::DRAG_POST_MENU;

    // Multiple entries - build a menu with icons
    // Add id and type to actions
    const int ICON_SIZE = 20;
    qDebug() << "add navaids" << totalSize;
    QMenu menu;
    QString menuPrefix(tr("Add ")), menuSuffix(tr(" to Flight Plan"));
    SymbolPainter symbolPainter;

    for(const map::MapAirport& obj : result.airports)
    {
      QAction *action = new QAction(symbolPainter.createAirportIcon(obj, ICON_SIZE),
                                    menuPrefix + map::airportText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, map::AIRPORT}));
      menu.addAction(action);
    }

    if(!result.airports.isEmpty() || !result.vors.isEmpty() || !result.ndbs.isEmpty() ||
       !result.waypoints.isEmpty())
      // There will be more entries - add a separator
      menu.addSeparator();

    for(const map::MapVor& obj : result.vors)
    {
      QAction *action = new QAction(symbolPainter.createVorIcon(obj, ICON_SIZE),
                                    menuPrefix + map::vorText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, map::VOR}));
      menu.addAction(action);
    }
    for(const map::MapNdb& obj : result.ndbs)
    {
      QAction *action = new QAction(symbolPainter.createNdbIcon(ICON_SIZE),
                                    menuPrefix + map::ndbText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, map::NDB}));
      menu.addAction(action);
    }
    for(const map::MapWaypoint& obj : result.waypoints)
    {
      QAction *action = new QAction(symbolPainter.createWaypointIcon(ICON_SIZE),
                                    menuPrefix + map::waypointText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, map::WAYPOINT}));
      menu.addAction(action);
    }

    // Always present - userpoint
    menu.addSeparator();
    {
      QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                    menuPrefix + "Userpoint" + menuSuffix, this);
      action->setData(QVariantList({-1, map::USER}));
      menu.addAction(action);
    }

    // Always present - userpoint
    menu.addSeparator();
    menu.addAction(new QAction(QIcon(":/littlenavmap/resources/icons/cancel.svg"),
                               tr("Cancel"), this));

    // Execute the menu
    QAction *action = menu.exec(QCursor::pos());

    if(action != nullptr && !action->data().isNull())
    {
      // Get id and type from selected action
      QVariantList data = action->data().toList();
      id = data.first().toInt();
      type = map::MapObjectTypes(data.at(1).toInt());
    }

    mouseState &= ~mw::DRAG_POST_MENU;
  }

  if(type == map::USER)
    // Get position for new user point from from screen
    pos = conv.sToW(newPoint.x(), newPoint.y());

  if((id != -1 && type != map::NONE) || type == map::USER)
  {
    if(leg != -1)
      emit routeAdd(id, pos, type, leg);
    else if(point != -1)
      emit routeReplace(id, pos, type, point);
  }
}

void MapWidget::contextMenuEvent(QContextMenuEvent *event)
{
  qDebug() << "contextMenuEvent state" << mouseState
           << "modifiers" << event->modifiers() << "reason" << event->reason()
           << "pos" << event->pos();

  if(mouseState != mw::NONE)
    return;

  QPoint point;
  if(event->reason() == QContextMenuEvent::Keyboard)
    // Event does not contain position if triggered by keyboard
    point = mapFromGlobal(QCursor::pos());
  else
    point = event->pos();

  QPoint menuPos = QCursor::pos();
  // Do not show context menu if point is not on the map
  if(!rect().contains(point))
  {
    menuPos = mapToGlobal(rect().center());
    point = QPoint();
  }

  hideTooltip();

  Ui::MainWindow *ui = mainWindow->getUi();

  // Texts with % will be replaced save them and let the ActionTextSaver restore them on return
  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch, ui->actionRouteAddPos, ui->actionRouteAppendPos,
                                          ui->actionMapShowInformation, ui->actionMapShowApproaches,
                                          ui->actionRouteDeleteWaypoint, ui->actionRouteAirportStart,
                                          ui->actionRouteAirportDest,
                                          ui->actionMapEditUserWaypoint});
  Q_UNUSED(textSaver);

  // Build menu - add actions
  QMenu menu;
  menu.addAction(ui->actionMapShowInformation);
  menu.addAction(ui->actionMapShowApproaches);
  menu.addSeparator();

  menu.addAction(ui->actionMapMeasureDistance);
  menu.addAction(ui->actionMapMeasureRhumbDistance);
  menu.addAction(ui->actionMapHideDistanceMarker);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideOneRangeRing);
  menu.addAction(ui->actionMapHideRangeRings);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteAirportDest);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAddPos);
  menu.addAction(ui->actionRouteAppendPos);
  menu.addAction(ui->actionRouteDeleteWaypoint);
  menu.addAction(ui->actionMapEditUserWaypoint);
  menu.addSeparator();

  menu.addAction(ui->actionShowInSearch);
  menu.addSeparator();

  menu.addAction(ui->actionMapSetMark);
  menu.addAction(ui->actionMapSetHome);

  int distMarkerIndex = -1;
  int rangeMarkerIndex = -1;
  bool visibleOnMap = false;
  Pos pos;

  if(!point.isNull())
  {
    qreal lon, lat;
    // Cursor can be outside or map region
    visibleOnMap = geoCoordinates(point.x(), point.y(), lon, lat);

    if(visibleOnMap)
    {
      pos = Pos(lon, lat);
      distMarkerIndex = screenIndex->getNearestDistanceMarkIndex(point.x(), point.y(), screenSearchDistance);
      rangeMarkerIndex = screenIndex->getNearestRangeMarkIndex(point.x(), point.y(), screenSearchDistance);
    }
  }

  // Disable all menu items that depend on position
  ui->actionMapSetMark->setEnabled(visibleOnMap);
  ui->actionMapSetHome->setEnabled(visibleOnMap);
  ui->actionMapMeasureDistance->setEnabled(visibleOnMap);
  ui->actionMapMeasureRhumbDistance->setEnabled(visibleOnMap);
  ui->actionMapRangeRings->setEnabled(visibleOnMap);

  ui->actionMapHideRangeRings->setEnabled(!screenIndex->getRangeMarks().isEmpty() ||
                                          !screenIndex->getDistanceMarks().isEmpty());
  ui->actionMapHideOneRangeRing->setEnabled(visibleOnMap && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visibleOnMap && distMarkerIndex != -1);

  ui->actionMapShowInformation->setEnabled(false);
  ui->actionMapShowApproaches->setEnabled(false);
  ui->actionMapNavaidRange->setEnabled(false);
  ui->actionShowInSearch->setEnabled(false);
  ui->actionRouteAddPos->setEnabled(visibleOnMap);
  ui->actionRouteAppendPos->setEnabled(visibleOnMap);
  ui->actionRouteAirportStart->setEnabled(false);
  ui->actionRouteAirportDest->setEnabled(false);
  ui->actionRouteDeleteWaypoint->setEnabled(false);

  ui->actionMapEditUserWaypoint->setEnabled(false);

  // Get objects near position
  map::MapSearchResult result;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDistance, result, procPoints);

  map::MapAirport *airport = nullptr;
  SimConnectAircraft *aiAircraft = nullptr;
  SimConnectUserAircraft *userAircraft = nullptr;
  map::MapVor *vor = nullptr;
  map::MapNdb *ndb = nullptr;
  map::MapWaypoint *waypoint = nullptr;
  map::MapUserpoint *userpoint = nullptr;
  map::MapAirway *airway = nullptr;
  map::MapParking *parking = nullptr;
  map::MapAirspace *airspace = nullptr;

  // Get only one object of each type
  if(result.userAircraft.getPosition().isValid())
    userAircraft = &result.userAircraft;
  if(!result.aiAircraft.isEmpty())
    aiAircraft = &result.aiAircraft.first();
  if(!result.airports.isEmpty())
    airport = &result.airports.first();
  if(!result.parkings.isEmpty())
    parking = &result.parkings.first();
  if(!result.vors.isEmpty())
    vor = &result.vors.first();
  if(!result.ndbs.isEmpty())
    ndb = &result.ndbs.first();
  if(!result.waypoints.isEmpty())
    waypoint = &result.waypoints.first();
  if(!result.userPoints.isEmpty())
    userpoint = &result.userPoints.first();
  if(!result.airways.isEmpty())
    airway = &result.airways.first();
  if(!result.airspaces.isEmpty())
    airspace = &result.airspaces.first();

  // Add "more" text if multiple navaids will be added to the information panel
  bool andMore = (result.vors.size() + result.ndbs.size() + result.waypoints.size() +
                  result.userPoints.size() + result.airways.size()) > 1 && airport == nullptr;

  bool isAircraft = false;
  // Collect information from the search result - build text only for one object for several menu items
  QString informationText, measureText, rangeRingText, departureText, departureParkingText, destinationText,
          addRouteText, searchText;

  if(airspace != nullptr)
    informationText = tr("Airspace");

  if(airway != nullptr)
    informationText = map::airwayText(*airway);

  if(userpoint != nullptr)
    // No show information on user point
    informationText.clear();

  if(waypoint != nullptr)
    informationText = measureText = addRouteText = searchText = map::waypointText(*waypoint);

  if(ndb != nullptr)
    informationText = measureText = rangeRingText = addRouteText = searchText = map::ndbText(*ndb);

  if(vor != nullptr)
    informationText = measureText = rangeRingText = addRouteText = searchText = map::vorText(*vor);

  if(airport != nullptr)
    informationText = measureText = departureText
                                      = destinationText = addRouteText = searchText = map::airportText(*airport);

  if(parking != nullptr)
  {
    departureParkingText = map::parkingName(parking->name) + " " + QLocale().toString(parking->number);
    informationText.clear();
    measureText.clear();
    rangeRingText.clear();
    destinationText.clear();
    addRouteText.clear();
    searchText.clear();
  }

  if(aiAircraft != nullptr)
  {
    informationText = tr("%1 / %2").arg(aiAircraft->getAirplaneRegistration()).arg(
      aiAircraft->getAirplaneModel());
    isAircraft = true;
  }

  if(userAircraft != nullptr)
  {
    informationText = tr("User Aircraft");
    isAircraft = true;
  }

  // Build "delete from flight plan" text
  int routeIndex = -1;
  map::MapObjectTypes deleteType = map::NONE;
  QString routeText;
  if(airport != nullptr && airport->routeIndex != -1)
  {
    routeText = map::airportText(*airport);
    routeIndex = airport->routeIndex;
    deleteType = map::AIRPORT;
  }
  else if(vor != nullptr && vor->routeIndex != -1)
  {
    routeText = map::vorText(*vor);
    routeIndex = vor->routeIndex;
    deleteType = map::VOR;
  }
  else if(ndb != nullptr && ndb->routeIndex != -1)
  {
    routeText = map::ndbText(*ndb);
    routeIndex = ndb->routeIndex;
    deleteType = map::NDB;
  }
  else if(waypoint != nullptr && waypoint->routeIndex != -1)
  {
    routeText = map::waypointText(*waypoint);
    routeIndex = waypoint->routeIndex;
    deleteType = map::WAYPOINT;
  }
  else if(userpoint != nullptr && userpoint->routeIndex != -1)
  {
    routeText = map::userpointText(*userpoint);
    routeIndex = userpoint->routeIndex;
    deleteType = map::USER;
  }

  // Update "set airport as start/dest"
  if(airport != nullptr || parking != nullptr)
  {
    QString airportText(departureText);

    if(parking != nullptr)
    {
      // Get airport for parking
      map::MapAirport parkAp;
      mapQuery->getAirportById(parkAp, parking->airportId);
      airportText = map::airportText(parkAp) + " / ";
    }

    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(
      ui->actionRouteAirportStart->text().arg(airportText + departureParkingText));

    if(airport != nullptr)
    {
      ui->actionRouteAirportDest->setEnabled(true);
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(destinationText));
    }
    else
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }
  else
  {
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // Update "show information" for airports, navaids, airways and airspaces
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     airway != nullptr || airspace != nullptr)
  {
    ui->actionMapShowInformation->setEnabled(true);
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().
                                          arg(informationText + (andMore ? tr(" and more") : QString())));
  }
  else
  {
    if(isAircraft)
    {
      ui->actionMapShowInformation->setEnabled(true);
      ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(informationText));
    }
    else
      ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(QString()));
  }

  // Update "show in search" and "add to route" only for airports an navaids
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(searchText));
    ui->actionRouteAddPos->setEnabled(true);
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(addRouteText));
    ui->actionRouteAppendPos->setEnabled(true);
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(addRouteText));
  }
  else
  {
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(tr("Position")));
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(tr("Position")));
  }

  if(airport != nullptr && (airport->flags & map::AP_PROCEDURE))
  {
    ui->actionMapShowApproaches->setEnabled(true);
    ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(informationText));
  }
  else
    ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(QString()));

  // Update "delete in route"
  if(routeIndex != -1 && NavApp::getRoute().canEditPoint(routeIndex))
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(routeText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(QString()));

  // Update "name user waypoint"
  if(routeIndex != -1 && userpoint != nullptr)
  {
    ui->actionMapEditUserWaypoint->setEnabled(true);
    ui->actionMapEditUserWaypoint->setText(ui->actionMapEditUserWaypoint->text().arg(routeText));
  }
  else
    ui->actionMapEditUserWaypoint->setText(ui->actionMapEditUserWaypoint->text().arg(tr("User Point")));

  // Update "show range rings for Navaid"
  if(vor != nullptr || ndb != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(rangeRingText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && !measureText.isEmpty())
  {
    // Set text to measure "from airport" etc.
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(measureText));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(measureText));
  }
  else
  {
    // Noting found at cursor - use "measure from here"
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(tr("here")));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(tr("here")));
  }

  // Show the menu ------------------------------------------------
  QAction *action = menu.exec(menuPos);

  if(action != nullptr)
    qDebug() << Q_FUNC_INFO << "selected" << action->text();
  else
    qDebug() << Q_FUNC_INFO << "no action selected";

  if(action == ui->actionMapHideRangeRings)
  {
    clearRangeRingsAndDistanceMarkers();
    update();
  }

  if(visibleOnMap)
  {
    if(action == ui->actionShowInSearch)
    {
      ui->dockWidgetSearch->raise();
      ui->dockWidgetSearch->show();
      if(airport != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(0);
        emit showInSearch(map::AIRPORT, airport->ident, QString(), QString());
      }
      else if(vor != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(map::VOR, vor->ident, vor->region, QString() /*, vor->airportIdent*/);
      }
      else if(ndb != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(map::NDB, ndb->ident, ndb->region, QString() /*, ndb->airportIdent*/);
      }
      else if(waypoint != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(map::WAYPOINT, waypoint->ident, waypoint->region, QString() /*, waypoint->airportIdent*/);
      }
    }
    else if(action == ui->actionMapNavaidRange)
    {
      if(vor != nullptr)
        addNavRangeRing(vor->position, map::VOR, vor->ident, vor->getFrequencyOrChannel(), vor->range);
      else if(ndb != nullptr)
        addNavRangeRing(ndb->position, map::NDB, ndb->ident, QString::number(ndb->frequency), ndb->range);
    }
    else if(action == ui->actionMapRangeRings)
      addRangeRing(pos);
    else if(action == ui->actionMapSetMark)
      changeSearchMark(pos);
    else if(action == ui->actionMapHideOneRangeRing)
    {
      screenIndex->getRangeMarks().removeAt(rangeMarkerIndex);
      mainWindow->setStatusMessage(QString(tr("Range ring removed from map.")));
      update();
    }
    else if(action == ui->actionMapHideDistanceMarker)
    {
      screenIndex->getDistanceMarks().removeAt(distMarkerIndex);
      mainWindow->setStatusMessage(QString(tr("Measurement line removed from map.")));
      update();
    }
    else if(action == ui->actionMapMeasureDistance || action == ui->actionMapMeasureRhumbDistance)
    {
      // Distance line
      map::DistanceMarker dm;
      dm.isRhumbLine = action == ui->actionMapMeasureRhumbDistance;
      dm.to = pos;

      // Build distance line depending on selected airport or navaid (color, magvar, etc.)
      if(airport != nullptr)
      {
        dm.text = airport->name + " (" + airport->ident + ")";
        dm.from = airport->position;
        dm.magvar = airport->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::colorForAirport(*airport);
      }
      else if(vor != nullptr)
      {
        if(vor->tacan)
          dm.text = vor->ident + " " + vor->channel;
        else
          dm.text = vor->ident + " " + QLocale().toString(vor->frequency / 1000., 'f', 2);
        dm.from = vor->position;
        dm.magvar = vor->magvar;
        dm.hasMagvar = !vor->dmeOnly;
        dm.color = mapcolors::vorSymbolColor;
      }
      else if(ndb != nullptr)
      {
        dm.text = ndb->ident + " " + QLocale().toString(ndb->frequency / 100., 'f', 2);
        dm.from = ndb->position;
        dm.magvar = ndb->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::ndbSymbolColor;
      }
      else if(waypoint != nullptr)
      {
        dm.text = waypoint->ident;
        dm.from = waypoint->position;
        dm.magvar = waypoint->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::waypointSymbolColor;
      }
      else
      {
        dm.hasMagvar = false;
        dm.from = pos;
        dm.color = dm.isRhumbLine ? mapcolors::distanceRhumbColor : mapcolors::distanceColor;
      }

      screenIndex->getDistanceMarks().append(dm);

      // Start mouse dragging and disable context menu so we can catch the right button click as cancel
      mouseState = mw::DRAG_DISTANCE;
      setContextMenuPolicy(Qt::PreventContextMenu);
      currentDistanceMarkerIndex = screenIndex->getDistanceMarks().size() - 1;
    }
    else if(action == ui->actionRouteDeleteWaypoint)
      NavApp::getRouteController()->routeDelete(routeIndex);
    else if(action == ui->actionMapEditUserWaypoint)
      NavApp::getRouteController()->editUserWaypointName(routeIndex);
    else if(action == ui->actionRouteAddPos || action == ui->actionRouteAppendPos ||
            action == ui->actionRouteAirportStart ||
            action == ui->actionRouteAirportDest || action == ui->actionMapShowInformation)
    {
      Pos position = pos;
      map::MapObjectTypes type;

      int id = -1;
      if(airport != nullptr)
      {
        id = airport->id;
        type = map::AIRPORT;
      }
      else if(parking != nullptr)
      {
        id = parking->id;
        type = map::PARKING;
      }
      else if(vor != nullptr)
      {
        id = vor->id;
        type = map::VOR;
      }
      else if(ndb != nullptr)
      {
        id = ndb->id;
        type = map::NDB;
      }
      else if(waypoint != nullptr)
      {
        id = waypoint->id;
        type = map::WAYPOINT;
      }
      else
      {
        if(userpoint != nullptr)
          id = userpoint->id;
        type = map::USER;
        position = pos;
      }

      if(action == ui->actionRouteAirportStart && parking != nullptr)
        emit routeSetParkingStart(*parking);
      else if(action == ui->actionRouteAddPos || action == ui->actionRouteAppendPos)
      {
        if(parking != nullptr)
        {
          // Adjust values in case user selected "add" on a parking position
          type = map::USER;
          id = -1;
        }

        if(action == ui->actionRouteAddPos)
          emit routeAdd(id, position, type, -1 /* leg index */);
        else if(action == ui->actionRouteAppendPos)
          emit routeAdd(id, position, type, map::INVALID_INDEX_VALUE);
      }
      else if(action == ui->actionRouteAirportStart)
        emit routeSetStart(*airport);
      else if(action == ui->actionRouteAirportDest)
        emit routeSetDest(*airport);
      else if(action == ui->actionMapShowInformation)
        emit showInformation(result);
    }
    else if(action == ui->actionMapShowApproaches)
      emit showApproaches(*airport);
  }
}

void MapWidget::addNavRangeRing(const atools::geo::Pos& pos, map::MapObjectTypes type,
                                const QString& ident, const QString& frequency, int range)
{
  map::RangeMarker ring;
  ring.type = type;
  ring.center = pos;

  if(type == map::VOR)
  {
    if(frequency.endsWith("X") || frequency.endsWith("Y"))
      ring.text = ident + " " + frequency;
    else
      ring.text = ident + " " + QString::number(frequency.toFloat() / 1000., 'f', 2);
  }
  else if(type == map::NDB)
    ring.text = ident + " " + QString::number(frequency.toFloat() / 100., 'f', 2);

  ring.ranges.append(range);
  screenIndex->getRangeMarks().append(ring);
  qDebug() << "navaid range" << ring.center;
  update();
  mainWindow->setStatusMessage(tr("Added range rings for %1.").arg(ident));
}

void MapWidget::addRangeRing(const atools::geo::Pos& pos)
{
  map::RangeMarker rings;
  rings.type = map::NONE;
  rings.center = pos;

  const QVector<int> dists = OptionData::instance().getMapRangeRings();
  for(int dist : dists)
    rings.ranges.append(atools::roundToInt(Unit::rev(dist, Unit::distNmF)));

  screenIndex->getRangeMarks().append(rings);

  qDebug() << "range rings" << rings.center;
  update();
  mainWindow->setStatusMessage(tr("Added range rings for position."));
}

void MapWidget::clearRangeRingsAndDistanceMarkers()
{
  qDebug() << "range rings hide";

  screenIndex->getRangeMarks().clear();
  screenIndex->getDistanceMarks().clear();
  currentDistanceMarkerIndex = -1;

  update();
  mainWindow->setStatusMessage(tr("All range rings and measurement lines removed from map."));
}

void MapWidget::workOffline(bool offline)
{
  qDebug() << "Work offline" << offline;
  model()->setWorkOffline(offline);

  mainWindow->renderStatusChanged(Marble::RenderStatus::Complete);

  if(!offline)
    update();
}

void MapWidget::elevationDisplayTimerTimeout()
{
  qreal lon, lat;
  QPoint point = mapFromGlobal(QCursor::pos());
  if(geoCoordinates(point.x(), point.y(), lon, lat, GeoDataCoordinates::Degree))
  {
    Pos pos(lon, lat);
    pos.setAltitude(NavApp::getElevationProvider()->getElevation(pos));
    mainWindow->updateMapPosLabel(pos);
  }
}

bool MapWidget::eventFilter(QObject *obj, QEvent *e)
{
  if(e->type() == QEvent::MouseButtonDblClick)
  {
    // Catch the double click event

    // Do not propagate further
    e->accept();

    // Call own event handler
    event(e);

    // Do not process further
    return true;
  }

  if(e->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);

    if(mouseEvent != nullptr && mouseEvent->modifiers() & Qt::ControlModifier)
      // Remove control modifer to disable Marble rectangle dragging
      mouseEvent->setModifiers(mouseEvent->modifiers() & ~Qt::ControlModifier);
  }

  if(e->type() == QEvent::MouseMove)
  {
    // Update coordinate display in status bar
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
    qreal lon, lat;
    if(geoCoordinates(mouseEvent->pos().x(), mouseEvent->pos().y(), lon, lat, GeoDataCoordinates::Degree))
    {
      if(NavApp::getElevationProvider()->isGlobeOfflineProvider())
        elevationDisplayTimer.start();
      mainWindow->updateMapPosLabel(Pos(lon, lat, static_cast<double>(map::INVALID_ALTITUDE_VALUE)));
    }
    else
      mainWindow->updateMapPosLabel(Pos());
  }

  if(e->type() == QEvent::MouseMove && mouseState != mw::NONE)
  {
    // Do not allow mouse scrolling during drag actions
    e->accept();
    event(e);

    // Do not process further
    return true;
  }

  QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);
  if(e->type() == QEvent::MouseMove && mouseEvent->buttons() == Qt::NoButton && mouseState == mw::NONE)
  {
    // No not pass movements to marble widget to avoid cursor fighting
    e->accept();
    event(e);
    // Do not process further
    return true;
  }

  // Pass to base class and keep on processing
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

void MapWidget::cancelDragAll()
{
  cancelDragRoute();
  cancelDragDistance();

  mouseState = mw::NONE;
  setViewContext(Marble::Still);
  update();
}

/* Stop route editing and reset all coordinates */
void MapWidget::cancelDragRoute()
{
  if(mouseState & mw::DRAG_ROUTE_POINT || mouseState & mw::DRAG_ROUTE_LEG)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    routeDragCur = QPoint();
    routeDragFrom = atools::geo::EMPTY_POS;
    routeDragTo = atools::geo::EMPTY_POS;
    routeDragPoint = -1;
    routeDragLeg = -1;
  }
}

/* Stop new distance line or change dragging and restore backup or delete new line */
void MapWidget::cancelDragDistance()
{
  if(cursor().shape() != Qt::ArrowCursor)
    setCursor(Qt::ArrowCursor);

  if(mouseState & mw::DRAG_DISTANCE)
    // Remove new one
    screenIndex->getDistanceMarks().removeAt(currentDistanceMarkerIndex);
  else if(mouseState & mw::DRAG_CHANGE_DISTANCE)
    // Replace modified one with backup
    screenIndex->getDistanceMarks()[currentDistanceMarkerIndex] = distanceMarkerBackup;
  currentDistanceMarkerIndex = -1;
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

#ifdef DEBUG_MOVING_AIRPLANE
  static int packetId = 0;
  static Pos lastPos;
  static QPoint lastPoint;

  if(event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
  {
    if(QPoint(lastPoint - event->pos()).manhattanLength() > 4)
    {
      qreal lon, lat;
      geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      Pos pos(lon, lat, 5000);

      atools::fs::sc::SimConnectData data = atools::fs::sc::SimConnectData::buildDebugForPosition(pos, lastPos);
      data.setPacketId(packetId++);

      emit NavApp::getConnectClient()->dataPacketReceived(data);
      lastPos = pos;
      lastPoint = event->pos();
    }
  }
#endif
  if(mouseState & mw::DRAG_DISTANCE || mouseState & mw::DRAG_CHANGE_DISTANCE)
  {
    // Currently dragging a measurement line
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    qreal lon, lat;
    bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
    if(visible)
    {
      // Position is valid update the distance mark continuously
      if(!screenIndex->getDistanceMarks().isEmpty())
        screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = Pos(lon, lat);
    }

    // Force fast updates while dragging
    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState & mw::DRAG_ROUTE_LEG || mouseState & mw::DRAG_ROUTE_POINT)
  {
    // Currently dragging a flight plan leg
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    qreal lon, lat;
    bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
    if(visible)
      // Update current point
      routeDragCur = QPoint(event->pos().x(), event->pos().y());

    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState == mw::NONE)
  {
    if(event->buttons() == Qt::NoButton)
    {
      // No dragging going on now - update cursor over flight plan legs or markers
      const Route& route = NavApp::getRoute();

      Qt::CursorShape cursorShape = Qt::ArrowCursor;
      bool routeEditMode = mainWindow->getUi()->actionRouteEditMode->isChecked();

      // Make distance a bit larger to prefer points
      if(routeEditMode &&
         screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                screenSearchDistance * 4 / 3) != -1 &&
         route.size() > 1)
        // Change cursor at one route point
        cursorShape = Qt::SizeAllCursor;
      else if(routeEditMode &&
              screenIndex->getNearestRouteLegIndex(event->pos().x(), event->pos().y(), screenSearchDistance) != -1 &&
              route.size() > 1)
        // Change cursor above a route line
        cursorShape = Qt::CrossCursor;
      else if(screenIndex->getNearestDistanceMarkIndex(event->pos().x(), event->pos().y(),
                                                       screenSearchDistance) != -1)
        // Change cursor at the end of an marker
        cursorShape = Qt::CrossCursor;

      if(cursor().shape() != cursorShape)
        setCursor(cursorShape);
    }
  }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
  hideTooltip();

  // Remember mouse position to check later if mouse was moved during click (drag map scroll)
  mouseMoved = event->pos();
  if(mouseState == mw::DRAG_DISTANCE || mouseState == mw::DRAG_CHANGE_DISTANCE ||
     mouseState == mw::DRAG_ROUTE_POINT || mouseState == mw::DRAG_ROUTE_LEG)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    if(event->button() == Qt::LeftButton)
      // Done with any dragging
      mouseState |= mw::DRAG_POST;
    else if(event->button() == Qt::RightButton)
      // Cancel any dragging
      mouseState |= mw::DRAG_POST_CANCEL;
  }
  else if(mouseState == mw::NONE && event->buttons() & Qt::RightButton)
    // First right click after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);
  else
  {
    // No drag and drop mode - use hand to indicate scrolling
    if(event->button() == Qt::LeftButton && cursor().shape() != Qt::OpenHandCursor)
      setCursor(Qt::OpenHandCursor);
  }
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  hideTooltip();

  if(mouseState & mw::DRAG_ROUTE_POINT || mouseState & mw::DRAG_ROUTE_LEG)
  {
    // End route point dragging
    if(mouseState & mw::DRAG_POST)
    {
      // Ending route dragging - update route
      qreal lon, lat;
      bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      if(visible)
        updateRouteFromDrag(routeDragCur, mouseState, routeDragLeg, routeDragPoint);
    }

    // End all dragging
    cancelDragRoute();
    mouseState = mw::NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(mouseState & mw::DRAG_DISTANCE || mouseState & mw::DRAG_CHANGE_DISTANCE)
  {
    // End distance marker dragging
    if(!screenIndex->getDistanceMarks().isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(mouseState & mw::DRAG_POST)
      {
        qreal lon, lat;
        bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
        if(visible)
          // Update distance measurment line
          screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = Pos(lon, lat);
      }
      else if(mouseState & mw::DRAG_POST_CANCEL)
        cancelDragDistance();
    }
    else
    {
      if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }

    mouseState = mw::NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(event->button() == Qt::LeftButton && (event->pos() - mouseMoved).manhattanLength() < 4)
  {
    // Start all dragging if left button was clicked and mouse was not moved
    currentDistanceMarkerIndex = screenIndex->getNearestDistanceMarkIndex(event->pos().x(),
                                                                          event->pos().y(),
                                                                          screenSearchDistance);
    if(currentDistanceMarkerIndex != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = mw::DRAG_CHANGE_DISTANCE;
      distanceMarkerBackup = screenIndex->getDistanceMarks().at(currentDistanceMarkerIndex);
      setContextMenuPolicy(Qt::PreventContextMenu);
    }
    else
    {
      if(mainWindow->getUi()->actionRouteEditMode->isChecked())
      {
        const Route& route = NavApp::getRoute();

        if(route.size() > 1)
        {
          // Make distance a bit larger to prefer points
          int routePoint =
            screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                   screenSearchDistance * 4 / 3);
          if(routePoint != -1)
          {
            routeDragPoint = routePoint;
            qDebug() << "route point" << routePoint;

            // Found a leg - start dragging
            mouseState = mw::DRAG_ROUTE_POINT;

            routeDragCur = QPoint(event->pos().x(), event->pos().y());

            if(routePoint > 0)
              routeDragFrom = route.at(routePoint - 1).getPosition();
            else
              routeDragFrom = atools::geo::EMPTY_POS;

            if(routePoint < route.size() - 1)
              routeDragTo = route.at(routePoint + 1).getPosition();
            else
              routeDragTo = atools::geo::EMPTY_POS;
            setContextMenuPolicy(Qt::PreventContextMenu);
          }
          else
          {
            int routeLeg = screenIndex->getNearestRouteLegIndex(event->pos().x(),
                                                                event->pos().y(), screenSearchDistance);
            if(routeLeg != -1)
            {
              routeDragLeg = routeLeg;
              qDebug() << "route leg" << routeLeg;
              // Found a leg - start dragging
              mouseState = mw::DRAG_ROUTE_LEG;

              routeDragCur = QPoint(event->pos().x(), event->pos().y());

              routeDragFrom = route.at(routeLeg).getPosition();
              routeDragTo = route.at(routeLeg + 1).getPosition();
              setContextMenuPolicy(Qt::PreventContextMenu);
            }
          }
        }
      }

      if(mouseState == mw::NONE)
      {
        if(cursor().shape() != Qt::ArrowCursor)
          setCursor(Qt::ArrowCursor);
        handleInfoClick(event->pos());
      }
    }
  }
  else
  {
    // No drag and drop mode - switch back to arrow after scrolling
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);
  }

  mouseMoved = QPoint();
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << "mouseDoubleClickEvent";

  if(mouseState != mw::NONE)
    return;

  map::MapSearchResult mapSearchResult;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(event->pos().x(), event->pos().y(), screenSearchDistance, mapSearchResult, procPoints);

  if(mapSearchResult.userAircraft.getPosition().isValid())
  {
    showPos(mapSearchResult.userAircraft.getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing user aircraft on map.")));
  }
  else if(!mapSearchResult.aiAircraft.isEmpty())
  {
    showPos(mapSearchResult.aiAircraft.first().getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing AI / multiplayer aircraft on map.")));
  }
  else if(!mapSearchResult.airports.isEmpty())
  {
    showRect(mapSearchResult.airports.first().bounding, true);
    mainWindow->setStatusMessage(QString(tr("Showing airport on map.")));
  }
  else
  {
    if(!mapSearchResult.vors.isEmpty())
      showPos(mapSearchResult.vors.first().position, 0.f, true);
    else if(!mapSearchResult.ndbs.isEmpty())
      showPos(mapSearchResult.ndbs.first().position, 0.f, true);
    else if(!mapSearchResult.waypoints.isEmpty())
      showPos(mapSearchResult.waypoints.first().position, 0.f, true);
    else if(!mapSearchResult.userPoints.isEmpty())
      showPos(mapSearchResult.userPoints.first().position, 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing navaid on map.")));
  }
}

/* Stop all line drag and drop if the map looses focus */
void MapWidget::focusOutEvent(QFocusEvent *event)
{
  Q_UNUSED(event);

  if(!(mouseState & mw::DRAG_POST_MENU))
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }
}

void MapWidget::leaveEvent(QEvent *)
{
  mainWindow->updateMapPosLabel(Pos());
}

void MapWidget::keyPressEvent(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Escape)
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }

  if(event->key() == Qt::Key_Menu && mouseState == mw::NONE)
    // First menu key press after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

const QList<int>& MapWidget::getRouteHighlights() const
{
  return screenIndex->getRouteHighlights();
}

const QList<map::RangeMarker>& MapWidget::getRangeRings() const
{
  return screenIndex->getRangeMarks();
}

const QList<map::DistanceMarker>& MapWidget::getDistanceMarkers() const
{
  return screenIndex->getDistanceMarks();
}

void MapWidget::hideTooltip()
{
  QToolTip::hideText();
  tooltipPos = QPoint();
}

void MapWidget::updateTooltip()
{
  showTooltip(true);
}

void MapWidget::showTooltip(bool update)
{
  // qDebug() << Q_FUNC_INFO << "update" << update << "QToolTip::isVisible()" << QToolTip::isVisible();

  if(databaseLoadStatus)
    return;

  // Try to avoid spurious tooltip events
  if(update && !QToolTip::isVisible())
    return;

  // Build a new tooltip HTML for weather changes or aircraft updates
  QString text = mapTooltip->buildTooltip(mapSearchResultTooltip, procPointsTooltip, NavApp::getRoute(),
                                          paintLayer->getMapLayer()->isAirportDiagram());

  if(!text.isEmpty() && !tooltipPos.isNull())
    QToolTip::showText(tooltipPos, text /*, nullptr, QRect(), 3600 * 1000*/);
  else
    hideTooltip();
}

const atools::fs::sc::SimConnectUserAircraft& MapWidget::getUserAircraft() const
{
  return screenIndex->getUserAircraft();
}

const QVector<atools::fs::sc::SimConnectAircraft>& MapWidget::getAiAircraft() const
{
  return screenIndex->getAiAircraft();
}

void MapWidget::deleteAircraftTrack()
{
  aircraftTrack.clearTrack();
  emit updateActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Aircraft track removed from map.")));
}

bool MapWidget::event(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

    // Load tooltip data into mapSearchResultTooltip
    mapSearchResultTooltip = map::MapSearchResult();
    procPointsTooltip.clear();
    screenIndex->getAllNearest(helpEvent->pos().x(), helpEvent->pos().y(), screenSearchDistanceTooltip,
                               mapSearchResultTooltip, procPointsTooltip);
    tooltipPos = helpEvent->globalPos();

    // Build HTML
    showTooltip(false);
    event->accept();
    return true;
  }

  return QWidget::event(event);
}

/* Update the visible objects indication in the status bar. */
void MapWidget::updateVisibleObjectsStatusBar()
{
  if(!NavApp::hasDataInDatabase())
  {
    mainWindow->setMapObjectsShownMessageText(tr("<b style=\"color:red\">Database empty.</b>"),
                                              tr("The currently selected Scenery Database is empty."));
  }
  else
  {

    const MapLayer *layer = paintLayer->getMapLayer();

    if(layer != nullptr)
    {
      map::MapObjectTypes shown = paintLayer->getShownMapObjects();

      QStringList airportLabel;
      atools::util::HtmlBuilder tooltip(false);
      tooltip.b(tr("Currently shown on map:"));
      tooltip.table();

      // Collect airport information ==========================================================
      if(layer->isAirport() && ((shown& map::AIRPORT_HARD) || (shown& map::AIRPORT_SOFT) ||
                                (shown & map::AIRPORT_ADDON)))
      {
        QString runway, runwayShort;
        QString apShort, apTooltip, apTooltipAddon;
        bool showAddon = shown & map::AIRPORT_ADDON;
        bool showEmpty = shown & map::AIRPORT_EMPTY;
        bool showStock = (shown& map::AIRPORT_HARD) || (shown & map::AIRPORT_SOFT);
        bool showHard = shown & map::AIRPORT_HARD;
        bool showAny = showStock | showAddon;

        // Prepare runway texts
        if(!(shown& map::AIRPORT_HARD) && (shown & map::AIRPORT_SOFT))
        {
          runway = tr(" soft runways (S)");
          runwayShort = tr(",S");
        }
        else if((shown& map::AIRPORT_HARD) && !(shown & map::AIRPORT_SOFT))
        {
          runway = tr(" hard runways (H)");
          runwayShort = tr(",H");
        }
        else if((shown& map::AIRPORT_HARD) && (shown & map::AIRPORT_SOFT))
        {
          runway = tr(" all runway types (H,S)");
          runwayShort = tr(",H,S");
        }
        else if(!(shown& map::AIRPORT_HARD) && !(shown & map::AIRPORT_SOFT))
        {
          runway.clear();
          runwayShort.clear();
        }

        // Prepare short airport indicator
        if(showAddon)
          apShort = showEmpty ? tr("AP,A,E") : tr("AP,A");
        else if(showStock)
          apShort = showEmpty ? tr("AP,E") : tr("AP");

        // Build the rest of the strings
        if(layer->getDataSource() == layer::ALL)
        {
          if(layer->getMinRunwayLength() > 0)
          {
            if(showAny)
              apShort.append(">").append(QLocale().toString(layer->getMinRunwayLength() / 100)).append(runwayShort);

            if(showStock)
              apTooltip = tr("Airports (AP) with runway length > %1 and%2").
                          arg(Unit::distShortFeet(layer->getMinRunwayLength())).
                          arg(runway);

            if(showAddon)
              apTooltipAddon = tr("Add-on airports with runway length > %1").
                               arg(Unit::distShortFeet(layer->getMinRunwayLength()));
          }
          else
          {
            if(showStock)
            {
              apShort.append(runwayShort);
              apTooltip = tr("Airports (AP) with%1").arg(runway);
            }
            if(showAddon)
              apTooltipAddon = tr("Add-on airports (A)");
          }
        }
        else if(layer->getDataSource() == layer::MEDIUM)
        {
          if(showAny)
            apShort.append(tr(">%1%2").arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT / 100, false)).
                           arg(runwayShort));

          if(showStock)
            apTooltip = tr("Airports (AP) with runway length > %1 and%2").
                        arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT)).
                        arg(runway);

          if(showAddon)
            apTooltipAddon.append(tr("Add-on airports (A) with runway length > %1").
                                  arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT)));
        }
        else if(layer->getDataSource() == layer::LARGE)
        {
          if(showAddon || showHard)
            apShort.append(tr(">%1,H").arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT / 100, false)));
          else
            apShort.clear();

          if(showStock && showHard)
            apTooltip = tr("Airports (AP) with runway length > %1 and hard runways (H)").
                        arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT));

          if(showAddon)
            apTooltipAddon.append(tr("Add-on airports (A) with runway length > %1").
                                  arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT)));
        }

        airportLabel.append(apShort);

        if(!apTooltip.isEmpty())
          tooltip.tr().td(apTooltip).trEnd();

        if(!apTooltipAddon.isEmpty())
          tooltip.tr().td(apTooltipAddon).trEnd();

        if(showEmpty)
          tooltip.tr().td(tr("Empty airports (E) with zero rating")).trEnd();
      }

      QStringList navaidLabel, navaidsTooltip;
      // Collect navaid information ==========================================================
      if(layer->isVor() && shown & map::VOR)
      {
        navaidLabel.append(tr("V"));
        navaidsTooltip.append(tr("VOR (V)"));
      }
      if(layer->isNdb() && shown & map::NDB)
      {
        navaidLabel.append(tr("N"));
        navaidsTooltip.append(tr("NDB (N)"));
      }
      if(layer->isIls() && shown & map::ILS)
      {
        navaidLabel.append(tr("I"));
        navaidsTooltip.append(tr("ILS (I)"));
      }
      if(layer->isWaypoint() && shown & map::WAYPOINT)
      {
        navaidLabel.append(tr("W"));
        navaidsTooltip.append(tr("Waypoints (W)"));
      }
      if(layer->isAirway() && shown & map::AIRWAYJ)
      {
        navaidLabel.append(tr("JA"));
        navaidsTooltip.append(tr("Jet Airways (JA)"));
      }
      if(layer->isAirway() && shown & map::AIRWAYV)
      {
        navaidLabel.append(tr("VA"));
        navaidsTooltip.append(tr("Victor Airways (VA)"));
      }

      QStringList airspacesTooltip, airspaceGroupLabel, airspaceGroupTooltip;
      if(shown & map::AIRSPACE)
      {
        map::MapAirspaceTypes airspaceTypes = paintLayer->getShownAirspacesTypesByLayer();
        // Collect airspace information ==========================================================
        for(int i = 0; i < map::MAP_AIRSPACE_TYPE_BITS; i++)
        {
          map::MapAirspaceTypes type(1 << i);
          if(airspaceTypes & type)
            airspacesTooltip.append(map::airspaceTypeToString(type));
        }
        if(airspaceTypes & map::AIRSPACE_ICAO)
        {
          airspaceGroupLabel.append(tr("ICAO"));
          airspaceGroupTooltip.append(tr("Class A-E (ICAO)"));
        }
        if(airspaceTypes & map::AIRSPACE_FIR)
        {
          airspaceGroupLabel.append(tr("FIR"));
          airspaceGroupTooltip.append(tr("Flight Information Region, class F and/or G (FIR)"));
        }
        if(airspaceTypes & map::AIRSPACE_RESTRICTED)
        {
          airspaceGroupLabel.append(tr("RSTR"));
          airspaceGroupTooltip.append(tr("Restricted (RSTR)"));
        }
        if(airspaceTypes & map::AIRSPACE_SPECIAL)
        {
          airspaceGroupLabel.append(tr("SPEC"));
          airspaceGroupTooltip.append(tr("Special (SPEC)"));
        }
        if(airspaceTypes & map::AIRSPACE_OTHER || airspaceTypes & map::AIRSPACE_CENTER)
        {
          airspaceGroupLabel.append(tr("OTR"));
          airspaceGroupTooltip.append(tr("Centers and others (OTR)"));
        }
      }

      if(!navaidsTooltip.isEmpty())
        tooltip.tr().td().b(tr("Navaids: ")).text(navaidsTooltip.join(", ")).tdEnd().trEnd();
      else
        tooltip.tr().td(tr("No navaids")).trEnd();

      if(!airspacesTooltip.isEmpty())
      {
        tooltip.tr().td().b(tr("Airspace Groups: ")).text(airspaceGroupTooltip.join(", ")).tdEnd().trEnd();
        tooltip.tr().td().b(tr("Airspaces: ")).text(airspacesTooltip.join(", ")).tdEnd().trEnd();
      }
      else
        tooltip.tr().td(tr("No airspaces")).trEnd();

      QStringList aiLabel;
      if(NavApp::isConnected())
      {
        QStringList ai;

        // AI vehicles
        if(shown & map::AIRCRAFT_AI && layer->isAiAircraftLarge())
        {
          QString ac;
          if(!layer->isAiAircraftSmall())
          {
            ac = tr("Aircraft > %1 ft").arg(layer::LARGE_AIRCRAFT_SIZE);
            aiLabel.append("A>");
          }
          else
          {
            ac = tr("Aircraft");
            aiLabel.append("A");
          }

          if(layer->isAiAircraftGround())
          {
            ac.append(tr(" on ground"));
            aiLabel.last().append("G");
          }
          ai.append(ac);
        }

        if(shown & map::AIRCRAFT_AI_SHIP && layer->isAiShipLarge())
        {
          if(!layer->isAiShipSmall())
          {
            ai.append(tr("Ships > %1 ft").arg(layer::LARGE_SHIP_SIZE));
            aiLabel.append("S>");
          }
          else
          {
            ai.append(tr("Ships"));
            aiLabel.append("S");
          }
        }
        if(!ai.isEmpty())
          tooltip.tr().td().b(tr("AI/multiplayer: ")).text(ai.join(", ")).tdEnd().trEnd();
        else
          tooltip.tr().td(tr("No AI/Multiplayer")).trEnd();
      }
      tooltip.tableEnd();

      QStringList label;
      if(!airportLabel.isEmpty())
        label.append(airportLabel.join(tr(",")));
      if(!navaidLabel.isEmpty())
        label.append(navaidLabel.join(tr(",")));
      if(!airspaceGroupLabel.isEmpty())
        label.append(airspaceGroupLabel.join(tr(",")));
      if(!aiLabel.isEmpty())
        label.append(aiLabel.join(tr(",")));

      // Update the statusbar label text and tooltip of the label
      mainWindow->setMapObjectsShownMessageText(label.join(" / "), tooltip.getHtml());
    }
  }
}

void MapWidget::paintEvent(QPaintEvent *paintEvent)
{
  if(!active)
  {
    QPainter painter(this);
    painter.fillRect(paintEvent->rect(), QGuiApplication::palette().color(QPalette::Window));
    return;
  }

  bool changed = false;
  const GeoDataLatLonAltBox visibleLatLonAltBox = viewport()->viewLatLonAltBox();

  if(viewContext() == Marble::Still &&
     (zoom() != currentZoom || visibleLatLonAltBox != currentViewBoundingBox))
  {
    // This paint event has changed zoom or the visible bounding box
    currentZoom = zoom();
    currentViewBoundingBox = visibleLatLonAltBox;

    // qDebug() << "paintEvent map view has changed zoom" << currentZoom
    // << "distance" << distance() << " (" << meterToNm(distance() * 1000.) << " km)";

    if(!changedByHistory)
      // Not changed by next/last in history
      history.addEntry(Pos(centerLongitude(), centerLatitude()), distance());

    changedByHistory = false;
    changed = true;
  }

  MarbleWidget::paintEvent(paintEvent);

  if(changed)
  {
    // Major change - update index and visible objects
    updateVisibleObjectsStatusBar();
    screenIndex->updateRouteScreenGeometry(currentViewBoundingBox);
    screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
    screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  }

  if(paintLayer->getOverflow() > 0)
    emit resultTruncated(paintLayer->getOverflow());
}

void MapWidget::handleInfoClick(QPoint pos)
{
  qDebug() << Q_FUNC_INFO;

  map::MapSearchResult result;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(pos.x(), pos.y(), screenSearchDistance, result, procPoints);

  emit showInformation(result);
}

bool MapWidget::loadKml(const QString& filename, bool center)
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

void MapWidget::defaultMapDetail()
{
  mapDetailLevel = MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailLevel);
}

void MapWidget::increaseMapDetail()
{
  if(mapDetailLevel < MapLayerSettings::MAP_MAX_DETAIL_FACTOR)
  {
    mapDetailLevel++;
    setMapDetail(mapDetailLevel);
  }
}

void MapWidget::decreaseMapDetail()
{
  if(mapDetailLevel > MapLayerSettings::MAP_MIN_DETAIL_FACTOR)
  {
    mapDetailLevel--;
    setMapDetail(mapDetailLevel);
  }
}

void MapWidget::setMapDetail(int factor)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  mapDetailLevel = factor;
  setDetailLevel(mapDetailLevel);
  ui->actionMapMoreDetails->setEnabled(mapDetailLevel < MapLayerSettings::MAP_MAX_DETAIL_FACTOR);
  ui->actionMapLessDetails->setEnabled(mapDetailLevel > MapLayerSettings::MAP_MIN_DETAIL_FACTOR);
  ui->actionMapDefaultDetails->setEnabled(mapDetailLevel != MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR);
  update();

  int det = mapDetailLevel - MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  QString detStr;
  if(det == 0)
    detStr = tr("Normal");
  else if(det > 0)
    detStr = "+" + QString::number(det);
  else if(det < 0)
    detStr = QString::number(det);

  // Update status bar label
  mainWindow->setDetailLabelText(tr("Detail %1").arg(detStr));
  mainWindow->setStatusMessage(tr("Map detail level changed."));
}
