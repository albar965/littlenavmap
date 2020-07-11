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

#include "mapgui/mapwidget.h"

#include "common/aircrafttrack.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/jumpback.h"
#include "common/mapcolors.h"
#include "common/maptools.h"
#include "common/symbolpainter.h"
#include "common/tabindexes.h"
#include "common/unit.h"
#include "connect/connectclient.h"
#include "fs/perf/aircraftperf.h"
#include "fs/sc/simconnectdata.h"
#include "gui/dialog.h"
#include "gui/holddialog.h"
#include "gui/mainwindow.h"
#include "gui/signalblocker.h"
#include "gui/trafficpatterndialog.h"
#include "gui/widgetstate.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapcontextmenu.h"
#include "mapgui/maplayersettings.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/maptooltip.h"
#include "mapgui/mapvisible.h"
#include "mappainter/mappaintlayer.h"
#include "navapp.h"
#include "online/onlinedatacontroller.h"
#include "route/routecontroller.h"
#include "route/routealtitude.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "userdata/userdataicons.h"
#include "weather/windreporter.h"

#include <QContextMenuEvent>
#include <QToolTip>
#include <QClipboard>

#include <marble/AbstractFloatItem.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/MarbleModel.h>

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

// Update rates defined by delta values
const static QHash<opts::SimUpdateRate, SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
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

// Get elevation when mouse is still
const int ALTITUDE_UPDATE_TIMEOUT = 200;

// Delay recognition to avoid detection of bumps
const int LANDING_TIMEOUT = 4000;
const int TAKEOFF_TIMEOUT = 2000;
const int FUEL_ON_OFF_TIMEOUT = 1000;

/* Update rate on tooltip for bearing display */
const int MAX_SIM_UPDATE_TOOLTIP_MS = 500;

// Disable center waypoint and aircraft if distance to flight plan is larger
const float MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM = 50.f;

// Default zoom distance if start position was not set (usually first start after installation */
const int DEFAULT_MAP_DISTANCE = 7000;

/* If width and height of a bounding rect are smaller than this use show point */
const float POS_IS_POINT_EPSILON = 0.0001f;

using atools::geo::Pos;

MapWidget::MapWidget(MainWindow *parent)
  : MapPaintWidget(parent, true /* real visible widget */), mainWindow(parent)
{
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

  elevationDisplayTimer.setInterval(ALTITUDE_UPDATE_TIMEOUT);
  elevationDisplayTimer.setSingleShot(true);
  connect(&elevationDisplayTimer, &QTimer::timeout, this, &MapWidget::elevationDisplayTimerTimeout);

  jumpBack = new JumpBack(this);
  connect(jumpBack, &JumpBack::jumpBack, this, &MapWidget::jumpBackToAircraftTimeout);

  takeoffLandingTimer.setSingleShot(true);
  connect(&takeoffLandingTimer, &QTimer::timeout, this, &MapWidget::takeoffLandingTimeout);

  fuelOnOffTimer.setSingleShot(true);
  connect(&fuelOnOffTimer, &QTimer::timeout, this, &MapWidget::fuelOnOffTimeout);

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
}

MapWidget::~MapWidget()
{
  elevationDisplayTimer.stop();
  takeoffLandingTimer.stop();
  fuelOnOffTimer.stop();

  qDebug() << Q_FUNC_INFO << "removeEventFilter";
  removeEventFilter(this);

  qDebug() << Q_FUNC_INFO << "delete jumpBack";
  delete jumpBack;

  qDebug() << Q_FUNC_INFO << "delete mapTooltip";
  delete mapTooltip;

  qDebug() << Q_FUNC_INFO << "delete mapVisible";
  delete mapVisible;

  qDebug() << Q_FUNC_INFO << "delete pushButtonExitFullscreen";
  delete pushButtonExitFullscreen;
}

void MapWidget::addFullScreenExitButton()
{
  removeFullScreenExitButton();

  pushButtonExitFullscreen = new QPushButton(QIcon(":/littlenavmap/resources/icons/fullscreen.svg"),
                                             tr("Exit fullscreen mode"), this);
  pushButtonExitFullscreen->setToolTip(tr("Leave fullscreen mode and restore normal window layout"));
  pushButtonExitFullscreen->setStatusTip(pushButtonExitFullscreen->toolTip());

  // Need to set palette since button inherits a empty one from the mapwidget
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
    delete pushButtonExitFullscreen;
    pushButtonExitFullscreen = nullptr;
  }
}

void MapWidget::getUserpointDragPoints(QPoint& cur, QPixmap& pixmap)
{
  cur = userpointDragCur;
  pixmap = userpointDragPixmap;
}

map::MapWeatherSource MapWidget::getMapWeatherSource() const
{
  return paintLayer->getWeatherSource();
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
    jumpBackToAircraftStart(true /* save distance too */);

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
    jumpBackToAircraftStart(true /* save distance too */);

    // Do not fix zoom - display as is
    setDistanceToMap(entry.getDistance(), false /* Allow adjust zoom */);
    centerPosOnMap(entry.getPos());
    noStoreInHistory = true;
    mainWindow->setStatusMessage(tr("Map position history back."));
    showAircraft(false);
  }
}

void MapWidget::handleInfoClick(QPoint point)
{
  qDebug() << Q_FUNC_INFO << point;

#ifdef DEBUG_NETWORK_INFORMATION
  qreal lon, lat;
  bool visible = geoCoordinates(point.x(), point.y(), lon, lat);
  if(visible)
    NavApp::getRouteController()->debugNetworkClick(Pos(lon, lat));
#endif

  mapSearchResultInfoClick.clear();
  getScreenIndexConst()->getAllNearest(point.x(), point.y(), screenSearchDistance, mapSearchResultInfoClick,
                                       map::QUERY_NONE /* For double click */);

  // Removes the online aircraft from onlineAircraft which also have a simulator shadow in simAircraft
  NavApp::getOnlinedataController()->filterOnlineShadowAircraft(mapSearchResultInfoClick.onlineAircraft,
                                                                mapSearchResultInfoClick.aiAircraft);

  // Remove all unwanted features
  optsd::DisplayClickOptions opts = OptionData::instance().getDisplayClickOptions();

  if(!opts.testFlag(optsd::CLICK_AIRCRAFT_USER))
    mapSearchResultInfoClick.userAircraft.clear();

  if(!opts.testFlag(optsd::CLICK_AIRCRAFT_AI))
  {
    mapSearchResultInfoClick.aiAircraft.clear();
    mapSearchResultInfoClick.onlineAircraft.clear();
    mapSearchResultInfoClick.onlineAircraftIds.clear();
  }

  if(!opts.testFlag(optsd::CLICK_AIRPORT))
  {
    mapSearchResultInfoClick.airports.clear();
    mapSearchResultInfoClick.airportIds.clear();
  }

  if(!(opts.testFlag(optsd::CLICK_NAVAID)))
  {
    mapSearchResultInfoClick.vors.clear();
    mapSearchResultInfoClick.vorIds.clear();
    mapSearchResultInfoClick.ndbs.clear();
    mapSearchResultInfoClick.ndbIds.clear();
    mapSearchResultInfoClick.waypoints.clear();
    mapSearchResultInfoClick.waypointIds.clear();
    mapSearchResultInfoClick.ils.clear();
    mapSearchResultInfoClick.airways.clear();
    mapSearchResultInfoClick.userpoints.clear();
    mapSearchResultInfoClick.logbookEntries.clear();
  }

  if(!opts.testFlag(optsd::CLICK_AIRSPACE))
    mapSearchResultInfoClick.airspaces.clear();

  if(opts.testFlag(optsd::CLICK_AIRPORT) && opts.testFlag(optsd::CLICK_AIRPORT_PROC) &&
     mapSearchResultInfoClick.hasAirports())
    emit showProcedures(mapSearchResultInfoClick.airports.first());

  emit showInformation(mapSearchResultInfoClick);
}

