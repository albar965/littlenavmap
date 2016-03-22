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

#include "mapgui/mappaintlayer.h"
#include "mapgui/navmapwidget.h"
#include "settings/settings.h"
#include "gui/mainwindow.h"
#include "mapgui/mapscale.h"
#include "table/formatter.h"
#include "geo/calculations.h"
#include "common/maptools.h"
#include "common/mapcolors.h"
#include "route/routecontroller.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QSettings>
#include <QToolTip>

#include <marble/MarbleLocale.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleModel.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleModel.h>
#include "coordinateconverter.h"
#include "maplayer.h"
#include "ui_mainwindow.h"

#include <gui/actiontextsaver.h>

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent, MapQuery *query)
  : Marble::MarbleWidget(parent), parentWindow(parent), mapQuery(query)
{
  installEventFilter(this);

  // Set the map quality to gain speed
  setMapQualityForViewContext(NormalQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  inputHandler()->setInertialEarthRotationEnabled(false);

  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  MarbleWidgetInputHandler *input = inputHandler();
  input->setMouseButtonPopupEnabled(Qt::RightButton, false);
  input->setMouseButtonPopupEnabled(Qt::LeftButton, false);
  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::contextMenu);
}

NavMapWidget::~NavMapWidget()
{
  delete paintLayer;
  highlightMapObjects.deleteAllObjects();
}

void NavMapWidget::setTheme(const QString& theme, int index)
{
  // mapWidget->setAnimationsEnabled(false);
  // mapWidget->setShowCrosshairs(false);
  // mapWidget->setShowBackground(false);
  // mapWidget->setShowAtmosphere(false);
  // mapWidget->setShowGrid(true);

  setMapThemeId(theme);

  if(index == 1)
  {
    setShowTerrain(true);
    setShowRelief(true);
  }
  else
  {
    setShowTerrain(false);
    setShowRelief(false);
  }
  setShowClouds(false);
  setShowBorders(true);
  setShowIceLayer(false);
  setShowLakes(true);
  setShowRivers(true);
}

void NavMapWidget::setShowMapPois(bool show)
{
  qDebug() << "setShowMapPois" << show;
  setShowPlaces(show);
  setShowCities(show);
  setShowOtherPlaces(show);
}

void NavMapWidget::setShowMapFeatures(maptypes::MapObjectTypes type, bool show)
{
  qDebug() << "setShowMapFeatures" << type << "show" << show;
  paintLayer->setShowMapFeatures(type, show);
}

void NavMapWidget::setDetailFactor(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
}

RouteController *NavMapWidget::getRouteController() const
{
  return parentWindow->getRouteController();
}

void NavMapWidget::preDatabaseLoad()
{
  paintLayer->preDatabaseLoad();
}

void NavMapWidget::postDatabaseLoad()
{
  paintLayer->postDatabaseLoad();
}

void NavMapWidget::historyNext()
{
  const MapPosHistoryEntry& entry = history.next();
  if(entry.pos.isValid())
  {
    setZoom(entry.zoom);
    centerOn(entry.pos.getLonX(), entry.pos.getLatY(), false);
  }
}

void NavMapWidget::historyBack()
{
  const MapPosHistoryEntry& entry = history.back();
  if(entry.pos.isValid())
  {
    setZoom(entry.zoom);
    centerOn(entry.pos.getLonX(), entry.pos.getLatY(), false);
  }
}

void NavMapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  s->setValue("Map/MarkLonX", static_cast<double>(markPos.getLonX()));
  s->setValue("Map/MarkLatY", static_cast<double>(markPos.getLatY()));

  s->setValue("Map/HomeLonX", static_cast<double>(homePos.getLonX()));
  s->setValue("Map/HomeLatY", static_cast<double>(homePos.getLatY()));
  s->setValue("Map/HomeZoom", homeZoom);
  history.saveState("Map/History");
}

void NavMapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  if(s->contains("Map/MarkLonX") && s->contains("Map/MarkLatY"))
  {
    markPos = atools::geo::Pos(s->value("Map/MarkLonX").toDouble(), s->value("Map/MarkLatY").toDouble());
    emit markChanged(markPos);
  }

  if(s->contains("Map/HomeLonX") && s->contains("Map/HomeLatY") && s->contains("Map/HomeZoom"))
  {
    homePos = atools::geo::Pos(s->value("Map/HomeLonX").toDouble(), s->value("Map/HomeLatY").toDouble());
    homeZoom = s->value("Map/HomeZoom").toInt();
    emit homeChanged(markPos);
  }
  history.restoreState("Map/History");
}

void NavMapWidget::showSavedPos()
{
  const MapPosHistoryEntry& currentPos = history.current();

  if(currentPos.pos.isValid())
    centerOn(currentPos.pos.getLonX(), currentPos.pos.getLatY(), false);

  if(currentPos.zoom != -1)
    setZoom(currentPos.zoom);
}

void NavMapWidget::showPos(const atools::geo::Pos& pos, int zoomValue)
{
  qDebug() << "NavMapWidget::showPoint" << pos;
  setZoom(zoomValue);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void NavMapWidget::showRect(const atools::geo::Rect& rect)
{
  qDebug() << "NavMapWidget::showRect" << rect;
  centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                            GeoDataCoordinates::Degree), false);
}

void NavMapWidget::showMark()
{
  qDebug() << "NavMapWidget::showMark" << markPos;
  if(markPos.isValid())
  {
    setZoom(2700);
    centerOn(markPos.getLonX(), markPos.getLatY(), false);
  }
}

void NavMapWidget::showHome()
{
  qDebug() << "NavMapWidget::showHome" << markPos;
  if(homeZoom != -1)
    setZoom(homeZoom);

  if(homePos.isValid())
    centerOn(homePos.getLonX(), homePos.getLatY(), false);
}

void NavMapWidget::changeMark(const atools::geo::Pos& pos)
{
  markPos = pos;
  emit markChanged(markPos);

  update();
}

void NavMapWidget::changeRouteHighlight(const QList<RouteMapObject>& routeHighlight)
{
  routeHighlightMapObjects.clear();
  routeHighlightMapObjects = routeHighlight;
  update();
}

void NavMapWidget::changeHighlight(const maptypes::MapSearchResult& positions)
{
  highlightMapObjects.deleteAllObjects();
  highlightMapObjects = positions;
  update();
}

void NavMapWidget::changeHome()
{
  homePos = atools::geo::Pos(centerLongitude(), centerLatitude());
  homeZoom = zoom();
  update();
}

