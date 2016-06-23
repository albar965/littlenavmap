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

#include <QContextMenuEvent>
#include <QToolTip>
#include <QRubberBand>

#include <marble/MarbleLocale.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/MarbleModel.h>

using namespace Marble;
using atools::gui::MapPosHistoryEntry;
using atools::gui::MapPosHistory;

MapWidget::MapWidget(MainWindow *parent, MapQuery *query)
  : Marble::MarbleWidget(parent), mainWindow(parent), mapQuery(query)
{
  installEventFilter(this);

  // Set the map quality to gain speed
  // TODO configuration option
  setMapQualityForViewContext(HighQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapTooltip = new MapTooltip(this, mapQuery, mainWindow->getWeatherReporter());
  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  screenIndex = new MapScreenIndex(this, mapQuery, paintLayer);

  MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);
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
  switch(index)
  {
    case MapWidget::OPENTOPOMAP:
      setPropertyValue("ice", false);
      ui->actionMapShowCities->setEnabled(false);
      ui->actionMapShowHillshading->setEnabled(false);
      break;
    case MapWidget::OSM:
      ui->actionMapShowCities->setEnabled(false);
      ui->actionMapShowHillshading->setEnabled(true);
      break;
    case MapWidget::POLITICAL:
    case MapWidget::PLAIN:
      ui->actionMapShowCities->setEnabled(true);
      ui->actionMapShowHillshading->setEnabled(false);
      break;
    case MapWidget::INVALID:
      break;
  }

  setMapThemeId(theme);
  updateMapShowFeatures();
}

void MapWidget::updateMapShowFeatures()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  setShowMapPois(ui->actionMapShowCities->isChecked() &&
                 (currentComboIndex == MapWidget::POLITICAL || currentComboIndex == MapWidget::PLAIN));
  setShowGrid(ui->actionMapShowGrid->isChecked());
  setPropertyValue("hillshading", ui->actionMapShowHillshading->isChecked() &&
                   currentComboIndex == MapWidget::OSM);

  setShowMapFeatures(maptypes::AIRWAYV,
                     ui->actionMapShowVictorAirways->isChecked());
  setShowMapFeatures(maptypes::AIRWAYJ,
                     ui->actionMapShowJetAirways->isChecked());

  setShowMapFeatures(maptypes::ROUTE,
                     ui->actionMapShowRoute->isChecked());
  setShowMapFeatures(maptypes::AIRCRAFT,
                     ui->actionMapShowAircraft->isChecked());
  setShowMapFeatures(maptypes::AIRCRAFT_TRACK,
                     ui->actionMapShowAircraftTrack->isChecked());

  setShowMapFeatures(maptypes::AIRPORT_HARD,
                     ui->actionMapShowAirports->isChecked());
  setShowMapFeatures(maptypes::AIRPORT_SOFT,
                     ui->actionMapShowSoftAirports->isChecked());
  setShowMapFeatures(maptypes::AIRPORT_EMPTY,
                     ui->actionMapShowEmptyAirports->isChecked());
  setShowMapFeatures(maptypes::AIRPORT_ADDON,
                     ui->actionMapShowAddonAirports->isChecked());

  setShowMapFeatures(maptypes::AIRPORT,
                     ui->actionMapShowAirports->isChecked() ||
                     ui->actionMapShowSoftAirports->isChecked() ||
                     ui->actionMapShowEmptyAirports->isChecked() ||
                     ui->actionMapShowAddonAirports->isChecked());

  setShowMapFeatures(maptypes::VOR, ui->actionMapShowVor->isChecked());
  setShowMapFeatures(maptypes::NDB, ui->actionMapShowNdb->isChecked());
  setShowMapFeatures(maptypes::ILS, ui->actionMapShowIls->isChecked());
  setShowMapFeatures(maptypes::WAYPOINT, ui->actionMapShowWp->isChecked());
  updateVisibleObjectsStatusBar();
  update();
}

void MapWidget::setShowMapPois(bool show)
{
  qDebug() << "setShowMapPois" << show;
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
  setShowTerrain(show);
}

