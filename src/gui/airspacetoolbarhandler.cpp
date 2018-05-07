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
  connect(NavApp::getMainUi()->actionShowAirspacesOnline, &QAction::toggled,
          this, &AirspaceToolBarHandler::allAirspacesToggled);
}

AirspaceToolBarHandler::~AirspaceToolBarHandler()
{

}

void AirspaceToolBarHandler::allAirspacesToggled()
{
  updateButtonsAndActions();
}

void AirspaceToolBarHandler::updateAirspaceToolButtons()
{
  map::MapAirspaceFilter filter = NavApp::getShownMapAirspaces();
  // qDebug() << Q_FUNC_INFO << types;

  QAction *airspaceAction = NavApp::getMainUi()->actionShowAirspaces;
  bool hasAirspaces = NavApp::hasDatabaseAirspaces();
  airspaceAction->setEnabled(hasAirspaces);

  QAction *airspaceActionOnline = NavApp::getMainUi()->actionShowAirspacesOnline;
  bool hasAirspacesOnline = NavApp::hasOnlineData();
  airspaceActionOnline->setEnabled(hasAirspacesOnline);

  // Enable or disable all tool buttons
  for(int i = 0; i < airspaceToolButtons.size(); i++)
  {
    if(airspaceToolGroups.at(i) == nullptr)
      // Depress the button if the state is not default
      airspaceToolButtons.at(i)->setChecked(airspaceToolButtonFilters.at(i).types & filter.types ||
                                            airspaceToolButtonFilters.at(i).flags & filter.flags);
    else
      // Depress button if the first is not selected in groups
      airspaceToolButtons.at(i)->setChecked(!airspaceToolGroups.at(i)->actions().first()->isChecked());

    // Enable button if any airspace type is enabled
    airspaceToolButtons.at(i)->setEnabled((airspaceAction->isChecked() && hasAirspaces) ||
                                          (airspaceActionOnline->isChecked() && hasAirspacesOnline));
  }
}

void AirspaceToolBarHandler::updateAirspaceToolActions()
{
  map::MapAirspaceFilter filter = NavApp::getShownMapAirspaces();
  // qDebug() << Q_FUNC_INFO << types;

  for(QAction *action : airspaceActions)
  {
    map::MapAirspaceFilter filterFromAction = action->data().value<map::MapAirspaceFilter>();

    if(!(filterFromAction.flags & map::AIRSPACE_ALL_ON) && !(filterFromAction.flags & map::AIRSPACE_ALL_OFF))
    {
      action->blockSignals(true);
      action->setChecked(filterFromAction.types & filter.types || filterFromAction.flags & filter.flags);
      action->blockSignals(false);
    }
  }

  for(QAction *action : airspaceActions)
    action->setEnabled(
      NavApp::getMainUi()->actionShowAirspaces->isChecked() ||
      NavApp::getMainUi()->actionShowAirspacesOnline->isChecked());
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
    map::MapAirspaceFilter typeFromAction = sendAction->data().value<map::MapAirspaceFilter>();
    map::MapAirspaceFilter newFilter = NavApp::getShownMapAirspaces();

    if(typeFromAction.flags & map::AIRSPACE_ALL_ON)
      newFilter.types |= typeFromAction.types;
    else if(typeFromAction.flags & map::AIRSPACE_ALL_OFF)
      newFilter.types &= ~typeFromAction.types;
    else
    {
      for(QAction *action : airspaceActions)
      {
        map::MapAirspaceFilter filter = action->data().value<map::MapAirspaceFilter>();

        if(!(filter.flags & map::AIRSPACE_ALL_ON) && !(filter.flags & map::AIRSPACE_ALL_OFF))
        {
          if(action->isChecked())
            newFilter.types |= filter.types;
          else
            newFilter.types &= ~filter.types;
        }
      }
    }

    // Remove the button only flags
    newFilter.flags &= ~map::AIRSPACE_ALL_ON;
    newFilter.flags &= ~map::AIRSPACE_ALL_OFF;

    emit updateAirspaceTypes(newFilter);
    updateAirspaceToolButtons();
    updateAirspaceToolActions();
  }
}

void AirspaceToolBarHandler::actionGroupTriggered(QAction *action)
{
  qDebug() << Q_FUNC_INFO;

  map::MapAirspaceFilter newFilter = NavApp::getShownMapAirspaces();
  newFilter.flags = map::AIRSPACE_FLAG_NONE;

  QActionGroup *group = dynamic_cast<QActionGroup *>(sender());
  if(group != nullptr)
  {
    // Have to do the group selection here since it is broken by blockSignals
    for(QAction *groupAction :  group->actions())
    {
      map::MapAirspaceFilter filter = action->data().value<map::MapAirspaceFilter>();
      if(groupAction == action)
        newFilter.flags |= filter.flags;

      action->blockSignals(true);
      groupAction->setChecked(groupAction == action);
      action->blockSignals(false);
    }
  }

  emit updateAirspaceTypes(newFilter);
  updateAirspaceToolButtons();
}

