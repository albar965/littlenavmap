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

#ifndef AIRSPACETOOLBARHANDLER_H
#define AIRSPACETOOLBARHANDLER_H

#include "common/mapflags.h"

#include <QApplication>
#include <QVector>

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

  /* Update buttons and menus based on NavApp::getShownMapAirspaces() */
  void updateButtonsAndActions();

  const QVector<QToolButton *>& getAirspaceToolButtons() const
  {
    return airspaceToolButtons;
  }

signals:
  void updateAirspaceTypes(map::MapAirspaceFilter types);

private:
  /* Update button depressed state or not */
  void updateAirspaceToolButtons();

  /* Check or uncheck menu items */
  void updateAirspaceToolActions();

  void createAirspaceToolButton(const QString& icon, const QString& help,
                                const std::initializer_list<map::MapAirspaceTypes>& types,
                                const std::initializer_list<map::MapAirspaceFlags>& flags,
                                bool groupActions = false);
  void actionTriggered();
  void actionGroupTriggered(QAction *action);
  void allAirspacesToggled();

  /* List of all actions */
  QVector<QAction *> airspaceActions;

  /* List of actions that are not grouped */
  QVector<QToolButton *> airspaceToolButtons;

  /* List of grouped actions or null if not grouped */
  QVector<QActionGroup *> airspaceToolGroups;

  /* Flags and types for each button */
  QVector<map::MapAirspaceFilter> airspaceToolButtonFilters;
  MainWindow *mainWindow;

};

#endif // AIRSPACETOOLBARHANDLER_H
