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

#include "common/unitstringtool.h"
#include "common/unit.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QWidget>
#include <QDebug>

UnitStringTool::UnitStringTool()
{
}

void UnitStringTool::init(const QList<QWidget *>& widgets, bool fuelAsVolume)
{
  QList<QWidget *> widgetList;
  for(QWidget *widget:widgets)
  {
    if(const QLayout *layout = dynamic_cast<const QLayout *>(widget))
    {
      // Recurse into layouts
      for(int i = 0; i < layout->count(); i++)
        widgetList.append(layout->itemAt(i)->widget());
    }
    else
      widgetList.append(widget);
  }

  // Append widgets to list
  for(QWidget *widget:widgetList)
    widgetDataList.append(WidgetData(widget));

  // Save text and replace placeholders
  for(WidgetData& data : widgetDataList)
    update(data, true /* save original text */, fuelAsVolume);
}

void UnitStringTool::update(bool fuelAsVolume)
{
  for(WidgetData& data : widgetDataList)
    update(data, false, fuelAsVolume);
}

void UnitStringTool::updateBase(WidgetData& widgetData, bool save, bool fuelAsVolume)
{
  if(save)
    widgetData.texts << widgetData.widget->toolTip() << widgetData.widget->statusTip();
  widgetData.widget->setToolTip(Unit::replacePlaceholders(
                                  widgetData.texts.at(widgetData.texts.size() - 2), fuelAsVolume));
  widgetData.widget->setStatusTip(Unit::replacePlaceholders(
                                    widgetData.texts.at(widgetData.texts.size() - 1), fuelAsVolume));
}

void UnitStringTool::update(WidgetData& widgetData, bool save, bool fuelAsVolume)
{
  QWidget *widget = widgetData.widget;

  // Replace texts in widget
  if(const QLayout *layout = dynamic_cast<const QLayout *>(widget))
  {
    // Update all widgets in layout
    for(int i = 0; i < layout->count(); i++)
      update(widgetData, save, fuelAsVolume);
  }
  else if(QLineEdit *le = dynamic_cast<QLineEdit *>(widget))
  {
    if(save)
      widgetData.texts << le->placeholderText();
    le->setPlaceholderText(Unit::replacePlaceholders(widgetData.texts.first(), fuelAsVolume));
  }
  else if(QTextEdit *te = dynamic_cast<QTextEdit *>(widget))
  {
    if(save)
      widgetData.texts << te->placeholderText();
    te->setPlaceholderText(Unit::replacePlaceholders(widgetData.texts.first(), fuelAsVolume));
  }
  else if(QSpinBox *sb = dynamic_cast<QSpinBox *>(widget))
  {
    if(save)
      widgetData.texts << sb->prefix() << sb->suffix();

    sb->setPrefix(Unit::replacePlaceholders(widgetData.texts.at(0), fuelAsVolume));
    sb->setSuffix(Unit::replacePlaceholders(widgetData.texts.at(1), fuelAsVolume));
  }
  else if(QDoubleSpinBox *dsb = dynamic_cast<QDoubleSpinBox *>(widget))
  {
    if(save)
      widgetData.texts << dsb->prefix() << dsb->suffix();
    dsb->setPrefix(Unit::replacePlaceholders(widgetData.texts.at(0), fuelAsVolume));
    dsb->setSuffix(Unit::replacePlaceholders(widgetData.texts.at(1), fuelAsVolume));
  }
  else if(QComboBox *cb = dynamic_cast<QComboBox *>(widget))
  {
    for(int i = 0; i < cb->count(); i++)
    {
      if(save)
        widgetData.texts << cb->itemText(i);
      cb->setItemText(i, Unit::replacePlaceholders(widgetData.texts.at(i), fuelAsVolume));
    }
  }
  else if(QAction *a = dynamic_cast<QAction *>(widget))
  {
    if(save)
      widgetData.texts << a->text();
    a->setText(Unit::replacePlaceholders(widgetData.texts.first(), fuelAsVolume));
  }
  else if(QAbstractButton *b = dynamic_cast<QAbstractButton *>(widget))
  {
    if(save)
      widgetData.texts << b->text();
    b->setText(Unit::replacePlaceholders(widgetData.texts.first(), fuelAsVolume));
  }
  else
    qWarning() << "Found unsupported widet type in save" << widget->metaObject()->className();

  updateBase(widgetData, save, fuelAsVolume);
}
