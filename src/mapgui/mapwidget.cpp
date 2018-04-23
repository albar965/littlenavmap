/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "userdata/userdataicons.h"
#include "route/routecontroller.h"
#include "atools.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "mapgui/maptooltip.h"
#include "common/symbolpainter.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/mapvisible.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"
#include "util/htmlbuilder.h"
#include "mapgui/maplayersettings.h"
#include "common/unit.h"
#include "gui/widgetstate.h"
#include "gui/application.h"
#include "sql/sqlrecord.h"

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

// Delay recognition to avoid detection of bumps
const int TAKEOFF_LANDING_TIMEOUT = 5000;

/* If width and height of a bounding rect are smaller than this use show point */
const float POS_IS_POINT_EPSILON = 0.0001f;

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

/* width/height / factor = inner rectangle which has to overlap */
const float MAX_SQUARE_FACTOR_FOR_CENTER_LEG_SPHERICAL = 4.f;
const float MAX_SQUARE_FACTOR_FOR_CENTER_LEG_MERCATOR = 4.f;

using namespace Marble;
using atools::gui::MapPosHistoryEntry;
using atools::gui::MapPosHistory;
using atools::geo::Rect;
using atools::geo::Pos;
using atools::sql::SqlRecord;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;
using atools::almostNotEqual;

MapWidget::MapWidget(MainWindow *parent)
  : Marble::MarbleWidget(parent), mainWindow(parent)
{
  mapQuery = NavApp::getMapQuery();
  airportQuery = NavApp::getAirportQuerySim();

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

  screenIndex = new MapScreenIndex(this, paintLayer);

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

  jumpBackToAircraftTimer.setSingleShot(true);
  connect(&jumpBackToAircraftTimer, &QTimer::timeout, this, &MapWidget::jumpBackToAircraftTimeout);

  takeoffLandingTimer.setSingleShot(true);
  connect(&takeoffLandingTimer, &QTimer::timeout, this, &MapWidget::takeoffLandingTimeout);

  mapVisible = new MapVisible(paintLayer);
}

MapWidget::~MapWidget()
{
  elevationDisplayTimer.stop();
  jumpBackToAircraftTimer.stop();
  takeoffLandingTimer.stop();

  qDebug() << Q_FUNC_INFO << "removeEventFilter";
  removeEventFilter(this);

  qDebug() << Q_FUNC_INFO << "delete mapVisible";
  delete mapVisible;

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

  // Ignore any overlay state signals the widget sends while switching theme
  ignoreOverlayUpdates = true;

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

  ignoreOverlayUpdates = false;

  // Show or hide overlays again
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

  setShowMapFeatures(map::AIRSPACE, getShownAirspaces().flags & map::AIRSPACE_ALL &&
                     ui->actionShowAirspaces->isChecked());
  setShowMapFeatures(map::AIRSPACE_ONLINE, getShownAirspaces().flags & map::AIRSPACE_ALL &&
                     ui->actionShowAirspacesOnline->isChecked());

  setShowMapFeatures(map::FLIGHTPLAN, ui->actionMapShowRoute->isChecked());
  setShowMapFeatures(map::COMPASS_ROSE, ui->actionMapShowCompassRose->isChecked());
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

  mapVisible->updateVisibleObjectsStatusBar();

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

  if(type & map::AIRSPACE || type & map::AIRSPACE_ONLINE)
    screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

void MapWidget::setShowMapAirspaces(map::MapAirspaceFilter types)
{
  paintLayer->setShowAirspaces(types);
  mapVisible->updateVisibleObjectsStatusBar();
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

void MapWidget::setDetailLevel(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
  mapVisible->updateVisibleObjectsStatusBar();
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
}

map::MapObjectTypes MapWidget::getShownMapFeatures() const
{
  return paintLayer->getShownMapObjects();
}

map::MapAirspaceFilter MapWidget::getShownAirspaces() const
{
  return paintLayer->getShownAirspaces();
}

map::MapAirspaceFilter MapWidget::getShownAirspaceTypesByLayer() const
{
  return paintLayer->getShownAirspacesTypesByLayer();
}

void MapWidget::getUserpointDragPoints(QPoint& cur, QPixmap& pixmap)
{
  cur = userpointDragCur;
  pixmap = userpointDragPixmap;
}

void MapWidget::getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur)
{
  from = routeDragFrom;
  to = routeDragTo;
  cur = routeDragCur;
}

void MapWidget::preDatabaseLoad()
{
  jumpBackToAircraftCancel();
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
  mapVisible->updateVisibleObjectsStatusBar();
}

void MapWidget::historyNext()
{
  const MapPosHistoryEntry& entry = history.next();
  if(entry.isValid())
  {
    jumpBackToAircraftStart();

    setDistance(entry.getDistance());
    centerOn(entry.getPos().getLonX(), entry.getPos().getLatY(), false);
    noStoreInHistory = true;
    mainWindow->setStatusMessage(tr("Map position history next."));
    showAircraft(false);
  }
}

void MapWidget::historyBack()
{
  const MapPosHistoryEntry& entry = history.back();
  if(entry.isValid())
  {
    jumpBackToAircraftStart();

    setDistance(entry.getDistance());
    centerOn(entry.getPos().getLonX(), entry.getPos().getLatY(), false);
    noStoreInHistory = true;
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
  s.setValueVar(lnm::MAP_AIRSPACES, QVariant::fromValue(paintLayer->getShownAirspaces()));

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
  ui->actionShowAirspacesOnline->blockSignals(true);
  ui->actionShowAirspacesOnline->setChecked(true);
  ui->actionShowAirspacesOnline->blockSignals(false);
  ui->actionMapShowRoute->blockSignals(true);
  ui->actionMapShowRoute->setChecked(true);
  ui->actionMapShowRoute->blockSignals(false);
  ui->actionMapShowAircraft->blockSignals(true);
  ui->actionMapShowAircraft->setChecked(true);
  ui->actionMapShowAircraft->blockSignals(false);
  ui->actionMapShowCompassRose->blockSignals(true);
  ui->actionMapShowCompassRose->setChecked(false);
  ui->actionMapShowCompassRose->blockSignals(false);
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
  paintLayer->setShowAirspaces({map::AIRSPACE_DEFAULT, map::AIRSPACE_FLAG_DEFAULT});
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

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_TRAIL)
    aircraftTrack.restoreState();
  aircraftTrack.setMaxTrackEntries(OptionData::instance().getAircraftTrackMaxPoints());

  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : mapOverlays.values())
    state.restore(action);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    map::MapAirspaceFilter defaultValue = {map::AIRSPACE_DEFAULT, map::AIRSPACE_FLAG_DEFAULT};
    paintLayer->setShowAirspaces(s.valueVar(lnm::MAP_AIRSPACES,
                                            QVariant::fromValue(defaultValue)).value<map::MapAirspaceFilter>());
  }
  else
    paintLayer->setShowAirspaces({map::AIRSPACE_DEFAULT, map::AIRSPACE_FLAG_DEFAULT});

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
        qDebug() << "showing float item" << overlay->name() << "id" << overlay->nameId();
        overlay->setVisible(true);
        overlay->show();
      }
      else
      {
        qDebug() << "hiding float item" << overlay->name() << "id" << overlay->nameId();
        overlay->setVisible(false);
        overlay->hide();
      }
      overlay->blockSignals(false);
    }
  }
}

