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

#include <QVector>
#include <functional>

class QWidget;

typedef std::function<QString(const QVector<QWidget *>& widgets)> QueryBuilderFuncType;

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
  QueryBuilder(QueryBuilderFuncType funcParam, QVector<QWidget *> widgetsParam, const QStringList& cols)
    : func(funcParam), widgets(widgetsParam), columns(cols)
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
  QString build() const
  {
    if(func)
      return func(widgets);
    else
      return QString();
  }

  /* Get triggering widgets. Normally used in the callback function to extract filter values.
   *  Currently only line edit widgets supported. */
  const QVector<QWidget *>& getWidgets() const
  {
    return widgets;
  }

  /* Get affected or used column names */
  const QStringList& getColumns() const
  {
    return columns;
  }

private:
  QueryBuilderFuncType func;
  QVector<QWidget *> widgets;
  QStringList columns;
};

#endif // LNM_QUERYBUILDER_H
