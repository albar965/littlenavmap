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

#ifndef LITTLENAVMAP_SQLSORTFILTERPROXYMODEL_H
#define LITTLENAVMAP_SQLSORTFILTERPROXYMODEL_H

#include "geo/pos.h"

#include <QSortFilterProxyModel>

class SqlModel;

namespace sqlproxymodel {
enum SearchDirection
{
  // Numbers have to match index in the combo box
  ALL = 0,
  NORTH = 1,
  EAST = 2,
  SOUTH = 3,
  WEST = 4
};

}

class SqlProxyModel :
  public QSortFilterProxyModel
{
  Q_OBJECT

public:
  SqlProxyModel(QObject *parent, SqlModel *parentSqlModel);
  virtual ~SqlProxyModel();

  void setDistanceFilter(const atools::geo::Pos& center, sqlproxymodel::SearchDirection dir,
                         int minDistance, int maxDistance);
  void clearDistanceFilter();

  virtual void sort(int column, Qt::SortOrder order) override;

private:
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  virtual bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;
  virtual QVariant data(const QModelIndex& index, int role) const override;

  bool matchDist(const atools::geo::Pos& pos) const;
  atools::geo::Pos buildPos(int row) const;

  SqlModel *sqlModel = nullptr;
  atools::geo::Pos centerPos;
  sqlproxymodel::SearchDirection direction;
  int minDist = 0, maxDist = 0;

};

#endif // LITTLENAVMAP_SQLSORTFILTERPROXYMODEL_H
