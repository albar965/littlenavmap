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

#include "table/sqlproxymodel.h"
#include "geo/calculations.h"
#include "table/sqlmodel.h"
#include "table/formatter.h"

#include <QApplication>

SqlProxyModel::SqlProxyModel(QObject *parent, SqlModel *parentSqlModel)
  : QSortFilterProxyModel(parent), sqlModel(parentSqlModel)
{
}

SqlProxyModel::~SqlProxyModel()
{

}

void SqlProxyModel::setDistanceFilter(const atools::geo::Pos& center, sqlproxymodel::SearchDirection dir,
                                      int minDistance, int maxDistance)
{
  minDist = minDistance;
  maxDist = maxDistance;
  centerPos = center;
  direction = dir;
}

void SqlProxyModel::clearDistanceFilter()
{
  centerPos = atools::geo::Pos();
}

bool SqlProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  Q_UNUSED(sourceParent);
  using namespace atools::geo;

  float lonx = sqlModel->getRawData(sourceRow, "lonx").toFloat();
  float laty = sqlModel->getRawData(sourceRow, "laty").toFloat();

  switch(direction)
  {
  case sqlproxymodel::ALL:
    return matchDist(lonx, laty);

  case sqlproxymodel::NORTH:
    if(laty > centerPos.getLatY())
      return matchDist(lonx, laty);
    else
      return false;

  case sqlproxymodel::EAST:
    if(lonx > centerPos.getLonX())
      return matchDist(lonx, laty);
    else
      return false;

  case sqlproxymodel::SOUTH:
    if(laty < centerPos.getLatY())
      return matchDist(lonx, laty);
    else
      return false;

  case sqlproxymodel::WEST:
    if(lonx < centerPos.getLonX())
      return matchDist(lonx, laty);
    else
      return false;
  }
  return true;
}

bool SqlProxyModel::matchDist(float lonx, float laty) const
{
  using namespace atools::geo;

  float dist = meterToNm(Pos(lonx, laty).distanceMeterTo(centerPos));
  return dist >= minDist && dist <= maxDist;
}

void SqlProxyModel::sort(int column, Qt::SortOrder order)
{
  QSortFilterProxyModel::sort(column, order);
  sqlModel->setSort(sqlModel->getColumnName(column), order);

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  while(canFetchMore(QModelIndex()))
    fetchMore(QModelIndex());
  QGuiApplication::restoreOverrideCursor();
}

QVariant SqlProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}

bool SqlProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
  if(sqlModel->getColumnName(sourceLeft.column()) == "distance" &&
     sqlModel->getColumnName(sourceRight.column()) == "distance")
  {
    float lonxLeft = sqlModel->getRawData(sourceLeft.row(), "lonx").toFloat();
    float latyLeft = sqlModel->getRawData(sourceLeft.row(), "laty").toFloat();
    float distLeft = atools::geo::Pos(lonxLeft, latyLeft).distanceMeterTo(centerPos);

    float lonxRight = sqlModel->getRawData(sourceRight.row(), "lonx").toFloat();
    float latyRight = sqlModel->getRawData(sourceRight.row(), "laty").toFloat();
    float distRight = atools::geo::Pos(lonxRight, latyRight).distanceMeterTo(centerPos);
    return distLeft < distRight;
  }
  else
    return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
}

QVariant SqlProxyModel::data(const QModelIndex& index, int role) const
{
  if(sqlModel->getColumnName(index.column()) == "distance")
  {
    if(role == Qt::DisplayRole)
    {
      QModelIndex i = mapToSource(index);
      float lonxLeft = sqlModel->getRawData(i.row(), "lonx").toFloat();
      float latyLeft = sqlModel->getRawData(i.row(), "laty").toFloat();
      float dist = atools::geo::meterToNm(atools::geo::Pos(lonxLeft, latyLeft).distanceMeterTo(centerPos));

      return formatter::formatDoubleUnit(dist, QString(), 2);
    }
    else if(role == Qt::TextAlignmentRole)
      return Qt::AlignRight;
  }

  return QSortFilterProxyModel::data(index, role);
}
