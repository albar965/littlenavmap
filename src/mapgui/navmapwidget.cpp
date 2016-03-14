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

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : Marble::MarbleWidget(parent), parentWindow(parent), db(sqlDb)
{
  installEventFilter(this);

  // Set the map quality to gain speed
  setMapQualityForViewContext(NormalQuality, Still);
  setMapQualityForViewContext(LowQuality, Animation);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapQuery = new MapQuery(db);
  mapQuery->initQueries();
  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::contextMenu);
}

NavMapWidget::~NavMapWidget()
{
  delete paintLayer;
  delete mapQuery;
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

void NavMapWidget::setShowMapFeatures(maptypes::ObjectTypes type, bool show)
{
  qDebug() << "setShowMapFeatures" << type << "show" << show;
  paintLayer->setShowMapFeatures(type, show);
}

void NavMapWidget::setDetailFactor(int factor)
{
  qDebug() << "setDetailFactor" << factor;
  paintLayer->setDetailFactor(factor);
}

void NavMapWidget::preDatabaseLoad()
{
  paintLayer->preDatabaseLoad();
  mapQuery->deInitQueries();
}

void NavMapWidget::postDatabaseLoad()
{
  paintLayer->postDatabaseLoad();
  mapQuery->initQueries();
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
  update();
}

void NavMapWidget::changeHome()
{
  homePos = atools::geo::Pos(centerLongitude(), centerLatitude());
  homeZoom = zoom();
  update();
}

void NavMapWidget::contextMenu(const QPoint& pos)
{
  Q_UNUSED(pos);
  qInfo() << "tableContextMenu";

  Ui::MainWindow *ui = parentWindow->getUi();
  QMenu m;
  m.addAction(ui->actionMapSetMark);
  m.addAction(ui->actionMapSetHome);

  QString actionShowInSearchText;
  actionShowInSearchText = ui->actionShowInSearch->text();

  CoordinateConverter conv(viewport());
  MapSearchResult res;
  mapQuery->getNearestObjects(conv, paintLayer->getMapLayer(), maptypes::ALL, pos.x(), pos.y(), 20, res);

  MapAirport ap;
  if(!res.airports.isEmpty())
    ap = *res.airports.first();

  if(ap.valid)
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().
                                    arg(ap.name + " (" + ap.ident + ")"));
  else
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(tr("Map Object")));

  ui->actionShowInSearch->setDisabled(ap.valid);

  m.addAction(ui->actionShowInSearch);
  QPoint cpos = QCursor::pos();
  QAction *act = m.exec(cpos);
  if(act == ui->actionShowInSearch)
  {
    qDebug() << "SearchController::objectSelected type" << maptypes::AIRPORT << "ident" << ap.ident;

    emit objectSelected(maptypes::AIRPORT, ap.ident, QString());
  }
  else if(act == ui->actionMapSetMark)
  {
    qreal lon, lat;
    if(geoCoordinates(pos.x(), pos.y(), lon, lat))
    {
      markPos = atools::geo::Pos(lon, lat);

      update();
      qDebug() << "new mark" << atools::geo::Pos(lon, lat);

      emit markChanged(atools::geo::Pos(lon, lat));
    }
  }

  ui->actionShowInSearch->setText(actionShowInSearchText);
}

bool NavMapWidget::eventFilter(QObject *obj, QEvent *e)
{
  if(e->type() == QEvent::MouseButtonDblClick)
  {
    e->accept();
    qDebug() << "eventFilter mouseDoubleClickEvent";
    event(e);
    return true;
  }
  Marble::MarbleWidget::eventFilter(obj, e);
  return false;
}

void NavMapWidget::mousePressEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  qDebug() << "mousePressEvent";
}

void NavMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  qDebug() << "mouseReleaseEvent";
}

void NavMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << "mouseDoubleClickEvent";

  CoordinateConverter conv(viewport());
  MapSearchResult res;
  const MapLayer *mapLayer = paintLayer->getMapLayer();
  mapQuery->getNearestObjects(conv, mapLayer, maptypes::AIRPORT, event->pos().x(), event->pos().y(), 20, res);
  if(!res.airports.isEmpty())
    showRect(res.airports.at(0)->bounding);
}

