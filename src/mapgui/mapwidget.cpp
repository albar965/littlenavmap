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

#include "mapgui/mapwidget.h"

#include "options/optiondata.h"
#include "common/constants.h"
#include "mapgui/mappaintlayer.h"
#include "settings/settings.h"
#include "gui/mainwindow.h"
#include "mapgui/mapscale.h"
#include "geo/calculations.h"
#include "common/maptools.h"
#include "common/mapcolors.h"
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
#include "zip/zipreader.h"

#include <QContextMenuEvent>
#include <QToolTip>
#include <QRubberBand>
#include <QMessageBox>

#include <marble/MarbleLocale.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/MarbleModel.h>

// Default zoom distance if start position was not set (usually first start after installation */
const int DEFAULT_MAP_DISTANCE = 7000;

// Update rates defined by delta values
const static QHash<opts::SimUpdateRate, MapWidget::SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
  {
    {
      opts::FAST, {1, 1.f, 1.f, 1.f}
    },
    {
      opts::MEDIUM, {2, 1.f, 10.f, 10.f}
    },
    {
      opts::LOW, {4, 4.f, 10.f, 100.f}
    }
  });

using namespace Marble;
using atools::gui::MapPosHistoryEntry;
using atools::gui::MapPosHistory;

MapWidget::MapWidget(MainWindow *parent, MapQuery *query)
  : Marble::MarbleWidget(parent), mainWindow(parent), mapQuery(query)
{
  // Read cache sizes from options
  updateCacheSizes();

  // Event filter needed to disable some unwanted Marble default functionality
  installEventFilter(this);

  // Set the map quality to gain speed
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  // All nautical miles and feet for now
  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);

  // Avoid stuttering movements
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapTooltip = new MapTooltip(this, mapQuery, mainWindow->getWeatherReporter());

  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  screenIndex = new MapScreenIndex(this, mapQuery, paintLayer);

  // Disable all unwante popups on mouse click
  MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);

  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();
}

MapWidget::~MapWidget()
{
  delete paintLayer;
  delete mapTooltip;
  delete screenIndex;
}

void MapWidget::setTheme(const QString& theme, int index)
{
  Q_UNUSED(index);
  qDebug() << "setting map theme to " << theme;

  Ui::MainWindow *ui = mainWindow->getUi();
  currentComboIndex = MapWidget::MapThemeComboIndex(index);

  // Update theme specific options
  switch(index)
  {
    case MapWidget::OPENTOPOMAP:
      setPropertyValue("ice", false);
      ui->actionMapShowCities->setEnabled(false);
      ui->actionMapShowHillshading->setEnabled(false);
      break;
    case MapWidget::OPENSTREETMAP:
      ui->actionMapShowCities->setEnabled(false);
      ui->actionMapShowHillshading->setEnabled(true);
      break;
    case MapWidget::SIMPLE:
    case MapWidget::PLAIN:
      ui->actionMapShowCities->setEnabled(true);
      ui->actionMapShowHillshading->setEnabled(false);
      break;
    case MapWidget::INVALID:
      break;
  }

  setMapThemeId(theme);
  updateMapObjectsShown();
}

void MapWidget::optionsChanged()
{
  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();

  updateCacheSizes();
}

void MapWidget::updateCacheSizes()
{
  // kb
  qDebug() << "Volatile cache to" << OptionData::instance().getCacheSizeMemoryMb() * 1000L << "kb";
  setVolatileTileCacheLimit(OptionData::instance().getCacheSizeMemoryMb() * 1000L);
  // kb
  qDebug() << "Persistent cache to" << OptionData::instance().getCacheSizeDiskMb() * 1000L << "kb";
  model()->setPersistentTileCacheLimit(OptionData::instance().getCacheSizeDiskMb() * 1000L);
}

void MapWidget::updateMapObjectsShown()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  setShowMapPois(ui->actionMapShowCities->isChecked() &&
                 (currentComboIndex == MapWidget::SIMPLE || currentComboIndex == MapWidget::PLAIN));
  setShowGrid(ui->actionMapShowGrid->isChecked());
  setPropertyValue("hillshading", ui->actionMapShowHillshading->isChecked() &&
                   currentComboIndex == MapWidget::OPENSTREETMAP);

  setShowMapFeatures(maptypes::AIRWAYV, ui->actionMapShowVictorAirways->isChecked());
  setShowMapFeatures(maptypes::AIRWAYJ, ui->actionMapShowJetAirways->isChecked());

  setShowMapFeatures(maptypes::ROUTE, ui->actionMapShowRoute->isChecked());
  setShowMapFeatures(maptypes::AIRCRAFT, ui->actionMapShowAircraft->isChecked());
  setShowMapFeatures(maptypes::AIRCRAFT_TRACK, ui->actionMapShowAircraftTrack->isChecked());

  setShowMapFeatures(maptypes::AIRPORT_HARD, ui->actionMapShowAirports->isChecked());
  setShowMapFeatures(maptypes::AIRPORT_SOFT, ui->actionMapShowSoftAirports->isChecked());

  bool showEmpty = ui->actionMapShowEmptyAirports->isChecked() ||
                   !(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS);

  setShowMapFeatures(maptypes::AIRPORT_EMPTY, showEmpty);

  // Force addon airport independent of other settings or not
  setShowMapFeatures(maptypes::AIRPORT_ADDON, ui->actionMapShowAddonAirports->isChecked());

  // Set the general airport flag if any airport is selected
  setShowMapFeatures(maptypes::AIRPORT,
                     ui->actionMapShowAirports->isChecked() ||
                     ui->actionMapShowSoftAirports->isChecked() ||
                     showEmpty ||
                     ui->actionMapShowAddonAirports->isChecked());

  setShowMapFeatures(maptypes::VOR, ui->actionMapShowVor->isChecked());
  setShowMapFeatures(maptypes::NDB, ui->actionMapShowNdb->isChecked());
  setShowMapFeatures(maptypes::ILS, ui->actionMapShowIls->isChecked());
  setShowMapFeatures(maptypes::WAYPOINT, ui->actionMapShowWp->isChecked());

  updateVisibleObjectsStatusBar();

  // Update widget
  update();
}

