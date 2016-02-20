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

#ifndef LITTLENAVMAP_CONTROLLER_H
#define LITTLENAVMAP_CONTROLLER_H

#include "table/columnlist.h"
#include "table/sqlmodel.h"

#include <QItemSelectionModel>
#include <QObject>
#include <functional>

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class QWidget;
class QTableView;
class QModelIndex;
class QPoint;
class Column;

/*
 * Combines all functionality around the table SQL model, view, view header and
 * more.
 */
class Controller :
  public QObject
{
  Q_OBJECT

public:
  /*
   * @param hasLogbookEntries true if logbook table is present and populated
   * @param hasAirportTable true if additional airport information is available
   */
  Controller(QWidget *parent, atools::sql::SqlDatabase *sqlDb, ColumnList *cols, QTableView *view);
  virtual ~Controller();

  /* Assign a QLineEdit to a column descriptor */
  void assignLineEdit(const QString& field, QLineEdit *edit);

  /* Assign a QComboBox to a column descriptor */
  void assignComboBox(const QString& field, QComboBox *combo);

  /* Update logbook status if it has been loaded later */
  void setHasLogbook(bool value);

  void setHasAirports(bool value);

  /* Create a new SqlModel, build and execute a query */
  void prepareModel();

  /* Delete any SqlModl */
  void clearModel();

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

  /* Group by column at the given index */
  void groupByColumn(const QModelIndex& index);

  /* Release grouping */
  void ungroup();

  /* Set a filter by text from a line edit */
  void filterByLineEdit(const QString& field, const QString& text);

  /* Set a filter by an index from a combo box */
  void filterByComboBox(const QString& field, int value, bool noFilter);

  /* Use "and" or "or" to combine searches */
  void filterOperator(bool useAnd);

  /* Connect model reset signal */
  void connectModelReset(std::function<void(void)> func);

  /* Connect signal that is emitted if more data is fetched */
  void connectFetchedMore(std::function<void(void)> func);

  bool isGrouped() const;
  const QItemSelection getSelection() const;

  /* Get model index for the given cursor position */
  QModelIndex getModelIndexAt(const QPoint& pos) const;

  /* Get header caption for column at given model index */
  QString getHeaderNameAt(const QModelIndex& index) const;

  /* Get data at given model index */
  QString getFieldDataAt(const QModelIndex& index) const;

  /* Get descriptor for column name */
  const Column *getColumn(const QString& colName) const;

  /* Get descriptor for column at physical index */
  const Column *getColumn(int physicalIndex) const;

  /* Number of rows currently loaded into the table view */
  int getVisibleRowCount() const;

  /* Total number of rows returned by the last query */
  int getTotalRowCount() const;

  QString getCurrentSqlQuery() const;

  /* Get all descriptors for currently displayed columns */
  QVector<const Column *> getCurrentColumns() const;

  /* Return field data formatted as in the table view */
  QString formatModelData(const QString& col, const QVariant& var) const;
  QVariantList getFormattedModelData(int row) const;

  /* get values from the database */
  QVariantList getRawModelData(int row) const;
  QStringList getRawModelColumns() const;

  /* Column name for sorted column */
  QString getSortColumn() const;

  /* @return true if column at physical index is smaller
   * than minimal size + 1 */
  bool isColumnVisibleInView(int physicalIndex) const;

  /* Get the visual index for the column which is affected
   * by column reordering */
  int getColumnVisualIndex(int physicalIndex) const;

  atools::sql::SqlDatabase *getSqlDatabase() const
  {
    return db;
  }

  /* Select all rows in view */
  void selectAll();

private:
  /* Adapt columns to query change */
  void processViewColumns();

  /* Save view state to settings */
  void saveViewState();

  /* Load view state from settings */
  void restoreViewState();

  QWidget *parentWidget = nullptr;
  atools::sql::SqlDatabase *db = nullptr;
  QTableView *view = nullptr;
  SqlModel *model = nullptr;
  ColumnList *columns = nullptr;
  QByteArray viewState;
};

#endif // LITTLENAVMAP_CONTROLLER_H
