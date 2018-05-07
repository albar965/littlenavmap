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

#ifndef LITTLENAVMAP_CONTROLLER_H
#define LITTLENAVMAP_CONTROLLER_H

#include "search/sqlmodel.h"
#include "search/sqlproxymodel.h"

namespace atools {
namespace geo {
class Pos;
}
namespace sql {
class SqlDatabase;
}
}

class QWidget;
class QTableView;
class ColumnList;

/*
 * Combines all functionality around the table SQL model, view, view header and
 * more.
 */
class SqlController
{
public:
  SqlController(atools::sql::SqlDatabase *sqlDb, ColumnList *cols, QTableView *view);
  virtual ~SqlController();

  /* Create a new SqlModel, build and execute a query */
  void prepareModel();

  /* Load all rows into the view */
  void loadAllRows();

  /* Restore columns ordering, sorting and column widths to default */
  void resetView();

  /* Clear search and erase all text in search widgets */
  void resetSearch();

  /* Filter by text at the given index */
  void filterIncluding(const QModelIndex& index);

  /* Filter excluding by text at the given index */
  void filterExcluding(const QModelIndex& index);

  /* Set a filter by text from a line edit */
  void filterByLineEdit(const Column *col, const QString& text);

  /* Set a filter by an index from a combo box */
  void filterByComboBox(const Column *col, int value, bool noFilter);
  void filterByCheckbox(const Column *col, int state, bool triState);
  void filterBySpinBox(const Column *col, int value);
  void filterByMinMaxSpinBox(const Column *col, int minValue, int maxValue);

  const QItemSelection getSelection() const;

  /* Get model index for the given cursor position */
  QModelIndex getModelIndexAt(const QPoint& pos) const;

  /* Get data at given model index */
  QString getFieldDataAt(const QModelIndex& index) const;

  /* Get descriptor for column name */
  const Column *getColumnDescriptor(const QString& colName) const;

  /* Get descriptor for column at physical index */
  const Column *getColumnDescriptor(int physicalIndex) const;

  /* Number of rows currently loaded into the table view */
  int getVisibleRowCount() const;

  /* Total number of rows returned by the last query */
  int getTotalRowCount() const;

  /* Get the SQL query that was used to populate the table */
  QString getCurrentSqlQuery() const;

  /* Get all descriptors for currently displayed columns */
  QVector<const Column *> getCurrentColumns() const;

  /* Get variant from model for row and column */
  QVariant getRawData(int row, const QString& colname) const;
  QVariant getRawData(int row, int col) const;

  /* Same without row translation */
  QVariant getRawDataLocal(int row, const QString& colname) const;
  QVariant getRawDataLocal(int row, int col) const;

  /* Column name for sorted column */
  QString getSortColumn() const;
  int getSortColumnIndex() const;

  /* @return true if column at physical index is smaller
   * than minimal size + 1 */
  bool isColumnVisibleInView(int physicalIndex) const;

  /* Get the visual index for the column which is affected
   * by column reordering */
  int getColumnVisualIndex(int physicalIndex) const;

  /* Get SQL database from model */
  atools::sql::SqlDatabase *getSqlDatabase() const
  {
    return db;
  }

  /* Select all rows in view */
  void selectAllRows();
  void selectNoRows();

  /* Get the database id for the row at the index or -1 if the index is not valid */
  int getIdForRow(const QModelIndex& index);

  /* Reset search and filter by fields in record */
  void filterByRecord(const atools::sql::SqlRecord& record);

  /* Start or end distance search depending if center is valid or not */
  void filterByDistance(const atools::geo::Pos& center, sqlproxymodel::SearchDirection dir,
                        float minDistance, float maxDistance);

  /* Update distance search for changed values from spin box widgets */
  void filterByDistanceUpdate(sqlproxymodel::SearchDirection dir, float minDistance, float maxDistance);

  /* Load all rows if a distance search is active. */
  void loadAllRowsForDistanceSearch();

  /* True if distance search is active */
  bool isDistanceSearch()
  {
    return proxyModel != nullptr;
  }

  /* Set the callback that will handle data rows and values, i.e. format values to strings.
   * Set the desired data roles that the callback should be called for */
  void setDataCallback(const SqlModel::DataFunctionType& value, const QSet<Qt::ItemDataRole>& roles);

  /* Get position for the row at the given index. The query needs to have a lonx and laty column */
  atools::geo::Pos getGeoPos(const QModelIndex& index);

  /* Get SQL model */
  SqlModel *getSqlModel() const
  {
    return model;
  }

  /* Initializes the record with all column names from the current query */
  void initRecord(atools::sql::SqlRecord& rec);

  /* Fills a initialized record with data from the current model query */
  void fillRecord(int row, atools::sql::SqlRecord& rec);

  void preDatabaseLoad();
  void postDatabaseLoad();

  void updateHeaderData();

  /* Update query on changes in the database. Loads all data needed to restore selection if keepSelection is true */
  void refreshData(bool loadAll, bool keepSelection);

  /* Update view only */
  void refreshView();

  /* True if the row exists in the model */
  bool hasRow(int row) const;

  /* True if query contains column */
  bool hasColumn(const QString& colName) const;

private:
  void viewSetModel(QAbstractItemModel *newModel);

  /* Adapt columns to query change */
  void processViewColumns();

  /* Convert indexes if proxy model for distance search is used */
  QModelIndex toSource(const QModelIndex& index) const;
  QModelIndex fromSource(const QModelIndex& index) const;

  /* Proxy model used to distance search. null if distance search is not active.
   * While the normal SQL model acts as a primary (rectangle based) filter the proxy
   * model will do proper min/max radius filtering at a secondary stage.
   * To get correct results all rows have to be loaded from the SQL model and have to be piped through the
   * proxy model. */
  SqlProxyModel *proxyModel = nullptr;

  SqlModel *model = nullptr;
  QWidget *parentWidget = nullptr;
  atools::sql::SqlDatabase *db = nullptr;
  QTableView *view = nullptr;
  ColumnList *columns = nullptr;

  /* The model query is executed immediately if distance search is not used. For distance search
   * use a delayed approach that checks if any query parameters are changed. Changed parameters
   * are indicated by this bool */
  bool searchParamsChanged = false;
  atools::geo::Pos currentDistanceCenter;
};

#endif // LITTLENAVMAP_CONTROLLER_H
