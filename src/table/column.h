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
  Column& groupShow(bool b = true);

  /* Can calculate minimum value in grouping query */
  Column& canMin(bool b = true);

  /* Can calculate maximum value in grouping query */
  Column& canMax(bool b = true);

  /* Can summarize value in grouping query */
  Column& canSum(bool b = true);

  /* Column can be used in filters */
  Column& filter(bool b = true);

  /* Column can be used in group by */
  Column& group(bool b = true);

  /* Table can not be sorted by this column */
  Column& noSort(bool b = true);

  /* Column is part of default view */
  Column& noDefault(bool b = true);

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

  Column& conditions(const QString& include, const QString& exclude);

  /* Sort order if this is the sort by column in default view */
  Column& defaultSortOrder(Qt::SortOrder order);

  Column& indexCondMap(const QStringList& indexMap);

  Column& includesName(bool value = true);

  Column& virtualCol(bool value = true);

  Column& condition(const QString& cond);

  bool isGroupShow() const
  {
    return colGroupByShow;
  }

  bool isMin() const
  {
    return colGroupByMin;
  }

  bool isMax() const
  {
    return colGroupByMax;
  }

  bool isSum() const
  {
    return colGroupBySum;
  }

  bool isFilter() const
  {
    return colCanBeFiltered;
  }

  bool isGroup() const
  {
    return colCanBeGrouped;
  }

  bool isNoSort() const
  {
    return colCanNotBeSorted;
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

  bool isIncludesName() const
  {
    return colQueryIncludesName;
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
    return colMinWidget;
  }

  QWidget *getMaxWidget() const
  {
    return colMaxWidget;
  }

  bool isNoDefault() const
  {
    return colIsNoDefaultColumn;
  }

  bool alwaysAnd() const
  {
    return colIsAlwaysAndColumn;
  }

  QString getSortFuncAsc() const
  {
    return colSortFuncAsc;
  }

  QString getSortFuncDesc() const
  {
    return colSortFuncDesc;
  }

  bool isHidden() const
  {
    return colIsHiddenColumn;
  }

  bool isVirtual() const
  {
    return colIsVirtual;
  }

  bool isDefaultSort() const
  {
    return colIsDefaultSortColumn;
  }

  Qt::SortOrder getDefaultSortOrder() const
  {
    return colDefaultSortOrd;
  }

  const QString& getExcludeCondition() const
  {
    return colExcludeCond;
  }

  const QString& getIncludeCondition() const
  {
    return colIncludeCond;
  }

  const QString& getCondition() const
  {
    return colCondition;
  }

  const QStringList& getIndexConditionMap() const
  {
    return colIndexConditionMap;
  }

  bool hasIncludeExcludeCond() const
  {
    return !(colIncludeCond.isEmpty() || colExcludeCond.isEmpty());
  }

  bool hasMinMaxSpinbox() const
  {
    return !(colMaxWidget == nullptr || colMinWidget == nullptr);
  }

  bool hasIndexConditionMap() const
  {
    return !colIndexConditionMap.isEmpty();
  }

private:
  QString colName;
  QString colDisplayName;
  QWidget *colWidget = nullptr, *colMaxWidget = nullptr, *colMinWidget = nullptr;
  QString colSortFuncAsc, colSortFuncDesc;

  /* Conditions used for checkboxes */
  QString colExcludeCond, colIncludeCond;

  /* Conditions used for spinboxes and others */
  QString colCondition;

  /* Condition list used for combo boxes */
  QStringList colIndexConditionMap;

  bool colGroupByShow = false;
  bool colGroupByMin = false;
  bool colGroupByMax = false;
  bool colGroupBySum = false;
  bool colCanBeFiltered = false;
  bool colCanBeGrouped = false;
  bool colCanNotBeSorted = false;
  bool colIsNoDefaultColumn = false;
  bool colIsDefaultSortColumn = false;
  bool colIsAlwaysAndColumn = false;
  bool colIsHiddenColumn = false;
  bool colQueryIncludesName = false;
  bool colIsVirtual = false;

  Qt::SortOrder colDefaultSortOrd = Qt::SortOrder::AscendingOrder;
};

#endif // LITTLENAVMAP_COLUMN_H
