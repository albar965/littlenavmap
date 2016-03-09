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

#include "table/columnlist.h"
#include "logging/loggingdefs.h"
#include "table/column.h"

#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>

ColumnList::ColumnList(const QString& table, const QString& idColumnName)
  : tablename(table), idColumn(idColumnName)
{
}

ColumnList::~ColumnList()
{
  clear();
}

void ColumnList::clear()
{
  qDeleteAll(columns);
  columns.clear();
  nameColumnMap.clear();
  tablename.clear();
  minDistanceWidget = nullptr;
  maxDistanceWidget = nullptr;
  distanceDirectionWidget = nullptr;
  distanceCheckBox = nullptr;
  distanceUpdate = nullptr;
}

ColumnList& ColumnList::append(const Column& col)
{
  Column *c = new Column(col);
  c->index = columns.size();
  columns.append(c);
  nameColumnMap.insert(col.getColumnName(), c);
  return *this;
}

const Column *ColumnList::getColumn(const QString& field) const
{
  if(nameColumnMap.contains(field))
    return nameColumnMap.value(field);
  else
    return nullptr;
}

const Column *ColumnList::getColumn(int index) const
{
  return columns.at(index);
}

const Column *ColumnList::getIdColumn() const
{
  if(nameColumnMap.contains(idColumn))
    return nameColumnMap.value(idColumn);
  else
    return nullptr;
}

const Column *ColumnList::getDefaultSortColumn() const
{
  for(const Column *c : columns)
    if(c->isDefaultSort())
      return c;

  return nullptr;
}

void ColumnList::assignWidget(const QString& field, QWidget *widget)
{
  if(nameColumnMap.contains(field))
    nameColumnMap.value(field)->widget(widget);
  else
    qWarning() << "Cannot assign widget to" << field;
}

void ColumnList::assignDistanceSearchWidgets(QPushButton *updateButton, QCheckBox *checkBox,
                                             QComboBox *directionWidget, QSpinBox *minWidget,
                                             QSpinBox *maxWidget)
{
  distanceCheckBox = checkBox;
  minDistanceWidget = minWidget;
  maxDistanceWidget = maxWidget;
  distanceDirectionWidget = directionWidget;
  distanceUpdate = updateButton;
}

void ColumnList::assignMinMaxWidget(const QString& field, QWidget *minWidget, QWidget *maxWidget)
{
  if(nameColumnMap.contains(field))
  {
    nameColumnMap.value(field)->minWidget(minWidget);
    nameColumnMap.value(field)->maxWidget(maxWidget);
  }
  else
    qWarning() << "Cannot assign widget to" << field;
}

void ColumnList::clearWidgets(const QStringList& exceptColNames)
{
  for(Column *cd : columns)
    if(!exceptColNames.contains(cd->getColumnName()))
    {
      if(QLineEdit * le = cd->getLineEditWidget())
        le->setText("");
      if(QComboBox * cb = cd->getComboBoxWidget())
        cb->setCurrentIndex(0);
      if(QCheckBox * check = cd->getCheckBoxWidget())
      {
        if(check->isTristate())
          check->setCheckState(Qt::PartiallyChecked);
        else
          check->setCheckState(Qt::Unchecked);
      }
      if(QSpinBox * spin = cd->getSpinBoxWidget())
        spin->setValue(0);

      if(QSpinBox * maxspin = cd->getMaxSpinBoxWidget())
        maxspin->setValue(maxspin->maximum());
      if(QSpinBox * minspin = cd->getMinSpinBoxWidget())
        minspin->setValue(minspin->minimum());
    }

  if(minDistanceWidget != nullptr)
  {
    minDistanceWidget->setValue(0);
    minDistanceWidget->setMinimum(0);
    minDistanceWidget->setMaximum(5000);
  }

  if(maxDistanceWidget != nullptr)
  {
    maxDistanceWidget->setValue(100);
    maxDistanceWidget->setMinimum(100);
    maxDistanceWidget->setMaximum(5000);
  }
  if(distanceDirectionWidget != nullptr)
    distanceDirectionWidget->setCurrentIndex(0);

  if(distanceCheckBox != nullptr)
    distanceCheckBox->setCheckState(Qt::Unchecked);
}

void ColumnList::enableWidgets(bool enabled, const QStringList& exceptColNames)
{
  for(Column *cd : columns)
    if(!exceptColNames.contains(cd->getColumnName()))
      if(cd->getWidget() != nullptr)
        cd->getWidget()->setEnabled(enabled);

  if(distanceUpdate != nullptr)
    distanceUpdate->setEnabled(enabled);

  if(minDistanceWidget != nullptr)
    minDistanceWidget->setEnabled(enabled);

  if(maxDistanceWidget != nullptr)
    maxDistanceWidget->setEnabled(enabled);

  if(distanceCheckBox != nullptr)
    distanceCheckBox->setEnabled(enabled);

  if(distanceDirectionWidget != nullptr)
    distanceDirectionWidget->setEnabled(enabled);
}
