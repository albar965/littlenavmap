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

#include "search/sqlmodel.h"

#include "gui/application.h"
#include "gui/errorhandler.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "exception.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "sql/sqlrecord.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QSqlError>
#include <QRegularExpression>
#include <QComboBox>
#include <QStringBuilder>

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

void SqlModel::filterByBuilder(const QWidget *widget)
{
  qDebug() << Q_FUNC_INFO;
  buildQuery(widget);
}

void SqlModel::filterIncluding(QModelIndex index, bool forceQueryBuilder, bool exact)
{
  filterBy(index, false, forceQueryBuilder, exact);
  buildQuery();
}

void SqlModel::filterExcluding(QModelIndex index, bool forceQueryBuilder, bool exact)
{
  filterBy(index, true, forceQueryBuilder, exact);
  buildQuery();
}

void SqlModel::filterByBoundingRect(const atools::geo::Rect& boundingRectangle)
{
  boundingRect = boundingRectangle;
  buildQuery();
}

void SqlModel::filterByRecord(const atools::sql::SqlRecord& record, bool ignoreQueryBuilder, bool exact)
{
  for(int i = 0; i < record.count(); i++)
    filterBy(false /* exclude */, record.fieldName(i),
             record.value(i), queryBuilder.getColumns().contains(record.fieldName(i)), ignoreQueryBuilder, exact);
  buildQuery();
}

void SqlModel::filterBy(QModelIndex index, bool exclude, bool forceQueryBuilder, bool exact)
{
  filterBy(exclude, getSqlRecord().fieldName(index.column()), QSqlQueryModel::data(index), forceQueryBuilder,
           false /* ignoreQueryBuilder */, exact);
}

/* Simple include/exclude filter. Updates the attached search widgets */
void SqlModel::filterBy(bool exclude, QString whereCol, QVariant whereValueDisp, bool forceQueryBuilder, bool ignoreQueryBuilder,
                        bool exact)
{
  // If there is already a filter on the same column remove it
  if(whereConditionMap.contains(whereCol))
    whereConditionMap.remove(whereCol);

  QString whereOp;
  QVariant whereValueSql = whereValueDisp;

  // Replace '*' with '%' for SQL
  buildSqlWhereValue(whereValueSql, exact);

  if(whereValueDisp.isNull())
    whereOp = exclude ? "is not null" : "is null";
  else
    whereOp = exclude ? " not like " : " like ";

  if(exact)
  {
    // Generate SQL value string from display string using exact values
    QString whereValueDispStr = whereValueDisp.toString();
    if(!whereValueDispStr.startsWith('"'))
      whereValueDispStr = '"' % whereValueDispStr;
    if(!whereValueDispStr.endsWith('"'))
      whereValueDispStr = whereValueDispStr % '"';

    // Put back into variant
    whereValueDisp = whereValueDispStr;
  }

  const Column *col = columns->getColumn(whereCol);

  // Ignore callback messages which try to re-run the query when changing widget values
  updatingWidgets = true;

  // Either use query builder forced or use it if columns match
  // Using based on column match can be ignored if not wanted
  if(!ignoreQueryBuilder && (queryBuilder.getColumns().contains(whereCol) || forceQueryBuilder))
  {
    // Get all related widgets
    for(const QueryWidget& queryWidget : queryBuilder.getQueryWidgets())
    {
      // Check if widget contains this column
      if(queryWidget.getColumns().contains(whereCol))
      {
        // Field matches with one of the query builder columns
        QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(queryWidget.getWidget());
        if(lineEdit != nullptr)
          lineEdit->setText(whereValueDisp.toString());

        // First matching query builder is used
        break;
      }
    }
  }
  else if(QLineEdit *edit = columns->getColumn(whereCol)->getLineEditWidget())
  {
    // Set the search text into the corresponding line edit
    edit->setText((exclude ? "-" : QString()) % whereValueDisp.toString());
  }
  else if(QComboBox *combo = columns->getColumn(whereCol)->getComboBoxWidget())
  {
    if(combo->isEditable())
    {
      QString dispStr = whereValueDisp.toString();

      // Remove quotation around combo box values because the list in the combo does not use them
      if(dispStr.startsWith('"'))
        dispStr.remove(0, 1);

      if(dispStr.endsWith('"'))
        dispStr.chop(1);

      // Set the search text into the corresponding line edit
      combo->setCurrentText((exclude ? "-" : QString()) % dispStr);
    }
  }
  else if(QCheckBox *checkBox = columns->getColumn(whereCol)->getCheckBoxWidget())
  {
    if(checkBox->isTristate())
    {
      // Update check box state for tri state boxes
      if(whereValueDisp.isNull())
        checkBox->setCheckState(Qt::PartiallyChecked);
      else
      {
        bool val = whereValueDisp.toInt() > 0;
        if(exclude)
          val = !val;
        checkBox->setCheckState(val ? Qt::Checked : Qt::Unchecked);
      }
    }
    else
    {
      // Update check box state for normal boxes
      bool val = whereValueDisp.isNull() || whereValueDisp.toInt() == 0;
      if(exclude)
        val = !val;
      checkBox->setCheckState(val ? Qt::Unchecked : Qt::Checked);
    }
  }

  if(!forceQueryBuilder)
    whereConditionMap.insert(whereCol, {whereOp, whereValueSql, whereValueDisp, col});

  // Done updating - allow updates to the query again
  updatingWidgets = false;
}

