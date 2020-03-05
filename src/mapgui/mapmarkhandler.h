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

#ifndef LNM_MAPMARKHANDLER_H
#define LNM_MAPMARKHANDLER_H
#include "common/mapflags.h"

#include <QObject>

class QAction;
class QToolButton;

/*
 * Adds a toolbutton and four actions to it that allow to show or hide user features like holds, range rings and others.
 */
class MapMarkHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit MapMarkHandler(QWidget *parent);
  virtual ~MapMarkHandler();

  /* Initialize, create actions and add button to toolbar */
  void addToolbarButton();

  void saveState();
  void restoreState();

  map::MapMarkTypes getMarkTypes() const
  {
    return markTypes;
  }

  bool isShown(map::MapMarkTypes type) const
  {
    return markTypes & type;
  }

  void showMarkTypes(map::MapMarkTypes types);

  QString getMarkTypesText() const;

  void resetSettingsToDefault();

signals:
  /* Redraw map */
  void updateMarkTypes(map::MapMarkTypes types);

private:
  void toolbarActionTriggered();
  void actionAllTriggered();
  void actionNoneTriggered();

  void flagsToActions();
  void actionsToFlags();
  QAction *addButton(const QString& icon, const QString& text, const QString& tooltip, map::MapMarkTypes type);

  /* Actions for toolbar button and menu */
  QAction *actionAll = nullptr, *actionNone = nullptr, *actionRangeRings = nullptr, *actionMeasurementLines = nullptr,
          *actionHolds = nullptr, *actionPatterns = nullptr;

  /* Toolbutton getting all actions for dropdown menu */
  QToolButton *toolButton = nullptr;

  /* Default is to show all */
  map::MapMarkTypes markTypes = map::MARK_ALL;
};

#endif // LNM_MAPMARKHANDLER_H
