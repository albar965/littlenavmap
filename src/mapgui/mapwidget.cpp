/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/jumpback.h"
#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "connect/connectclient.h"
#include "fs/gpx/gpxio.h"
#include "fs/gpx/gpxtypes.h"
#include "fs/perf/aircraftperf.h"
#include "fs/sc/simconnectdata.h"
#include "geo/aircrafttrail.h"
#include "geo/calculations.h"
#include "geo/marbleconverter.h"
#include "gui/coordinatedialog.h"
#include "gui/dialog.h"
#include "gui/holddialog.h"
#include "gui/mainwindow.h"
#include "gui/rangemarkerdialog.h"
#include "gui/signalblocker.h"
#include "gui/tools.h"
#include "gui/trafficpatterndialog.h"
#include "gui/widgetstate.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/mapcontextmenu.h"
#include "mapgui/mapdetailhandler.h"
#include "mapgui/maplayersettings.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/mapthemehandler.h"
#include "mapgui/mapthemehandler.h"
#include "mapgui/maptooltip.h"
#include "mapgui/mapvisible.h"
#include "mappainter/mappaintlayer.h"
#include "online/onlinedatacontroller.h"
#include "query/airportquery.h"
#include "query/airspacequeries.h"
#include "query/procedurequery.h"
#include "route/routealtitude.h"
#include "route/routecontroller.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "userdata/userdatacontroller.h"
#include "userdata/userdataicons.h"
#include "weather/windreporter.h"

#include <QContextMenuEvent>
#include <QToolTip>
#include <QClipboard>
#include <QPushButton>
#include <QStringBuilder>

#include <marble/AbstractFloatItem.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/MarbleModel.h>
#include <marble/MapThemeManager.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
using Qt::hex;
using Qt::dec;
using Qt::endl;
#endif

/* Stores delta values depending on fast or slow update. User aircraft is only updated if
 * delta values are exceeded. */
struct SimUpdateDelta
{
  float manhattanLengthDelta;
  float headingDelta;
  float speedDelta;
  float altitudeDelta;
  qint64 timeDeltaMs;
};

Q_DECLARE_TYPEINFO(SimUpdateDelta, Q_PRIMITIVE_TYPE);

const static qreal SIM_UPDATE_CLOSE_KM = 1.;

// Update rates defined by delta values for higher zoom distances
const static QHash<opts::SimUpdateRate, SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
{
  // manhattanLengthDelta; headingDelta; speedDelta; altitudeDelta; timeDeltaMs;
  {
    opts::FAST, {0.5f, 1.f, 1.f, 1.f, 75}
  },
  {
    opts::MEDIUM, {1.f, 1.f, 10.f, 10.f, 250}
  },
  {
    opts::LOW, {2.f, 4.f, 10.f, 100.f, 550}
  }
});

// Update rates defined by delta values for close zoom distance < SIM_UPDATE_CLOSE_KM
// Update more often to avoid jumping on ground
const static QHash<opts::SimUpdateRate, SimUpdateDelta> SIM_UPDATE_DELTA_MAP_CLOSE(
{
  // manhattanLengthDelta; headingDelta; speedDelta; altitudeDelta; timeDeltaMs;
  {
    opts::FAST, {0.1f, 0.5f, 1.f, 1.f, 75}
  },
  {
    opts::MEDIUM, {0.2f, 0.75f, 2.f, 5.f, 100}
  },
  {
    opts::LOW, {1.f, 2.f, 5.f, 10.f, 200}
  }
});

// Do not zoom closer automatically
static float MIN_AUTO_ZOOM_NM = 0.2f;

// Maps minimum zoom in NM by altitude above ground in ft
// Use odd numbers to avoid jumping at typical flown altitude levels
static const QVector<std::pair<float, float> > ALT_TO_MIN_ZOOM_FT_NM =
{
  {100.f, MIN_AUTO_ZOOM_NM}, // 0.2 NM for flying below 55 ft AGL
  {200.f, 0.2f},
  {550.f, 0.4f},
  {1250.f, 0.6f},
  {2250.f, 1.f},
  {3250.f, 2.f},
  {4250.f, 3.f},
  {5250.f, 4.f},
  {7750.f, 5.f},
  {10250.f, 6.f},
  {12250.f, 7.f},
  {14250.f, 8.f},
  {1000000.f, 10.f} // Use this for all above
};

// Keep aircraft and next waypoint centered within this margins
const int PLAN_SIM_UPDATE_BOX = 85;

// Get elevation when mouse is still
const int ALTITUDE_UPDATE_TIMEOUT_MS = 200;

// Delay recognition to avoid detection of bumps
const int LANDING_TIMEOUT_MS = 4000;
const int TAKEOFF_TIMEOUT_MS = 2000;
const int FUEL_ON_OFF_TIMEOUT_MS = 1000;

/* Update rate on tooltip for bearing display */
const int MAX_SIM_UPDATE_TOOLTIP_MS = 500;

/* Disable center waypoint and aircraft if distance to flight plan is larger */
const float MAX_FLIGHTPLAN_DIST_FOR_CENTER_NM = 50.f;

/* Default zoom distance if start position was not set (usually first start after installation */
const double DEFAULT_MAP_DISTANCE_KM = 7000.;

const double MAP_ZOOM_OUT_LIMIT_KM = 10000.;

using atools::geo::Pos;

MapWidget::MapWidget(MainWindow *parent)
  : MapPaintWidget(parent, QueryManager::instance()->getQueriesGui(), true /* visible */, false /* web */), mainWindow(parent)
{
  takeoffLandingLastAircraft = new atools::fs::sc::SimConnectUserAircraft;
  mapResultTooltip = new map::MapResult;
  mapResultTooltipLast = new map::MapResult;
  mapResultInfoClick = new map::MapResult;
  distanceMarkerBackup = new map::DistanceMarker;
  userpointDrag = new map::MapUserpoint;

  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  setMinimumSize(QSize(50, 50));

  // Event filter needed to disable some unwanted Marble default functionality
  installEventFilter(this);

  // Disable all unwanted bloated useless Marble default popups on mouse click
  Marble::MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);

  // Avoid stuttering movements - jump directly to positions
  input->setInertialEarthRotationEnabled(false);

  mapTooltip = new MapTooltip(mainWindow);

  // Also set in optionsChanged()
  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();

  elevationDisplayTimer.setInterval(ALTITUDE_UPDATE_TIMEOUT_MS);
  elevationDisplayTimer.setSingleShot(true);
  connect(&elevationDisplayTimer, &QTimer::timeout, this, &MapWidget::elevationDisplayTimerTimeout);

  jumpBack = new JumpBack(this, atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_MAP_JUMP_BACK_DEBUG, false).toBool());
  connect(jumpBack, &JumpBack::jumpBack, this, &MapWidget::jumpBackToAircraftTimeout);

  takeoffLandingTimer.setSingleShot(true);
  connect(&takeoffLandingTimer, &QTimer::timeout, this, &MapWidget::takeoffLandingTimeout);

  fuelOnOffTimer.setSingleShot(true);
  connect(&fuelOnOffTimer, &QTimer::timeout, this, &MapWidget::fuelOnOffTimeout);

  resetPaintForDragTimer.setSingleShot(true);
  resetPaintForDragTimer.setInterval(200);
  connect(&resetPaintForDragTimer, &QTimer::timeout, this, &MapWidget::resetPaintForDrag);

  // Fill overlay / action map ============================
  // "Compass" id "compass"
  // "License" id "license"
  // "Scale Bar" id "scalebar"
  // "Navigation" id "navigation"
  // "Overview Map" id "overviewmap"
  mapOverlays.insert("compass", mainWindow->getUi()->actionMapOverlayCompass);
  mapOverlays.insert("scalebar", mainWindow->getUi()->actionMapOverlayScalebar);
  mapOverlays.insert("navigation", mainWindow->getUi()->actionMapOverlayNavigation);
  mapOverlays.insert("overviewmap", mainWindow->getUi()->actionMapOverlayOverview);

  mapVisible = new MapVisible(paintLayer);

  // Need to limit commonly used shortcuts like home, up, down, etc. to the map window =========
  Ui::MainWindow *ui = NavApp::getMainUi();
  addActions({ui->actionMapShowHome, ui->actionMapAircraftCenterNow, ui->actionRouteCenter, ui->actionMapShowMark,
              ui->actionMapCopyCoordinates, ui->actionMapBack, ui->actionMapNext, ui->actionMapDetailsMore,
              ui->actionMapDetailsLess, ui->actionMapDetailsTextMore, ui->actionMapDetailsTextLess});

  ui->actionMapShowHome->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapAircraftCenterNow->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteCenter->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapShowMark->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  // ui->actionMapJumpCoordinates->setShortcutContext(Qt::WidgetShortcut); // Globally available
  ui->actionMapCopyCoordinates->setShortcutContext(Qt::WidgetShortcut); // Only map display window
  ui->actionMapBack->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapNext->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapDetailsMore->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapDetailsLess->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapDetailsTextMore->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionMapDetailsTextLess->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

MapWidget::~MapWidget()
{
  resetPaintForDragTimer.stop();
  elevationDisplayTimer.stop();
  takeoffLandingTimer.stop();
  fuelOnOffTimer.stop();

  qDebug() << Q_FUNC_INFO << "removeEventFilter";
  removeEventFilter(this);

  ATOOLS_DELETE_LOG(jumpBack);
  ATOOLS_DELETE_LOG(mapTooltip);
  ATOOLS_DELETE_LOG(mapVisible);
  ATOOLS_DELETE_LOG(pushButtonExitFullscreen);
  ATOOLS_DELETE_LOG(takeoffLandingLastAircraft);
  ATOOLS_DELETE_LOG(mapResultTooltip);
  ATOOLS_DELETE_LOG(mapResultTooltipLast);
  ATOOLS_DELETE_LOG(mapResultInfoClick);
  ATOOLS_DELETE_LOG(distanceMarkerBackup);
  ATOOLS_DELETE_LOG(userpointDrag);
}

void MapWidget::addFullScreenExitButton()
{
  removeFullScreenExitButton();

  pushButtonExitFullscreen = new QPushButton(QIcon(":/littlenavmap/resources/icons/fullscreen.svg"),
                                             tr("Exit fullscreen mode"), this);
  pushButtonExitFullscreen->setToolTip(tr("Leave fullscreen mode and restore normal window layout"));
  pushButtonExitFullscreen->setStatusTip(pushButtonExitFullscreen->toolTip());

  // Need to set palette since button inherits an empty one from the mapwidget
  pushButtonExitFullscreen->setPalette(QApplication::palette());
  pushButtonExitFullscreen->setFont(QApplication::font());
  pushButtonExitFullscreen->adjustSize();
  pushButtonExitFullscreen->move(size().width() / 8, 0);
  pushButtonExitFullscreen->show();

  connect(pushButtonExitFullscreen, &QPushButton::clicked, this, &MapWidget::exitFullScreenPressed);
}

void MapWidget::removeFullScreenExitButton()
{
  if(pushButtonExitFullscreen != nullptr)
  {
    disconnect(pushButtonExitFullscreen, &QPushButton::clicked, this, &MapWidget::exitFullScreenPressed);
    ATOOLS_DELETE(pushButtonExitFullscreen);
  }
}

void MapWidget::getUserpointDragPoints(QPoint& cur, QPixmap& pixmap)
{
  cur = userpointDragCur;
  pixmap = userpointDragPixmap;
}

map::MapWeatherSource MapWidget::getMapWeatherSource() const
{
  return paintLayer->getMapWeatherSource();
}

void MapWidget::getRouteDragPoints(atools::geo::LineString& fixedPos, QPoint& cur)
{
  fixedPos = routeDragFixed;
  cur = routeDragCur;
}

void MapWidget::historyNext()
{
  const atools::gui::MapPosHistoryEntry& entry = history.next();
  if(entry.isValid())
  {
    jumpBackToAircraftStart();

    // Do not fix zoom - display as is
    setDistanceToMap(entry.getDistance(), false /* Allow adjust zoom */);
    centerPosOnMap(entry.getPos());
    noStoreInHistory = true;
    mainWindow->setStatusMessage(tr("Map position history next."));
    showAircraft(false);
  }
}

void MapWidget::historyBack()
{
  const atools::gui::MapPosHistoryEntry& entry = history.back();
  if(entry.isValid())
  {
    jumpBackToAircraftStart();

    // Do not fix zoom - display as is
    setDistanceToMap(entry.getDistance(), false /* Allow adjust zoom */);
    centerPosOnMap(entry.getPos());
    noStoreInHistory = true;
    mainWindow->setStatusMessage(tr("Map position history back."));
    showAircraft(false);
  }
}

void MapWidget::handleInfoClick(const QPoint& point)
{
  qDebug() << Q_FUNC_INFO << point;

#ifdef DEBUG_NETWORK_INFORMATION
  qreal lon, lat;
  bool visible = geoCoordinates(point.x(), point.y(), lon, lat);
  if(visible)
    NavApp::getRouteController()->debugNetworkClick(Pos(lon, lat));
#endif

  mapResultInfoClick->clear();

  map::MapObjectQueryTypes queryTypes = map::QUERY_PREVIEW_PROC_POINTS;

  if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    queryTypes |= map::QUERY_PROC_RECOMMENDED;
    queryTypes |= map::QUERY_PROC_POINTS;

    if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS;

    if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
      queryTypes |= map::QUERY_ALTERNATE;
  }

  getScreenIndexConst()->getAllNearest(point, screenSearchDistance, *mapResultInfoClick, queryTypes);

  // Removes the online aircraft from onlineAircraft which also have a simulator shadow in simAircraft
  NavApp::getOnlinedataController()->removeOnlineShadowedAircraft(mapResultInfoClick->onlineAircraft, mapResultInfoClick->aiAircraft);

  // Remove all unwanted features
  optsd::DisplayClickOptions opts = OptionData::instance().getDisplayClickOptions();

  if(!opts.testFlag(optsd::CLICK_AIRCRAFT_USER))
    mapResultInfoClick->userAircraft.clear();

  if(!opts.testFlag(optsd::CLICK_AIRCRAFT_AI))
  {
    mapResultInfoClick->aiAircraft.clear();
    mapResultInfoClick->onlineAircraft.clear();
    mapResultInfoClick->onlineAircraftIds.clear();
  }

  if(!(opts.testFlag(optsd::CLICK_NAVAID)))
  {
    mapResultInfoClick->vors.clear();
    mapResultInfoClick->vorIds.clear();
    mapResultInfoClick->ndbs.clear();
    mapResultInfoClick->ndbIds.clear();
    mapResultInfoClick->holdings.clear();
    mapResultInfoClick->holdingIds.clear();
    mapResultInfoClick->waypoints.clear();
    mapResultInfoClick->waypointIds.clear();
    mapResultInfoClick->ils.clear();
    mapResultInfoClick->airways.clear();
    mapResultInfoClick->userpoints.clear();
    mapResultInfoClick->logbookEntries.clear();
  }

  if(!opts.testFlag(optsd::CLICK_AIRSPACE))
    mapResultInfoClick->airspaces.clear();

  if(opts.testFlag(optsd::CLICK_AIRPORT_PROC) && mapResultInfoClick->hasAirports())
  {
    bool departureFilter, arrivalFilter;
    NavApp::getRouteConst().getAirportProcedureFlags(mapResultInfoClick->airports.constFirst(), -1, departureFilter, arrivalFilter);

    emit showProcedures(mapResultInfoClick->airports.constFirst(), departureFilter, arrivalFilter);
  }

  if(!opts.testFlag(optsd::CLICK_AIRPORT))
  {
    mapResultInfoClick->airports.clear();
    mapResultInfoClick->airportIds.clear();
  }

  qDebug() << Q_FUNC_INFO << "CLICK ========================================";
  qDebug() << Q_FUNC_INFO << *mapResultInfoClick;
  qDebug() << Q_FUNC_INFO << "==============================================";

  emit showInformation(*mapResultInfoClick);
}

void MapWidget::handleRouteClick(const QPoint& point)
{
  qDebug() << Q_FUNC_INFO << point;

  // Check if click enabled and flight plan is visible
  if(OptionData::instance().getDisplayClickOptions().testFlag(optsd::CLICK_FLIGHTPLAN) &&
     getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    Pos pos = CoordinateConverter(viewport()).sToW(point);
    if(pos.isValid())
    {
      // Get all objects near click and remove all having no route index, i.e. are not a part of the route
      map::MapResult result;
      getScreenIndexConst()->getAllNearest(point, screenSearchDistance, result, map::QUERY_NONE /* For double click */);
      result.removeNoRouteIndex();

      // Add only route related objects and sort by distance
      map::MapResultIndex index;
      index.add(result, map::NAV_FLIGHTPLAN).sort(pos);

      if(!index.isEmpty())
        emit showInRoute(map::routeIndex(index.first()));
    }
  }
}

void MapWidget::fuelOnOffTimeout()
{
  if(atools::gui::Application::isShuttingDown())
    return;

  const atools::fs::sc::SimConnectUserAircraft aircraft = getScreenIndexConst()->getLastUserAircraft();

  if(aircraft.hasFuelFlow())
  {
    qDebug() << Q_FUNC_INFO << "Engine start detected" << aircraft.getZuluTime();
    emit aircraftEngineStarted(aircraft);
  }
  else
  {
    qDebug() << Q_FUNC_INFO << "Engine stop detected" << aircraft.getZuluTime();
    emit aircraftEngineStopped(aircraft);
  }
}

void MapWidget::jumpBackToAircraftStart()
{
#ifdef DEBUG_INFORMATION_JUMPBACK
  qDebug() << Q_FUNC_INFO;
#endif

  if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked() && NavApp::isConnectedAndAircraft())
  {
    if(jumpBack->isActive())
      // Simply restart
      jumpBack->restart();
    else
      // Start and save coordinates
      jumpBack->start(isCenterLegAndAircraftActive() ? getCenterPosAndDistance() : getCenterPos() /* saveDistance */);
  }
}

void MapWidget::jumpBackToAircraftCancel()
{
#ifdef DEBUG_INFORMATION_JUMPBACK
  qDebug() << Q_FUNC_INFO;
#endif

  jumpBack->cancel();
}

void MapWidget::jumpBackToAircraftTimeout(const atools::geo::Pos& pos)
{
#ifdef DEBUG_INFORMATION_JUMPBACK
  qDebug() << Q_FUNC_INFO << pos;
#endif

  if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked() && NavApp::isConnectedAndAircraft() &&
     OptionData::instance().getFlags2().testFlag(opts2::ROUTE_NO_FOLLOW_ON_MOVE))
  {

    if(mouseState != mapwin::NONE || viewContext() == Marble::Animation || contextMenuActive || QToolTip::isVisible())
      // Restart/extend as long as menu is active, user is dragging around or tooltip is visible
      jumpBack->restart();
    else
    {
      jumpBack->cancel();

      hideTooltip();
      centerPosOnMap(pos);

      if(pos.getAltitude() > 0.f)
      {
#ifdef DEBUG_INFORMATION_JUMPBACK
        qDebug() << Q_FUNC_INFO << "Adjusting distance" << pos.getAltitude();
#endif

        setDistanceToMap(pos.getAltitude(), false /* allowAdjust */);

        if(isCenterLegAndAircraftActive() && avoidBlurredMap)
        {
          zoomOut(Marble::Instant);
          zoomIn(Marble::Instant);
        }
      }

      mainWindow->setStatusMessage(tr("Jumped back to aircraft."));
    }
  }
  else
    jumpBack->cancel();
}

