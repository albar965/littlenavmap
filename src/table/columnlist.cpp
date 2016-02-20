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

#include <QLineEdit>
#include <QComboBox>

ColumnList::ColumnList(const QString& table)
  : tablename(table)
{
}

ColumnList::~ColumnList()
{

}

void ColumnList::clear()
{
  columns.clear();
  nameColumnMap.clear();
}

ColumnList& ColumnList::append(Column col)
{
  columns.append(col);
  nameColumnMap.insert(col.getColumnName(), columns.size() - 1);
  return *this;
}

const Column *ColumnList::getColumn(const QString& field) const
{
  if(nameColumnMap.contains(field))
    return &columns.at(nameColumnMap.value(field));
  else
    return nullptr;
}

void ColumnList::assignLineEdit(const QString& field, QLineEdit *widget)
{
  if(nameColumnMap.contains(field))
    columns[nameColumnMap.value(field)].lineEdit(widget);
  else
    qWarning() << "Cannot assign line edit to" << field;
}

void ColumnList::assignComboBox(const QString& field, QComboBox *widget)
{
  if(nameColumnMap.contains(field))
    columns[nameColumnMap.value(field)].comboBox(widget);
  else
    qWarning() << "Cannot assign combo box to" << field;
}

void ColumnList::clearWidgets(const QStringList& exceptColNames)
{
  for(Column& cd : columns)
    if(!exceptColNames.contains(cd.getColumnName()))
    {
      if(cd.getLineEditWidget() != nullptr)
        cd.getLineEditWidget()->setText("");
      else if(cd.getComboBoxWidget() != nullptr)
        cd.getComboBoxWidget()->setCurrentIndex(0);
    }
}

void ColumnList::enableWidgets(bool enabled, const QStringList& exceptColNames)
{
  for(Column& cd : columns)
    if(!exceptColNames.contains(cd.getColumnName()))
    {
      if(cd.getLineEditWidget() != nullptr)
        cd.getLineEditWidget()->setEnabled(enabled);
      else if(cd.getComboBoxWidget() != nullptr)
        cd.getComboBoxWidget()->setEnabled(enabled);
    }
}