void MapWidget::fuelOnOffTimeout()
{
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

void MapWidget::takeoffLandingTimeout()
{
  const atools::fs::sc::SimConnectUserAircraft aircraft = getScreenIndexConst()->getLastUserAircraft();

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

void MapWidget::jumpBackToAircraftUpdateDistance()
{
  QVariantList values = jumpBack->getValues();
  if(values.size() == 3)
  {
    values.replace(2, distance());
    jumpBack->updateValues(values);
  }
}

void MapWidget::jumpBackToAircraftStart(bool saveDistance)
{
  if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked())
  {
    if(jumpBack->isActive())
      // Simply restart
      jumpBack->restart();
    else
      // Start and save coordinates
      jumpBack->start({centerLongitude(), centerLatitude(), saveDistance ? distance() : QVariant(QVariant::Double)});
  }
}

void MapWidget::jumpBackToAircraftCancel()
{
  jumpBack->cancel();
}

void MapWidget::jumpBackToAircraftTimeout(const QVariantList& values)
{
  if(NavApp::getMainUi()->actionMapAircraftCenter->isChecked() && NavApp::isConnectedAndAircraft() &&
     OptionData::instance().getFlags2() & opts2::ROUTE_NO_FOLLOW_ON_MOVE)
  {

    if(mouseState != mw::NONE || viewContext() == Marble::Animation || contextMenuActive)
      // Restart as long as menu is active or user is dragging around
      jumpBack->restart();
    else
    {
      jumpBack->cancel();

      hideTooltip();
      centerPosOnMap(Pos(values.at(0).toFloat(), values.at(1).toFloat()));

      if(values.size() > 2)
      {
        QVariant distVar = values.at(2);
        if(distVar.isValid() && !distVar.isNull())
          setDistanceToMap(distVar.toDouble(), false /* Allow adjust zoom */);
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
#ifdef DEBUG_MOVING_AIRPLANE
    if(QGuiApplication::queryKeyboardModifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
      return QWidget::event(event);

#endif

    if(mouseState == mw::NONE)
    {
      QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

      // Load tooltip data into mapSearchResultTooltip
      mapSearchResultTooltip = map::MapSearchResult();
      getScreenIndexConst()->getAllNearest(helpEvent->pos().x(),
                                           helpEvent->pos().y(), screenSearchDistanceTooltip,
                                           mapSearchResultTooltip,
                                           map::QUERY_PROC_POINTS | map::QUERY_HOLDS | map::QUERY_PATTERNS |
                                           map::QUERY_RANGEMARKER);

      NavApp::getOnlinedataController()->filterOnlineShadowAircraft(mapSearchResultTooltip.onlineAircraft,
                                                                    mapSearchResultTooltip.aiAircraft);

      tooltipPos = helpEvent->globalPos();

      // Build HTML
      showTooltip(false /* update */);
      event->accept();
      return true;
    }
  }

  return QWidget::event(event);
}

void MapWidget::hideTooltip()
{
  QToolTip::hideText();
  tooltipPos = QPoint();
}

void MapWidget::handleHistory()
{
  if(!noStoreInHistory)
    // Not changed by next/last in history
    history.addEntry(Pos(centerLongitude(), centerLatitude()), distance());

  noStoreInHistory = false;
}

void MapWidget::updateTooltip()
{
  showTooltip(true /* update */);
}

void MapWidget::showTooltip(bool update)
{
  if(databaseLoadStatus)
    return;

  // Try to avoid spurious tooltip events
  if(update && !QToolTip::isVisible())
    return;

  // Build a new tooltip HTML for weather changes or aircraft updates
  QString text = mapTooltip->buildTooltip(mapSearchResultTooltip, NavApp::getRouteConst(),
                                          paintLayer->getMapLayer()->isAirportDiagram());

  if(!text.isEmpty() && !tooltipPos.isNull())
    QToolTip::showText(tooltipPos, text /*, nullptr, QRect(), 3600 * 1000*/);
  else
    hideTooltip();
}

/* Stop all line drag and drop if the map loses focus */
void MapWidget::focusOutEvent(QFocusEvent *)
{
  hideTooltip();

  if(!(mouseState & mw::DRAG_POST_MENU))
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }
}

void MapWidget::leaveEvent(QEvent *)
{
  hideTooltip();
  mainWindow->updateMapPosLabel(Pos(), -1, -1);
}

void MapWidget::keyPressEvent(QKeyEvent *event)
{
  // #ifdef DEBUG_INFORMATION
  // qDebug() << Q_FUNC_INFO << hex << event->key() << dec << event->modifiers();
  // #endif

  // Does not work for key presses that are consumed by the widget
  if(event->key() == Qt::Key_Escape)
  {
    cancelDragAll();
    setContextMenuPolicy(Qt::DefaultContextMenu);
  }
  else if(event->key() == Qt::Key_Plus && !(event->modifiers() & Qt::ControlModifier))
    zoomInOut(true /* in */, event->modifiers() & Qt::ShiftModifier /* smooth */);
  else if(event->key() == Qt::Key_Minus && !(event->modifiers() & Qt::ControlModifier))
    zoomInOut(false /* in */, event->modifiers() & Qt::ShiftModifier /* smooth */);
  else if(event->key() == Qt::Key_Asterisk)
    zoomInOut(true /* in */, true /* smooth */);
  else if(event->key() == Qt::Key_Slash)
    zoomInOut(false /* in */, true /* smooth */);
  else if(event->key() == Qt::Key_Menu && mouseState == mw::NONE)
    // First menu key press after dragging - enable context menu again
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

bool MapWidget::mousePressCheckModifierActions(QMouseEvent *event)
{
  if(mouseState != mw::NONE || event->type() != QEvent::MouseButtonRelease)
    // Not if dragging or for button release
    return false;

  qreal lon, lat;
  // Cursor can be outside or map region
  if(geoCoordinates(event->pos().x(), event->pos().y(), lon, lat))
  {
    Pos pos(lon, lat);

    // Look for navaids or airports nearby click
    map::MapSearchResult result;
    getScreenIndexConst()->getAllNearest(event->pos().x(), event->pos().y(), screenSearchDistance, result,
                                         map::QUERY_NONE);

    // Range rings =======================================================================
    if(event->modifiers() == Qt::ShiftModifier)
    {
      NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_RANGE_RINGS);
      int index = getScreenIndexConst()->getNearestRangeMarkIndex(event->pos().x(), event->pos().y(),
                                                                  screenSearchDistance);
      if(index != -1)
        // Remove any ring for Shift+Click into center
        removeRangeRing(index);
      else
      {
        // Add rings for Shift+Click
        if(result.hasVor())
          // Add VOR range
          addNavRangeRing(pos, map::VOR, result.vors.first().ident, result.vors.first().getFrequencyOrChannel(),
                          result.vors.first().range);
        else if(result.hasNdb())
          // Add NDB range
          addNavRangeRing(pos, map::NDB, result.ndbs.first().ident, QString::number(result.ndbs.first().frequency),
                          result.ndbs.first().range);
        else
          // Add range rings per configuration
          addRangeRing(pos);
      }
      return true;
    }
    // Edit route =======================================================================
    else if(event->modifiers() == (Qt::AltModifier | Qt::ControlModifier) ||
            event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))
    {
      int routeIndex = getScreenIndexConst()->getNearestRoutePointIndex(event->pos().x(),
                                                                        event->pos().y(), screenSearchDistance);

      if(routeIndex != -1)
      {
        // Position is editable - remove
        NavApp::getRouteController()->routeDelete(routeIndex);
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
        if(NavApp::getElevationProvider()->isGlobeOfflineProvider())
          pos.setAltitude(atools::geo::meterToFeet(NavApp::getElevationProvider()->getElevationMeter(pos)));
        emit addUserpointFromMap(result, pos);
      }
    }
    // Measurement =======================================================================
    else if(event->modifiers() == Qt::ControlModifier || event->modifiers() == Qt::AltModifier)
    {
      NavApp::getMapMarkHandler()->showMarkTypes(map::MARK_MEASUREMENT);
      int index = getScreenIndexConst()->getNearestDistanceMarkIndex(event->pos().x(), event->pos().y(),
                                                                     screenSearchDistance);
      if(index != -1)
        // Remove any measurement line for Ctrl+Click or Alt+Click into center
        removeDistanceMarker(index);
      else
        // Add measurement line for Ctrl+Click or Alt+Click into center
        addMeasurement(pos, result);
      return true;
    }
  }
  return false;
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "state" << mouseState << "modifiers" << event->modifiers() << "pos" << event->pos();
#endif

#ifdef DEBUG_MOVING_AIRPLANE
  debugMovingPlane(event);
#endif

  hideTooltip();

  jumpBackToAircraftStart(true /* save distance too */);

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
    if(touchAreaClicked(event))
    {
      // Touch/navigation areas are enabled and cursor is within a touch area
      if(event->button() == Qt::LeftButton && cursor().shape() != Qt::PointingHandCursor)
        setCursor(Qt::PointingHandCursor);
    }
    else if(!pointVisible(event->pos()))
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

  // Take actions (add/remove range rings, measurement)
  if(mousePressCheckModifierActions(event))
    // Event was consumed - do not proceed here
    return;

  hideTooltip();

  jumpBackToAircraftStart(true /* save distance too */);

  // Check if mouse was move for dragging before switching to drag and drop
  int mouseMoveTolerance = 4;
  bool mouseMove = (event->pos() - mouseMoved).manhattanLength() >= mouseMoveTolerance;
  bool touchArea = touchAreaClicked(event);

  if(mouseState & mw::DRAG_ROUTE_POINT || mouseState & mw::DRAG_ROUTE_LEG)
  {
    // End route point dragging
    if(mouseState & mw::DRAG_POST)
    {
      // Ending route dragging - update route
      qreal lon, lat;
      bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      if(visible)
        updateRoute(routeDragCur, routeDragLeg, routeDragPoint, false /* click add */, false /* click append */);
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
    if(!getScreenIndexConst()->getDistanceMarks().isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(mouseState & mw::DRAG_POST)
      {
        qreal lon, lat;
        bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
        if(visible)
          // Update distance measurement line
          getScreenIndex()->getDistanceMarks()[currentDistanceMarkerIndex].to = Pos(lon, lat);
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
  else if(touchArea && !mouseMove)
    // Touch/navigation areas are enabled and cursor is within a touch area - scroll, zoom, etc.
    handleTouchAreaClick(event);
  else if(event->button() == Qt::LeftButton && !mouseMove)
  {
    // Start all dragging if left button was clicked and mouse was not moved ==========================
    currentDistanceMarkerIndex = getScreenIndexConst()->getNearestDistanceMarkIndex(event->pos().x(), event->pos().y(),
                                                                                    screenSearchDistance);
    if(currentDistanceMarkerIndex != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = mw::DRAG_CHANGE_DISTANCE;
      distanceMarkerBackup = getScreenIndexConst()->getDistanceMarks().at(currentDistanceMarkerIndex);
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
            getScreenIndexConst()->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                             screenSearchDistance * 4 / 3);
          if(routePoint != -1)
          {
            // Drag a waypoint ==============================================
            routeDragPoint = routePoint;
            qDebug() << "route point" << routePoint;

            // Found a leg - start dragging
            mouseState = mw::DRAG_ROUTE_POINT;

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
            int routeLeg = getScreenIndexConst()->getNearestRouteLegIndex(event->pos().x(),
                                                                          event->pos().y(), screenSearchDistance);
            if(routeLeg != -1)
            {
              routeDragLeg = routeLeg;
              qDebug() << "route leg" << routeLeg;
              // Found a leg - start dragging
              mouseState = mw::DRAG_ROUTE_LEG;

              routeDragCur = QPoint(event->pos().x(), event->pos().y());

              routeDragFixed.clear();
              routeDragFixed.append(route.value(routeLeg).getPosition());
              routeDragFixed.append(route.value(routeLeg + 1).getPosition());
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

        if(OptionData::instance().getMapNavigation() == opts::MAP_NAV_CLICK_CENTER)
        {
          // Center map on click
          qreal lon, lat;
          bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
          if(visible)
            showPos(Pos(lon, lat), map::INVALID_DISTANCE_VALUE, true);
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

  // Show pos and show rect already call this
  // jumpBackToAircraftStart();

  if(mouseState != mw::NONE)
    return;

  if(touchAreaClicked(event))
  {
    // Touch/navigation areas are enabled and cursor is within a touch area - scroll, zoom, etc.
    handleTouchAreaClick(event);
    return;
  }

  hideTooltip();

  map::MapSearchResult mapSearchResult;

  if(OptionData::instance().getMapNavigation() == opts::MAP_NAV_CLICK_CENTER &&
     !mapSearchResultInfoClick.isEmpty(map::AIRPORT | map::AIRCRAFT_ALL | map::NAV_ALL | map::ILS | map::USERPOINT |
                                       map::USERPOINTROUTE))
  {
    // Do info click and use previous result from single click event if the double click was on a map object
    mapSearchResult = mapSearchResultInfoClick;
    mapSearchResultInfoClick.clear();
  }
  else
    getScreenIndexConst()->getAllNearest(event->pos().x(), event->pos().y(), screenSearchDistance, mapSearchResult,
                                         map::QUERY_HOLDS | map::QUERY_PATTERNS | map::QUERY_RANGEMARKER);

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
    else if(!mapSearchResult.ils.isEmpty())
      showRect(mapSearchResult.ils.first().bounding, true);
    else if(!mapSearchResult.userpointsRoute.isEmpty())
      showPos(mapSearchResult.userpointsRoute.first().position, 0.f, true);
    else if(!mapSearchResult.userpoints.isEmpty())
      showPos(mapSearchResult.userpoints.first().position, 0.f, true);
    else if(!mapSearchResult.trafficPatterns.isEmpty())
      showPos(mapSearchResult.trafficPatterns.first().position, 0.f, true);
    else if(!mapSearchResult.rangeMarkers.isEmpty())
      showPos(mapSearchResult.rangeMarkers.first().position, 0.f, true);
    else if(!mapSearchResult.holds.isEmpty())
      showPos(mapSearchResult.holds.first().position, 0.f, true);
    mainWindow->setStatusMessage(QString(tr("Showing on map.")));
  }
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "pixelDelta" << event->pixelDelta() << "angleDelta" << event->angleDelta()
           << event->source()
           << "geometry()" << geometry()
           << "rect()" << rect()
           << "event->pos()" << event->pos();
#endif

  if(!rect().contains(event->pos()))
    // Ignore wheel events that appear outside of the view and on the scrollbars
    return;

  if(event->timestamp() > lastWheelEventTimestamp + 500)
    lastWheelPos = 0;

  // Sum up wheel events
  lastWheelPos += event->angleDelta().y();

  if(event->modifiers() == Qt::ControlModifier)
  {
    // Adjust map detail ===================================================================
    if(std::abs(lastWheelPos) >= 50)
    {
      if(event->angleDelta().y() > 0)
        increaseMapDetail();
      else if(event->angleDelta().y() < 0)
        decreaseMapDetail();
    }
  }
  else
  {
    // Zoom in/out ========================================================================
    // Check for threshold
    if(std::abs(lastWheelPos) >= 120)
    {
      bool directionIn = lastWheelPos > 0;
      lastWheelPos = 0;

      qreal lon, lat;
      if(geoCoordinates(event->pos().x(), event->pos().y(), lon, lat, Marble::GeoDataCoordinates::Degree))
      {
        // Position is visible
        qreal centerLat = centerLatitude();
        qreal centerLon = centerLongitude();

        zoomInOut(directionIn, event->modifiers() == Qt::ShiftModifier /* smooth */);

        // Get global coordinates of cursor in new zoom level
        qreal lon2, lat2;
        geoCoordinates(event->pos().x(), event->pos().y(), lon2, lat2, Marble::GeoDataCoordinates::Degree);

        opts::MapNavigation nav = OptionData::instance().getMapNavigation();
        if(nav == opts::MAP_NAV_CLICK_DRAG_MOVE || nav == opts::MAP_NAV_TOUCHSCREEN)
          // Correct position and move center back to mouse cursor position
          centerOn(centerLon + (lon - lon2), centerLat + (lat - lat2));
      }
    }
  }
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
      zoomIn();
      break;

    case MOVEUP:
      eventConsumed = true;
      moveUp();
      break;

    case ZOOMOUT:
      eventConsumed = true;
      zoomOut();
      break;

    case MOVELEFT:
      eventConsumed = true;
      moveLeft();
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
  qreal lon, lat;
  QPoint point = mapFromGlobal(QCursor::pos());

  if(rect().contains(point))
  {
    if(geoCoordinates(point.x(), point.y(), lon, lat, Marble::GeoDataCoordinates::Degree))
    {
      Pos pos(lon, lat);
      pos.setAltitude(NavApp::getElevationProvider()->getElevationMeter(pos));
      mainWindow->updateMapPosLabel(pos, point.x(), point.y());
    }
  }
}

bool MapWidget::pointVisible(const QPoint& point)
{
  qreal lon, lat;
  return geoCoordinates(point.x(), point.y(), lon, lat);
}

bool MapWidget::eventFilter(QObject *obj, QEvent *e)
{
  bool jumpBackWasActive = false;

  if(e->type() == QEvent::KeyPress || e->type() == QEvent::Wheel)
    jumpBackWasActive = jumpBack->isActive();

  if(e->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    if(keyEvent != nullptr)
    {
      if(atools::contains(static_cast<Qt::Key>(keyEvent->key()), {Qt::Key_Plus, Qt::Key_Minus}) &&
         (keyEvent->modifiers() & Qt::ControlModifier))
      {
        // Catch Ctrl++ and Ctrl+- and use it only for details
        // Do not let marble use it for zooming
        // Keys processed by actions

        e->accept(); // Do not propagate further
        event(e); // Call own event handler
        return true; // Do not process further
      }

      if(atools::contains(static_cast<Qt::Key>(keyEvent->key()),
                          {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down}))
        // Movement starts delay every time
        jumpBackToAircraftStart(true /* save distance too */);

      if(atools::contains(static_cast<Qt::Key>(keyEvent->key()), {Qt::Key_Plus, Qt::Key_Minus,
                                                                  Qt::Key_Asterisk, Qt::Key_Slash}))
      {
        if(jumpBack->isActive() || isCenterLegAndAircraftActive())
          // Movement starts delay every time
          jumpBackToAircraftStart(true /* save distance too */);

        if(!jumpBackWasActive && !isCenterLegAndAircraftActive())
          // Remember and update zoom factor if jump was not active
          jumpBackToAircraftUpdateDistance();

        // Pass to key event handler for zooming
        e->accept(); // Do not propagate further
        event(e); // Call own event handler
        return true; // Do not process further
      }
    }
  }

  if(e->type() == QEvent::Wheel)
  {
    if(jumpBack->isActive() || isCenterLegAndAircraftActive())
      // Only delay if already active. Allow zooming and jumpback if autozoom is on
      jumpBackToAircraftStart(true /* save distance too */);

    if(!jumpBackWasActive && !isCenterLegAndAircraftActive())
      // Remember and update zoom factor if jump was not active
      jumpBackToAircraftUpdateDistance();
  }

  QMouseEvent *mEvent = dynamic_cast<QMouseEvent *>(e);
  if(mEvent != nullptr)
  {
    // Filter any obscure Marble actions around the visible globe

    if(!pointVisible(mEvent->pos()))
    {
      e->accept(); // Do not propagate further
      event(e); // Call own event handler
      return true; // Do not process further
    }
  }

  if(e->type() == QEvent::MouseButtonDblClick)
  {
    // Catch the double click event

    e->accept(); // Do not propagate further
    event(e); // Call own event handler
    return true; // Do not process further
  }

  if(e->type() == QEvent::Wheel)
  {
    // Catch the wheel event and do own zooming since Marble is buggy

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
    if(geoCoordinates(mouseEvent->pos().x(), mouseEvent->pos().y(), lon, lat, Marble::GeoDataCoordinates::Degree))
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

  if(mouseState & mw::DRAG_DISTANCE)
    // Remove new one
    getScreenIndex()->getDistanceMarks().removeAt(currentDistanceMarkerIndex);
  else if(mouseState & mw::DRAG_CHANGE_DISTANCE)
    // Replace modified one with backup
    getScreenIndex()->getDistanceMarks()[currentDistanceMarkerIndex] = distanceMarkerBackup;
  currentDistanceMarkerIndex = -1;
}

void MapWidget::startUserpointDrag(const map::MapUserpoint& userpoint, const QPoint& point)
{
  if(userpoint.isValid())
  {
    userpointDragPixmap = *NavApp::getUserdataIcons()->
                          getIconPixmap(userpoint.type, paintLayer->getMapLayer()->getUserPointSymbolSize());
    userpointDragCur = point;
    userpointDrag = userpoint;
    // Start mouse dragging and disable context menu so we can catch the right button click as cancel
    mouseState = mw::DRAG_USER_POINT;
    setContextMenuPolicy(Qt::PreventContextMenu);
  }

}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

#ifdef DEBUG_MOVING_AIRPLANE
  debugMovingPlane(event);
#endif

  qreal lon = 0., lat = 0.;
  bool visible = false;
  // Change cursor and keep aircraft from centering if moving in any drag and drop mode ================
  if(mouseState & mw::DRAG_ALL)
  {
    jumpBackToAircraftStart(true /* save distance too */);

    // Currently dragging a measurement line
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
  }

  if(mouseState & mw::DRAG_DISTANCE || mouseState & mw::DRAG_CHANGE_DISTANCE)
  {
    // Changing or adding distance measurement line ==========================================
    // Position is valid update the distance mark continuously
    if(visible && !getScreenIndexConst()->getDistanceMarks().isEmpty())
      getScreenIndex()->getDistanceMarks()[currentDistanceMarkerIndex].to = Pos(lon, lat);

  }
  else if(mouseState & mw::DRAG_ROUTE_LEG || mouseState & mw::DRAG_ROUTE_POINT)
  {
    // Dragging route leg or waypoint ==========================================
    if(visible)
      // Update current point
      routeDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState & mw::DRAG_USER_POINT)
  {
    // Moving userpoint ==========================================
    if(visible)
      // Update current point
      userpointDragCur = QPoint(event->pos().x(), event->pos().y());
  }
  else if(mouseState == mw::NONE)
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
      else if(!pointVisible(event->pos()))
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
           getScreenIndexConst()->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                            screenSearchDistance * 4 / 3) != -1 &&
           route.size() > 1)
          // Change cursor at one route point
          cursorShape = Qt::SizeAllCursor;
        else if(routeEditMode &&
                getScreenIndexConst()->getNearestRouteLegIndex(event->pos().x(), event->pos().y(),
                                                               screenSearchDistance) != -1 &&
                route.size() > 1)
          // Change cursor above a route line
          cursorShape = Qt::CrossCursor;
        else if(getScreenIndexConst()->getNearestDistanceMarkIndex(event->pos().x(), event->pos().y(),
                                                                   screenSearchDistance) != -1)
          // Change cursor at the end of an marker
          cursorShape = Qt::CrossCursor;
        else if(getScreenIndexConst()->getNearestTrafficPatternIndex(event->pos().x(), event->pos().y(),
                                                                     screenSearchDistance) != -1)
          // Change cursor at the active position
          cursorShape = Qt::PointingHandCursor;
        else if(getScreenIndexConst()->getNearestHoldIndex(event->pos().x(), event->pos().y(),
                                                           screenSearchDistance) != -1)
          // Change cursor at the active position
          cursorShape = Qt::PointingHandCursor;
        else if(getScreenIndexConst()->getNearestRangeMarkIndex(event->pos().x(), event->pos().y(),
                                                                screenSearchDistance) != -1)
          // Change cursor at the end of an marker
          cursorShape = Qt::PointingHandCursor;

        if(cursor().shape() != cursorShape)
          setCursor(cursorShape);
      }
    } // if(event->buttons() == Qt::NoButton)
    else
      // A mouse button is pressed
      jumpBackToAircraftStart(true /* save distance too */);
  }

  if(mouseState & mw::DRAG_ALL)
  {
    // Force fast updates while dragging
    setViewContext(Marble::Animation);
    update();
  }
}

void MapWidget::addMeasurement(const atools::geo::Pos& pos, const map::MapSearchResult& result)
{
  addMeasurement(pos, atools::firstOrNull(result.airports),
                 atools::firstOrNull(result.vors),
                 atools::firstOrNull(result.ndbs),
                 atools::firstOrNull(result.waypoints));
}

void MapWidget::addMeasurement(const atools::geo::Pos& pos, const map::MapAirport *airport,
                               const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint)
{
  // Distance line
  map::DistanceMarker dm;
  dm.to = pos;

  // Build distance line depending on selected airport or navaid (color, magvar, etc.)
  if(airport != nullptr && airport->isValid())
  {
    dm.text = tr("%1 (%2)").arg(airport->name).arg(airport->ident);
    dm.from = airport->position;
    dm.magvar = airport->magvar;
    dm.color = mapcolors::colorForAirport(*airport);
  }
  else if(vor != nullptr && vor->isValid())
  {
    if(vor->tacan)
      dm.text = tr("%1 %2").arg(vor->ident).arg(vor->channel);
    else
      dm.text = tr("%1 %2").arg(vor->ident).arg(QLocale().toString(vor->frequency / 1000., 'f', 2));
    dm.from = vor->position;
    dm.magvar = vor->magvar;
    dm.color = mapcolors::vorSymbolColor;
  }
  else if(ndb != nullptr && ndb->isValid())
  {
    dm.text = tr("%1 %2").arg(ndb->ident).arg(QLocale().toString(ndb->frequency / 100., 'f', 2));
    dm.from = ndb->position;
    dm.magvar = ndb->magvar;
    dm.color = mapcolors::ndbSymbolColor;
  }
  else if(waypoint != nullptr && waypoint->isValid())
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
    dm.color = mapcolors::distanceColor;
  }

  getScreenIndex()->getDistanceMarks().append(dm);

  // Start mouse dragging and disable context menu so we can catch the right button click as cancel
  mouseState = mw::DRAG_DISTANCE;
  setContextMenuPolicy(Qt::PreventContextMenu);
  currentDistanceMarkerIndex = getScreenIndexConst()->getDistanceMarks().size() - 1;
}

void MapWidget::contextMenuEvent(QContextMenuEvent *event)
{
  using atools::sql::SqlRecord;
  using atools::fs::sc::SimConnectAircraft;
  using atools::fs::sc::SimConnectUserAircraft;

  qDebug() << Q_FUNC_INFO
           << "state" << mouseState
           << "modifiers" << event->modifiers()
           << "reason" << event->reason()
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

  if(event->reason() != QContextMenuEvent::Keyboard)
    // Move menu position off the cursor to avoid accidental selection on touchpads
    menuPos += QPoint(3, 3);

  hideTooltip();

  // ==================================================================================================
  // Build a context menu with sub-menus depending on objects at the clicked position
  MapContextMenu contextMenu(mainWindow, this);

  // ==================================================================================================
  // Open menu
  if(contextMenu.exec(menuPos, point))
  {
    // Disable map updates
    contextMenuActive = false;

    // Selected action - one of the fixed or created ones
    QAction *action = contextMenu.getSelectedAction();

    // Selected map object
    const map::MapBase *base = contextMenu.getSelectedBase();

    // Global position where clicked
    atools::geo::Pos pos = contextMenu.getPos();

    // Look at fixed pre-defined actions wich are shared ============================================
    Ui::MainWindow *ui = NavApp::getMainUi();
    if(action == ui->actionMapRangeRings)
      addRangeRing(pos);
    else if(action == ui->actionMapSetMark)
      changeSearchMark(pos);
    else if(action == ui->actionMapCopyCoordinates)
    {
      QGuiApplication::clipboard()->setText(Unit::coords(pos));
      mainWindow->setStatusMessage(QString(tr("Coordinates copied to clipboard.")));
    }
    else if(action == ui->actionMapHideOneRangeRing)
      removeRangeRing(contextMenu.getRangeMarkerIndex());
    else if(action == ui->actionMapHideDistanceMarker)
      removeDistanceMarker(contextMenu.getDistMarkerIndex());
    else if(action == ui->actionMapHideTrafficPattern)
      removeTrafficPatterm(contextMenu.getTrafficPatternIndex());
    else if(action == ui->actionMapHideHold)
      removeHold(contextMenu.getHoldIndex());
    else if(action == ui->actionMapSetHome)
      changeHome();
    else
    {
      // No pre-defined action - only type available
      map::MapAirport airport = base != nullptr ? base->asObj<map::MapAirport>(map::AIRPORT) : map::MapAirport();
      map::MapVor vor = base != nullptr ? base->asObj<map::MapVor>(map::VOR) : map::MapVor();
      map::MapNdb ndb = base != nullptr ? base->asObj<map::MapNdb>(map::NDB) : map::MapNdb();
      map::MapWaypoint waypoint = base != nullptr ? base->asObj<map::MapWaypoint>(map::WAYPOINT) : map::MapWaypoint();
      map::MapUserpoint userpoint = base != nullptr ?
                                    base->asObj<map::MapUserpoint>(map::USERPOINT) : map::MapUserpoint();

      int id = base != nullptr ? base->id : -1;
      map::MapObjectTypes type = base != nullptr ? base->objType : map::NONE;

      // Create a result object as it is needed for some methods
      map::MapSearchResult result;
      result.fromMapBase(base);

      switch(contextMenu.getSelectedActionType())
      {
        case mc::NONE:
          break;

        case mc::INFORMATION:
          emit showInformation(result);
          break;

        case mc::PROCEDURE:
          emit showProcedures(airport);
          break;

        case mc::CUSTOMPROCEDURE:
          emit showProceduresCustom(airport);
          break;

        case mc::MEASURE:
          addMeasurement(pos, &airport, &vor, &ndb, &waypoint);
          break;

        case mc::NAVAIDRANGE:
          // Navaid range rings
          if(vor.isValid())
            addNavRangeRing(vor.position, map::VOR, vor.ident, vor.getFrequencyOrChannel(), vor.range);
          else if(ndb.isValid())
            addNavRangeRing(ndb.position, map::NDB, ndb.ident, QString::number(ndb.frequency), ndb.range);
          break;

        case mc::PATTERN:
          addTrafficPattern(airport);
          break;

        case mc::HOLD:
          addHold(result, pos);
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
          emit routeAdd(id, pos, type, -1 /* leg index */);
          break;

        case mc::APPENDROUTE:
          emit routeAdd(id, pos, type, map::INVALID_INDEX_VALUE);
          break;

        case mc::DELETEROUTEWAYPOINT:
          NavApp::getRouteController()->routeDelete(contextMenu.getSelectedRouteIndex());
          break;

        case mc::EDITROUTEUSERPOINT:
          NavApp::getRouteController()->editUserWaypointName(contextMenu.getSelectedRouteIndex());
          break;

        case mc::USERPOINTADD:
          if(NavApp::getElevationProvider()->isGlobeOfflineProvider())
            pos.setAltitude(atools::geo::meterToFeet(NavApp::getElevationProvider()->getElevationMeter(pos)));
          emit addUserpointFromMap(result, pos);
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
      }
    }
  }

  // Enable map updates again
  contextMenuActive = false;
}

void MapWidget::updateRoute(QPoint newPoint, int leg, int point, bool fromClickAdd, bool fromClickAppend)
{
  qDebug() << "End route drag" << newPoint << "leg" << leg << "point" << point;

  // Get all objects where the mouse button was released
  map::MapSearchResult result;
  getScreenIndexConst()->getAllNearest(newPoint.x(), newPoint.y(), screenSearchDistance, result, map::QUERY_NONE);

  // Allow only aiports for alterates
  if(point >= 0)
  {
    if(NavApp::getRouteConst().value(point).isAlternate())
      result.clear(map::MapObjectType(~map::AIRPORT));
  }

  // Count number of all objects
  int totalSize = result.size(map::AIRPORT_ALL | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT);

  int id = -1;
  map::MapObjectTypes type = map::NONE;
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
    mouseState |= mw::DRAG_POST_MENU;

    QString menuText = tr("Add %1 to Flight Plan");
    if(fromClickAdd)
      menuText = tr("Insert %1 to Flight Plan");
    else if(fromClickAppend)
      menuText = tr("Append %1 to Flight Plan");

    // Multiple entries - build a menu with icons
    showFeatureSelectionMenu(id, type, result, menuText);

    mouseState &= ~mw::DRAG_POST_MENU;
  }

  Pos pos = atools::geo::EMPTY_POS;
  if(type == map::USERPOINTROUTE)
    // Get position for new user point from from screen
    pos = CoordinateConverter(viewport()).sToW(newPoint.x(), newPoint.y());

  if((id != -1 && type != map::NONE) || type == map::USERPOINTROUTE)
  {
    if(fromClickAdd)
      emit routeAdd(id, pos, type, -1 /* leg index */);
    else if(fromClickAppend)
      emit routeAdd(id, pos, type, map::INVALID_INDEX_VALUE);
    else
    {
      // From drag
      if(leg != -1)
        emit routeAdd(id, pos, type, leg);
      else if(point != -1)
        emit routeReplace(id, pos, type, point);
    }
  }
}

bool MapWidget::showFeatureSelectionMenu(int& id, map::MapObjectTypes& type, const map::MapSearchResult& result,
                                         const QString& menuText)
{
  // Add id and type to actions
  const int ICON_SIZE = 20;
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  SymbolPainter symbolPainter;

  for(const map::MapAirport& obj : result.airports)
  {
    QAction *action = new QAction(symbolPainter.createAirportIcon(obj, ICON_SIZE),
                                  menuText.arg(map::airportText(obj, 20)), this);
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
                                  menuText.arg(map::vorText(obj)), this);
    action->setData(QVariantList({obj.id, map::VOR}));
    menu.addAction(action);
  }
  for(const map::MapNdb& obj : result.ndbs)
  {
    QAction *action = new QAction(symbolPainter.createNdbIcon(ICON_SIZE),
                                  menuText.arg(map::ndbText(obj)), this);
    action->setData(QVariantList({obj.id, map::NDB}));
    menu.addAction(action);
  }
  for(const map::MapWaypoint& obj : result.waypoints)
  {
    QAction *action = new QAction(symbolPainter.createWaypointIcon(ICON_SIZE),
                                  menuText.arg(map::waypointText(obj)), this);
    action->setData(QVariantList({obj.id, map::WAYPOINT}));
    menu.addAction(action);
  }

  int numUserpoints = 0;
  for(const map::MapUserpoint& obj : result.userpoints)
  {
    QAction *action = nullptr;
    if(numUserpoints > 5)
    {
      action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE), tr("More ..."), this);
      action->setDisabled(true);
      menu.addAction(action);
      break;
    }
    else
    {
      action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                           menuText.arg(map::userpointText(obj)), this);
      action->setData(QVariantList({obj.id, map::USERPOINT}));
      menu.addAction(action);
    }
    numUserpoints++;
  }

  // Always present - userpoint
  menu.addSeparator();
  {
    QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                  menuText.arg(tr("Position")), this);
    action->setData(QVariantList({-1, map::USERPOINTROUTE}));
    menu.addAction(action);
  }

  // Always present - cancel
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
    return true;
  }
  return false;
}

