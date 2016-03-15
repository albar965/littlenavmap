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

#include "airporticondelegate.h"
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

AirportIconDelegate::AirportIconDelegate(const ColumnList *columns)
  : cols(columns)
{
  symbolPainter = new SymbolPainter();
}

AirportIconDelegate::~AirportIconDelegate()
{
  delete symbolPainter;
}

void AirportIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
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
  MapAirport ap = mapAirport(sqlModel, idx.row());

  QFont font(painter->font());
  if(ap.flags.testFlag(SCENERY))
    font.setBold(true);
  if(ap.flags.testFlag(ADDON))
    font.setItalic(true);
  painter->setFont(font);

  if(idx.column() == sqlModel->getSortColumnIndex())
    painter->fillRect(option.rect, mapcolors::alternatingRowColor(idx.row(), true, option.state));

  if(option.state.testFlag(QStyle::State_Selected) && option.state.testFlag(QStyle::State_Active))
    painter->setPen(QApplication::palette().color(QPalette::Active, QPalette::HighlightedText));

  QRect textRect = option.rect;
  textRect.setWidth(textRect.width() - 1);
  painter->drawText(textRect, Qt::AlignRight, text);

  int symSize = option.rect.height() - 4;
  int w = painter->fontMetrics().maxWidth();
  symbolPainter->drawAirportSymbol(painter, ap,
                                   option.rect.x() + (option.rect.width() - w) / 2,
                                   option.rect.y() + symSize / 2 + 2,
                                   symSize,
                                   false, false);

  painter->restore();
}

QSize AirportIconDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_UNUSED(index);
  return QSize(option.fontMetrics.maxWidth() + option.rect.height() - 4, option.rect.height() - 4);
}

QVariant AirportIconDelegate::value(const SqlModel *sqlModel, int row, const QString& name) const
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

MapAirport AirportIconDelegate::mapAirport(const SqlModel *sqlModel, int row) const
{
  MapAirport ap;
  ap.longestRunwayHeading =
    static_cast<int>(std::roundf(value(sqlModel, row, "longest_runway_heading").toFloat()));

  ap.flags |= flag(sqlModel, row, "num_helipad", HELIPORT);
  ap.flags |= flag(sqlModel, row, "rating", SCENERY);
  ap.flags |= flag(sqlModel, row, "has_avgas", FUEL);
  ap.flags |= flag(sqlModel, row, "has_jetfuel", FUEL);
  ap.flags |= flag(sqlModel, row, "tower_frequency", TOWER);
  ap.flags |= flag(sqlModel, row, "is_closed", CLOSED);
  ap.flags |= flag(sqlModel, row, "is_military", MIL);
  ap.flags |= flag(sqlModel, row, "is_addon", ADDON);
  ap.flags |= flag(sqlModel, row, "num_approach", APPR);
  ap.flags |= flag(sqlModel, row, "num_runway_hard", HARD);
  ap.flags |= flag(sqlModel, row, "num_runway_soft", SOFT);
  ap.flags |= flag(sqlModel, row, "num_runway_water", WATER);
  ap.flags |= flag(sqlModel, row, "num_runway_light", LIGHT);

  return ap;
}

MapAirportFlags AirportIconDelegate::flag(const SqlModel *sqlModel, int row, const QString& field,
                                          MapAirportFlags flag) const
{
  QVariant val = value(sqlModel, row, field);
  if(val.isNull())
    return NONE;
  else
    return val.toInt() > 0 ? flag : NONE;
}
