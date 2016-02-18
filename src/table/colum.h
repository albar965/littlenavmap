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

class QLineEdit;
class QComboBox;

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
  Column(const QString& columnName, const QString& columnDisplayName = QString()) :
    colName(columnName), displayName(columnDisplayName)
  {

  }

  /* Show column in grouping queries */
  Column& canGroupShow(bool b = true)
  {
    groupByShow = b;
    return *this;
  }

  /* Can calculate minimum value in grouping query */
  Column& canMin(bool b = true)
  {
    groupByMin = b;
    return *this;
  }

  /* Can calculate maximum value in grouping query */
  Column& canMax(bool b = true)
  {
    groupByMax = b;
    return *this;
  }

  /* Can summarize value in grouping query */
  Column& canSum(bool b = true)
  {
    groupBySum = b;
    return *this;
  }

  /* Column can be used in filters */
  Column& canFilter(bool b = true)
  {
    canBeFiltered = b;
    return *this;
  }

  /* Column can be used in group by */
  Column& canGroup(bool b = true)
  {
    canBeGrouped = b;
    return *this;
  }

  /* Table can be sorted by column */
  Column& canSort(bool b = true)
  {
    canBeSorted = b;
    return *this;
  }

  /* Column is part of default view */
  Column& defaultCol(bool b = true)
  {
    isDefaultColumn = b;
    return *this;
  }

  /* Column is hidden in view */
  Column& hidden(bool b = true)
  {
    isHiddenColumn = b;
    return *this;
  }

  /* Column is defining sort order in default view */
  Column& defaultSort(bool b = true)
  {
    isDefaultSortColumn = b;
    return *this;
  }

  /* Column is always add using "and" to the search criteria */
  Column& alwaysAnd(bool b = true)
  {
    isAlwaysAndColumn = b;
    return *this;
  }

  /* Sort function for column */
  Column& sortFunc(const QString& sortFuncAsc, const QString& sortFuncDesc)
  {
    sortFuncForColAsc = sortFuncAsc;
    sortFuncForColDesc = sortFuncDesc;
    return *this;
  }

  /* QLineEdit widget that is used for filtering this column */
  Column& lineEdit(QLineEdit *edit)
  {
    lineEditWidget = edit;
    return *this;
  }

  /* QComboBox widget that is used for filtering this column */
  Column& comboBox(QComboBox *combo)
  {
    comboBoxWidget = combo;
    return *this;
  }

  /* Sort order if this is the sort by column in default view */
  Column& defaultSortOrder(Qt::SortOrder order)
  {
    defaultSortOrd = order;
    return *this;
  }

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

  QString getColumnName() const
  {
    return colName;
  }

  QString getDisplayName() const
  {
    if(displayName.isEmpty())
      return colName;
    else
      return displayName;
  }

  QLineEdit *getLineEditWidget() const
  {
    return lineEditWidget;
  }

  QComboBox *getComboBoxWidget() const
  {
    return comboBoxWidget;
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

private:
  QString colName;
  QString displayName;
  QLineEdit *lineEditWidget = nullptr;
  QComboBox *comboBoxWidget = nullptr;
  QString sortFuncForColAsc, sortFuncForColDesc;

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

  Qt::SortOrder defaultSortOrd = Qt::SortOrder::AscendingOrder;
};

#endif // LITTLENAVMAP_COLUMN_H