void MapWidget::simDataCalcFuelOnOff(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                     const atools::fs::sc::SimConnectUserAircraft& last)
{
  // Engine start and shutdown events ===================================
  if(last.isValid() && aircraft.isValid() &&
     !aircraft.isSimPaused() && !aircraft.isSimReplay() &&
     !last.isSimPaused() && !last.isSimReplay())
  {
    // start timer to emit takeoff/landing signal
    if(last.hasFuelFlow() != aircraft.hasFuelFlow())
      fuelOnOffTimer.start(FUEL_ON_OFF_TIMEOUT);
  }
}

void MapWidget::simDataCalcTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                          const atools::fs::sc::SimConnectUserAircraft& last)
{
  // ========================================================
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
      takeoffLandingTimer.start(aircraft.isFlying() ? TAKEOFF_TIMEOUT : LANDING_TIMEOUT);
  }
}

void MapWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  const atools::fs::sc::SimConnectUserAircraft& aircraft = simulatorData.getUserAircraftConst();
  if(databaseLoadStatus || !aircraft.isValid())
    return;

  if(NavApp::getMainUi()->actionMapShowSunShadingSimulatorTime->isChecked())
    // Update sun shade on globe with simulator time
    setSunShadingDateTime(aircraft.getZuluTime());

  getScreenIndex()->updateSimData(simulatorData);
  const atools::fs::sc::SimConnectUserAircraft& last = getScreenIndexConst()->getLastUserAircraft();

  simDataCalcTakeoffLanding(aircraft, last);
  simDataCalcFuelOnOff(aircraft, last);

  // map::MapRunwayEnd runwayEnd;
  // map::MapAirport airport;
  // NavApp::getAirportQuerySim()->getBestRunwayEndForPosAndCourse(runwayEnd, airport,
  // aircraft.getPosition(), aircraft.getTrackDegTrue());

  // Create screen coordinates =============================
  CoordinateConverter conv(viewport());
  bool curPosVisible = false;
  QPoint curPoint = conv.wToS(aircraft.getPosition(), CoordinateConverter::DEFAULT_WTOS_SIZE, &curPosVisible);
  QPoint diff = curPoint - conv.wToS(last.getPosition());
  const OptionData& od = OptionData::instance();
  QRect widgetRect = rect();

  // Used to check if objects are still visible
  QRect widgetRectSmall = widgetRect.adjusted(10, 10, -10, -10);
  curPosVisible = widgetRectSmall.contains(curPoint);

  bool wasEmpty = aircraftTrack->isEmpty();