/* Changes the whereConditionMap. Removes, replaces or adds where conditions based on input */
void SqlModel::filter(const Column *col, const QVariant& variantDisp, const QVariant& maxValue, bool exact)
{
  Q_ASSERT(col != nullptr);
  QString colName = col->getColumnName();
  bool colAlreadyFiltered = whereConditionMap.contains(colName);

  if((variantDisp.isNull() && !maxValue.isValid()) ||
     (variantDisp.isNull() && maxValue.isNull()) ||
     (variantDisp.type() == QVariant::String && variantDisp.toString().isEmpty()))
  {
    // If we get a null value or an empty string and the
    // column is already filtered remove it
    if(colAlreadyFiltered)
      whereConditionMap.remove(colName);
  }
  else
  {
    QVariant variantSql;
    QString oper;

    if(col->hasMinMaxSpinbox())
    {
      // Two spinboxes giving min and max values
      if(!variantDisp.isNull() && maxValue.isNull())
      {
        // Only min value set
        oper = '>';
        variantSql = variantDisp;
      }
      else if(variantDisp.isNull() && !maxValue.isNull())
      {
        // Only max value set
        oper = '<';
        variantSql = maxValue;
      }
      else
        // Min and max values set - use range and leave newVariant invalid
        oper = QString("between %1 and %2").arg(variantDisp.toInt()).arg(maxValue.toInt());
    }
    else if(!col->getCondition().isEmpty())
    {
      // Single spinbox giving a min or max value
      oper = col->getCondition();
      variantSql = variantDisp;
    }
    else if(col->hasIndexConditionMap())
      // A combo box
      oper = col->getIndexConditionMap().at(variantDisp.toInt());
    else if(col->hasIncludeExcludeCond())
    {
      // A checkbox - tri state is already filtered by the caller
      if(variantDisp.toInt() == 0)
        oper = col->getExcludeCondition();
      else
        oper = col->getIncludeCondition();
    }
    else
    {
      if(variantDisp.type() == QVariant::String)
      {
        // Use like queries for strings so we will query case insensitive
        QString newVal = variantDisp.toString();

        if(newVal.startsWith('-'))
        {
          if(newVal == '-')
          {
            // A single '-' translates to not nulls
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

        // Replace '*' with '%' for SQL
        buildSqlWhereValue(newVal, exact);

        variantSql = newVal;
      }
      else if(variantDisp.type() == QVariant::Int || variantDisp.type() == QVariant::UInt ||
              variantDisp.type() == QVariant::LongLong || variantDisp.type() == QVariant::ULongLong ||
              variantDisp.type() == QVariant::Double)
      {
        // Use equal for numbers
        variantSql = variantDisp;
        oper = '=';
      }
    }

    if(colAlreadyFiltered)
    {
      // Replace values in existing condition
      whereConditionMap[colName].oper = oper;
      whereConditionMap[colName].valueSql = variantSql;
      whereConditionMap[colName].valueDisplay = variantDisp;
      whereConditionMap[colName].col = col;
    }
    else
      // Insert new condition
      whereConditionMap.insert(colName, {oper, variantSql, variantDisp, col});
  }
  buildQuery(col->getLineEditWidget());
}

void SqlModel::buildSqlWhereValue(QVariant& whereValue, bool exact) const
{
  QString val = whereValue.toString();
  buildSqlWhereValue(val, exact);
  whereValue = val;
}

void SqlModel::buildSqlWhereValue(QString& whereValue, bool exact) const
{
  // Replace '*' with '%' for SQL

  if(!whereValue.isEmpty())
  {
    // Replace user placeholders
    if(whereValue.contains('*'))
      whereValue = whereValue.replace('*', '%');

    if(whereValue.startsWith('"') && whereValue.endsWith('"'))
      // Remove quotes from exact searches
      whereValue = whereValue.remove('"');
    else if(!exact)
      // Enclose with percent to have partial matches
      whereValue = '%' % whereValue % '%';
  }
}

void SqlModel::setSort(const QString& colname, Qt::SortOrder order)
{
  orderByColIndex = getSqlRecord().indexOf(colname);

  orderByCol = colname;
  orderByOrder = sortOrderToSql(order);
}

void SqlModel::setDataCallback(const sqlmodeltypes::DataFunctionType& func, const QSet<Qt::ItemDataRole>& roles)
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
    if(!cd->isHidden() && !(!isDistanceSearchActive() && cd->isDistance()))
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
    if(!col->getSqlFunc().isEmpty())
      // Use SQL function as column if defined
      colNames.append(QString("(%1) as %2").arg(col->getSqlFunc()).arg(col->getColumnName()));
    else if(col->isDistance() || !tableCols.contains(col->getColumnName()))
      // Add null for special distance columns
      // Null for columns which do not exist in the database
      colNames.append("null as " % col->getColumnName());
    else
      colNames.append(col->getColumnName());
  }

  // Concatenate to one string
  QString queryCols;
  bool first = true;
  for(const QString& cname : qAsConst(colNames))
  {
    if(!first)
      queryCols += ", ";
    queryCols += cname;

    first = false;
  }
  return queryCols;
}