bool MapWidget::event(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    if(NavApp::isDebugMovingAircraft())
    {
      if(QGuiApplication::queryKeyboardModifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
        return QWidget::event(event);
    }

    if(mouseState == mapwin::NONE)
    {
      const QHelpEvent *helpEvent = dynamic_cast<const QHelpEvent *>(event);
      if(helpEvent != nullptr)
      {
        // Remember position
        tooltipGlobalPos = helpEvent->globalPos();

#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "tooltipGlobalPos" << tooltipGlobalPos;
#endif

        // Update result set - fetch all near cursor
        updateTooltipResult();

        // Build HTML
        showTooltip(false /* update */);
        event->accept();
        return true;
      }
    }
  }

  return QWidget::event(event);
}

void MapWidget::updateTooltipResult()
{
  // Get map objects for tooltip ===========================================================================
  map::MapObjectQueryTypes queryTypes = map::QUERY_MARK | map::QUERY_PREVIEW_PROC_POINTS;

  const OptionData& optiondata = OptionData::instance();

  // Enable features not always shown depending on visiblity
  if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    queryTypes |= map::QUERY_PROC_RECOMMENDED;
    queryTypes |= map::QUERY_PROC_POINTS;

    if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS;

    if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
      queryTypes |= map::QUERY_ALTERNATE;
  }

  if(getShownMapTypes().testFlag(map::AIRCRAFT_TRAIL) && optiondata.getDisplayTooltipOptions().testFlag(optsd::TOOLTIP_AIRCRAFT_TRAIL))
    queryTypes |= map::QUERY_AIRCRAFT_TRAIL;

  if(optiondata.getDisplayTooltipOptions().testFlag(optsd::TOOLTIP_AIRCRAFT_TRAIL) &&
     getShownMapDisplayTypes().testFlag(map::LOGBOOK_TRACK))
    queryTypes |= map::QUERY_AIRCRAFT_TRAIL_LOG;

  // Load tooltip data into mapSearchResultTooltip
  *mapResultTooltip = map::MapResult();
  getScreenIndexConst()->getAllNearest(mapFromGlobal(tooltipGlobalPos), screenSearchDistanceTooltip, *mapResultTooltip, queryTypes);

  NavApp::getOnlinedataController()->removeOnlineShadowedAircraft(mapResultTooltip->onlineAircraft, mapResultTooltip->aiAircraft);
}

void MapWidget::hideTooltip()
{
  // Passing empty string hides tooltip
  // This affects and hides tooltips across the whole application
  QToolTip::showText(tooltipGlobalPos, QString(), this);

  tooltipGlobalPos = QPoint();
}

void MapWidget::handleHistory()
{
  if(!noStoreInHistory)
    // Not changed by next/last in history
    history.addEntry(getCenterPos(), distance());

  noStoreInHistory = false;
}

void MapWidget::updateTooltip()
{
  showTooltip(true /* update */);
}

void MapWidget::showTooltip(bool update)
{
  if(databaseLoadStatus || noRender() || atools::gui::Application::isShuttingDown())
    return;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "tooltipPos" << tooltipGlobalPos << "update" << update << "QToolTip::isVisible()" << QToolTip::isVisible();
#else
  Q_UNUSED(update)
#endif

  // Do not hide or show anything if position is outside map window
  // Position is set by MapWidget::event() on tooltip event
  if(!tooltipGlobalPos.isNull())
  {
    map::AircraftTrailSegment trailSegment;
    Pos pos = getGeoPos(mapFromGlobal(tooltipGlobalPos));
    if(pos.isValid())
    {
      // Build a new tooltip HTML for weather changes or aircraft updates
      QString text;
      if(paintLayer->getMapLayer() != nullptr)
        text = mapTooltip->buildTooltip(*mapResultTooltip, pos, NavApp::getRouteConst(), paintLayer->getMapLayer()->isAirportDiagram());

      if(!text.isEmpty())
        QToolTip::showText(tooltipGlobalPos, text, this);
      else
        // No text - hide
        hideTooltip();
    }
    else
      // Outside of globe
      hideTooltip();
  }
}

/* Stop all line drag and drop if the map loses focus */
void MapWidget::focusOutEvent(QFocusEvent *)
{
  if(atools::gui::Application::isShuttingDown())
    return;

  hideTooltip();

  if(!(mouseState & mapwin::DRAG_POST_MENU))
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }
}

void MapWidget::leaveEvent(QEvent *)
{
  if(atools::gui::Application::isShuttingDown())
    return;

  hideTooltip();
  mainWindow->updateMapPosLabel(Pos(), -1, -1);
}

void MapWidget::keyPressEvent(QKeyEvent *keyEvent)
{
  Qt::Key key = static_cast<Qt::Key>(keyEvent->key());

  // Get only real modifiers
  Qt::KeyboardModifiers modifiers = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);

#ifdef DEBUG_INFORMATION_KEY_INPUT
  qDebug() << Q_FUNC_INFO << "Text" << keyEvent->text()
           << "Scan code 0x" << Qt::hex << keyEvent->nativeScanCode() << Qt::dec << "Key" << "Modifiers" << modifiers;
#endif

  // Does not work for key presses that are consumed by the widget
  if(key == Qt::Key_Escape)
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }
  else if(key == Qt::Key_Menu)
  {
    if(mouseState == mapwin::NONE)
      // First menu key press after dragging - enable context menu again
      setContextMenuPolicy(Qt::DefaultContextMenu);
  }
  else if(key == Qt::Key_Asterisk)
    zoomInOut(true /* in */, true /* smooth */);
  else if(key == Qt::Key_Slash)
    zoomInOut(false /* in */, true /* smooth */);
  else if(keyEvent->modifiers().testFlag(Qt::KeypadModifier))
  {
    // Check shift for smooth zooming for keypad input only
    bool shift = modifiers == Qt::ShiftModifier;
    if(key == Qt::Key_Plus)
      zoomInOut(true /* in */, shift /* smooth */);
    else if(key == Qt::Key_Minus)
      zoomInOut(false /* in */, shift /* smooth */);
  }
  else if(!(modifiers == Qt::ControlModifier)) // Do not use with Ctrl since this is used for map details
  {
    // Do not check shift since different keyboard layouts might affect this
    if(key == Qt::Key_Plus)
      zoomInOut(true /* in */, false /* smooth */);
    else if(key == Qt::Key_Minus)
      zoomInOut(false /* in */, false /* smooth */);
  }
}

bool MapWidget::mousePressCheckModifierActions(QMouseEvent *event)
{
  if(mouseState != mapwin::NONE || event->type() != QEvent::MouseButtonRelease || noRender())
    // Not if dragging or for button release
    return false;

  // Cursor can be outside or map region
  Pos pos = getGeoPos(event->pos());
  if(pos.isValid())
  {
    // Look for navaids or airports nearby click
    map::MapObjectQueryTypes queryTypes = map::QUERY_NONE;

    if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
    {
      queryTypes |= map::QUERY_PROC_RECOMMENDED;

      if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
        queryTypes |= map::QUERY_PROC_MISSED_POINTS;

      if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
        queryTypes |= map::QUERY_ALTERNATE;
    }

    map::MapResult result;
    getScreenIndexConst()->getAllNearest(event->pos(), screenSearchDistance, result, queryTypes);

    // Range rings =======================================================================
    if(event->modifiers() == Qt::ShiftModifier)
    {
      NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_RANGE);
      int id = getScreenIndexConst()->getNearestRangeMarkId(event->pos().x(), event->pos().y(), screenSearchDistance);
      if(id != -1)
        // Remove any ring for Shift+Click into center
        removeRangeMark(id);
      else
      {
        // Add rings for Shift+Click
        if(result.hasVor())
        {
          // Add VOR range
          const map::MapVor& vor = result.vors.constFirst();
          addNavRangeMark(vor.position, map::VOR, vor.ident, vor.getFrequencyOrChannel(), vor.range);
        }
        else if(result.hasNdb())
        {
          // Add NDB range
          const map::MapNdb& ndb = result.ndbs.constFirst();
          addNavRangeMark(ndb.position, map::NDB, ndb.ident, QString::number(ndb.frequency), ndb.range);
        }
        else
          // Add range rings per configuration - attach to waypoint/airport if present
          addRangeMark(pos, result, false /* showDialog */);
      }
      return true;
    }
    // Edit route =======================================================================
    else if(event->modifiers() == (Qt::AltModifier | Qt::ControlModifier) || event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))
    {
      // First check for not editable points if these are procedures which can be removed ======================
      int routeIndex = getScreenIndexConst()->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                                        screenSearchDistance, false /* editableOnly */);

      if(NavApp::getRouteConst().value(routeIndex).isAnyProcedure())
      {
        NavApp::getRouteController()->routeDelete(routeIndex, false /* selectCurrent */);
        return true;
      }

      // No procedure found - check for editable points which can be removed or added ===============
      routeIndex = getScreenIndexConst()->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                                    screenSearchDistance, true /* editableOnly */);

      if(routeIndex != -1)
      {
        // Position is editable - remove
        NavApp::getRouteController()->routeDelete(routeIndex, false /* selectCurrent */);
        return true;
      }

      if(event->modifiers() == (Qt::AltModifier | Qt::ControlModifier))
      {
        // Add to nearest leg of flight plan
        updateRoute(event->pos(), -1, -1, true /* click add */, false /* click append */);
        return true;
      }
      else if(event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))
      {
        // Append to flight plan
        updateRoute(event->pos(), -1, -1, false /* click add */, true /* click append */);
        return true;
      }
    }
    // Edit userpoint =======================================================================
    else if(event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
    {
      // Add or edit userpoint
      if(result.hasUserpoints())
        emit editUserpointFromMap(result);
      else
      {
        if(NavApp::isGlobeOfflineProvider())
          pos.setAltitude(NavApp::getElevationProvider()->getElevationFt(pos));
        emit addUserpointFromMap(result, pos, false /* airportAddon */);
      }
    }
    // Measurement =======================================================================
    else if(event->modifiers() == Qt::ControlModifier || event->modifiers() == Qt::AltModifier)
    {
      NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_DISTANCE);
      int id = getScreenIndexConst()->getNearestDistanceMarkId(event->pos().x(), event->pos().y(), screenSearchDistance);
      if(id != -1)
        // Remove any measurement line for Ctrl+Click or Alt+Click into center
        removeDistanceMark(id);
      else
        // Add measurement line for Ctrl+Click or Alt+Click into center
        addDistanceMarker(pos, result);
      return true;
    }
  }
  return false;
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "pos" << event->pos()
           << "noRender()" << noRender();
