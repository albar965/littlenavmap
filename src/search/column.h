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

#ifndef LITTLENAVMAP_COLUMN_H
#define LITTLENAVMAP_COLUMN_H

#include <QStringList>
#include <functional>

class QAction;
class QWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;

/*
 * A column descriptor defines query and view behaviour for each column. Key is
 * the column or alias name of the query result. The column name for the table
 * view header is also stored.
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

  /* Column can be used in filters. Action is used to show hidden search options if needed. */
  Column& filter(bool value = true, QAction *showOptionsActionParam = nullptr);
  Column& filterByBuilder(bool value = true);

  /* Column like ident can override other filters */
  Column& override (bool value = true);
  Column& minOverrideLength(int val = -1);

  /* Table can not be sorted by this column */
  Column& noSort(bool value = true);

  /* Column is part of default view */
  Column& noDefault(bool value = true);

  /* Column is hidden in view */
  Column& hidden(bool value = true);

  /* Column is defining sort order in default view */
  Column& defaultSort(bool value = true);

  /* Sort function for column */
  Column& sortFunc(const QString& sortFuncAsc, const QString& sortFuncDesc);

  /* Widget that is used for filtering this column */
  Column& widget(QWidget *widget);

  /* Widgets that are used for filtering this column with min and max values */
  Column& minWidget(QWidget *widget);
  Column& maxWidget(QWidget *widget);

  /* Conditions used when filtering this colum. Can be like ">0" or "is null" and are triggered by a checkbox. */
  Column& conditions(const QString& include, const QString& exclude);

  /* Sort order if this is the sort by column in default view */
  Column& defaultSortOrder(Qt::SortOrder order);

  /* SQL conditions that are used by a combo box. Combo box index matches index in list. */
  Column& indexCondMap(const QStringList& indexMap);

  /* Set to true if a condition map includes the column name */
  Column& includesName(bool value = true);

  /* Can be set to indicate that this is one of the tow distance search special columns "distance" and "heading". */
  Column& distanceCol(bool value = true);

  /* Indicates a condition that should be use for a spin box value, i.e. ">", "<" etc. */
  Column& condition(const QString& cond);

  /* Unit conversion function */
  Column& convertFunc(std::function<float(float value)> unitConvertFunc);

  /* Sql function to build column like "strftime('%s', destination_time) - strftime('%s', departure_time)" */
  Column& sqlFunc(const QString& sqlFunctionParam);

  /* true if either column or min/max widgets are set and enabled */
  bool isWidgetEnabled() const;

  bool isFilter() const
  {
    return colCanBeFiltered;
  }

  bool isFilterByBuilder() const
  {
    return colCanBeFilteredBuilder;
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

  /* Get widgets of the special type. Returns null if no widget of this type exists */
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

  bool isNoDefault() const
  {
    return colIsNoDefaultColumn;
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

  bool isDistance() const
  {
    return colIsDistance;
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

  int getIndex() const
  {
    return index;
  }

  QString getColWidgetSuffix() const
  {
    return colWidgetSuffix;
  }

  QString getColMaxWidgetSuffix() const
  {
    return colMaxWidgetSuffix;
  }

  QString getColMinWidgetSuffix() const
  {
    return colMinWidgetSuffix;
  }

  std::function<float(float value)> getUnitConvert() const
  {
    return unitConvert;
  }

  const QString& getSqlFunc() const
  {
    return sqlFunction;
  }

  /* Action which is used to show the potentially hidden related option for this column */
  QAction *getShowOptionsAction() const
  {
    return showOptionsAction;
  }

private:
  friend class ColumnList;

  QString colName;
  QString colDisplayName;
  const QString colOrigDisplayName;

  QWidget *colWidget = nullptr, *colMaxWidget = nullptr, *colMinWidget = nullptr;
  QAction *showOptionsAction = nullptr;

  QString colWidgetSuffix, colMaxWidgetSuffix, colMinWidgetSuffix;
  QString colSortFuncAsc, colSortFuncDesc;

  /* Conditions used for checkboxes */
  QString colExcludeCond, colIncludeCond;

  /* Conditions used for spinboxes and others */
  QString colCondition;

  /* SQL function to build column */
  QString sqlFunction;

  /* Condition list used for combo boxes */
  QStringList colIndexConditionMap;

  int index = -1;

  /* Function to convert from default units to widget units */
  std::function<float(float value)> unitConvert = nullptr;

  bool colCanBeFiltered = false;
  bool colCanBeFilteredBuilder = false;
  bool colCanNotBeSorted = false;
  bool colIsNoDefaultColumn = false;
  bool colIsDefaultSortColumn = false;
  bool colIsHiddenColumn = false;
  bool colQueryIncludesName = false;
  bool colIsDistance = false;

  Qt::SortOrder colDefaultSortOrd = Qt::SortOrder::AscendingOrder;
};

#endif // LITTLENAVMAP_COLUMN_H
