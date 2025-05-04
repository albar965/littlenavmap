/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "search/navicondelegate.h"

#include "search/sqlmodel.h"
#include "search/sqlproxymodel.h"
#include "common/symbolpainter.h"
#include "sql/sqlrecord.h"
#include "common/maptypes.h"
#include "app/navapp.h"

#include <QPainter>

NavIconDelegate::NavIconDelegate(const ColumnList *columns)
  : cols(columns)
{
  symbolPainter = new SymbolPainter();
}

NavIconDelegate::~NavIconDelegate()
{
  delete symbolPainter;
}

void NavIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QModelIndex idx(index);
  const SqlModel *sqlModel = dynamic_cast<const SqlModel *>(index.model());
  if(sqlModel == nullptr)
  {
    // Convert index to source if distance search proxy is used
    const SqlProxyModel *sqlProxyModel = dynamic_cast<const SqlProxyModel *>(index.model());
    Q_ASSERT(sqlProxyModel != nullptr);
    sqlModel = dynamic_cast<const SqlModel *>(sqlProxyModel->sourceModel());
    idx = sqlProxyModel->mapToSource(index);
  }
  Q_ASSERT(sqlModel != nullptr);

  // Create a style copy
  QStyleOptionViewItem opt(option);
  opt.displayAlignment = Qt::AlignRight;
  opt.font.setBold(true);

  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);

  // Draw the text
  QStyledItemDelegate::paint(painter, opt, index);

  // Get nav type from SQL model
  QString navtype = sqlModel->getSqlRecord(idx.row()).valueStr("nav_type");
  map::MapTypes type = map::navTypeToMapType(navtype);

  float symbolSize = option.rect.height() - 6.f;
  float x = option.rect.x() + symbolSize;
  float y = option.rect.y() + symbolSize / 2.f + 2.f;

  if(type == map::WAYPOINT)
    // An empty waypoint is enough to draw the symbol
    symbolPainter->drawWaypointSymbol(painter, QColor(), x, y, symbolSize * 0.7f, false);
  else if(type == map::NDB)
    symbolPainter->drawNdbSymbol(painter, x, y, symbolSize, false, false, NavApp::isGuiStyleDark());
  else if(type == map::VOR)
  {
    map::MapVor vor;
    vor.dmeOnly = navtype == "D";
    vor.hasDme = navtype == "VD" || navtype == "D";
    vor.tacan = map::navTypeTacan(navtype);
    vor.vortac = map::navTypeVortac(navtype);
    symbolPainter->drawVorSymbol(painter, vor, x, y, symbolSize * 0.9f, 0.f,
                                 false /* routeFill */, false /* fast */, NavApp::isGuiStyleDark());
  }
}