void NavMapWidget::contextMenu(const QPoint& point)
{
  Q_UNUSED(point);
  qInfo() << "tableContextMenu";

  if(mouseState != NONE)
    return;

  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::ActionTextSaver textSaver({ui->actionMapMeasureDistance, ui->actionMapMeasureRhumbDistance,
                                          ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                          ui->actionShowInSearch});
  Q_UNUSED(textSaver);

  QMenu menu;
  menu.addAction(ui->actionMapSetMark);
  menu.addAction(ui->actionMapSetHome);
  menu.addSeparator();
  menu.addAction(ui->actionMapMeasureDistance);
  menu.addAction(ui->actionMapMeasureRhumbDistance);
  menu.addAction(ui->actionMapHideDistanceMarker);
  menu.addSeparator();
  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideOneRangeRing);
  menu.addAction(ui->actionMapHideRangeRings);

  CoordinateConverter conv(viewport());
  maptypes::MapSearchResult result;
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(),
                              paintLayer->getShownMapFeatures(),
                              point.x(), point.y(), 20, result);

  int distMarkerIndex = getNearestDistanceMarkerIndex(point.x(), point.y(), 10);
  int rangeMarkerIndex = getNearestRangeMarkerIndex(point.x(), point.y(), 10);

  qreal lon, lat;
  bool visible = geoCoordinates(point.x(), point.y(), lon, lat);
  atools::geo::Pos pos(lon, lat);

  // Disable all menu items that depend on position
  ui->actionMapSetMark->setEnabled(visible);
  ui->actionMapSetHome->setEnabled(visible);
  ui->actionMapMeasureDistance->setEnabled(visible);
  ui->actionMapMeasureRhumbDistance->setEnabled(visible);
  ui->actionMapRangeRings->setEnabled(visible);
  ui->actionMapNavaidRange->setEnabled(visible);

  ui->actionMapHideRangeRings->setEnabled(!rangeMarkers.isEmpty() || !distanceMarkers.isEmpty());
  ui->actionMapHideOneRangeRing->setEnabled(visible && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visible && distMarkerIndex != -1);

  maptypes::MapObjectTypes selectedType = maptypes::NONE;
  // Create copies since the cache might be emptied by draw events after showing the menu
  maptypes::MapAirport ap;
  maptypes::MapVor vor;
  maptypes::MapNdb ndb;
  maptypes::MapWaypoint wp;
  maptypes::MapIls ils;

  QString text;
  ui->actionMapNavaidRange->setEnabled(false);
  if(!result.airports.isEmpty())
  {
    ap = *result.airports.first();
    text = "Airport " + ap.name + " (" + ap.ident + ")";
    selectedType = maptypes::AIRPORT;
    ui->actionShowInSearch->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(text));
  }
  else if(!result.vors.isEmpty())
  {
    vor = *result.vors.first();
    text = "VOR " + vor.name + " (" + vor.ident + ")";
    selectedType = maptypes::VOR;
    ui->actionShowInSearch->setEnabled(true);
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(text));
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(text));
  }
  else if(!result.ndbs.isEmpty())
  {
    ndb = *result.ndbs.first();
    text = "NDB " + ndb.name + " (" + ndb.ident + ")";
    selectedType = maptypes::NDB;
    ui->actionShowInSearch->setEnabled(true);
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(text));
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(text));
  }
  else if(!result.waypoints.isEmpty())
  {
    wp = *result.waypoints.first();
    text = "Waypoint " + wp.ident;
    selectedType = maptypes::WAYPOINT;
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(text));
  }
  else
  {
    text = "here";
    ui->actionShowInSearch->setEnabled(false);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));
  }

  ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(text));
  ui->actionMapMeasureRhumbDistance->setText(ui->actionMapMeasureRhumbDistance->text().arg(text));

  if(!result.ils.isEmpty())
    ils = *result.ils.first();

  menu.addAction(ui->actionShowInSearch);
  QAction *action = menu.exec(QCursor::pos());

  if(action == ui->actionMapHideRangeRings)
  {
    rangeMarkers.clear();
    distanceMarkers.clear();
    currentDistanceMarkerIndex = -1;
    update();
  }

  if(visible)
  {
    if(action == ui->actionShowInSearch)
    {
      if(selectedType == maptypes::AIRPORT)
      {
        ui->tabWidgetSearch->setCurrentIndex(0);
        emit objectSelected(selectedType, ap.ident, QString(), QString());
      }
      else if(selectedType == maptypes::VOR)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(selectedType, vor.ident, vor.region, vor.apIdent);
      }
      else if(selectedType == maptypes::NDB)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(selectedType, ndb.ident, ndb.region, ndb.apIdent);
      }
      else if(selectedType == maptypes::WAYPOINT)
      {
        ui->tabWidgetSearch->setCurrentIndex(1);
        emit objectSelected(selectedType, wp.ident, wp.region, wp.apIdent);
      }
    }
    else if(action == ui->actionMapNavaidRange)
    {
      if(selectedType == maptypes::VOR)
        addNavRangeRing(vor.position, selectedType, vor.ident, vor.frequency, vor.range);
      else if(selectedType == maptypes::NDB)
        addNavRangeRing(ndb.position, selectedType, ndb.ident, ndb.frequency, ndb.range);
      else if(selectedType == maptypes::ILS)
        addNavRangeRing(ils.position, selectedType, ils.ident, ils.frequency, ils.range);
    }
    else if(action == ui->actionMapRangeRings)
      addRangeRing(pos);
    else if(action == ui->actionMapSetMark)
      changeMark(pos);
    else if(action == ui->actionMapHideOneRangeRing)
    {
      rangeMarkers.removeAt(rangeMarkerIndex);
      update();
    }
    else if(action == ui->actionMapHideDistanceMarker)
    {
      distanceMarkers.removeAt(distMarkerIndex);
      update();
    }
    else if(action == ui->actionMapMeasureDistance || action == ui->actionMapMeasureRhumbDistance)
    {
      // Distance line
      maptypes::DistanceMarker dm;
      dm.type = selectedType;

      if(selectedType == maptypes::VOR)
      {
        dm.text = vor.ident + " " + formatter::formatDoubleUnit(vor.frequency / 1000., QString(), 2);
        dm.from = vor.position;
        dm.magvar = vor.magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::vorSymbolColor;
      }
      else if(selectedType == maptypes::NDB)
      {
        dm.text = ndb.ident + " " + formatter::formatDoubleUnit(ndb.frequency / 100., QString(), 2);
        dm.from = ndb.position;
        dm.magvar = ndb.magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::ndbSymbolColor;
      }
      else if(selectedType == maptypes::WAYPOINT)
      {
        dm.text = wp.ident;
        dm.from = wp.position;
        dm.magvar = wp.magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::waypointSymbolColor;
      }
      else if(selectedType == maptypes::AIRPORT)
      {
        dm.text = ap.name + " (" + ap.ident + ")";
        dm.from = ap.position;
        dm.magvar = ap.magvar;
        dm.hasMagvar = true;
        dm.color = mapcolors::colorForAirport(ap);
      }
      else
      {
        dm.hasMagvar = false;
        dm.from = pos;
        dm.color = dm.rhumbLine ? mapcolors::distanceColor : mapcolors::distanceRhumbColor;
      }

      dm.rhumbLine = action == ui->actionMapMeasureRhumbDistance;
      dm.position = pos;
      distanceMarkers.append(dm);

      mouseState = DISTANCE_DRAG;
      setContextMenuPolicy(Qt::NoContextMenu);
      currentDistanceMarkerIndex = distanceMarkers.size() - 1;
    }
  }
}

