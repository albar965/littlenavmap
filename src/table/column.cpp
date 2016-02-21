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

#include "table/column.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

Column::Column(const QString& columnName, const QString& columnDisplayName)
  : colName(columnName), displayName(columnDisplayName)
{

}

Column::Column(const QString& columnName, QWidget *widget, const QString& columnDisplayName)
  : colName(columnName), displayName(columnDisplayName), colWidget(widget)
{

}

Column& Column::canGroupShow(bool b)
{
  groupByShow = b;
  return *this;
}

Column& Column::canMin(bool b)
{
  groupByMin = b;
  return *this;
}

Column& Column::canMax(bool b)
{
  groupByMax = b;
  return *this;
}

Column& Column::canSum(bool b)
{
  groupBySum = b;
  return *this;
}

Column& Column::canFilter(bool b)
{
  canBeFiltered = b;
  return *this;
}

Column& Column::canGroup(bool b)
{
  canBeGrouped = b;
  return *this;
}

Column& Column::canSort(bool b)
{
  canBeSorted = b;
  return *this;
}

Column& Column::defaultCol(bool b)
{
  isDefaultColumn = b;
  return *this;
}

Column& Column::hidden(bool b)
{
  isHiddenColumn = b;
  return *this;
}

Column& Column::defaultSort(bool b)
{
  isDefaultSortColumn = b;
  return *this;
}

Column& Column::alwaysAnd(bool b)
{
  isAlwaysAndColumn = b;
  return *this;
}

Column& Column::sortFunc(const QString& sortFuncAsc, const QString& sortFuncDesc)
{
  sortFuncForColAsc = sortFuncAsc;
  sortFuncForColDesc = sortFuncDesc;
  return *this;
}

Column& Column::widget(QWidget *widget)
{
  colWidget = widget;
  return *this;
}

Column& Column::minWidget(QWidget *widget)
{
  minColWidget = widget;
  return *this;
}

Column& Column::maxWidget(QWidget *widget)
{
  maxColWidget = widget;
  return *this;
}

Column& Column::defaultSortOrder(Qt::SortOrder order)
{
  defaultSortOrd = order;
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
  return dynamic_cast<QSpinBox *>(minColWidget);
}

QSpinBox *Column::getMaxSpinBoxWidget() const
{
  return dynamic_cast<QSpinBox *>(maxColWidget);
}
