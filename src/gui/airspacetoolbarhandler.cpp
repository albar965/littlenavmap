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

#include "gui/airspacetoolbarhandler.h"

#include "navapp.h"
#include "ui_mainwindow.h"
#include "common/maptypes.h"

#include <QAction>
#include <QToolButton>
#include <QDebug>

AirspaceToolBarHandler::AirspaceToolBarHandler(MainWindow *parent)
  : mainWindow(parent)
{
  connect(NavApp::getMainUi()->actionShowAirspaces, &QAction::toggled,
          this, &AirspaceToolBarHandler::allAirspacesToggled);
}

AirspaceToolBarHandler::~AirspaceToolBarHandler()
{

}

void AirspaceToolBarHandler::allAirspacesToggled()
{
  updateAirspaceToolButtons();
}

void AirspaceToolBarHandler::updateAirspaceToolButtons()
{
  map::MapAirspaceTypes types = NavApp::getShownMapAirspaces();
  qDebug() << Q_FUNC_INFO << types;

  for(int i = 0; i < airspaceToolButtons.size(); i++)
  {
    if(airspaceToolGroups.at(i) == nullptr)
      // Depress the button if the state is not default
      airspaceToolButtons.at(i)->setChecked(airspaceToolButtonTypes.at(i) & types);
    else
      // Depress button if the first is not selected in groups
      airspaceToolButtons.at(i)->setChecked(!airspaceToolGroups.at(i)->actions().first()->isChecked());

    airspaceToolButtons.at(i)->setEnabled(NavApp::getMainUi()->actionShowAirspaces->isChecked());
  }
}

void AirspaceToolBarHandler::updateAirspaceToolActions()
{
  map::MapAirspaceTypes types = NavApp::getShownMapAirspaces();
  qDebug() << Q_FUNC_INFO << types;

  for(QAction *action : airspaceActions)
  {
    map::MapAirspaceTypes typeFromAction = static_cast<map::MapAirspaceTypes>(action->data().toInt());

    if(!(typeFromAction & map::AIRSPACE_ALL_ON) && !(typeFromAction & map::AIRSPACE_ALL_OFF))
    {
      action->blockSignals(true);
      action->setChecked(typeFromAction & types);
      action->blockSignals(false);
    }
  }
}

void AirspaceToolBarHandler::updateButtonsAndActions()
{
  updateAirspaceToolActions();
  updateAirspaceToolButtons();
}

void AirspaceToolBarHandler::actionTriggered()
{
  qDebug() << Q_FUNC_INFO;

  QAction *sendAction = dynamic_cast<QAction *>(sender());
  if(sendAction != nullptr)
  {
    map::MapAirspaceTypes typeFromAction = static_cast<map::MapAirspaceTypes>(sendAction->data().toInt());
    map::MapAirspaceTypes newTypes = NavApp::getShownMapAirspaces();

    if(typeFromAction & map::AIRSPACE_ALL_ON)
      newTypes |= typeFromAction;
    else if(typeFromAction & map::AIRSPACE_ALL_OFF)
      newTypes &= ~typeFromAction;
    else
    {
      for(QAction *action : airspaceActions)
      {
        map::MapAirspaceTypes type = static_cast<map::MapAirspaceTypes>(action->data().toInt());

        if(!(type & map::AIRSPACE_ALL_ON) && !(type & map::AIRSPACE_ALL_OFF))
        {
          if(action->isChecked())
            newTypes |= type;
          else
            newTypes &= ~type;
        }
      }
    }

    // Remove the button only flags
    newTypes &= ~map::AIRSPACE_ALL_ON;
    newTypes &= ~map::AIRSPACE_ALL_OFF;

    emit updateAirspaceTypes(newTypes);
    updateAirspaceToolButtons();
    updateAirspaceToolActions();
  }
}

