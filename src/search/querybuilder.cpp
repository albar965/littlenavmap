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

#include "search/querybuilder.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

QueryBuilderResultVector QueryBuilder::build() const
{
  QueryBuilderResultVector result;
  if(func)
  {
    for(const QueryWidget& queryWidget : queryWidgets)
    {
      if(queryWidget.isWidgetEnabled())
        result.append(func(queryWidget));
    }
  }
  return result;
}

const QVector<QWidget *> QueryBuilder::getWidgets() const
{
  QVector<QWidget *> widgets;
  for(const QueryWidget& queryWidget : queryWidgets)
    widgets.append(queryWidget.getWidget());
  return widgets;
}

const QStringList QueryBuilder::getColumns() const
{
  QStringList columns;
  for(const QueryWidget& queryWidget : queryWidgets)
    columns.append(queryWidget.getColumns());
  return columns;
}

void QueryBuilder::resetWidgets()
{
  for(QWidget *widget : getWidgets())
  {
    if(widget != nullptr)
    {
      QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(widget);
      if(lineEdit != nullptr)
        lineEdit->clear();

      QCheckBox *check = dynamic_cast<QCheckBox *>(widget);
      if(check != nullptr)
        check->setCheckState(check->isTristate() ? Qt::PartiallyChecked : Qt::Unchecked);

      QSpinBox *spin = dynamic_cast<QSpinBox *>(widget);
      if(spin != nullptr)
        spin->setValue(0);

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
      }
    }
  }
}

bool QueryWidget::isWidgetEnabled() const
{
  return widget != nullptr && widget->isEnabled();
}
