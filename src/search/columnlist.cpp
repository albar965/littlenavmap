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

#include "search/columnlist.h"
#include "search/column.h"
#include "common/unit.h"

#include <QDebug>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>

ColumnList::ColumnList(const QString& tableName, const QString& idColumnName)
  : table(tableName), idColumn(idColumnName)
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
  table.clear();
  minDistanceWidget = nullptr;
  maxDistanceWidget = nullptr;
  distanceDirectionWidget = nullptr;
  distanceCheckBox = nullptr;
}

bool ColumnList::isDistanceCheckBoxChecked() const
{
  return distanceCheckBox != nullptr ? distanceCheckBox->isChecked() : false;
}

void ColumnList::updateUnits()
{
  // Replace widget suffices and table headers
  for(Column *col : columns)
  {
    col->colDisplayName = Unit::replacePlaceholders(col->colOrigDisplayName);

    QSpinBox *sb = col->getSpinBoxWidget();
    if(sb != nullptr)
      sb->setSuffix(Unit::replacePlaceholders(sb->suffix(), col->colWidgetSuffix));

    sb = col->getMinSpinBoxWidget();
    if(sb != nullptr)
      sb->setSuffix(Unit::replacePlaceholders(sb->suffix(), col->colMinWidgetSuffix));

    sb = col->getMaxSpinBoxWidget();
    if(sb != nullptr)
      sb->setSuffix(Unit::replacePlaceholders(sb->suffix(), col->colMaxWidgetSuffix));
  }

  if(minDistanceWidget != nullptr)
    minDistanceWidget->setSuffix(
      Unit::replacePlaceholders(minDistanceWidget->suffix(), minDistanceWidgetSuffix));

  if(maxDistanceWidget != nullptr)
    maxDistanceWidget->setSuffix(
      Unit::replacePlaceholders(maxDistanceWidget->suffix(), maxDistanceWidgetSuffix));
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

bool ColumnList::hasColumn(const QString& field) const
{
  return nameColumnMap.contains(field);
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

void ColumnList::assignDistanceSearchWidgets(QCheckBox *checkBox,
                                             QComboBox *directionWidget, QSpinBox *minWidget,
                                             QSpinBox *maxWidget)
{
  distanceCheckBox = checkBox;
  minDistanceWidget = minWidget;
  maxDistanceWidget = maxWidget;
  distanceDirectionWidget = directionWidget;
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

void ColumnList::resetWidgets(const QStringList& exceptColNames)
{
  for(Column *cd : columns)
  {
    // Reset widgets assigned to columns
    if(!exceptColNames.contains(cd->getColumnName()))
    {
      if(QLineEdit *le = cd->getLineEditWidget())
        le->setText(QString());
      if(QComboBox *cb = cd->getComboBoxWidget())
      {
        if(cb->isEditable())
        {
          cb->setCurrentText(QString());
          cb->setCurrentIndex(-1);
        }
        else
          cb->setCurrentIndex(0);
      }
      if(QCheckBox *check = cd->getCheckBoxWidget())
      {
        if(check->isTristate())
          check->setCheckState(Qt::PartiallyChecked);
        else
          check->setCheckState(Qt::Unchecked);
      }
      if(QSpinBox *spin = cd->getSpinBoxWidget())
        spin->setValue(0);

      if(QSpinBox *maxspin = cd->getMaxSpinBoxWidget())
        maxspin->setValue(maxspin->maximum());
      if(QSpinBox *minspin = cd->getMinSpinBoxWidget())
        minspin->setValue(minspin->minimum());
    }
  }

  // Reset distance search widgets
  if(minDistanceWidget != nullptr)
  {
    // Values should match the GUI values
    minDistanceWidget->setValue(0);
    minDistanceWidget->setMinimum(0);
    minDistanceWidget->setMaximum(8000);
  }

  if(maxDistanceWidget != nullptr)
  {
    // Values should match the GUI values
    maxDistanceWidget->setValue(100);
    maxDistanceWidget->setMinimum(100);
    maxDistanceWidget->setMaximum(8000);
  }
  if(distanceDirectionWidget != nullptr)
    distanceDirectionWidget->setCurrentIndex(0);

  if(distanceCheckBox != nullptr)
    distanceCheckBox->setCheckState(Qt::Unchecked);
}

void ColumnList::enableWidgets(bool enabled, const QStringList& exceptColNames)
{
  // Enable widgets assigned to columns
  for(Column *cd : columns)
  {
    if(!exceptColNames.contains(cd->getColumnName()) && cd->getWidget() != nullptr)
      cd->getWidget()->setEnabled(enabled);
  }

  // Enable distance search widgets
  if(minDistanceWidget != nullptr)
    minDistanceWidget->setEnabled(enabled);

  if(maxDistanceWidget != nullptr)
    maxDistanceWidget->setEnabled(enabled);

  if(distanceCheckBox != nullptr)
    distanceCheckBox->setEnabled(enabled);

  if(distanceDirectionWidget != nullptr)
    distanceDirectionWidget->setEnabled(enabled);
}
