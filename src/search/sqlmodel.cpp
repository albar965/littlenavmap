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

#include "search/sqlmodel.h"

#include "gui/application.h"
#include "gui/errorhandler.h"
#include "search/columnlist.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "exception.h"
#include "search/column.h"
#include "sql/sqlrecord.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QSqlError>
#include <QRegularExpression>
#include <QComboBox>

using atools::sql::SqlQuery;
using atools::sql::SqlDatabase;
using atools::gui::ErrorHandler;
using atools::sql::SqlRecord;

SqlModel::SqlModel(QWidget *parent, SqlDatabase *sqlDb, const ColumnList *columnList)
  : QSqlQueryModel(parent), db(sqlDb), columns(columnList), parentWidget(parent)
{
  // Set default handler
  setDataCallback(nullptr, QSet<Qt::ItemDataRole>());

  buildQuery();
}

SqlModel::~SqlModel()
{
}

void SqlModel::filterIncluding(QModelIndex index)
{
  filterBy(index, false);
  buildQuery();
}

void SqlModel::filterExcluding(QModelIndex index)
{
  filterBy(index, true);
  buildQuery();
}

void SqlModel::filterByBoundingRect(const atools::geo::Rect& boundingRectangle)
{
  boundingRect = boundingRectangle;
  buildQuery();
}

void SqlModel::filterByRecord(const atools::sql::SqlRecord& record)
{
  for(int i = 0; i < record.count(); i++)
    filterBy(false /* exclude */, record.fieldName(i), record.value(i));
}

/* Filter by value at index (context menu in table view) */
void SqlModel::filterBy(QModelIndex index, bool exclude)
{
  QString whereCol = getSqlRecord().fieldName(index.column());
  filterBy(exclude, whereCol, QSqlQueryModel::data(index));
}

/* Simple include/exclude filter. Updates the attached search widgets */
void SqlModel::filterBy(bool exclude, QString whereCol, QVariant whereValue)
{
  // If there is already a filter on the same column remove it
  if(whereConditionMap.contains(whereCol))
    whereConditionMap.remove(whereCol);

  QString whereOp;
  if(whereValue.isNull())
    whereOp = exclude ? "is not null" : "is null";
  else
    whereOp = exclude ? " not like " : " like ";

  const Column *colDescr = columns->getColumn(whereCol);

  if(QLineEdit *edit = columns->getColumn(whereCol)->getLineEditWidget())
    // Set the search text into the corresponding line edit
    edit->setText((exclude ? "-" : "") + whereValue.toString());
  else if(QComboBox *combo = columns->getColumn(whereCol)->getComboBoxWidget())
  {
    if(combo->isEditable())
      // Set the search text into the corresponding line edit
      combo->setCurrentText((exclude ? "-" : "") + whereValue.toString());
  }
  else if(QCheckBox *checkBox = columns->getColumn(whereCol)->getCheckBoxWidget())
  {
    if(checkBox->isTristate())
    {
      // Update check box state for tri state boxes
      if(whereValue.isNull())
        checkBox->setCheckState(Qt::PartiallyChecked);
      else
      {
        bool val = whereValue.toInt() > 0;
        if(exclude)
          val = !val;
        checkBox->setCheckState(val ? Qt::Checked : Qt::Unchecked);
      }
    }
    else
    {
      // Update check box state for normal boxes
      bool val = whereValue.isNull() || whereValue.toInt() == 0;
      if(exclude)
        val = !val;
      checkBox->setCheckState(val ? Qt::Unchecked : Qt::Checked);
    }
  }
  whereConditionMap.insert(whereCol, {whereOp, whereValue, whereValue, colDescr});
}

