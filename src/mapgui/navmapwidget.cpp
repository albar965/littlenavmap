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

#include "ui_mainwindow.h"

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : Marble::MarbleWidget(parent), parentWindow(parent), db(sqlDb)
{
  installEventFilter(this);

  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);
  // inputHandler()->setInertialEarthRotationEnabled(false);
  // MarbleLocale::MeasurementSystem distanceUnit;
  // distanceUnit = MarbleGlobal::getInstance()->locale()->measurementSystem();

  paintLayer = new MapPaintLayer(this, db);
  addLayer(paintLayer);

  // MarbleWidgetInputHandler *handler = inputHandler();
  // handler->installEventFilter();
  // MarbleAbstractPresenter *pres = new MarbleAbstractPresenter;
  // setInputHandler(nullptr);
  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::mapContextMenu);
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

void NavMapWidget::showPoint(double lonX, double latY, int zoom)
{
  qDebug() << "NavMapWidget::showPoint";
  setZoom(zoom);
  centerOn(lonX, latY, false);
}

void NavMapWidget::mapContextMenu(const QPoint& pos)
{
  Q_UNUSED(pos);
  qInfo() << "tableContextMenu";

  QMenu m;
  m.addAction(parentWindow->getUi()->actionSetMark);

  QPoint cpos = QCursor::pos();
  QAction *act = m.exec(cpos);
  if(act == parentWindow->getUi()->actionSetMark)
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
  qDebug() << "mousePressEvent";
}

void NavMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
  qDebug() << "mouseReleaseEvent";
}

void NavMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
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
    const MapAirport *ap = paintLayer->getAirportAtPos(helpEvent->pos().x(), helpEvent->pos().y());
    if(ap != nullptr)
      QToolTip::showText(helpEvent->globalPos(), QString::number(ap->id) + "\n" + ap->ident + "\n" + ap->name);
    else
    {
      QToolTip::hideText();
      event->ignore();
    }

    return true;
  }
  return QWidget::event(event);
}
