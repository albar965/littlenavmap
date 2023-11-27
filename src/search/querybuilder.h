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

#ifndef LNM_QUERYBUILDER_H
#define LNM_QUERYBUILDER_H

#include <QStringList>
#include <QVector>

#include <functional>

class QWidget;

/*
 * Result class for query builder callback function.
 */
class QueryBuilderResult
{
public:
  explicit QueryBuilderResult()
    : overrideQuery(false)
  {

  }

  explicit QueryBuilderResult(const QString& whereParam, bool overrideParam)
    : where(whereParam), overrideQuery(overrideParam)
  {
  }

  /* Complete where clause without "where" */
  const QString& getWhere() const
  {
    return where;
  }

  bool isEmpty() const
  {
    return where.isEmpty();
  }

  /* Can override other search options */
  bool isOverrideQuery() const
  {
    return overrideQuery;
  }

private:
  QString where; /* partial where clause */
  bool overrideQuery; /* true if this result should override all other queries */

};

/*
 * Connects a widget with a list of search columns. Used to omit searches from hidden or disabled widgets.
 */
class QueryWidget
{
public:
  QueryWidget(QWidget *widgetParam, const QStringList& columnParam, bool allowOverrideParam, bool allowExcludeParam)
    : widget(widgetParam), columns(columnParam), allowOverride(allowOverrideParam), allowExclude(allowExcludeParam)
  {
  }

  QWidget *getWidget() const
  {
    return widget;
  }

  bool isWidgetEnabled() const;

  /* Get all table columns covered by this widget */
  const QStringList& getColumns() const
  {
    return columns;
  }

  /* Can override other search options */
  bool isAllowOverride() const
  {
    return allowOverride;
  }

  /* Allow operator "-" to exclude search terms */
  bool isAllowExclude() const
  {
    return allowExclude;
  }

private:
  QWidget *widget;
  const QStringList columns;
  bool allowOverride, allowExclude;
};

typedef QVector<QueryWidget> QueryWidgetVector;
typedef QVector<QueryBuilderResult> QueryBuilderResultVector;
typedef std::function<QueryBuilderResult(const QueryWidget& queryWidget)> QueryBuilderFuncType;

/*
 * A callback object which can build a where clause for more than one column in search.
 * Uses a function object which can point to any method, function or lambda.
 * Only one per search can be used.
 */
class QueryBuilder
{
public:
  /*
   * @param funcParam Callback function which will be called with the list of widgets
   * @param widgetsParam List of widgets that are connected to the SqlController find methods.
   *                        Currently only line edit widgets supported.
   * @param cols Affected/used column names.
   */
  QueryBuilder(QueryBuilderFuncType funcParam, const QueryWidgetVector& queryWidgetsParam)
    : func(funcParam), queryWidgets(queryWidgetsParam)
  {
  }

  QueryBuilder()
  {

  }

  /* true if function is set and object is not default constructed */
  bool isValid()const
  {
    return func ? true : false;
  }

  /* Invoke callback to get query string for each query widget. */
  QueryBuilderResultVector build() const;

  /* Get triggering widgets. Normally used in the callback function to extract filter values.
   * Currently only line edit widgets supported. */
  const QVector<QWidget *> getWidgets() const;

  /* Get affected or used column names for all query widgets */
  const QStringList getColumns() const;

  /* Clear contents of all widgets of type QLineEdit, QCheckBox, QComboBox and QSpinBox */
  void resetWidgets();

  const QueryWidgetVector& getQueryWidgets() const
  {
    return queryWidgets;
  }

private:
  QueryBuilderFuncType func;
  QueryWidgetVector queryWidgets;
};

#endif // LNM_QUERYBUILDER_H
