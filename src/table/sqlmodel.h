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

#ifndef LITTLENAVMAP_SQLMODEL_H
#define LITTLENAVMAP_SQLMODEL_H

#include "geo/rect.h"

#include <QColor>
#include <QSqlQueryModel>

namespace atools {
namespace sql {
class SqlQuery;
class SqlDatabase;
}
}

class Column;
class ColumnList;

/*
 * Extends the QSqlQueryModel and adds query building based on filters, ordering
 * and grouping.
 */
class SqlModel :
  public QSqlQueryModel
{
  Q_OBJECT

public:
  SqlModel(QWidget *parent, atools::sql::SqlDatabase *sqlDb, const ColumnList *columnList);
  virtual ~SqlModel();

  /* Creates an include filer for value at index in the table */
  void filterIncluding(QModelIndex index);

  /* Creates an exclude filer for value at index in the table */
  void filterExcluding(QModelIndex index);

  /* Group by the column at index */
  void groupByColumn(QModelIndex index);

  /* Release grouping and go back to default view */
  void ungroup();

  /* Clear all filters, sort order and grouping and go back to default view */
  void reset();

  /* clear all filters */
  void resetSearch();

  /* Set header captions based on current query and the descriptions in
   * ColumnList*/
  void fillHeaderData();

  /* @return true if group by view is enabled */
  bool isGrouped() const;

  /* Get the sorted column index that was saved before grouping */
  int getLastSortIndex() const;

  /* Get the sort order that was saved before grouping */
  Qt::SortOrder getLastSortOrder() const;

  /* Get descriptor for column name */
  const Column *getColumnModel(const QString& colName) const;

  /* Get descriptor for column index */
  const Column *getColumnModel(int colIndex) const;

  /* Add a filter for a column. Placeholder and negation will be adapted to SQL
   * query */
  void filter(const Column *col, const QVariant& value, const QVariant& maxValue = QVariant());

  /* Operator to connect all conditions ("and" or "or") */
  void filterOperator(const QString& op);

  /* Get field data formatted for display as seen in the table view */
  QVariant getFormattedFieldData(const QModelIndex& index) const;

  /* Get row data formatted for display as seen in the table view */
  QVariantList getFormattedRowData(int row);

  /* Format given data for display */
  QString formatValue(const QString& colName, const QVariant& value) const;

  Qt::SortOrder getSortOrder() const;

  QString getSortColumn() const
  {
    return orderByCol;
  }

  QString groupByColumn() const
  {
    return groupByCol;
  }

  int getTotalRowCount() const
  {
    return totalRowCount;
  }

  QString getCurrentSqlQuery() const
  {
    return currentSqlQuery;
  }

  /* Emit signal fetchedMore */
  virtual void fetchMore(const QModelIndex& parent) override;

  /* Get unformatted data from the model */
  QVariantList getRawRowData(int row) const;
  QVariant getRawData(int row, int col) const;
  QVariant getRawData(int row, const QString& colname) const;
  QStringList getRawColumns() const;

  void resetSqlQuery();

  void filterByBoundingRect(const atools::geo::Rect& boundingRectangle);

signals:
  /* Emitted when more data was fetched*/
  void fetchedMore();

private:
  struct WhereCondition
  {
    QString oper; /* operator (like, not like) */
    QVariant value;
    const Column *col;
  };

  atools::geo::Rect boundingRect;

  /* Column header was clicked */
  virtual void sort(int column, Qt::SortOrder order) override;

  /* Format data for display */
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  /* Build full list of columns to query including group by and aggregated
   * columns */
  QString buildColumnList();

  /* Build where statement */
  QString buildWhere();

  /* Convert a value to string for the where clause */
  QString buildWhereValue(const WhereCondition& cond);

  /* Create SQL query and set it into the model */
  void buildQuery();

  /* Filter by value at index (context menu in table view) */
  void filterBy(QModelIndex index, bool exclude);
  QString  sortOrderToSql(Qt::SortOrder order);

  QString groupByCol, orderByCol, orderByOrder, whereOperator = "and";
  QString currentSqlQuery;

  int orderByColIndex = 0;
  QHash<QString, WhereCondition> whereConditionMap;
  atools::sql::SqlDatabase *db;
  const ColumnList *columns;
  QWidget *parentWidget;
  int totalRowCount = 0;

  QString lastOrderByCol;
  Qt::SortOrder lastOrderByOrder = Qt::DescendingOrder;

  /* Alternating colors for normal display and display of sorted column */
  QColor rowBgColor, rowAltBgColor, rowSortBgColor, rowSortAltBgColor;
  void clearWhereConditions();

};

#endif // LITTLENAVMAP_SQLMODEL_H