void NavMapWidget::addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type,
                                   const QString& ident, int frequency, int range)
{
  maptypes::RangeMarker ring;
  ring.type = type;
  ring.position = pos;

  if(type == maptypes::VOR || type == maptypes::ILS)
    ring.text = ident + " " + formatter::formatDoubleUnit(frequency / 1000., QString(), 2);
  else if(type == maptypes::NDB)
    ring.text = ident + " " + formatter::formatDoubleUnit(frequency / 100., QString(), 2);

  ring.ranges.append(range);
  rangeMarkers.append(ring);
  qDebug() << "navaid range" << ring.position;
  update();
}

void NavMapWidget::addRangeRing(const atools::geo::Pos& pos)
{
  maptypes::RangeMarker rings;
  rings.type = maptypes::NONE;
  rings.position = pos;
  rings.ranges = {50, 100, 200, 500};
  rangeMarkers.append(rings);

  qDebug() << "range rings" << rings.position;
  update();
}

void NavMapWidget::clearRangeRings()
{
  qDebug() << "range rings hide";
  rangeMarkers.clear();
  update();
}

bool NavMapWidget::eventFilter(QObject *obj, QEvent *e)
{
  if(e->type() == QEvent::MouseButtonDblClick)
  {
    qDebug() << "eventFilter mouseDoubleClickEvent";
    e->accept();
    event(e);
    return true;
  }
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

void NavMapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(mouseState == DISTANCE_DRAG || mouseState == DISTANCE_DRAG_CHANGE)
  {
    if(cursor().shape() != Qt::CrossCursor)
      setCursor(Qt::CrossCursor);

    qreal lon, lat;
    bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
    if(visible)
    {
      atools::geo::Pos p(lon, lat);
      if(!distanceMarkers.isEmpty())
        distanceMarkers[currentDistanceMarkerIndex].position = p;
    }
    setViewContext(Marble::Animation);
    event->accept();
    update();
  }
  else if(event->buttons() == Qt::NoButton)
  {
    if(getNearestDistanceMarkerIndex(event->pos().x(), event->pos().y(), 10) != -1)
    {
      // Change cursor at the end of an marker
      if(cursor().shape() != Qt::CrossCursor)
        setCursor(Qt::CrossCursor);
    }
    else if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);
  }
}