/* Changes the whereConditionMap. Removes, replaces or adds where conditions based on input */
void SqlModel::filter(const Column *col, const QVariant& value, const QVariant& maxValue)
{
  Q_ASSERT(col != nullptr);
  QString colName = col->getColumnName();
  bool colAlreadyFiltered = whereConditionMap.contains(colName);

  if((value.isNull() && !maxValue.isValid()) ||
     (value.isNull() && maxValue.isNull()) ||
     (value.type() == QVariant::String && value.toString().isEmpty()))
  {
    // If we get a null value or an empty string and the
    // column is already filtered remove it
    if(colAlreadyFiltered)
      whereConditionMap.remove(colName);
  }
  else
  {
    QVariant newVariant;
    QString oper;

    if(col->hasMinMaxSpinbox())
    {
      // Two spinboxes giving min and max values
      if(!value.isNull() && maxValue.isNull())
      {
        // Only min value set
        oper = ">";
        newVariant = value;
      }
      else if(value.isNull() && !maxValue.isNull())
      {
        // Only max value set
        oper = "<";
        newVariant = maxValue;
      }
      else
        // Min and max values set - use range and leave newVariant invalid
        oper = QString("between %1 and %2").arg(value.toInt()).arg(maxValue.toInt());
    }
    else if(!col->getCondition().isEmpty())
    {
      // Single spinbox giving a min or max value
      oper = col->getCondition();
      newVariant = value;
    }
    else if(col->hasIndexConditionMap())
      // A combo box
      oper = col->getIndexConditionMap().at(value.toInt());
    else if(col->hasIncludeExcludeCond())
    {
      // A checkbox - tri state is already filtered by the caller
      if(value.toInt() == 0)
        oper = col->getExcludeCondition();
      else
        oper = col->getIncludeCondition();
    }
    else
    {
      if(value.type() == QVariant::String)
      {
        // Use like queries for strings so we will query case insensitive
        QString newVal = value.toString();

        if(newVal.startsWith("-"))
        {
          if(newVal == "-")
          {
            // A single "-" translates to not nulls
            oper = "is not null";
            newVal.clear();
          }
          else
          {
            oper = "not like";
            newVal.remove(0, 1);
          }
        }
        else
          oper = "like";

        // Replace "*" with "%" for SQL
        if(newVal.contains("*"))
          newVal = newVal.replace("*", "%");
        else if(!newVal.isEmpty())
          newVal = newVal + "%";

        newVariant = newVal;
      }
      else if(value.type() == QVariant::Int || value.type() == QVariant::UInt ||
              value.type() == QVariant::LongLong || value.type() == QVariant::ULongLong ||
              value.type() == QVariant::Double)
      {
        // Use equal for numbers
        newVariant = value;
        oper = "=";
      }
    }

    if(colAlreadyFiltered)
    {
      // Replace values in existing condition
      whereConditionMap[colName].oper = oper;
      whereConditionMap[colName].value = newVariant;
      whereConditionMap[colName].valueRaw = value;
      whereConditionMap[colName].col = col;
    }
    else
      // Insert new condition
      whereConditionMap.insert(colName, {oper, newVariant, value, col});
  }
  buildQuery();
}

void SqlModel::setSort(const QString& colname, Qt::SortOrder order)
{
  orderByColIndex = getSqlRecord().indexOf(colname);
  orderByCol = colname;
  orderByOrder = sortOrderToSql(order);
}

void SqlModel::setDataCallback(const DataFunctionType& func, const QSet<Qt::ItemDataRole>& roles)
{
  if(func == nullptr)
  {
    // Set all back to default
    handlerRoles.clear();
    handlerRoles << Qt::DisplayRole << Qt::BackgroundRole << Qt::TextAlignmentRole;
    using namespace std::placeholders;
    dataFunction = std::bind(&SqlModel::defaultDataHandler, this, _1, _2, _3, _4, _5, _6);
  }
  else
  {
    handlerRoles = roles;
    dataFunction = func;
  }
}

void SqlModel::resetSort()
{
  orderByCol.clear();
  orderByOrder.clear();
  buildQuery();
  fillHeaderData();
}

void SqlModel::resetView()
{
  orderByCol.clear();
  orderByOrder.clear();
  clearWhereConditions();
  buildQuery();
  fillHeaderData();
}

void SqlModel::resetSearch()
{
  clearWhereConditions();
  buildQuery();
  // no need to rebuild header - view remains the same
}

/* Clear all query conditions */
void SqlModel::clearWhereConditions()
{
  whereConditionMap.clear();
  boundingRect = atools::geo::Rect();
}