void MapWidget::setShowMapPois(bool show)
{
  qDebug() << "setShowMapPois" << show;
  // Enable all POI stuff
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
  setShowTerrain(show);
}

void MapWidget::setShowMapFeatures(maptypes::MapObjectTypes type, bool show)
{
  paintLayer->setShowMapObjects(type, show);
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
}

void MapWidget::setDetailLevel(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
  updateVisibleObjectsStatusBar();
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
}

maptypes::MapObjectTypes MapWidget::getShownMapFeatures()
{
  return paintLayer->getShownMapObjects();
}

RouteController *MapWidget::getRouteController() const
{
  return mainWindow->getRouteController();
}

void MapWidget::getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur)
{
  from = routeDragFrom;
  to = routeDragTo;
  cur = routeDragCur;
}

void MapWidget::preDatabaseLoad()
{
  databaseLoadStatus = true;
  paintLayer->preDatabaseLoad();
}

void MapWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  paintLayer->postDatabaseLoad();
  screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
  screenIndex->updateRouteScreenGeometry();
  update();
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
  }
}

void MapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  writePluginSettings(*s.getQSettings());

  s.setValue(lnm::MAP_MARKLONX, searchMarkPos.getLonX());
  s.setValue(lnm::MAP_MARKLATY, searchMarkPos.getLatY());

  s.setValue(lnm::MAP_HOMELONX, homePos.getLonX());
  s.setValue(lnm::MAP_HOMELATY, homePos.getLatY());
  s.setValue(lnm::MAP_HOMEDISTANCE, homeDistance);
  s.setValue(lnm::MAP_KMLFILES, kmlFilePaths);

  s.setValue(lnm::MAP_DETAILFACTOR, mapDetailLevel);

  history.saveState(lnm::MAP_HISTORY);
  screenIndex->saveState();
}

void MapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  readPluginSettings(*s.getQSettings());

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
    mapDetailLevel = s.valueInt(lnm::MAP_DETAILFACTOR, MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR);
  else
    mapDetailLevel = MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailLevel);

  if(s.contains(lnm::MAP_MARKLONX) && s.contains(lnm::MAP_MARKLATY))
    searchMarkPos = atools::geo::Pos(s.valueFloat(lnm::MAP_MARKLONX), s.valueFloat(lnm::MAP_MARKLATY));
  else
    searchMarkPos = atools::geo::Pos(0.f, 0.f);
  emit searchMarkChanged(searchMarkPos);

  if(s.contains(lnm::MAP_HOMELONX) && s.contains(lnm::MAP_HOMELATY) && s.contains(lnm::MAP_HOMEDISTANCE))
  {
    homePos = atools::geo::Pos(s.valueFloat(lnm::MAP_HOMELONX), s.valueFloat(lnm::MAP_HOMELATY));
    homeDistance = s.valueFloat(lnm::MAP_HOMEDISTANCE);
  }
  else
  {
    // Looks like first start after installation
    homePos = atools::geo::Pos(0.f, 0.f);
    homeDistance = DEFAULT_MAP_DISTANCE;
  }

  history.restoreState(lnm::MAP_HISTORY);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_KML)
    kmlFilePaths = s.valueStrList(lnm::MAP_KMLFILES);
  screenIndex->restoreState();
}

void MapWidget::mainWindowShown()
{
  // Create a copy of KML files where all missing files will be removed from the recent list
  QStringList copyKml(kmlFilePaths);
  for(const QString& kml : kmlFilePaths)
  {
    if(!loadKml(kml, false /* center */))
      copyKml.removeAll(kml);
  }

  kmlFilePaths = copyKml;

  showSavedPosOnStartup();
}

void MapWidget::showSavedPosOnStartup()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_HOME)
    showHome();
  else if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_LAST)
  {
    const MapPosHistoryEntry& currentPos = history.current();

    hideTooltip();

    if(currentPos.isValid())
    {
      centerOn(currentPos.getPos().getLonX(), currentPos.getPos().getLatY(), false);
      setDistance(currentPos.getDistance());
    }
    else
    {
      centerOn(0.f, 0.f, false);
      setDistance(DEFAULT_MAP_DISTANCE);
    }
  }
}

void MapWidget::showPos(const atools::geo::Pos& pos, int distanceNm)
{
  qDebug() << "NavMapWidget::showPoint" << pos;
  hideTooltip();

  if(distanceNm == -1)
    setDistance(atools::geo::nmToKm(OptionData::instance().getMapZoomShow()));
  else
    setDistance(atools::geo::nmToKm(distanceNm));
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void MapWidget::showRect(const atools::geo::Rect& rect)
{
  qDebug() << "NavMapWidget::showRect" << rect;
  hideTooltip();
  centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                            GeoDataCoordinates::Degree), false);
}

