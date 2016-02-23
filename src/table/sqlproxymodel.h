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

#include <geo/pos.h>

class SqlModel;
class SqlProxyModel :
  public QSortFilterProxyModel
{
  Q_OBJECT

public:
  SqlProxyModel(QObject *parent, SqlModel *parentSqlModel);
  virtual ~SqlProxyModel();

  void setDistanceFilter(const atools::geo::Pos& centerPos, int minDistance, int maxDistance);
  void clearDistanceFilter();

private:
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
  virtual void sort(int column, Qt::SortOrder order) override;

  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  QVariantList getRawRowData(const QModelIndex& index) const;

  SqlModel *sqlModel = nullptr;
  atools::geo::Pos pos;
  int minDist = 0, maxDist = 0;
  bool distanceFilter = false;
};

#endif // SQLSORTFILTERPROXYMODEL_H
