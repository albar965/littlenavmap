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
#include <QContextMenuEvent>

using namespace Marble;

MapPaintLayer::MapPaintLayer(MarbleWidget *widget) :
  m_widget(widget)
{

}

bool MapPaintLayer::render(GeoPainter *painter,
                           ViewportParams *viewport,
                           const QString& renderPos,
                           GeoSceneLayer *layer)
{
  // qDebug() << ;
  // Have window title reflect the current paint layer
  GeoDataCoordinates home(8.26589, 50.29824, 0.0, GeoDataCoordinates::Degree);
  painter->mapQuality();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->setPen(QPen(QBrush(QColor::fromRgb(255, 0, 0, 200)), 3.0, Qt::SolidLine, Qt::RoundCap));

  if(m_widget->viewContext() == Marble::Still)
    painter->drawEllipse(home, 10, 5, true);

  GeoDataCoordinates France(2.2, 48.52, 0.0, GeoDataCoordinates::Degree);
  painter->setPen(QColor(0, 0, 0));
  painter->drawText(France, "France");

  GeoDataCoordinates Canada(-77.02, 48.52, 0.0, GeoDataCoordinates::Degree);
  painter->setPen(QColor(0, 0, 0));
  painter->drawText(Canada, "Canada");

  // A line from France to Canada without tessellation

  GeoDataLineString shapeNoTessellation(NoTessellation);
  shapeNoTessellation << France << Canada;

  painter->setPen(QColor(255, 0, 0));
  painter->drawPolyline(shapeNoTessellation);

  // The same line, but with tessellation

  GeoDataLineString shapeTessellate(Tessellate);
  shapeTessellate << France << Canada;

  painter->setPen(QColor(0, 255, 0));
  painter->drawPolyline(shapeTessellate);

  // Now following the latitude circles

  GeoDataLineString shapeLatitudeCircle(RespectLatitudeCircle | Tessellate);
  shapeLatitudeCircle << France << Canada;

  painter->setPen(QColor(0, 0, 255));
  painter->drawPolyline(shapeLatitudeCircle);

  // RespectLatitudeCircle = 0x2,
  // FollowGround = 0x4,
  // RotationIndicatesFill = 0x8,
  // SkipLatLonNormalization = 0x10
  return true;
}