#endif

  if(noRender())
  {
    // Zoomed to far out - reset cursor and ignore input
    setCursor(Qt::ArrowCursor);
    return;
  }

  hideTooltip();

  jumpBackToAircraftStart();

  // Avoid repaints
  resetPaintForDragTimer.stop();

  // Remember mouse position to check later if mouse was moved during click (drag map scroll)
  mouseMoved = event->pos();
  if(mouseState & mapwin::DRAG_ALL)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    if(event->button() == Qt::LeftButton)
      // Done with any dragging
      mouseState |= mapwin::DRAG_POST;
    else if(event->button() == Qt::RightButton)
      // Cancel any dragging
      mouseState |= mapwin::DRAG_POST_CANCEL;
  }
  else if(mouseState == mapwin::NONE && event->buttons() & Qt::RightButton)
    // First right click after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);
  else
  {
    if(touchAreaClicked(event))
    {
      // Touch/navigation areas are enabled and cursor is within a touch area
      if(event->button() == Qt::LeftButton && cursor().shape() != Qt::PointingHandCursor)
        setCursor(Qt::PointingHandCursor);
    }
    else if(!isPointVisible(event->pos()))
    {
      // Position is outside visible globe
      if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }
    else
    {
      // No drag and drop mode - use hand to indicate scrolling
      if(event->button() == Qt::LeftButton && cursor().shape() != Qt::OpenHandCursor)
        setCursor(Qt::OpenHandCursor);
    }

    setViewContext(Marble::Still);
  }
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "pos" << event->pos();
#endif

  if(noRender())
  {
    // Zoomed to far out - reset cursor and ignore input
    setCursor(Qt::ArrowCursor);
    return;
  }

  // Take actions (add/remove range rings, measurement)
  if(mousePressCheckModifierActions(event))
    // Event was consumed - do not proceed here
    return;

  hideTooltip();

  jumpBackToAircraftStart();

  // Avoid repaints
  resetPaintForDragTimer.stop();

  // Check if mouse was move for dragging before switching to drag and drop
  int mouseMoveTolerance = 4;
  bool mouseMove = (event->pos() - mouseMoved).manhattanLength() >= mouseMoveTolerance;
  bool touchArea = touchAreaClicked(event);
  MapScreenIndex *screenIndex = getScreenIndex();

  if(mouseState & mapwin::DRAG_ROUTE_POINT || mouseState & mapwin::DRAG_ROUTE_LEG)
  {
    // End route point dragging
    if(mouseState & mapwin::DRAG_POST)
    {
      // Ending route dragging - update route
      if(getGeoPos(event->pos()).isValid())
        updateRoute(routeDragCur, routeDragLeg, routeDragPoint, false /* click add */, false /* click append */);
    }

    // End all dragging
    cancelDragRoute();
    mouseState = mapwin::NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(mouseState.testFlag(mapwin::DRAG_DIST_NEW_END) || mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_START) ||
          mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_END))
  {
    // Either new measurement line which is fixed at origin and dragged at end or one of the ends is dragged
    if(!screenIndex->getDistanceMarks().isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(mouseState.testFlag(mapwin::DRAG_POST))
      {
        Pos pos = getGeoPos(event->pos());
        if(pos.isValid())
        {
          if(mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_START))
          {
            // Update origin of distance measurement line - check if navaid or airport is the new origin and assign label if
            map::MapObjectQueryTypes queryTypes = map::QUERY_NONE;

            if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
            {
              queryTypes |= map::QUERY_PROC_RECOMMENDED;

              if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
                queryTypes |= map::QUERY_PROC_MISSED_POINTS;

              if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
                queryTypes |= map::QUERY_ALTERNATE;
            }
            map::MapResult result;
            screenIndex->getAllNearest(event->pos(), screenSearchDistance, result, map::QUERY_NONE);
            fillDistanceMarker(screenIndex->getDistanceMark(currentDistanceMarkerId), pos, result);
          }
          else if(mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_END) || mouseState.testFlag(mapwin::DRAG_DIST_NEW_END))
            // New or end was moved - update coordinates only
            screenIndex->updateDistanceMarkerToPos(currentDistanceMarkerId, pos);
          currentDistanceMarkerId = -1;
        }
      }
      else if(mouseState & mapwin::DRAG_POST_CANCEL)
        cancelDragDistance();
    }
    else
    {
      if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }

    mouseState = mapwin::NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(mouseState & mapwin::DRAG_USER_POINT)
  {
    // End userpoint dragging
    if(mouseState & mapwin::DRAG_POST)
    {
      // Ending route dragging - update route
      Pos pos = getGeoPos(event->pos());
      if(pos.isValid())
      {
        // Create a copy before cancel
        map::MapUserpoint newUserpoint = *userpointDrag;
        newUserpoint.position = pos;
        emit moveUserpointFromMap(newUserpoint);
      }
    }

    cancelDragUserpoint();

    // End all dragging
    mouseState = mapwin::NONE;
    setViewContext(Marble::Still);
    update();
  }
  else if(touchArea && !mouseMove)
    // Touch/navigation areas are enabled and cursor is within a touch area - scroll, zoom, etc.
    handleTouchAreaClick(event);
  else if(event->button() == Qt::LeftButton && !mouseMove)
  {
    // Start all dragging if left button was clicked and mouse was not moved ==========================
    bool origin; // true if origin was clicked
    currentDistanceMarkerId = screenIndex->getNearestDistanceMarkId(event->pos().x(), event->pos().y(), screenSearchDistance, &origin);
    if(currentDistanceMarkerId != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = origin ? mapwin::DRAG_DIST_CHANGE_START : mapwin::DRAG_DIST_CHANGE_END; // Either change end or origin
      *distanceMarkerBackup = screenIndex->getDistanceMarks().value(currentDistanceMarkerId);
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
            screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(), screenSearchDistance * 4 / 3,
                                                   true /* editableOnly */);
          if(routePoint != -1)
          {
            // Drag a waypoint ==============================================
            routeDragPoint = routePoint;
            qDebug() << "route point" << routePoint;

            // Found a leg - start dragging
            mouseState = mapwin::DRAG_ROUTE_POINT;

            routeDragCur = QPoint(event->pos().x(), event->pos().y());
            routeDragFixed.clear();

            if(routePoint > 0 && route.value(routePoint).isAlternate())
            {
              // Alternate airports are treated as endpoints
              routeDragFixed.append(route.getDestinationAirportLeg().getPosition());
            }
            else if(routePoint == route.getDestinationAirportLegIndex() && route.hasAlternates())
            {
              // Add all lines to alternates as fixed lines if moving destination with alternates
              if(routePoint > 0)
                routeDragFixed.append(route.value(routePoint - 1).getPosition());
              for(int i = route.getAlternateLegsOffset(); i < route.size(); i++)
                routeDragFixed.append(route.value(i).getPosition());
            }
            else
            {
              if(routePoint > 0)
                // First point of route
                routeDragFixed.append(route.value(routePoint - 1).getPosition());

              if(routePoint < route.size() - 1)
                // Last point of plan
                routeDragFixed.append(route.value(routePoint + 1).getPosition());
            }

            setContextMenuPolicy(Qt::PreventContextMenu);
          }
          else
          {
            // Drag a route leg ===================================================
            int routeLeg = screenIndex->getNearestRouteLegIndex(event->pos().x(), event->pos().y(), screenSearchDistance);
            if(routeLeg != -1)
            {
              routeDragLeg = routeLeg;
              qDebug() << "route leg" << routeLeg;
              // Found a leg - start dragging
              mouseState = mapwin::DRAG_ROUTE_LEG;

              routeDragCur = QPoint(event->pos().x(), event->pos().y());

              routeDragFixed.clear();
              routeDragFixed.append(route.value(routeLeg).getPosition());
              routeDragFixed.append(route.value(routeLeg + 1).getPosition());
              setContextMenuPolicy(Qt::PreventContextMenu);
            }
          }
        }
      }

      if(mouseState == mapwin::NONE && !noRender())
      {
        if(cursor().shape() != Qt::ArrowCursor)
          setCursor(Qt::ArrowCursor);
        handleInfoClick(event->pos()); // Show information
        handleRouteClick(event->pos()); // Select in flight plan

        if(OptionData::instance().getMapNavigation() == opts::MAP_NAV_CLICK_CENTER)
        {
          // Center map on click
          Pos pos = getGeoPos(event->pos());
          if(pos.isValid())
            showPos(pos, map::INVALID_DISTANCE_VALUE, true);
        }
      }
    }
  } // else if(event->button() == Qt::LeftButton && mouseMove)
  else
  {
    if(touchArea)
    {
      // Touch/navigation areas are enabled and cursor is within a touch area
      if(cursor().shape() != Qt::PointingHandCursor)
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
      // No drag and drop mode - switch back to arrow after scrolling
      if(cursor().shape() != Qt::ArrowCursor)
        setCursor(Qt::ArrowCursor);
    }
  }

  mouseMoved = QPoint();
  mainWindow->updateMarkActionStates();
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "pos" << event->pos();
#endif

  if(noRender())
  {
    // Zoomed to far out - reset cursor and ignore input
    setCursor(Qt::ArrowCursor);
    return;
  }

  if(mouseState != mapwin::NONE)
    return;

  if(touchAreaClicked(event))
  {
    // Touch/navigation areas are enabled and cursor is within a touch area - scroll, zoom, etc.
    handleTouchAreaClick(event);
    return;
  }

  hideTooltip();

  // Avoid repaints
  resetPaintForDragTimer.stop();

  if(noRender())
    return;

  map::MapResult mapSearchResult;

  if(OptionData::instance().getMapNavigation() == opts::MAP_NAV_CLICK_CENTER &&
     !mapResultInfoClick->isEmpty(map::AIRPORT | map::AIRCRAFT_ALL | map::NAV_ALL | map::ILS | map::USERPOINT | map::USERPOINTROUTE))
  {
    // Do info click and use previous result from single click event if the double click was on a map object
    mapSearchResult = *mapResultInfoClick;
    mapResultInfoClick->clear();
  }
  else
  {
    map::MapObjectQueryTypes queryTypes = map::QUERY_MARK_HOLDINGS | map::QUERY_MARK_PATTERNS | map::QUERY_MARK_RANGE;

    if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
    {
      queryTypes |= map::QUERY_PROC_RECOMMENDED;
      queryTypes |= map::QUERY_PROC_POINTS;

      if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
        queryTypes |= map::QUERY_PROC_MISSED_POINTS;

      if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
        queryTypes |= map::QUERY_ALTERNATE;
    }

    getScreenIndexConst()->getAllNearest(event->pos(), screenSearchDistance, mapSearchResult, queryTypes);
  }

  if(mapSearchResult.userAircraft.isValid())
  {
    showPos(mapSearchResult.userAircraft.getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing user aircraft on map.")));
  }
  else if(!mapSearchResult.aiAircraft.isEmpty())
  {
    showPos(mapSearchResult.aiAircraft.constFirst().getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing AI / multiplayer aircraft on map.")));
  }
  else if(!mapSearchResult.onlineAircraft.isEmpty())
  {
    showPos(mapSearchResult.onlineAircraft.constFirst().getPosition(), 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing online client aircraft on map.")));
  }
  else if(!mapSearchResult.airports.isEmpty())
  {
    showRect(mapSearchResult.airports.constFirst().bounding, true);
    mainWindow->setStatusMessage(QString(tr("Showing airport on map.")));
  }
  else
  {
    if(!mapSearchResult.vors.isEmpty())
      showPos(mapSearchResult.vors.constFirst().position, 0.f, true);
    else if(!mapSearchResult.ndbs.isEmpty())
      showPos(mapSearchResult.ndbs.constFirst().position, 0.f, true);
    else if(!mapSearchResult.waypoints.isEmpty())
      showPos(mapSearchResult.waypoints.constFirst().position, 0.f, true);
    else if(!mapSearchResult.ils.isEmpty())
      showRect(mapSearchResult.ils.constFirst().bounding, true);
    else if(!mapSearchResult.userpointsRoute.isEmpty())
      showPos(mapSearchResult.userpointsRoute.constFirst().position, 0.f, true);
    else if(!mapSearchResult.userpoints.isEmpty())
      showPos(mapSearchResult.userpoints.constFirst().position, 0.f, true);
    else if(!mapSearchResult.patternMarks.isEmpty())
      showPos(mapSearchResult.patternMarks.constFirst().position, 0.f, true);
    else if(!mapSearchResult.rangeMarks.isEmpty())
      showPos(mapSearchResult.rangeMarks.constFirst().position, 0.f, true);
    else if(!mapSearchResult.holdings.isEmpty())
      showPos(mapSearchResult.holdings.constFirst().position, 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing on map.")));
  }
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
  static const int ANGLE_THRESHOLD = 120;

#ifdef DEBUG_INFORMATION_WHEEL
  qDebug() << Q_FUNC_INFO << "pixelDelta" << event->pixelDelta() << "angleDelta" << event->angleDelta()
           << "lastWheelAngleY" << lastWheelAngleY << "lastWheelAngleX" << lastWheelAngleX
           << event->source() << "geometry()" << geometry() << "rect()" << rect() << "event->pos()" << event->pos()
           << "event->angleDelta()" << event->angleDelta() << "event->modifiers()" << event->modifiers();
#endif

  if(!rect().contains(event->position().toPoint()))
    // Ignore wheel events that appear outside of the view and on the scrollbars
    return;

  // Pixel is null for mouse wheel - otherwise touchpad
  int angleDeltaY = event->angleDelta().y();
  int angleDeltaX = event->angleDelta().x();

  // Sum up wheel events to start action one threshold is exceeded
  lastWheelAngleY += angleDeltaY;
  lastWheelAngleX += angleDeltaX;

  if(atools::sign(lastWheelAngleY) != atools::sign(angleDeltaY))
    // User changed direction while moving - reverse direction
    // to allow immediate scroll direction change
    lastWheelAngleY = ANGLE_THRESHOLD * atools::sign(angleDeltaY);

  if(atools::sign(lastWheelAngleX) != atools::sign(angleDeltaX))
    // User changed direction while moving - reverse direction
    // to allow immediate scroll direction change
    lastWheelAngleX = ANGLE_THRESHOLD * atools::sign(angleDeltaX);

  // Get only real modifiers
  Qt::KeyboardModifiers modifiers = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);

  bool accepted = std::abs(lastWheelAngleY) >= ANGLE_THRESHOLD ||
                  (std::abs(lastWheelAngleX) >= ANGLE_THRESHOLD && modifiers == Qt::AltModifier);
  bool directionIn = lastWheelAngleY > 0;

  if(NavApp::isDebugMovingAircraft() && (modifiers == (Qt::ControlModifier | Qt::ShiftModifier) ||
                                         modifiers == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)))
  {
    if(angleDeltaY > 0)
      debugMovingAircraft(event, -1);
    else if(angleDeltaY < 0)
      debugMovingAircraft(event, 1);
  }
  else if(accepted)
  {
    bool reverse = OptionData::instance().getFlags().testFlag(opts::GUI_REVERSE_WHEEL);
    if(reverse)
    {
      angleDeltaX = -angleDeltaX;
      angleDeltaY = -angleDeltaY;
      directionIn = !directionIn;
    }

    // Reset summed up values if accepted
    lastWheelAngleY = 0;
    if(modifiers == Qt::ControlModifier)
    {
      // Adjust map detail ===================================================================
      if(angleDeltaY > 0)
        NavApp::getMapDetailHandler()->increaseMapDetail();
      else if(angleDeltaY < 0)
        NavApp::getMapDetailHandler()->decreaseMapDetail();
    }
    else if(modifiers == (Qt::ControlModifier | Qt::ShiftModifier))
    {
      // Adjust map label detail ===================================================================
      if(angleDeltaY > 0)
        NavApp::getMapDetailHandler()->increaseMapDetailText();
      else if(angleDeltaY < 0)
        NavApp::getMapDetailHandler()->decreaseMapDetailText();
    }
    // This completely fails on Windows
    // else if(modifiers == Qt::AltModifier)
    // {
    //// Move in map position history ===================================================================
    // if(angleDeltaY > 0 || angleDeltaX > 0)
    // historyNext();
    // else if(angleDeltaY < 0 || angleDeltaX < 0)
    // historyBack();
    // }
    else if(modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)
    {
      // Zoom in/out ========================================================================
      // Check for threshold
      qreal lon, lat;
      if(geoCoordinates(event->position().x(), event->position().y(), lon, lat, Marble::GeoDataCoordinates::Degree))
      {
        // Position is visible
        qreal centerLat = centerLatitude();
        qreal centerLon = centerLongitude();

        zoomInOut(directionIn, modifiers == Qt::ShiftModifier /* smooth */);

        // Get global coordinates of cursor in new zoom level
        qreal lon2, lat2;
        geoCoordinates(event->position().x(), event->position().y(), lon2, lat2, Marble::GeoDataCoordinates::Degree);

        opts::MapNavigation nav = OptionData::instance().getMapNavigation();
        if(nav == opts::MAP_NAV_CLICK_DRAG_MOVE || nav == opts::MAP_NAV_TOUCHSCREEN)
          // Correct position and move center back to mouse cursor position
          centerOn(centerLon + (lon - lon2), centerLat + (lat - lat2));
      }
    }
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distance NM" << atools::geo::kmToNm(distance())
           << "distance km" << distance() << "zoom" << zoom();
#endif
}

bool MapWidget::touchAreaClicked(QMouseEvent *event)
{
  TouchArea click = touchAreaClick(event);
  return click != CENTER && click != NONE;
}

MapWidget::TouchArea MapWidget::touchAreaClick(QMouseEvent *event)
{
  if((event->button() != Qt::LeftButton && event->button() != Qt::NoButton) ||
     OptionData::instance().getMapNavigation() != opts::MAP_NAV_TOUCHSCREEN)
    return NONE;
  else
  {
    int areaSize = OptionData::instance().getMapNavTouchArea();
    int w = width() * areaSize / 100;
    int h = height() * areaSize / 100;

    // 3 x 3 grid
    int col, row, x = event->x(), y = event->y();
    if(x < w)
      col = 0;
    else if(x < width() - w)
      col = 1;
    else
      col = 2;

    if(y < h)
      row = 0;
    else if(y < height() - h)
      row = 1;
    else
      row = 2;

    return static_cast<TouchArea>(col + row * 3);
  }
}

bool MapWidget::handleTouchAreaClick(QMouseEvent *event)
{
  // Other should not proceed if true
  bool eventConsumed = false;

  if(OptionData::instance().getMapNavigation() != opts::MAP_NAV_TOUCHSCREEN)
    // Areas not enabled
    return eventConsumed;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "pos" << event->pos();
#endif

  switch(touchAreaClick(event))
  {

    case ZOOMIN:
      eventConsumed = true;
      zoomIn(Marble::Instant);
      break;

    case MOVEUP:
      eventConsumed = true;
      moveUp(Marble::Instant);
      break;

    case ZOOMOUT:
      eventConsumed = true;
      zoomOut(Marble::Instant);
      break;

    case MOVELEFT:
      eventConsumed = true;
      moveLeft(Marble::Instant);
      break;

    case NONE:
    case CENTER:
      // No-op
      break;

    case MOVERIGHT:
      eventConsumed = true;
      moveRight();
      break;

    case MapWidget::BACKWARD:
      eventConsumed = true;
      historyBack();
      break;

    case MOVEDOWN:
      eventConsumed = true;
      moveDown();
      break;

    case MapWidget::FORWARD:
      eventConsumed = true;
      historyNext();
      break;
  }

  if(eventConsumed)
  {
    // Restore cursor
    if(cursor().shape() != Qt::PointingHandCursor)
      setCursor(Qt::PointingHandCursor);
  }

  return eventConsumed;
}

void MapWidget::elevationDisplayTimerTimeout()
{
  if(!atools::gui::Application::isShuttingDown())
  {
    QPoint point = mapFromGlobal(QCursor::pos());

    if(rect().contains(point))
    {
      Pos pos = getGeoPos(point);
      if(pos.isValid())
      {
        pos.setAltitude(NavApp::getElevationProvider()->getElevationMeter(pos));
        mainWindow->updateMapPosLabel(pos, point.x(), point.y());
      }
    }
  }
}

bool MapWidget::eventFilter(QObject *obj, QEvent *eventParam)
{
  const static QSet<Qt::Key> ZOOM_KEYS({Qt::Key_Plus, Qt::Key_Minus, Qt::Key_Asterisk, Qt::Key_Slash});
  const static QSet<Qt::Key> MOVE_KEYS({Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down});

  if(eventParam->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(eventParam);
    if(keyEvent != nullptr)
    {
      // Get only real modifiers
      Qt::KeyboardModifiers modifiers = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
      Qt::Key key = static_cast<Qt::Key>(keyEvent->key());
      bool consumeEvent = false;

#ifdef DEBUG_INFORMATION_KEY_INPUT
      qDebug() << Q_FUNC_INFO << "Text" << keyEvent->text()
               << "Scan code" << Qt::hex << keyEvent->nativeScanCode()
               << "Key" << Qt::hex << key
               << "Modifiers" << Qt::dec << modifiers
               << "Object" << (obj != nullptr ? obj->objectName() : "nullptr");
#endif

      // Cursor keys + alt are sent and scroll map if history forward or backward actions are disabled at end of history
      // Also disable Shift/Ctrl/Alt+Direction keys to avoid overlap with OS functions
      // Suppress this here
      if(!consumeEvent && MOVE_KEYS.contains(key) && (modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)))
        consumeEvent = true;

      // Catch useless home event where Marble zooms way out. Suppress.
      if(!consumeEvent && key == Qt::Key_Home)
        consumeEvent = true;

      // Catch Ctrl++ and Ctrl+- and use it only for details
      // Do not let marble use it for zooming
      // Keys processed by actions
      // Suppress this here
      if(!consumeEvent && (key == Qt::Key_Plus || key == Qt::Key_Minus) &&
         (modifiers == Qt::ControlModifier || modifiers == (Qt::ControlModifier | Qt::ShiftModifier)))
        consumeEvent = true;

      // Movement starts delay every time
      if(!consumeEvent && MOVE_KEYS.contains(key))
        jumpBackToAircraftStart();

      if(!consumeEvent && ZOOM_KEYS.contains(key))
      {
        jumpBackToAircraftStart();

        // Pass to key event handler for zooming
        eventParam->accept(); // Do not propagate further
        event(eventParam); // Call own event handler
        return true; // Do not process further
      }

      if(consumeEvent)
      {
        eventParam->accept(); // Do not propagate further
        return true; // Do not process further
      }
    }
  }

  if(eventParam->type() == QEvent::Wheel)
    jumpBackToAircraftStart();

  QMouseEvent *mEvent = dynamic_cast<QMouseEvent *>(eventParam);
  if(mEvent != nullptr)
  {
    // Filter any obscure Marble actions around the visible globe

    if(!isPointVisible(mEvent->pos()))
    {
      eventParam->accept(); // Do not propagate further
      event(eventParam); // Call own event handler
      return true; // Do not process further
    }
  }

  if(eventParam->type() == QEvent::MouseButtonDblClick)
  {
    // Catch the double click event

    eventParam->accept(); // Do not propagate further
    event(eventParam); // Call own event handler
    return true; // Do not process further
  }

  if(eventParam->type() == QEvent::Wheel)
  {
    // Catch the wheel event and do own zooming since Marble is buggy

    eventParam->accept(); // Do not propagate further
    event(eventParam); // Call own event handler
    return true; // Do not process further
  }

  if(eventParam->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(eventParam);

    if(mouseEvent != nullptr && mouseEvent->modifiers() & Qt::ControlModifier)
      // Remove control modifer to disable Marble rectangle dragging
      mouseEvent->setModifiers(mouseEvent->modifiers() & ~Qt::ControlModifier);
  }

  if(eventParam->type() == QEvent::MouseMove)
  {
    // Update coordinate display in status bar
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(eventParam);
    Pos pos = getGeoPos(mouseEvent->pos());
    if(pos.isValid())
    {
      if(NavApp::isGlobeOfflineProvider())
        elevationDisplayTimer.start();
      mainWindow->updateMapPosLabel(pos.alt(static_cast<double>(map::INVALID_ALTITUDE_VALUE)),
                                    mouseEvent->pos().x(), mouseEvent->pos().y());
    }
    else
      mainWindow->updateMapPosLabel(atools::geo::EMPTY_POS, -1, -1);
  }

  if(eventParam->type() == QEvent::MouseMove && mouseState != mapwin::NONE)
  {
    // Do not allow mouse scrolling during drag actions
    eventParam->accept();
    event(eventParam);

    // Do not process further
    return true;
  }

  QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(eventParam);
  if(eventParam->type() == QEvent::MouseMove && mouseEvent->buttons() == Qt::NoButton && mouseState == mapwin::NONE)
  {
    // No not pass movements to marble widget to avoid cursor fighting
    eventParam->accept();
    event(eventParam);
    // Do not process further
    return true;
  }

  // Pass to base class and keep on processing
  Marble::MarbleWidget::eventFilter(obj, eventParam);
  return false;
}

void MapWidget::cancelDragAll()
{
  cancelDragRoute();
  cancelDragUserpoint();
  cancelDragDistance();

  mouseState = mapwin::NONE;
  setViewContext(Marble::Still);
  update();
}

/* Stop userpoint editing and reset coordinates and pixmap */
void MapWidget::cancelDragUserpoint()
{
  if(mouseState & mapwin::DRAG_USER_POINT)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    userpointDragCur = QPoint();
    *userpointDrag = map::MapUserpoint();
    userpointDragPixmap = QPixmap();
  }
}

/* Stop route editing and reset all coordinates */
void MapWidget::cancelDragRoute()
{
  if(mouseState & mapwin::DRAG_ROUTE_POINT || mouseState & mapwin::DRAG_ROUTE_LEG)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);

    routeDragCur = QPoint();
    routeDragFixed.clear();
    routeDragPoint = -1;
    routeDragLeg = -1;
  }
}

/* Stop new distance line or change dragging and restore backup or delete new line */
void MapWidget::cancelDragDistance()
{
  if(cursor().shape() != Qt::ArrowCursor)
    setCursor(Qt::ArrowCursor);

  if(mouseState.testFlag(mapwin::DRAG_DIST_NEW_END))
    // Remove new distance measurement line
    getScreenIndex()->removeDistanceMark(currentDistanceMarkerId);
  else if(mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_END) || mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_START))
    // Replace modified line with backup
    getScreenIndex()->updateDistanceMarker(currentDistanceMarkerId, *distanceMarkerBackup);
  currentDistanceMarkerId = -1;
}

void MapWidget::startUserpointDrag(const map::MapUserpoint& userpoint, const QPoint& point)
{
  if(userpoint.isValid() && paintLayer->getMapLayer() != nullptr)
  {
    userpointDragPixmap = *NavApp::getUserdataIcons()->getIconPixmap(userpoint.type, paintLayer->getMapLayer()->getUserPointSymbolSize());
    userpointDragCur = point;
    *userpointDrag = userpoint;
    // Start mouse dragging and disable context menu so we can catch the right button click as cancel
    mouseState = mapwin::DRAG_USER_POINT;
    setContextMenuPolicy(Qt::PreventContextMenu);
  }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

  if(NavApp::isDebugMovingAircraft())
  {
    if(event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) ||
       event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))
      debugMovingAircraft(event, 0.f);
  }

  const MapScreenIndex *screenIndex = getScreenIndexConst();
  Pos pos;
  // Change cursor and keep aircraft from centering if moving in any drag and drop mode ================
  if(mouseState & mapwin::DRAG_ALL)
  {
    jumpBackToAircraftStart();

    // Currently dragging a measurement line
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    pos = getGeoPos(event->pos());
  }

  if(mouseState.testFlag(mapwin::DRAG_DIST_NEW_END) || mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_START) ||
     mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_END))
  {
    // Changing or adding distance measurement line ==========================================
    // Position is valid update the distance mark continuously
    if(pos.isValid() && !screenIndex->getDistanceMarks().isEmpty())
    {
      if(mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_START))
        getScreenIndex()->updateDistanceMarkerFromPos(currentDistanceMarkerId, pos);
      else if(mouseState.testFlag(mapwin::DRAG_DIST_CHANGE_END) || mouseState.testFlag(mapwin::DRAG_DIST_NEW_END))
        getScreenIndex()->updateDistanceMarkerToPos(currentDistanceMarkerId, pos);
    }

  }
  else if(mouseState.testFlag(mapwin::DRAG_ROUTE_LEG) || mouseState.testFlag(mapwin::DRAG_ROUTE_POINT))
  {
    // Dragging route leg or waypoint ==========================================
    if(pos.isValid())
      // Update current point
      routeDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState.testFlag(mapwin::DRAG_USER_POINT))
  {
    // Moving userpoint ==========================================
    if(pos.isValid())
      // Update current point
      userpointDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState == mapwin::NONE && !noRender())
  {
    // No drag mode - just mouse movement - change cursor =================================

    if(event->buttons() == Qt::NoButton)
    {
      TouchArea touchClick = touchAreaClick(event);
      if(touchClick != NONE && touchClick != CENTER)
      {
        // Touchscreen mode and mouse over one area =====================================
        if(cursor().shape() != Qt::PointingHandCursor)
          setCursor(Qt::PointingHandCursor);
      }
      else if(!isPointVisible(event->pos()))
      {
        // Position is outside visible globe
        if(cursor().shape() != Qt::ArrowCursor)
          setCursor(Qt::ArrowCursor);
      }
      else
      {
        // Normal mode change cursor over route waypoints or legs and others  =====================================

        // No dragging going on now - update cursor over flight plan legs or markers
        const Route& route = NavApp::getRouteConst();

        Qt::CursorShape cursorShape = Qt::ArrowCursor;
        bool routeEditMode = mainWindow->getUi()->actionRouteEditMode->isChecked();

        // Make distance a bit larger to prefer points
        if(routeEditMode &&
           screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(), screenSearchDistance * 4 / 3,
                                                  true /* editableOnly */) != -1 && route.size() > 1)
          // Change cursor at one route point
          cursorShape = Qt::SizeAllCursor;
        else if(routeEditMode &&
                screenIndex->getNearestRouteLegIndex(event->pos().x(), event->pos().y(), screenSearchDistance) != -1 &&
                route.size() > 1)
          // Change cursor above a route line
          cursorShape = Qt::CrossCursor;
        else if(screenIndex->getNearestDistanceMarkId(event->pos().x(), event->pos().y(), screenSearchDistance) != -1)
          // Change cursor at the end of an marker
          cursorShape = Qt::CrossCursor;
        else if(screenIndex->getNearestTrafficPatternId(event->pos().x(), event->pos().y(), screenSearchDistance) != -1)
          // Change cursor at the active position
          cursorShape = Qt::PointingHandCursor;
        else if(screenIndex->getNearestHoldId(event->pos().x(), event->pos().y(), screenSearchDistance) != -1)
          // Change cursor at the active position
          cursorShape = Qt::PointingHandCursor;
        else if(screenIndex->getNearestAirportMsaId(event->pos().x(), event->pos().y(), screenSearchDistance) != -1)
          // Change cursor at the active position
          cursorShape = Qt::PointingHandCursor;
        else if(screenIndex->getNearestRangeMarkId(event->pos().x(), event->pos().y(), screenSearchDistance) != -1)
          // Change cursor at the end of an marker
          cursorShape = Qt::PointingHandCursor;

        if(cursor().shape() != cursorShape)
          setCursor(cursorShape);
      }
    } // if(event->buttons() == Qt::NoButton)
    else
      // A mouse button is pressed
      jumpBackToAircraftStart();
  }

  if(mouseState & mapwin::DRAG_ALL)
  {
    // Set context for fast redraw
    setViewContext(Marble::Animation);
    update();

    // Start timer to call resetPaintForDrag later to do a full redraw to avoid missing map objects
    resetPaintForDragTimer.start();
  }
}