void MapWidget::showSearchMark()
{
  qDebug() << "NavMapWidget::showMark" << searchMarkPos;

  hideTooltip();

  if(searchMarkPos.isValid())
  {
    setDistance(atools::geo::nmToKm(OptionData::instance().getMapZoomShow()));
    centerOn(searchMarkPos.getLonX(), searchMarkPos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showAircraft(bool centerAircraftChecked)
{
  qDebug() << "NavMapWidget::showAircraft" << searchMarkPos;

  if(centerAircraftChecked && simData.getPosition().isValid())
    centerOn(simData.getPosition().getLonX(), simData.getPosition().getLatY(), false);
}

void MapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << searchMarkPos;

  hideTooltip();

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
  homePos = atools::geo::Pos(centerLongitude(), centerLatitude());
  homeDistance = distance();
  update();
  mainWindow->setStatusMessage(QString(tr("Changed home position.")));
}

void MapWidget::changeRouteHighlights(const RouteMapObjectList& routeHighlight)
{
  screenIndex->getRouteHighlights() = routeHighlight;
  update();
}

void MapWidget::routeChanged(bool geometryChanged)
{
  if(geometryChanged)
  {
    screenIndex->updateRouteScreenGeometry();
    update();
  }
}

void MapWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(databaseLoadStatus)
    return;

  simData = simulatorData;

  CoordinateConverter conv(viewport());
  QPoint curPos = conv.wToS(simulatorData.getPosition());
  QPoint diff = curPos - conv.wToS(lastSimData.getPosition());

  bool wasEmpty = aircraftTrack.isEmpty();
  aircraftTrack.appendTrackPos(simulatorData.getPosition(),
                               simulatorData.getFlags() & atools::fs::sc::ON_GROUND);

  if(wasEmpty != aircraftTrack.isEmpty())
    // We have a track update toolbar and menu
    emit updateActionStates();

  if(paintLayer->getShownMapObjects() & maptypes::AIRCRAFT)
  {
    // Show aircraft is enabled

    // Get delta values for update rate
    const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

    using atools::almostNotEqual;
    if(!lastSimData.getPosition().isValid() ||
       diff.manhattanLength() >= deltas.manhattanLengthDelta ||
       almostNotEqual(lastSimData.getHeadingDegMag(),
                      simData.getHeadingDegMag(), deltas.headingDelta) ||
       almostNotEqual(lastSimData.getIndicatedSpeedKts(),
                      simData.getIndicatedSpeedKts(),
                      deltas.speedDelta) ||
       almostNotEqual(lastSimData.getPosition().getAltitude(),
                      simData.getPosition().getAltitude(),
                      deltas.altitudeDelta))
    {
      lastSimData = simulatorData;

      // Calculate the amount that has to be substracted from each side of the rectangle
      float boxFactor = (100.f - OptionData::instance().getSimUpdateBox()) / 100.f / 2.f;
      int dx = static_cast<int>(width() * boxFactor);
      int dy = static_cast<int>(height() * boxFactor);

      QRect widgetRect = geometry();
      widgetRect.adjust(dx, dy, -dx, -dy);

      if(!widgetRect.contains(curPos) && mainWindow->getUi()->actionMapAircraftCenter->isChecked())
        centerOn(simData.getPosition().getLonX(), simData.getPosition().getLatY(), false);
      else
        update();
    }
  }
  else if(paintLayer->getShownMapObjects() & maptypes::AIRCRAFT_TRACK)
  {
    // No aircraft but track - update track only
    if(!lastSimData.getPosition().isValid() || diff.manhattanLength() > 4)
    {
      lastSimData = simulatorData;
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

void MapWidget::disconnectedFromSimulator()
{
  // Clear all data on disconnect
  simData = atools::fs::sc::SimConnectData();
  aircraftTrack.clear();
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

const maptypes::MapSearchResult& MapWidget::getSearchHighlights() const
{
  return screenIndex->getSearchHighlights();
}

void MapWidget::changeSearchHighlights(const maptypes::MapSearchResult& positions)
{
  screenIndex->getSearchHighlights() = positions;
  update();
}

/* Update the flight plan from a drag and drop result. Show a menu if multiple objects are
 * found at the button release position. */
void MapWidget::updateRouteFromDrag(QPoint newPoint, MouseStates state, int leg, int point)
{
  qDebug() << "End route drag" << newPoint << "state" << state << "leg" << leg << "point" << point;

  // Get all objects where the mouse button was released
  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(newPoint.x(), newPoint.y(), screenSearchDistance, result);

  CoordinateConverter conv(viewport());

  // Get objects from cache - already present objects will be skipped
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(), false,
                              paintLayer->getShownMapObjects() &
                              (maptypes::AIRPORT_ALL | maptypes::VOR | maptypes::NDB | maptypes::WAYPOINT),
                              newPoint.x(), newPoint.y(), screenSearchDistance, result);

  int totalSize = result.airports.size() + result.vors.size() + result.ndbs.size() + result.waypoints.size();

  int id = -1;
  maptypes::MapObjectTypes type = maptypes::NONE;
  atools::geo::Pos pos = atools::geo::EMPTY_POS;
  if(totalSize == 0)
  {
    // Nothing at the position - add userpoint
    qDebug() << "add userpoint";
    type = maptypes::USER;
  }
  else if(totalSize == 1)
  {
    // Only one entry at the position - add single navaid without menu
    qDebug() << "add navaid";

    if(!result.airports.isEmpty())
    {
      id = result.airports.first().id;
      type = maptypes::AIRPORT;
    }
    else if(!result.vors.isEmpty())
    {
      id = result.vors.first().id;
      type = maptypes::VOR;
    }
    else if(!result.ndbs.isEmpty())
    {
      id = result.ndbs.first().id;
      type = maptypes::NDB;
    }
    else if(!result.waypoints.isEmpty())
    {
      id = result.waypoints.first().id;
      type = maptypes::WAYPOINT;
    }
  }
  else
  {
    // Multiple entries - build a menu with icons
    // Add id and type to actions
    const int ICON_SIZE = 20;
    qDebug() << "add navaids" << totalSize;
    QMenu menu;
    QString menuPrefix(tr("Add ")), menuSuffix(tr(" to Flight Plan"));
    SymbolPainter symbolPainter;

    for(const maptypes::MapAirport& obj : result.airports)
    {
      QAction *action = new QAction(symbolPainter.createAirportIcon(obj, ICON_SIZE),
                                    menuPrefix + maptypes::airportText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::AIRPORT}));
      menu.addAction(action);
    }

    if(!result.airports.isEmpty() || !result.vors.isEmpty() || !result.ndbs.isEmpty() ||
       !result.waypoints.isEmpty())
      // There will be more entries - add a separator
      menu.addSeparator();

    for(const maptypes::MapVor& obj : result.vors)
    {
      QAction *action = new QAction(symbolPainter.createVorIcon(obj, ICON_SIZE),
                                    menuPrefix + maptypes::vorText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::VOR}));
      menu.addAction(action);
    }
    for(const maptypes::MapNdb& obj : result.ndbs)
    {
      QAction *action = new QAction(symbolPainter.createNdbIcon(ICON_SIZE),
                                    menuPrefix + maptypes::ndbText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::NDB}));
      menu.addAction(action);
    }
    for(const maptypes::MapWaypoint& obj : result.waypoints)
    {
      QAction *action = new QAction(symbolPainter.createWaypointIcon(ICON_SIZE),
                                    menuPrefix + maptypes::waypointText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::WAYPOINT}));
      menu.addAction(action);
    }

    // Always present - userpoint
    menu.addSeparator();
    {
      QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                    menuPrefix + "Userpoint" + menuSuffix, this);
      action->setData(QVariantList({-1, maptypes::USER}));
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
      id = data.at(0).toInt();
      type = maptypes::MapObjectTypes(data.at(1).toInt());
    }
  }

  if(type == maptypes::USER)
    // Get position for new user point from from screen
    pos = conv.sToW(newPoint.x(), newPoint.y());

  if((id != -1 && type != maptypes::NONE) || type == maptypes::USER)
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

  QPoint point;
  if(event->reason() == QContextMenuEvent::Keyboard)
    point = mapFromGlobal(QCursor::pos());
  else
    point = event->pos();

  if(mouseState != NONE)
    return;

  hideTooltip();

  Ui::MainWindow *ui = mainWindow->getUi();

  // Texts with % will be replaced save them and let the ActionTextSaver restore them on return
  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch, ui->actionRouteAdd,
                                          ui->actionMapShowInformation,
                                          ui->actionRouteDeleteWaypoint, ui->actionRouteAirportStart,
                                          ui->actionRouteAirportDest});
  Q_UNUSED(textSaver);

  // Build menu - add actions
  QMenu menu;
  menu.addAction(ui->actionMapShowInformation);
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

  menu.addAction(ui->actionRouteAdd);
  menu.addAction(ui->actionRouteDeleteWaypoint);
  menu.addSeparator();

  menu.addAction(ui->actionShowInSearch);
  menu.addSeparator();

  menu.addAction(ui->actionMapSetMark);
  menu.addAction(ui->actionMapSetHome);

  int distMarkerIndex = screenIndex->getNearestDistanceMarkIndex(point.x(), point.y(), screenSearchDistance);
  int rangeMarkerIndex = screenIndex->getNearestRangeMarkIndex(point.x(), point.y(), screenSearchDistance);

  qreal lon, lat;
  // Cursor can be outside or map region
  bool visibleOnMap = geoCoordinates(point.x(), point.y(), lon, lat);
  atools::geo::Pos pos(lon, lat);

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
  ui->actionMapNavaidRange->setEnabled(false);
  ui->actionShowInSearch->setEnabled(false);
  ui->actionRouteAdd->setEnabled(visibleOnMap);
  ui->actionRouteAirportStart->setEnabled(false);
  ui->actionRouteAirportDest->setEnabled(false);
  ui->actionRouteDeleteWaypoint->setEnabled(false);

  // Get objects near position
  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDistance, result);

  maptypes::MapAirport *airport = nullptr;
  maptypes::MapVor *vor = nullptr;
  maptypes::MapNdb *ndb = nullptr;
  maptypes::MapWaypoint *waypoint = nullptr;
  maptypes::MapUserpoint *userpoint = nullptr;
  maptypes::MapParking *parking = nullptr;

  // Get only one object of each type
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

  // Collect information from the search result - build text only for one object
  QString mapObjectText;
  if(airport != nullptr)
    mapObjectText = maptypes::airportText(*airport);
  else if(parking != nullptr)
    mapObjectText = maptypes::parkingName(parking->name) + " " + QLocale().toString(parking->number);
  else if(vor != nullptr)
    mapObjectText = maptypes::vorText(*vor);
  else if(ndb != nullptr)
    mapObjectText = maptypes::ndbText(*ndb);
  else if(waypoint != nullptr)
    mapObjectText = maptypes::waypointText(*waypoint);
  else if(userpoint != nullptr)
    mapObjectText = maptypes::userpointText(*userpoint);

  // Collect information from the search result - only VOR and NDB can have range rings
  QString navRingText;
  if(vor != nullptr)
    navRingText = maptypes::vorText(*vor);
  else if(ndb != nullptr)
    navRingText = maptypes::ndbText(*ndb);

  // Build "delete from flight plan" text
  int deleteRouteIndex = -1;
  maptypes::MapObjectTypes deleteType = maptypes::NONE;
  QString deleteText;
  if(airport != nullptr && airport->routeIndex != -1)
  {
    deleteText = maptypes::airportText(*airport);
    deleteRouteIndex = airport->routeIndex;
    deleteType = maptypes::AIRPORT;
  }
  else if(vor != nullptr && vor->routeIndex != -1)
  {
    deleteText = maptypes::vorText(*vor);
    deleteRouteIndex = vor->routeIndex;
    deleteType = maptypes::VOR;
  }
  else if(ndb != nullptr && ndb->routeIndex != -1)
  {
    deleteText = maptypes::ndbText(*ndb);
    deleteRouteIndex = ndb->routeIndex;
    deleteType = maptypes::NDB;
  }
  else if(waypoint != nullptr && waypoint->routeIndex != -1)
  {
    deleteText = maptypes::waypointText(*waypoint);
    deleteRouteIndex = waypoint->routeIndex;
    deleteType = maptypes::WAYPOINT;
  }
  else if(userpoint != nullptr && userpoint->routeIndex != -1)
  {
    deleteText = maptypes::userpointText(*userpoint);
    deleteRouteIndex = userpoint->routeIndex;
    deleteType = maptypes::USER;
  }

  // Update "set airport as start/dest"
  if(airport != nullptr || parking != nullptr)
  {
    QString airportText;

    if(parking != nullptr)
    {
      // Get airport for parking
      maptypes::MapAirport parkAp;
      mapQuery->getAirportById(parkAp, parking->airportId);
      airportText = maptypes::airportText(parkAp) + " / ";
    }

    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(airportText + mapObjectText));

    if(airport != nullptr)
    {
      ui->actionRouteAirportDest->setEnabled(true);
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(mapObjectText));
    }
    else
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }
  else
  {
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // Update "show in search" and "add to route" and "show information"
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(mapObjectText));

    ui->actionRouteAdd->setEnabled(true);
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg(mapObjectText));

    ui->actionMapShowInformation->setEnabled(true);
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(mapObjectText));
  }
  else
  {
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg(tr("Position")));
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(QString()));
  }

  // Update "delete in route"
  if(deleteRouteIndex != -1)
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(deleteText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(QString()));

  // Update "show range rings for Navaid"
  if(vor != nullptr || ndb != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(navRingText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && !mapObjectText.isEmpty())
  {
    // Set text to measure "from airport" etc.
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(mapObjectText));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(mapObjectText));
  }
  else
  {
    // Noting found at cursor - use "measure from here"
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(tr("here")));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(tr("here")));
  }

  // Show the menu ------------------------------------------------
  QAction *action = menu.exec(QCursor::pos());

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
        emit showInSearch(maptypes::AIRPORT, airport->ident, QString(), QString());
      }
      else if(vor != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(maptypes::VOR, vor->ident, vor->region, vor->airportIdent);
      }
      else if(ndb != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(maptypes::NDB, ndb->ident, ndb->region, ndb->airportIdent);
      }
      else if(waypoint != nullptr)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit showInSearch(maptypes::WAYPOINT, waypoint->ident, waypoint->region, waypoint->airportIdent);
      }
    }
    else if(action == ui->actionMapNavaidRange)
    {
      if(vor != nullptr)
        addNavRangeRing(vor->position, maptypes::VOR, vor->ident, vor->frequency, vor->range);
      else if(ndb != nullptr)
        addNavRangeRing(ndb->position, maptypes::NDB, ndb->ident, ndb->frequency, ndb->range);
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
      maptypes::DistanceMarker dm;
      dm.isRhumbLine = action == ui->actionMapMeasureRhumbDistance;
      dm.to = pos;

      // Build distance line depending on selected airport or navaid (color, magvar, etc.)
      if(vor != nullptr)
      {
        dm.text = vor->ident + " " + QLocale().toString(vor->frequency / 1000., 'f', 2);
        dm.from = vor->position;
        dm.magvar = vor->magvar;
        dm.hasMagvar = true;
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
      else if(airport != nullptr)
      {
        dm.text = airport->name + " (" + airport->ident + ")";
        dm.from = airport->position;
        dm.magvar = airport->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::colorForAirport(*airport);
      }
      else
      {
        dm.hasMagvar = false;
        dm.from = pos;
        dm.color = dm.isRhumbLine ? mapcolors::distanceRhumbColor : mapcolors::distanceColor;
      }

      screenIndex->getDistanceMarks().append(dm);

      // Start mouse dragging and disable context menu so we can catch the right button click as cancel
      mouseState = DRAG_DISTANCE;
      setContextMenuPolicy(Qt::PreventContextMenu);
      currentDistanceMarkerIndex = screenIndex->getDistanceMarks().size() - 1;
    }
    else if(action == ui->actionRouteDeleteWaypoint)
      emit routeDelete(deleteRouteIndex);
    else if(action == ui->actionRouteAdd || action == ui->actionRouteAirportStart ||
            action == ui->actionRouteAirportDest || action == ui->actionMapShowInformation)
    {
      atools::geo::Pos position = pos;
      maptypes::MapObjectTypes type;

      int id = -1;
      if(airport != nullptr)
      {
        id = airport->id;
        type = maptypes::AIRPORT;
      }
      else if(parking != nullptr)
      {
        id = parking->id;
        type = maptypes::PARKING;
      }
      else if(vor != nullptr)
      {
        id = vor->id;
        type = maptypes::VOR;
      }
      else if(ndb != nullptr)
      {
        id = ndb->id;
        type = maptypes::NDB;
      }
      else if(waypoint != nullptr)
      {
        id = waypoint->id;
        type = maptypes::WAYPOINT;
      }
      else
      {
        if(userpoint != nullptr)
          id = userpoint->id;
        type = maptypes::USER;
        position = pos;
      }

      if(action == ui->actionRouteAirportStart && parking != nullptr)
        emit routeSetParkingStart(*parking);
      else if(action == ui->actionRouteAdd)
      {
        if(parking != nullptr)
        {
          // Adjust values in case user selected "add" on a parking position
          type = maptypes::USER;
          id = -1;
        }
        emit routeAdd(id, position, type, -1 /* leg index */);
      }
      else if(action == ui->actionRouteAirportStart)
        emit routeSetStart(*airport);
      else if(action == ui->actionRouteAirportDest)
        emit routeSetDest(*airport);
      else if(action == ui->actionMapShowInformation)
        emit showInformation(result);
    }
  }
}

