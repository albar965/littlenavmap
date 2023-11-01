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

#include "mapgui/mapmarkhandler.h"

#include "atools.h"
#include "common/constants.h"
#include "gui/actionbuttonhandler.h"
#include "gui/choicedialog.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "options/optiondata.h"
#include "perf/aircraftperfcontroller.h"
#include "route/routecontroller.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"

#include <QMessageBox>

MapMarkHandler::MapMarkHandler(MainWindow *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
  buttonHandler = new atools::gui::ActionButtonHandler(mainWindow);
}

MapMarkHandler::~MapMarkHandler()
{
  delete buttonHandler;
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
  map::MapTypes markTypesOld = markTypes;
  markTypes |= types;

  if(markTypes != markTypesOld)
  {
    flagsToActions();
    emit updateMarkTypes(markTypes);
  }
}

QStringList MapMarkHandler::getMarkTypesText() const
{
  QStringList types;
  if(markTypes & map::MARK_RANGE)
    types.append(tr("Range Rings"));
  if(markTypes & map::MARK_DISTANCE)
    types.append(tr("Measurement Lines"));
  if(markTypes & map::MARK_HOLDING)
    types.append(tr("Holdings"));
  if(markTypes & map::MARK_PATTERNS)
    types.append(tr("Traffic Patterns"));
  if(markTypes & map::MARK_MSA)
    types.append(tr("MSA Sector Diagrams"));
  return types;
}

void MapMarkHandler::resetSettingsToDefault()
{
  markTypes = map::MARK_ALL;
  flagsToActions();
}

void MapMarkHandler::clearRangeRings() const
{
  clearRangeRingsAndDistanceMarkers(false /* quiet */, map::MARK_RANGE);
}

void MapMarkHandler::clearDistanceMarkers() const
{
  clearRangeRingsAndDistanceMarkers(false /* quiet */, map::MARK_DISTANCE);
}

void MapMarkHandler::clearHoldings() const
{
  clearRangeRingsAndDistanceMarkers(false /* quiet */, map::MARK_HOLDING);
}

void MapMarkHandler::clearPatterns() const
{
  clearRangeRingsAndDistanceMarkers(false /* quiet */, map::MARK_PATTERNS);
}

void MapMarkHandler::clearMsa() const
{
  clearRangeRingsAndDistanceMarkers(false /* quiet */, map::MARK_MSA);
}

void MapMarkHandler::addToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  toolButton = new QToolButton(ui->toolBarMapOptions);

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

  ui->toolBarMapOptions->insertWidget(ui->actionMapShowRoute, toolButton);

  // Create and add actions to toolbar and menu =================================
  actionAll = new QAction(tr("&All User Features"), buttonMenu);
  actionAll->setToolTip(tr("Toggle all / current selection of user features"));
  actionAll->setStatusTip(actionAll->toolTip());
  buttonMenu->addAction(actionAll);
  buttonHandler->setAllAction(actionAll);
  ui->menuViewUserFeatures->addAction(actionAll);

  actionNone = new QAction(tr("&No User Features"), buttonMenu);
  actionNone->setToolTip(tr("Toggle none / current selection of user features"));
  actionNone->setStatusTip(actionNone->toolTip());
  buttonMenu->addAction(actionNone);
  buttonHandler->setNoneAction(actionNone);
  ui->menuViewUserFeatures->addAction(actionNone);

  ui->menuViewUserFeatures->addSeparator();
  buttonMenu->addSeparator();

  actionRangeRings = addAction(":/littlenavmap/resources/icons/rangerings.svg", tr("&Range Rings"), tr("Show or hide range rings"));
  actionMeasurementLines = addAction(":/littlenavmap/resources/icons/distancemeasure.svg", tr("&Measurement Lines"),
                                     tr("Show or hide measurement lines"));
  actionPatterns = addAction(":/littlenavmap/resources/icons/trafficpattern.svg", tr("&Traffic Patterns"),
                             tr("Show or hide traffic patterns"));
  actionHolds = addAction(":/littlenavmap/resources/icons/enroutehold.svg", tr("&Holdings"), tr("Show or hide holdings"));
  actionAirportMsa = addAction(":/littlenavmap/resources/icons/msa.svg", tr("&MSA Diagrams"), tr("Show or hide MSA sector diagrams"));

  // Connect all action signals to same handler method
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionAllTriggered, this, &MapMarkHandler::toolbarActionTriggered);
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionNoneTriggered, this, &MapMarkHandler::toolbarActionTriggered);
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionOtherTriggered, this, &MapMarkHandler::toolbarActionTriggered);
}

QAction *MapMarkHandler::addAction(const QString& icon, const QString& text, const QString& tooltip)
{
  QAction *action = new QAction(QIcon(icon), text, toolButton->menu());
  action->setToolTip(tooltip);
  action->setStatusTip(tooltip);
  action->setCheckable(true);

  buttonHandler->addOtherAction(action);
  toolButton->menu()->addAction(action);
  NavApp::getMainUi()->menuViewUserFeatures->addAction(action);

  return action;
}

void MapMarkHandler::toolbarActionTriggered(QAction *)
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

