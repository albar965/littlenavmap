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

#include "search/sqlproxymodel.h"

#include "geo/calculations.h"
#include "search/sqlmodel.h"
#include "common/unit.h"
#include "common/mapflags.h"

#include <QApplication>

using namespace atools::geo;

SqlProxyModel::SqlProxyModel(QObject *parent, SqlModel *sqlModel)
  : QSortFilterProxyModel(parent), sourceSqlModel(sqlModel)
{
}

SqlProxyModel::~SqlProxyModel()
{

}

void SqlProxyModel::setDistanceFilter(const Pos& center, sqlproxymodel::SearchDirection dir,
                                      float minDistance, float maxDistance)
{
  minDistMeter = nmToMeter(minDistance);
  maxDistMeter = nmToMeter(maxDistance);
  centerPos = center;
  direction = dir;
}

void SqlProxyModel::clearDistanceFilter()
{
  centerPos = Pos();
}

/* Does the filtering by minimum and maximum distance and direction */
bool SqlProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  Q_UNUSED(sourceParent);

  if(sourceSqlModel->isOverrideModeActive())
    return true;

  Pos pos = buildPos(sourceRow);
  float heading = normalizeCourse(centerPos.angleDegTo(pos));

  switch(direction)
  {
    case sqlproxymodel::ALL:
      // All directions
      return matchDistance(pos);

    case sqlproxymodel::NORTH:
      if(MIN_NORTH_DEG <= heading || heading <= MAX_NORTH_DEG)
        return matchDistance(pos);
      else
        return false;

    case sqlproxymodel::EAST:
      if(MIN_EAST_DEG <= heading && heading <= MAX_EAST_DEG)
        return matchDistance(pos);
      else
        return false;

    case sqlproxymodel::SOUTH:
      if(MIN_SOUTH_DEG <= heading && heading <= MAX_SOUTH_DEG)
        return matchDistance(pos);
      else
        return false;

    case sqlproxymodel::WEST:
      if(MIN_WEST_DEG <= heading && heading <= MAX_WEST_DEG)
        return matchDistance(pos);
      else
        return false;
  }
  return true;
}

bool SqlProxyModel::matchDistance(const Pos& pos) const
{
  if(sourceSqlModel->isOverrideModeActive())
    return true;

  float distMeter = pos.distanceMeterTo(centerPos);
  return distMeter >= minDistMeter && distMeter <= maxDistMeter;
}

void SqlProxyModel::sort(int column, Qt::SortOrder order)
{
  QSortFilterProxyModel::sort(column, order);

  // Update query in underlying SQL model
  sourceSqlModel->setSort(sourceSqlModel->getColumnName(column), order);

  // Fetch all data and set wait cursor
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  while(canFetchMore(QModelIndex()))
    fetchMore(QModelIndex());
  QGuiApplication::restoreOverrideCursor();
}

QVariant SqlProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return sourceModel()->headerData(section, orientation, role);
}

/* Defines greater and lower than for sorting of the two columns distance and heading */
bool SqlProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
  QString leftCol = sourceSqlModel->getColumnName(sourceLeft.column());
  QString rightCol = sourceSqlModel->getColumnName(sourceRight.column());

  if(leftCol == "distance" && rightCol == "distance")
  {
    // Sort by distance
    float distLeft = buildPos(sourceLeft.row()).distanceMeterTo(centerPos);
    float distRight = buildPos(sourceRight.row()).distanceMeterTo(centerPos);
    return distLeft < distRight;
  }
  else if(leftCol == "heading" && rightCol == "heading")
  {
    // Sort by heading
    float headingLeft = normalizeCourse(centerPos.angleDegTo(buildPos(sourceLeft.row())));
    float headingRight = normalizeCourse(centerPos.angleDegTo(buildPos(sourceRight.row())));
    return headingLeft < headingRight;
  }
  else
    // Let the model do the sorting for other columns
    return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
}

/* Returns the formatted data for the "distance" and "heading" column */
QVariant SqlProxyModel::data(const QModelIndex& index, int role) const
{
  if(sourceSqlModel->getColumnName(index.column()) == "distance")
  {
    if(role == Qt::DisplayRole)
      return Unit::distMeter(buildPos(mapToSource(index).row()).distanceMeterTo(centerPos), false);
    else if(role == Qt::TextAlignmentRole)
      return Qt::AlignRight;
  }
  else if(sourceSqlModel->getColumnName(index.column()) == "heading")
  {
    if(role == Qt::DisplayRole)
    {
      float heading = normalizeCourse(centerPos.angleDegTo(buildPos(mapToSource(index).row())));
      if(heading < map::INVALID_COURSE_VALUE)
        return QLocale().toString(heading, 'f', 0);
      else
        return QVariant();
    }
    else if(role == Qt::TextAlignmentRole)
      return Qt::AlignRight;
  }

  return QSortFilterProxyModel::data(index, role);
}

Pos SqlProxyModel::buildPos(int row) const
{
  return Pos(sourceSqlModel->getRawData(row, "lonx").toFloat(), sourceSqlModel->getRawData(row, "laty").toFloat());
}