void MapWidget::resetPaintForDrag()
{
  if(atools::gui::Application::isShuttingDown())
    return;

  // Reset context for full redraw when using drag and drop
  if(mouseState & mapwin::DRAG_ALL)
  {
    // Do a full redraw with all details and reload
    setViewContext(Marble::Still);
    update();
  }
}

void MapWidget::fillDistanceMarker(map::DistanceMarker& distanceMarker, const atools::geo::Pos& pos, const map::MapResult& result)
{
  fillDistanceMarker(distanceMarker, pos,
                     atools::constFirstPtrOrNull(result.airports),
                     atools::constFirstPtrOrNull(result.vors),
                     atools::constFirstPtrOrNull(result.ndbs),
                     atools::constFirstPtrOrNull(result.waypoints),
                     atools::constFirstPtrOrNull(result.userpoints));
}

void MapWidget::fillDistanceMarker(map::DistanceMarker& distanceMarker, const atools::geo::Pos& pos, const map::MapAirport *airport,
                                   const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                                   const map::MapUserpoint *userpoint)
{
  distanceMarker.flags = map::DIST_MARK_NONE;
  distanceMarker.color = QColor();
  distanceMarker.text.clear();

  // Build distance line depending on selected airport or navaid (color, magvar, etc.)
  if(userpoint != nullptr && userpoint->isValid())
  {
    distanceMarker.text = map::userpointShortText(*userpoint, 20);
    distanceMarker.from = userpoint->position;
    distanceMarker.magvar = NavApp::getMagVar(userpoint->position, 0.f);
  }
  else if(airport != nullptr && airport->isValid())
  {
    distanceMarker.text = airport->displayIdent();
    distanceMarker.from = airport->position;
    distanceMarker.magvar = airport->magvar;
    distanceMarker.color = mapcolors::colorForAirport(*airport);
  }
  else if(vor != nullptr && vor->isValid())
  {
    if(vor->tacan)
      distanceMarker.text = tr("%1 %2").arg(vor->ident).arg(vor->channel);
    else
      distanceMarker.text = tr("%1 %2").arg(vor->ident).arg(QLocale().toString(vor->frequency / 1000., 'f', 2));
    distanceMarker.from = vor->position;
    distanceMarker.magvar = vor->magvar;
    distanceMarker.color = mapcolors::vorSymbolColor;

    if(!vor->dmeOnly)
      distanceMarker.flags |= map::DIST_MARK_RADIAL; // Also TACAN

    if(vor->isCalibratedVor())
      distanceMarker.flags |= map::DIST_MARK_MAGVAR; // Only VOR, VORDME and VORTAC
  }
  else if(ndb != nullptr && ndb->isValid())
  {
    distanceMarker.text = tr("%1 %2").arg(ndb->ident).arg(QLocale().toString(ndb->frequency / 100., 'f', 2));
    distanceMarker.from = ndb->position;
    distanceMarker.magvar = ndb->magvar;
    distanceMarker.color = mapcolors::ndbSymbolColor;
    distanceMarker.flags = map::DIST_MARK_RADIAL;
  }
  else if(waypoint != nullptr && waypoint->isValid())
  {
    distanceMarker.text = waypoint->ident;
    distanceMarker.from = waypoint->position;
    distanceMarker.magvar = waypoint->magvar;
    distanceMarker.color = mapcolors::waypointSymbolColor;
  }
  else
  {
    distanceMarker.magvar = NavApp::getMagVar(pos, 0.f);
    distanceMarker.from = pos;
  }
}

void MapWidget::addDistanceMarker(const atools::geo::Pos& pos, const map::MapResult& result)
{
  addDistanceMarker(pos,
                    atools::constFirstPtrOrNull(result.airports),
                    atools::constFirstPtrOrNull(result.vors),
                    atools::constFirstPtrOrNull(result.ndbs),
                    atools::constFirstPtrOrNull(result.waypoints),
                    atools::constFirstPtrOrNull(result.userpoints));
}

void MapWidget::addDistanceMarker(const atools::geo::Pos& pos, const map::MapAirport *airport,
                                  const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                                  const map::MapUserpoint *userpoint)
{
  // Enable display of user feature
  NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_DISTANCE);

  // Distance line
  map::DistanceMarker distanceMarker;
  distanceMarker.id = map::getNextUserFeatureId();
  distanceMarker.position = distanceMarker.to = pos;

  fillDistanceMarker(distanceMarker, pos, airport, vor, ndb, waypoint, userpoint);

  getScreenIndex()->addDistanceMark(distanceMarker);

  // Start mouse dragging and disable context menu so we can catch the right button click as cancel
  mouseState = mapwin::DRAG_DIST_NEW_END;
  setContextMenuPolicy(Qt::PreventContextMenu);
  currentDistanceMarkerId = distanceMarker.id;
}

void MapWidget::contextMenuEvent(QContextMenuEvent *event)
{
  using atools::sql::SqlRecord;
  using atools::fs::sc::SimConnectAircraft;
  using atools::fs::sc::SimConnectUserAircraft;

  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "reason" << event->reason()
           << "pos" << event->pos();

  if(mouseState != mapwin::NONE)
    return;

  // Disable any automatic scrolling
  contextMenuActive = true;

  QPoint point;
  if(event->reason() == QContextMenuEvent::Keyboard)
    // Event does not contain position if triggered by keyboard
    point = mapFromGlobal(QCursor::pos());
  else
    point = event->pos();

  QPoint menuPoint = QCursor::pos();
  bool notInViewport = false;
  // Do not show context menu if point is not on the map
  if(!rect().contains(point))
  {
    point = rect().center();
    menuPoint = mapToGlobal(point);
    notInViewport = true;
  }

  if(event->reason() != QContextMenuEvent::Keyboard)
    // Move menu position off the cursor to avoid accidental selection on touchpads
    menuPoint += QPoint(3, 3);

  hideTooltip();

  // ==================================================================================================
  // Build a context menu with sub-menus depending on objects at the clicked position
  MapContextMenu contextMenu(mainWindow, this, NavApp::getRouteConst());

  // Disconnect actions used in main menu to avoid double signals
  disconnectGlobalActions();

  // ==================================================================================================
  // Open menu - use null point if cursor is outside of map window
  if(contextMenu.exec(menuPoint, notInViewport ? QPoint() : point))
  {
    // Enable map updates again
    contextMenuActive = false;
    connectGlobalActions();

    // Selected action - one of the fixed or created ones
    const QAction *action = contextMenu.getSelectedAction();

    // Selected map object
    const map::MapBase *base = contextMenu.getSelectedBase();
    map::MapAirport airport = base != nullptr ? base->asObj<map::MapAirport>() : map::MapAirport();

    qDebug() << Q_FUNC_INFO << airport;

    bool departureFilter, arrivalFilter;
    NavApp::getRouteConst().getAirportProcedureFlags(airport, -1, departureFilter, arrivalFilter);
    qDebug() << Q_FUNC_INFO << "departureFilter" << departureFilter << "arrivalFilter" << arrivalFilter;

    // Global position where clicked
    atools::geo::Pos pos = contextMenu.getPos();

    // Look at fixed pre-defined actions wich are shared ============================================
    Ui::MainWindow *ui = NavApp::getMainUi();
    if(action == ui->actionMapRangeRings)
      addRangeMark(pos, true /* showDialog */);
    else if(action == ui->actionMapSetMark)
      changeSearchMark(pos);
    else if(action == ui->actionMapJumpCoordinates)
      jumpToCoordinatesPos(pos);
    else if(action == ui->actionMapCopyCoordinates)
      copyCoordinatesPos(pos);
    else if(action == ui->actionMapSetHome)
      changeHome();
    else
    {
      // No pre-defined action - only type available ============================
      int id = -1;
      map::MapType type = map::NONE;
      map::MapVor vor;
      map::MapNdb ndb;
      map::MapWaypoint waypoint;
      map::MapUserpoint userpoint;
      const proc::MapProcedureLegs *legs = nullptr;

      if(base != nullptr)
      {
        id = base->id;
        type = base->objType;
        vor = base->asObj<map::MapVor>();
        ndb = base->asObj<map::MapNdb>();
        waypoint = base->asObj<map::MapWaypoint>();
        userpoint = base->asObj<map::MapUserpoint>();

        const map::MapProcedurePoint *procpoint = base->asPtr<map::MapProcedurePoint>();
        if(procpoint != nullptr)
        {
          const proc::MapProcedureLeg& leg = procpoint->getLeg();
          if(leg.isAnyTransition())
            // Can use legs including transition
            legs = procpoint->legs;
          else
          {
            // Load the approach without transition
            map::MapAirport procAp = queries->getAirportQueryNav()->getAirportById(procpoint->legs->ref.airportId);
            legs = queries->getProcedureQuery()->getProcedureLegs(procAp, procpoint->legs->ref.procedureId);
          }
        }
      }

      // Create a result object as it is needed for some methods
      map::MapResult result;
      result.addFromMapBase(base);

      switch(contextMenu.getSelectedActionType())
      {
        case mc::NONE:
          break;

        case mc::INFORMATION:
          emit showInformation(result);
          break;

        case mc::PROCEDURE:
          if(airport.isValid())
            emit showProcedures(airport, departureFilter, arrivalFilter);
          break;

        case mc::PROCEDUREADD:
          if(legs != nullptr)
          {
            emit routeInsertProcedure(*legs);
            NavApp::getSearchController()->clearProcedureSelectionAndPreviews();
          }
          break;

        case mc::CUSTOMAPPROACH:
          if(airport.isValid())
            emit showCustomApproach(airport, QString());
          break;

        case mc::CUSTOMDEPARTURE:
          if(airport.isValid())
            emit showCustomDeparture(airport, QString());
          break;

        case mc::MEASURE:
          addDistanceMarker(pos, &airport, &vor, &ndb, &waypoint, &userpoint);
          break;

        case mc::NAVAIDRANGE:
          // Navaid range rings
          if(vor.isValid())
            addNavRangeMark(vor.position, map::VOR, vor.ident, vor.getFrequencyOrChannel(), vor.range);
          else if(ndb.isValid())
            addNavRangeMark(ndb.position, map::NDB, ndb.ident, QString::number(ndb.frequency), ndb.range);
          break;

        case mc::PATTERN:
          addPatternMark(airport);
          break;

        case mc::HOLDING:
          addHold(result, pos);
          break;

        case mc::AIRPORT_MSA:
          addMsaMark(result.airportMsa.value(0));
          break;

        case mc::DEPARTURE:
          // Departure parking, helipad or airport
          if(type == map::PARKING)
            emit routeSetParkingStart(base->asObj<map::MapParking>());
          else if(type == map::HELIPAD)
            emit routeSetHelipadStart(base->asObj<map::MapHelipad>());
          else
            emit routeSetStart(airport);
          break;

        case mc::DESTINATION:
          emit routeSetDest(airport);
          break;

        case mc::ALTERNATE:
          emit routeAddAlternate(airport);
          break;

        case mc::ADDROUTE:
          emit routeAdd(id, pos, type, legindex::AUTO);
          break;

        case mc::APPENDROUTE:
          emit routeAdd(id, pos, type, legindex::APPEND);
          break;

        case mc::DIRECT:
          emit directTo(id, pos, type, contextMenu.getSelectedRouteIndex());
          break;

        case mc::DELETEROUTEWAYPOINT:
          NavApp::getRouteController()->routeDelete(contextMenu.getSelectedRouteIndex(), true /* selectCurrent */);
          break;

        case mc::EDITROUTEUSERPOINT:
          NavApp::getRouteController()->editUserWaypointName(contextMenu.getSelectedRouteIndex());
          break;

        case mc::CONVERTPROCEDURE:
          NavApp::getRouteController()->convertProcedure(contextMenu.getSelectedRouteIndex());
          break;

        case mc::MARKAIRPORTADDON:
          if(NavApp::isGlobeOfflineProvider())
            pos.setAltitude(NavApp::getElevationProvider()->getElevationFt(pos));
          emit addUserpointFromMap(result, pos, true /* airportAddon */);
          break;

        case mc::USERPOINTADD:
          if(NavApp::isGlobeOfflineProvider())
            pos.setAltitude(NavApp::getElevationProvider()->getElevationFt(pos));
          emit addUserpointFromMap(result, pos, false /* airportAddon */);
          break;

        case mc::USERPOINTEDIT:
          emit editUserpointFromMap(result);
          break;

        case mc::USERPOINTMOVE:
          startUserpointDrag(userpoint, point);
          break;

        case mc::USERPOINTDELETE:
          emit deleteUserpointFromMap(id);
          break;

        case mc::LOGENTRYEDIT:
          emit editLogEntryFromMap(id);
          break;

        case mc::SHOWINSEARCH:
          showResultInSearch(base);
          break;

        case mc::SHOWINROUTE:
          emit showInRoute(map::routeIndex(base));
          break;

        case mc::REMOVEUSER:
          if(type == map::MARK_RANGE)
            removeRangeMark(contextMenu.getSelectedId());
          else if(type == map::MARK_DISTANCE)
            removeDistanceMark(contextMenu.getSelectedId());
          else if(type == map::MARK_HOLDING)
            removeHoldMark(contextMenu.getSelectedId());
          else if(type == map::MARK_PATTERNS)
            removePatternMark(contextMenu.getSelectedId());
          else if(type == map::MARK_MSA)
            removeMsaMark(contextMenu.getSelectedId());
          break;
      }
    }
  }
  else
    connectGlobalActions();

  // Enable map updates again
  contextMenuActive = false;
}

void MapWidget::updateRoute(const QPoint& point, int leg, int pointIndex, bool fromClickAdd, bool fromClickAppend)
{
  qDebug() << "End route drag" << point << "leg" << leg << "point" << pointIndex;

  // Get all objects where the mouse button was released
  map::MapResult result;
  map::MapObjectQueryTypes queryTypes = map::QUERY_NONE;

  if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    queryTypes |= map::QUERY_PROC_RECOMMENDED;

    if(getShownMapTypes().testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS;

    if(getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
      queryTypes |= map::QUERY_ALTERNATE;
  }

  getScreenIndexConst()->getAllNearest(point, screenSearchDistance, result, queryTypes);

  // Allow only airports for alternates
  if(pointIndex >= 0)
  {
    if(NavApp::getRouteConst().value(pointIndex).isAlternate())
      result.clear(map::MapType(~map::AIRPORT));
  }

  // Count number of all objects
  int totalSize = result.size(map::AIRPORT_ALL_MASK | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT);

  int id = -1;
  map::MapTypes type = map::NONE;
  if(totalSize == 0)
  {
    // Nothing at the position - add userpoint
    qDebug() << Q_FUNC_INFO << "userpoint";
    type = map::USERPOINTROUTE;
  }
  else if(totalSize == 1)
  {
    // Only one entry at the position - add single navaid without menu
    qDebug() << Q_FUNC_INFO << "navaid";
    result.getIdAndType(id, type, {map::AIRPORT, map::VOR, map::NDB, map::WAYPOINT, map::USERPOINT});
  }
  else
  {
    qDebug() << Q_FUNC_INFO << "menu";

    // Avoid drag cancel when loosing focus
    mouseState |= mapwin::DRAG_POST_MENU;

    QString menuText = tr("Add %1 to Flight Plan");
    if(fromClickAdd)
      menuText = tr("Insert %1 to Flight Plan");
    else if(fromClickAppend)
      menuText = tr("Append %1 to Flight Plan");

    // Multiple entries - build a menu with icons
    showFeatureSelectionMenu(id, type, result, menuText);

    mouseState &= ~mapwin::DRAG_POST_MENU;
  }

  Pos pos = atools::geo::EMPTY_POS;
  if(type == map::USERPOINTROUTE)
    // Get position for new user point from from screen
    pos = CoordinateConverter(viewport()).sToW(point.x(), point.y());

  if((id != -1 && type != map::NONE) || type == map::USERPOINTROUTE)
  {
    if(fromClickAdd)
      emit routeAdd(id, pos, type, legindex::AUTO);
    else if(fromClickAppend)
      emit routeAdd(id, pos, type, legindex::APPEND);
    else
    {
      // From drag
      if(leg != legindex::AUTO)
        emit routeAdd(id, pos, type, leg);
      else if(pointIndex != -1)
        emit routeReplace(id, pos, type, pointIndex);
    }
  }
}

bool MapWidget::showFeatureSelectionMenu(int& id, map::MapTypes& type, const map::MapResult& result, const QString& menuText)
{
  // Add id and type to actions
  const int ICON_SIZE = 20;
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());

  for(const map::MapAirport& obj : result.airports)
  {
    QAction *action = new QAction(SymbolPainter::createAirportIcon(obj, ICON_SIZE), menuText.arg(map::airportText(obj, 20)), this);
    action->setData(QVariant::fromValue(map::MapRef(obj.id, map::AIRPORT)));
    menu.addAction(action);
  }

  if(!result.airports.isEmpty() || !result.vors.isEmpty() || !result.ndbs.isEmpty() ||
     !result.waypoints.isEmpty() || !result.userpoints.isEmpty())
    // There will be more entries - add a separator
    menu.addSeparator();

  for(const map::MapVor& obj : result.vors)
  {
    QAction *action = new QAction(SymbolPainter::createVorIcon(obj, ICON_SIZE, NavApp::isGuiStyleDark()),
                                  menuText.arg(map::vorText(obj)), this);
    action->setData(QVariant::fromValue(map::MapRef(obj.id, map::VOR)));
    menu.addAction(action);
  }
  for(const map::MapNdb& obj : result.ndbs)
  {
    QAction *action = new QAction(SymbolPainter::createNdbIcon(ICON_SIZE, NavApp::isGuiStyleDark()),
                                  menuText.arg(map::ndbText(obj)), this);
    action->setData(QVariant::fromValue(map::MapRef(obj.id, map::NDB)));
    menu.addAction(action);
  }
  for(const map::MapWaypoint& obj : result.waypoints)
  {
    QAction *action = new QAction(SymbolPainter::createWaypointIcon(ICON_SIZE), menuText.arg(map::waypointText(obj)), this);
    action->setData(QVariant::fromValue(map::MapRef(obj.id, map::WAYPOINT)));
    menu.addAction(action);
  }

  int numUserpoints = 0;
  for(const map::MapUserpoint& obj : result.userpoints)
  {
    QAction *action = nullptr;
    if(numUserpoints > 5)
    {
      action = new QAction(SymbolPainter::createUserpointIcon(ICON_SIZE), tr("More ..."), this);
      action->setDisabled(true);
      menu.addAction(action);
      break;
    }
    else
    {
      action = new QAction(SymbolPainter::createUserpointIcon(ICON_SIZE), menuText.arg(map::userpointText(obj)), this);
      action->setData(QVariant::fromValue(map::MapRef(obj.id, map::USERPOINT)));
      menu.addAction(action);
    }
    numUserpoints++;
  }

  // Always present - userpoint
  menu.addSeparator();
  {
    QAction *action = new QAction(SymbolPainter::createUserpointIcon(ICON_SIZE), menuText.arg(tr("Position")), this);
    action->setData(QVariant::fromValue(map::MapRef(-1, map::USERPOINTROUTE)));
    menu.addAction(action);
  }

  // Always present - cancel
  menu.addSeparator();
  menu.addAction(new QAction(QIcon(":/littlenavmap/resources/icons/cancel.svg"), tr("Cancel"), this));

  // Execute the menu
  QAction *action = menu.exec(QCursor::pos());

  if(action != nullptr && !action->data().isNull())
  {
    // Get id and type from selected action
    map::MapRef data = action->data().value<map::MapRef>();
    id = data.id;
    type = data.objType;
    return true;
  }
  return false;
}