void MapWidget::addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type,
                                const QString& ident, int frequency, int range)
{
  maptypes::RangeMarker ring;
  ring.type = type;
  ring.center = pos;

  if(type == maptypes::VOR)
    ring.text = ident + " " + QLocale().toString(frequency / 1000., 'f', 2);
  else if(type == maptypes::NDB)
    ring.text = ident + " " + QLocale().toString(frequency / 100., 'f', 2);

  ring.ranges.append(range);
  screenIndex->getRangeMarks().append(ring);
  qDebug() << "navaid range" << ring.center;
  update();
  mainWindow->setStatusMessage(tr("Added range rings for %1.").arg(ident));
}

void MapWidget::addRangeRing(const atools::geo::Pos& pos)
{
  maptypes::RangeMarker rings;
  rings.type = maptypes::NONE;
  rings.center = pos;
  rings.ranges = OptionData::instance().getMapRangeRings();
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

  if(!offline)
    update();
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
    {
      // Remove control modifer to disable Marble rectangle dragging
      mouseEvent->setModifiers(mouseEvent->modifiers() & ~Qt::ControlModifier);
    }
  }

  if(e->type() == QEvent::MouseMove && mouseState != NONE)
  {
    // Do not allow mouse scrolling during drag actions
    e->accept();
    event(e);

    // Do not process further
    return true;
  }

  // Pass to base class and keep on processing
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

  if(mouseState & DRAG_DISTANCE || mouseState & DRAG_CHANGE_DISTANCE)
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
        screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = atools::geo::Pos(lon, lat);
    }

    // Force fast updates while dragging
    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState & DRAG_ROUTE_LEG || mouseState & DRAG_ROUTE_POINT)
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
  else if(mouseState == NONE)
  {
    if(event->buttons() == Qt::NoButton)
    {
      // No dragging going on now - update cursor over flight plan legs or markers
      const RouteMapObjectList& rmos = mainWindow->getRouteController()->getRouteMapObjects();

      Qt::CursorShape cursorShape = Qt::ArrowCursor;
      bool routeEditMode = mainWindow->getUi()->actionRouteEditMode->isChecked();

      if(routeEditMode &&
         screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(), 5) != -1 &&
         rmos.size() > 1)
        // Change cursor at one route point
        cursorShape = Qt::CrossCursor;
      else if(routeEditMode &&
              screenIndex->getNearestRouteLegIndex(event->pos().x(), event->pos().y(), 5) != -1 &&
              rmos.size() > 1)
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
  if(mouseState == DRAG_DISTANCE || mouseState == DRAG_CHANGE_DISTANCE ||
     mouseState == DRAG_ROUTE_POINT || mouseState == DRAG_ROUTE_LEG)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    if(event->button() == Qt::LeftButton)
      // Done with any dragging
      mouseState |= DRAG_POST;
    else if(event->button() == Qt::RightButton)
      // Cancel any dragging
      mouseState |= DRAG_POST_CANCEL;
  }
  else if(mouseState == NONE && event->buttons() & Qt::RightButton)
    // First right click after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  hideTooltip();

  if(mouseState & DRAG_ROUTE_POINT || mouseState & DRAG_ROUTE_LEG)
  {
    // End route point dragging
    if(mouseState & DRAG_POST)
    {
      // Ending route dragging - update route
      qreal lon, lat;
      bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      if(visible)
        updateRouteFromDrag(routeDragCur, mouseState, routeDragLeg, routeDragPoint);
    }

    // End all dragging
    mouseState = NONE;
    routeDragCur = QPoint();
    routeDragFrom = atools::geo::EMPTY_POS;
    routeDragTo = atools::geo::EMPTY_POS;
    routeDragPoint = -1;
    routeDragLeg = -1;
    setViewContext(Marble::Still);
    update();
  }
  else if(mouseState & DRAG_DISTANCE || mouseState & DRAG_CHANGE_DISTANCE)
  {
    // End distance marker dragging
    if(!screenIndex->getDistanceMarks().isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(mouseState & DRAG_POST)
      {
        qreal lon, lat;
        bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
        if(visible)
          // Update distance measurment line
          screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = atools::geo::Pos(lon, lat);
      }
      else if(mouseState & DRAG_POST_CANCEL)
      {
        if(mouseState & DRAG_DISTANCE)
          // Remove new one
          screenIndex->getDistanceMarks().removeAt(currentDistanceMarkerIndex);
        else if(mouseState & DRAG_CHANGE_DISTANCE)
          // Replace modified one with backup
          screenIndex->getDistanceMarks()[currentDistanceMarkerIndex] = distanceMarkerBackup;
        currentDistanceMarkerIndex = -1;
      }
    }
    else
    {
      if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }

    mouseState = NONE;
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
      mouseState = DRAG_CHANGE_DISTANCE;
      distanceMarkerBackup = screenIndex->getDistanceMarks().at(currentDistanceMarkerIndex);
      setContextMenuPolicy(Qt::PreventContextMenu);
    }
    else
    {
      if(mainWindow->getUi()->actionRouteEditMode->isChecked())
      {
        const RouteMapObjectList& rmos = mainWindow->getRouteController()->getRouteMapObjects();

        if(rmos.size() > 1)
        {
          int routePoint = screenIndex->getNearestRoutePointIndex(event->pos().x(),
                                                                  event->pos().y(), screenSearchDistance);
          if(routePoint != -1)
          {
            routeDragPoint = routePoint;
            qDebug() << "route point" << routePoint;

            // Found a leg - start dragging
            mouseState = DRAG_ROUTE_POINT;

            routeDragCur = QPoint(event->pos().x(), event->pos().y());

            if(routePoint > 0)
              routeDragFrom = rmos.at(routePoint - 1).getPosition();
            else
              routeDragFrom = atools::geo::EMPTY_POS;

            if(routePoint < rmos.size() - 1)
              routeDragTo = rmos.at(routePoint + 1).getPosition();
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
              mouseState = DRAG_ROUTE_LEG;

              routeDragCur = QPoint(event->pos().x(), event->pos().y());

              routeDragFrom = rmos.at(routeLeg).getPosition();
              routeDragTo = rmos.at(routeLeg + 1).getPosition();
              setContextMenuPolicy(Qt::PreventContextMenu);
            }
          }
        }
      }

      if(mouseState == NONE)
      {
        qDebug() << "Single click info";
        handleInfoClick(event->pos());
      }
    }
  }
  mouseMoved = QPoint();
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << "mouseDoubleClickEvent";

  maptypes::MapSearchResult mapSearchResult;
  screenIndex->getAllNearest(event->pos().x(), event->pos().y(), screenSearchDistance, mapSearchResult);

  if(!mapSearchResult.airports.isEmpty())
  {
    if(mapSearchResult.airports.at(0).bounding.isPoint())
      // Some airports just indicate a position and have no bounding rectangle
      showPos(mapSearchResult.airports.at(0).bounding.getTopLeft());
    else
      showRect(mapSearchResult.airports.at(0).bounding);
    mainWindow->setStatusMessage(QString(tr("Showing airport on map.")));
  }
  else
  {
    if(!mapSearchResult.vors.isEmpty())
      showPos(mapSearchResult.vors.at(0).position);
    else if(!mapSearchResult.ndbs.isEmpty())
      showPos(mapSearchResult.ndbs.at(0).position);
    else if(!mapSearchResult.waypoints.isEmpty())
      showPos(mapSearchResult.waypoints.at(0).position);
    else if(!mapSearchResult.userPoints.isEmpty())
      showPos(mapSearchResult.userPoints.at(0).position);
    mainWindow->setStatusMessage(QString(tr("Showing navaid on map.")));
  }
}