void MapWidget::overlayStateToMenu()
{
  qDebug() << Q_FUNC_INFO << "ignoreOverlayUpdates" << ignoreOverlayUpdates;
  if(!ignoreOverlayUpdates)
  {
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
}

void MapWidget::overlayStateFromMenu()
{
  qDebug() << Q_FUNC_INFO << "ignoreOverlayUpdates" << ignoreOverlayUpdates;
  if(!ignoreOverlayUpdates)
  {
    for(const QString& name : mapOverlays.keys())
    {
      AbstractFloatItem *overlay = floatItem(name);
      if(overlay != nullptr)
      {
        bool show = mapOverlays.value(name)->isChecked();
        overlay->blockSignals(true);
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
        overlay->blockSignals(false);
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
    qDebug() << "Show Route" << NavApp::getRouteConst().getBoundingRect();
    if(!NavApp::getRouteConst().isFlightplanEmpty())
      showRect(NavApp::getRouteConst().getBoundingRect(), false);
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

void MapWidget::showPos(const atools::geo::Pos& pos, float distanceNm, bool doubleClick)
{
#if DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << pos << distanceNm << doubleClick;
#endif
  if(!pos.isValid())
  {
    qWarning() << Q_FUNC_INFO << "Invalid position";
    return;
  }

  hideTooltip();
  showAircraft(false);
  jumpBackToAircraftStart();

  float dst = distanceNm;

  if(dst == 0.f)
    dst = atools::geo::nmToKm(Unit::rev(doubleClick ?
                                        OptionData::instance().getMapZoomShowClick() :
                                        OptionData::instance().getMapZoomShowMenu(), Unit::distNmF));

  if(dst < map::INVALID_DISTANCE_VALUE)
    setDistance(dst);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void MapWidget::showRect(const atools::geo::Rect& rect, bool doubleClick)
{
#if DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << rect << doubleClick;
#endif

  hideTooltip();
  showAircraft(false);
  jumpBackToAircraftStart();

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
    jumpBackToAircraftStart();
    setDistance(atools::geo::nmToKm(Unit::rev(OptionData::instance().getMapZoomShowMenu(), Unit::distNmF)));
    centerOn(searchMarkPos.getLonX(), searchMarkPos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showAircraft(bool centerAircraftChecked)
{
  qDebug() << Q_FUNC_INFO;

  if(!(OptionData::instance().getFlags2() & opts::ROUTE_NO_FOLLOW_ON_MOVE) || centerAircraftChecked)
  {
    // Keep old behavior if jump back to aircraft is disabled

    // Adapt the menu item status if this method was not called by the action
    QAction *acAction = mainWindow->getUi()->actionMapAircraftCenter;
    if(acAction->isEnabled())
    {
      acAction->blockSignals(true);
      acAction->setChecked(centerAircraftChecked);
      acAction->blockSignals(false);
      qDebug() << "Aircraft center set to" << centerAircraftChecked;
    }

    if(centerAircraftChecked && screenIndex->getUserAircraft().isValid())
      centerOn(screenIndex->getUserAircraft().getPosition().getLonX(),
               screenIndex->getUserAircraft().getPosition().getLatY(), false);
  }
}

void MapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << homePos;

  hideTooltip();
  jumpBackToAircraftStart();
  showAircraft(false);
  if(!atools::almostEqual(homeDistance, 0.))
    // Only center position is valid
    setDistance(homeDistance);

  if(homePos.isValid())
  {
    jumpBackToAircraftStart();
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

bool MapWidget::isCenterLegAndAircraftActive()
{
  const Route& route = NavApp::getRouteConst();
  return OptionData::instance().getFlags2() & opts::ROUTE_AUTOZOOM &&
         !route.isEmpty() && route.isActiveValid() && screenIndex->getUserAircraft().isFlying();
}

void MapWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  const atools::fs::sc::SimConnectUserAircraft& aircraft = simulatorData.getUserAircraft();
  if(databaseLoadStatus || !aircraft.isValid())
    return;

  screenIndex->updateSimData(simulatorData);
  const atools::fs::sc::SimConnectUserAircraft& last = screenIndex->getLastUserAircraft();

  // Calculate travel distance since last takeoff event ===================================
  if(!takeoffLandingLastAircraft.isValid())
    // Set for the first time
    takeoffLandingLastAircraft = aircraft;
  else if(aircraft.isValid() && !aircraft.isSimReplay() && !takeoffLandingLastAircraft.isSimReplay())
  {
    // Use less accuracy for longer routes
    float epsilon = takeoffLandingDistanceNm > 20. ? Pos::POS_EPSILON_500M : Pos::POS_EPSILON_10M;

    // Check manhattan distance in degree to minimize samples
    if(takeoffLandingLastAircraft.getPosition().distanceSimpleTo(aircraft.getPosition()) > epsilon)
    {
      if(takeoffTimeMs > 0)
      {
        // Calculate averaget TAS
        qint64 currentSampleTime = aircraft.getZuluTime().toMSecsSinceEpoch();

        // Only every ten seconds since the simulator timestamps are not precise enough
        if(currentSampleTime > takeoffLastSampleTimeMs + 10000)
        {
          qint64 lastPeriod = currentSampleTime - takeoffLastSampleTimeMs;
          qint64 flightimeToCurrentPeriod = currentSampleTime - takeoffTimeMs;

          if(flightimeToCurrentPeriod > 0)
            takeoffLandingAverageTasKts = ((takeoffLandingAverageTasKts * (takeoffLastSampleTimeMs - takeoffTimeMs)) +
                                           (aircraft.getTrueAirspeedKts() * lastPeriod)) / flightimeToCurrentPeriod;
          takeoffLastSampleTimeMs = currentSampleTime;
        }
      }

      takeoffLandingDistanceNm +=
        atools::geo::meterToNm(takeoffLandingLastAircraft.getPosition().distanceMeterTo(aircraft.getPosition()));

      takeoffLandingLastAircraft = aircraft;
    }
  }

  // Check for takeoff or landing events ===================================
  if(last.isValid() && aircraft.isValid() &&
     !aircraft.isSimPaused() && !aircraft.isSimReplay() &&
     !last.isSimPaused() && !last.isSimReplay())
  {
    // start timer to emit takeoff/landing signal
    if(last.isFlying() != aircraft.isFlying())
      takeoffLandingTimer.start(TAKEOFF_LANDING_TIMEOUT);
  }

  // Create screen coordinates =============================
  CoordinateConverter conv(viewport());
  bool curPosVisible = false;
  QPoint curPoint = conv.wToS(aircraft.getPosition(), CoordinateConverter::DEFAULT_WTOS_SIZE, &curPosVisible);
  QPoint diff = curPoint - conv.wToS(last.getPosition());
  const OptionData& od = OptionData::instance();
  QRect widgetRect = rect();

  // Used to check if objects are still visible
  QRect widgetRectSmall = widgetRect.adjusted(widgetRect.width() / 20, widgetRect.height() / 20,
                                              -widgetRect.width() / 20, -widgetRect.height() / 20);
  curPosVisible = widgetRectSmall.contains(curPoint);

  bool wasEmpty = aircraftTrack.isEmpty();
#ifdef DEBUG_INFORMATION_DISABLED
  qDebug() << "curPos" << curPos;
  qDebug() << "widgetRectSmall" << widgetRectSmall;
#endif

  if(aircraftTrack.appendTrackPos(aircraft.getPosition(), aircraft.getZuluTime(), aircraft.isOnGround()))
    emit aircraftTrackPruned();

  if(wasEmpty != aircraftTrack.isEmpty())
    // We have a track - update toolbar and menu
    emit updateActionStates();

  // ================================================================================
  // Check if screen has to be updated/scrolled/zoomed
  if(paintLayer->getShownMapObjects() & map::AIRCRAFT || paintLayer->getShownMapObjects() & map::AIRCRAFT_AI)
  {
    // Show aircraft is enabled
    bool centerAircraft = mainWindow->getUi()->actionMapAircraftCenter->isChecked();

    // Get delta values for update rate
    const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(od.getSimUpdateRate());

    // Limit number of updates per second =================================================
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

      // Check if position has changed significantly
      bool posHasChanged = !last.isValid() || // No previous position
                           diff.manhattanLength() >= deltas.manhattanLengthDelta; // Screen position has changed

      // Check if any data like heading has changed which requires a redraw
      bool dataHasChanged = posHasChanged ||
                            almostNotEqual(last.getHeadingDegMag(), aircraft.getHeadingDegMag(), deltas.headingDelta) || // Heading has changed
                            almostNotEqual(last.getIndicatedSpeedKts(),
                                           aircraft.getIndicatedSpeedKts(), deltas.speedDelta) || // Speed has changed
                            almostNotEqual(last.getPosition().getAltitude(),
                                           aircraft.getPosition().getAltitude(), deltas.altitudeDelta); // Altitude has changed

      if(dataHasChanged)
        screenIndex->updateLastSimData(simulatorData);

      // Option to udpate always
      bool updateAlways = od.getFlags() & opts::SIM_UPDATE_MAP_CONSTANTLY;

      // Check if centering of leg is reqired =======================================
      const Route& route = NavApp::getRouteConst();
      bool centerAircraftAndLeg = isCenterLegAndAircraftActive();

      // Get position of next waypoint and check visibility
      Pos nextWpPos;
      QPoint nextWpPoint;
      bool nextWpPosVisible = false;
      if(centerAircraftAndLeg)
      {
        nextWpPos = route.getActiveLeg() != nullptr ? route.getActiveLeg()->getPosition() : Pos();
        nextWpPoint = conv.wToS(nextWpPos, CoordinateConverter::DEFAULT_WTOS_SIZE, &nextWpPosVisible);
        nextWpPosVisible = widgetRectSmall.contains(nextWpPoint);
      }

      bool mapUpdated = false;
      if(centerAircraft && !contextMenuActive) // centering required by button but not while menu is open
      {
        if(!curPosVisible || // Not visible on world map
           posHasChanged || // Significant change in position might require zooming or re-centering
           aiVisible) // Paint always for visible AI
        {
          // Do not update if user is using drag and drop or scrolling around
          if(mouseState == mw::NONE && viewContext() == Marble::Still && !jumpBackToAircraftActive)
          {
            if(centerAircraftAndLeg)
            {
              QRect aircraftWpRect = QRect(curPoint, nextWpPoint).normalized();
              float factor = projection() ==
                             Mercator ? MAX_SQUARE_FACTOR_FOR_CENTER_LEG_MERCATOR :
                             MAX_SQUARE_FACTOR_FOR_CENTER_LEG_SPHERICAL;

              // Check if the rectangle from aircraft and waypoint overlaps with a shrinked screen rectangle
              // to check if it is still centered
              QRect innerRect = widgetRect.adjusted(width() / 4, height() / 4, -width() / 4, -height() / 4);
              bool centered = innerRect.intersects(atools::geo::rectToSquare(aircraftWpRect));

              // Minimum zoom depends on flight altitude
              float minZoom = atools::geo::nmToKm(14.7f);
              float alt = aircraft.getAltitudeAboveGroundFt();
              if(alt < 500.f)
                minZoom = atools::geo::nmToKm(0.5f);
              else if(alt < 1200.f)
                minZoom = atools::geo::nmToKm(0.9f);
              else if(alt < 2200.f)
                minZoom = atools::geo::nmToKm(1.8f);
              else if(alt < 10500.f)
                minZoom = atools::geo::nmToKm(7.4f);
              else if(alt < 20500.f)
                minZoom = atools::geo::nmToKm(14.7f);

              // Check if zoom in is needed
              bool rectTooSmall = aircraftWpRect.width() < width() / factor &&
                                  aircraftWpRect.height() < height() / factor;

              if(distance() < minZoom * 1.2f)
                // Already close enough - do not zoom in
                rectTooSmall = false;

              if(!curPosVisible || !nextWpPosVisible || updateAlways || rectTooSmall || !centered)
              {
                // Postpone scren updates
                setUpdatesEnabled(false);

                Rect rect(nextWpPos);
                rect.extend(aircraft.getPosition());
#ifdef DEBUG_INFORMATION
                qDebug() << Q_FUNC_INFO;
                qDebug() << "curPosVisible" << curPosVisible;
                qDebug() << "curPoint" << curPoint;
                qDebug() << "nextWpPosVisible" << nextWpPosVisible;
                qDebug() << "nextWpPoint" << nextWpPoint;
                qDebug() << "updateAlways" << updateAlways;
                qDebug() << "rectTooSmall" << rectTooSmall;
                qDebug() << "centered" << centered;

                qDebug() << "innerRect" << innerRect;
                qDebug() << "widgetRect" << widgetRect;
                qDebug() << "aircraftWpRect" << aircraftWpRect;
                qDebug() << "ac.getPosition()" << aircraft.getPosition();
                qDebug() << "rect" << rect;
                qDebug() << "minZoom" << minZoom;
#endif

                if(!rect.isPoint() /*&& rect.getWidthDegree() > 0.01 && rect.getHeightDegree() > 0.01*/)
                {
                  // Calculate a correction to inflate rectangle towards north and south since Marble is not
                  // capable of precise zooming in Mercator. Correction is increasing towards the poles
                  float nsCorrection = 0.f;
                  if(projection() == Mercator)
                  {
                    float lat = static_cast<float>(std::abs(centerLatitude()));
                    if(lat > 70.f)
                      nsCorrection = rect.getHeightDegree() / 2.f;
                    else if(lat > 60.f)
                      nsCorrection = rect.getHeightDegree() / 3.f;
                    else if(lat > 50.f)
                      nsCorrection = rect.getHeightDegree() / 6.f;

#ifdef DEBUG_INFORMATION
                    qDebug() << "nsCorrection" << nsCorrection;
#endif
                  }

                  GeoDataLatLonBox box(rect.getNorth() + nsCorrection, rect.getSouth() - nsCorrection,
                                       rect.getEast(), rect.getWest(),
                                       GeoDataCoordinates::Degree);

                  if(!box.isNull() &&
                     // Keep the stupid Marble widget from crashing
                     box.width(GeoDataCoordinates::Degree) < 180. &&
                     box.height(GeoDataCoordinates::Degree) < 180. &&
                     box.width(GeoDataCoordinates::Degree) > 0.000001 &&
                     box.height(GeoDataCoordinates::Degree) > 0.000001)
                  {
                    centerOn(box, false);
                    if(distance() < minZoom)
                      // Correct zoom for minimum distance
                      setDistance(minZoom);
                  }
                  else
                    centerOn(aircraft.getPosition().getLonX(), aircraft.getPosition().getLatY());
                }
                else if(rect.isValid())
                  centerOn(aircraft.getPosition().getLonX(), aircraft.getPosition().getLatY());

                mapUpdated = true;
              }
            }
            else
            {
              // Calculate the amount that has to be substracted from each side of the rectangle
              float boxFactor = (100.f - od.getSimUpdateBox()) / 100.f / 2.f;
              int dx = static_cast<int>(width() * boxFactor);
              int dy = static_cast<int>(height() * boxFactor);

              widgetRect.adjust(dx, dy, -dx, -dy);

              if(!widgetRect.contains(curPoint) || // Aircraft out of box or ...
                 updateAlways) // ... update always
              {
                setUpdatesEnabled(false);

                // Center aircraft only
                // Save distance value to avoid "zoom creep"
                double savedDistance = distance();
                centerOn(aircraft.getPosition().getLonX(), aircraft.getPosition().getLatY());
                setDistance(savedDistance);
                mapUpdated = true;
              }
            }
          }
        }
      }

      if(!updatesEnabled())
        setUpdatesEnabled(true);

      if(mapUpdated || dataHasChanged)
        // Not scrolled or zoomed but needs a redraw
        update();
    }
  }
  else if(paintLayer->getShownMapObjects() & map::AIRCRAFT_TRACK)
  {
    // No aircraft but track - update track only
    if(!last.isValid() || diff.manhattanLength() > 4)
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
  jumpBackToAircraftCancel();
  update();
}

void MapWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  // Clear all data on disconnect
  screenIndex->updateSimData(atools::fs::sc::SimConnectData());
  mapVisible->updateVisibleObjectsStatusBar();
  jumpBackToAircraftCancel();
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
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << approach;
#endif

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
                              (map::AIRPORT_ALL | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT),
                              newPoint.x(), newPoint.y(), screenSearchDistance, result);

  int totalSize = result.airports.size() + result.vors.size() + result.ndbs.size() + result.waypoints.size() +
                  result.userpoints.size();

  int id = -1;
  map::MapObjectTypes type = map::NONE;
  Pos pos = atools::geo::EMPTY_POS;
  if(totalSize == 0)
  {
    // Nothing at the position - add userpoint
    qDebug() << "add userpoint";
    type = map::USERPOINTROUTE;
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
    else if(!result.userpoints.isEmpty())
    {
      id = result.userpoints.first().id;
      type = map::USERPOINT;
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
       !result.waypoints.isEmpty() || !result.userpoints.isEmpty())
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
    for(const map::MapUserpoint& obj : result.userpoints)
    {
      QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                    menuPrefix + map::userpointText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, map::USERPOINT}));
      menu.addAction(action);
    }

    // Always present - userpoint
    menu.addSeparator();
    {
      QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                    menuPrefix + tr("Userpoint") + menuSuffix, this);
      action->setData(QVariantList({-1, map::USERPOINTROUTE}));
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

  if(type == map::USERPOINTROUTE)
    // Get position for new user point from from screen
    pos = conv.sToW(newPoint.x(), newPoint.y());

  if((id != -1 && type != map::NONE) || type == map::USERPOINTROUTE)
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

  // Disable any automatic scrolling
  contextMenuActive = true;

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

  // ===================================================================================
  // Texts with % will be replaced save them and let the ActionTextSaver restore them on return
  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch, ui->actionRouteAddPos, ui->actionRouteAppendPos,
                                          ui->actionMapShowInformation, ui->actionMapShowApproaches,
                                          ui->actionRouteDeleteWaypoint, ui->actionRouteAirportStart,
                                          ui->actionRouteAirportDest,
                                          ui->actionMapEditUserWaypoint, ui->actionMapUserdataAdd,
                                          ui->actionMapUserdataEdit, ui->actionMapUserdataDelete,
                                          ui->actionMapUserdataMove});
  Q_UNUSED(textSaver);

  // ===================================================================================
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

  menu.addAction(ui->actionMapUserdataAdd);
  menu.addAction(ui->actionMapUserdataEdit);
  menu.addAction(ui->actionMapUserdataMove);
  menu.addAction(ui->actionMapUserdataDelete);
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

  ui->actionMapUserdataAdd->setEnabled(visibleOnMap);
  ui->actionMapUserdataEdit->setEnabled(false);
  ui->actionMapUserdataDelete->setEnabled(false);
  ui->actionMapUserdataMove->setEnabled(false);

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

  // Get objects near position =============================================================
  map::MapSearchResult result;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDistance, result, procPoints);

  map::MapAirport *airport = nullptr;
  SimConnectAircraft *aiAircraft = nullptr;
  SimConnectAircraft *onlineAircraft = nullptr;
  SimConnectUserAircraft *userAircraft = nullptr;
  map::MapVor *vor = nullptr;
  map::MapNdb *ndb = nullptr;
  map::MapWaypoint *waypoint = nullptr;
  map::MapUserpointRoute *userpointRoute = nullptr;
  map::MapAirway *airway = nullptr;
  map::MapParking *parking = nullptr;
  map::MapHelipad *helipad = nullptr;
  map::MapAirspace *airspace = nullptr, *onlineCenter = nullptr;
  map::MapUserpoint *userpoint = nullptr;

  bool airportDestination = false, airportDeparture = false;
  // ===================================================================================
  // Get only one object of each type
  if(result.userAircraft.isValid())
    userAircraft = &result.userAircraft;

  if(!result.aiAircraft.isEmpty())
    aiAircraft = &result.aiAircraft.first();

  if(!result.onlineAircraft.isEmpty())
    onlineAircraft = &result.onlineAircraft.first();

  if(!result.airports.isEmpty())
  {
    airport = &result.airports.first();
    airportDestination = NavApp::getRouteConst().isAirportDestination(airport->ident);
    airportDeparture = NavApp::getRouteConst().isAirportDeparture(airport->ident);
  }

  if(!result.parkings.isEmpty())
    parking = &result.parkings.first();

  if(!result.helipads.isEmpty() && result.helipads.first().startId != -1)
    // Only helipads with start position are allowed
    helipad = &result.helipads.first();

  if(!result.vors.isEmpty())
    vor = &result.vors.first();

  if(!result.ndbs.isEmpty())
    ndb = &result.ndbs.first();

  if(!result.waypoints.isEmpty())
    waypoint = &result.waypoints.first();

  if(!result.userPointsRoute.isEmpty())
    userpointRoute = &result.userPointsRoute.first();

  if(!result.userpoints.isEmpty())
    userpoint = &result.userpoints.first();

  if(!result.airways.isEmpty())
    airway = &result.airways.first();

  if(!result.airspaces.isEmpty())
    airspace = &result.airspaces.first();

  // Add "more" text if multiple navaids will be added to the information panel
  bool andMore = (result.vors.size() + result.ndbs.size() + result.waypoints.size() + result.userpoints.size() +
                  result.userPointsRoute.size() + result.airways.size()) > 1 && airport == nullptr;

  // ===================================================================================
  // Collect information from the search result - build text only for one object for several menu items
  bool isAircraft = false;
  QString informationText, measureText, rangeRingText, departureText, departureParkingText, destinationText,
          addRouteText, searchText, editUserpointText;

  if(airspace != nullptr)
  {
    if(airspace->online)
    {
      onlineCenter = airspace;
      searchText = informationText = tr("Online Center %1").arg(onlineCenter->name);
    }
    else
      informationText = tr("Airspace");
  }

  if(airway != nullptr)
    informationText = map::airwayText(*airway);

  if(userpointRoute != nullptr)
    // No show information on user point
    informationText.clear();

  if(userpoint != nullptr)
    editUserpointText = informationText = measureText = addRouteText = searchText = map::userpointText(*userpoint);

  if(waypoint != nullptr)
    informationText = measureText = addRouteText = searchText = map::waypointText(*waypoint);

  if(ndb != nullptr)
    informationText = measureText = rangeRingText = addRouteText = searchText = map::ndbText(*ndb);

  if(vor != nullptr)
    informationText = measureText = rangeRingText = addRouteText = searchText = map::vorText(*vor);

  if(airport != nullptr)
    informationText = measureText = departureText
                                      = destinationText = addRouteText = searchText = map::airportText(*airport);

  int departureParkingAirportId = -1;
  // Parking or helipad only if no airport at cursor
  if(airport == nullptr)
  {
    if(helipad != nullptr)
    {
      departureParkingAirportId = helipad->airportId;
      departureParkingText = tr("Helipad %1").arg(helipad->runwayName);
    }

    if(parking != nullptr)
    {
      departureParkingAirportId = parking->airportId;
      if(parking->number == -1)
        departureParkingText = map::parkingName(parking->name);
      else
        departureParkingText = map::parkingName(parking->name) + " " + QLocale().toString(parking->number);
    }
  }

  if(departureParkingAirportId != -1)
  {
    // Clear texts which are not valid for parking positions
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

  if(onlineAircraft != nullptr)
  {
    searchText = informationText = tr("Online Client Aircraft %1").arg(onlineAircraft->getAirplaneRegistration());
    isAircraft = true;
  }

  if(userAircraft != nullptr)
  {
    informationText = tr("User Aircraft");
    isAircraft = true;
  }

  // ===================================================================================
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
  else if(userpointRoute != nullptr && userpointRoute->routeIndex != -1)
  {
    routeText = map::userpointRouteText(*userpointRoute);
    routeIndex = userpointRoute->routeIndex;
    deleteType = map::USERPOINTROUTE;
  }

  // ===================================================================================
  // Update "set airport as start/dest"
  if(airport != nullptr || departureParkingAirportId != -1)
  {
    QString airportText(departureText);

    if(departureParkingAirportId != -1)
    {
      // Get airport for parking
      map::MapAirport parkAp;
      airportQuery->getAirportById(parkAp, departureParkingAirportId);
      airportText = map::airportText(parkAp) + " / ";
    }

    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(airportText + departureParkingText));

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
    // No airport or selected parking position
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // ===================================================================================
  // Update "show information" for airports, navaids, airways and airspaces
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     airway != nullptr || airspace != nullptr || userpoint != nullptr)
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

  // ===================================================================================
  // Update "edit userpoint" and "add userpoint"
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
    ui->actionMapUserdataAdd->setText(ui->actionMapUserdataAdd->text().arg(informationText));
  else
    ui->actionMapUserdataAdd->setText(ui->actionMapUserdataAdd->text().arg(QString()));

  if(userpoint != nullptr)
  {
    ui->actionMapUserdataEdit->setEnabled(true);
    ui->actionMapUserdataEdit->setText(ui->actionMapUserdataEdit->text().arg(editUserpointText));
    ui->actionMapUserdataDelete->setEnabled(true);
    ui->actionMapUserdataDelete->setText(ui->actionMapUserdataDelete->text().arg(editUserpointText));
    ui->actionMapUserdataMove->setEnabled(true);
    ui->actionMapUserdataMove->setText(ui->actionMapUserdataMove->text().arg(editUserpointText));
  }
  else
  {
    ui->actionMapUserdataEdit->setText(ui->actionMapUserdataEdit->text().arg(QString()));
    ui->actionMapUserdataDelete->setText(ui->actionMapUserdataDelete->text().arg(QString()));
    ui->actionMapUserdataMove->setText(ui->actionMapUserdataMove->text().arg(QString()));
  }

  // ===================================================================================
  // Update "show in search" and "add to route" only for airports an navaids
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     userpoint != nullptr)
  {
    ui->actionRouteAddPos->setEnabled(true);
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(addRouteText));
    ui->actionRouteAppendPos->setEnabled(true);
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(addRouteText));
  }
  else
  {
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(tr("Position")));
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(tr("Position")));
  }

  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     userpoint != nullptr || onlineAircraft != nullptr || onlineCenter != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(searchText));
  }
  else
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));

  if(airport != nullptr)
  {
    bool hasAnyArrival = NavApp::getAirportQueryNav()->hasAnyArrivalProcedures(airport->ident);
    bool hasDeparture = NavApp::getAirportQueryNav()->hasDepartureProcedures(airport->ident);

    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture)
      {
        if(hasDeparture)
        {
          ui->actionMapShowApproaches->setEnabled(true);
          ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("Departure ")).
                                               arg(informationText));
        }
        else
          ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no departure procedure)").arg(airport->ident));
      }
      else if(airportDestination)
      {
        if(hasAnyArrival)
        {
          ui->actionMapShowApproaches->setEnabled(true);
          ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("Arrival ")).
                                               arg(informationText));
        }
        else
          ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no arrival procedure)").arg(airport->ident));
      }
      else
      {
        ui->actionMapShowApproaches->setEnabled(true);
        ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("all ")).arg(informationText));
      }
    }
    else
      ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no procedure)").arg(airport->ident));
  }
  else
    ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(QString()).arg(QString()));

  // Update "delete in route"
  if(routeIndex != -1 && NavApp::getRouteConst().canEditPoint(routeIndex))
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(routeText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(tr("Position")));

  // Update "name user waypoint"
  if(routeIndex != -1 && userpointRoute != nullptr)
  {
    ui->actionMapEditUserWaypoint->setEnabled(true);
    ui->actionMapEditUserWaypoint->setText(ui->actionMapEditUserWaypoint->text().arg(routeText));
  }
  else
    ui->actionMapEditUserWaypoint->setText(ui->actionMapEditUserWaypoint->text().arg(tr("Position")));

  // Update "show range rings for Navaid"
  if(vor != nullptr || ndb != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(rangeRingText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && helipad == nullptr && !measureText.isEmpty())
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

  qDebug() << "departureParkingAirportId " << departureParkingAirportId;
  qDebug() << "airport " << airport;
  qDebug() << "vor " << vor;
  qDebug() << "ndb " << ndb;
  qDebug() << "waypoint " << waypoint;
  qDebug() << "parking " << parking;
  qDebug() << "helipad " << helipad;
  qDebug() << "routeIndex " << routeIndex;
  qDebug() << "userpointRoute " << userpointRoute;
  qDebug() << "informationText" << informationText;
  qDebug() << "measureText" << measureText;
  qDebug() << "departureText" << departureText;
  qDebug() << "destinationText" << destinationText;
  qDebug() << "addRouteText" << addRouteText;
  qDebug() << "searchText" << searchText;

  // Show the menu ------------------------------------------------
  QAction *action = menu.exec(menuPos);

  contextMenuActive = false;

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
      // Create records and send show in search signal
      // This works only with line edit fields
      ui->dockWidgetSearch->raise();
      ui->dockWidgetSearch->show();
      if(airport != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(0);
        emit showInSearch(map::AIRPORT, SqlRecord().appendFieldAndValue("ident", airport->ident));
      }
      else if(vor != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        SqlRecord rec;
        rec.appendFieldAndValue("ident", vor->ident);
        if(!vor->region.isEmpty())
          rec.appendFieldAndValue("region", vor->region);

        emit showInSearch(map::VOR, rec);
      }
      else if(ndb != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        SqlRecord rec;
        rec.appendFieldAndValue("ident", ndb->ident);
        if(!ndb->region.isEmpty())
          rec.appendFieldAndValue("region", ndb->region);

        emit showInSearch(map::NDB, rec);
      }
      else if(waypoint != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        SqlRecord rec;
        rec.appendFieldAndValue("ident", waypoint->ident);
        if(!waypoint->region.isEmpty())
          rec.appendFieldAndValue("region", waypoint->region);

        emit showInSearch(map::WAYPOINT, rec);
      }
      else if(userpoint != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(3);
        SqlRecord rec;
        if(!userpoint->ident.isEmpty())
          rec.appendFieldAndValue("ident", userpoint->ident);
        if(!userpoint->region.isEmpty())
          rec.appendFieldAndValue("region", userpoint->region);
        if(!userpoint->name.isEmpty())
          rec.appendFieldAndValue("name", userpoint->name);
        if(!userpoint->type.isEmpty())
          rec.appendFieldAndValue("type", userpoint->type);
        if(!userpoint->tags.isEmpty())
          rec.appendFieldAndValue("tags", userpoint->tags);

        emit showInSearch(map::USERPOINT, rec);
      }
      else if(onlineAircraft != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(4);
        SqlRecord rec;
        rec.appendFieldAndValue("callsign", onlineAircraft->getAirplaneRegistration());

        emit showInSearch(map::AIRCRAFT_ONLINE, rec);
      }
      else if(onlineCenter != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(5);
        SqlRecord rec;
        rec.appendFieldAndValue("callsign", onlineCenter->name);

        emit showInSearch(map::AIRSPACE_ONLINE, rec);
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
        dm.color = mapcolors::vorSymbolColor;
      }
      else if(ndb != nullptr)
      {
        dm.text = ndb->ident + " " + QLocale().toString(ndb->frequency / 100., 'f', 2);
        dm.from = ndb->position;
        dm.magvar = ndb->magvar;
        dm.color = mapcolors::ndbSymbolColor;
      }
      else if(waypoint != nullptr)
      {
        dm.text = waypoint->ident;
        dm.from = waypoint->position;
        dm.magvar = waypoint->magvar;
        dm.color = mapcolors::waypointSymbolColor;
      }
      else
      {
        dm.magvar = NavApp::getMagVar(pos, 0.f);
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
      else if(helipad != nullptr)
      {
        id = helipad->id;
        type = map::HELIPAD;
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
      else if(userpoint != nullptr)
      {
        id = userpoint->id;
        type = map::USERPOINT;
      }
      else if(waypoint != nullptr)
      {
        id = waypoint->id;
        type = map::WAYPOINT;
      }
      else if(aiAircraft != nullptr)
      {
        id = aiAircraft->getId();
        type = map::AIRCRAFT_AI;
      }
      else if(onlineAircraft != nullptr)
      {
        id = onlineAircraft->getId();
        type = map::AIRCRAFT_ONLINE;
      }
      else if(onlineCenter != nullptr)
      {
        id = onlineCenter->id;
        type = map::AIRSPACE_ONLINE;
      }
      else
      {
        if(userpointRoute != nullptr)
          id = userpointRoute->id;
        type = map::USERPOINTROUTE;
        position = pos;
      }

      if(action == ui->actionRouteAirportStart && parking != nullptr)
        emit routeSetParkingStart(*parking);
      else if(action == ui->actionRouteAirportStart && helipad != nullptr)
        emit routeSetHelipadStart(*helipad);
      else if(action == ui->actionRouteAddPos || action == ui->actionRouteAppendPos)
      {
        if(parking != nullptr || helipad != nullptr)
        {
          // Adjust values in case user selected "add" on a parking position
          type = map::USERPOINTROUTE;
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
    else if(action == ui->actionMapUserdataAdd)
    {
      if(NavApp::getElevationProvider()->isGlobeOfflineProvider())
        pos.setAltitude(atools::geo::meterToFeet(NavApp::getElevationProvider()->getElevationMeter(pos)));
      emit addUserpointFromMap(result, pos);
    }
    else if(action == ui->actionMapUserdataEdit)
      emit editUserpointFromMap(result);
    else if(action == ui->actionMapUserdataDelete)
      emit deleteUserpointFromMap(userpoint->id);
    else if(action == ui->actionMapUserdataMove)
    {
      if(userpoint != nullptr)
      {
        userpointDragPixmap = *NavApp::getUserdataIcons()->getIconPixmap(userpoint->type,
                                                                         paintLayer->getMapLayer()->getUserPointSymbolSize());
        userpointDragCur = point;
        userpointDrag = *userpoint;
        // Start mouse dragging and disable context menu so we can catch the right button click as cancel
        mouseState = mw::DRAG_USER_POINT;
        setContextMenuPolicy(Qt::PreventContextMenu);
      }
    }
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

  if(rect().contains(point))
  {
    if(geoCoordinates(point.x(), point.y(), lon, lat, GeoDataCoordinates::Degree))
    {
      Pos pos(lon, lat);
      pos.setAltitude(NavApp::getElevationProvider()->getElevationMeter(pos));
      mainWindow->updateMapPosLabel(pos, point.x(), point.y());
    }
  }
}

bool MapWidget::eventFilter(QObject *obj, QEvent *e)
{
  if(e->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    if(keyEvent != nullptr)
    {
      if(atools::contains(static_cast<Qt::Key>(keyEvent->key()),
                          {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down}))
        // Movement starts delay every time
        jumpBackToAircraftStart();

      if(jumpBackToAircraftActive)
        // Only delay if already active
        jumpBackToAircraftStart();

      if(atools::contains(static_cast<Qt::Key>(keyEvent->key()), {Qt::Key_Plus, Qt::Key_Minus}) &&
         (keyEvent->modifiers() & Qt::ControlModifier))
      {
        // Catch Ctrl++ and Ctrl+- and use it only for details
        // Do not let marble use it for zooming

        e->accept(); // Do not propagate further
        event(e); // Call own event handler
        return true; // Do not process further
      }
    }
  }

  if(e->type() == QEvent::Wheel && (jumpBackToAircraftActive || isCenterLegAndAircraftActive()))
    // Only delay if already active. Allow zooming and jumpback if autozoom is on
    jumpBackToAircraftStart();

  if(e->type() == QEvent::MouseButtonDblClick)
  {
    // Catch the double click event

    e->accept(); // Do not propagate further
    event(e); // Call own event handler
    return true; // Do not process further
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
      mainWindow->updateMapPosLabel(Pos(lon, lat, static_cast<double>(map::INVALID_ALTITUDE_VALUE)),
                                    mouseEvent->pos().x(), mouseEvent->pos().y());
    }
    else
      mainWindow->updateMapPosLabel(Pos(), -1, -1);
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
  cancelDragUserpoint();
  cancelDragDistance();

  mouseState = mw::NONE;
  setViewContext(Marble::Still);
  update();
}

/* Stop userpoint editing and reset coordinates and pixmap */
void MapWidget::cancelDragUserpoint()
{
  if(mouseState & mw::DRAG_USER_POINT)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    userpointDragCur = QPoint();
    userpointDrag = map::MapUserpoint();
    userpointDragPixmap = QPixmap();
  }
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

#ifdef DEBUG_MOVING_AIRPLANE
void MapWidget::debugMovingPlane(QMouseEvent *event)
{
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
}

#endif

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

#ifdef DEBUG_MOVING_AIRPLANE
  debugMovingPlane(event);
#endif

  qreal lon = 0., lat = 0.;
  bool visible = false;
  if(mouseState & mw::DRAG_ALL)
  {
    jumpBackToAircraftStart();

    // Currently dragging a measurement line
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
  }

  if(mouseState & mw::DRAG_DISTANCE || mouseState & mw::DRAG_CHANGE_DISTANCE)
  {
    // Position is valid update the distance mark continuously
    if(visible && !screenIndex->getDistanceMarks().isEmpty())
      screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = Pos(lon, lat);

  }
  else if(mouseState & mw::DRAG_ROUTE_LEG || mouseState & mw::DRAG_ROUTE_POINT)
  {
    if(visible)
      // Update current point
      routeDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState & mw::DRAG_USER_POINT)
  {
    if(visible)
      // Update current point
      userpointDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState == mw::NONE)
  {
    if(event->buttons() == Qt::NoButton)
    {
      // No dragging going on now - update cursor over flight plan legs or markers
      const Route& route = NavApp::getRouteConst();

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
    else
      jumpBackToAircraftStart();
  }

  if(mouseState & mw::DRAG_ALL)
  {
    // Force fast updates while dragging
    setViewContext(Marble::Animation);
    update();
  }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
#ifdef DEBUG_MOVING_AIRPLANE
  debugMovingPlane(event);
#endif

  hideTooltip();

  jumpBackToAircraftStart();

  // Remember mouse position to check later if mouse was moved during click (drag map scroll)
  mouseMoved = event->pos();
  if(mouseState & mw::DRAG_ALL)
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

  jumpBackToAircraftStart();

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
  else if(mouseState & mw::DRAG_USER_POINT)
  {
    // End userpoint dragging
    if(mouseState & mw::DRAG_POST)
    {
      // Ending route dragging - update route
      qreal lon, lat;
      bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      if(visible)
      {
        // Create a copy before cancel
        map::MapUserpoint newUserpoint = userpointDrag;
        newUserpoint.position = Pos(lon, lat);
        emit moveUserpointFromMap(newUserpoint);
      }
    }

    cancelDragUserpoint();

    // End all dragging
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
        const Route& route = NavApp::getRouteConst();

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
  qDebug() << Q_FUNC_INFO;

  jumpBackToAircraftStart();

  if(mouseState != mw::NONE)
    return;

  map::MapSearchResult mapSearchResult;
  QList<proc::MapProcedurePoint> procPoints;
  screenIndex->getAllNearest(event->pos().x(), event->pos().y(), screenSearchDistance, mapSearchResult, procPoints);

  if(mapSearchResult.userAircraft.isValid())
  {
    showPos(mapSearchResult.userAircraft.getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing user aircraft on map.")));
  }
  else if(!mapSearchResult.aiAircraft.isEmpty())
  {
    showPos(mapSearchResult.aiAircraft.first().getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing AI / multiplayer aircraft on map.")));
  }
  else if(!mapSearchResult.onlineAircraft.isEmpty())
  {
    showPos(mapSearchResult.onlineAircraft.first().getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing online client aircraft on map.")));
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
    else if(!mapSearchResult.userPointsRoute.isEmpty())
      showPos(mapSearchResult.userPointsRoute.first().position, 0.f, true);
    else if(!mapSearchResult.userpoints.isEmpty())
      showPos(mapSearchResult.userpoints.first().position, 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing navaid or userpoint on map.")));
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
  mainWindow->updateMapPosLabel(Pos(), -1, -1);
}

void MapWidget::keyPressEvent(QKeyEvent *event)
{
  // Does not work for key presses that are consumed by the widget

  if(event->key() == Qt::Key_Escape)
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }

  if(event->key() == Qt::Key_Menu && mouseState == mw::NONE)
    // First menu key press after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);

  jumpBackToAircraftStart();
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
  QString text = mapTooltip->buildTooltip(mapSearchResultTooltip, procPointsTooltip, NavApp::getRouteConst(),
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

    if(!noStoreInHistory)
      // Not changed by next/last in history
      history.addEntry(Pos(centerLongitude(), centerLatitude()), distance());

    noStoreInHistory = false;
    changed = true;
  }

  MarbleWidget::paintEvent(paintEvent);

  if(changed)
  {
    // Major change - update index and visible objects
    mapVisible->updateVisibleObjectsStatusBar();
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

  // Remove all undesired features
  opts::DisplayClickOptions opts = OptionData::instance().getDisplayClickOptions();
  if(!(opts & opts::CLICK_AIRPORT))
  {
    result.airports.clear();
    result.airportIds.clear();
  }

  if(!(opts & opts::CLICK_NAVAID))
  {
    result.vors.clear();
    result.vorIds.clear();
    result.ndbs.clear();
    result.ndbIds.clear();
    result.waypoints.clear();
    result.waypointIds.clear();
    result.airways.clear();
    result.userpoints.clear();
  }

  if(!(opts & opts::CLICK_AIRSPACE))
    result.airspaces.clear();

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

void MapWidget::jumpBackToAircraftStart()
{
  if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked() && NavApp::isConnected() &&
     NavApp::isUserAircraftValid() && OptionData::instance().getFlags2() & opts::ROUTE_NO_FOLLOW_ON_MOVE)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO;
#endif

    if(!jumpBackToAircraftActive)
    {
      // Do not update position if already active
      jumpBackToAircraftDistance = distance();
      jumpBackToAircraftPos = Pos(centerLongitude(), centerLatitude());
      jumpBackToAircraftActive = true;
    }

    // Restart timer
    jumpBackToAircraftTimer.setInterval(OptionData::instance().getSimNoFollowAircraftOnScrollSeconds() * 1000);
    jumpBackToAircraftTimer.start();
  }
}

void MapWidget::jumpBackToAircraftCancel()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  jumpBackToAircraftTimer.stop();
  jumpBackToAircraftActive = false;
}

void MapWidget::onlineClientAndAtcUpdated()
{
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  update();
}

void MapWidget::onlineNetworkChanged()
{
  screenIndex->resetAirspaceOnlineScreenGeometry();
  screenIndex->updateAirspaceScreenGeometry(currentViewBoundingBox);
  update();
}

void MapWidget::takeoffLandingTimeout()
{
  const atools::fs::sc::SimConnectUserAircraft aircraft = screenIndex->getLastUserAircraft();

  if(aircraft.isFlying())
  {
    // In air after  status has changed
    qDebug() << Q_FUNC_INFO << "Takeoff detected" << aircraft.getZuluTime();

    takeoffLandingDistanceNm = 0.;
    takeoffLandingAverageTasKts = aircraft.getTrueAirspeedKts();
    takeoffLastSampleTimeMs = takeoffTimeMs = aircraft.getZuluTime().toMSecsSinceEpoch();

    emit aircraftTakeoff(aircraft);
  }
  else
  {
    // On ground after status has changed
    qDebug() << Q_FUNC_INFO << "Landing detected takeoffLandingDistanceNm" << takeoffLandingDistanceNm;
    emit aircraftLanding(aircraft,
                         static_cast<float>(takeoffLandingDistanceNm),
                         static_cast<float>(takeoffLandingAverageTasKts));
  }
}

void MapWidget::jumpBackToAircraftTimeout()
{
  if(mouseState != mw::NONE || viewContext() == Marble::Animation || contextMenuActive)
  {
    // Restart as long as menu is active or user is dragging around
    jumpBackToAircraftStart();
  }
  else
  {
    jumpBackToAircraftActive = false;

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO;
#endif

    if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked() && NavApp::isConnected() &&
       NavApp::isUserAircraftValid() && OptionData::instance().getFlags2() & opts::ROUTE_NO_FOLLOW_ON_MOVE)
    {
      hideTooltip();
      setDistance(jumpBackToAircraftDistance);
      centerOn(jumpBackToAircraftPos.getLonX(), jumpBackToAircraftPos.getLatY(), false);
      mainWindow->setStatusMessage(tr("Jumped back to aircraft."));
    }
  }
}