void MapWidget::setShowMapFeatures(maptypes::MapObjectTypes type, bool show)
{
  paintLayer->setShowMapFeatures(type, show);
  screenIndex->updateAirwayScreenLines(curBox);
}

void MapWidget::setDetailFactor(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
  updateVisibleObjectsStatusBar();
  screenIndex->updateAirwayScreenLines(curBox);
}

maptypes::MapObjectTypes MapWidget::getShownMapFeatures()
{
  return paintLayer->getShownMapFeatures();
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
  screenIndex->updateAirwayScreenLines(curBox);
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

  s.setValue(lnm::MAP_MARKLONX, markPos.getLonX());
  s.setValue(lnm::MAP_MARKLATY, markPos.getLatY());

  s.setValue(lnm::MAP_HOMELONX, homePos.getLonX());
  s.setValue(lnm::MAP_HOMELATY, homePos.getLatY());
  s.setValue(lnm::MAP_HOMEDISTANCE, homeDistance);
  s.setValue(lnm::MAP_KMLFILES, kmlFiles);
  history.saveState(lnm::MAP_HISTORY);
  screenIndex->saveState();
}

void MapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  readPluginSettings(*s.getQSettings());

  if(s.contains(lnm::MAP_MARKLONX) && s.contains(lnm::MAP_MARKLATY))
  {
    markPos = atools::geo::Pos(s.valueFloat(lnm::MAP_MARKLONX), s.valueFloat(lnm::MAP_MARKLATY));
    emit markChanged(markPos);
  }

  if(s.contains(lnm::MAP_HOMELONX) && s.contains(lnm::MAP_HOMELATY) && s.contains(lnm::MAP_HOMEDISTANCE))
  {
    homePos = atools::geo::Pos(s.valueFloat(lnm::MAP_HOMELONX), s.valueFloat(lnm::MAP_HOMELATY));
    homeDistance = s.valueFloat(lnm::MAP_HOMEDISTANCE);
    emit homeChanged(homePos);
  }
  history.restoreState(lnm::MAP_HISTORY);

  if(s.contains(lnm::MAP_KMLFILES))
    kmlFiles = s.valueStrList(lnm::MAP_KMLFILES);

  QStringList copyKml(kmlFiles);
  for(const QString& kml : kmlFiles)
  {
    if(QFile::exists(kml))
      model()->addGeoDataFile(kml);
    else
      copyKml.removeAll(kml);
  }
  kmlFiles = copyKml;
  screenIndex->restoreState();
}

void MapWidget::showSavedPos()
{
  const MapPosHistoryEntry& currentPos = history.current();

  QToolTip::hideText();
  tooltipPos = QPoint();

  if(currentPos.isValid())
  {
    centerOn(currentPos.getPos().getLonX(), currentPos.getPos().getLatY(), false);
    setDistance(currentPos.getDistance());
  }
  else
  {
    centerOn(0.f, 0.f, false);
    setDistance(7000);
  }
}

void MapWidget::showPos(const atools::geo::Pos& pos, int zoomValue)
{
  qDebug() << "NavMapWidget::showPoint" << pos;
  QToolTip::hideText();
  tooltipPos = QPoint();
  setZoom(zoomValue);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void MapWidget::showRect(const atools::geo::Rect& rect)
{
  qDebug() << "NavMapWidget::showRect" << rect;
  QToolTip::hideText();
  tooltipPos = QPoint();
  centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                            GeoDataCoordinates::Degree), false);
}