/* Set header captions */
void SqlModel::fillHeaderData()
{
  SqlRecord sqlRecord = getSqlRecord();
  int cnt = sqlRecord.count();
  for(int i = 0; i < cnt; i++)
  {
    const Column *cd = columns->getColumn(sqlRecord.fieldName(i));
    if(!cd->isHidden() && !(!boundingRect.isValid() && cd->isDistance()))
      setHeaderData(i, Qt::Horizontal, cd->getDisplayName());
  }
}

const Column *SqlModel::getColumnModel(const QString& colName) const
{
  return columns->getColumn(colName);
}

const Column *SqlModel::getColumnModel(int colIndex) const
{
  return columns->getColumn(getSqlRecord().fieldName(colIndex));
}

QString SqlModel::sortOrderToSql(Qt::SortOrder order)
{
  switch(order)
  {
    case Qt::AscendingOrder:
      return "asc";

    case Qt::DescendingOrder:
      return "desc";
  }
  return QString();
}

/* Do own sorting in the SQL model */
void SqlModel::sort(int column, Qt::SortOrder order)
{

  QString colname = getSqlRecord().fieldName(column);
  if(columns->getColumn(colname)->isNoSort())
    return;

  orderByColIndex = column;
  orderByCol = colname;
  orderByOrder = sortOrderToSql(order);

  buildQuery();
}

/* Build full list of columns to query */
QString SqlModel::buildColumnList(const atools::sql::SqlRecord& tableCols)
{
  QVector<QString> colNames;
  for(const Column *col : columns->getColumns())
  {
    if(!col->isDistance() && !tableCols.contains(col->getColumnName()))
    {
      // Skip not existing columns for backwards compatibility
      qWarning() << Q_FUNC_INFO << columns->getTablename() + "." + col->getColumnName() << "does not exist";
      continue;
    }

    if(col->isDistance())
      // Add null for special distance columns
      colNames.append("null as " + col->getColumnName());
    else
      colNames.append(col->getColumnName());
  }

  // Concatenate to one string
  QString queryCols;
  bool first = true;
  for(QString cname : colNames)
  {
    if(!first)
      queryCols += ", ";
    queryCols += cname;

    first = false;
  }
  return queryCols;
}