const RouteMapObjectList& MapWidget::getRouteHighlights() const
{
  return screenIndex->getRouteHighlights();
}

const QList<maptypes::RangeMarker>& MapWidget::getRangeRings() const
{
  return screenIndex->getRangeMarks();
}

const QList<maptypes::DistanceMarker>& MapWidget::getDistanceMarkers() const
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
  if(databaseLoadStatus)
    return;

  // Build a new tooltip HTML for weather changes
  QString text = mapTooltip->buildTooltip(mapSearchResultTooltip,
                                          mainWindow->getRouteController()->getRouteMapObjects(),
                                          paintLayer->getMapLayer()->isAirportDiagram());

  if(!text.isEmpty() && !tooltipPos.isNull())
    QToolTip::showText(tooltipPos, text, nullptr, QRect(), 3600 * 1000);
  else
    hideTooltip();
}

void MapWidget::deleteAircraftTrack()
{
  aircraftTrack.clear();
  emit updateActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Aircraft track removed from map.")));
}

bool MapWidget::event(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    // Load tooltip data into mapSearchResultTooltip
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    mapSearchResultTooltip = maptypes::MapSearchResult();
    screenIndex->getAllNearest(helpEvent->pos().x(), helpEvent->pos().y(), screenSearchDistanceTooltip,
                               mapSearchResultTooltip);
    tooltipPos = helpEvent->globalPos();

    // Build HTML
    updateTooltip();
    event->accept();
    return true;
  }

  return QWidget::event(event);
}