#ifdef DEBUG_INFORMATION_DISABLED
  qDebug() << "curPos" << curPos;
  qDebug() << "widgetRectSmall" << widgetRectSmall;
#endif

  if(aircraftTrack->appendTrackPos(aircraft.getPosition(), aircraft.getZuluTime(), aircraft.isOnGround()))
    emit aircraftTrackPruned();

  if(wasEmpty != aircraftTrack->isEmpty())
    // We have a track - update toolbar and menu
    emit updateActionStates();

  // ================================================================================
  // Update tooltip for bearing
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  if(now - lastSimUpdateTooltipMs > MAX_SIM_UPDATE_TOOLTIP_MS)
  {
    lastSimUpdateTooltipMs = now;

    // Update tooltip if it has bearing/distance fields
    if((mapSearchResultTooltip.hasAirports() || mapSearchResultTooltip.hasVor() || mapSearchResultTooltip.hasNdb() ||
        mapSearchResultTooltip.hasWaypoints() || mapSearchResultTooltip.hasIls() ||
        mapSearchResultTooltip.hasUserpoints()) && NavApp::isConnectedAndAircraft())
    {
      updateTooltip();
    }
  }

  // ================================================================================
  // Check if screen has to be updated/scrolled/zoomed
  if(paintLayer->getShownMapObjects() & map::AIRCRAFT ||
     paintLayer->getShownMapObjects() & map::AIRCRAFT_AI ||
     paintLayer->getShownMapObjects() & map::AIRCRAFT_ONLINE)
  {
    // Show aircraft is enabled
    bool centerAircraft = mainWindow->getUi()->actionMapAircraftCenter->isChecked();

    // Get delta values for update rate
    const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(od.getSimUpdateRate());

    // Limit number of updates per second =================================================
    if(now - lastSimUpdateMs > deltas.timeDeltaMs)
    {
      lastSimUpdateMs = now;

      // Check if any AI aircraft are visible
      bool aiVisible = false;
      if(paintLayer->getShownMapObjects() & map::AIRCRAFT_AI ||
         paintLayer->getShownMapObjects() & map::AIRCRAFT_AI_SHIP ||
         paintLayer->getShownMapObjects() & map::AIRCRAFT_ONLINE)
      {
        for(const atools::fs::sc::SimConnectAircraft& ai : simulatorData.getAiAircraftConst())
        {
          if(getCurrentViewBoundingBox().contains(
               Marble::GeoDataCoordinates(ai.getPosition().getLonX(), ai.getPosition().getLatY(), 0,
                                          Marble::GeoDataCoordinates::Degree)))
          {
            aiVisible = true;
            break;
          }
        }
      }

      using atools::almostNotEqual;

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
        getScreenIndex()->updateLastSimData(simulatorData);

      // Option to udpate always
      bool updateAlways = od.getFlags() & opts::SIM_UPDATE_MAP_CONSTANTLY;

      // Check if centering of leg is reqired =======================================
      const Route& route = NavApp::getRouteConst();
      const RouteLeg *activeLeg = route.getActiveLeg();
      bool centerAircraftAndLeg = isCenterLegAndAircraftActive();

      // Get position of next waypoint and check visibility
      Pos nextWpPos;
      QPoint nextWpPoint;
      bool nextWpPosVisible = false;
      if(centerAircraftAndLeg)
      {
        nextWpPos = activeLeg != nullptr ? route.getActiveLeg()->getPosition() : Pos();
        nextWpPoint = conv.wToS(nextWpPos, CoordinateConverter::DEFAULT_WTOS_SIZE, &nextWpPosVisible);
        nextWpPosVisible = widgetRectSmall.contains(nextWpPoint);
      }

      if(centerAircraft && !contextMenuActive) // centering required by button but not while menu is open
      {
        if(!curPosVisible || // Not visible on world map
           posHasChanged) // Significant change in position might require zooming or re-centering
        {
          // Do not update if user is using drag and drop or scrolling around
          // No updates while jump back is active and user is moving around
          if(mouseState == mw::NONE && viewContext() == Marble::Still && !jumpBack->isActive())
          {
            if(centerAircraftAndLeg)
            {
              // Update four times based on flying time to next waypoint - this is recursive
              // and will update more often close to the wp
              int timeToWpUpdateMs =
                std::max(static_cast<int>(atools::geo::meterToNm(aircraft.getPosition().distanceMeterTo(nextWpPos)) /
                                          (aircraft.getGroundSpeedKts() + 1.f) * 3600.f / 4.f), 4) * 1000;

              // Zoom to rectangle every 15 seconds
              bool zoomToRect = now - lastCenterAcAndWp > timeToWpUpdateMs;

#ifdef DEBUG_INFORMATION_SIMUPDATE
              qDebug() << Q_FUNC_INFO;
              qDebug() << "curPosVisible" << curPosVisible;
              qDebug() << "nextWpPosVisible" << nextWpPosVisible;
              qDebug() << "updateAlways" << updateAlways;
              qDebug() << "zoomToRect" << zoomToRect;
#endif
              if(!curPosVisible || !nextWpPosVisible || updateAlways || zoomToRect)
              {
                // Wait 15 seconds after every update
                lastCenterAcAndWp = now;

                // Postpone screen updates
                setUpdatesEnabled(false);

                atools::geo::Rect rect(nextWpPos);
                rect.extend(aircraft.getPosition());

                if(rect.getWidthDegree() > 180.f || rect.getHeightDegree() > 180.f)
                  rect = atools::geo::Rect(nextWpPos);

#ifdef DEBUG_INFORMATION_SIMUPDATE
                qDebug() << Q_FUNC_INFO;
                qDebug() << "curPoint" << curPoint;
                qDebug() << "nextWpPoint" << nextWpPoint;
                qDebug() << "widgetRect" << widgetRect;
                qDebug() << "ac.getPosition()" << aircraft.getPosition();
                qDebug() << "rect" << rect;
#endif

                if(!rect.isPoint(POS_IS_POINT_EPSILON))
                {
                  centerRectOnMap(rect);

                  float altToZoom = aircraft.getAltitudeAboveGroundFt() > 12000.f ? 1400.f : 2800.f;
                  // Minimum zoom depends on flight altitude
                  float minZoomDist = atools::geo::nmToKm(
                    std::min(std::max(aircraft.getAltitudeAboveGroundFt() / altToZoom, 0.4f), 28.f));

                  if(distance() < minZoomDist)
                  {
#ifdef DEBUG_INFORMATION
                    qDebug() << Q_FUNC_INFO << "distance() < minZoom" << distance() << "<" << minZoomDist;
#endif
                    // Correct zoom for minimum distance
                    setDistanceToMap(minZoomDist);
#ifdef DEBUG_INFORMATION
                    qDebug() << Q_FUNC_INFO << "zoom()" << zoom();
#endif
                  }
                }
                else if(rect.isValid())
                  centerPosOnMap(aircraft.getPosition());
              } // if(!curPosVisible || !nextWpPosVisible || updateAlways || rectTooSmall || !centered)
            } // if(centerAircraftAndLeg)
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
                centerPosOnMap(aircraft.getPosition());
              }
            }
          }
        }
      }

      if(!updatesEnabled())
        setUpdatesEnabled(true);

      if((dataHasChanged || aiVisible) && !contextMenuActive)
        // Not scrolled or zoomed but needs a redraw
        update();
    }
  }
  else if(paintLayer->getShownMapObjects() & map::AIRCRAFT_TRACK)
  {
    // No aircraft but track - update track only
    if(!last.isValid() || diff.manhattanLength() > 4)
    {
      getScreenIndex()->updateLastSimData(simulatorData);

      if(!contextMenuActive)
        update();
    }
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
      setDistanceToMap(DEFAULT_MAP_DISTANCE, true /* Allow adjust zoom */);
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
  for(const QString& name : mapOverlays.keys())
  {
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

void MapWidget::overlayStateToMenu()
{
  qDebug() << Q_FUNC_INFO << "ignoreOverlayUpdates" << ignoreOverlayUpdates;
  if(!ignoreOverlayUpdates)
  {
    for(const QString& name : mapOverlays.keys())
    {
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
    for(const QString& name : mapOverlays.keys())
    {
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
  for(QAction *action : mapOverlays.values())
    connect(action, &QAction::toggled, this, &MapWidget::overlayStateFromMenu);

  for(const QString& name : mapOverlays.keys())
  {
    Marble::AbstractFloatItem *overlay = floatItem(name);
    if(overlay != nullptr)
      connect(overlay, &Marble::AbstractFloatItem::visibilityChanged, this, &MapWidget::overlayStateToMenu);
  }
}

bool MapWidget::isCenterLegAndAircraftActive()
{
  const Route& route = NavApp::getRouteConst();
  return OptionData::instance().getFlags2() & opts2::ROUTE_AUTOZOOM && // Waypoint and aircraft center enabled
         !route.isEmpty() && // Have a route
         route.getActiveLegIndex() < map::INVALID_INDEX_VALUE && // Active leg present - special case 0 for one waypoint only
         getScreenIndexConst()->getUserAircraft().isFlying() && // Aircraft in air
         route.getDistanceToFlightPlan() < MAX_FLIGHT_PLAN_DIST_FOR_CENTER_NM; // not too far away from flight plan
}

void MapWidget::optionsChanged()
{
  screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  screenSearchDistanceTooltip = OptionData::instance().getMapTooltipSensitivity();
  MapPaintWidget::optionsChanged();
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

  // Mark coordinates
  s.setValue(lnm::MAP_MARKLONX, searchMarkPos.getLonX());
  s.setValue(lnm::MAP_MARKLATY, searchMarkPos.getLatY());

  // Home coordinates and zoom
  s.setValue(lnm::MAP_HOMELONX, homePos.getLonX());
  s.setValue(lnm::MAP_HOMELATY, homePos.getLatY());
  s.setValue(lnm::MAP_HOMEDISTANCE, homeDistance);

  s.setValue(lnm::MAP_KMLFILES, kmlFilePaths);
  s.setValue(lnm::MAP_DETAILFACTOR, mapDetailLevel);
  s.setValueVar(lnm::MAP_AIRSPACES, QVariant::fromValue(paintLayer->getShownAirspaces()));

  // Sun shading settings =====================================
  s.setValue(lnm::MAP_SUN_SHADING_TIME_OPTION, paintLayer->getSunShading());

  // Weather source settings =====================================
  s.setValue(lnm::MAP_WEATHER_SOURCE, paintLayer->getWeatherSource());

  history.saveState(atools::settings::Settings::getConfigFilename(".history"));
  getScreenIndexConst()->saveState();
  aircraftTrack->saveState();

  overlayStateToMenu();
  atools::gui::WidgetState state(lnm::MAP_OVERLAY_VISIBLE, false /*save visibility*/, true /*block signals*/);
  for(QAction *action : mapOverlays.values())
    state.save(action);
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

  // Sun shading settings ========================================
  map::MapSunShading sunShading =
    static_cast<map::MapSunShading>(s.valueInt(lnm::MAP_SUN_SHADING_TIME_OPTION, map::SUN_SHADING_SIMULATOR_TIME));

  if(sunShading == map::SUN_SHADING_USER_TIME)
    sunShading = map::SUN_SHADING_SIMULATOR_TIME;

  sunShadingToUi(sunShading);
  paintLayer->setSunShading(sunShading);

  // Weather source settings ========================================
  map::MapWeatherSource weatherSource =
    static_cast<map::MapWeatherSource>(s.valueInt(lnm::MAP_WEATHER_SOURCE, map::WEATHER_SOURCE_SIMULATOR));
  weatherSourceToUi(weatherSource);
  paintLayer->setWeatherSource(weatherSource);

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
  getScreenIndex()->restoreState();

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_TRAIL)
    aircraftTrack->restoreState();
  aircraftTrack->setMaxTrackEntries(OptionData::instance().getAircraftTrackMaxPoints());

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

  history.restoreState(atools::settings::Settings::getConfigFilename(".history"));
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
                                             tr("Do not show this dialog again."));
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
  if(ui->actionMapShowWeatherSimulator->isChecked())
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

  atools::gui::SignalBlocker blocker({ui->actionMapShowAirports, ui->actionMapShowSoftAirports,
                                      ui->actionMapShowEmptyAirports, ui->actionMapShowAddonAirports,
                                      ui->actionMapShowVor, ui->actionMapShowNdb, ui->actionMapShowWp,
                                      ui->actionMapShowIls, ui->actionMapShowVictorAirways, ui->actionMapShowJetAirways,
                                      ui->actionMapShowTracks, ui->actionShowAirspaces, ui->actionMapShowRoute,
                                      ui->actionMapShowTocTod, ui->actionMapShowAircraft, ui->actionMapShowCompassRose,
                                      ui->actionMapShowCompassRoseAttach, ui->actionMapAircraftCenter,
                                      ui->actionMapShowAircraftAi, ui->actionMapShowAircraftAiBoat,
                                      ui->actionMapShowAircraftTrack, ui->actionInfoApproachShowMissedAppr,
                                      ui->actionMapShowGrid, ui->actionMapShowCities, ui->actionMapShowHillshading,
                                      ui->actionMapShowMinimumAltitude, ui->actionMapShowAirportWeather,
                                      ui->actionMapShowSunShading});

  // Menu map =====================================
  ui->actionMapAircraftCenter->setChecked(true);

  // Menu view =====================================
  // Submenu airports
  ui->actionMapShowAirports->setChecked(true);
  ui->actionMapShowSoftAirports->setChecked(true);
  ui->actionMapShowEmptyAirports->setChecked(true);
  ui->actionMapShowAddonAirports->setChecked(true);

  // Submenu navaids
  ui->actionMapShowVor->setChecked(true);
  ui->actionMapShowNdb->setChecked(true);
  ui->actionMapShowWp->setChecked(true);
  ui->actionMapShowIls->setChecked(true);
  ui->actionMapShowVictorAirways->setChecked(false);
  ui->actionMapShowJetAirways->setChecked(false);
  ui->actionMapShowTracks->setChecked(false);

  // Submenu airspaces
  ui->actionShowAirspaces->setChecked(true);

  // -----------------
  ui->actionMapShowRoute->setChecked(true);
  ui->actionMapShowTocTod->setChecked(true);
  ui->actionInfoApproachShowMissedAppr->setChecked(true);
  ui->actionMapShowAircraft->setChecked(true);
  ui->actionMapShowAircraftTrack->setChecked(true);
  ui->actionMapShowCompassRose->setChecked(false);
  ui->actionMapShowCompassRoseAttach->setChecked(true);

  // -----------------
  ui->actionMapShowAircraftAi->setChecked(true);
  ui->actionMapShowAircraftAiBoat->setChecked(false);

  // -----------------
  ui->actionMapShowGrid->setChecked(true);
  ui->actionMapShowCities->setChecked(true);
  ui->actionMapShowHillshading->setChecked(true);
  ui->actionMapShowMinimumAltitude->setChecked(true);

  // -----------------
  ui->actionMapShowAirportWeather->setChecked(true);
  // Weather sources unmodified

  // -----------------
  ui->actionMapShowSunShading->setChecked(false);
  // Sun shading data unmodified
}

void MapWidget::updateThemeUi(int index)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  if(index >= map::CUSTOM)
  {
    // Enable all buttons for custom maps
    ui->actionMapShowCities->setEnabled(true);
    ui->actionMapShowHillshading->setEnabled(true);
    ui->actionMapShowSunShading->setEnabled(true);
  }
  else
  {
    // Update theme specific options
    switch(index)
    {
      case map::STAMENTERRAIN:
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(false);
        ui->actionMapShowSunShading->setEnabled(true);
        break;

      case map::OPENTOPOMAP:
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(false);
        ui->actionMapShowSunShading->setEnabled(true);
        break;

      case map::OPENSTREETMAP:
      case map::CARTODARK:
      case map::CARTOLIGHT:
        ui->actionMapShowCities->setEnabled(false);
        ui->actionMapShowHillshading->setEnabled(true);
        ui->actionMapShowSunShading->setEnabled(true);
        break;

      case map::SIMPLE:
      case map::PLAIN:
      case map::ATLAS:
        ui->actionMapShowCities->setEnabled(true);
        ui->actionMapShowHillshading->setEnabled(false);
        ui->actionMapShowSunShading->setEnabled(false);
        break;
      case map::INVALID:
        qWarning() << "Invalid theme index" << index;
        break;
    }
  }
}

void MapWidget::updateMapVisibleUi() const
{
  mapVisible->updateVisibleObjectsStatusBar();
}

void MapWidget::updateDetailUi(int mapDetails)
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->actionMapMoreDetails->setEnabled(mapDetails < MapLayerSettings::MAP_MAX_DETAIL_FACTOR);
  ui->actionMapLessDetails->setEnabled(mapDetails > MapLayerSettings::MAP_MIN_DETAIL_FACTOR);
  ui->actionMapDefaultDetails->setEnabled(mapDetails != MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR);
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
  Ui::MainWindow *ui = mainWindow->getUi();

  // Sun shading ====================================================
  setShowMapSunShading(ui->actionMapShowSunShading->isChecked() &&
                       currentThemeIndex != map::SIMPLE && currentThemeIndex != map::PLAIN
                       && currentThemeIndex != map::ATLAS);
  paintLayer->setSunShading(sunShadingFromUi());

  // Weather source ====================================================
  paintLayer->setWeatherSource(weatherSourceFromUi());

  // Other map features ====================================================
  setShowMapPois(ui->actionMapShowCities->isChecked() &&
                 (currentThemeIndex == map::SIMPLE || currentThemeIndex == map::PLAIN
                  || currentThemeIndex == map::ATLAS));
  setShowGrid(ui->actionMapShowGrid->isChecked());

  // Need to keep track of hillshading separately since Marble has not getter
  hillshading = ui->actionMapShowHillshading->isChecked() &&
                (currentThemeIndex == map::OPENSTREETMAP ||
                 currentThemeIndex == map::CARTODARK ||
                 currentThemeIndex == map::CARTOLIGHT ||
                 currentThemeIndex >= map::CUSTOM);
  setPropertyValue("hillshading", hillshading);

  setShowMapFeatures(map::AIRWAYV, ui->actionMapShowVictorAirways->isChecked());
  setShowMapFeatures(map::AIRWAYJ, ui->actionMapShowJetAirways->isChecked());
  setShowMapFeatures(map::TRACK, ui->actionMapShowTracks->isChecked());

  setShowMapFeatures(map::AIRSPACE, getShownAirspaces().flags & map::AIRSPACE_ALL &&
                     ui->actionShowAirspaces->isChecked());

  setShowMapFeaturesDisplay(map::FLIGHTPLAN, ui->actionMapShowRoute->isChecked());
  setShowMapFeaturesDisplay(map::FLIGHTPLAN_TOC_TOD, ui->actionMapShowTocTod->isChecked());
  setShowMapFeatures(map::MISSED_APPROACH, ui->actionInfoApproachShowMissedAppr->isChecked());

  setShowMapFeaturesDisplay(map::COMPASS_ROSE, ui->actionMapShowCompassRose->isChecked());
  setShowMapFeaturesDisplay(map::COMPASS_ROSE_ATTACH, ui->actionMapShowCompassRoseAttach->isChecked());
  setShowMapFeatures(map::AIRCRAFT, ui->actionMapShowAircraft->isChecked());
  setShowMapFeatures(map::AIRCRAFT_TRACK, ui->actionMapShowAircraftTrack->isChecked());
  setShowMapFeatures(map::AIRCRAFT_AI, ui->actionMapShowAircraftAi->isChecked());
  setShowMapFeatures(map::AIRCRAFT_AI_SHIP, ui->actionMapShowAircraftAiBoat->isChecked());

  setShowMapFeatures(map::AIRPORT_HARD, ui->actionMapShowAirports->isChecked());
  setShowMapFeatures(map::AIRPORT_SOFT, ui->actionMapShowSoftAirports->isChecked());

  // Display types which are not used in structs
  setShowMapFeaturesDisplay(map::AIRPORT_WEATHER, ui->actionMapShowAirportWeather->isChecked());
  setShowMapFeaturesDisplay(map::MINIMUM_ALTITUDE, ui->actionMapShowMinimumAltitude->isChecked());
  setShowMapFeaturesDisplay(map::WIND_BARBS, NavApp::getWindReporter()->isWindShown());
  setShowMapFeaturesDisplay(map::WIND_BARBS_ROUTE, NavApp::getWindReporter()->isRouteWindShown());

  setShowMapFeaturesDisplay(map::LOGBOOK_DIRECT, NavApp::getLogdataController()->isDirectPreviewShown());
  setShowMapFeaturesDisplay(map::LOGBOOK_ROUTE, NavApp::getLogdataController()->isRoutePreviewShown());
  setShowMapFeaturesDisplay(map::LOGBOOK_TRACK, NavApp::getLogdataController()->isTrackPreviewShown());

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
                      appendFieldAndValue("departure_ident", logEntry.departureIdent).
                      appendFieldAndValue("destination_ident", logEntry.destinationIdent).
                      appendFieldAndValue("simulator", logEntry.simulator).
                      appendFieldAndValue("aircraft_type", logEntry.aircraftType).
                      appendFieldAndValue("aircraft_registration", logEntry.aircraftRegistration),
                      true /* select */);
  }
  else if(base->objType == map::USERPOINT)
  {
    map::MapUserpoint userpoint = base->asObj<map::MapUserpoint>();

    SqlRecord rec;
    if(!userpoint.ident.isEmpty())
      rec.appendFieldAndValue("ident", userpoint.ident);
    if(!userpoint.region.isEmpty())
      rec.appendFieldAndValue("region", userpoint.region);
    if(!userpoint.name.isEmpty())
      rec.appendFieldAndValue("name", userpoint.name);
    if(!userpoint.type.isEmpty())
      rec.appendFieldAndValue("type", userpoint.type);
    if(!userpoint.tags.isEmpty())
      rec.appendFieldAndValue("tags", userpoint.tags);

    emit showInSearch(map::USERPOINT, rec, true /* select */);
  }
  else if(base->objType == map::AIRPORT)
    emit showInSearch(map::AIRPORT, SqlRecord().appendFieldAndValue("ident", base->asObj<map::MapAirport>().ident),
                      true /* select */);
  else if(base->objType == map::VOR)
  {
    map::MapVor vor = base->asObj<map::MapVor>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", vor.ident);
    if(!vor.region.isEmpty())
      rec.appendFieldAndValue("region", vor.region);

    emit showInSearch(map::VOR, rec, true /* select */);
  }
  else if(base->objType == map::NDB)
  {
    map::MapNdb ndb = base->asObj<map::MapNdb>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", ndb.ident);
    if(!ndb.region.isEmpty())
      rec.appendFieldAndValue("region", ndb.region);

    emit showInSearch(map::NDB, rec, true /* select */);
  }
  else if(base->objType == map::WAYPOINT)
  {
    map::MapWaypoint waypoint = base->asObj<map::MapWaypoint>();
    SqlRecord rec;
    rec.appendFieldAndValue("ident", waypoint.ident);
    if(!waypoint.region.isEmpty())
      rec.appendFieldAndValue("region", waypoint.region);

    emit showInSearch(map::WAYPOINT, rec, true /* select */);
  }
  else if(base->objType == map::AIRCRAFT)
  {
    // Can only show online clients if aircraft is shadow of a client
    map::MapUserAircraft aircraft = base->asObj<map::MapUserAircraft>();
    atools::fs::sc::SimConnectAircraft shadowAircraft;
    NavApp::getOnlinedataController()->getShadowAircraft(shadowAircraft, aircraft.getAircraft());
    emit showInSearch(map::AIRCRAFT_ONLINE,
                      SqlRecord().appendFieldAndValue("callsign", shadowAircraft.getAirplaneRegistration()),
                      true /* select */);
  }
  else if(base->objType == map::AIRCRAFT_AI)
  {
    // Can only show online clients if aircraft is shadow of a client
    map::MapAiAircraft aircraft = base->asObj<map::MapAiAircraft>();
    atools::fs::sc::SimConnectAircraft shadowAircraft;
    NavApp::getOnlinedataController()->getShadowAircraft(shadowAircraft, aircraft.getAircraft());
    emit showInSearch(map::AIRCRAFT_ONLINE,
                      SqlRecord().appendFieldAndValue("callsign", shadowAircraft.getAirplaneRegistration()),
                      true /* select */);
  }
  else if(base->objType == map::AIRCRAFT_ONLINE)
    emit showInSearch(map::AIRCRAFT_ONLINE,
                      SqlRecord().appendFieldAndValue("callsign",
                                                      base->asObj<map::MapOnlineAircraft>().
                                                      getAircraft().getAirplaneRegistration()),
                      true /* select */);
  else if(base->objType == map::AIRSPACE)
    emit showInSearch(map::AIRSPACE,
                      SqlRecord().appendFieldAndValue("callsign",
                                                      base->asObj<map::MapAirspace>().name), true /* select */);
}