/* Create SQL query and set it into the model */
void SqlModel::buildQuery(const QWidget *widgetFromBuilder)
{
  Q_UNUSED(widgetFromBuilder)

  // Ignore signals/messages from values set in widgets
  if(updatingWidgets)
    return;

  QString tablename = columns->getTablename();

  atools::sql::SqlRecord tableCols = db->record(tablename);
  QString queryCols = buildColumnList(tableCols);

  QVector<const Column *> overrideColumns;
  QString queryWhere = buildWhere(tableCols, overrideColumns);

  QString queryOrder;
  const Column *col = columns->getColumn(orderByCol);
  // Distance columns are no search criteria
  if(!orderByCol.isEmpty() && !orderByOrder.isEmpty() && !col->isDistance())
  {
    Q_ASSERT(col != nullptr);

    if(!col->getSqlFunc().isEmpty())
      queryOrder += "order by (" % col->getSqlFunc() % ") " % orderByOrder;
    else if(!tableCols.contains(orderByCol))
    {
      // Skip not existing columns for backwards compatibility
      qWarning() << Q_FUNC_INFO << tablename % '.' % col->getColumnName() << "does not exist";
    }
    else if(!(col->getSortFuncAsc().isEmpty() && col->getSortFuncDesc().isEmpty()))
    {
      // Use sort functions to have null values at end of the list - will avoid indexes
      if(orderByOrder == "asc")
        queryOrder += "order by " % col->getSortFuncAsc().arg(orderByCol) % ' ' % orderByOrder;
      else if(orderByOrder == "desc")
        queryOrder += "order by " % col->getSortFuncDesc().arg(orderByCol) % ' ' % orderByOrder;
      else
        Q_ASSERT(orderByOrder != "asc" && orderByOrder != "desc");
    }
    else
      queryOrder += "order by " % orderByCol % ' ' % orderByOrder;
  }

  currentSqlQuery = "select " % queryCols % " from " % tablename % ' ' % queryWhere % ' ' % queryOrder;

  // Build a query to find the total row count of the result ==================
  totalRowCount = 0;
  currentSqlCountQuery = "select count(1) from " % tablename % ' ' % queryWhere;

  // Build a query to fetch the whole result set in getFullResultSet() ==================
  if(!isDistanceSearchActive())
  {
    QStringList colList(columns->getIdColumnName());

    // Add coordinates if available
    if(columns->hasColumn("lonx") && columns->hasColumn("laty"))
    {
      colList.append("lonx");
      colList.append("laty");
    }

    currentSqlFetchQuery = "select " % colList.join(", ") % " from " % tablename % ' ' % queryWhere;
  }
  else
    // No result set for distance searches since the result is not filtered by precise distance
    currentSqlFetchQuery.clear();

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << currentSqlQuery;
#endif

  // Emit the columns which probably override other search options
  QStringList overrideColumnTitles;
  if(!overrideColumns.isEmpty())
  {
    for(const Column *ocol : qAsConst(overrideColumns))
      overrideColumnTitles.append(ocol->getDisplayName());
  }
  emit overrideMode(overrideColumnTitles);

  try
  {
    // Count total rows
    updateTotalCount();

    if(!isDistanceSearchActive())
      // Delay query for bounding rectangle query with proxy model
      resetSqlQuery(false /* force */);
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
QString SqlModel::buildWhere(const atools::sql::SqlRecord& tableCols, QVector<const Column *>& overridingColumns)
{
  const static QRegularExpression REQUIRED_COL_MATCH(".*/\\*([A-Za-z0-9_]+)\\*/.*");

  // Used to build SQL later - does not contain query builder columns and overrides are removed
  QHash<QString, WhereCondition> tempWhereConditionMap(whereConditionMap);
  overrideModeActive = false;

  // SQL where clause
  QStringList queryWhereBuilder;

  // Use query builder callback to get all where clauses ==================================
  const QueryBuilderResultVector results = queryBuilder.build();
  for(const QueryBuilderResult& builderResult : results)
  {
    if(!builderResult.isEmpty())
    {
      // Use builder callback SQL as first clause
      queryWhereBuilder.append(builderResult.getWhere());

      // Check if result overrides due to string length and other conditions exist or bounding query is done
      if(builderResult.isOverrideQuery() && (!whereConditionMap.isEmpty() || isDistanceSearchActive()))
      {
        // Get first overriding column for display
        overridingColumns.append(columns->getColumn(queryBuilder.getColumns().constFirst()));
        overrideModeActive = true;

        // No other conditions used if overriding
        tempWhereConditionMap.clear();
      }
    }
  }

  queryWhereBuilder.removeAll(QString());
  queryWhereBuilder.removeDuplicates();

  QString queryWhere;
  if(!queryWhereBuilder.isEmpty())
    queryWhere = '(' % queryWhereBuilder.join(" and ") % ')';

  // Build SQL from where condition objects ================================================
  for(const WhereCondition& cond : tempWhereConditionMap)
  {
    // Check if widget is enable and/or visible - do not consider query values from hidden search options
    if(!cond.col->isWidgetEnabled())
      continue;

    // Extract the required column from the comment in the operator and  check if it exists in the table
    // Currently used in airport search rating/3d query
    QString checkCol = cond.col->getColumnName();

    QRegularExpressionMatch match = REQUIRED_COL_MATCH.match(cond.oper);
    if(match.hasMatch())
      checkCol = match.captured(1);

    if(!tableCols.contains(checkCol))
    {
      // Skip not existing columns for backwards compatibility
      qWarning() << Q_FUNC_INFO << columns->getTablename() % '.' % cond.col->getColumnName() << "does not exist";
      continue;
    }

    if(!queryWhere.isEmpty())
      queryWhere += WHERE_OPERATOR;

    if(cond.col->isIncludesName())
      // Condition includes column name
      queryWhere += ' ' % cond.oper % ' ';
    else
      queryWhere += cond.col->getColumnName() % ' ' % cond.oper % ' ';

    if(!cond.valueSql.isNull())
      queryWhere += buildWhereValue(cond);
  }

  if(isDistanceSearchActive() && !overrideModeActive)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << boundingRect;
#endif

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

    if(!queryWhere.isEmpty())
      queryWhere += WHERE_OPERATOR;
    queryWhere += rectCond;
  }

  if(!queryWhere.isEmpty())
    queryWhere = "where (" % queryWhere % ')';

  return queryWhere;
}

bool SqlModel::isDistanceSearchActive() const
{
  return boundingRect.isValid() && columns->isDistanceCheckBoxActive();
}

/* Convert a value to string for the where clause */
QString SqlModel::buildWhereValue(const WhereCondition& cond)
{
  QVariant::Type type = cond.valueSql.type();
  QString val;
  if(type == QVariant::String || type == QVariant::Char)
    // Use semicolons for string and escape single quotes
    val = " '" % cond.valueSql.toString().replace("'", "''") % "'";
  else if(type == QVariant::Bool || type == QVariant::Int || type == QVariant::UInt || type == QVariant::LongLong ||
          type == QVariant::ULongLong || type == QVariant::Double)
    val = ' ' % cond.valueSql.toString();
  return val;
}

void SqlModel::refreshData(bool force)
{
  resetSqlQuery(force);
  updateTotalCount();
}

void SqlModel::resetSqlQuery(bool force)
{
  // Update can be forced when changing database rows, for distance search or if the query differs
  if(force || isDistanceSearchActive() || QSqlQueryModel::query().lastQuery() != currentSqlQuery)
  {
    QSqlQueryModel::setQuery(currentSqlQuery, db->getQSqlDatabase());

    if(lastError().isValid())
      atools::gui::ErrorHandler(parentWidget).handleSqlError(lastError());
  }
}

Qt::SortOrder SqlModel::getSortOrder() const
{
  return orderByOrder == "desc" ? Qt::DescendingOrder : Qt::AscendingOrder;
}

/* Default data handler - simply returns the value */
QVariant SqlModel::defaultDataHandler(int, int, const Column *, const QVariant&,
                                      const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  if(role == Qt::DisplayRole)
    return displayRoleValue;

  return QVariant();
}

QVariant SqlModel::rawData(const QModelIndex& index) const
{
  return index.isValid() ? QSqlQueryModel::data(index, Qt::DisplayRole) : QVariant();
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
    if(!isDistanceSearchActive())
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

void SqlModel::getFullResultSet(QVector<std::pair<int, atools::geo::Pos> >& result)
{
  if(!currentSqlFetchQuery.isEmpty())
  {
    try
    {
      // Use current query to fetch data
      const QString idColumnName = columns->getIdColumnName();
      SqlQuery stmt(db);
      stmt.exec(currentSqlFetchQuery);
      while(stmt.next())
      {
        // Use invalid coordinate if columns are not available
        atools::geo::Pos pos(stmt.valueFloat("lonx", atools::geo::Pos::INVALID_VALUE),
                             stmt.valueFloat("laty", atools::geo::Pos::INVALID_VALUE));
        result.append(std::make_pair(stmt.valueInt(idColumnName), pos));
      }
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
}

atools::sql::SqlRecord SqlModel::getSqlRecord() const
{
  return atools::sql::SqlRecord(record(), currentSqlQuery);
}

atools::sql::SqlRecord SqlModel::getSqlRecord(int row) const
{
  return atools::sql::SqlRecord(record(row), currentSqlQuery);
}