/* Update the visible objects indication in the status bar. */
void MapWidget::updateVisibleObjectsStatusBar()
{
  const MapLayer *layer = paintLayer->getMapLayer();

  if(layer != nullptr)
  {
    maptypes::MapObjectTypes shown = paintLayer->getShownMapObjects();

    QStringList visible;
    atools::util::HtmlBuilder visibleTooltip(false);
    visibleTooltip.b(tr("Currently shown on map:"));
    visibleTooltip.table();

    if(layer->isAirport() && shown & maptypes::AIRPORT_ALL)
    {
      QString ap(tr("AP")), apTooltip(tr("Airports"));
      if(layer->getDataSource() == layer::ALL)
      {
        if(layer->getMinRunwayLength() > 0)
        {
          ap.append(">" + QLocale().toString(layer->getMinRunwayLength() / 100));
          apTooltip.append(tr(" with runway length > %1 ft").
                           arg(QLocale().toString(layer->getMinRunwayLength())));
        }
      }
      else if(layer->getDataSource() == layer::MEDIUM)
      {
        ap.append(">40");
        apTooltip.append(tr(" with runway length > %1 ft").arg(QLocale().toString(4000)));
      }
      else if(layer->getDataSource() == layer::LARGE)
      {
        ap.append(">80,H");
        apTooltip.append(tr(" with runway length > %1 ft and hard runways").arg(QLocale().toString(8000)));
      }

      visible.append(ap);
      visibleTooltip.tr().td(apTooltip).trEnd();
    }

    if(layer->isVor() && shown & maptypes::VOR)
    {
      visible.append(tr("VOR"));
      visibleTooltip.tr().td(tr("VOR")).trEnd();
    }
    if(layer->isNdb() && shown & maptypes::NDB)
    {
      visible.append(tr("NDB"));
      visibleTooltip.tr().td(tr("NDB")).trEnd();
    }
    if(layer->isIls() && shown & maptypes::ILS)
    {
      visible.append(tr("ILS"));
      visibleTooltip.tr().td(tr("ILS")).trEnd();
    }
    if(layer->isWaypoint() && shown & maptypes::WAYPOINT)
    {
      visible.append(tr("W"));
      visibleTooltip.tr().td(tr("Waypoints")).trEnd();
    }
    if(layer->isAirway() && shown & maptypes::AIRWAYJ)
    {
      visible.append(tr("J"));
      visibleTooltip.tr().td(tr("Jet Airways")).trEnd();
    }
    if(layer->isAirway() && shown & maptypes::AIRWAYV)
    {
      visible.append(tr("V"));
      visibleTooltip.tr().td(tr("Victor Airways")).trEnd();
    }
    visibleTooltip.tableEnd();

    // Update the statusbar label text and tooltip of the label
    mainWindow->setMapObjectsShownMessageText(visible.join(","), visibleTooltip.getHtml());
  }
}