void MapWidget::simDataCalcFuelOnOff(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                     const atools::fs::sc::SimConnectUserAircraft& last)
{
  // Engine start and shutdown events ===================================
  if(last.isValid() && aircraft.isValid() &&
     !aircraft.isSimPaused() && !aircraft.isSimReplay() && !last.isSimPaused() && !last.isSimReplay())
  {
    // start timer to emit takeoff/landing signal
    if(last.hasFuelFlow() != aircraft.hasFuelFlow())
      fuelOnOffTimer.start(FUEL_ON_OFF_TIMEOUT_MS);
  }
}

void MapWidget::simDataCalcTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                          const atools::fs::sc::SimConnectUserAircraft& last)
{
  // ========================================================
  // Calculate travel distance since last takeoff event ===================================
  if(!takeoffLandingLastAircraft->isValid())
    // Set for the first time
    *takeoffLandingLastAircraft = aircraft;
  else if(aircraft.isValid())
  {
    // Use less accuracy for longer routes
    float epsilon = takeoffLandingDistanceNm > 20. ? Pos::POS_EPSILON_500M : Pos::POS_EPSILON_10M;

    // Check manhattan distance in degree to minimize samples
    if(takeoffLandingLastAircraft->getPosition().distanceSimpleTo(aircraft.getPosition()) > epsilon)
    {
      // Do not record distance on replay
      if(!aircraft.isSimReplay() && !takeoffLandingLastAircraft->isSimReplay())
        takeoffLandingDistanceNm +=
          atools::geo::meterToNm(takeoffLandingLastAircraft->getPosition().distanceMeterTo(aircraft.getPosition()));

      *takeoffLandingLastAircraft = aircraft;
    }
  }

  // Check for takeoff or landing events ===================================
  // Replay is filtered out in the timer slot
  if(last.isValid() && aircraft.isValid() && !aircraft.isSimPaused() && !last.isSimPaused())
  {
#ifdef DEBUG_INFORMATION_TAKEOFFLANDING
    qDebug() << Q_FUNC_INFO << "all valid";
#endif
    // start timer to emit takeoff/landing signal
    if(last.isFlying() != aircraft.isFlying())
    {
#ifdef DEBUG_INFORMATION_TAKEOFFLANDING
      qDebug() << Q_FUNC_INFO << "last flying != current flying";
#endif
      // Call MapWidget::takeoffLandingTimeout() later
      takeoffLandingTimer.start(aircraft.isFlying() ? TAKEOFF_TIMEOUT_MS : LANDING_TIMEOUT_MS);
    }
  }
}

void MapWidget::takeoffLandingTimeout()
{
  if(atools::gui::Application::isShuttingDown())
    return;

  const atools::fs::sc::SimConnectUserAircraft aircraft = getScreenIndexConst()->getLastUserAircraft();

  if(aircraft.isFlying())
  {
    // In air after  status has changed
    qDebug() << Q_FUNC_INFO << "Takeoff detected" << aircraft.getZuluTime() << "replay" << aircraft.isSimReplay();

    // Clear for no zoom close
    touchdownDetectedZoom = false;

    // Set for one zoom close
    takeoffDetectedZoom = true; // Reset in simDataChanged()

    // Do not record logbook entries for replay
    if(!aircraft.isSimReplay() && !takeoffLandingLastAircraft->isSimReplay())
    {
      takeoffTimeSim = takeoffLandingLastAircraft->getZuluTime();
      takeoffLandingDistanceNm = 0.;

      // Delete the profile track to avoid the messy collection of older tracks
      NavApp::getMainWindow()->deleteProfileAircraftTrailPoints();

      emit aircraftTakeoff(aircraft);
    }
  }
  else
  {
    // On ground after status has changed
    qDebug() << Q_FUNC_INFO << "Landing detected takeoffLandingDistanceNm" << takeoffLandingDistanceNm
             << "replay" << aircraft.isSimReplay();

    // Set for one zoom close
    touchdownDetectedZoom = true; // Reset in simDataChanged()

    // Clear for no zoom out
    takeoffDetectedZoom = false;

    // Do not record logbook entries for replay
    if(!aircraft.isSimReplay() && !takeoffLandingLastAircraft->isSimReplay())
      emit aircraftLanding(aircraft, static_cast<float>(takeoffLandingDistanceNm));
  }
}

void MapWidget::resetTakeoffLandingDetection()
{
  takeoffLandingTimer.stop();
  fuelOnOffTimer.stop();
  takeoffLandingDistanceNm = 0.;
  *takeoffLandingLastAircraft = atools::fs::sc::SimConnectUserAircraft();
  takeoffTimeSim = QDateTime();
}

void MapWidget::showGridConfiguration()
{
  qDebug() << Q_FUNC_INFO;

  // Look through all render plugins and look for GraticulePlugin
  const QList<Marble::RenderPlugin *> plugins = renderPlugins();
  for(Marble::RenderPlugin *plugin : plugins)
  {
    if(plugin->nameId() == "coordinate-grid")
    {
      // Get configuration dialog - settings will be saved by the plugin
      QDialog *configDialog = plugin->configDialog();
      if(configDialog != nullptr)
        configDialog->exec();
      break;
    }
  }
}

void MapWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  using atools::almostNotEqual;
  using atools::geo::angleAbsDiff;

  const atools::fs::sc::SimConnectUserAircraft& aircraft = simulatorData.getUserAircraftConst();

  // Emit signal later once all values are updated - check for aircraft state changes
  bool userAircraftValidToggled = getScreenIndexConst()->getUserAircraft().isFullyValid() != aircraft.isFullyValid();

  getScreenIndex()->updateSimData(simulatorData);

  if(databaseLoadStatus || !aircraft.isValid())
  {
    getScreenIndex()->updateLastSimData(atools::fs::sc::SimConnectData());

    // Update action states if needed
    if(userAircraftValidToggled)
      emit userAircraftValidChanged();
    return;
  }

  if(NavApp::getMainUi()->actionMapShowSunShadingSimulatorTime->isChecked())
    // Update sun shade on globe with simulator time
    setSunShadingDateTime(aircraft.getZuluTime(), false /* force */);

#ifdef DEBUG_INFORMATION_SIMUPDATE
  qDebug() << Q_FUNC_INFO << "=========================================================";
#endif

  // Create screen coordinates =============================
  CoordinateConverter conv(viewport());
  bool visible = false;
  QPoint aircraftPoint = conv.wToS(aircraft.getPosition(), CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);

  // Difference from last movement on map
  const atools::fs::sc::SimConnectUserAircraft& lastAircraft = getScreenIndexConst()->getLastUserAircraft();
  QPoint aircraftPointDiff = aircraftPoint - conv.wToS(lastAircraft.getPosition());
  const OptionData& od = OptionData::instance();

  // Zoom to aircraft and next waypoint - depends on various criteria
  bool centerAircraftAndLeg = isCenterLegAndAircraftActive();

  // Used to check if objects are still visible within smaller bounds
  // Calculate the amount that has to be subtracted from each side of the rectangle
  float boxFactor = (100.f - od.getSimUpdateBox()) / 100.f / 2.f;
  if(centerAircraftAndLeg)
    boxFactor = std::min(boxFactor, 0.35f);

  // Calculate margins for all sides - keep minimum of 32 pixels - this is used for plain aircraft centering
  QRect widgetRectSmall(rect()), widgetRectSmallPlan(rect());
  int dx = std::max(static_cast<int>(width() * boxFactor), 32);
  int dy = std::max(static_cast<int>(height() * boxFactor), 32);
  widgetRectSmall.adjust(dx, dy, -dx, -dy);

  boxFactor = (100.f - PLAN_SIM_UPDATE_BOX) / 100.f / 2.f;
  dx = std::max(static_cast<int>(width() * boxFactor), 32);
  dy = std::max(static_cast<int>(height() * boxFactor), 32);
  widgetRectSmallPlan.adjust(dx, dy, -dx, -dy);

  bool wasEmpty = aircraftTrail->isEmpty();
#ifdef DEBUG_INFORMATION_DISABLED
  qDebug() << "curPos" << curPos;
  qDebug() << "widgetRectSmall" << widgetRectSmall;
#endif

  // Update map if trail was truncated
  bool trailTruncated = aircraftTrail->appendTrailPos(aircraft, true /* allowSplit */) > 0;

  // Ignore truncatated trail for logbook - should never appear
  aircraftTrailLogbook->appendTrailPos(aircraft, false /* allowSplit */);

  if(wasEmpty != aircraftTrail->isEmpty())
    // We have a track - update toolbar and menu
    emit updateActionStates();

  // ================================================================================
  // Update tooltip for bearing
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  if(now - lastSimUpdateTooltipMs > MAX_SIM_UPDATE_TOOLTIP_MS && QToolTip::isVisible())
  {
    lastSimUpdateTooltipMs = now;

    // Update result set
    updateTooltipResult();

    // Update if any aircraft is shown for heading, speed and altitude
    bool updateAircraft = NavApp::isConnectedAndAircraft() && mapResultTooltip->hasAnyAircraft();

    // Update tooltip if bearing is shown
    bool updateBearing = NavApp::isConnectedAndAircraft();

    // Aircraft moved away from cursor or nothing in current result
    bool aircraftDisappeared = mapResultTooltip->isEmpty() && mapResultTooltipLast->hasAnyAircraft();

    // Nothing found at all or aircraft moved away from cursor
    // This affects and hides tooltips across the whole application and should not be used on each update
    if(aircraftDisappeared)
      hideTooltip();

    // Update tooltip if it has bearing/distance fields
    if(updateBearing || updateAircraft)
      showTooltip(true /* update */);

    // Remember last result to detech disappearing aircraft
    *mapResultTooltipLast = *mapResultTooltip;
  }

  // ================================================================================
  // Check if screen has to be updated/scrolled/zoomed

  // Show aircraft is enabled
  bool centerAircraftChecked = mainWindow->getUi()->actionMapAircraftCenter->isChecked();

  // Get delta values for update rate
  opts::SimUpdateRate rate = od.getSimUpdateRate();
  SimUpdateDelta deltas = distance() < SIM_UPDATE_CLOSE_KM ? SIM_UPDATE_DELTA_MAP_CLOSE.value(rate) : SIM_UPDATE_DELTA_MAP.value(rate);

  // Limit number of updates per second =================================================
  if(now - lastSimUpdateMs > deltas.timeDeltaMs)
  {
    // Check if any AI aircraft are visible
    bool aiVisible = false;
    if(paintLayer->getShownMapTypes() & map::AIRCRAFT_AI ||
       paintLayer->getShownMapTypes() & map::AIRCRAFT_AI_SHIP ||
       paintLayer->getShownMapTypes() & map::AIRCRAFT_ONLINE)
    {
      for(const atools::fs::sc::SimConnectAircraft& ai : simulatorData.getAiAircraftConst())
      {
        if(getCurrentViewBoundingBox().contains(mconvert::toGdc(ai.getPosition())))
        {
          aiVisible = true;
          break;
        }
      }
    }

    // Check if position has changed significantly
    bool posHasChanged = !lastAircraft.isValid() || // No previous position
                         aircraftPointDiff.manhattanLength() >= deltas.manhattanLengthDelta; // Screen position has changed

    // Check if any data like heading has changed which requires a redraw
    bool dataHasChanged = posHasChanged ||
                          lastAircraft.isFlying() != aircraft.isFlying() ||
                          lastAircraft.isOnGround() != aircraft.isOnGround() ||
                          angleAbsDiff(lastAircraft.getHeadingDegMag(),
                                       aircraft.getHeadingDegMag()) > deltas.headingDelta || // Heading has changed
                          almostNotEqual(lastAircraft.getIndicatedSpeedKts(),
                                         aircraft.getIndicatedSpeedKts(), deltas.speedDelta) || // Speed has changed
                          almostNotEqual(lastAircraft.getPosition().getAltitude(),
                                         aircraft.getActualAltitudeFt(), deltas.altitudeDelta); // Altitude has changed

    // Force an update every five seconds to avoid hanging map view if aircraft does not move on map
    if(now - lastSimUpdateMs > 5000)
      dataHasChanged = true;

    // We can update this after checking for time difference
    lastSimUpdateMs = now;

    // Check for takeoff, landing and fuel consumption changes ===========
    simDataCalcTakeoffLanding(aircraft, lastAircraft);
    simDataCalcFuelOnOff(aircraft, lastAircraft);

    if(dataHasChanged)
      // Also changes local "last"
      getScreenIndex()->updateLastSimData(simulatorData);

    // Option to udpate always
    bool updateAlways = od.getFlags().testFlag(opts::SIM_UPDATE_MAP_CONSTANTLY);

    // Check if centering of leg is reqired =======================================
    const Route& route = NavApp::getRouteConst();
    const RouteLeg *activeLeg = route.getActiveLeg();

    // Get position of next waypoint and check visibility
    Pos nextWpPos;
    QPoint nextWpPoint;
    bool nextWpPosVisible = false;
    if(centerAircraftAndLeg)
    {
      nextWpPos = activeLeg != nullptr ? route.getActiveLeg()->getPosition() : Pos();
      nextWpPoint = conv.wToS(nextWpPos, CoordinateConverter::DEFAULT_WTOS_SIZE, &nextWpPosVisible);
      nextWpPosVisible = widgetRectSmallPlan.contains(nextWpPoint);
    }

    // Use the touchdown rect also for minimum zoom if enabled
    float touchdownZoomRectKm = MIN_ZOOM_RECT_DIAMETER_KM;
    if(od.getFlags2().testFlag(opts2::ROUTE_ZOOM_LANDING))
      touchdownZoomRectKm = Unit::rev(od.getSimZoomOnLandingDistance(), Unit::distMeterF) / 1000.f;

    float takeoffZoomRectKm = MAX_ZOOM_RECT_DIAMETER_KM;
    if(od.getFlags2().testFlag(opts2::ROUTE_ZOOM_TAKEOFF))
      takeoffZoomRectKm = Unit::rev(od.getSimZoomOnTakeoffDistance(), Unit::distMeterF) / 1000.f;

    if(centerAircraftChecked && !contextMenuActive) // centering required by button but not while menu is open
    {
      // Postpone screen updates
      setUpdatesEnabled(false);

      bool aircraftVisible = centerAircraftAndLeg ?
                             widgetRectSmallPlan.contains(aircraftPoint) : // Box for aircraft and waypoint
                             widgetRectSmall.contains(aircraftPoint); // Use defined box in options

      // Do not update if user is using drag and drop or scrolling around
      // No updates while jump back is active and user is moving around
      if(mouseState == mapwin::NONE && viewContext() == Marble::Still && !jumpBack->isActive())
      {
        if(!aircraftVisible || // Not visible on world map
           posHasChanged) // Significant change in position might require zooming or re-centering
        {
          if(centerAircraftAndLeg)
          {
            // Aircraft and next waypoint ===================================================================

            // Update four times based on flying time to next waypoint - this is recursive
            // and will update more often close to the wp
            int timeToWpUpdateMs =
              std::max(static_cast<int>(atools::geo::meterToNm(aircraft.getPosition().distanceMeterTo(nextWpPos)) /
                                        (aircraft.getGroundSpeedKts() + 1.f) * 3600.f / 4.f), 4) * 1000;

            // Zoom to rectangle every 15 seconds
            bool zoomToRect = now - lastCenterAcAndWp > timeToWpUpdateMs;

#ifdef DEBUG_INFORMATION_SIMUPDATE
            qDebug() << Q_FUNC_INFO << "==========";
            qDebug() << "curPosVisible" << aircraftVisible;
            qDebug() << "nextWpPosVisible" << nextWpPosVisible;
            qDebug() << "updateAlways" << updateAlways;
            qDebug() << "zoomToRect" << zoomToRect;
#endif
            if(!aircraftVisible || !nextWpPosVisible || updateAlways || zoomToRect)
            {
              // Wait 15 seconds after every update
              lastCenterAcAndWp = now;

              atools::geo::Rect aircraftWpRect(nextWpPos);
              aircraftWpRect.extend(aircraft.getPosition());

              // if(std::abs(aircraftWpRect.getWidthDegree()) < 0.0005)
              // qDebug() << Q_FUNC_INFO;

              if(std::abs(aircraftWpRect.getWidthDegree()) > 170.f || std::abs(aircraftWpRect.getHeightDegree()) > 170.f)
                aircraftWpRect = atools::geo::Rect(nextWpPos);

              if(!aircraftWpRect.isPoint(POS_IS_POINT_EPSILON_DEG))
              {
                // Not a point but probably a flat rectangle

                if(std::abs(aircraftWpRect.getWidthDegree()) <= POS_IS_POINT_EPSILON_DEG * 2.f)
                  // Expand E/W direction
                  aircraftWpRect.inflate(POS_IS_POINT_EPSILON_DEG, 0.f);

                if(std::abs(aircraftWpRect.getHeightDegree()) <= POS_IS_POINT_EPSILON_DEG * 2.f)
                  // Expand N/S direction
                  aircraftWpRect.inflate(0.f, POS_IS_POINT_EPSILON_DEG);
              }

#ifdef DEBUG_INFORMATION_SIMUPDATE
              qDebug() << Q_FUNC_INFO << "+++++++++++++++++++";
              qDebug() << "aircraftPoint" << aircraftPoint;
              qDebug() << "nextWpPoint" << nextWpPoint;
              qDebug() << "this->rect()" << this->rect();
              qDebug() << "widgetRectSmall" << widgetRectSmall;
              qDebug() << "aircraft.getPosition()" << aircraft.getPosition();
              qDebug() << "aircraftWpRect" << aircraftWpRect;
              qDebug() << "aircraftWpRect.getWidthDegree()" << aircraftWpRect.getWidthDegree();
              qDebug() << "aircraftWpRect.getHeightDegree()" << aircraftWpRect.getHeightDegree();
#endif

              if(!aircraftWpRect.isPoint(POS_IS_POINT_EPSILON_DEG))
              {
                // Get zoom distance from table
                auto zoomDist = std::lower_bound(ALT_TO_MIN_ZOOM_FT_NM.constBegin(), ALT_TO_MIN_ZOOM_FT_NM.constEnd(),
                                                 aircraft.getAltitudeAboveGroundFt(),
                                                 [](const std::pair<float, float>& pair, float value)->bool {
                  return pair.first < value;
                });

                // Smaller values mean zoom closer.
                float factor = od.getSimUpdateBoxCenterLegZoom() / 100.f;
                float minZoomDistKm = atools::geo::nmToKm(std::max(zoomDist->second * factor, MIN_AUTO_ZOOM_NM));

#ifndef DEBUG_PRETEND_ZOOM
                // Center on map for now ================================================
                centerRectOnMap(aircraftWpRect);
#endif

#ifdef DEBUG_INFORMATION_SIMUPDATE
                qDebug() << Q_FUNC_INFO << "distance()" << distance();
#endif
                // ================================================
                // Zoom out for a maximum of four times until aircraft and waypoint fit into the shrinked rectangle
                for(int i = 0; i < 4; i++)
                {
                  // Check if aircraft and next waypoint fit onto the map =======================
                  aircraftPoint = conv.wToS(aircraft.getPosition(), CoordinateConverter::DEFAULT_WTOS_SIZE, &aircraftVisible);
                  nextWpPoint = conv.wToS(nextWpPos, CoordinateConverter::DEFAULT_WTOS_SIZE, &nextWpPosVisible);
                  aircraftVisible = aircraftVisible && widgetRectSmallPlan.contains(aircraftPoint);
                  nextWpPosVisible = nextWpPosVisible && widgetRectSmallPlan.contains(nextWpPoint);

#ifdef DEBUG_INFORMATION
                  qDebug() << Q_FUNC_INFO << "adjustement iteration" << i
                           << "aircraftVisible" << aircraftVisible << "nextWpPosVisible" << nextWpPosVisible;
#endif

#ifndef DEBUG_PRETEND_ZOOM
                  if(!aircraftVisible || !nextWpPosVisible || distance() < minZoomDistKm)
                    // Either point is not visible - zoom out
                    zoomOut(Marble::Instant);
                  else
#endif
                  // Both are visible - done
                  break;
                }

#ifdef DEBUG_INFORMATION_SIMUPDATE
                qDebug() << Q_FUNC_INFO << "distance()" << distance();
#endif

                // ================================================
                // Avoid zooming too close - have to recalculate values again due to zoomOut above
                aircraftPoint = conv.wToS(aircraft.getPosition(), CoordinateConverter::DEFAULT_WTOS_SIZE, &aircraftVisible);
                nextWpPoint = conv.wToS(nextWpPos, CoordinateConverter::DEFAULT_WTOS_SIZE, &nextWpPosVisible);
                aircraftVisible = aircraftVisible && widgetRectSmallPlan.contains(aircraftPoint);
                nextWpPosVisible = nextWpPosVisible && widgetRectSmallPlan.contains(nextWpPoint);

                // Get distance in pixel on screen
                double aircraftWpScreenDistPixel = QLineF(aircraftPoint, nextWpPoint).length();

                // Do not zoom out if distance is larger than 1/5 of the screen size
                double minSizePixelScreenRect = QLineF(rect().topLeft(), rect().bottomRight()).length() / 5;

#ifdef DEBUG_INFORMATION
                qDebug() << Q_FUNC_INFO << "distance()" << distance() << "minZoomDistKm" << minZoomDistKm
                         << "zoomDist->first" << zoomDist->first << "zoomDist->second" << zoomDist->second
                         << "aircraftWpScreenDistPixel" << aircraftWpScreenDistPixel
                         << "minSizePixelScreenRect" << minSizePixelScreenRect;
#endif

                if(distance() < minZoomDistKm && aircraftWpScreenDistPixel > minSizePixelScreenRect)
                {
                  // Correct zoom for minimum distance
#ifndef DEBUG_PRETEND_ZOOM
                  setDistanceToMap(minZoomDistKm);
#endif
#ifdef DEBUG_INFORMATION
                  qDebug() << Q_FUNC_INFO << "after setDistanceToMap: distance()" << distance();
#endif
                }
#ifdef DEBUG_INFORMATION_SIMUPDATE
                qDebug() << Q_FUNC_INFO << "distance()" << distance();
#endif
              } // if(!aircraftWpRect.isPoint(POS_IS_POINT_EPSILON))
#ifndef DEBUG_PRETEND_ZOOM
              else if(aircraftWpRect.isValid())
              {
#ifdef DEBUG_INFORMATION
                qDebug() << Q_FUNC_INFO << "aircraftWpRect too small" << aircraftWpRect;
#endif
                centerPosOnMap(aircraft.getPosition());
              }
#endif
            } // if(!aircraftVisible || !nextWpPosVisible || updateAlways || zoomToRect)
          } // if(centerAircraftAndLeg)
          else
          {
            // Center aircraft only ===================================================================
            if(!widgetRectSmall.contains(aircraftPoint) || // Aircraft out of user defined box or ...
               updateAlways) // ... update always
              // Center aircraft only
              centerPosOnMap(aircraft.getPosition());
          }
        } // if(!aircraftVisible || ...&Center map on aircraft and next flight plan waypoint
      } // if(mouseState == mw::NONE && viewContext() == Marble::Still && !jumpBack->isActive())
    } // if(centerAircraftChecked && !contextMenuActive)

    // Zoom close after touchdown ===================================================================
    // Only if user is not mousing around on the map
    if(mouseState == mapwin::NONE && viewContext() == Marble::Still && !contextMenuActive)
    {
      if(touchdownDetectedZoom && od.getFlags2().testFlag(opts2::ROUTE_ZOOM_LANDING))
      {
        qDebug() << Q_FUNC_INFO << "Touchdown detected - zooming close" << touchdownZoomRectKm << "km";
        centerPosOnMap(aircraft.getPosition());
        setDistanceToMap(touchdownZoomRectKm);
        touchdownDetectedZoom = false;
      }
      else if(takeoffDetectedZoom && !centerAircraftAndLeg && od.getFlags2().testFlag(opts2::ROUTE_ZOOM_TAKEOFF))
      {
        qDebug() << Q_FUNC_INFO << "Takeoff detected - zooming out" << takeoffZoomRectKm << "km";
        centerPosOnMap(aircraft.getPosition());
        setDistanceToMap(takeoffZoomRectKm);
        takeoffDetectedZoom = false;
      }
    }

    if(!updatesEnabled())
      // Re-enabling updates implicitly calls update() on the widget
      setUpdatesEnabled(true);
    else if((dataHasChanged || aiVisible || trailTruncated) && !contextMenuActive)
      // Not scrolled or zoomed but needs a redraw
      update();

    // Set flag if aircraft is or was close enought to the takeoff position on the runway
    const proc::MapProcedureLegs& sidLegs = route.getSidLegs();
    if(aircraft.isOnGround() &&
       sidLegs.runwayEndSim.position.distanceMeterTo(aircraft.getPosition()) < atools::geo::feetToMeter(sidLegs.runwaySim.width))
      emit aircraftHasPassedTakeoffPoint(aircraft);

  } // if(now - lastSimUpdateMs > deltas.timeDeltaMs)

  // Update action states if needed
  if(userAircraftValidToggled)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "userAircraftValidChanged()";
