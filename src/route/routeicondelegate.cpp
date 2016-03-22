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

#include "routeicondelegate.h"
#include "logging/loggingdefs.h"
#include "mapgui/mapquery.h"
#include "mapgui/symbolpainter.h"
#include "common/mapcolors.h"

#include <QPainter>
#include <QSqlQueryModel>
#include <QApplication>

RouteIconDelegate::RouteIconDelegate(const QList<RouteMapObject>& routeMapObjects)
  : routeObjects(routeMapObjects)
{
  symbolPainter = new SymbolPainter();
}

RouteIconDelegate::~RouteIconDelegate()
{
  delete symbolPainter;
}

void RouteIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->fillRect(option.rect, option.backgroundBrush);

  const RouteMapObject& mapObj = routeObjects.at(index.row());

  QFont font(painter->font());
  font.setBold(true);

  int symSize = option.rect.height() - 4;
  int w = painter->fontMetrics().maxWidth();

  if(mapObj.getMapObjectType() == maptypes::AIRPORT)
  {
    if(mapObj.getAirport().flags.testFlag(maptypes::AP_ADDON))
      font.setItalic(true);

    symbolPainter->drawAirportSymbol(painter, mapObj.getAirport(),
                                     option.rect.x() + (option.rect.width() - w) / 2,
                                     option.rect.y() + symSize / 2 + 2,
                                     symSize,
                                     false, false);
  }
  else if(mapObj.getMapObjectType() == maptypes::VOR)
    symbolPainter->drawVorSymbol(painter, mapObj.getVor(),
                                 option.rect.x() + (option.rect.width() - w) / 2,
                                 option.rect.y() + symSize / 2 + 2,
                                 symSize, false, false, 0);

  else if(mapObj.getMapObjectType() == maptypes::NDB)
    symbolPainter->drawNdbSymbol(painter, mapObj.getNdb(),
                                 option.rect.x() + (option.rect.width() - w) / 2,
                                 option.rect.y() + symSize / 2 + 2,
                                 symSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::WAYPOINT)
    symbolPainter->drawWaypointSymbol(painter, mapObj.getWaypoint(),
                                      option.rect.x() + (option.rect.width() - w) / 2,
                                      option.rect.y() + symSize / 2 + 2,
                                      symSize, false, false);

  painter->setFont(font);

  if(!mapObj.isValid())
    painter->setPen(QColor(Qt::red));

  QRect textRect = option.rect;
  textRect.setWidth(textRect.width() - 1);
  painter->drawText(textRect, Qt::AlignRight, index.data(Qt::DisplayRole).toString());
  painter->restore();
}

QSize RouteIconDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_UNUSED(index);
  return QSize(option.fontMetrics.maxWidth() + option.rect.height() - 4, option.rect.height() - 4);
}