void NavMapWidget::mousePressEvent(QMouseEvent *event)
{
  qDebug() << "mousePressEvent";
  if(mouseState == DISTANCE_DRAG || mouseState == DISTANCE_DRAG_CHANGE)
    if(!distanceMarkers.isEmpty())
    {
      setCursor(Qt::ArrowCursor);
      if(event->button() == Qt::LeftButton)
      {
        qreal lon, lat;
        bool visible = geoCoordinates(event->pos().x(), event->pos().y(), lon, lat);
        if(visible)
          distanceMarkers[currentDistanceMarkerIndex].position = atools::geo::Pos(lon, lat);
      }
      else if(event->button() == Qt::RightButton)
      {
        if(mouseState == DISTANCE_DRAG)
          // Remove new one
          distanceMarkers.removeAt(currentDistanceMarkerIndex);
        else if(mouseState == DISTANCE_DRAG_CHANGE)
          // Replace modified one with backup
          distanceMarkers[currentDistanceMarkerIndex] = distanceMarkerBackup;
        currentDistanceMarkerIndex = -1;
      }

      event->accept();
    }
  if(mouseState == NONE && event->button() == Qt::RightButton)
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void NavMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  qDebug() << "mouseReleaseEvent";

  if(mouseState == DISTANCE_DRAG || mouseState == DISTANCE_DRAG_CHANGE)
  {
    // End dragging
    setViewContext(Marble::Still);
    mouseState = NONE;
    event->accept();
    update();
  }
  else if(mouseState == NONE && event->button() == Qt::LeftButton)
  {
    currentDistanceMarkerIndex = getNearestDistanceMarkerIndex(event->pos().x(), event->pos().y(), 10);
    if(currentDistanceMarkerIndex != -1)
    {
      // Found an end - create a backup and start dragging
      mouseState = DISTANCE_DRAG_CHANGE;
      distanceMarkerBackup = distanceMarkers.at(currentDistanceMarkerIndex);
      setContextMenuPolicy(Qt::NoContextMenu);
    }
  }
}

void NavMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  using namespace maptypes;

  qDebug() << "mouseDoubleClickEvent";

  CoordinateConverter conv(viewport());
  MapSearchResult mapSearchResult;
  const MapLayer *mapLayer = paintLayer->getMapLayer();
  mapQuery->getNearestObjects(conv, mapLayer,
                              paintLayer->getShownMapFeatures() &
                              (maptypes::AIRPORT_ALL | maptypes::VOR | maptypes::NDB | maptypes::WAYPOINT),
                              event->pos().x(), event->pos().y(), 10, mapSearchResult);

  getNearestHighlightMapObjects(event->x(), event->y(), 10, mapSearchResult);
  getNearestRouteMapObjects(event->x(), event->y(), 10,
                            parentWindow->getRouteController()->getRouteMapObjects(), mapSearchResult);

  if(!mapSearchResult.airports.isEmpty())
  {
  if(mapSearchResult.airports.at(0)->bounding.isPoint())
    showPos(mapSearchResult.airports.at(0)->bounding.getTopLeft());
  else
    showRect(mapSearchResult.airports.at(0)->bounding);
  }
  else if(!mapSearchResult.vors.isEmpty())
    showPos(mapSearchResult.vors.at(0)->position);
  else if(!mapSearchResult.ndbs.isEmpty())
    showPos(mapSearchResult.ndbs.at(0)->position);
  else if(!mapSearchResult.waypoints.isEmpty())
    showPos(mapSearchResult.waypoints.at(0)->position);
}

