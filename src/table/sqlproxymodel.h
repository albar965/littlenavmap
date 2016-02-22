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

#ifndef SQLSORTFILTERPROXYMODEL_H
#define SQLSORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class SqlProxyModel :
  public QSortFilterProxyModel
{
  Q_OBJECT

public:
  SqlProxyModel(QObject *parent = 0);
  virtual ~SqlProxyModel();

  // QSortFilterProxyModel interface
protected:
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

#endif // SQLSORTFILTERPROXYMODEL_H
