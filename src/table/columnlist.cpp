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

ColumnList::ColumnList(const QString& table)
  : tablename(table)
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
}

ColumnList& ColumnList::append(const Column& col)
{
  Column *c = new Column(col);
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

void ColumnList::assignWidget(const QString& field, QWidget *widget)
{
  if(nameColumnMap.contains(field))
    nameColumnMap.value(field)->widget(widget);
  else
    qWarning() << "Cannot assign line edit to" << field;
}

void ColumnList::clearWidgets(const QStringList& exceptColNames)
{
  for(Column *cd : columns)
    if(!exceptColNames.contains(cd->getColumnName()))
    {
      if(QLineEdit * le = cd->getLineEditWidget())
        le->setText("");
      else if(QComboBox * cb = cd->getComboBoxWidget())
        cb->setCurrentIndex(0);
      else if(QCheckBox * check = cd->getCheckBoxWidget())
      {
        if(check->isTristate())
          check->setCheckState(Qt::PartiallyChecked);
        else
          check->setCheckState(Qt::Unchecked);
      }
      else if(QSpinBox * spin = cd->getSpinBoxWidget())
        spin->setValue(0);
      else if(QSpinBox * maxspin = cd->getMaxSpinBoxWidget())
        spin->setValue(maxspin->maximum());
      else if(QSpinBox * minspin = cd->getMaxSpinBoxWidget())
        spin->setValue(minspin->minimum());
    }
}

void ColumnList::enableWidgets(bool enabled, const QStringList& exceptColNames)
{
  for(Column *cd : columns)
    if(!exceptColNames.contains(cd->getColumnName()))
      if(cd->getWidget() != nullptr)
        cd->getWidget()->setEnabled(enabled);
}