void MapWidget::showMark()
{
  qDebug() << "NavMapWidget::showMark" << markPos;

  QToolTip::hideText();
  tooltipPos = QPoint();

  if(markPos.isValid())
  {
    setZoom(2700);
    centerOn(markPos.getLonX(), markPos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing distance search center."));
  }
}

void MapWidget::showAircraft(bool state)
{
  qDebug() << "NavMapWidget::showAircraft" << markPos;

  if(state && simData.getPosition().isValid())
  {
    if(zoom() < 1800)
      setZoom(1800);
    centerOn(simData.getPosition().getLonX(), simData.getPosition().getLatY(), false);
    // mainWindow->statusMessage(tr("Showing simulator aircraft."));
  }
}

void MapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << markPos;

  QToolTip::hideText();
  tooltipPos = QPoint();

  if(!atools::almostEqual(homeDistance, 0.))
    setDistance(homeDistance);

  if(homePos.isValid())
  {
    centerOn(homePos.getLonX(), homePos.getLatY(), false);
    mainWindow->setStatusMessage(tr("Showing home position."));
  }
}

void MapWidget::changeMark(const atools::geo::Pos& pos)
{
  markPos = pos;
  emit markChanged(markPos);
  emit updateActionStates();
  update();
  mainWindow->setStatusMessage(tr("Distance search center position changed."));
}

void MapWidget::changeRouteHighlight(const RouteMapObjectList& routeHighlight)
{
  screenIndex->getRouteHighlights() = routeHighlight;
  update();
}