QStringList MapMarkHandler::mapFlagTexts(map::MapTypes types) const
{
  QStringList featureStr;
  if(types.testFlag(map::MARK_RANGE))
    featureStr.append(tr("range rings"));

  if(types.testFlag(map::MARK_DISTANCE))
    featureStr.append(tr("measurement lines"));

  if(types.testFlag(map::MARK_PATTERNS))
    featureStr.append(tr("traffic patterns"));

  if(types.testFlag(map::MARK_HOLDING))
    featureStr.append(tr("holdings"));

  if(types.testFlag(map::MARK_MSA))
    featureStr.append(tr("MSA diagrams"));

  return featureStr;
}

void MapMarkHandler::clearRangeRingsAndDistanceMarkers(bool quiet, map::MapTypes types) const
{
  if(!quiet)
  {
    // Only one type ========================
    QString text = atools::strJoin(mapFlagTexts(types), tr(", "), tr(" and "));
    int result = atools::gui::Dialog(mainWindow).showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_MARKS + QString::number(types),
                                                                    tr("Delete all %1 from map?").arg(text),
                                                                    tr("Do not &show this dialog again."),
                                                                    QMessageBox::Yes | QMessageBox::No,
                                                                    QMessageBox::No, QMessageBox::Yes);

    if(result == QMessageBox::Yes)
      NavApp::getMapWidgetGui()->clearAllMarkers(types);
  }
  else
    // More than one type from choice dialog
    NavApp::getMapWidgetGui()->clearAllMarkers(types);
}

void MapMarkHandler::routeResetAll()
{
  enum Choice
  {
    EMPTY_FLIGHT_PLAN,
    DELETE_TRAIL,
    DELETE_ACTIVE_LEG,
    RESTART_PERF,
    RESTART_LOGBOOK,
    REMOVE_MARK_RANGE,
    REMOVE_MARK_DISTANCE,
    REMOVE_MARK_HOLDING,
    REMOVE_MARK_PATTERNS,
    REMOVE_MARK_MSA
  };

  qDebug() << Q_FUNC_INFO;

  // Create a dialog with four checkboxes
  atools::gui::ChoiceDialog choiceDialog(mainWindow, QApplication::applicationName() + tr(" - Reset for new Flight"),
                                         tr("Select items to reset for a new flight"), lnm::RESET_FOR_NEW_FLIGHT_DIALOG, "RESET.html");
  choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

  choiceDialog.addCheckBox(EMPTY_FLIGHT_PLAN, tr("&Create an empty flight plan"), QString(), true /* checked */);

  choiceDialog.addLine();
  choiceDialog.addCheckBox(DELETE_TRAIL, tr("&Delete user aircraft trail"),
                           tr("Delete simulator aircraft trail from map and elevation profile"), true /* checked */);
  choiceDialog.addCheckBox(DELETE_ACTIVE_LEG, tr("&Reset active flight plan leg"),
                           tr("Reset the active flight plan leg"), true /* checked */);
  choiceDialog.addCheckBox(RESTART_PERF, tr("Restart the aircraft &performance collection"),
                           tr("Restarts the background aircraft performance collection"), true /* checked */);
  choiceDialog.addCheckBox(RESTART_LOGBOOK, tr("Reset flight detection in &logbook"),
                           tr("Reset the logbook to detect takeoff and landing for new logbook entries"), true /* checked */);

  choiceDialog.addLine();
  choiceDialog.addLabel(tr("Remove user features placed on map:"));
  choiceDialog.addCheckBox(REMOVE_MARK_RANGE, tr("&Range rings"), QString(), false /* checked */);
  choiceDialog.addCheckBox(REMOVE_MARK_DISTANCE, tr("&Measurement lines"), QString(), false /* checked */);
  choiceDialog.addCheckBox(REMOVE_MARK_HOLDING, tr("&Holdings"), QString(), false /* checked */);
  choiceDialog.addCheckBox(REMOVE_MARK_PATTERNS, tr("&Traffic patterns"), QString(), false /* checked */);
  choiceDialog.addCheckBox(REMOVE_MARK_MSA, tr("&MSA diagrams"), QString(), false /* checked */);
  choiceDialog.addSpacer();

  choiceDialog.restoreState();

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    if(choiceDialog.isChecked(EMPTY_FLIGHT_PLAN))
      mainWindow->routeNew();

    if(choiceDialog.isChecked(DELETE_TRAIL))
      mainWindow->deleteAircraftTrail(true /* quiet */);

    if(choiceDialog.isChecked(DELETE_ACTIVE_LEG))
      NavApp::getRouteController()->resetActiveLeg();

    if(choiceDialog.isChecked(RESTART_PERF))
      NavApp::getAircraftPerfController()->restartCollection(true /* quiet */);

    if(choiceDialog.isChecked(RESTART_LOGBOOK))
    {
      NavApp::getLogdataController()->resetTakeoffLandingDetection();
      NavApp::getMapWidgetGui()->resetTakeoffLandingDetection();
    }

    map::MapTypes types = map::NONE;
    types.setFlag(map::MARK_RANGE, choiceDialog.isChecked(REMOVE_MARK_RANGE));
    types.setFlag(map::MARK_DISTANCE, choiceDialog.isChecked(REMOVE_MARK_DISTANCE));
    types.setFlag(map::MARK_HOLDING, choiceDialog.isChecked(REMOVE_MARK_HOLDING));
    types.setFlag(map::MARK_PATTERNS, choiceDialog.isChecked(REMOVE_MARK_PATTERNS));
    types.setFlag(map::MARK_MSA, choiceDialog.isChecked(REMOVE_MARK_MSA));

    if(types != map::NONE)
      clearRangeRingsAndDistanceMarkers(true /* quiet */, types);
  }
}
