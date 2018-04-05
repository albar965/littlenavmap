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

#include "search/column.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

Column::Column(const QString& columnName, const QString& columnDisplayName)
  : colName(columnName), colDisplayName(columnDisplayName), colOrigDisplayName(columnDisplayName)
{

}

Column::Column(const QString& columnName, QWidget *widget, const QString& columnDisplayName)
  : colName(columnName), colDisplayName(columnDisplayName), colOrigDisplayName(columnDisplayName), colWidget(widget)
{

}

Column& Column::filter(bool b)
{
  colCanBeFiltered = b;
  return *this;
}

Column& Column::override(bool b)
{
  colCanOverride = b;
  return *this;
}

Column& Column::minOverrideLength(int val)
{
  colMinOverrideLength = val;
  return *this;
}

Column& Column::noSort(bool b)
{
  colCanNotBeSorted = b;
  return *this;
}

Column& Column::noDefault(bool b)
{
  colIsNoDefaultColumn = b;
  return *this;
}

Column& Column::hidden(bool b)
{
  colIsHiddenColumn = b;
  return *this;
}

Column& Column::defaultSort(bool b)
{
  colIsDefaultSortColumn = b;
  return *this;
}

Column& Column::sortFunc(const QString& sortFuncAsc, const QString& sortFuncDesc)
{
  colSortFuncAsc = sortFuncAsc;
  colSortFuncDesc = sortFuncDesc;
  return *this;
}

Column& Column::widget(QWidget *widget)
{
  colWidget = widget;
  return *this;
}

Column& Column::minWidget(QWidget *widget)
{
  colMinWidget = widget;
  return *this;
}

Column& Column::maxWidget(QWidget *widget)
{
  colMaxWidget = widget;
  return *this;
}

Column& Column::conditions(const QString& include, const QString& exclude)
{
  colIncludeCond = include;
  colExcludeCond = exclude;
  return *this;
}

Column& Column::condition(const QString& cond)
{
  colCondition = cond;
  return *this;
}

Column& Column::convertFunc(std::function<float(float)> unitConvertFunc)
{
  unitConvert = unitConvertFunc;
  return *this;
}

Column& Column::defaultSortOrder(Qt::SortOrder order)
{
  colDefaultSortOrd = order;
  return *this;
}

Column& Column::indexCondMap(const QStringList& indexMap)
{
  colIndexConditionMap = indexMap;
  return *this;
}

Column& Column::includesName(bool value)
{
  colQueryIncludesName = value;
  return *this;
}

Column& Column::distanceCol(bool value)
{
  colIsDistance = value;
  return *this;
}

QLineEdit *Column::getLineEditWidget() const
{
  return dynamic_cast<QLineEdit *>(colWidget);
}

QComboBox *Column::getComboBoxWidget() const
{
  return dynamic_cast<QComboBox *>(colWidget);
}

QCheckBox *Column::getCheckBoxWidget() const
{
  return dynamic_cast<QCheckBox *>(colWidget);
}

QSpinBox *Column::getSpinBoxWidget() const
{
  return dynamic_cast<QSpinBox *>(colWidget);
}

QSpinBox *Column::getMinSpinBoxWidget() const
{
  return dynamic_cast<QSpinBox *>(colMinWidget);
}

QSpinBox *Column::getMaxSpinBoxWidget() const
{
  return dynamic_cast<QSpinBox *>(colMaxWidget);
}
