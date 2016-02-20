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

#include "mappaintlayer.h"
#include "navmapwidget.h"
#include <marble/MarbleLocale.h>
#include <QContextMenuEvent>
#include "settings/settings.h"

#include <QSettings>

using namespace Marble;

NavMapWidget::NavMapWidget(QWidget *parent)
  : Marble::MarbleWidget(parent)
{
  MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);

  // MarbleLocale::MeasurementSystem distanceUnit;
  // distanceUnit = MarbleGlobal::getInstance()->locale()->measurementSystem();

  MapPaintLayer *layer = new MapPaintLayer(this);
  addLayer(layer);

  // MarbleWidgetInputHandler *localInputHandler = inputHandler();
  // MarbleAbstractPresenter *pres = new MarbleAbstractPresenter;
  // setInputHandler(nullptr);
}

void NavMapWidget::saveState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  s->setValue("Map/Zoom", zoom());
  s->setValue("Map/LonX", centerLongitude());
  s->setValue("Map/LatY", centerLatitude());
}

void NavMapWidget::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  if(s->contains("Map/Zoom"))
    setZoom(s->value("Map/Zoom").toInt());

  if(s->contains("Map/LonX") && s->contains("Map/LatY"))
    centerOn(s->value("Map/LonX").toDouble(), s->value("Map/LatY").toDouble(), false);
}
