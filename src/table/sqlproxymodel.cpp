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

void SqlProxyModel::setDistanceFilter(const atools::geo::Pos& centerPos, sqlproxymodel::SearchDirection dir,
                                      int minDistance, int maxDistance)
{
  minDist = minDistance;
  maxDist = maxDistance;
  pos = centerPos;
  direction = dir;
}

void SqlProxyModel::clearDistanceFilter()
{
  pos = atools::geo::Pos();
}

bool SqlProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  Q_UNUSED(sourceRow);
  Q_UNUSED(sourceParent);

  if(pos.isValid())
  {
    using namespace atools::geo;

    float lonx = sqlModel->getRawData(sourceRow, "lonx").toFloat();
    float laty = sqlModel->getRawData(sourceRow, "laty").toFloat();

    float dist = meterToNm(Pos(lonx, laty).distanceMeterTo(pos));

    bool matchDist = dist >= minDist && dist <= maxDist;
    switch(direction)
    {
    case sqlproxymodel::ALL:
      return matchDist;

    case sqlproxymodel::NORTH:
      return laty > pos.getLatY() ? matchDist : false;

    case sqlproxymodel::EAST:
      return lonx > pos.getLonX() ? matchDist : false;

    case sqlproxymodel::SOUTH:
      return laty < pos.getLatY() ? matchDist : false;

    case sqlproxymodel::WEST:
      return lonx < pos.getLonX() ? matchDist : false;
    }
    return false;
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
