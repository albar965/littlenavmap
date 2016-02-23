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

#include "sqlproxymodel.h"
#include "logging/loggingdefs.h"
#include "geo/calculations.h"
#include "sqlmodel.h"

SqlProxyModel::SqlProxyModel(QObject *parent, SqlModel *parentSqlModel)
  : QSortFilterProxyModel(parent), sqlModel(parentSqlModel)
{

}

SqlProxyModel::~SqlProxyModel()
{

}

void SqlProxyModel::setDistanceFilter(const atools::geo::Pos& centerPos, int minDistance, int maxDistance)
{
  minDist = minDistance;
  maxDist = maxDistance;
  pos = centerPos;
  distanceFilter = true;
}

void SqlProxyModel::clearDistanceFilter()
{
  distanceFilter = false;
}

bool SqlProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  Q_UNUSED(sourceRow);
  Q_UNUSED(sourceParent);

  if(distanceFilter)
  {
    using namespace atools::geo;

    float lonx = sqlModel->getRawData(sourceRow, "lonx").toFloat();
    float laty = sqlModel->getRawData(sourceRow, "laty").toFloat();
    Pos apPos(lonx, laty);

    float dist = meterToNm(apPos.distanceMeterTo(pos));

    return dist >= minDist && dist <= maxDist;
  }
  return true;
}

void SqlProxyModel::sort(int column, Qt::SortOrder order)
{
  sourceModel()->sort(column, order);
}

QVariant SqlProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}
