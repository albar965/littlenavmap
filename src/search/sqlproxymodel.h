/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_SQLPROXYMODEL_H
#define LITTLENAVMAP_SQLPROXYMODEL_H

#include "geo/pos.h"
#include "search/sqlmodeltypes.h"

#include <QSortFilterProxyModel>

class SqlModel;

/*
 * Proxy that does the second stage (fine) filtering for distance searches. The default model does a simple
 * rectangle base query and passes the results to this proxy which filters by minimumn and maximum radius
 * and direction.
 * Dynamic loading on demand (like the SQL model does) does not work with this model. Therefore all results
 * have to be fetched.
 */
class SqlProxyModel :
  public QSortFilterProxyModel
{
  Q_OBJECT

public:
  explicit SqlProxyModel(QObject *parent, SqlModel *sqlModel);
  virtual ~SqlProxyModel() override;

  /*
   * Sets new distance search parameters.
   * @param center center point for filter
   * @param dir direction
   * @param minDistance minimum distance to center point in nautical miles
   * @param maxDistance maximum distance to center point in nautical miles
   */
  void setDistanceFilter(const atools::geo::Pos& center, sqlmodeltypes::SearchDirection dir, float minDistance, float maxDistance);

  /* Clear distance search and stop all filtering */
  void clearDistanceFilter();

  /* Sorts the model by column in the given order and fetches all data from the underlying model. */
  virtual void sort(int column, Qt::SortOrder order) override;

private:
  /* Returns the formatted data for the "distance" and "heading" column */
  virtual QVariant data(const QModelIndex& index, int role) const override;

  /* Returns the data for the given role and section in the header with the specified orientation. */
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  /* Does the filtering by minimum and maximum distance and direction */
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex&) const override;

  /* Defines greater and lower than for sorting of the two columns distance and heading */
  virtual bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

  bool matchDistance(const atools::geo::Pos& pos) const;

  /* Build position object from SQL row data */
  atools::geo::Pos buildPos(int row) const;

  /* Direction filter ranges are decreased by this value on each side */
  static float constexpr DIR_RANGE_DEG = 22.5f;

  /* Direction filter parameters */
  static float constexpr MIN_NORTH_DEG = 270.f + DIR_RANGE_DEG, MAX_NORTH_DEG = 90.f - DIR_RANGE_DEG;
  static float constexpr MIN_EAST_DEG = 0.f + DIR_RANGE_DEG, MAX_EAST_DEG = 180.f - DIR_RANGE_DEG;
  static float constexpr MIN_SOUTH_DEG = 90.f + DIR_RANGE_DEG, MAX_SOUTH_DEG = 270.f - DIR_RANGE_DEG;
  static float constexpr MIN_WEST_DEG = 180.f + DIR_RANGE_DEG, MAX_WEST_DEG = 360.f - DIR_RANGE_DEG;

  SqlModel *sourceSqlModel = nullptr;
  atools::geo::Pos centerPos;
  sqlmodeltypes::SearchDirection direction;
  float minDistMeter = 0.f, maxDistMeter = 0.f;

};

#endif // LITTLENAVMAP_SQLPROXYMODEL_H
