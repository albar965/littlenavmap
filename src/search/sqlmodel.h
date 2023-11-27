/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
#include "search/querybuilder.h"
#include "search/sqlmodeltypes.h"

#include <QSqlQueryModel>

namespace atools {
namespace sql {
class SqlQuery;
class SqlDatabase;
class SqlRecord;
}
}

class Column;
class ColumnList;

/*
 * Extends the QSqlQueryModel and adds query building based on filters and ordering.
 */
class SqlModel :
  public QSqlQueryModel
{
  Q_OBJECT

public:
  /*
   * @param parent parent widget
   * @param sqlDb database to use
   * @param columnList column descriptors that will be used to build the SQL queries
   */
  explicit SqlModel(QWidget *parent, atools::sql::SqlDatabase *sqlDb, const ColumnList *columnList);
  virtual ~SqlModel() override;

  /* Filter by using query builder callback */
  void filterByBuilder(const QWidget *widget);

  /* Creates an include filer for value at index in the table. Uses exact query value in double
   * quotes resulting in like "AAA" instead of like "%AAA%" */
  void filterIncluding(QModelIndex index, bool forceQueryBuilder, bool exact);

  /* Creates an exclude filer for value at index in the table */
  void filterExcluding(QModelIndex index, bool forceQueryBuilder, bool exact);

  /* Clear all filters, sort order and go back to default view */
  void resetView();
  void resetSort();

  /* clear all filters */
  void resetSearch();

  /* Set header captions based on current query and the descriptions in
   * ColumnList*/
  void fillHeaderData();

  /* Get descriptor for column name */
  const Column *getColumnModel(const QString& colName) const;

  /* Get descriptor for column index */
  const Column *getColumnModel(int colIndex) const;

  /* Add a filter for a column. Placeholder and negation will be adapted to SQL
   * query. Exact omits the % around queries like "AAA" */
  void filter(const Column *col, const QVariant& value, const QVariant& maxValue, bool exact);

  /* Get field data formatted for display as seen in the table view */
  QVariant getFormattedFieldData(const QModelIndex& index) const;

  /* Query the full result set into a vector of pairs with id and optional coordinates.
   * This does not work when using distance search. */
  void getFullResultSet(QVector<std::pair<int, atools::geo::Pos> >& result);

  Qt::SortOrder getSortOrder() const;

  QString getSortColumn() const
  {
    return orderByCol;
  }

  int getSortColumnIndex() const
  {
    return orderByColIndex;
  }

  int getTotalRowCount() const
  {
    return totalRowCount;
  }

  QString getCurrentSqlQuery() const
  {
    return currentSqlQuery;
  }

  /* Fetch more data and emit signal fetchedMore */
  virtual void fetchMore(const QModelIndex& parent) override;

  /* Get unformatted data from the model */
  QVariant getRawData(int row, int col) const;
  QVariant getRawData(int row, const QString& colname) const;

  /* Sets the SQL query into the model. This will start the query and fetch data from the database. */
  void updateSqlQuery();

  /* Set query to model causing a refresh. Unless force is set the query is compared to the current query and skipped if equal */
  void resetSqlQuery(bool force);

  /* Set a filter for objects within the given bounding rectangle */
  void filterByBoundingRect(const atools::geo::Rect& boundingRectangle);

  QString getColumnName(int col) const;

  /* Set sort order for the given column name. Does not update or restart the query */
  void setSort(const QString& colname, Qt::SortOrder order);

  /* Reset search and filter by ident, region and airport
   * "ignoreQueryBuilder" set to true will cause columns in record matching query builder
   * columns to be used as normal queries (not query builder). */
  void filterByRecord(const atools::sql::SqlRecord& record, bool ignoreQueryBuilder, bool exact);

  /* Get empty record (no data) containing field/column information */
  atools::sql::SqlRecord getSqlRecord() const;

  /* Get a SQL record that contains field/column information and data for the given row */
  atools::sql::SqlRecord getSqlRecord(int row) const;

  /* Fetch and format data for display */
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  /* Fetch data without any converts for display role */
  QVariant rawData(const QModelIndex& index) const;

  /*
   * Sets a data callback that is called for each table cell and the given item data roles.
   * @param func callback function or method. Use std::bind to create callbacks to non static methods.
   * @param roles Roles that this callback should be called for
   */
  void setDataCallback(const sqlmodeltypes::DataFunctionType& func, const QSet<Qt::ItemDataRole>& roles);

  bool isOverrideModeActive() const
  {
    return overrideModeActive;
  }

  /* Update model after data change */
  void refreshData(bool force);

  void setQueryBuilder(const QueryBuilder& builder)
  {
    queryBuilder = builder;
  }

  /* Set by derived classes to avoid unneeded queries on startup. */
  void setRestoreFinished()
  {
    restoreFinished = true;
  }

  bool isRestoreFinished() const
  {
    return restoreFinished;
  }

  /* true if widgets are currently updated by filterBy(). Avoids unneeded database queries. */
  bool isUpdatingWidgets() const
  {
    return updatingWidgets;
  }

signals:
  /* Emitted when more data was fetched */
  void fetchedMore();

  /* One or more columns overrides all other search options */
  void overrideMode(const QStringList& overrideColumnTitles);

private:
  // Hide the record method
  using QSqlQueryModel::record;

  struct WhereCondition
  {
    QString oper; /* operator (like, not like) */
    QVariant valueSql; /* Condition value including % or other SQL characters */
    QVariant valueDisplay; /* Raw value as entered in the search form */
    const Column *col; /* Column descriptor */
  };

  virtual void sort(int column, Qt::SortOrder order) override;

  void filterBy(bool exclude, QString whereCol, QVariant whereValue, bool forceQueryBuilder, bool ignoreQueryBuilder, bool exact);
  QString buildColumnList(const atools::sql::SqlRecord& tableCols);
  QString buildWhere(const atools::sql::SqlRecord& tableCols, QVector<const Column *>& overridingColumns);
  QString buildWhereValue(const WhereCondition& cond);
  void buildQuery(const QWidget *widgetFromBuilder = nullptr);
  void clearWhereConditions();

  /* Filter by value at index (context menu in table view). forceQueryBuilder to always use it. */
  void filterBy(QModelIndex index, bool exclude, bool forceQueryBuilder, bool exact);
  QString  sortOrderToSql(Qt::SortOrder order);
  QVariant defaultDataHandler(int, int, const Column *, const QVariant&,
                              const QVariant& displayRoleValue, Qt::ItemDataRole role) const;
  void updateTotalCount();
  void buildSqlWhereValue(QVariant& whereValue, bool exact) const;
  void buildSqlWhereValue(QString& whereValue, bool exact) const;
  bool isDistanceSearchActive() const;

  /* Default - all conditions are combined using "and" */
  const QString WHERE_OPERATOR = " and ";

  QString orderByCol /* Order by column name */, orderByOrder /* "asc" or "desc" */;
  int orderByColIndex = 0;

  QString currentSqlQuery, currentSqlCountQuery, currentSqlFetchQuery;

  /* Data callback */
  sqlmodeltypes::DataFunctionType dataFunction = nullptr;
  /* Roles for the data callback */
  QSet<Qt::ItemDataRole> handlerRoles;

  /* A bounding rectangle query is used if this is valid */
  atools::geo::Rect boundingRect;

  QueryBuilder queryBuilder;

  /* Maps column name to where condition struct */
  QHash<QString, WhereCondition> whereConditionMap;

  atools::sql::SqlDatabase *db;

  /* List of column descriptors */
  const ColumnList *columns;

  QWidget *parentWidget;
  int totalRowCount = 0;

  /* Set by buildWhere. Will ignore all other filter options */
  bool overrideModeActive = false;

  bool restoreFinished = false;
  bool updatingWidgets = false;
};

#endif // LITTLENAVMAP_SQLMODEL_H
