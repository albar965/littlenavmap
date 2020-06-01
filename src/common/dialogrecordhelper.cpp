/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "common/dialogrecordhelper.h"

#include "sql/sqlrecord.h"
#include "common/unit.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QPlainTextEdit>

DialogRecordHelper::DialogRecordHelper(atools::sql::SqlRecord *recordParam, bool multiEditParam)
  : record(recordParam), multiEdit(multiEditParam)
{

}

bool DialogRecordHelper::isSetValue(QCheckBox *checkBox)
{
  if(multiEdit)
    // Box is checked
    return checkBox != nullptr && checkBox->isChecked();
  else
    // No multi edit copy always
    return true;
}

bool DialogRecordHelper::isRemoveValue(QCheckBox *checkBox)
{
  if(multiEdit)
    return checkBox == nullptr || !checkBox->isChecked();
  else
    return false;
}

void DialogRecordHelper::dialogToRecordStr(QLineEdit *widget, const QString& name, QCheckBox *checkBox, bool toUpper)
{
  if(isSetValue(checkBox))
    record->setValue(name, toUpper ? widget->text().toUpper() : widget->text());

  if(isRemoveValue(checkBox))
    // do not set to null but remove completely
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordStr(QPlainTextEdit *widget, const QString& name, QCheckBox *checkBox)
{
  if(isSetValue(checkBox))
    record->setValue(name, widget->toPlainText());

  if(isRemoveValue(checkBox))
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordFuel(QSpinBox *widget, const QString& name, QCheckBox *checkBox,
                                            std::function<float(float value, bool fuelAsVolume)> func,
                                            bool fuelAsVolume)
{
  if(isSetValue(checkBox))
  {
    if(func)
      record->setValue(name, Unit::rev(widget->value(), func, fuelAsVolume));
    else
      record->setValue(name, widget->value());
  }

  if(isRemoveValue(checkBox))
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordInt(QSpinBox *widget, const QString& name, QCheckBox *checkBox,
                                           std::function<float(float value)> func)
{
  if(isSetValue(checkBox))
  {
    if(func)
      record->setValue(name, Unit::rev(widget->value(), func));
    else
      record->setValue(name, widget->value());
  }

  if(isRemoveValue(checkBox))
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordInt(QSpinBox *widget, const QString& name, QCheckBox *checkBox)
{
  if(isSetValue(checkBox))
    record->setValue(name, widget->value());

  if(isRemoveValue(checkBox))
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordInt(QComboBox *widget, const QString& name, QCheckBox *checkBox)
{
  if(isSetValue(checkBox))
    record->setValue(name, widget->currentIndex());

  if(isRemoveValue(checkBox))
    record->remove(name);
}

void DialogRecordHelper::dialogToRecordDateTime(QDateTimeEdit *widget, const QString& name, QCheckBox *checkBox)
{
  if(isSetValue(checkBox))
    record->setValue(name, widget->dateTime());

  if(isRemoveValue(checkBox))
    record->remove(name);
}
