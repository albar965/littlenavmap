/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#ifndef AIRSPACETOOLBARHANDLER_H
#define AIRSPACETOOLBARHANDLER_H

#include "common/maptypes.h"

#include <QApplication>

class QAction;
class QActionGroup;
class QToolButton;
class MainWindow;

/* Manages the airspace toolbar, its tool buttons and all the actions in the drop down menus */
class AirspaceToolBarHandler :
  public QObject
{
  Q_OBJECT

public:
  AirspaceToolBarHandler(MainWindow *parent);
  virtual ~AirspaceToolBarHandler();
  void createToolButtons();

  void updateButtonsAndActions();

signals:
  void updateAirspaceTypes(map::MapAirspaceTypes types);

private:
  void updateAirspaceToolButtons();
  void updateAirspaceToolActions();
  void createAirspaceToolButton(const QString& icon, const QString& help,
                                const std::initializer_list<map::MapAirspaceTypes>& types,
                                bool groupActions = false);
  void actionTriggered();
  void actionGroupTriggered(QAction *action);

  QVector<QAction *> airspaceActions;
  QVector<QToolButton *> airspaceToolButtons;
  QVector<map::MapAirspaceTypes> airspaceToolButtonTypes;
  QVector<QActionGroup *> airspaceToolGroups;
  MainWindow *mainWindow;
};

#endif // AIRSPACETOOLBARHANDLER_H