void MapWidget::paintEvent(QPaintEvent *paintEvent)
{
  bool changed = false;
  const GeoDataLatLonAltBox visibleLatLonAltBox = viewport()->viewLatLonAltBox();

  if(viewContext() == Marble::Still &&
     (zoom() != currentZoom || visibleLatLonAltBox != currentViewBoundingBox))
  {
    // This paint event has changed zoom or the visible bounding box
    currentZoom = zoom();
    currentViewBoundingBox = visibleLatLonAltBox;

    // qDebug() << "paintEvent map view has changed zoom" << currentZoom
    // << "distance" << distance() << " (" << atools::geo::meterToNm(distance() * 1000.) << " km)";

    if(!changedByHistory)
      // Not changed by next/last in history
      history.addEntry(atools::geo::Pos(centerLongitude(), centerLatitude()), distance());

    changedByHistory = false;
    changed = true;
  }

  MarbleWidget::paintEvent(paintEvent);

  if(changed)
  {
    // Major change - update index and visible objects
    updateVisibleObjectsStatusBar();
    screenIndex->updateRouteScreenGeometry();
    screenIndex->updateAirwayScreenGeometry(currentViewBoundingBox);
  }
}

void MapWidget::handleInfoClick(QPoint pos)
{
  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(pos.x(), pos.y(), screenSearchDistance, result);
  emit showInformation(result);
}

