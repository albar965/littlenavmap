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

#include "table/sqlmodel.h"
#include "gui/errorhandler.h"
#include "table/columnlist.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlutil.h"
#include "logging/loggingdefs.h"
#include "table/column.h"

#include <QApplication>
#include <QLineEdit>
#include <QSqlField>
#include <QCheckBox>
#include <QSpinBox>

#include <functional>

using atools::sql::SqlQuery;
using atools::sql::SqlUtil;
using atools::sql::SqlDatabase;
using atools::gui::ErrorHandler;

SqlModel::SqlModel(QWidget *parent, SqlDatabase *sqlDb, const ColumnList *columnList)
  : QSqlQueryModel(parent), db(sqlDb), columns(columnList), parentWidget(parent)
{
  handlerRoles << Qt::DisplayRole << Qt::BackgroundRole << Qt::TextAlignmentRole;
  setFormatCallback(nullptr);
  setDataCallback(nullptr);

  buildQuery();
}

SqlModel::~SqlModel()
{
}

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
      if(!value.isNull() && maxValue.isNull())
      {
        oper = ">";
        newVariant = value;
      }
      else if(value.isNull() && !maxValue.isNull())
      {
        oper = "<";
        newVariant = maxValue;
      }
      else
        oper = QString("between %1 and %2").arg(value.toInt()).arg(maxValue.toInt());
    }
    else if(col->hasIndexConditionMap())
      // A combo box
      oper = col->getIndexConditionMap().at(value.toInt());
    else if(col->hasIncludeExcludeCond())
    {
      // A checkbox
      if(value.toInt() == 0)
        oper = col->getExcludeCondition();
      else
        oper = col->getIncludeCondition();
    }
    else
    {
      if(value.type() == QVariant::String)
      {
        // Use like queries for string
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
          newVal = newVal.toUpper().replace("*", "%");
        else if(!newVal.isEmpty())
          newVal = newVal.toUpper() + "%";

        newVariant = newVal;
      }
      else if(value.type() == QVariant::Int)
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
      whereConditionMap[colName].col = col;
    }
    else
      whereConditionMap.insert(colName, {oper, newVariant, col});
  }
  buildQuery();
}

void SqlModel::filterOperator(const QString& op)
{
  whereOperator = op;
  buildQuery();
}

