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

#include "search/sqlcontroller.h"

#include "geo/calculations.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "sql/sqlrecord.h"

#include <QTableView>
#include <QHeaderView>
#include <QSpinBox>
#include <QApplication>

using atools::sql::SqlQuery;
using atools::sql::SqlDatabase;

SqlController::SqlController(atools::sql::SqlDatabase *sqlDb, ColumnList *cols, QTableView *tableView)
  : db(sqlDb), view(tableView), columns(cols)
{
}

SqlController::~SqlController()
{
  viewSetModel(nullptr);

  if(proxyModel != nullptr)
    proxyModel->clear();
  delete proxyModel;
  proxyModel = nullptr;

  if(model != nullptr)
    model->clear();
  delete model;
  model = nullptr;
}

void SqlController::preDatabaseLoad()
{
  viewSetModel(nullptr);

  if(model != nullptr)
    model->clear();
}

void SqlController::postDatabaseLoad()
{
  if(proxyModel != nullptr)
    viewSetModel(proxyModel);
  else
    viewSetModel(model);
  model->updateSqlQuery();
  model->resetSqlQuery();
  model->fillHeaderData();
}

void SqlController::refreshData(bool loadAll, bool keepSelection)
{
  QItemSelectionModel *sm = view->selectionModel();

  // Get all selected rows and highest selected row number
  int maxRow = 0;
  QSet<int> rows;
  if(sm != nullptr && keepSelection)
  {
    for(const QModelIndex& index : sm->selectedRows(0))
    {
      if(index.row() > maxRow)
        maxRow = index.row();
      rows.insert(index.row());
    }
  }

  // Reload query model
  model->refreshData();

  if(loadAll)
  {
    while(model->canFetchMore())
      model->fetchMore(QModelIndex());
  }

  // Selection changes when updating model
  sm = view->selectionModel();

  if(sm != nullptr && keepSelection)
  {
    int visibleRowCount = getVisibleRowCount();

    // Check if selected rows have to be loaded
    if(maxRow >= visibleRowCount)
    {
      // Load until done or highest selected row is covered
      while(model->canFetchMore() && getVisibleRowCount() < maxRow)
        model->fetchMore(QModelIndex());
    }

    // Update selection in new data result set
    int totalRowCount = getTotalRowCount();
    sm->blockSignals(true);
    for(int row : rows)
    {
      if(row < totalRowCount)
        sm->select(model->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    sm->blockSignals(false);
  }
}

void SqlController::refreshView()
{
  view->update();
}

bool SqlController::hasColumn(const QString& colName) const
{
  return columns->hasColumn(colName);
}

void SqlController::filterIncluding(const QModelIndex& index)
{
  view->clearSelection();
  model->filterIncluding(toSource(index));
  searchParamsChanged = true;
}

void SqlController::filterExcluding(const QModelIndex& index)
{
  view->clearSelection();
  model->filterExcluding(toSource(index));
  searchParamsChanged = true;
}

atools::geo::Pos SqlController::getGeoPos(const QModelIndex& index)
{
  if(index.isValid())
  {
    QVariant lon = getRawData(index.row(), "lonx");
    QVariant lat = getRawData(index.row(), "laty");

    if(!lon.isNull() && !lat.isNull())
      return atools::geo::Pos(lon.toFloat(), lat.toFloat());
  }
  return atools::geo::Pos();
}

void SqlController::filterByLineEdit(const Column *col, const QString& text)
{
  view->clearSelection();
  model->filter(col, text);
  searchParamsChanged = true;
}

void SqlController::filterBySpinBox(const Column *col, int value)
{
  view->clearSelection();
  if(col->getSpinBoxWidget()->value() == col->getSpinBoxWidget()->minimum())
    // Send a null variant if spin box is at minimum value
    model->filter(col, QVariant(QVariant::Int));
  else
    model->filter(col, value);
  searchParamsChanged = true;
}

void SqlController::filterByRecord(const atools::sql::SqlRecord& record)
{
  view->clearSelection();
  model->filterByRecord(record);
  searchParamsChanged = true;
}

void SqlController::filterByMinMaxSpinBox(const Column *col, int minValue, int maxValue)
{
  view->clearSelection();
  QVariant minVal(minValue), maxVal(maxValue);
  if(col->getMinSpinBoxWidget()->value() == col->getMinSpinBoxWidget()->minimum())
    // Send a null variant for min if minimum spin box is at minimum value
    minVal = QVariant(QVariant::Int);

  if(col->getMaxSpinBoxWidget()->value() == col->getMaxSpinBoxWidget()->maximum())
    // Send a null variant for max if maximum spin box is at maximum value
    maxVal = QVariant(QVariant::Int);

  model->filter(col, minVal, maxVal);
  searchParamsChanged = true;
}

void SqlController::filterByCheckbox(const Column *col, int state, bool triState)
{
  view->clearSelection();
  if(triState)
  {
    Qt::CheckState s = static_cast<Qt::CheckState>(state);
    switch(s)
    {
      case Qt::Unchecked:
        model->filter(col, 0);
        break;
      case Qt::PartiallyChecked:
        // null for partially checked
        model->filter(col, QVariant(QVariant::Int));
        break;
      case Qt::Checked:
        model->filter(col, 1);
        break;
    }
  }
  else
    model->filter(col, state == Qt::Checked ? 1 : QVariant(QVariant::Int));
  searchParamsChanged = true;
}

void SqlController::filterByComboBox(const Column *col, int value, bool noFilter)
{
  view->clearSelection();
  if(noFilter)
    // Index 0 for combo box means here: no filter, so remove it and send null variant
    model->filter(col, QVariant(QVariant::Int));
  else
    model->filter(col, value);
  searchParamsChanged = true;
}

void SqlController::filterByDistance(const atools::geo::Pos& center, sqlproxymodel::SearchDirection dir,
                                     float minDistance, float maxDistance)
{
  if(center.isValid())
  {
    // Start or update distance search
    view->clearSelection();

    currentDistanceCenter = center;
    atools::geo::Rect rect(center, atools::geo::nmToMeter(maxDistance));

    bool proxyWasNull = false;
    if(proxyModel == nullptr)
    {
      // No proxy - create a new one and assign it to the model
      proxyWasNull = true;
      // Controller takes ownership
      proxyModel = new SqlProxyModel(nullptr, model);
      proxyModel->setDynamicSortFilter(true);
      proxyModel->setSourceModel(model);

      viewSetModel(proxyModel);
    }

    // Update distances in proxy to get precise radius filtering (second filter stage)
    proxyModel->setDistanceFilter(center, dir, minDistance, maxDistance);

    // Update rectangle filter in query model (first coarse filter stage)
    model->filterByBoundingRect(rect);

    if(proxyWasNull)
    {
      // New proxy created so set ordering and more
      model->fillHeaderData();
      view->reset();
      processViewColumns();
    }
  }
  else
  {
    // End distance search
    view->clearSelection();
    currentDistanceCenter = atools::geo::Pos();

    // Set sql model back into view
    viewSetModel(model);

    if(proxyModel != nullptr)
    {
      proxyModel->clear();
      delete proxyModel;
      proxyModel = nullptr;
    }

    model->filterByBoundingRect(atools::geo::Rect());
    model->fillHeaderData();
    processViewColumns();
  }
  searchParamsChanged = true;
}

void SqlController::filterByDistanceUpdate(sqlproxymodel::SearchDirection dir, float minDistance,
                                           float maxDistance)
{
  if(proxyModel != nullptr)
  {
    view->clearSelection();
    // Create new bounding rectangle for first stage search
    atools::geo::Rect rect(currentDistanceCenter, atools::geo::nmToMeter(maxDistance));

    // Update proxy second stage filter
    proxyModel->setDistanceFilter(currentDistanceCenter, dir, minDistance, maxDistance);
    // Update SQL model coarse first stage filter
    model->filterByBoundingRect(rect);
    searchParamsChanged = true;
  }
}

/* Set new model into view and delete old selection model to avoid memory leak */
void SqlController::viewSetModel(QAbstractItemModel *newModel)
{
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(newModel);
  delete m;
}

void SqlController::selectAllRows()
{
  Q_ASSERT(view->selectionModel() != nullptr);
  view->selectAll();
}

void SqlController::selectNoRows()
{
  Q_ASSERT(view->selectionModel() != nullptr);
  view->clearSelection();
}

const QItemSelection SqlController::getSelection() const
{
  if(view->selectionModel() != nullptr)
    return view->selectionModel()->selection();
  else
    return QItemSelection();
}

int SqlController::getVisibleRowCount() const
{
  if(proxyModel != nullptr)
    // Proxy fine second stage filter knows precise count
    return proxyModel->rowCount();
  else if(model != nullptr)
    return model->rowCount();

  return 0;
}

int SqlController::getTotalRowCount() const
{
  if(proxyModel != nullptr)
    // Proxy fine second stage filter knows precise count
    return proxyModel->rowCount();
  else if(model != nullptr)
    return model->getTotalRowCount();
  else
    return 0;
}

bool SqlController::isColumnVisibleInView(int physicalIndex) const
{
  return view->columnWidth(physicalIndex) > view->horizontalHeader()->minimumSectionSize() + 1;
}

int SqlController::getColumnVisualIndex(int physicalIndex) const
{

  return view->horizontalHeader()->visualIndex(physicalIndex);
}

const Column *SqlController::getColumnDescriptor(const QString& colName) const
{
  return model->getColumnModel(colName);
}

const Column *SqlController::getColumnDescriptor(int physicalIndex) const
{
  return model->getColumnModel(physicalIndex);
}

void SqlController::resetView()
{
  // Reorder columns to match model order
  QHeaderView *header = view->horizontalHeader();
  for(int i = 0; i < header->count(); i++)
    header->moveSection(header->visualIndex(i), i);

  if(proxyModel != nullptr)
  {
    // For distance search switch back to distance column sort - tell proxy ...
    proxyModel->sort(0, Qt::DescendingOrder);
    // ... and adjust query
    model->setSort("distance", Qt::DescendingOrder);
  }
  else
    model->resetSort();

  processViewColumns();

  view->resizeColumnsToContents();
}

void SqlController::resetSearch()
{
  if(columns != nullptr)
    // Will also delete proxy by check box message
    columns->resetWidgets();

  if(model != nullptr)
    model->resetSearch();
}

QString SqlController::getCurrentSqlQuery() const
{
  return model->getCurrentSqlQuery();
}

QModelIndex SqlController::getModelIndexAt(const QPoint& pos) const
{
  return view->indexAt(pos);
}

QString SqlController::getFieldDataAt(const QModelIndex& index) const
{
  return model->getFormattedFieldData(toSource(index)).toString();
}

QModelIndex SqlController::toSource(const QModelIndex& index) const
{
  if(proxyModel != nullptr)
    return proxyModel->mapToSource(index);
  else
    return index;
}

QModelIndex SqlController::fromSource(const QModelIndex& index) const
{
  if(proxyModel != nullptr)
    return proxyModel->mapFromSource(index);
  else
    return index;
}

int SqlController::getIdForRow(const QModelIndex& index)
{
  if(index.isValid())
    return model->getRawData(toSource(index).row(), columns->getIdColumnName()).toInt();
  else
    return -1;
}

/* Adapt view to model columns - hide/show, indicate sort, sort, etc. */
void SqlController::processViewColumns()
{
  const Column *colDescrCurSort = nullptr;
  atools::sql::SqlRecord rec = model->getSqlRecord();
  int cnt = rec.count();
  for(int i = 0; i < cnt; i++)
  {
    QString field = rec.fieldName(i);
    const Column *colDescr = columns->getColumn(field);

    if(!isDistanceSearch() && colDescr->isDistance())
      // Hide special distance search columns "distance" and "heading"
      view->hideColumn(i);
    else if(colDescr->isHidden())
      view->hideColumn(i);
    else
      view->showColumn(i);

    // Set sort column
    if(model->getSortColumn().isEmpty())
    {
      if(colDescr->isDefaultSort())
      {
        // Highlight default sort column
        view->sortByColumn(i, colDescr->getDefaultSortOrder());
        colDescrCurSort = colDescr;
      }
    }
    else if(field == model->getSortColumn())
    {
      // Highlight user sort column
      view->sortByColumn(i, model->getSortOrder());
      colDescrCurSort = colDescr;
    }
  }

  // Apply sort order to view
  const Column *colDescrDefSort = columns->getDefaultSortColumn();
  if(colDescrDefSort != nullptr)
  {
    int idx = rec.indexOf(colDescrDefSort->getColumnName());
    if(colDescrCurSort == nullptr)
      // Sort by default
      view->sortByColumn(idx, colDescrDefSort->getDefaultSortOrder());
    else
    {
      if(colDescrCurSort->isHidden())
        view->sortByColumn(idx, colDescrDefSort->getDefaultSortOrder());
      else if(!isDistanceSearch())
        if(colDescrCurSort->isDistance())
          view->sortByColumn(idx, colDescrDefSort->getDefaultSortOrder());
    }
  }
}

void SqlController::updateHeaderData()
{
  model->fillHeaderData();
}

void SqlController::prepareModel()
{
  model = new SqlModel(parentWidget, db, columns);

  viewSetModel(model);

  model->fillHeaderData();
  processViewColumns();
}

void SqlController::loadAllRowsForDistanceSearch()
{
  if(searchParamsChanged && proxyModel != nullptr)
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Run query again
    model->resetSqlQuery();

    // Let proxy know that filter parameters have changed
    proxyModel->invalidate();

    while(model->canFetchMore())
      // Fetch as long as we can
      model->fetchMore(QModelIndex());

    QGuiApplication::restoreOverrideCursor();
    searchParamsChanged = false;
  }
}

void SqlController::setDataCallback(const SqlModel::DataFunctionType& value,
                                    const QSet<Qt::ItemDataRole>& roles)
{
  model->setDataCallback(value, roles);
}

void SqlController::loadAllRows()
{
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  if(proxyModel != nullptr)
  {
    // Run query again
    model->resetSqlQuery();

    // Let proxy know that filter parameters have changed
    proxyModel->invalidate();
  }

  while(model->canFetchMore())
    model->fetchMore(QModelIndex());

  QGuiApplication::restoreOverrideCursor();
}

QVector<const Column *> SqlController::getCurrentColumns() const
{
  QVector<const Column *> cols;
  atools::sql::SqlRecord rec = model->getSqlRecord();
  for(int i = 0; i < rec.count(); i++)
    cols.append(columns->getColumn(rec.fieldName(i)));
  return cols;
}

void SqlController::initRecord(atools::sql::SqlRecord& rec)
{
  rec.clear();
  atools::sql::SqlRecord from = model->getSqlRecord();
  for(int i = 0; i < from.count(); i++)
    rec.appendField(from.fieldName(i), from.fieldType(i));
}

bool SqlController::hasRow(int row) const
{
  int srow = row;
  if(proxyModel != nullptr)
    srow = toSource(proxyModel->index(row, 0)).row();

  return model->hasIndex(srow, 0);
}

void SqlController::fillRecord(int row, atools::sql::SqlRecord& rec)
{
  int srow = row;
  if(proxyModel != nullptr)
    srow = toSource(proxyModel->index(row, 0)).row();

  for(int i = 0; i < rec.count(); i++)
    rec.setValue(i, model->getRawData(srow, i));
}

QVariant SqlController::getRawData(int row, const QString& colname) const
{
  return getRawData(row, model->getSqlRecord().indexOf(colname));
}

QVariant SqlController::getRawData(int row, int col) const
{
  int srow = row;
  if(proxyModel != nullptr)
    srow = toSource(proxyModel->index(row, 0)).row();
  return model->getRawData(srow, col);
}

QVariant SqlController::getRawDataLocal(int row, const QString& colname) const
{
  return getRawDataLocal(row, model->getSqlRecord().indexOf(colname));
}

QVariant SqlController::getRawDataLocal(int row, int col) const
{
  return model->getRawData(row, col);
}

QString SqlController::getSortColumn() const
{
  return model->getSortColumn();
}

int SqlController::getSortColumnIndex() const
{
  return model->getSortColumnIndex();
}