void NavMapWidget::mouseMoveEvent(QMouseEvent *event)
{
  if(event->buttons() == Qt::NoButton)
    if(cursor().shape() != Qt::ArrowCursor)
      setCursor(Qt::ArrowCursor);
}

bool NavMapWidget::event(QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

    CoordinateConverter conv(viewport());
    MapSearchResult res;

    const MapLayer *mapLayer = paintLayer->getMapLayer();
    mapQuery->getNearestObjects(conv, mapLayer, paintLayer->getShownMapFeatures(),
                                helpEvent->pos().x(), helpEvent->pos().y(), 20, res);

    QString text;
    if(mapLayer != nullptr && mapLayer->isAirportDiagram())
    {
      if(!res.towers.isEmpty())
      {
        const MapAirport& ap = *res.towers.first();

        if(ap.towerFrequency > 0)
          text += "Tower: " + formatter::formatDoubleUnit(ap.towerFrequency / 1000., QString(), 2) + "\n";
        else
          text += "Tower\n";
      }
      if(!res.parkings.isEmpty())
      {
        const MapParking& p = *res.parkings.first();

        text += "Parking " + QString::number(p.number) + "\n" +
                p.name + "\n" +
                p.type + "\n" +
                QString::number(p.radius * 2) + " ft diameter\n" +
                (p.jetway ? "Has Jetway\n" : "");

      }
      if(!res.helipads.isEmpty())
      {
        const MapHelipad& p = *res.helipads.first();

        text += "Helipad:\n" +
                p.surface + "\n" +
                p.type + "\n" +
                QString::number(p.width) + " ft diameter\n" +
                (p.closed ? "Is Closed\n" : "");

      }
    }

    if(!res.airports.isEmpty())
    {
      MapAirport ap = *res.airports.first();
      text += ap.name + " (" + ap.ident + ")\n" +
              "Longest Runway: " + QLocale().toString(ap.longestRunwayLength) + " ft\n" +
              "Altitude: " + QLocale().toString(ap.altitude) + " ft\n";

      if(ap.hard())
        text += "Has Hard Runways\n";
      if(ap.soft())
        text += "Has Soft Runways\n";
      if(ap.water())
        text += "Has Water Runways\n";

      if(ap.towerFrequency > 0)
        text += "Tower: " + formatter::formatDoubleUnit(ap.towerFrequency / 1000., QString(), 2) + "\n";
      if(ap.atisFrequency > 0)
        text += "ATIS: " + formatter::formatDoubleUnit(ap.atisFrequency / 1000., QString(), 2) + "\n";
      if(ap.awosFrequency > 0)
        text += "AWOS: " + formatter::formatDoubleUnit(ap.awosFrequency / 1000., QString(), 2) + "\n";
      if(ap.asosFrequency > 0)
        text += "ASOS: " + formatter::formatDoubleUnit(ap.asosFrequency / 1000., QString(), 2) + "\n";
      if(ap.unicomFrequency > 0)
        text += "Unicom: " + formatter::formatDoubleUnit(ap.unicomFrequency / 1000., QString(), 2) + "\n";
    }

    if(!res.vors.isEmpty())
    {
      MapVor vor = *res.vors.first();
      text += "VOR: " + vor.name + " (" + vor.ident + ")\n";
    }

    if(!res.ndbs.isEmpty())
    {
      MapNdb ndb = *res.ndbs.first();
      text += "NDB: " + ndb.name + " (" + ndb.ident + ")\n";
    }

    if(!res.waypoints.isEmpty())
    {
      MapWaypoint wp = *res.waypoints.first();
      text += "Waypoint: " + wp.ident + "\n";
    }

    if(!res.markers.isEmpty())
    {
      MapMarker wp = *res.markers.first();
      text += "Marker: " + wp.type + "\n";
    }

    if(!text.isEmpty())
      QToolTip::showText(helpEvent->globalPos(), text.trimmed());
    else
    {
      QToolTip::hideText();
      event->ignore();
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
    qDebug() << curBox.toString();
    history.addEntry(atools::geo::Pos(centerLongitude(), centerLatitude()), zoom());
  }

  MarbleWidget::paintEvent(paintEvent);
}