QVariant SqlModel::getFormattedFieldData(const QModelIndex& index) const
{
  return data(index);
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

void SqlModel::filterBy(QModelIndex index, bool exclude)
{
  QString whereCol = record().field(index.column()).name();
  QVariant whereValue = QSqlQueryModel::data(index);

  // If there is already a filter on the same column remove it
  if(whereConditionMap.contains(whereCol))
    whereConditionMap.remove(whereCol);

  QString whereOp;
  if(whereValue.isNull())
    whereOp = exclude ? "is not null" : "is null";
  else
    whereOp = exclude ? " not like " : " like ";

  const Column *col = columns->getColumn(whereCol);
  Q_ASSERT(col != nullptr);

  // Set the search text into the corresponding line edit
  if(QLineEdit * edit = columns->getColumn(whereCol)->getLineEditWidget())
    edit->setText((exclude ? "-" : "") + whereValue.toString());
  else if(QCheckBox * check = columns->getColumn(whereCol)->getCheckBoxWidget())
  {
    if(check->isTristate())
    {
      if(whereValue.isNull())
        check->setCheckState(Qt::PartiallyChecked);
      else
      {
        bool val = whereValue.toInt() > 0;
        if(exclude)
          val = !val;
        check->setCheckState(val ? Qt::Checked : Qt::Unchecked);
      }
    }
    else
    {
      bool val = whereValue.isNull() || whereValue.toInt() == 0;
      if(exclude)
        val = !val;
      check->setCheckState(val ? Qt::Unchecked : Qt::Checked);
    }
  }
  whereConditionMap.insert(whereCol, {whereOp, whereValue, col});
}

void SqlModel::groupByColumn(QModelIndex index)
{
  groupByCol = record().fieldName(index.column());
  orderByCol = groupByCol;
  orderByOrder = "asc";
  clearWhereConditions();
  buildQuery();
  fillHeaderData();
}

void SqlModel::setSort(const QString& colname, Qt::SortOrder order)
{
  orderByColIndex = record().indexOf(colname);
  orderByCol = colname;
  orderByOrder = sortOrderToSql(order);
}

void SqlModel::setFormatCallback(const FormatFunctionType& value)
{
  using namespace std::placeholders;

  if(value == nullptr)
    formatFunc = std::bind(&SqlModel::defaultFormatHandler, this, _1, _2, _3);
  else
    formatFunc = value;
}

void SqlModel::setDataCallback(const DataFunctionType& value)
{
  using namespace std::placeholders;

  if(value == nullptr)
    dataFunc = std::bind(&SqlModel::defaultDataHandler, this, _1, _2, _3, _4, _5, _6);
  else
    dataFunc = value;
}

void SqlModel::reset()
{
  orderByCol.clear();
  orderByOrder.clear();
  groupByCol.clear();
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

void SqlModel::clearWhereConditions()
{
  whereConditionMap.clear();
  boundingRect = atools::geo::Rect();
}

void SqlModel::ungroup()
{
  clearWhereConditions();
  groupByCol.clear();

  // Restore last sort order
  orderByCol = lastOrderByCol;
  orderByOrder = sortOrderToSql(lastOrderByOrder);

  buildQuery();
  fillHeaderData();
}

void SqlModel::fillHeaderData()
{
  int cnt = record().count();
  for(int i = 0; i < cnt; ++i)
  {
    QString field = record().fieldName(i);
    const Column *cd = columns->getColumn(field);

    Q_ASSERT_X(cd != nullptr, "fillHeaderData", QString("field \"" + field + "\" is null").toLocal8Bit());

    if(!cd->isHidden() && !(!boundingRect.isValid() && cd->isVirtual()))
    {
      qDebug() << "Header" << i << "display" << cd->getDisplayName();
      setHeaderData(i, Qt::Horizontal, cd->getDisplayName());
    }
  }
}

bool SqlModel::isGrouped() const
{
  return !groupByCol.isEmpty();
}

int SqlModel::getLastSortIndex() const
{
  return record().indexOf(lastOrderByCol);
}

Qt::SortOrder SqlModel::getLastSortOrder() const
{
  return lastOrderByOrder;
}

const Column *SqlModel::getColumnModel(const QString& colName) const
{
  return columns->getColumn(colName);
}

const Column *SqlModel::getColumnModel(int colIndex) const
{
  return columns->getColumn(record().fieldName(colIndex));
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

void SqlModel::sort(int column, Qt::SortOrder order)
{
  orderByColIndex = column;
  orderByCol = record().field(column).name();
  orderByOrder = sortOrderToSql(order);

  if(groupByCol.isEmpty())
  {
    // Remember this sort order for the next ungroup
    lastOrderByCol = orderByCol;
    lastOrderByOrder = order;
  }
  buildQuery();
}

QString SqlModel::buildColumnList()
{
  QVector<QString> colNames;
  for(const Column *col : columns->getColumns())
  {
    if(col->isVirtual())
      colNames.append("null as " + col->getColumnName());
    else if(groupByCol.isEmpty())
      // Not grouping - default view
      // if(col->isNoDefaultCol() /*|| !col->isHiddenCol()*/)
      colNames.append(col->getColumnName());
    else if(col->getColumnName() == groupByCol || col->isGroupShow())
      // Add the group by column
      colNames.append(col->getColumnName());
    else
    {
      // Add all aggregate columns
      QString cname = col->getColumnName();
      if(col->isMin())
        colNames.append("min(" + cname + ") as " + cname + "_min");
      if(col->isMax())
        colNames.append("max(" + cname + ") as " + cname + "_max");
      if(col->isSum())
        colNames.append("sum(" + cname + ") as " + cname + "_sum");
    }
  }

  if(!groupByCol.isEmpty())
    // Always add total count when grouping
    colNames.append("count(*) as num_flights");

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

QString SqlModel::buildWhereValue(const WhereCondition& cond)
{
  QString val;
  if(cond.value.type() == QVariant::String || cond.value.type() == QVariant::Char)
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

QString SqlModel::buildWhere()
{
  QString queryWhere;
  QString queryWhereAnd;

  int numCond = 0, numAndCond = 0;
  for(const WhereCondition& cond : whereConditionMap)
  {
    if(!cond.col->alwaysAnd())
    {
      if(numCond++ > 0)
        queryWhere += " " + whereOperator + " ";
      if(cond.col->isIncludesName())
        queryWhere += " " + cond.oper + " ";
      else
        queryWhere += cond.col->getColumnName() + " " + cond.oper + " ";
      if(!cond.value.isNull())
        queryWhere += buildWhereValue(cond);
    }
    else
    {
      if(numAndCond++ > 0)
        queryWhereAnd += " and ";
      if(cond.col->isIncludesName())
        queryWhereAnd += " " + cond.oper + " ";
      else
        queryWhereAnd += cond.col->getColumnName() + " " + cond.oper + " ";
      if(!cond.value.isNull())
        queryWhereAnd += buildWhereValue(cond);
    }
  }

  if(boundingRect.isValid())
  {
    QString rectCond = QString("(lonx between %1 and %2 and laty between %3 and %4)").
                       arg(boundingRect.getTopLeft().getLonX()).arg(boundingRect.getBottomRight().getLonX()).
                       arg(boundingRect.getBottomRight().getLatY()).arg(boundingRect.getTopLeft().getLatY());

    if(numCond > 0)
      queryWhere += " " + whereOperator + " ";
    queryWhere += rectCond;
    numCond++;
  }

  if(numCond > 0)
    queryWhere = "(" + queryWhere + ")";

  if(numAndCond > 0)
  {
    if(numCond > 0)
      queryWhere += " and ";
    queryWhere += queryWhereAnd;
  }

  if(numCond > 0 || numAndCond > 0)
    queryWhere = " where " + queryWhere;

  return queryWhere;
}

void SqlModel::buildQuery()
{
  QString queryCols = buildColumnList();

  QString queryWhere = buildWhere();

  QString queryGroup;
  if(!groupByCol.isEmpty())
    queryGroup += "group by " + groupByCol;

  QString queryOrder;
  const Column *col = columns->getColumn(orderByCol);
  if(!orderByCol.isEmpty() && !orderByOrder.isEmpty() && !col->isVirtual())
  {
    Q_ASSERT(col != nullptr);

    if(!(col->getSortFuncAsc().isEmpty() && col->getSortFuncDesc().isEmpty()))
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

  if(boundingRect.isValid())
  {
    atools::geo::Pos center = boundingRect.getCenter();

    QString srt("(lonx - %1) * (lonx - %1) + (laty - %2) * (laty - %2)");

    if(queryOrder.isEmpty())
      queryOrder += "order by ";
    else
      queryOrder += ",";

    queryOrder += srt.arg(center.getLonX()).arg(center.getLatY()) + orderByOrder;
  }

  currentSqlQuery = "select " + queryCols + " from " + columns->getTablename() +
                    " " + queryWhere + " " + queryGroup + " " + queryOrder;

  // Build a query to find the total row count of the result
  totalRowCount = 0;
  QString queryCount;
  if(isGrouped())
    queryCount = "select count(1) from "
                 "(select count(" + groupByCol + ") from " +
                 columns->getTablename() + " " + queryWhere + " " + queryGroup + ")";
  else
    queryCount = "select count(1) from " + columns->getTablename() + " " + queryWhere;

  try
  {
    SqlQuery countStmt(db);
    countStmt.exec(queryCount);
    if(countStmt.next())
      totalRowCount = countStmt.value(0).toInt();

    qDebug() << "Query" << currentSqlQuery;
    qDebug() << "Query Count" << queryCount;

    if(!boundingRect.isValid())
      resetSqlQuery();
  }
  catch(std::exception& e)
  {
    atools::gui::ErrorHandler(parentWidget).handleException(e, "While executing query");
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parentWidget).handleUnknownException("While executing query");
  }
}

void SqlModel::resetSqlQuery()
{
  QSqlQueryModel::setQuery(currentSqlQuery, db->getQSqlDatabase());

  if(lastError().isValid())
    atools::gui::ErrorHandler(parentWidget).handleSqlError(lastError());
}

QString SqlModel::formatValue(const QString& colName, const QVariant& value) const
{
  return formatFunc(columns->getColumn(colName), QVariant(), value);
}

Qt::SortOrder SqlModel::getSortOrder() const
{
  return orderByOrder == "desc" ? Qt::DescendingOrder : Qt::AscendingOrder;
}

void SqlModel::setHandlerRoles(const QSet<Qt::ItemDataRole>& value)
{
  handlerRoles = value;
}

QString SqlModel::defaultFormatHandler(const Column *col, const QVariant& value,
                                       const QVariant& dataValue) const
{
  Q_UNUSED(col);
  Q_UNUSED(dataValue);
  return value.toString();
}

QVariant SqlModel::defaultDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& value,
                                      const QVariant& dataValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(colIndex);
  Q_UNUSED(rowIndex);
  if(role == Qt::DisplayRole)
    return formatFunc(col, value, dataValue);

  return QVariant();
}

QVariant SqlModel::data(const QModelIndex& index, int role) const
{
  if(!index.isValid())
    return QVariant();

  Qt::ItemDataRole dataRole = static_cast<Qt::ItemDataRole>(role);
  QVariant roleValue = QSqlQueryModel::data(index, role);
  if(handlerRoles.contains(dataRole))
  {
    QVariant dataValue = QSqlQueryModel::data(index, Qt::DisplayRole);
    QString col = record().field(index.column()).name();
    const Column *column = columns->getColumn(col);

    int row = -1;
    if(!boundingRect.isValid())
      // no reliable row information with proxy
      row = index.row();

    QVariant retval = dataFunc(index.column(), row, column, roleValue, dataValue, dataRole);
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

QVariantList SqlModel::getRawRowData(int row) const
{
  QVariantList values;
  for(int i = 0; i < columnCount(); ++i)
    values.append(QSqlQueryModel::data(createIndex(row, i)));
  return values;
}

QVariant SqlModel::getRawData(int row, const QString& colname) const
{
  return getRawData(row, record().indexOf(colname));
}

QVariant SqlModel::getRawData(int row, int col) const
{
  return QSqlQueryModel::data(createIndex(row, col));
}

QStringList SqlModel::getRawColumns() const
{
  QStringList cols;
  for(int i = 0; i < columnCount(); ++i)
    cols.append(record().field(i).name());
  return cols;
}

QString SqlModel::getColumnName(int col) const
{
  return record().fieldName(col);
}

QVariantList SqlModel::getFormattedRowData(int row)
{
  QVariantList values;
  for(int i = 0; i < columnCount(); ++i)
  {
    QModelIndex idx = createIndex(row, i);
    values.append(formatValue(record().field(idx.column()).name(), QSqlQueryModel::data(idx)));
  }
  return values;
}
