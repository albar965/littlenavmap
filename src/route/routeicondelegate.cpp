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

#include "route/routeicondelegate.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"

#include <QPainter>

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
  int symbolSize = option.rect.height() - 4;
  int x = option.rect.x() + symbolSize;
  int y = option.rect.y() + symbolSize / 2 + 2;

  // Create a style copy to catch all active, inactive, highlighted and more colors
  QStyleOptionViewItem styleOption(option);
  styleOption.font.setBold(true);
  styleOption.displayAlignment = Qt::AlignRight;

  const RouteMapObject& mapObj = routeObjects.at(index.row());
  if(mapObj.getMapObjectType() == maptypes::AIRPORT && mapObj.getAirport().addon())
    // Italic for addons
    styleOption.font.setItalic(true);

  if(mapObj.getMapObjectType() == maptypes::INVALID)
  {
    // Used red text for invalid entries (for active window, inactive window an selected row)
    styleOption.palette.setColor(QPalette::Active, QPalette::Text, QColor(Qt::red));
    styleOption.palette.setColor(QPalette::Inactive, QPalette::Text, QColor(Qt::red));
    styleOption.palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(Qt::red));
  }

  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);

  // Draw the text
  QStyledItemDelegate::paint(painter, styleOption, index);

  // Draw the icon
  if(mapObj.getMapObjectType() == maptypes::AIRPORT)
    symbolPainter->drawAirportSymbol(painter, mapObj.getAirport(), x, y, symbolSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::VOR)
    symbolPainter->drawVorSymbol(painter, mapObj.getVor(), x, y, symbolSize, false, false, 0);
  else if(mapObj.getMapObjectType() == maptypes::NDB)
    symbolPainter->drawNdbSymbol(painter, mapObj.getNdb(), x, y, symbolSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::WAYPOINT)
    symbolPainter->drawWaypointSymbol(painter, mapObj.getWaypoint(), QColor(), x, y, symbolSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::USER)
    symbolPainter->drawUserpointSymbol(painter, x, y, symbolSize, false, false);
  else if(mapObj.getMapObjectType() == maptypes::INVALID)
    symbolPainter->drawWaypointSymbol(painter, mapObj.getWaypoint(), mapcolors::routeInvalidPointColor,
                                      x, y, symbolSize, false, false);
}
