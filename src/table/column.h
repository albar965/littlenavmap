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

#ifndef LITTLENAVMAP_COLUMN_H
#define LITTLENAVMAP_COLUMN_H

#include <QString>
#include <QStringList>

class QWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;

/*
 * A column descriptor defines query and view behaviour for each column. Key is
 * the column or alias name of the query result. The column name for the table
 * view header is also stored. This also includes columns in group by queries
 * that aggregate values.
 */
class Column
{
public:
  Column()
  {
  }

  /*
   * @param columnName Name or alias of the column as returned by the query
   * @param columnDisplayName Table header title for the column
   */
  Column(const QString& columnName, const QString& columnDisplayName = QString());
  Column(const QString& columnName, QWidget *widget, const QString& columnDisplayName = QString());

  /* Show column in grouping queries */
  Column& canGroupShow(bool b = true);

  /* Can calculate minimum value in grouping query */
  Column& canMin(bool b = true);

  /* Can calculate maximum value in grouping query */
  Column& canMax(bool b = true);

  /* Can summarize value in grouping query */
  Column& canSum(bool b = true);

  /* Column can be used in filters */
  Column& canFilter(bool b = true);

  /* Column can be used in group by */
  Column& canGroup(bool b = true);

  /* Table can be sorted by column */
  Column& canSort(bool b = true);

  /* Column is part of default view */
  Column& defaultCol(bool b = true);

  /* Column is hidden in view */
  Column& hidden(bool b = true);

  /* Column is defining sort order in default view */
  Column& defaultSort(bool b = true);

  /* Column is always add using "and" to the search criteria */
  Column& alwaysAnd(bool b = true);

  /* Sort function for column */
  Column& sortFunc(const QString& sortFuncAsc, const QString& sortFuncDesc);

  /* Widget that is used for filtering this column */
  Column& widget(QWidget *widget);

  Column& minWidget(QWidget *widget);
  Column& maxWidget(QWidget *widget);

  Column& cond(const QString& include, const QString& exclude);

  /* Sort order if this is the sort by column in default view */
  Column& defaultSortOrder(Qt::SortOrder order);

  Column& indexCondMap(const QStringList& indexMap);

  Column& includesColName(bool value = true);

  bool isGroupShow() const
  {
    return groupByShow;
  }

  bool isMin() const
  {
    return groupByMin;
  }

  bool isMax() const
  {
    return groupByMax;
  }

  bool isSum() const
  {
    return groupBySum;
  }

  bool isFilter() const
  {
    return canBeFiltered;
  }

  bool isGroup() const
  {
    return canBeGrouped;
  }

  bool isSort() const
  {
    return canBeSorted;
  }

  const QString& getColumnName() const
  {
    return colName;
  }

  const QString& getDisplayName() const
  {
    if(colDisplayName.isEmpty())
      return colName;
    else
      return colDisplayName;
  }

  bool isIncludesColName() const
  {
    return queryIncludesColName;
  }

  QLineEdit *getLineEditWidget() const;
  QComboBox *getComboBoxWidget() const;
  QCheckBox *getCheckBoxWidget() const;
  QSpinBox *getSpinBoxWidget() const;
  QSpinBox *getMinSpinBoxWidget() const;
  QSpinBox *getMaxSpinBoxWidget() const;

  QWidget *getWidget() const
  {
    return colWidget;
  }

  QWidget *getMinWidget() const
  {
    return minColWidget;
  }

  QWidget *getMaxWidget() const
  {
    return maxColWidget;
  }

  bool isDefaultCol() const
  {
    return isDefaultColumn;
  }

  bool isAlwaysAndCol() const
  {
    return isAlwaysAndColumn;
  }

  QString getSortFuncColAsc() const
  {
    return sortFuncForColAsc;
  }

  QString getSortFuncColDesc() const
  {
    return sortFuncForColDesc;
  }

  bool isHiddenCol() const
  {
    return isHiddenColumn;
  }

  bool isDefaultSortCol() const
  {
    return isDefaultSortColumn;
  }

  Qt::SortOrder getDefaultSortOrder() const
  {
    return defaultSortOrd;
  }

  const QString& getExcludeCondition() const
  {
    return colExcludeCond;
  }

  const QString& getIncludeCondition() const
  {
    return colIncludeCond;
  }

  const QStringList& getIndexConditionMap() const
  {
    return indexConditionMap;
  }

  bool hasIncludeExcludeCond() const
  {
    return !(colIncludeCond.isEmpty() || colExcludeCond.isEmpty());
  }

  bool hasMinMaxSpinbox() const
  {
    return !(maxColWidget == nullptr || minColWidget == nullptr);
  }

  bool hasIndexConditionMap() const
  {
    return !indexConditionMap.isEmpty();
  }

private:
  QString colName;
  QString colDisplayName;
  QWidget *colWidget = nullptr, *maxColWidget = nullptr, *minColWidget = nullptr;
  QString sortFuncForColAsc, sortFuncForColDesc;

  QString colExcludeCond, colIncludeCond;
  QStringList indexConditionMap;

  bool groupByShow = false;
  bool groupByMin = false;
  bool groupByMax = false;
  bool groupBySum = false;
  bool canBeFiltered = false;
  bool canBeGrouped = false;
  bool canBeSorted = false;
  bool isDefaultColumn = false;
  bool isDefaultSortColumn = false;
  bool isAlwaysAndColumn = false;
  bool isHiddenColumn = false;
  bool queryIncludesColName = false;

  Qt::SortOrder defaultSortOrd = Qt::SortOrder::AscendingOrder;
};

#endif // LITTLENAVMAP_COLUMN_H
