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
#include "mapquery.h"

#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoPainter.h>
#include <marble/LayerInterface.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleLocale.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>
#include <QContextMenuEvent>

using namespace Marble;

MapPaintLayer::MapPaintLayer(NavMapWidget *widget, atools::sql::SqlDatabase *sqlDb)
  : navMapWidget(widget), db(sqlDb)
{

}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  // qDebug() << "zoom" << marbleWidget->zoom() << "distance" << marbleWidget->distance();
  // qDebug() << "render cur box" << curBox.toString(GeoDataCoordinates::Degree);

  if(navMapWidget->getMark().isValid())
  {
    qreal x, y;
    bool hidden = false;
    bool visible = viewport->screenCoordinates(navMapWidget->getMark(), x, y, hidden);

    int xc = x, yc = y;
    if(visible && !hidden)
    {
      QPen bigPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);

      painter->setPen(bigPen);

      painter->drawLine(xc, yc - 10, xc, yc + 10);
      painter->drawLine(xc - 10, yc, xc + 10, yc);

      QPen smallPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);
      painter->setPen(smallPen);
      painter->drawLine(xc, yc - 8, xc, yc + 8);
      painter->drawLine(xc - 8, yc, xc + 8, yc);
    }
  }

  if(navMapWidget->distance() < 500)
  {
    MapQuery mq(db);

    if(navMapWidget->viewContext() == Marble::Still)
    {
      ap.clear();
      mq.getAirports(curBox, ap);
    }

    painter->mapQuality();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen bigPen(QBrush(QColor::fromRgb(0, 0, 0)), 3, Qt::SolidLine, Qt::RoundCap);
    QPen smallPen(QBrush(QColor::fromRgb(0, 0, 0)), 1, Qt::SolidLine, Qt::RoundCap);
    QPen backPen(QBrush(QColor::fromRgb(255, 255, 255, 200)), 1, Qt::SolidLine, Qt::RoundCap);

    if(navMapWidget->distance() > 200)
      painter->setPen(bigPen);
    else
      painter->setPen(smallPen);

    painter->setBrush(QBrush(QColor::fromRgb(255, 255, 255, 200)));
    QFont f = painter->font();

    if(navMapWidget->distance() > 200)
      f.setPointSizeF(f.pointSizeF() / 1.5f);

    painter->setFont(f);
    QFontMetrics metrics(f);

    for(const MapAirport& a : ap)
    {
      qreal x, y;
      bool hidden = false;
      bool visible = viewport->screenCoordinates(a.coords, x, y, hidden);

      int xc = x, yc = y;
      if(visible && !hidden)
      {
        if(navMapWidget->distance() > 200)
          painter->drawPoint(xc, yc);
        else
          painter->drawEllipse(xc, yc, 5, 5);

        xc += 5;
        int h = metrics.height();
        if(navMapWidget->distance() < 200)
        {

          int w = metrics.width(a.ident);
          painter->setPen(backPen);
          painter->drawRoundedRect(xc - 2, yc - h + metrics.descent(), w + 4, h, 5, 5);
          painter->setPen(smallPen);
          painter->drawText(xc, yc, a.ident);
        }

        if(navMapWidget->distance() < 100)
        {
          yc += h;
          int w = metrics.width(a.name);
          painter->setPen(backPen);
          painter->drawRoundedRect(xc - 2, yc - h + metrics.descent(), w + 4, h, 5, 5);
          painter->setPen(smallPen);
          painter->drawText(xc, yc, a.name);
        }
      }
    }
  }

  return true;
}

const MapAirport *MapPaintLayer::getAirportAtPos(int xs, int ys)
{
  const ViewportParams *vp = navMapWidget->viewport();

  for(const MapAirport& a : ap)
  {
    qreal x, y;
    bool visible = vp->screenCoordinates(a.coords, x, y);
    int xa = x, ya = y;

    if(visible)
      if((std::abs(xs - xa) + std::abs(ys - ya)) < 10)
        return &a;
  }

  return nullptr;
}