#endif
    emit userAircraftValidChanged();
  }
}

void MapWidget::mainWindowShown()
{
  qDebug() << Q_FUNC_INFO;

  // Create a copy of KML files where all missing files will be removed from the recent list
  QStringList cleanKmlFilePaths(kmlFilePaths);
  for(const QString& kml : qAsConst(kmlFilePaths))
  {
    if(!loadKml(kml, false /* center */))
      // Not found
      cleanKmlFilePaths.removeAll(kml);
  }

  cleanKmlFilePaths.sort();

  kmlFilePaths = cleanKmlFilePaths;

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

  const atools::gui::MapPosHistoryEntry& currentPos = history.current();

  if(OptionData::instance().getFlags() & opts::STARTUP_SHOW_ROUTE)
  {
    qDebug() << "Show Route" << NavApp::getRouteConst().getBoundingRect();
    if(!NavApp::getRouteConst().isFlightplanEmpty())
      showRect(NavApp::getRouteConst().getBoundingRect(), false /* double click */);
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
      centerPosOnMap(currentPos.getPos());
      // Do not fix zoom - display as is
      setDistanceToMap(currentPos.getDistance(), false /* Allow adjust zoom */);
    }
    else
    {
      qDebug() << "Show 0,0" << currentPos;
      centerPosOnMap(Pos(0.f, 0.f));
      setDistanceToMap(DEFAULT_MAP_DISTANCE_KM, true /* Allow adjust zoom */);
    }
  }
  history.activate();
}

void MapWidget::clearHistory()
{
  history.clear();
}

void MapWidget::showOverlays(bool show, bool showScalebar)
{
  for(auto it = mapOverlays.constBegin(); it != mapOverlays.constEnd(); ++it)
  {
    QString name = it.key();
    Marble::AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
    {
      if(showScalebar && overlay->nameId() == "scalebar")
        continue;

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

void MapWidget::overlayStateToMenu() const
{
  qDebug() << Q_FUNC_INFO << "ignoreOverlayUpdates" << ignoreOverlayUpdates;
  if(!ignoreOverlayUpdates)
  {
    for(auto it = mapOverlays.constBegin(); it != mapOverlays.constEnd(); ++it)
    {
      QString name = it.key();
      Marble::AbstractFloatItem *overlay = floatItem(name);
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
    for(auto it = mapOverlays.constBegin(); it != mapOverlays.constEnd(); ++it)
    {
      QString name = it.key();
      Marble::AbstractFloatItem *overlay = floatItem(name);
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
  for(QAction *action : qAsConst(mapOverlays))
    connect(action, &QAction::toggled, this, &MapWidget::overlayStateFromMenu);

  for(auto it = mapOverlays.constBegin(); it != mapOverlays.constEnd(); ++it)
  {
    Marble::AbstractFloatItem *overlay = floatItem(it.key());
    if(overlay != nullptr)
      connect(overlay, &Marble::AbstractFloatItem::visibilityChanged, this, &MapWidget::overlayStateToMenu);
  }
}

bool MapWidget::isCenterLegAndAircraftActive()
{
  const Route& route = NavApp::getRouteConst();
  return OptionData::instance().getFlags2().testFlag(opts2::ROUTE_AUTOZOOM) && // Waypoint and aircraft center enabled
         !route.isEmpty() && // Have a route
         route.getActiveLegIndex() < map::INVALID_INDEX_VALUE && // Active leg present - special case 0 for one waypoint only
         getScreenIndexConst()->getUserAircraft().isFlying() && // Aircraft in air
         route.getDistanceToFlightplan() < MAX_FLIGHTPLAN_DIST_FOR_CENTER_NM; // not too far away from flight plan
}

void MapWidget::optionsChanged()
{
  const OptionData& optiondata = OptionData::instance();
  screenSearchDistance = optiondata.getMapClickSensitivity();
  screenSearchDistanceTooltip = optiondata.getMapTooltipSensitivity();

  aircraftTrail->setMaxTrackEntries(optiondata.getAircraftTrailMaxPoints());

  // aircraftTrailLogbook uses AircraftTrail::MAX_TRACK_ENTRIES which is the maximum

  MapPaintWidget::optionsChanged();
}

void MapWidget::saveState() const
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  QSettings *qSettings = atools::settings::Settings::getQSettings();
  writePluginSettings(*qSettings);
  // Workaround to overviewmap storing absolute paths which will be invalid when moving program location
  settings.remove("plugin_overviewmap/path_earth");
  settings.remove("plugin_overviewmap/path_jupiter");
  settings.remove("plugin_overviewmap/path_mars");
  settings.remove("plugin_overviewmap/path_mercury");
  settings.remove("plugin_overviewmap/path_moon");
  settings.remove("plugin_overviewmap/path_neptune");
  settings.remove("plugin_overviewmap/path_pluto");
  settings.remove("plugin_overviewmap/path_saturn");
  settings.remove("plugin_overviewmap/path_sky");
  settings.remove("plugin_overviewmap/path_sun");
  settings.remove("plugin_overviewmap/path_uranus");
  settings.remove("plugin_overviewmap/path_venus");

  // Mark coordinates
  settings.setValue(lnm::MAP_MARKLONX, searchMarkPos.getLonX());
  settings.setValue(lnm::MAP_MARKLATY, searchMarkPos.getLatY());

  // Home coordinates and zoom
  settings.setValue(lnm::MAP_HOMELONX, homePos.getLonX());
  settings.setValue(lnm::MAP_HOMELATY, homePos.getLatY());
  settings.setValue(lnm::MAP_HOMEDISTANCE, homeDistance);

  settings.setValue(lnm::MAP_KMLFILES, kmlFilePaths);

  settings.setValueVar(lnm::MAP_AIRSPACES, QVariant::fromValue(paintLayer->getShownAirspaces()));

  // Sun shading settings =====================================
  settings.setValue(lnm::MAP_SUN_SHADING_TIME_OPTION, paintLayer->getSunShading());

  // Weather source settings =====================================
  settings.setValue(lnm::MAP_WEATHER_SOURCE, paintLayer->getMapWeatherSource());

  history.saveState(atools::settings::Settings::getConfigFilename(".history"));
  getScreenIndexConst()->saveState();
  aircraftTrail->saveState(lnm::AIRCRAFT_TRACK_SUFFIX, 2 /* numBackups */);
  aircraftTrailLogbook->saveState(lnm::LOGBOOK_TRACK_SUFFIX, 0 /* numBackups */);

  overlayStateToMenu();
  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : qAsConst(mapOverlays))
    state.save(action);

  settings.setValue(lnm::LOGDATA_TAKEOFF_LANDING_DISTANCE, takeoffLandingDistanceNm);
}

void MapWidget::restoreState()
{
  qDebug() << Q_FUNC_INFO;
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  readPluginSettings(*atools::settings::Settings::getQSettings());

  // Sun shading settings ========================================
  map::MapSunShading sunShading =
    static_cast<map::MapSunShading>(settings.valueInt(lnm::MAP_SUN_SHADING_TIME_OPTION, map::SUN_SHADING_SIMULATOR_TIME));

  if(sunShading == map::SUN_SHADING_USER_TIME)
    sunShading = map::SUN_SHADING_SIMULATOR_TIME;

  sunShadingToUi(sunShading);
  paintLayer->setSunShading(sunShading);

  // Weather source settings ========================================
  map::MapWeatherSource weatherSource =
    static_cast<map::MapWeatherSource>(settings.valueInt(lnm::MAP_WEATHER_SOURCE, map::WEATHER_SOURCE_SIMULATOR));
  weatherSourceToUi(weatherSource);
  paintLayer->setWeatherSource(weatherSource);

  if(settings.contains(lnm::MAP_MARKLONX) && settings.contains(lnm::MAP_MARKLATY))
    searchMarkPos = Pos(settings.valueFloat(lnm::MAP_MARKLONX), settings.valueFloat(lnm::MAP_MARKLATY));
  else
    searchMarkPos = Pos(0.f, 0.f);

  if(settings.contains(lnm::MAP_HOMELONX) && settings.contains(lnm::MAP_HOMELATY) && settings.contains(lnm::MAP_HOMEDISTANCE))
  {
    homePos = Pos(settings.valueFloat(lnm::MAP_HOMELONX), settings.valueFloat(lnm::MAP_HOMELATY));
    homeDistance = settings.valueFloat(lnm::MAP_HOMEDISTANCE);
  }
  else
  {
    // Looks like first start after installation
    homePos = Pos(0.f, 0.f);
    homeDistance = DEFAULT_MAP_DISTANCE_KM;
  }

  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_KML) && !atools::gui::Application::isSafeMode())
  {
    kmlFilePaths = settings.valueStrList(lnm::MAP_KMLFILES);
    for(QString& path : kmlFilePaths)
      path = atools::nativeCleanPath(path);
  }

  // Restore range rings, patterns, holds and more
  getScreenIndex()->restoreState();

  aircraftTrail->setMaxTrackEntries(OptionData::instance().getAircraftTrailMaxPoints());
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_TRAIL) && !atools::gui::Application::isSafeMode())
    aircraftTrail->restoreState(lnm::AIRCRAFT_TRACK_SUFFIX);

  // aircraftTrailLogbook uses AircraftTrail::MAX_TRACK_ENTRIES
  aircraftTrailLogbook->restoreState(lnm::LOGBOOK_TRACK_SUFFIX);

  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : qAsConst(mapOverlays))
    state.restore(action);

  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_MAP_SETTINGS))
    paintLayer->setShowAirspaces(settings.valueVar(lnm::MAP_AIRSPACES,
                                                   QVariant::fromValue(map::MapAirspaceFilter())).value<map::MapAirspaceFilter>());
  else
    paintLayer->setShowAirspaces(map::MapAirspaceFilter());

  history.restoreState(atools::settings::Settings::getConfigFilename(".history"));

  takeoffLandingDistanceNm = settings.valueDouble(lnm::LOGDATA_TAKEOFF_LANDING_DISTANCE);

  updateMapVisibleUiPostDatabaseLoad();
}

void MapWidget::sunShadingToUi(map::MapSunShading sunShading)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  switch(sunShading)
  {
    case map::SUN_SHADING_SIMULATOR_TIME:
      ui->actionMapShowSunShadingSimulatorTime->setChecked(true);
      break;
    case map::SUN_SHADING_REAL_TIME:
      ui->actionMapShowSunShadingRealTime->setChecked(true);
      break;
    case map::SUN_SHADING_USER_TIME:
      ui->actionMapShowSunShadingUserTime->setChecked(true);
      break;
  }
}

bool MapWidget::checkPos(const atools::geo::Pos& pos)
{
  if(isVisibleWidget() && projection() == Marble::Mercator && pos.isPole(5.f /* epsilon */))
  {
    atools::gui::Dialog(this).showWarnMsgBox(lnm::ACTIONS_SHOW_ZOOM_WARNING,
                                             tr("<p>Cannot zoom to a position nearby the "
                                                  "poles in Mercator projection.<br/>"
                                                  "Use Spherical instead.</p>"),
                                             tr("Do not &show this dialog again."));
    return false;
  }

  // Keep zooming
  return true;
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
  if(pushButtonExitFullscreen != nullptr)
    pushButtonExitFullscreen->move(size().width() / 8, 0);

  MapPaintWidget::resizeEvent(event);
}

map::MapSunShading MapWidget::sunShadingFromUi()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->actionMapShowSunShadingSimulatorTime->isChecked())
    return map::SUN_SHADING_SIMULATOR_TIME;
  else if(ui->actionMapShowSunShadingRealTime->isChecked())
    return map::SUN_SHADING_REAL_TIME;
  else if(ui->actionMapShowSunShadingUserTime->isChecked())
    return map::SUN_SHADING_USER_TIME;

  return map::SUN_SHADING_SIMULATOR_TIME;
}

