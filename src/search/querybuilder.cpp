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

#include "search/querybuilder.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

void QueryBuilder::resetWidgets()
{
  for(QWidget *widget : widgets)
  {
    QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(widget);
    if(lineEdit != nullptr)
    {
      lineEdit->clear();
      continue;
    }

    QCheckBox *check = dynamic_cast<QCheckBox *>(widget);
    if(check != nullptr)
    {
      check->setCheckState(check->isTristate() ? Qt::PartiallyChecked : Qt::Unchecked);
      continue;
    }

    QSpinBox *spin = dynamic_cast<QSpinBox *>(widget);
    if(spin != nullptr)
    {
      spin->setValue(0);
      continue;
    }

    QComboBox *cb = dynamic_cast<QComboBox *>(widget);
    if(cb != nullptr)
    {
      if(cb->isEditable())
      {
        cb->setCurrentText(QString());
        cb->setCurrentIndex(-1);
      }
      else
        cb->setCurrentIndex(0);
      continue;
    }
  }
}
