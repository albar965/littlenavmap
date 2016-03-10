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

#include "coordinateconverter.h"
#include "maplayer.h"
#include "ui_mainwindow.h"

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : Marble::MarbleWidget(parent), parentWindow(parent), db(sqlDb)
{
  installEventFilter(this);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  inputHandler()->setInertialEarthRotationEnabled(false);

  mapQuery = new MapQuery(db);
  mapQuery->initQueries();
  paintLayer = new MapPaintLayer(this, mapQuery);
  addLayer(paintLayer);

  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::mapContextMenu);
  connect(this, &MarbleWidget::zoomChanged, this, &NavMapWidget::zoomHasChanged);
}

NavMapWidget::~NavMapWidget()
{
  delete paintLayer;
  delete mapQuery;
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

void NavMapWidget::zoomHasChanged(int zoom)
{
  if(zoom != curZoom)
  {
    curZoom = zoom;
    curBox = viewport()->viewLatLonAltBox();
    qDebug() << "zoom" << curZoom << "distance" << distanceFromZoom(zoom);
    qDebug() << curBox.toString();
  }
}

void NavMapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  s->setValue("Map/Zoom", zoom());
  s->setValue("Map/LonX", centerLongitude());
  s->setValue("Map/LatY", centerLatitude());

  s->setValue("Map/MarkLonX", mark.longitude(GeoDataCoordinates::Degree));
  s->setValue("Map/MarkLatY", mark.latitude(GeoDataCoordinates::Degree));
}

void NavMapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  if(s->contains("Map/Zoom"))
    setZoom(s->value("Map/Zoom").toInt());

  if(s->contains("Map/LonX") && s->contains("Map/LatY"))
    centerOn(s->value("Map/LonX").toDouble(), s->value("Map/LatY").toDouble(), true);

  if(s->contains("Map/MarkLonX") && s->contains("Map/MarkLatY"))
  {
    mark.setLongitude(s->value("Map/MarkLonX").toDouble(), GeoDataCoordinates::Degree);
    mark.setLatitude(s->value("Map/MarkLatY").toDouble(), GeoDataCoordinates::Degree);

    atools::geo::Pos newPos(s->value("Map/MarkLonX").toDouble(), s->value("Map/MarkLatY").toDouble());
    qDebug() << "new mark" << newPos;
    emit markChanged(newPos);
  }
}

void NavMapWidget::showPoint(const atools::geo::Pos& pos, int zoom)
{
  qDebug() << "NavMapWidget::showPoint" << pos;
  setZoom(zoom);
  centerOn(pos.getLonX(), pos.getLatY(), false);
}

void NavMapWidget::showRect(const atools::geo::Rect& rect)
{
  qDebug() << "NavMapWidget::showRect" << rect;
  centerOn(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                            GeoDataCoordinates::Degree), false);
}

void NavMapWidget::changeMark(const atools::geo::Pos& pos)
{
  mark.setLongitude(pos.getLonX(), GeoDataCoordinates::Degree);
  mark.setLatitude(pos.getLatY(), GeoDataCoordinates::Degree);

  update();
  qDebug() << "new mark" << pos;
}

void NavMapWidget::mapContextMenu(const QPoint& pos)
{
  Q_UNUSED(pos);
  qInfo() << "tableContextMenu";

  Ui::MainWindow *ui = parentWindow->getUi();
  QMenu m;
  m.addAction(ui->actionSetMark);

  QString actionShowInSearchText;
  actionShowInSearchText = ui->actionShowInSearch->text();

  CoordinateConverter conv(viewport());
  MapSearchResult res;
  mapQuery->getNearestObjects(conv, pos.x(), pos.y(), 20, res);

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
  else if(act == ui->actionSetMark)
  {
    qreal lon, lat;
    if(geoCoordinates(pos.x(), pos.y(), lon, lat))
    {
      mark.setLongitude(lon, GeoDataCoordinates::Degree);
      mark.setLatitude(lat, GeoDataCoordinates::Degree);

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
  Q_UNUSED(event);
  qDebug() << "mouseDoubleClickEvent";
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
    mapQuery->getNearestObjects(conv, helpEvent->pos().x(), helpEvent->pos().y(), 20, res);

    const MapLayer *mapLayer = paintLayer->getMapLayer();

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

        text += "Helipad\n" +
                p.surface + "\n" +
                p.type + "\n" +
                QString::number(p.width) + " ft diameter\n" +
                (p.closed ? "Is Closed\n" : "");

      }
    }

    if(!res.airports.isEmpty())
    {
      MapAirport ap = *res.airports.first();
      text = ap.name + " (" + ap.ident + ")\n" +
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
