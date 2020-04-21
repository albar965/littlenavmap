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

#include "mapgui/mapmarkhandler.h"

#include "settings/settings.h"
#include "common/constants.h"
#include "navapp.h"
#include "options/optiondata.h"
#include "ui_mainwindow.h"

MapMarkHandler::MapMarkHandler(QWidget *parent)
  : QObject(parent)
{

}

MapMarkHandler::~MapMarkHandler()
{

}

void MapMarkHandler::saveState()
{
  actionsToFlags();
  atools::settings::Settings::instance().setValue(lnm::MAP_MARK_DISPLAY, static_cast<int>(markTypes));
}

void MapMarkHandler::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
    markTypes = static_cast<map::MapMarkTypes>(atools::settings::Settings::instance().
                                               valueInt(lnm::MAP_MARK_DISPLAY, static_cast<int>(map::MARK_ALL)));
  flagsToActions();
}

void MapMarkHandler::showMarkTypes(map::MapMarkTypes types)
{
  markTypes |= types;
  flagsToActions();
}

QString MapMarkHandler::getMarkTypesText() const
{
  QStringList types;
  if(markTypes == map::MARK_NONE)
    return tr("None");

  if(markTypes & map::MARK_RANGE_RINGS)
    types.append(tr("Range Rings"));
  if(markTypes & map::MARK_MEASUREMENT)
    types.append(tr("Measurement Lines"));
  if(markTypes & map::MARK_HOLDS)
    types.append(tr("Holdings"));
  if(markTypes & map::MARK_PATTERNS)
    types.append(tr("Traffic Patterns"));
  return types.join(tr((", ")));
}

void MapMarkHandler::resetSettingsToDefault()
{
  markTypes = map::MARK_ALL;
  flagsToActions();
}

void MapMarkHandler::addToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  toolButton = new QToolButton(ui->toolbarMapOptions);

  // Create and add toolbar button =====================================
  toolButton->setIcon(QIcon(":/littlenavmap/resources/icons/userfeatures.svg"));
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolButton->setToolTip(tr("Select user features like range rings or holdings for display"));
  toolButton->setStatusTip(toolButton->toolTip());
  toolButton->setCheckable(true);

  ui->toolbarMapOptions->insertWidget(ui->actionMapShowRoute, toolButton);

  // Create and add actions to toolbar and menu =================================
  actionAll = new QAction(tr("&All"), toolButton);
  actionAll->setToolTip(tr("Show all user features"));
  actionAll->setStatusTip(actionAll->toolTip());
  toolButton->addAction(actionAll);
  ui->menuViewUserFeatures->addAction(actionAll);
  connect(actionAll, &QAction::triggered, this, &MapMarkHandler::actionAllTriggered);

  actionNone = new QAction(tr("&None"), toolButton);
  actionNone->setToolTip(tr("Hide all user features"));
  actionNone->setStatusTip(actionNone->toolTip());
  toolButton->addAction(actionNone);
  ui->menuViewUserFeatures->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &MapMarkHandler::actionNoneTriggered);

  ui->menuViewUserFeatures->addSeparator();

  actionRangeRings = addButton(":/littlenavmap/resources/icons/rangerings.svg", tr("&Range Rings"),
                               tr("Show or hide range rings"), map::MARK_RANGE_RINGS);
  actionMeasurementLines = addButton(":/littlenavmap/resources/icons/distancemeasure.svg", tr("&Measurement Lines"),
                                     tr("Show or hide measurement lines"), map::MARK_MEASUREMENT);
  actionPatterns = addButton(":/littlenavmap/resources/icons/trafficpattern.svg", tr("&Traffic Patterns"),
                             tr("Show or hide traffic patterns"), map::MARK_PATTERNS);
  actionHolds = addButton(":/littlenavmap/resources/icons/hold.svg", tr("&Holdings"),
                          tr("Show or hide holdings"), map::MARK_HOLDS);
}

QAction *MapMarkHandler::addButton(const QString& icon, const QString& text, const QString& tooltip,
                                   map::MapMarkTypes type)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QAction *action = new QAction(QIcon(icon), text, toolButton);
  action->setToolTip(tooltip);
  action->setStatusTip(tooltip);
  action->setData(static_cast<int>(type));
  action->setCheckable(true);

  toolButton->addAction(action);
  ui->menuViewUserFeatures->addAction(action);

  connect(action, &QAction::triggered, this, &MapMarkHandler::toolbarActionTriggered);

  return action;
}

void MapMarkHandler::actionAllTriggered()
{
  markTypes = map::MARK_ALL;
  flagsToActions();
}

void MapMarkHandler::actionNoneTriggered()
{
  markTypes = map::MARK_NONE;
  flagsToActions();
}

void MapMarkHandler::toolbarActionTriggered()
{
  actionsToFlags();
  toolButton->setChecked(markTypes & map::MARK_ALL);
  emit updateMarkTypes(markTypes);
}

void MapMarkHandler::flagsToActions()
{
  actionRangeRings->setChecked(markTypes & map::MARK_RANGE_RINGS);
  actionMeasurementLines->setChecked(markTypes & map::MARK_MEASUREMENT);
  actionHolds->setChecked(markTypes & map::MARK_HOLDS);
  actionPatterns->setChecked(markTypes & map::MARK_PATTERNS);
  toolButton->setChecked(markTypes & map::MARK_ALL);
}

void MapMarkHandler::actionsToFlags()
{
  markTypes = map::MARK_NONE;
  if(actionRangeRings->isChecked())
    markTypes |= map::MARK_RANGE_RINGS;
  if(actionMeasurementLines->isChecked())
    markTypes |= map::MARK_MEASUREMENT;
  if(actionHolds->isChecked())
    markTypes |= map::MARK_HOLDS;
  if(actionPatterns->isChecked())
    markTypes |= map::MARK_PATTERNS;
}