void MapWidget::weatherSourceToUi(map::MapWeatherSource weatherSource)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  switch(weatherSource)
  {
    case map::WEATHER_SOURCE_DISABLED:
      ui->actionMapShowWeatherDisabled->setChecked(true);
      break;
    case map::WEATHER_SOURCE_SIMULATOR:
      ui->actionMapShowWeatherSimulator->setChecked(true);
      break;
    case map::WEATHER_SOURCE_ACTIVE_SKY:
      ui->actionMapShowWeatherActiveSky->setChecked(true);
      break;
    case map::WEATHER_SOURCE_NOAA:
      ui->actionMapShowWeatherNoaa->setChecked(true);
      break;
    case map::WEATHER_SOURCE_VATSIM:
      ui->actionMapShowWeatherVatsim->setChecked(true);
      break;
    case map::WEATHER_SOURCE_IVAO:
      ui->actionMapShowWeatherIvao->setChecked(true);
      break;
  }
}

map::MapWeatherSource MapWidget::weatherSourceFromUi()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->actionMapShowWeatherDisabled->isChecked())
    return map::WEATHER_SOURCE_DISABLED;
  else if(ui->actionMapShowWeatherSimulator->isChecked())
    return map::WEATHER_SOURCE_SIMULATOR;
  else if(ui->actionMapShowWeatherActiveSky->isChecked())
    return map::WEATHER_SOURCE_ACTIVE_SKY;
  else if(ui->actionMapShowWeatherNoaa->isChecked())
    return map::WEATHER_SOURCE_NOAA;
  else if(ui->actionMapShowWeatherVatsim->isChecked())
    return map::WEATHER_SOURCE_VATSIM;
  else if(ui->actionMapShowWeatherIvao->isChecked())
    return map::WEATHER_SOURCE_IVAO;

  return map::WEATHER_SOURCE_SIMULATOR;
}

void MapWidget::updateSunShadingOption()
{
  paintLayer->setSunShading(sunShadingFromUi());
}

void MapWidget::resetSettingActionsToDefault()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::SignalBlocker blocker({ui->actionMapShowAirports, ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp,
                                      ui->actionMapShowIls, ui->actionMapShowGls, ui->actionMapShowHolding, ui->actionMapShowAirportMsa,
                                      ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways, ui->actionMapShowTracks,
                                      ui->actionShowAirspaces, ui->actionMapShowRoute, ui->actionMapShowTocTod, ui->actionMapShowAlternate,
                                      ui->actionMapShowAircraft, ui->actionMapShowCompassRose, ui->actionMapShowCompassRoseAttach,
                                      ui->actionMapShowEndurance, ui->actionMapShowSelectedAltRange, ui->actionMapShowTurnPath,
                                      ui->actionMapShowAircraftAi, ui->actionMapShowAircraftOnline, ui->actionShowDirectToRunway,
                                      ui->actionMapShowAircraftAiBoat, ui->actionMapShowAircraftTrack, ui->actionInfoApproachShowMissedAppr,
                                      ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowMinimumAltitude,
                                      ui->actionMapShowAirportWeather, ui->actionMapShowSunShading});

  // Menu view =====================================
  // Button airports
  ui->actionMapShowAirports->setChecked(true);

  // Submenu navaids
  ui->actionMapShowVor->setChecked(true);
  ui->actionMapShowNdb->setChecked(true);
  ui->actionMapShowWp->setChecked(true);
  ui->actionMapShowIls->setChecked(true);
  ui->actionMapShowGls->setChecked(false);
  ui->actionMapShowHolding->setChecked(false);
  ui->actionMapShowAirportMsa->setChecked(false);
  ui->actionMapShowVictorAirways->setChecked(false);
  ui->actionMapShowJetAirways->setChecked(false);
  ui->actionMapShowTracks->setChecked(false);

  // Submenu airspaces
  ui->actionShowAirspaces->setChecked(true);

  // -----------------
  ui->actionMapShowRoute->setChecked(true);
  ui->actionMapShowAlternate->setChecked(true);
  ui->actionMapShowTocTod->setChecked(true);
  ui->actionInfoApproachShowMissedAppr->setChecked(true);
  ui->actionShowDirectToRunway->setChecked(true);
  // --
  ui->actionMapShowAircraft->setChecked(true);
  ui->actionMapShowAircraftTrack->setChecked(true);
  ui->actionMapShowCompassRose->setChecked(false);
  ui->actionMapShowCompassRoseAttach->setChecked(true);
  // --
  ui->actionMapShowSelectedAltRange->setChecked(false);
  ui->actionMapShowTurnPath->setChecked(false);
  ui->actionMapShowEndurance->setChecked(false);
  // --
  ui->actionMapShowAircraftAi->setChecked(true);
  ui->actionMapShowAircraftOnline->setChecked(true);
  ui->actionMapShowAircraftAiBoat->setChecked(false);
  // --
  ui->actionMapShowGrid->setChecked(true);
  ui->actionMapShowCities->setChecked(true);
  ui->actionMapShowMinimumAltitude->setChecked(true);
  // --
  ui->actionMapShowAirportWeather->setChecked(true);
  // Weather sources unmodified
  // --
  // Sun shading data unmodified
  ui->actionMapShowSunShading->setChecked(false);
  // --

  // Set elsewhere
  // Map projection Mercator
  // Map theme OpenStreetMap
}

void MapWidget::updateThemeUi(const QString& themeId)
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->actionMapShowCities->setEnabled(NavApp::getMapThemeHandler()->hasPlacemarks(themeId));
  ui->actionMapShowSunShading->setEnabled(NavApp::getMapThemeHandler()->canSunShading(themeId));
}

void MapWidget::updateMapVisibleUi() const
{
  mapVisible->updateVisibleObjectsStatusBar();
}

void MapWidget::updateMapVisibleUiPostDatabaseLoad() const
{
  mapVisible->postDatabaseLoad();
  mapVisible->updateVisibleObjectsStatusBar();
}

void MapWidget::updateShowAircraftUi(bool centerAircraftChecked)
{
  // Adapt the menu item status if this method was not called by the action
  QAction *acAction = mainWindow->getUi()->actionMapAircraftCenter;
  if(acAction->isEnabled())
  {
    acAction->blockSignals(true);
    acAction->setChecked(centerAircraftChecked);
    acAction->blockSignals(false);
    qDebug() << "Aircraft center set to" << centerAircraftChecked;
  }
}

void MapWidget::updateMapObjectsShown()
{
  // Checked if enabled and check state is true
  using atools::gui::checked;

  Ui::MainWindow *ui = mainWindow->getUi();

  // Sun shading ====================================================
  setShowMapSunShading(ui->actionMapShowSunShading->isChecked());
  paintLayer->setSunShading(sunShadingFromUi());

  // Weather source ====================================================
  paintLayer->setWeatherSource(weatherSourceFromUi());

  // Other map features ====================================================
  setShowMapPois(ui->actionMapShowCities->isChecked());
  setShowGrid(ui->actionMapShowGrid->isChecked());

  // Remember current values in paint layer to compare and detect changes
  map::MapTypes oldTypes = getShownMapTypes();
  map::MapDisplayTypes oldDisplayTypes = getShownMapDisplayTypes();
  int oldMinRunwayLength = getShownMinimumRunwayFt();

  setShowMapObject(map::AIRWAYV, ui->actionMapShowVictorAirways->isChecked());
  setShowMapObject(map::AIRWAYJ, ui->actionMapShowJetAirways->isChecked());
  setShowMapObject(map::TRACK, ui->actionMapShowTracks->isChecked() && NavApp::hasTracks());

  setShowMapObject(map::AIRSPACE, getShownAirspaces().types & map::AIRSPACE_ALL && ui->actionShowAirspaces->isChecked());

  setShowMapObjectDisplay(map::FLIGHTPLAN, ui->actionMapShowRoute->isChecked());
  setShowMapObjectDisplay(map::FLIGHTPLAN_TOC_TOD, ui->actionMapShowTocTod->isChecked());
  setShowMapObjectDisplay(map::FLIGHTPLAN_ALTERNATE, ui->actionMapShowAlternate->isChecked());
  setShowMapObject(map::MISSED_APPROACH, ui->actionInfoApproachShowMissedAppr->isChecked());

  setShowMapObjectDisplay(map::COMPASS_ROSE, ui->actionMapShowCompassRose->isChecked());
  setShowMapObjectDisplay(map::COMPASS_ROSE_ATTACH, ui->actionMapShowCompassRoseAttach->isChecked());
  setShowMapObjectDisplay(map::AIRCRAFT_ENDURANCE, ui->actionMapShowEndurance->isChecked());
  setShowMapObjectDisplay(map::AIRCRAFT_SELECTED_ALT_RANGE, ui->actionMapShowSelectedAltRange->isChecked());
  setShowMapObjectDisplay(map::AIRCRAFT_TURN_PATH, ui->actionMapShowTurnPath->isChecked());
  setShowMapObjectDisplay(map::DIRECT_TO_DEPARTURE, ui->actionShowDirectToRunway->isChecked());
  setShowMapObject(map::AIRCRAFT, ui->actionMapShowAircraft->isChecked());
  setShowMapObject(map::AIRCRAFT_TRAIL, ui->actionMapShowAircraftTrack->isChecked());
  setShowMapObject(map::AIRCRAFT_AI, ui->actionMapShowAircraftAi->isChecked());
  setShowMapObject(map::AIRCRAFT_ONLINE, ui->actionMapShowAircraftOnline->isChecked());
  setShowMapObject(map::AIRCRAFT_AI_SHIP, ui->actionMapShowAircraftAiBoat->isChecked());

  // Display types which are not used in structs
  setShowMapObjectDisplay(map::AIRPORT_WEATHER, ui->actionMapShowAirportWeather->isChecked());
  setShowMapObjectDisplay(map::MORA, ui->actionMapShowMinimumAltitude->isChecked());
  setShowMapObjectDisplay(map::WIND_BARBS, NavApp::getWindReporter()->isWindShown());
  setShowMapObjectDisplay(map::WIND_BARBS_ROUTE, NavApp::getWindReporter()->isRouteWindShown());

  setShowMapObjectDisplay(map::LOGBOOK_DIRECT, NavApp::getLogdataController()->isDirectPreviewShown());
  setShowMapObjectDisplay(map::LOGBOOK_ROUTE, NavApp::getLogdataController()->isRoutePreviewShown());
  setShowMapObjectDisplay(map::LOGBOOK_TRACK, NavApp::getLogdataController()->isTrackPreviewShown());

  setShowMapObject(map::VOR, ui->actionMapShowVor->isChecked());
  setShowMapObject(map::NDB, ui->actionMapShowNdb->isChecked());
  setShowMapObject(map::WAYPOINT, ui->actionMapShowWp->isChecked());
  setShowMapObject(map::HOLDING, ui->actionMapShowHolding->isChecked());
  setShowMapObject(map::AIRPORT_MSA, ui->actionMapShowAirportMsa->isChecked());

  // ILS and marker are shown together
  setShowMapObject(map::ILS, ui->actionMapShowIls->isChecked());
  setShowMapObject(map::MARKER, ui->actionMapShowIls->isChecked());
  setShowMapObjectDisplay(map::GLS, ui->actionMapShowGls->isChecked());

  setShowMapObjects(NavApp::getMapMarkHandler()->getMarkTypes(), map::MARK_ALL);
  setShowMapObjects(NavApp::getMapAirportHandler()->getAirportTypes(), map::AIRPORT_ALL_MASK);
  paintLayer->setShowMinimumRunwayFt(NavApp::getMapAirportHandler()->getMinimumRunwayFt());

  updateGeometryIndex(oldTypes, oldDisplayTypes, oldMinRunwayLength);

  mapVisible->updateVisibleObjectsStatusBar();

  emit shownMapFeaturesChanged(paintLayer->getShownMapTypes());

#ifdef DEBUG_INFORMATION
  printMapTypesToLog();
#endif

  // Update widget
  update();
}

void MapWidget::showResultInSearch(const map::MapBase *base)
{
  if(base == nullptr)
    return;

  qDebug() << Q_FUNC_INFO << *base;
  using atools::sql::SqlRecord;

  if(base->objType == map::LOGBOOK)
  {
    map::MapLogbookEntry logEntry = base->asObj<map::MapLogbookEntry>();
    emit showInSearch(map::LOGBOOK, SqlRecord().
                      appendFieldAndValueIf("departure_ident", logEntry.departureIdent).
                      appendFieldAndValueIf("destination_ident", logEntry.destinationIdent).
                      appendFieldAndValueIf("simulator", logEntry.simulator).
                      appendFieldAndValueIf("aircraft_type", logEntry.aircraftType).
                      appendFieldAndValueIf("aircraft_registration", logEntry.aircraftRegistration),
                      true /* select */);
  }
  else if(base->objType == map::USERPOINT)
  {
    map::MapUserpoint userpoint = base->asObj<map::MapUserpoint>();

    SqlRecord rec;
    rec.appendFieldAndValue("ident", userpoint.ident);
    rec.appendFieldAndValueIf("region", userpoint.region);
    rec.appendFieldAndValueIf("name", userpoint.name);
    rec.appendFieldAndValueIf("type", userpoint.type);
    rec.appendFieldAndValueIf("tags", userpoint.tags);

    emit showInSearch(map::USERPOINT, rec, true /* select */);
  }
  else if(base->objType == map::AIRPORT)
    emit showInSearch(map::AIRPORT, SqlRecord().
                      appendFieldAndValue("ident", QString("\"" % base->asObj<map::MapAirport>().ident % "\"")), true /* select */);
  else if(base->objType == map::VOR)
  {
    map::MapVor vor = base->asObj<map::MapVor>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", QString("\"" % vor.ident % "\""));
    rec.appendFieldAndValueIf("name", vor.name);
    rec.appendFieldAndValueIf("region", vor.region);

    emit showInSearch(map::VOR, rec, true /* select */);
  }
  else if(base->objType == map::NDB)
  {
    map::MapNdb ndb = base->asObj<map::MapNdb>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", QString("\"" % ndb.ident % "\""));
    rec.appendFieldAndValueIf("name", ndb.name);
    rec.appendFieldAndValueIf("region", ndb.region);

    emit showInSearch(map::NDB, rec, true /* select */);
  }
  else if(base->objType == map::WAYPOINT)
  {
    map::MapWaypoint waypoint = base->asObj<map::MapWaypoint>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", QString("\"" % waypoint.ident % "\""));
    rec.appendFieldAndValueIf("name", QString());
    rec.appendFieldAndValueIf("region", waypoint.region);

    emit showInSearch(map::WAYPOINT, rec, true /* select */);
  }
  else if(base->objType == map::AIRCRAFT)
  {
    // Can only show online clients if aircraft is shadow of a client
    map::MapUserAircraft aircraft = base->asObj<map::MapUserAircraft>();
    atools::fs::sc::SimConnectAircraft shadowAircraft =
      NavApp::getOnlinedataController()->getShadowedOnlineAircraft(aircraft.getAircraft());

    if(shadowAircraft.isValid())
      emit showInSearch(map::AIRCRAFT_ONLINE, SqlRecord().appendFieldAndValue("callsign", shadowAircraft.getAirplaneRegistration()),
                        true /* select */);
  }
  else if(base->objType == map::AIRCRAFT_AI)
  {
    // Can only show online clients if aircraft is shadow of a client
    map::MapAiAircraft aircraft = base->asObj<map::MapAiAircraft>();
    atools::fs::sc::SimConnectAircraft shadowAircraft =
      NavApp::getOnlinedataController()->getShadowedOnlineAircraft(aircraft.getAircraft());
    if(shadowAircraft.isValid())
      emit showInSearch(map::AIRCRAFT_ONLINE, SqlRecord().appendFieldAndValue("callsign", shadowAircraft.getAirplaneRegistration()),
                        true /* select */);
  }
  else if(base->objType == map::AIRCRAFT_ONLINE)
    emit showInSearch(map::AIRCRAFT_ONLINE, SqlRecord().appendFieldAndValue("callsign",
                                                                            base->asObj<map::MapOnlineAircraft>().
                                                                            getAircraft().getAirplaneRegistration()),
                      true /* select */);
  else if(base->objType == map::AIRSPACE)
    emit showInSearch(map::AIRSPACE, SqlRecord().appendFieldAndValue("callsign", base->asObj<map::MapAirspace>().name), true /* select */);
}

void MapWidget::addPatternMark(const map::MapAirport& airport)
{
  qDebug() << Q_FUNC_INFO;

  TrafficPatternDialog dialog(mainWindow, airport);
  int retval = dialog.exec();
  if(retval == QDialog::Accepted)
  {
    // Enable display of user feature
    NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_PATTERNS);

    map::PatternMarker pattern;
    dialog.fillPatternMarker(pattern);
    getScreenIndex()->addPatternMark(pattern);
    mainWindow->updateMarkActionStates();
    update();
    mainWindow->setStatusMessage(tr("Added airport traffic pattern for %1.").arg(airport.displayIdent()));
  }
}

void MapWidget::removePatternMark(int id)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->removePatternMark(id);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Traffic pattern removed from map.")));
}

void MapWidget::addHold(const map::MapResult& result, const atools::geo::Pos& position)
{
  qDebug() << Q_FUNC_INFO;

  // Order of preference (same as in map context menu): airport, vor, ndb, waypoint, pos
  HoldDialog dialog(mainWindow, result, position);
  if(dialog.exec() == QDialog::Accepted)
  {
    // Enable display of user feature
    NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_HOLDING);

    map::HoldingMarker holding;
    dialog.fillHold(holding);

    getScreenIndex()->addHoldingMark(holding);

    mainWindow->updateMarkActionStates();

    update();
    mainWindow->setStatusMessage(tr("Added hold."));
  }
}

void MapWidget::removeHoldMark(int id)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->removeHoldingMark(id);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Holding removed from map.")));
}

void MapWidget::addMsaMark(map::MapAirportMsa airportMsa)
{
  qDebug() << Q_FUNC_INFO;

  if(airportMsa.isValid())
  {
    // Enable display of user feature
    NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_MSA);

    map::MsaMarker msa;
    msa.id = map::getNextUserFeatureId();
    msa.msa = airportMsa;
    msa.position = msa.msa.position;
    getScreenIndex()->addMsaMark(msa);
    mainWindow->updateMarkActionStates();

    update();
    mainWindow->setStatusMessage(tr("Added MSA diagram."));
  }
}

void MapWidget::removeMsaMark(int id)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->removeMsaMark(id);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("MSA sector diagram removed from map.")));
}

void MapWidget::resetSettingsToDefault()
{
  paintLayer->setShowAirspaces(map::MapAirspaceFilter());
  NavApp::getMapDetailHandler()->defaultMapDetail();
}

