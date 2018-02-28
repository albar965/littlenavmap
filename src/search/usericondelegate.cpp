/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "search/usericondelegate.h"

#include "search/sqlmodel.h"
#include "common/symbolpainter.h"
#include "sql/sqlrecord.h"
#include "userdata/userdataicons.h"

#include <QPainter>

UserIconDelegate::UserIconDelegate(const ColumnList *columns, UserdataIcons *userdataIcons)
  : cols(columns), icons(userdataIcons)
{
  symbolPainter = new SymbolPainter();
}

UserIconDelegate::~UserIconDelegate()
{
  delete symbolPainter;
}

void UserIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
  QModelIndex idx(index);
  const SqlModel *sqlModel = dynamic_cast<const SqlModel *>(index.model());
  Q_ASSERT(sqlModel != nullptr);

  // Create a style copy
  QStyleOptionViewItem opt(option);
  opt.displayAlignment = Qt::AlignRight;
  opt.font.setBold(true);

  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);

  // Draw the text
  QStyledItemDelegate::paint(painter, opt, index);

  // Get icon type from SQL model
  QString type = sqlModel->getSqlRecord(idx.row()).valueStr("type");

  // Draw icon
  painter->drawPixmap(option.rect.x() + 2, option.rect.y() + 1, *icons->getIconPixmap(type, option.rect.height() - 2));
}
