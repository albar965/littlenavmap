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

#include <marble/MarbleLocale.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleModel.h>

#include "ui_mainwindow.h"

using namespace Marble;

NavMapWidget::NavMapWidget(MainWindow *parent)
  : Marble::MarbleWidget(parent), parentWindow(parent)
{
  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);

  // MarbleLocale::MeasurementSystem distanceUnit;
  // distanceUnit = MarbleGlobal::getInstance()->locale()->measurementSystem();

  MapPaintLayer *layer = new MapPaintLayer(this);
  addLayer(layer);

  // MarbleWidgetInputHandler *localInputHandler = inputHandler();
  // MarbleAbstractPresenter *pres = new MarbleAbstractPresenter;
  // setInputHandler(nullptr);
  connect(this, &NavMapWidget::customContextMenuRequested, this, &NavMapWidget::mapContextMenu);

  GeoDataDocument *document = new GeoDataDocument;

  GeoDataIconStyle *style = new GeoDataIconStyle;
  GeoDataStyle *style2 = new GeoDataStyle;
  style2->setIconStyle(*style);

  place = new GeoDataPlacemark("Mark");
  place->setCoordinate(0., 0., 0., GeoDataCoordinates::Degree);
  document->append(place);

  // Add the document to MarbleWidget's tree model
  model()->treeModel()->addDocument(document);

}

void NavMapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  s->setValue("Map/Zoom", zoom());
  s->setValue("Map/LonX", centerLongitude());
  s->setValue("Map/LatY", centerLatitude());

  GeoDataCoordinates c = place->coordinate();

  s->setValue("Map/MarkLonX", c.longitude(GeoDataCoordinates::Degree));
  s->setValue("Map/MarkLatY", c.latitude(GeoDataCoordinates::Degree));
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
    place->setCoordinate(s->value("Map/MarkLonX").toDouble(), s->value("Map/MarkLatY").toDouble(),
                         0., GeoDataCoordinates::Degree);
    model()->treeModel()->updateFeature(place);

    atools::geo::Pos newPos(s->value("Map/MarkLonX").toDouble(), s->value("Map/MarkLatY").toDouble());
    qDebug() << "new mark" << newPos;
    emit markChanged(newPos);
  }
}

void NavMapWidget::showPoint(double lonX, double latY, int zoom)
{
  qDebug() << "NavMapWidget::showPoint";
  // GeoDataLookAt lookAt;
  // lookAt.setLatitude(latY);
  // lookAt.setLongitude(lonX);
  // lookAt.setAltitude(1000.);
  // lookAt.setRange(10);
  // flyTo(lookAt);
  // update();
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
      place->setCoordinate(lon, lat, 0., GeoDataCoordinates::Degree);
      model()->treeModel()->updateFeature(place);

      qDebug() << "new mark" << atools::geo::Pos(lon, lat);

      emit markChanged(atools::geo::Pos(lon, lat));
    }
  }
}