void AirspaceToolBarHandler::createToolButtons()
{
  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceicao.svg",
                           tr("Select ICAO airspaces"),
                           {map::CLASS_A, map::CLASS_B, map::CLASS_C, map::CLASS_D, map::CLASS_E}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacefir.svg",
                           tr("Select FIR airspaces"),
                           {map::CLASS_F, map::CLASS_G}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacerestr.svg",
                           tr("Select MOA, restricted, prohibited and danger airspaces"),
                           {map::MOA, map::RESTRICTED, map::PROHIBITED, map::GLIDERPROHIBITED, map::DANGER}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacespec.svg",
                           tr("Select warning, alert and training airspaces"),
                           {map::WARNING, map::CAUTION, map::ALERT, map::TRAINING}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceother.svg",
                           tr("Select centers and other airspaces"),
                           {map::CENTER, map::TOWER, map::CLEARANCE, map::GROUND, map::DEPARTURE, map::APPROACH,
                            map::NATIONAL_PARK, map::MODEC, map::RADAR, map::WAVEWINDOW, map::ONLINE_OBSERVER}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacealt.svg",
                           tr("Select altitude limitations for airspace display"),
                           {},
                           {map::AIRSPACE_ALL_ALTITUDE,
                            map::AIRSPACE_AT_FLIGHTPLAN,
                            map::AIRSPACE_BELOW_10000, map::AIRSPACE_BELOW_18000,
                            map::AIRSPACE_ABOVE_10000, map::AIRSPACE_ABOVE_18000}, true);
}

void AirspaceToolBarHandler::createAirspaceToolButton(const QString& icon, const QString& help,
                                                      const std::initializer_list<map::MapAirspaceTypes>& types,
                                                      const std::initializer_list<map::MapAirspaceFlags>& flags,
                                                      bool groupActions)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  map::MapAirspaceTypes allTypes = map::AIRSPACE_NONE;
  for(const map::MapAirspaceTypes& type : types)
    allTypes |= type;

  map::MapAirspaceFlags allFlags = map::AIRSPACE_FLAG_NONE;
  for(const map::MapAirspaceFlags& flag : flags)
    allFlags |= flag;

  ui->menuViewAirspaces->addSeparator();

  QToolButton *button = new QToolButton(ui->toolBarAirspaces);
  button->setIcon(QIcon(icon));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(help);
  button->setStatusTip(help);
  button->setCheckable(true);
  airspaceToolButtons.append(button);

  map::MapAirspaceFilter filter;
  filter.types = allTypes;
  filter.flags = allFlags;
  airspaceToolButtonFilters.append(filter);

  if(!groupActions)
  {
    // Add all on / all off menu items
    QAction *action = new QAction(tr("All"), button);
    action->setToolTip(tr("Enable all airspaces in this category"));
    action->setStatusTip(action->toolTip());
    map::MapAirspaceFilter filterOn;
    filterOn.types = allTypes;
    filterOn.flags = map::AIRSPACE_ALL_ON | allFlags;
    action->setData(QVariant::fromValue(filterOn));
    button->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);

    action = new QAction(tr("None"), button);
    action->setToolTip(tr("Disable all airspaces in this category"));
    action->setStatusTip(action->toolTip());
    map::MapAirspaceFilter filterOff;
    filterOff.types = allTypes;
    filterOff.flags = map::AIRSPACE_ALL_OFF | allFlags;
    action->setData(QVariant::fromValue(filterOff));
    button->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);
  }

  QActionGroup *group = nullptr;
  QObject *parent = button;

  if(groupActions) // Add radion button group
  {
    group = new QActionGroup(button);
    connect(group, &QActionGroup::triggered, this, &AirspaceToolBarHandler::actionGroupTriggered);
    parent = group;
    airspaceToolGroups.append(group);

    // Add radio button items based on flags
    for(const map::MapAirspaceFlags& flag: flags)
    {
      QAction *action = new QAction(map::airspaceFlagToString(flag), parent);
      action->setCheckable(true);

      map::MapAirspaceFilter f;
      f.types = map::AIRSPACE_NONE;
      f.flags = flag;

      action->setData(QVariant::fromValue(f));
      button->addAction(action);
      airspaceActions.append(action);
      if(!groupActions)
        connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);
      ui->menuViewAirspaces->addAction(action);
    }
  }
  else
  {
    airspaceToolGroups.append(nullptr);

    // Add tool button items based on types
    for(const map::MapAirspaceTypes& type : types)
    {
      QAction *action = new QAction(map::airspaceTypeToString(type), parent);
      action->setCheckable(true);

      map::MapAirspaceFilter f;
      f.types = type;
      f.flags = map::AIRSPACE_FLAG_NONE;

      action->setData(QVariant::fromValue(f));

      button->addAction(action);
      airspaceActions.append(action);
      if(!groupActions)
        connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionTriggered);
      ui->menuViewAirspaces->addAction(action);
    }
  }

  ui->toolBarAirspaces->addWidget(button);
}