bool NavMapWidget::event(QEvent *event)
{
  using namespace maptypes;

  if(event->type() == QEvent::ToolTip)
  {
  QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

  CoordinateConverter conv(viewport());

  QString text;

  const MapLayer *mapLayer = paintLayer->getMapLayer();
  MapSearchResult mapSearchResult;
  mapQuery->getNearestObjects(conv, mapLayer, paintLayer->getShownMapFeatures(),
                              helpEvent->pos().x(), helpEvent->pos().y(), 10, mapSearchResult);

  getNearestHighlightMapObjects(helpEvent->pos().x(), helpEvent->pos().y(), 10, mapSearchResult);
  getNearestRouteMapObjects(helpEvent->pos().x(), helpEvent->pos().y(), 10,
                            parentWindow->getRouteController()->getRouteMapObjects(), mapSearchResult);

  for(const MapAirport *ap : mapSearchResult.airports)
  {
    if(!text.isEmpty())
      text += "<hr/>";

    text += ap->name + " (" + ap->ident + ")<br/>" +
            "Longest Runway: " + QLocale().toString(ap->longestRunwayLength) + " ft<br/>" +
            "Altitude: " + QLocale().toString(ap->altitude) + " ft<br/>";
    text += "Magvar: " + formatter::formatDoubleUnit(ap->magvar, QString(), 1) + " °<br/>";

    if(ap->hard())
      text += "Has Hard Runways<br/>";
    if(ap->soft())
      text += "Has Soft Runways<br/>";
    if(ap->water())
      text += "Has Water Runways<br/>";

    if(ap->towerFrequency > 0)
      text += "Tower: " + formatter::formatDoubleUnit(ap->towerFrequency / 1000., QString(), 2) + "<br/>";
    if(ap->atisFrequency > 0)
      text += "ATIS: " + formatter::formatDoubleUnit(ap->atisFrequency / 1000., QString(), 2) + "<br/>";
    if(ap->awosFrequency > 0)
      text += "AWOS: " + formatter::formatDoubleUnit(ap->awosFrequency / 1000., QString(), 2) + "<br/>";
    if(ap->asosFrequency > 0)
      text += "ASOS: " + formatter::formatDoubleUnit(ap->asosFrequency / 1000., QString(), 2) + "<br/>";
    if(ap->unicomFrequency > 0)
      text += "Unicom: " + formatter::formatDoubleUnit(ap->unicomFrequency / 1000., QString(), 2) + "<br/>";
  }

  for(const MapVor *vor : mapSearchResult.vors)
  {
    if(!text.isEmpty())
      text += "<hr/>";
    QString type;
    if(vor->dmeOnly)
      type = "DME";
    else if(vor->hasDme)
      type = "VORDME";
    else
      type = "VOR";

    text += type + ": " + vor->name + " (" + vor->ident + ")<br/>";
    text += "Freq: " + formatter::formatDoubleUnit(vor->frequency / 1000., QString(), 2) + " MHz<br/>";
    if(!vor->dmeOnly)
      text += "Magvar: " + formatter::formatDoubleUnit(vor->magvar, QString(), 1) + " °<br/>";
    text += "Range: " + QString::number(vor->range) + " nm<br/>";
  }

  for(const MapNdb *ndb : mapSearchResult.ndbs)
  {
    if(!text.isEmpty())
      text += "<hr/>";
    text += "NDB: " + ndb->name + " (" + ndb->ident + ")<br/>";
    text += "Freq: " + formatter::formatDoubleUnit(ndb->frequency / 100., QString(), 2) + " kHz<br/>";
    text += "Magvar: " + formatter::formatDoubleUnit(ndb->magvar, QString(), 1) + " °<br/>";
    text += "Range: " + QString::number(ndb->range) + " nm<br/>";
  }

  for(const MapWaypoint *wp : mapSearchResult.waypoints)
  {
    if(!text.isEmpty())
      text += "<hr/>";
    text += "Waypoint: " + wp->ident + "<br/>";
    text += "Magvar: " + formatter::formatDoubleUnit(wp->magvar, QString(), 1) + " °<br/>";
  }

  for(const MapMarker *m : mapSearchResult.markers)
  {
    if(!text.isEmpty())
      text += "<hr/>";
    text += "Marker: " + m->type + "<br/>";
  }

  if(mapLayer != nullptr && mapLayer->isAirportDiagram())
  {
    for(const MapAirport *ap : mapSearchResult.towers)
    {
      if(!text.isEmpty())
        text += "<hr/>";

      if(ap->towerFrequency > 0)
        text += "Tower: " + formatter::formatDoubleUnit(ap->towerFrequency / 1000., QString(), 2) + "<br/>";
      else
        text += "Tower<br/>";
    }
    for(const MapParking *p : mapSearchResult.parkings)
    {
      if(!text.isEmpty())
        text += "<hr/>";

      text += "Parking " + QString::number(p->number) + "<br/>" +
              p->name + "<br/>" +
              p->type + "<br/>" +
              QString::number(p->radius * 2) + " ft<br/>" +
              (p->jetway ? "Has Jetway<br/>" : "");

    }
    for(const MapHelipad *p : mapSearchResult.helipads)
    {
      if(!text.isEmpty())
        text += "<hr/>";

      text += "Helipad:<br/>" +
              p->surface + "<br/>" +
              p->type + "<br/>" +
              QString::number(p->width) + " ft diameter<br/>" +
              (p->closed ? "Is Closed<br/>" : "");

    }
  }

  if(!text.isEmpty())
  {
    QToolTip::showText(helpEvent->globalPos(), text.trimmed(), nullptr, QRect(), 3600 * 1000);
    event->accept();
  }
  else
  {
    QToolTip::hideText();
    event->accept();
  }

  return true;
  }

  return QWidget::event(event);
}

