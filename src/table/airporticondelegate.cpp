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
#include <QSqlRecord>

#include <common/maptypesfactory.h>

AirportIconDelegate::AirportIconDelegate(const ColumnList *columns)
  : cols(columns)
{
  symbolPainter = new SymbolPainter();
  mapTypesFactory = new MapTypesFactory();
}

AirportIconDelegate::~AirportIconDelegate()
{
  delete symbolPainter;
  delete mapTypesFactory;
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

  maptypes::MapAirport ap;
  mapTypesFactory->fillAirport(sqlModel->record(idx.row()), ap, true);

  // Create a style copy
  QStyleOptionViewItem opt(option);
  opt.displayAlignment = Qt::AlignRight;
  if(ap.scenery())
    opt.font.setBold(true);
  if(ap.addon())
    opt.font.setItalic(true);

  // Draw the text
  QStyledItemDelegate::paint(painter, opt, index);

  painter->setRenderHint(QPainter::Antialiasing);
  int symSize = option.rect.height() - 4;
  symbolPainter->drawAirportSymbol(painter, ap,
                                   option.rect.x() + symSize,
                                   option.rect.y() + symSize / 2 + 2,
                                   symSize,
                                   false, false);
}
