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

#include "search/sqlproxymodel.h"

#include "geo/calculations.h"
#include "search/sqlmodel.h"

#include <QApplication>

using namespace atools::geo;

SqlProxyModel::SqlProxyModel(QObject *parent, SqlModel *parentSqlModel)
  : QSortFilterProxyModel(parent), sqlModel(parentSqlModel)
{
}

SqlProxyModel::~SqlProxyModel()
{

}

void SqlProxyModel::setDistanceFilter(const Pos& center, sqlproxymodel::SearchDirection dir,
                                      int minDistance, int maxDistance)
{
  minDist = minDistance;
  maxDist = maxDistance;
  centerPos = center;
  direction = dir;
}

void SqlProxyModel::clearDistanceFilter()
{
  centerPos = Pos();
}

bool SqlProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  Q_UNUSED(sourceParent);

  Pos pos = buildPos(sourceRow);
  float heading = normalizeCourse(centerPos.angleDegTo(pos));

  switch(direction)
  {
    case sqlproxymodel::ALL:
      return matchDist(pos);

    case sqlproxymodel::NORTH:
      if(292.5f <= heading || heading <= 67.5f)
        return matchDist(pos);
      else
        return false;

    case sqlproxymodel::EAST:
      if(22.5f <= heading && heading <= 157.5f)
        return matchDist(pos);
      else
        return false;

    case sqlproxymodel::SOUTH:
      if(112.5f <= heading && heading <= 247.5f)
        return matchDist(pos);
      else
        return false;

    case sqlproxymodel::WEST:
      if(202.5f <= heading && heading <= 337.5f)
        return matchDist(pos);
      else
        return false;
  }
  return true;
}

bool SqlProxyModel::matchDist(const Pos& pos) const
{
  float dist = meterToNm(pos.distanceMeterTo(centerPos));
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
    float distLeft = buildPos(sourceLeft.row()).distanceMeterTo(centerPos);
    float distRight = buildPos(sourceRight.row()).distanceMeterTo(centerPos);
    return distLeft < distRight;
  }
  else if(sqlModel->getColumnName(sourceLeft.column()) == "heading" &&
          sqlModel->getColumnName(sourceRight.column()) == "heading")
  {
    float headingLeft = normalizeCourse(centerPos.angleDegTo(buildPos(sourceLeft.row())));
    float headingRight = normalizeCourse(centerPos.angleDegTo(buildPos(sourceRight.row())));
    return headingLeft < headingRight;
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
      float dist = meterToNm(buildPos(mapToSource(index).row()).distanceMeterTo(centerPos));
      return QLocale().toString(dist, 'f', 1);
    }
    else if(role == Qt::TextAlignmentRole)
      return Qt::AlignRight;
  }
  else if(sqlModel->getColumnName(index.column()) == "heading")
  {
    if(role == Qt::DisplayRole)
    {
      float heading = normalizeCourse(centerPos.angleDegTo(buildPos(mapToSource(index).row())));
      return QLocale().toString(heading, 'f', 0);
    }
    else if(role == Qt::TextAlignmentRole)
      return Qt::AlignRight;
  }

  return QSortFilterProxyModel::data(index, role);
}

Pos SqlProxyModel::buildPos(int row) const
{
  return Pos(sqlModel->getRawData(row, "lonx").toFloat(),
             sqlModel->getRawData(row, "laty").toFloat());
}
