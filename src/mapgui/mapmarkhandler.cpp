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
  delete toolButton;
}

void MapMarkHandler::saveState()
{
  actionsToFlags();
  atools::settings::Settings::instance().setValueVar(lnm::MAP_MARK_DISPLAY, markTypes.asFlagType());
}

void MapMarkHandler::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {

    QVariant defaultValue = static_cast<atools::util::FlagType>(map::MARK_ALL);
    markTypes = atools::settings::Settings::instance().valueVar(lnm::MAP_MARK_DISPLAY, defaultValue).value<atools::util::FlagType>();
  }
  flagsToActions();
}

void MapMarkHandler::showMarkTypes(map::MapTypes types)
{
  markTypes |= types;
  flagsToActions();
}

QString MapMarkHandler::getMarkTypesText() const
{
  QStringList types;
  if(markTypes == map::NONE)
    return tr("None");

  if(markTypes & map::MARK_RANGE)
    types.append(tr("Range Rings"));
  if(markTypes & map::MARK_DISTANCE)
    types.append(tr("Measurement Lines"));
  if(markTypes & map::MARK_HOLDING)
    types.append(tr("Holdings"));
  if(markTypes & map::MARK_PATTERNS)
    types.append(tr("Traffic Patterns"));
  if(markTypes & map::MARK_MSA)
    types.append(tr("Airport MSA"));
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

  // Add tear off menu to button =======
  toolButton->setMenu(new QMenu(toolButton));
  QMenu *buttonMenu = toolButton->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  ui->toolbarMapOptions->insertWidget(ui->actionMapShowRoute, toolButton);

  // Create and add actions to toolbar and menu =================================
  actionAll = new QAction(tr("&All"), buttonMenu);
  actionAll->setToolTip(tr("Show all user features"));
  actionAll->setStatusTip(actionAll->toolTip());
  buttonMenu->addAction(actionAll);
  ui->menuViewUserFeatures->addAction(actionAll);
  connect(actionAll, &QAction::triggered, this, &MapMarkHandler::actionAllTriggered);

  actionNone = new QAction(tr("&None"), buttonMenu);
  actionNone->setToolTip(tr("Hide all user features"));
  actionNone->setStatusTip(actionNone->toolTip());
  buttonMenu->addAction(actionNone);
  ui->menuViewUserFeatures->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &MapMarkHandler::actionNoneTriggered);

  ui->menuViewUserFeatures->addSeparator();
  buttonMenu->addSeparator();

  actionRangeRings = addButton(":/littlenavmap/resources/icons/rangerings.svg", tr("&Range Rings"), tr("Show or hide range rings"));
  actionMeasurementLines = addButton(":/littlenavmap/resources/icons/distancemeasure.svg", tr("&Measurement Lines"),
                                     tr("Show or hide measurement lines"));
  actionPatterns = addButton(":/littlenavmap/resources/icons/trafficpattern.svg", tr("&Traffic Patterns"),
                             tr("Show or hide traffic patterns"));
  actionHolds = addButton(":/littlenavmap/resources/icons/enroutehold.svg", tr("&Holdings"), tr("Show or hide holdings"));
  actionAirportMsa = addButton(":/littlenavmap/resources/icons/msa.svg", tr("&MSA Diagrams"), tr("Show or hide airport MSA sectors"));
}

QAction *MapMarkHandler::addButton(const QString& icon, const QString& text, const QString& tooltip)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QAction *action = new QAction(QIcon(icon), text, toolButton->menu());
  action->setToolTip(tooltip);
  action->setStatusTip(tooltip);
  action->setCheckable(true);

  toolButton->menu()->addAction(action);
  ui->menuViewUserFeatures->addAction(action);

  connect(action, &QAction::triggered, this, &MapMarkHandler::toolbarActionTriggered);

  return action;
}

void MapMarkHandler::actionAllTriggered()
{
  markTypes = map::MARK_ALL;
  flagsToActions();
  emit updateMarkTypes(markTypes);
}

void MapMarkHandler::actionNoneTriggered()
{
  markTypes = map::NONE;
  flagsToActions();
  emit updateMarkTypes(markTypes);
}

void MapMarkHandler::toolbarActionTriggered()
{
  actionsToFlags();
  toolButton->setChecked(markTypes & map::MARK_ALL);
  emit updateMarkTypes(markTypes);
}

void MapMarkHandler::flagsToActions()
{
  actionRangeRings->setChecked(markTypes.testFlag(map::MARK_RANGE));
  actionMeasurementLines->setChecked(markTypes.testFlag(map::MARK_DISTANCE));
  actionHolds->setChecked(markTypes.testFlag(map::MARK_HOLDING));
  actionAirportMsa->setChecked(markTypes.testFlag(map::MARK_MSA));
  actionPatterns->setChecked(markTypes.testFlag(map::MARK_PATTERNS));
  toolButton->setChecked(markTypes & map::MARK_ALL);
}

void MapMarkHandler::actionsToFlags()
{
  markTypes = map::NONE;
  if(actionRangeRings->isChecked())
    markTypes |= map::MARK_RANGE;
  if(actionMeasurementLines->isChecked())
    markTypes |= map::MARK_DISTANCE;
  if(actionHolds->isChecked())
    markTypes |= map::MARK_HOLDING;
  if(actionAirportMsa->isChecked())
    markTypes |= map::MARK_MSA;
  if(actionPatterns->isChecked())
    markTypes |= map::MARK_PATTERNS;
}