void MapWidget::addTrafficPattern(const map::MapAirport& airport)
{
  qDebug() << Q_FUNC_INFO;

  TrafficPatternDialog dialog(mainWindow, airport);
  int retval = dialog.exec();
  if(retval == QDialog::Accepted)
  {
    map::TrafficPattern pattern;
    dialog.fillTrafficPattern(pattern);
    getTrafficPatterns().append(pattern);
    mainWindow->updateMarkActionStates();
    update();
    mainWindow->setStatusMessage(tr("Added airport traffic pattern for %1.").arg(airport.ident));
  }
}

void MapWidget::removeTrafficPatterm(int index)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->getTrafficPatterns().removeAt(index);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Traffic pattern removed from map.")));
}

void MapWidget::addHold(const map::MapSearchResult& result, const atools::geo::Pos& position)
{
  qDebug() << Q_FUNC_INFO;

  // Order of preference (same as in map context menu): airport, vor, ndb, waypoint, pos
  HoldDialog dialog(mainWindow, result, position);
  if(dialog.exec() == QDialog::Accepted)
  {
    map::Hold hold;
    dialog.fillHold(hold);

    getHolds().append(hold);

    mainWindow->updateMarkActionStates();

    update();
    mainWindow->setStatusMessage(tr("Added hold."));
  }
}

