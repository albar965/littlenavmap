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
#include "common/symbolpainter.h"
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
  int symSize = option.rect.height() - 4;
  int x = option.rect.x() + symSize;
  int y = option.rect.y() + symSize / 2 + 2;

  // Create a style copy
  QStyleOptionViewItem opt(option);
  opt.font.setBold(true);
  opt.displayAlignment = Qt::AlignRight;

  const RouteMapObject& mapObj = routeObjects.at(index.row());
  if(mapObj.getMapObjectType() == maptypes::AIRPORT && mapObj.getAirport().addon())
    // Italic for addons
    opt.font.setItalic(true);

  if(mapObj.getMapObjectType() == maptypes::INVALID)
  {
    // Used red text for invalid entries
    opt.palette.setColor(QPalette::Active, QPalette::Text, QColor(Qt::red));
    opt.palette.setColor(QPalette::Inactive, QPalette::Text, QColor(Qt::red));
    opt.palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(Qt::red));
  }

  // Draw the text
  QStyledItemDelegate::paint(painter, opt, index);

  // Draw the icon
  painter->setRenderHint(QPainter::Antialiasing);
  if(mapObj.getMapObjectType() == maptypes::AIRPORT)
    symbolPainter->drawAirportSymbol(painter, mapObj.getAirport(), x, y, symSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::VOR)
    symbolPainter->drawVorSymbol(painter, mapObj.getVor(), x, y, symSize, false, false, 0);
  else if(mapObj.getMapObjectType() == maptypes::NDB)
    symbolPainter->drawNdbSymbol(painter, mapObj.getNdb(), x, y, symSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::WAYPOINT)
    symbolPainter->drawWaypointSymbol(painter, mapObj.getWaypoint(), QColor(), x, y, symSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::USER)
    symbolPainter->drawUserpointSymbol(painter, x, y, symSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::INVALID)
    symbolPainter->drawWaypointSymbol(painter, mapObj.getWaypoint(), mapcolors::routeInvalidPointColor,
                                      x, y, symSize, false, false);

}