/* Create SQL query and set it into the model */
void SqlModel::buildQuery()
{
  atools::sql::SqlRecord tableCols = db->record(columns->getTablename());
  QString queryCols = buildColumnList(tableCols);

  QVector<const Column *> overrideColumns;
  QString queryWhere = buildWhere(tableCols, overrideColumns);

  QString queryOrder;
  const Column *col = columns->getColumn(orderByCol);
  // Distance columns are no search criteria
  if(!orderByCol.isEmpty() && !orderByOrder.isEmpty() && !col->isDistance())
  {
    Q_ASSERT(col != nullptr);

    if(!tableCols.contains(orderByCol))
    {
      // Skip not existing columns for backwards compatibility
      qWarning() << Q_FUNC_INFO << columns->getTablename() + "." + col->getColumnName() << "does not exist";
    }
    else if(!(col->getSortFuncAsc().isEmpty() && col->getSortFuncDesc().isEmpty()))
    {
      // Use sort functions to have null values at end of the list - will avoid indexes
      if(orderByOrder == "asc")
        queryOrder += "order by " + col->getSortFuncAsc().arg(orderByCol) + " " + orderByOrder;
      else if(orderByOrder == "desc")
        queryOrder += "order by " + col->getSortFuncDesc().arg(orderByCol) + " " + orderByOrder;
      else
        Q_ASSERT(orderByOrder != "asc" && orderByOrder != "desc");
    }
    else
      queryOrder += "order by " + orderByCol + " " + orderByOrder;
  }

  currentSqlQuery = "select " + queryCols + " from " + columns->getTablename() +
                    " " + queryWhere + " " + queryOrder;

  // Build a query to find the total row count of the result
  totalRowCount = 0;
  currentSqlCountQuery = "select count(1) from " + columns->getTablename() + " " + queryWhere;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << currentSqlQuery;
#endif

  QStringList overrideColumnTitles;
  if(!overrideColumns.isEmpty())
  {
    for(const Column *ocol : overrideColumns)
      overrideColumnTitles.append(ocol->getDisplayName());
  }
  emit overrideMode(overrideColumnTitles);

  try
  {
    // Count total rows
    updateTotalCount();

    if(!boundingRect.isValid())
      // Delay query for bounding rectangle query with proxy model
      resetSqlQuery();
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

void SqlModel::updateTotalCount()
{
  if(!currentSqlCountQuery.isEmpty())
  {
    SqlQuery countStmt(db);
    countStmt.exec(currentSqlCountQuery);
    if(countStmt.next())
      totalRowCount = countStmt.value(0).toInt();
    else
      totalRowCount = 0;
  }
  else
    totalRowCount = 0;
}

/* Build where statement */
QString SqlModel::buildWhere(const atools::sql::SqlRecord& tableCols, QVector<const Column *>& overrideColumns)
{
  const static QRegularExpression REQUIRED_COL_MATCH(".*/\\*([A-Za-z0-9_]+)\\*/.*");
  QString queryWhere;

  QHash<QString, WhereCondition> whereConditions;
  bool hasNonOverride = false;

  // Check for columns that override all other search options
  for(const QString& key : whereConditionMap.keys())
  {
    const WhereCondition& cond = whereConditionMap.value(key);

    if(cond.col->isOverride())
    {
      if(cond.col->getMinOverrideLength() == -1)
      {
        // Length not given - simply check if valid
        if(cond.value.isValid())
        {
          whereConditions.insert(key, cond);
          overrideColumns.append(cond.col);
        }
        else
          hasNonOverride = true;
      }
      else
      {
        // Check if minimum length for overriding is satisfied
        if(cond.valueRaw.toString().size() >= cond.col->getMinOverrideLength())
        {
          whereConditions.insert(key, cond);
          overrideColumns.append(cond.col);
        }
        else
          hasNonOverride = true;
      }
    }
    else
      hasNonOverride = true;
  }

  if(!hasNonOverride && !boundingRect.isValid())
    overrideColumns.clear();

  overrideModeActive = !overrideColumns.isEmpty();

  if(whereConditions.isEmpty())
    // No overrides found use all columns
    whereConditions = whereConditionMap;

  int numCond = 0;
  for(const WhereCondition& cond : whereConditions)
  {
    // Extract the required column from the comment in the operator and  check if it exists in the table
    QString checkCol = cond.col->getColumnName();
    QRegularExpressionMatch match = REQUIRED_COL_MATCH.match(cond.oper);
    if(match.hasMatch())
      checkCol = match.captured(1);

    if(!tableCols.contains(checkCol))
    {
      // Skip not existing columns for backwards compatibility
      qWarning() << Q_FUNC_INFO << columns->getTablename() + "." + cond.col->getColumnName() << "does not exist";
      continue;
    }

    if(numCond++ > 0)
      queryWhere += " " + WHERE_OPERATOR + " ";

    if(cond.col->isIncludesName())
      // Condition includes column name
      queryWhere += " " + cond.oper + " ";
    else
      queryWhere += cond.col->getColumnName() + " " + cond.oper + " ";

    if(!cond.value.isNull())
      queryWhere += buildWhereValue(cond);
  }

  if(boundingRect.isValid() && !overrideModeActive)
  {
    QString rectCond;
    if(boundingRect.crossesAntiMeridian())
    {
      QList<atools::geo::Rect> rect = boundingRect.splitAtAntiMeridian();

      rectCond = QString("((lonx between %1 and %2 and laty between %3 and %4) or "
                         "(lonx between %5 and %6 and laty between %7 and %8))").
                 arg(rect.at(0).getTopLeft().getLonX()).arg(rect.at(0).getBottomRight().getLonX()).
                 arg(rect.at(0).getBottomRight().getLatY()).arg(rect.at(0).getTopLeft().getLatY()).
                 arg(rect.at(1).getTopLeft().getLonX()).arg(rect.at(1).getBottomRight().getLonX()).
                 arg(rect.at(1).getBottomRight().getLatY()).arg(rect.at(1).getTopLeft().getLatY());
    }
    else
      rectCond = QString("(lonx between %1 and %2 and laty between %3 and %4)").
                 arg(boundingRect.getTopLeft().getLonX()).arg(boundingRect.getBottomRight().getLonX()).
                 arg(boundingRect.getBottomRight().getLatY()).arg(boundingRect.getTopLeft().getLatY());

    if(numCond > 0)
      queryWhere += " " + WHERE_OPERATOR + " ";
    queryWhere += rectCond;
    numCond++;
  }

  if(numCond > 0)
    queryWhere = "(" + queryWhere + ")";

  if(numCond > 0)
    queryWhere = " where " + queryWhere;

  return queryWhere;
}

/* Convert a value to string for the where clause */
QString SqlModel::buildWhereValue(const WhereCondition& cond)
{
  QString val;
  if(cond.value.type() == QVariant::String || cond.value.type() == QVariant::Char)
    // Use semicolons for string
    val = " '" + cond.value.toString().replace("'", "''") + "'";
  else if(cond.value.type() == QVariant::Bool ||
          cond.value.type() == QVariant::Int ||
          cond.value.type() == QVariant::UInt ||
          cond.value.type() == QVariant::LongLong ||
          cond.value.type() == QVariant::ULongLong ||
          cond.value.type() == QVariant::Double)
    val = " " + cond.value.toString();
  return val;
}

void SqlModel::refreshData()
{
  resetSqlQuery();
  updateTotalCount();
}

void SqlModel::resetSqlQuery()
{
  QSqlQueryModel::setQuery(currentSqlQuery, db->getQSqlDatabase());

  if(lastError().isValid())
    atools::gui::ErrorHandler(parentWidget).handleSqlError(lastError());
}

Qt::SortOrder SqlModel::getSortOrder() const
{
  return orderByOrder == "desc" ? Qt::DescendingOrder : Qt::AscendingOrder;
}

/* Default data handler - simply returns the value */
QVariant SqlModel::defaultDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& value,
                                      const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(colIndex);
  Q_UNUSED(rowIndex);
  Q_UNUSED(col);
  Q_UNUSED(value);
  if(role == Qt::DisplayRole)
    return displayRoleValue;

  return QVariant();
}

QVariant SqlModel::data(const QModelIndex& index, int role) const
{
  if(!index.isValid())
    return QVariant();

  Qt::ItemDataRole dataRole = static_cast<Qt::ItemDataRole>(role);

  // Get the default value for this role. Can be a font, color, etc.
  QVariant roleValue = QSqlQueryModel::data(index, role);

  if(handlerRoles.contains(dataRole))
  {
    // Callback wants to be called for this role

    // Get data to display
    QVariant dataValue = QSqlQueryModel::data(index, Qt::DisplayRole);
    QString col = getSqlRecord().fieldName(index.column());
    const Column *column = columns->getColumn(col);

    int row = -1;
    if(!boundingRect.isValid())
      // no reliable row information with proxy
      row = index.row();

    QVariant retval = dataFunction(index.column(), row, column, roleValue, dataValue, dataRole);
    if(retval.isValid())
      return retval;
  }
  return roleValue;
}

void SqlModel::fetchMore(const QModelIndex& parent)
{
  QSqlQueryModel::fetchMore(parent);
  emit fetchedMore();
}

QVariant SqlModel::getRawData(int row, const QString& colname) const
{
  return getRawData(row, getSqlRecord().indexOf(colname));
}

void SqlModel::updateSqlQuery()
{
  buildQuery();
}

QVariant SqlModel::getRawData(int row, int col) const
{
  return QSqlQueryModel::data(createIndex(row, col));
}

QString SqlModel::getColumnName(int col) const
{
  return getSqlRecord().fieldName(col);
}

QVariant SqlModel::getFormattedFieldData(const QModelIndex& index) const
{
  return data(index);
}

atools::sql::SqlRecord SqlModel::getSqlRecord() const
{
  return atools::sql::SqlRecord(record(), currentSqlQuery);
}

atools::sql::SqlRecord SqlModel::getSqlRecord(int row) const
{
  return atools::sql::SqlRecord(record(row), currentSqlQuery);
}
