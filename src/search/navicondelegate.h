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

#ifndef LITTLENAVMAP_NAVICONDELEGATE_H
#define LITTLENAVMAP_NAVICONDELEGATE_H

#include <QStyledItemDelegate>

class ColumnList;
class SymbolPainter;

/*
 * Paints navaid icons into the "ident" cell of the search result table view.
 */
class NavIconDelegate :
  public QStyledItemDelegate
{
  Q_OBJECT

public:
  NavIconDelegate(const ColumnList *columns);
  virtual ~NavIconDelegate();

private:
  const ColumnList *cols;
  SymbolPainter *symbolPainter;

  virtual void paint(QPainter *painter, const QStyleOptionViewItem& option,
                     const QModelIndex& index) const override;

};

#endif // LITTLENAVMAP_NAVICONDELEGATE_H
