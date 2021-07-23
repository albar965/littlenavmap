/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
 * Result class for query builder callback.
 */
struct QueryBuilderResult
{
  explicit QueryBuilderResult()
    : overrideQuery(false)
  {

  }

  explicit QueryBuilderResult(const QString& w, bool o)
    : where(w), overrideQuery(o)
  {
  }

  bool isEmpty() const
  {
    return where.isEmpty();
  }

  QString where; /* partial where clause */
  bool overrideQuery; /* true if this result should override all other queries */
};

typedef std::function<QueryBuilderResult(QWidget *widget)> QueryBuilderFuncType;

/* A callback object which can build a where clause for more than one column in search.
 * Uses a function object which can point to any method, function or lambda.
 * Only one per search can be used. */
class QueryBuilder
{
public:
  /*
   * @param funcParam Callback function which will be called with the list of widgets
   * @param widgetsParam List of widgets that are connected to the SqlController find methods.
   *                        Currently only line edit widgets supported.
   * @param cols Affected/used column names.
   */
  QueryBuilder(QueryBuilderFuncType funcParam, QWidget *widgetParam, const QStringList& cols)
    : func(funcParam), widget(widgetParam), columns(cols)
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

  /* Invoke callback to get query string */
  QueryBuilderResult build() const
  {
    if(func)
      return func(widget);
    else
      return QueryBuilderResult();
  }

  /* Get triggering widgets. Normally used in the callback function to extract filter values.
   *  Currently only line edit widgets supported. */
  QWidget *getWidget() const
  {
    return widget;
  }

  /* Get affected or used column names */
  const QStringList& getColumns() const
  {
    return columns;
  }

  /* Clear all widgets of type QLineEdit, QCheckBox, QComboBox and QSpinBox */
  void resetWidgets();

private:
  QueryBuilderFuncType func;
  QWidget *widget = nullptr;
  QStringList columns;
};

#endif // LNM_QUERYBUILDER_H