void NavMapWidget::paintEvent(QPaintEvent *paintEvent)
{
  const GeoDataLatLonAltBox visibleLatLonAltBox = viewport()->viewLatLonAltBox();
  if(viewContext() == Marble::Still && (zoom() != curZoom || visibleLatLonAltBox != curBox))
  {
    curZoom = zoom();
    curBox = visibleLatLonAltBox;

    qDebug() << "paintEvent map view has changed zoom" << curZoom << "distance" << distanceFromZoom(zoom());
    qDebug() << "pole" << curBox.containsPole() << curBox.toString(GeoDataCoordinates::Degree);
    history.addEntry(atools::geo::Pos(centerLongitude(), centerLatitude()), zoom());
  }

  MarbleWidget::paintEvent(paintEvent);
}

void NavMapWidget::getNearestRouteMapObjects(int xs, int ys, int screenDistance,
                                             const QList<RouteMapObject>& routeMapObjects,
                                             maptypes::MapSearchResult& mapobjects)
{
  using maptools::insertSortedByDistance;

  CoordinateConverter conv(viewport());
  int x, y;

  for(const RouteMapObject& obj : routeMapObjects)
    if(conv.wToS(obj.getPosition(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
      {
        switch(obj.getMapObjectType())
        {
          case maptypes::VOR :
            insertSortedByDistance(conv, mapobjects.vors, xs, ys, &obj.getVor());
            break;
          case maptypes::WAYPOINT:
            insertSortedByDistance(conv, mapobjects.waypoints, xs, ys, &obj.getWaypoint());
            break;
          case maptypes::NDB:
            insertSortedByDistance(conv, mapobjects.ndbs, xs, ys, &obj.getNdb());
            break;
          case maptypes::AIRPORT:
            insertSortedByDistance(conv, mapobjects.airports, xs, ys, &obj.getAirport());
            break;
        }
      }
}

void NavMapWidget::getNearestHighlightMapObjects(int xs, int ys, int screenDistance,
                                                 maptypes::MapSearchResult& mapobjects)
{
  using namespace maptypes;

  CoordinateConverter conv(viewport());
  int x, y;

  using maptools::insertSortedByDistance;

  for(const maptypes::MapAirport *obj : highlightMapObjects.airports)
    if(conv.wToS(obj->position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.airports, xs, ys, obj);

  for(const maptypes::MapVor *obj : highlightMapObjects.vors)
    if(conv.wToS(obj->position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.vors, xs, ys, obj);

  for(const maptypes::MapNdb *obj : highlightMapObjects.ndbs)
    if(conv.wToS(obj->position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.ndbs, xs, ys, obj);

  for(const maptypes::MapWaypoint *obj : highlightMapObjects.waypoints)
    if(conv.wToS(obj->position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        insertSortedByDistance(conv, mapobjects.waypoints, xs, ys, obj);
}

int NavMapWidget::getNearestDistanceMarkerIndex(int xs, int ys, int screenDistance)
{
  CoordinateConverter conv(viewport());
  int index = 0;
  int x, y;
  for(const maptypes::DistanceMarker& marker : distanceMarkers)
  {
    if(conv.wToS(marker.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        return index;

    index++;
  }
  return -1;
}

int NavMapWidget::getNearestRangeMarkerIndex(int xs, int ys, int screenDistance)
{
  CoordinateConverter conv(viewport());
  int index = 0;
  int x, y;
  for(const maptypes::RangeMarker& marker : rangeMarkers)
  {
    if(conv.wToS(marker.position, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
        return index;

    index++;
  }
  return -1;
}