bool MapWidget::loadKml(const QString& filename, bool center)
{
  bool retval = false;

  if(QFile::exists(filename))
  {
    // Try if it is a zip file first - ignore extension
    QString kmlString;
    atools::zip::ZipReader reader(filename);
    if(reader.exists() && reader.isReadable() &&
       reader.status() == atools::zip::ZipReader::NoError)
    {
      QByteArray filedata = reader.fileData("doc.kml");
      if(!filedata.isEmpty() && reader.status() == atools::zip::ZipReader::NoError)
        kmlString = QString(filedata).trimmed();
    }
    reader.close();

    if(kmlString.isEmpty())
    {
      // Try second if it is a text KML file if nothing was loaded in the last step
      QFile file(filename);
      if(file.open(QFile::ReadOnly | QFile::Text))
        kmlString = QTextStream(&file).readAll().trimmed();
    }

    // Do some rudimentary file content checking
    if(!kmlString.isEmpty() && kmlString.startsWith("<?xml") && kmlString.endsWith("</kml>"))
    {
      if(center && OptionData::instance().getFlags() & opts::GUI_CENTER_KML)
        // add file always centers
        model()->addGeoDataFile(filename);
      else
        // Have to read the data ourselves to avoid centering on startup
        model()->addGeoDataString(kmlString, filename);
      retval = true;
    }

    if(!retval)
      QMessageBox::warning(mainWindow, QApplication::applicationName(),
                           tr("File \"%1\" is malformed or neither KML nor KMZ.").arg(filename));
  }
  else
    QMessageBox::warning(mainWindow, QApplication::applicationName(),
                         tr("File \"%1\" does not exist.").arg(filename));

  return retval;
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
