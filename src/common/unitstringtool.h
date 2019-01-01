/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel albar965@mailbox.org
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

#ifndef LNM_UNITSTRINGTOOL_H
#define LNM_UNITSTRINGTOOL_H

#include <QHash>

class QWidget;
class QString;

/*
 * Changes units (%speed%, %dist%) in widget prefix, suffix and texts
 */
class UnitStringTool
{
public:
  UnitStringTool();

  /* Collect widgets, create text backups and replace unit placeholders */
  void init(const QList<QWidget *>& widgets, bool fuelAsVolume = false);

  /* Replace unit placeholders from backup texts. */
  void update(bool fuelAsVolume = false);

private:
  struct WidgetData
  {
    WidgetData(QWidget *w) : widget(w)
    {
    }

    /* List of backup texts */
    QStringList texts;
    QWidget *widget;
  };

  QList<WidgetData> widgetDataList;
  void update(WidgetData& widgetData, bool save, bool fuelAsVolume);

  /* Replace tooltip and status texts */
  void updateBase(WidgetData& widgetData, bool save, bool fuelAsVolume);

};

#endif // LNM_UNITSTRINGTOOL_H