void MapWidget::routeChanged(bool geometryChanged)
{
  if(geometryChanged)
  {
    paintLayer->routeChanged();
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
    emit updateActionStates();

  if(paintLayer->getShownMapFeatures() & maptypes::AIRCRAFT)
  {
    using atools::almostNotEqual;
    if(!lastSimData.getPosition().isValid() || diff.manhattanLength() > 1 ||
       almostNotEqual(lastSimData.getHeadingDegMag(), simData.getHeadingDegMag(), 1.f) ||
       almostNotEqual(lastSimData.getIndicatedSpeedKts(), simData.getIndicatedSpeedKts(), 10.f) ||
       almostNotEqual(lastSimData.getPosition().getAltitude(), simData.getPosition().getAltitude(), 10.f))
    {
      lastSimData = simulatorData;

      int dx = width() / 3;
      int dy = height() / 3;

      QRect widgetRect = geometry();
      widgetRect.adjust(dx, dy, -dx, -dy);

      if(!widgetRect.contains(curPos) && mainWindow->getUi()->actionMapAircraftCenter->isChecked())
        centerOn(simData.getPosition().getLonX(), simData.getPosition().getLatY(), false);
      else
        update();
    }
  }
  else if(paintLayer->getShownMapFeatures() & maptypes::AIRCRAFT_TRACK)
  {
    using atools::almostNotEqual;
    if(!lastSimData.getPosition().isValid() || diff.manhattanLength() > 4)
    {
      lastSimData = simulatorData;
      update();
    }
  }
}

void MapWidget::highlightProfilePoint(atools::geo::Pos pos)
{
  if(pos.isValid())
  {
    CoordinateConverter conv(viewport());
    int x, y;
    if(conv.wToS(pos, x, y))
    {
      if(profilePoint == nullptr)
        profilePoint = new QRubberBand(QRubberBand::Rectangle, this);

      const QRect& geo = profilePoint->geometry();

      if(geo.x() != x - 6 || geo.y() != y - 6)
      {
        profilePoint->setGeometry(x - 6, y - 6, 12, 12);
        profilePoint->show();
      }
      return;
    }
  }

  delete profilePoint;
  profilePoint = nullptr;
}

void MapWidget::disconnectedFromSimulator()
{
  simData = atools::fs::sc::SimConnectData();
  aircraftTrack.clear();
  update();
}

void MapWidget::addKmlFile(const QString& kmlFile)
{
  kmlFiles.append(kmlFile);
  model()->addGeoDataFile(kmlFile);
}

void MapWidget::clearKmlFiles()
{
  for(const QString& file : kmlFiles)
    model()->removeGeoData(file);
  kmlFiles.clear();
}

void MapWidget::changeHighlight(const maptypes::MapSearchResult& positions)
{
  screenIndex->getHighlights() = positions;
  update();
}

void MapWidget::changeHome()
{
  homePos = atools::geo::Pos(centerLongitude(), centerLatitude());
  homeDistance = distance();
  emit updateActionStates();
  update();
  mainWindow->setStatusMessage(QString(tr("Changed home position.")));
}

void MapWidget::updateRouteFromDrag(QPoint newPoint, MouseStates state, int leg, int point)
{
  qDebug() << "End route drag" << newPoint << "state" << state << "leg" << leg << "point" << point;

  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(newPoint.x(), newPoint.y(), screenSearchDistance, result);

  CoordinateConverter conv(viewport());

  // Get objects from cache - already present objects will be skipped
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(), false,
                              paintLayer->getShownMapFeatures() &
                              (maptypes::AIRPORT_ALL | maptypes::VOR | maptypes::NDB | maptypes::WAYPOINT),
                              newPoint.x(), newPoint.y(), screenSearchDistance, result);

  int totalSize = result.airports.size() + result.vors.size() + result.ndbs.size() + result.waypoints.size();

  int id = -1;
  maptypes::MapObjectTypes type = maptypes::NONE;
  atools::geo::Pos pos = atools::geo::EMPTY_POS;
  if(totalSize == 0)
  {
    // Add userpoint
    qDebug() << "add userpoint";
    type = maptypes::USER;
  }
  else if(totalSize == 1)
  {
    // Add single navaid
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
      QAction *action = new QAction(symbolPainter.createNdbIcon(obj, ICON_SIZE),
                                    menuPrefix + maptypes::ndbText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::NDB}));
      menu.addAction(action);
    }
    for(const maptypes::MapWaypoint& obj : result.waypoints)
    {
      QAction *action = new QAction(symbolPainter.createWaypointIcon(obj, ICON_SIZE),
                                    menuPrefix + maptypes::waypointText(obj) + menuSuffix, this);
      action->setData(QVariantList({obj.id, maptypes::WAYPOINT}));
      menu.addAction(action);
    }

    menu.addSeparator();
    {
      QAction *action = new QAction(symbolPainter.createUserpointIcon(ICON_SIZE),
                                    menuPrefix + "Userpoint" + menuSuffix, this);
      action->setData(QVariantList({-1, maptypes::USER}));
      menu.addAction(action);
    }

    menu.addSeparator();
    menu.addAction(new QAction(QIcon(":/littlenavmap/resources/icons/cancel.svg"),
                               tr("Cancel"), this));

    QAction *action = menu.exec(QCursor::pos());
    if(action != nullptr && !action->data().isNull())
    {
      QVariantList data = action->data().toList();
      id = data.at(0).toInt();
      type = maptypes::MapObjectTypes(data.at(1).toInt());
    }
  }

  if(type == maptypes::USER)
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

  QToolTip::hideText();

  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch, ui->actionRouteAdd,
                                          ui->actionMapShowInformation,
                                          ui->actionRouteDeleteWaypoint, ui->actionRouteAirportStart,
                                          ui->actionRouteAirportDest});
  Q_UNUSED(textSaver);

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

  int distMarkerIndex = screenIndex->getNearestDistanceMarksIndex(point.x(), point.y(), screenSearchDistance);
  int rangeMarkerIndex = screenIndex->getNearestRangeMarkIndex(point.x(), point.y(), screenSearchDistance);

  qreal lon, lat;
  bool visible = geoCoordinates(point.x(), point.y(), lon, lat);
  atools::geo::Pos pos(lon, lat);

  // Disable all menu items that depend on position
  ui->actionMapSetMark->setEnabled(visible);
  ui->actionMapSetHome->setEnabled(visible);
  ui->actionMapMeasureDistance->setEnabled(visible);
  ui->actionMapMeasureRhumbDistance->setEnabled(visible);
  ui->actionMapRangeRings->setEnabled(visible);

  ui->actionMapHideRangeRings->setEnabled(!screenIndex->getRangeMarks().isEmpty() ||
                                          !screenIndex->getDistanceMarks().isEmpty());
  ui->actionMapHideOneRangeRing->setEnabled(visible && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visible && distMarkerIndex != -1);

  ui->actionMapNavaidRange->setEnabled(false);
  ui->actionShowInSearch->setEnabled(false);
  ui->actionRouteAdd->setEnabled(true);
  ui->actionRouteAirportStart->setEnabled(false);
  ui->actionRouteAirportDest->setEnabled(false);
  ui->actionRouteDeleteWaypoint->setEnabled(false);

  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDistance, result);
  // maptypes::MapObjectTypes selectedSearchType = maptypes::NONE, selectedRangeType = maptypes::NONE;
  maptypes::MapAirport *airport = nullptr;
  maptypes::MapVor *vor = nullptr;
  maptypes::MapNdb *ndb = nullptr;
  maptypes::MapWaypoint *waypoint = nullptr;
  maptypes::MapUserpoint *userpoint = nullptr;
  maptypes::MapParking *parking = nullptr;

  if(!result.airports.isEmpty())
    airport = &result.airports.first();
  if(!result.parkings.isEmpty() && result.parkings.first().type != "FUEL")
    parking = &result.parkings.first();
  if(!result.vors.isEmpty())
    vor = &result.vors.first();
  if(!result.ndbs.isEmpty())
    ndb = &result.ndbs.first();
  if(!result.waypoints.isEmpty())
    waypoint = &result.waypoints.first();
  if(!result.userPoints.isEmpty())
    userpoint = &result.userPoints.first();

  QString searchText;
  // Collect information from the search result
  if(airport != nullptr)
    searchText = maptypes::airportText(*airport);
  else if(parking != nullptr)
    searchText = maptypes::parkingName(parking->name) + " " + QLocale().toString(parking->number);
  else if(vor != nullptr)
    searchText = maptypes::vorText(*vor);
  else if(ndb != nullptr)
    searchText = maptypes::ndbText(*ndb);
  else if(waypoint != nullptr)
    searchText = maptypes::waypointText(*waypoint);
  else if(userpoint != nullptr)
    searchText = maptypes::userpointText(*userpoint);

  QString navRingSearchText;
  // Collect information from the search result
  if(vor != nullptr)
    navRingSearchText = maptypes::vorText(*vor);
  else if(ndb != nullptr)
    navRingSearchText = maptypes::ndbText(*ndb);

  int delRouteIndex = -1;
  maptypes::MapObjectTypes delType = maptypes::NONE;
  QString delSearchText;
  // Collect information from the search result
  if(airport != nullptr && airport->routeIndex != -1)
  {
    delSearchText = maptypes::airportText(*airport);
    delRouteIndex = airport->routeIndex;
    delType = maptypes::AIRPORT;
  }
  else if(vor != nullptr && vor->routeIndex != -1)
  {
    delSearchText = maptypes::vorText(*vor);
    delRouteIndex = vor->routeIndex;
    delType = maptypes::VOR;
  }
  else if(ndb != nullptr && ndb->routeIndex != -1)
  {
    delSearchText = maptypes::ndbText(*ndb);
    delRouteIndex = ndb->routeIndex;
    delType = maptypes::NDB;
  }
  else if(waypoint != nullptr && waypoint->routeIndex != -1)
  {
    delSearchText = maptypes::waypointText(*waypoint);
    delRouteIndex = waypoint->routeIndex;
    delType = maptypes::WAYPOINT;
  }
  else if(userpoint != nullptr && userpoint->routeIndex != -1)
  {
    delSearchText = maptypes::userpointText(*userpoint);
    delRouteIndex = userpoint->routeIndex;
    delType = maptypes::USER;
  }

  // Update "set airport as start/dest"
  if(airport != nullptr || parking != nullptr)
  {
    QString airportText;

    if(parking != nullptr)
    {
      maptypes::MapAirport parkAp;
      mapQuery->getAirportById(parkAp, parking->airportId);
      airportText = maptypes::airportText(parkAp) + " / ";
    }

    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(airportText + searchText));

    if(airport != nullptr)
    {
      ui->actionRouteAirportDest->setEnabled(true);
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(searchText));
    }
    else
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }
  else
  {
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
  }

  // Update "show in search" and "add to route"
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(searchText));
    ui->actionRouteAdd->setEnabled(true);
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg(searchText));
    ui->actionMapShowInformation->setEnabled(true);
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(searchText));
  }
  else
  {
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));
    ui->actionRouteAdd->setText(ui->actionRouteAdd->text().arg(tr("Position")));
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(QString()));
  }

  // Update "delete in route"
  if(delRouteIndex != -1)
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(delSearchText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(QString()));

  // Update "show range rings for Navaid"
  if(vor != nullptr || ndb != nullptr)
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(navRingSearchText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && !searchText.isEmpty())
  {
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(searchText));
    ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(searchText));
  }
  else
  {
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

  if(visible)
  {
    if(action == ui->actionShowInSearch)
    {
      if(airport != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(0);
        emit objectSelected(maptypes::AIRPORT, airport->ident, QString(), QString());
      }
      else if(vor != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(maptypes::VOR, vor->ident, vor->region, vor->apIdent);
      }
      else if(ndb != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        // TODO check airport ident (probably not loaded)
        emit objectSelected(maptypes::NDB, ndb->ident, ndb->region, ndb->apIdent);
      }
      else if(waypoint != nullptr)
      {
        ui->dockWidgetSearch->raise();
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(maptypes::WAYPOINT, waypoint->ident, waypoint->region, waypoint->apIdent);
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
      changeMark(pos);
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
      dm.rhumbLine = action == ui->actionMapMeasureRhumbDistance;
      dm.to = pos;

      if(vor != nullptr)
      {
        dm.text = vor->ident + " " + QLocale().toString(vor->frequency / 1000., 'f', 2);
        dm.from = vor->position;
        dm.magvar = vor->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::vorSymbolColor;
        dm.type = maptypes::VOR;
      }
      else if(ndb != nullptr)
      {
        dm.text = ndb->ident + " " + QLocale().toString(ndb->frequency / 100., 'f', 2);
        dm.from = ndb->position;
        dm.magvar = ndb->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::ndbSymbolColor;
        dm.type = maptypes::NDB;
      }
      else if(waypoint != nullptr)
      {
        dm.text = waypoint->ident;
        dm.from = waypoint->position;
        dm.magvar = waypoint->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::waypointSymbolColor;
        dm.type = maptypes::WAYPOINT;
      }
      else if(airport != nullptr)
      {
        dm.text = airport->name + " (" + airport->ident + ")";
        dm.from = airport->position;
        dm.magvar = airport->magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::colorForAirport(*airport);
        dm.type = maptypes::AIRPORT;
      }
      else
      {
        dm.hasMagvar = false;
        dm.from = pos;
        dm.color = dm.rhumbLine ? mapcolors::distanceRhumbColor : mapcolors::distanceColor;
      }

      screenIndex->getDistanceMarks().append(dm);

      mouseState = DRAG_DISTANCE;
      setContextMenuPolicy(Qt::NoContextMenu);
      currentDistanceMarkerIndex = screenIndex->getDistanceMarks().size() - 1;
    }
    else if(action == ui->actionRouteDeleteWaypoint)
    {
      // ui->dockWidgetRoute->raise();
      emit routeDelete(delRouteIndex, delType);
    }
    else if(action == ui->actionRouteAdd || action == ui->actionRouteAirportStart ||
            action == ui->actionRouteAirportDest || action == ui->actionMapShowInformation)
    {
      // ui->dockWidgetRoute->raise();

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
          // Adjust values in case user selected add on a parking position
          type = maptypes::USER;
          id = -1;
        }
        emit routeAdd(id, position, type, -1);
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

  if(type == maptypes::VOR || type == maptypes::ILS)
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
  rings.ranges = {50, 100, 200, 500};
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
    qDebug() << "eventFilter mouseDoubleClickEvent";
    e->accept();
    event(e);
    return true;
  }

  if(e->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(e);

    if(mouseEvent != nullptr)
    {
      if(mouseEvent->modifiers() & Qt::ControlModifier)
      {
        // Remove control modifer to disable Marble rectangle dragging
        mouseEvent->setModifiers(mouseEvent->modifiers() & ~Qt::ControlModifier);
        qDebug() << "eventFilter ctrl click";
      }
    }
  }

  if(e->type() == QEvent::MouseMove && mouseState != NONE)
  {
    // Do not allow mouse scrolling during drag actions
    e->accept();
    event(e);
    return true;
  }
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

const maptypes::MapSearchResult& MapWidget::getHighlightMapObjects() const
{
  return screenIndex->getHighlights();
}

const RouteMapObjectList& MapWidget::getRouteHighlightMapObjects() const
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

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(!isActiveWindow())
    return;

  // qDebug() << "mouseMoveEvent state" << mouseState <<
  // "modifiers" << event->modifiers() << "buttons" << event->buttons()
  // << "button" << event->button() << "pos" << event->pos();

  if(mouseState & DRAG_DISTANCE || mouseState & DRAG_CHANGE_DISTANCE)
  {
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    qreal lon, lat;
    bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
    if(visible)
    {
      atools::geo::Pos p(lon, lat);
      if(!screenIndex->getDistanceMarks().isEmpty())
        screenIndex->getDistanceMarks()[currentDistanceMarkerIndex].to = p;
    }
    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState & DRAG_ROUTE_LEG || mouseState & DRAG_ROUTE_POINT)
  {
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    routeDragCur = QPoint(event->pos().x(), event->pos().y());

    setViewContext(Marble::Animation);
    update();
  }
  else if(mouseState == NONE)
    if(event->buttons() == Qt::NoButton)
    {
      const RouteMapObjectList& rmos = mainWindow->getRouteController()->getRouteMapObjects();

      Qt::CursorShape cursorShape = Qt::ArrowCursor;
      bool routeEditMode = mainWindow->getUi()->actionRouteEditMode->isChecked();

      if(routeEditMode &&
         screenIndex->getNearestRoutePointIndex(event->pos().x(), event->pos().y(),
                                                5) != -1 && rmos.size() > 1)
        // Change cursor at one route point
        cursorShape = Qt::CrossCursor;
      else if(routeEditMode &&
              screenIndex->getNearestRouteLegIndex(event->pos().x(), event->pos().y(),
                                                   5) != -1 && rmos.size() > 1)
        // Change cursor above a route line
        cursorShape = Qt::CrossCursor;
      else if(screenIndex->getNearestDistanceMarksIndex(event->pos().x(), event->pos().y(),
                                                         screenSearchDistance) != -1)
        // Change cursor at the end of an marker
        cursorShape = Qt::CrossCursor;

      if(cursor().shape() != cursorShape)
        setCursor(cursorShape);
    }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
  qDebug() << "mousePressEvent state" << mouseState <<
  "modifiers" << event->modifiers() << "buttons" << event->buttons()
           << "button" << event->button() << "pos" << event->pos();

  QToolTip::hideText();

  mouseMoved = event->pos();
  if(mouseState == DRAG_DISTANCE || mouseState == DRAG_CHANGE_DISTANCE ||
     mouseState == DRAG_ROUTE_POINT || mouseState == DRAG_ROUTE_LEG)
  {
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);
    if(event->button() == Qt::LeftButton)
      mouseState |= DRAG_POST;
    else if(event->button() == Qt::RightButton)
      mouseState |= DRAG_POST_CANCEL;
  }
  else if(mouseState == NONE && event->buttons() & Qt::RightButton)
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  qDebug() << "mouseReleaseEvent state" << mouseState <<
  "modifiers" << event->modifiers() << "buttons" << event->buttons()
           << "button" << event->button() << "pos" << event->pos();
  QToolTip::hideText();

  if(mouseState & DRAG_ROUTE_POINT || mouseState & DRAG_ROUTE_LEG)
  {
    // End route point dragging
    if(mouseState & DRAG_POST)
      updateRouteFromDrag(routeDragCur, mouseState, routeDragLeg, routeDragPoint);

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
    // Start all dragging
    currentDistanceMarkerIndex = screenIndex->getNearestDistanceMarksIndex(event->pos().x(),
                                                                            event->pos().y(),
                                                                            screenSearchDistance);
    if(currentDistanceMarkerIndex != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = DRAG_CHANGE_DISTANCE;
      distanceMarkerBackup = screenIndex->getDistanceMarks().at(currentDistanceMarkerIndex);
      setContextMenuPolicy(Qt::NoContextMenu);
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
            setContextMenuPolicy(Qt::NoContextMenu);
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
              setContextMenuPolicy(Qt::NoContextMenu);
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
  screenIndex->getAllNearest(event->pos().x(),
                                       event->pos().y(), screenSearchDistance, mapSearchResult);

  if(!mapSearchResult.airports.isEmpty())
  {
    if(mapSearchResult.airports.at(0).bounding.isPoint())
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

void MapWidget::updateTooltip()
{
  if(databaseLoadStatus)
    return;

  QString text = mapTooltip->buildTooltip(mapSearchResultTooltip,
                                          mainWindow->getRouteController()->getRouteMapObjects(),
                                          paintLayer->getMapLayer()->isAirportDiagram());

  if(!text.isEmpty() && !tooltipPos.isNull())
  {
    // qDebug() << "Tooltip show";
    QToolTip::showText(tooltipPos, text, nullptr, QRect(), 3600 * 1000);
  }
  else
  {
    // qDebug() << "Tooltip hide";
    tooltipPos = QPoint();
    QToolTip::hideText();
  }
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
    // qDebug() << "QEvent::ToolTip";
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    mapSearchResultTooltip = maptypes::MapSearchResult();
    screenIndex->getAllNearest(helpEvent->pos().x(),
                                         helpEvent->pos().y(), screenSearchDistanceTooltip,
                                         mapSearchResultTooltip);
    tooltipPos = helpEvent->globalPos();
    updateTooltip();
    event->accept();
    return true;
  }

  return QWidget::event(event);
}

void MapWidget::updateVisibleObjectsStatusBar()
{
  const MapLayer *layer = paintLayer->getMapLayer();

  if(layer != nullptr)
  {
    maptypes::MapObjectTypes shown = paintLayer->getShownMapFeatures();

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

    mainWindow->setShownMapObjectsMessageText(visible.join(","), visibleTooltip.getHtml());
  }
}

void MapWidget::paintEvent(QPaintEvent *paintEvent)
{
  bool changed = false;
  const GeoDataLatLonAltBox visibleLatLonAltBox = viewport()->viewLatLonAltBox();
  if(viewContext() == Marble::Still && (zoom() != curZoom || visibleLatLonAltBox != curBox))
  {
    curZoom = zoom();
    curBox = visibleLatLonAltBox;

    qDebug() << "paintEvent map view has changed zoom" << curZoom
             << "distance" << distance() << " (" << atools::geo::meterToNm(distance() * 1000.) << " km)";
    qDebug() << "pole" << curBox.containsPole() << curBox.toString(GeoDataCoordinates::Degree);

    if(!changedByHistory)
      history.addEntry(atools::geo::Pos(centerLongitude(), centerLatitude()), distance());
    changedByHistory = false;
    changed = true;
  }

  // QElapsedTimer t;
  // t.start();

  MarbleWidget::paintEvent(paintEvent);

  // if(viewContext() == Marble::Still)
  // qDebug() << "Time for all rendering" << t.elapsed() << "ms";

  if(changed)
  {
    updateVisibleObjectsStatusBar();
    screenIndex->updateRouteScreenGeometry();
    screenIndex->updateAirwayScreenLines(curBox);
  }
}

void MapWidget::handleInfoClick(QPoint pos)
{
  maptypes::MapSearchResult result;
  screenIndex->getAllNearest(pos.x(), pos.y(), 10, result);
  emit showInformation(result);
}
