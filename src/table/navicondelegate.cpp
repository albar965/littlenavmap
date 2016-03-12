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

#include "navicondelegate.h"
#include "column.h"
#include "columnlist.h"
#include "sqlmodel.h"
#include "sqlproxymodel.h"
#include "logging/loggingdefs.h"
#include "mapgui/mapquery.h"
#include "mapgui/symbolpainter.h"
#include "common/mapcolors.h"

#include <QPainter>
#include <QSqlQueryModel>
#include <QApplication>

NavIconDelegate::NavIconDelegate(const ColumnList *columns)
  : cols(columns)
{
  symbolPainter = new SymbolPainter();
}

NavIconDelegate::~NavIconDelegate()
{
  delete symbolPainter;
}

void NavIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
  QModelIndex idx(index);
  const SqlModel *sqlModel = dynamic_cast<const SqlModel *>(index.model());
  if(sqlModel == nullptr)
  {
    const SqlProxyModel *sqlProxyModel = dynamic_cast<const SqlProxyModel *>(index.model());
    Q_ASSERT(sqlProxyModel != nullptr);
    sqlModel = dynamic_cast<const SqlModel *>(sqlProxyModel->sourceModel());
    idx = sqlProxyModel->mapToSource(index);
  }
  Q_ASSERT(sqlModel != nullptr);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QString text = idx.data().toString();
  QString navtype = value(sqlModel, idx.row(), "nav_type").toString();

  if(idx.column() == sqlModel->getSortColumnIndex() &&
     (option.state & QStyle::State_Selected) == 0)
    painter->fillRect(option.rect, mapcolors::alternatingRowColor(idx.row(), true));

  QFont font(painter->font());
  font.setBold(true);
  painter->setFont(font);

  QRect textRect = option.rect;
  textRect.setWidth(textRect.width() - 1);
  painter->drawText(textRect, Qt::AlignRight, text);

  int symSize = option.rect.height() - 4;
  int w = painter->fontMetrics().maxWidth();

  if(navtype == "WAYPOINT")
    symbolPainter->drawWaypointSymbol(painter, MapWaypoint(),
                                      option.rect.x() + (option.rect.width() - w) / 2,
                                      option.rect.y() + symSize / 2 + 2,
                                      symSize,
                                      false);
  else if(navtype == "NDB")
    symbolPainter->drawNdbSymbol(painter, MapNdb(),
                                 option.rect.x() + (option.rect.width() - w) / 2,
                                 option.rect.y() + symSize / 2 + 2,
                                 symSize,
                                 false);
  else if(navtype == "VOR" || navtype == "VORDME" || navtype == "DME")
  {
    MapVor vor;
    vor.dmeOnly = navtype == "DME";
    vor.hasDme = navtype == "VORDME" || navtype == "DME";

    symbolPainter->drawVorSymbol(painter, vor,
                                 option.rect.x() + (option.rect.width() - w) / 2,
                                 option.rect.y() + symSize / 2 + 2,
                                 symSize,
                                 false, 0);
  }

  painter->restore();
}

QSize NavIconDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_UNUSED(index);
  return QSize(option.fontMetrics.maxWidth() + option.rect.height() - 4, option.rect.height() - 4);
}

QVariant NavIconDelegate::value(const SqlModel *sqlModel, int row, const QString& name) const
{
  const Column *col = cols->getColumn(name);
  Q_ASSERT(col != nullptr);

  QVariant data = sqlModel->getRawData(row, col->getIndex());

  if(data.isValid())
    return data;
  else
    qCritical() << "sibling for column" << name << "not found";

  return QVariant();
}