void MapWidget::removeHold(int index)
{
  qDebug() << Q_FUNC_INFO;

  getScreenIndex()->getHolds().removeAt(index);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Holding removed from map.")));
}

void MapWidget::resetSettingsToDefault()
{
  paintLayer->setShowAirspaces({map::AIRSPACE_DEFAULT, map::AIRSPACE_FLAG_DEFAULT});
  mapDetailLevel = MapLayerSettings::MAP_DEFAULT_DETAIL_FACTOR;
  setMapDetail(mapDetailLevel);
}

void MapWidget::showSearchMark()
{
  qDebug() << "NavMapWidget::showMark" << searchMarkPos;

  hideTooltip();
  showAircraft(false);

  if(searchMarkPos.isValid())
  {
    jumpBackToAircraftStart(true /* save distance too */);
    centerPosOnMap(searchMarkPos);
    setDistanceToMap(atools::geo::nmToKm(Unit::rev(OptionData::instance().getMapZoomShowMenu(), Unit::distNmF)));
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showHome()
{
  qDebug() << Q_FUNC_INFO << homePos;

  hideTooltip();
  jumpBackToAircraftStart(true /* save distance too */);
  showAircraft(false);
  if(!atools::almostEqual(homeDistance, 0.))
    // Only center position is valid - Do not fix zoom - display as is
    setDistanceToMap(homeDistance, false /* Allow adjust zoom */);

  if(homePos.isValid())
  {
    jumpBackToAircraftStart(true /* save distance too */);
    centerPosOnMap(homePos);
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

void MapWidget::addNavRangeRing(const atools::geo::Pos& pos, map::MapObjectTypes type,
                                const QString& ident, const QString& frequency, int range)
{
  map::RangeMarker ring;
  ring.type = type;
  ring.position = pos;

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
  getScreenIndex()->getRangeMarks().append(ring);
  qDebug() << "navaid range" << ring.position;

  update();
  mainWindow->updateMarkActionStates();
  mainWindow->setStatusMessage(tr("Added range rings for %1.").arg(ident));
}

void MapWidget::addRangeRing(const atools::geo::Pos& pos)
{
  map::RangeMarker rings;
  rings.type = map::NONE;
  rings.position = pos;

  const QVector<int> dists = OptionData::instance().getMapRangeRings();
  for(int dist : dists)
    rings.ranges.append(atools::roundToInt(Unit::rev(dist, Unit::distNmF)));

  getScreenIndex()->getRangeMarks().append(rings);

  qDebug() << "range rings" << rings.position;
  update();
  mainWindow->updateMarkActionStates();
  mainWindow->setStatusMessage(tr("Added range rings for position."));
}

void MapWidget::workOffline(bool offline)
{
  qDebug() << "Work offline" << offline;
  model()->setWorkOffline(offline);

  mainWindow->renderStatusUpdateLabel(Marble::RenderStatus::Complete, true /* forceUpdate */);

  if(!offline)
  {
    reloadMap();
    update();
  }
}

void MapWidget::zoomInOut(bool directionIn, bool smooth)
{
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
    if(currentThemeIndex == map::PLAIN || currentThemeIndex == map::SIMPLE)
    {
      if(directionIn)
        zoomViewBy(zoomStep() * 3);
      else
        zoomViewBy(-zoomStep() * 3);
    }
    else
    {
      if(directionIn)
        zoomIn();
      else
        zoomOut();
    }
  }
}

void MapWidget::removeRangeRing(int index)
{
  getScreenIndex()->getRangeMarks().removeAt(index);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Range ring removed from map.")));
}

void MapWidget::removeDistanceMarker(int index)
{
  getScreenIndex()->getDistanceMarks().removeAt(index);
  mainWindow->updateMarkActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Measurement line removed from map.")));
}

void MapWidget::setMapDetail(int factor)
{
  mapDetailLevel = factor;
  setDetailLevel(mapDetailLevel);
  updateDetailUi(mapDetailLevel);
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

void MapWidget::clearRangeRingsAndDistanceMarkers()
{
  qDebug() << "range rings hide";

  getScreenIndex()->getRangeMarks().clear();
  getScreenIndex()->getDistanceMarks().clear();
  getScreenIndex()->getTrafficPatterns().clear();
  getScreenIndex()->getHolds().clear();
  currentDistanceMarkerIndex = -1;

  update();
  mainWindow->updateMarkActionStates();
  mainWindow->setStatusMessage(tr("All range rings and measurement lines removed from map."));
}

void MapWidget::deleteAircraftTrack()
{
  aircraftTrack->clearTrack();
  emit updateActionStates();
  update();
}

void MapWidget::setDetailLevel(int factor)
{
  qDebug() << "setDetailFactor" << factor;

  if(factor != paintLayer->getDetailFactor())
  {
    paintLayer->setDetailFactor(factor);
    updateMapVisibleUi();
    getScreenIndex()->updateAllGeometry(getCurrentViewBoundingBox());
  }
}

#ifdef DEBUG_MOVING_AIRPLANE
void MapWidget::debugMovingPlane(QMouseEvent *event)
{
  using atools::fs::sc::SimConnectData;
  static int packetId = 0;
  static Pos lastPos;
  static QPoint lastPoint;

  if(event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
  {
    if(QPoint(lastPoint - event->pos()).manhattanLength() > 4)
    {
      qreal lon, lat;
      geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
      Pos pos(lon, lat);

      const Route& route = NavApp::getRouteConst();
      float projectionDistance = route.getProjectionDistance();

      if(!(projectionDistance < map::INVALID_DISTANCE_VALUE))
        projectionDistance = route.getDistanceFromStart(pos);

      float alt = 0.f;
      if(projectionDistance < map::INVALID_DISTANCE_VALUE)
        alt = NavApp::getAltitudeLegs().getAltitudeForDistance(route.getTotalDistance() - projectionDistance);

      const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerformance();
      bool ground = false;
      float vertSpeed = 0.f, tas = 0.f, fuelflow = 0.f, totalFuel = 1000.f;

      if(route.size() <= 2)
      {
        alt = NavApp::getRouteController()->getCruiseAltitudeWidget();
        tas = perf.getCruiseSpeed();
        fuelflow = perf.getCruiseFuelFlowLbs();
      }
      else
      {
        float distanceFromStart = route.getDistanceFromStart(pos);
        ground = distanceFromStart<0.5f || distanceFromStart> route.getTotalDistance() - 0.5f;

        if(route.isActiveAlternate() || route.isActiveMissed())
          ground = false;

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
            float tocDist = route.getTopOfClimbDistance();
            float todDist = route.getTopOfDescentDistance();

            tas = perf.getCruiseSpeed();
            fuelflow = perf.getCruiseFuelFlowLbs();

            if(projectionDistance < tocDist)
            {
              vertSpeed = perf.getClimbVertSpeed();
              tas = perf.getClimbSpeed();
              fuelflow = perf.getClimbFuelFlowLbs();
            }
            if(projectionDistance > todDist)
            {
              vertSpeed = -perf.getDescentVertSpeed();
              tas = perf.getDescentSpeed();
              fuelflow = perf.getDescentFuelFlowLbs();
            }
          }
        }
        else
        {
          tas = 20.f;
          fuelflow = 20.f;
          if(distanceFromStart < 0.2f || distanceFromStart > route.getTotalDistance() - 0.2f)
            fuelflow = 0.f;
        }
      }

      if(atools::almostEqual(fuelflow, 0.f) && !ground)
        fuelflow = 100.f;

      if(!(alt < map::INVALID_ALTITUDE_VALUE))
        alt = route.getCruisingAltitudeFeet();

      pos.setAltitude(alt);
      SimConnectData data = SimConnectData::buildDebugForPosition(pos, lastPos, ground, vertSpeed, tas, fuelflow,
                                                                  totalFuel, 10.f);
      data.setPacketId(packetId++);

      emit NavApp::getConnectClient()->dataPacketReceived(data);
      lastPos = pos;
      lastPoint = event->pos();
    }
  }
}

#endif