void AirspaceToolBarHandler::actionGroupTriggered(QAction *action)
{
  qDebug() << Q_FUNC_INFO;

  map::MapAirspaceTypes newTypes = NavApp::getShownMapAirspaces();

  QActionGroup *group = dynamic_cast<QActionGroup *>(sender());
  if(group != nullptr)
  {
    // Have to do the group selection here since it is broken by blockSignals
    for(QAction *groupAction :  group->actions())
    {
      map::MapAirspaceTypes type = static_cast<map::MapAirspaceTypes>(groupAction->data().toInt());
      if(groupAction != action)
        newTypes &= ~type;
      else
        newTypes |= type;

      action->blockSignals(true);
      groupAction->setChecked(groupAction == action);
      action->blockSignals(false);
    }
  }

  emit updateAirspaceTypes(newTypes);
  updateAirspaceToolButtons();
}

void AirspaceToolBarHandler::createToolButtons()
{
  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceicao.svg",
                           tr("Select ICAO airspaces"),
                           {map::CLASS_A, map::CLASS_B, map::CLASS_C, map::CLASS_D, map::CLASS_E});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacefir.svg",
                           tr("Select FIR airspaces"),
                           {map::CLASS_F, map::CLASS_G});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacerestr.svg",
                           tr("Select MOA, restricted, prohibited and danger airspaces"),
                           {map::MOA, map::RESTRICTED, map::PROHIBITED, map::DANGER});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacespec.svg",
                           tr("Select warning, alert and training airspaces"),
                           {map::WARNING, map::ALERT, map::TRAINING});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceother.svg",
                           tr("Select centers and other airspaces"),
                           {map::CENTER, map::TOWER, map::CLEARANCE, map::GROUND, map::DEPARTURE, map::APPROACH,
                            map::NATIONAL_PARK, map::MODEC, map::RADAR});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacealt.svg",
                           tr("Select altitude limitations for airspace display"),
                           {map::AIRSPACE_ALL_ALTITUDE,
                            map::AIRSPACE_AT_FLIGHTPLAN,
                            map::AIRSPACE_BELOW_10000, map::AIRSPACE_BELOW_18000,
                            map::AIRSPACE_ABOVE_10000, map::AIRSPACE_ABOVE_18000}, true);
}

void AirspaceToolBarHandler::createAirspaceToolButton(const QString& icon, const QString& help,
                                                      const std::initializer_list<map::MapAirspaceTypes>& types,
                                                      bool groupActions)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  map::MapAirspaceTypes allTypes = map::AIRSPACE_NONE;
  for(const map::MapAirspaceTypes& type : types)
    allTypes |= type;

  ui->menuAirspaces->addSeparator();

  QToolButton *button = new QToolButton(ui->toolBarAirspaces);
  button->setIcon(QIcon(icon));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(help);
  button->setStatusTip(help);
  button->setCheckable(true);
  airspaceToolButtons.append(button);
  airspaceToolButtonTypes.append(allTypes);

  if(!groupActions)
  {
    // Add all on / all off menu items
    QAction *action = new QAction(tr("All"), button);
    action->setData(static_cast<int>(map::MapAirspaceTypes(map::AIRSPACE_ALL_ON | allTypes)));
    button->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);

    action = new QAction(tr("None"), button);
    action->setData(static_cast<int>(map::MapAirspaceTypes(map::AIRSPACE_ALL_OFF | allTypes)));
    button->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);
  }

  QActionGroup *group = nullptr;
  QObject *parent = button;

  if(groupActions) // Added radion button group
  {
    group = new QActionGroup(button);
    connect(group, &QActionGroup::triggered, this, &AirspaceToolBarHandler::actionGroupTriggered);
    parent = group;
    airspaceToolGroups.append(group);
  }
  else
    airspaceToolGroups.append(nullptr);

  for(const map::MapAirspaceTypes& type : types)
  {
    QAction *action = new QAction(map::airspaceTypeToString(type), parent);
    action->setCheckable(true);
    action->setData(static_cast<int>(type));
    button->addAction(action);
    airspaceActions.append(action);
    if(!groupActions)
      connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);
    ui->menuAirspaces->addAction(action);
  }
  ui->toolBarAirspaces->addWidget(button);
}
