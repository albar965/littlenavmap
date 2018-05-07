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

#ifndef LITTLENAVMAP_SQLPROXYMODEL_H
#define LITTLENAVMAP_SQLPROXYMODEL_H

#include "geo/pos.h"

#include <QSortFilterProxyModel>

class SqlModel;

namespace sqlproxymodel {

/* Search direction. This is not the precise direction but an approximation where the ranges overlap.
 * E.g. EAST is 22.5f <= heading && heading <= 157.5f */
enum SearchDirection
{
  /* Numbers have to match index in the combo box */
  ALL = 0,
  NORTH = 1,
  EAST = 2,
  SOUTH = 3,
  WEST = 4
};

}

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
  SqlProxyModel(QObject *parent, SqlModel *sqlModel);
  virtual ~SqlProxyModel();

  /*
   * Sets new distance search parameters.
   * @param center center point for filter
   * @param dir direction
   * @param minDistance minimum distance to center point in nautical miles
   * @param maxDistance maximum distance to center point in nautical miles
   */
  void setDistanceFilter(const atools::geo::Pos& center, sqlproxymodel::SearchDirection dir,
                         float minDistance, float maxDistance);

  /* Clear distance search and stop all filtering */
  void clearDistanceFilter();

  /* Sorts the model by column in the given order and fetches all data from the underlying model. */
  virtual void sort(int column, Qt::SortOrder order) override;

private:
  virtual QVariant data(const QModelIndex& index, int role) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
  virtual bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

  bool matchDistance(const atools::geo::Pos& pos) const;
  atools::geo::Pos buildPos(int row) const;

  /* Direction filter ranges are decreased by this value on each side */
  static float Q_DECL_CONSTEXPR DIR_RANGE_DEG = 22.5f;

  /* Direction filter parameters */
  static float Q_DECL_CONSTEXPR MIN_NORTH_DEG = 270.f + DIR_RANGE_DEG, MAX_NORTH_DEG = 90.f - DIR_RANGE_DEG;
  static float Q_DECL_CONSTEXPR MIN_EAST_DEG = 0.f + DIR_RANGE_DEG, MAX_EAST_DEG = 180.f - DIR_RANGE_DEG;
  static float Q_DECL_CONSTEXPR MIN_SOUTH_DEG = 90.f + DIR_RANGE_DEG, MAX_SOUTH_DEG = 270.f - DIR_RANGE_DEG;
  static float Q_DECL_CONSTEXPR MIN_WEST_DEG = 180.f + DIR_RANGE_DEG, MAX_WEST_DEG = 360.f - DIR_RANGE_DEG;

  SqlModel *sourceSqlModel = nullptr;
  atools::geo::Pos centerPos;
  sqlproxymodel::SearchDirection direction;
  float minDistMeter = 0.f, maxDistMeter = 0.f;

};

#endif // LITTLENAVMAP_SQLPROXYMODEL_H