void MapWidget::showSearchMark()
{
  qDebug() << "NavMapWidget::showMark" << searchMarkPos;

  hideTooltip();
  showAircraft(false);

  if(searchMarkPos.isValid())
  {
    jumpBackToAircraftStart();
    centerPosOnMap(searchMarkPos);
    setDistanceToMap(atools::geo::nmToKm(Unit::rev(OptionData::instance().getMapZoomShowMenu(), Unit::distNmF)));
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showHome()
{
  qDebug() << Q_FUNC_INFO << homePos;

  hideTooltip();
  jumpBackToAircraftStart();
  showAircraft(false);
  if(!atools::almostEqual(homeDistance, 0.))
    // Only center position is valid - Do not fix zoom - display as is
    setDistanceToMap(homeDistance, false /* Allow adjust zoom */);

  if(homePos.isValid())
  {
    jumpBackToAircraftStart();
    centerPosOnMap(homePos);
    mainWindow->setStatusMessage(tr("Showing home position."));
  }
}

void MapWidget::copyCoordinatesCursor()
{
  QPoint point = mapFromGlobal(QCursor::pos());
  if(rect().contains(point))
    copyCoordinatesPos(getGeoPos(point));
}

void MapWidget::copyCoordinatesPos(const atools::geo::Pos& pos)
{
  qDebug() << Q_FUNC_INFO << pos;
  if(pos.isValid())
  {
    QGuiApplication::clipboard()->setText(Unit::coords(pos));
    mainWindow->setStatusMessage(QString(tr("Coordinates copied to clipboard.")));
  }
}

void MapWidget::jumpToCoordinatesCenter()
{
  // Cursor can be outside of map region
  QPoint point = mapFromGlobal(QCursor::pos());
  Pos pos = getGeoPos(point);

  if(!pos.isValid() || !rect().contains(point))
    // Use map center for initialization
    pos = getCenterPos();

  jumpToCoordinatesPos(pos);
}

void MapWidget::jumpToCoordinatesPos(const atools::geo::Pos& pos)
{
  qDebug() << Q_FUNC_INFO << pos;
  CoordinateDialog dialog(this, pos);
  if(dialog.exec() == QDialog::Accepted)
  {
    showPos(dialog.getPosition(), dialog.getZoomDistanceKm(), false /* doubleClick */);
    mainWindow->setStatusMessage(tr("Showing coordinates."));
  }
}

void MapWidget::cancelJumpBack()
{
  jumpBack->cancel();
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
  homePos = getCenterPos();
  homeDistance = distance();
  update();
  mainWindow->setStatusMessage(QString(tr("Changed home position.")));
}

void MapWidget::addNavRangeMark(const atools::geo::Pos& pos, map::MapTypes type,
                                const QString& displayIdent, const QString& frequency, float range)
{
  if(range > 0.f)
  {
    // Enable display of user feature
    NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_RANGE);

    map::RangeMarker marker;
    marker.id = map::getNextUserFeatureId();
    marker.navType = type;
    marker.position = pos;

    if(type == map::VOR)
    {
      if(frequency.endsWith('X') || frequency.endsWith('Y'))
        marker.text = tr("%1 %2").arg(displayIdent).arg(frequency);
      else
        marker.text = tr("%1 %2").arg(displayIdent).arg(QString::number(frequency.toFloat() / 1000., 'f', 2));
    }
    else if(type == map::NDB)
      marker.text = tr("%1 %2").arg(displayIdent).arg(QString::number(frequency.toFloat() / 100., 'f', 2));

    if(marker.navType == map::VOR)
      marker.color = mapcolors::vorSymbolColor;
    else if(marker.navType == map::NDB)
      marker.color = mapcolors::ndbSymbolColor;
    else
      marker.color = mapcolors::rangeRingColor;

    marker.ranges.append(range);
    getScreenIndex()->addRangeMark(marker);
    qDebug() << "navaid range" << marker.position;

    update();
    mainWindow->updateMarkActionStates();
    mainWindow->setStatusMessage(tr("Added range rings for %1.").arg(displayIdent));
  }
  else
    // No range - fall back to normal rings
    addRangeMark(pos, false /* showDialog */);
}

void MapWidget::fillRangeMarker(map::RangeMarker& rangeMarker, const atools::geo::Pos& pos, const map::MapResult& result)
{
  fillRangeMarker(rangeMarker, pos,
                  atools::constFirstPtrOrNull(result.airports),
                  atools::constFirstPtrOrNull(result.vors),
                  atools::constFirstPtrOrNull(result.ndbs),
                  atools::constFirstPtrOrNull(result.waypoints),
                  atools::constFirstPtrOrNull(result.userpoints));
}

void MapWidget::fillRangeMarker(map::RangeMarker& rangeMarker, const atools::geo::Pos& pos, const map::MapAirport *airport,
                                const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                                const map::MapUserpoint *userpoint)
{
  rangeMarker.navType = map::NONE;
  rangeMarker.text.clear();

  // Build range ring center and text depending on selected airport or navaid
  if(userpoint != nullptr && userpoint->isValid())
  {
    rangeMarker.text = map::userpointShortText(*userpoint, 20);
    rangeMarker.position = userpoint->position;
  }
  else if(airport != nullptr && airport->isValid())
  {
    rangeMarker.text = airport->displayIdent();
    rangeMarker.position = airport->position;
    rangeMarker.navType = map::AIRPORT;
  }
  else if(vor != nullptr && vor->isValid())
  {
    if(vor->tacan)
      rangeMarker.text = tr("%1 %2").arg(vor->ident).arg(vor->channel);
    else
      rangeMarker.text = tr("%1 %2").arg(vor->ident).arg(QLocale().toString(vor->frequency / 1000., 'f', 2));
    rangeMarker.position = vor->position;
    rangeMarker.navType = map::VOR;
  }
  else if(ndb != nullptr && ndb->isValid())
  {
    rangeMarker.text = tr("%1 %2").arg(ndb->ident).arg(QLocale().toString(ndb->frequency / 100., 'f', 2));
    rangeMarker.position = ndb->position;
    rangeMarker.navType = map::NDB;
  }
  else if(waypoint != nullptr && waypoint->isValid())
  {
    rangeMarker.text = waypoint->ident;
    rangeMarker.position = waypoint->position;
    rangeMarker.navType = map::WAYPOINT;
  }
  else
  {
    rangeMarker.position = pos;
  }
}

void MapWidget::addRangeMark(const atools::geo::Pos& pos, bool showDialog)
{
  // Create dialog which also loads the defaults from settings
  RangeMarkerDialog dialog(mainWindow, pos);
  bool dialogOpened = false;

  // Open dialog only if requested or if set in dialog
  if(showDialog || !dialog.isNoShowShiftClickEnabled())
  {
    if(dialog.exec() != QDialog::Accepted)
      return;

    dialogOpened = true;
  }

  // Enable display of user feature
  NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_RANGE);

  // Fill the marker object
  map::RangeMarker marker;
  dialog.fillRangeMarker(marker, dialogOpened);

  if(marker.isValid() && !marker.ranges.isEmpty())
  {
    getScreenIndex()->addRangeMark(marker);

    qDebug() << "range rings" << marker.position;
    update();
    mainWindow->updateMarkActionStates();
    mainWindow->setStatusMessage(tr("Added range rings for position."));
  }
}

void MapWidget::addRangeMark(const atools::geo::Pos& pos, const map::MapResult& result, bool showDialog)
{
  addRangeMark(pos,
               atools::constFirstPtrOrNull(result.airports),
               atools::constFirstPtrOrNull(result.vors),
               atools::constFirstPtrOrNull(result.ndbs),
               atools::constFirstPtrOrNull(result.waypoints),
               atools::constFirstPtrOrNull(result.userpoints),
               showDialog);
}

void MapWidget::addRangeMark(const atools::geo::Pos& pos, const map::MapAirport *airport,
                             const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                             const map::MapUserpoint *userpoint, bool showDialog)
{
  // Pre-fill the marker object with waypoint information before opening dialog
  map::RangeMarker marker;
  fillRangeMarker(marker, pos, airport, vor, ndb, waypoint, userpoint);

  // Create dialog which also loads the defaults from settings
  // Use the determined position (may have been snapped to waypoint/navaid)
  RangeMarkerDialog dialog(mainWindow, marker.position);
  bool dialogOpened = false;

  // Open dialog only if requested or if set in dialog
  if(showDialog || !dialog.isNoShowShiftClickEnabled())
  {
    if(dialog.exec() != QDialog::Accepted)
      return;

    dialogOpened = true;
  }

  // Enable display of user feature
  NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_RANGE);

  // Fill the marker object with dialog settings (radii, colors, etc.)
  dialog.fillRangeMarker(marker, dialogOpened);

  if(marker.isValid() && !marker.ranges.isEmpty())
  {
    getScreenIndex()->addRangeMark(marker);

    qDebug() << "range rings" << marker.position;
    update();
    mainWindow->updateMarkActionStates();

    // Create appropriate status message based on whether we attached to a waypoint/navaid
    if(!marker.text.isEmpty())
      mainWindow->setStatusMessage(tr("Added range rings for %1.").arg(marker.text));
    else
      mainWindow->setStatusMessage(tr("Added range rings for position."));
  }
}

void MapWidget::zoomInOut(bool directionIn, bool smooth)
{
  if(!directionIn && distance() > MAP_ZOOM_OUT_LIMIT_KM)
    return;

  // Reset context for full redraw when using drag and drop
  resetPaintForDrag();

  if(smooth)
  {
    // Smooth zoom
    if(directionIn)
      zoomViewBy(zoomStep() / 4);
    else
      zoomViewBy(-zoomStep() / 4);
  }
  else
  {
    // if(currentThemeIndex == map::PLAIN || currentThemeIndex == map::SIMPLE)
    if(NavApp::getMapThemeHandler()->hasDiscreteZoom(currentThemeId))
    {
      if(directionIn)
        zoomViewBy(zoomStep() * 3);
      else
        zoomViewBy(-zoomStep() * 3);
    }
    else
    {
      if(directionIn)
        zoomIn(Marble::Instant);
      else
        zoomOut(Marble::Instant);
    }
  }
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distance NM" << atools::geo::kmToNm(distance())
           << "distance km" << distance() << "zoom" << zoom();
#endif
}

void MapWidget::removeRangeMark(int id)
{
  getScreenIndex()->removeRangeMark(id);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Range ring removed from map.")));
}

void MapWidget::removeDistanceMark(int id)
{
  getScreenIndex()->removeDistanceMark(id);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Measurement line removed from map.")));
}

void MapWidget::setMapDetail(int level, int levelText)
{
  setDetailLevel(level, levelText);
  update();

  QString detailStr, detailStrText;
  if(level == MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStr = tr("—");
  else if(level > MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStr = "+" + QString::number(level);
  else if(level < MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStr = QString::number(level);

  if(levelText == MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStrText = tr("—");
  else if(levelText > MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStrText = "+" + QString::number(levelText);
  else if(levelText < MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    detailStrText = QString::number(levelText);

  // Update status bar label
  mainWindow->setDetailLabelText(tr("Detail %1/%2").arg(detailStr).arg(detailStrText));
  mainWindow->setStatusMessage(tr("Map detail level changed."));
}

void MapWidget::clearAllMarkers(map::MapTypes types)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->clearAllMarkers(types);

  if(types.testFlag(map::MARK_DISTANCE))
    currentDistanceMarkerId = -1;

  update();
  mainWindow->updateMarkActionStates();
  mainWindow->setStatusMessage(tr("User features removed from map."));
}

void MapWidget::loadAircraftTrail(const QString& filename, int& numLoaded, int& numTruncated)
{
  atools::fs::gpx::GpxData gpxData;
  atools::fs::gpx::GpxIO().loadGpx(gpxData, filename);

  numLoaded = gpxData.getNumPoints();
  numTruncated = 0;

  if(numLoaded > 0)
  {
    numTruncated = aircraftTrail->fillTrailFromGpxData(gpxData);
    qDebug() << Q_FUNC_INFO << "Num points" << gpxData.getNumPoints() << "numTruncated" << numTruncated;

    if(OptionData::instance().getFlags().testFlag(opts::GUI_CENTER_ROUTE))
      showRect(gpxData.getTrailRect(), false /* doubleClick */);

    update();
    emit updateActionStates();
    mainWindow->setStatusMessage(tr("User aircraft trail replaced."));
  }
}

void MapWidget::appendAircraftTrail(const QString& filename, int& numLoaded, int& numTruncated)
{
  atools::fs::gpx::GpxData gpxData;
  atools::fs::gpx::GpxIO().loadGpx(gpxData, filename);

  numLoaded = gpxData.getNumPoints();
  numTruncated = 0;

  if(numLoaded > 0)
  {
    numTruncated = aircraftTrail->appendTrailFromGpxData(gpxData);
    qDebug() << Q_FUNC_INFO << "Num points" << gpxData.getNumPoints() << "numTruncated" << numTruncated;

    if(OptionData::instance().getFlags().testFlag(opts::GUI_CENTER_ROUTE))
      showRect(gpxData.getTrailRect(), false /* doubleClick */);

    update();
    emit updateActionStates();
    mainWindow->setStatusMessage(tr("User aircraft trail appended."));
  }
}

void MapWidget::deleteAircraftTrail()
{
  aircraftTrail->clearTrail();
  emit updateActionStates();
  update();
}

void MapWidget::deleteAircraftTrailLogbook()
{
  aircraftTrailLogbook->clearTrail();
}

void MapWidget::setDetailLevel(int level, int levelText)
{
  qDebug() << Q_FUNC_INFO << level << levelText;

  if(level != paintLayer->getDetailLevel() || levelText != paintLayer->getDetailLevelText())
  {
    paintLayer->setDetailLevel(level, levelText);
    updateMapVisibleUi();
    getScreenIndex()->updateAllGeometry(getCurrentViewBoundingBox());
  }
}

void MapWidget::printMapTypesToLog()
{
  qDebug() << Q_FUNC_INFO << "Shown on map ===============================================" << Qt::endl
           << " - getShownMapDisplayTypes" << getShownMapDisplayTypes() << Qt::endl
           << " - getShownMapTypes" << getShownMapTypes() << Qt::endl
           << " - getShownAirspaces" << getShownAirspaces() << Qt::endl
           << " - getAirspaceSources" << queries->getAirspaceQueries()->getAirspaceSources() << Qt::endl
           << " - getMapWeatherSource" << getMapWeatherSource() << Qt::endl
           << " - getShownMinimumRunwayFt" << getShownMinimumRunwayFt() << Qt::endl
           << " - Userdata getSelectedTypes" << NavApp::getUserdataController()->getSelectedTypes() << Qt::endl
           << " - getMarkTypes" << NavApp::getMapMarkHandler()->getMarkTypes() << Qt::endl
           << " - projection" << projection() << "getCurrentThemeId" << getCurrentThemeId() << Qt::endl
           << " - getDetailLevel" << NavApp::getMapDetailHandler()->getDetailLevel()
           << "getDetailLevelText" << NavApp::getMapDetailHandler()->getDetailLevelText() << Qt::endl
           << "===============================================";
}

void MapWidget::debugMovingAircraft(QInputEvent *event, int upDown)
{
  using atools::fs::sc::SimConnectData;
  static int packetId = 0;
  static Pos lastPos;
  static QPoint lastPoint;
  static float alt = map::INVALID_ALTITUDE_VALUE, altInit = map::INVALID_ALTITUDE_VALUE;
  static bool ground = true;
  bool useProjection = false;

  bool helicopter = event->modifiers().testFlag(Qt::AltModifier);
  const Route& route = NavApp::getRouteConst();

  const QMouseEvent *mouseEvent = dynamic_cast<const QMouseEvent *>(event);
  const QWheelEvent *wheelEvent = dynamic_cast<const QWheelEvent *>(event);
  QPoint eventPos;
  if(mouseEvent != nullptr)
    eventPos = mouseEvent->pos();
  else if(wheelEvent != nullptr)
    eventPos = wheelEvent->pos();

  if(!(altInit < map::INVALID_ALTITUDE_VALUE))
  {
    map::MapAirport departureAirport = route.getDepartureAirportLeg().getAirport();
    map::MapAirport destinationAirport = route.getDepartureAirportLeg().getAirport();
    map::MapAirport nearestAirport;

    if(departureAirport.isValid() && destinationAirport.isValid())
      nearestAirport = lastPos.distanceMeterTo(departureAirport.position) < lastPos.distanceMeterTo(destinationAirport.position) ?
                       departureAirport : destinationAirport;

    if(!nearestAirport.isValid())
    {
      map::MapResult result;
      getScreenIndexConst()->getAllNearest(eventPos, 1000, result, map::QUERY_NONE);
      if(result.hasAirports())
        nearestAirport = result.airports.constFirst();
    }

    altInit = alt = nearestAirport.getAltitude();
  }

  if(wheelEvent != nullptr)
  {
    int factor = 50;
    if(std::abs(alt - altInit) > 5000.f)
      factor = 1000;
    else if(std::abs(alt - altInit) > 1000.f)
      factor = 500;
    else if(std::abs(alt - altInit) > 100.f)
      factor = 200;
    else if(std::abs(alt - altInit) > 10.f)
      factor = 100;

    ground = std::abs(alt - altInit) < 10.f;

    alt += upDown * factor;
    alt = atools::minmax(altInit, NavApp::getRouteController()->getCruiseAltitudeWidget(), alt);
  }
  else if(QPoint(lastPoint - eventPos).manhattanLength() > 2)
  {
    Pos pos = getGeoPos(eventPos);

    float projectionDistance = route.getProjectionDistance();
    if(useProjection)
    {
      if(!(projectionDistance < map::INVALID_DISTANCE_VALUE))
        projectionDistance = route.getDistanceFromStart(pos);

      if(projectionDistance < map::INVALID_DISTANCE_VALUE)
        alt = NavApp::getAltitudeLegs().getAltitudeForDistance(route.getTotalDistance() - projectionDistance);
    }

    const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerformance();
    float vertSpeed = 0.f, tas = 0.f, fuelflow = 0.f, totalFuel = perf.getUsableFuelLbs();
    float ice = 0.f;
    if(route.size() <= 2)
    {
      tas = perf.getCruiseSpeed();
      fuelflow = perf.getCruiseFuelFlowLbs();
      if(useProjection)
      {
        alt = NavApp::getRouteController()->getCruiseAltitudeWidget();
        ground = alt < NavApp::getElevationProvider()->getElevationFt(pos) + 500.f;
      }
    }
    else
    {
      float distanceFromStart = route.getDistanceFromStart(pos);
      if(useProjection)
      {
        float maxDist = helicopter ? 0.5f : 1.f;
        ground = distanceFromStart<maxDist || distanceFromStart> route.getTotalDistance() - maxDist;

        if(route.isActiveAlternate() || route.isActiveMissed())
          ground = false;
      }

      if(!ground)
      {
        if(route.isActiveAlternate() || route.isActiveMissed())
        {
          tas = perf.getAlternateSpeed();
          fuelflow = perf.getAlternateFuelFlowLbs();
          alt = NavApp::getRouteController()->getCruiseAltitudeWidget() / 2.f;
        }
        else
        {

          tas = perf.getCruiseSpeed();
          fuelflow = perf.getCruiseFuelFlowLbs();

          if(useProjection)
          {
            float tocDist = route.getTopOfClimbDistance();
            float todDist = route.getTopOfDescentDistance();

            if(projectionDistance < tocDist)
            {
              vertSpeed = perf.getClimbVertSpeed();
              tas = perf.getClimbSpeed();
              fuelflow = perf.getClimbFuelFlowLbs();
              ice = 50.f;
            }
            if(projectionDistance > todDist)
            {
              vertSpeed = -perf.getDescentVertSpeed();
              tas = perf.getDescentSpeed();
              fuelflow = perf.getDescentFuelFlowLbs();
            }
          }
        }
      }
      else
      {
        tas = 20.f;
        fuelflow = 20.f;
        if(useProjection)
        {
          if(distanceFromStart < 0.2f || distanceFromStart > route.getTotalDistance() - 0.2f)
            fuelflow = 0.f;
        }
      }
    }

    if(atools::almostEqual(fuelflow, 0.f) && !ground)
      fuelflow = 100.f;

    if(!(alt < map::INVALID_ALTITUDE_VALUE))
      alt = route.getCruiseAltitudeFt();

    pos.setAltitude(alt);
    SimConnectData data = SimConnectData::buildDebugMovingAircraft(pos, lastPos, ground, vertSpeed, tas, fuelflow, totalFuel, ice,
                                                                   route.getCruiseAltitudeFt(), NavApp::getMagVar(pos),
                                                                   perf.isJetFuel(), helicopter);
    data.setPacketId(packetId++);

    emit NavApp::getConnectClient()->dataPacketReceived(data);
    lastPos = pos;
    lastPoint = eventPos;
  }
}

void MapWidget::disconnectGlobalActions()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionMapJumpCoordinates->disconnect();
}

void MapWidget::connectGlobalActions()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionMapJumpCoordinates, &QAction::triggered, this, &MapWidget::jumpToCoordinatesCenter);
}
