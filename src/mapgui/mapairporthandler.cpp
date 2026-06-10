/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/mapairporthandler.h"

#include "atools.h"
#include "common/constants.h"
#include "common/unit.h"
#include "gui/signalblocker.h"
#include "app/navapp.h"
#include "options/optiondata.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"

#include <QWidgetAction>
#include <QStringBuilder>
#include <QActionGroup>

/* Fixed min and max values depending on distance unit */
static const int MINIMUM_SLIDER_FT = 0;
static const int MAXIMUM_SLIDER_FT = 200; // * 100

static const int MINIMUM_SLIDER_METER = 0;
static const int MAXIMUM_SLIDER_METER = 50; // * 100

static const int SLIDER_SCALE = 100;

namespace apinternal {

// AirportSliderAction ==================================================================

AirportSliderAction::AirportSliderAction(QObject *parent, bool typeMinSlider)
  : QWidgetAction(parent), minimumSlider(typeMinSlider)
{
  // Min slider to the left and max slider to the right
  sliderValueReal = minimumSlider ? getMinValueReal() : -getMaxValueReal();
  setValuesReal(sliderValueReal);
}

void AirportSliderAction::saveState() const
{
  atools::settings::Settings::instance().setValue(minimumSlider ? lnm::MAP_AIRPORT_RUNWAY_LENGTH_MIN : lnm::MAP_AIRPORT_RUNWAY_LENGTH_MAX,
                                                  sliderValueReal);
}

void AirportSliderAction::restoreState()
{
  sliderValueReal =
    atools::settings::Settings::instance().valueInt(minimumSlider ? lnm::MAP_AIRPORT_RUNWAY_LENGTH_MIN : lnm::MAP_AIRPORT_RUNWAY_LENGTH_MAX,
                                                    minimumSlider ? getMinValueReal() : getMaxValueReal());
  setValuesReal(sliderValueReal);
  sliderDistUnit = Unit::getUnitShortDist();
}

void AirportSliderAction::optionsChanged(const optc::OptionChangeFlags& changeFlags)
{
  // Set all sliders to new range for given unit and reset to unlimited
  // Block signals to avoid recursion

  if(changeFlags.testFlag(optc::OPTION_CHANGE_UNITS))
  {
    if(Unit::getUnitShortDist() != sliderDistUnit)
    {
      // Units have changed
      switch(sliderDistUnit)
      {
        case opts::DIST_SHORT_FT:
          // Old was ft. Convert to new local unit.
          sliderValueReal = atools::roundToInt(Unit::distShortFeetF(sliderValueReal));
          break;

        case opts::DIST_SHORT_METER:
          // Old was meter. Convert to new local unit.
          sliderValueReal = atools::roundToInt(Unit::distShortMeterF(sliderValueReal));
          break;
      }

      sliderDistUnit = Unit::getUnitShortDist();
      sliderValueReal = atools::minmax(getMinValueReal(), getMaxValueReal(), sliderValueReal);
    }

    setValuesReal(sliderValueReal);
    setMinMaxValues();
  }
}

void AirportSliderAction::setLimit(int limitReal)
{
  // Set ranges depending on type min : max sliders
  if(minimumSlider)
  {
    sliderLimitMinReal = getMinValueReal();
    sliderLimitMaxReal = limitReal == getMaxValueReal() ? getMaxValueReal() : limitReal - 1;
  }
  else
  {
    sliderLimitMinReal = limitReal == getMinValueReal() ? getMinValueReal() : limitReal + 1;
    sliderLimitMaxReal = getMaxValueReal();
  }
}

QWidget *AirportSliderAction::createWidget(QWidget *parent)
{
  QSlider *slider = new QSlider(Qt::Horizontal, parent);
  setMinMaxValue(slider);
  slider->setTickPosition(minimumSlider ? QSlider::TicksAbove : QSlider::TicksBelow);
  slider->setInvertedAppearance(!minimumSlider); // Minimum and maximum appear at their opposite location if true
  slider->setTickInterval(10);
  slider->setPageStep(10);
  slider->setSingleStep(10);
  slider->setTracking(true);
  setValueReal(slider, sliderValueReal);

  if(minimumSlider)
    slider->setToolTip(tr("Set minimum runway length for airports to display.\n"
                          "Airport visibility might also be affected by zoom distance."));
  else
    slider->setToolTip(tr("Set maximum runway length for airports to display."));

  connect(slider, &QSlider::valueChanged, this, &AirportSliderAction::sliderValueChanged);

  // Signal forwarded to MapAirportHandler::runwaySliderValueChanged()
  connect(slider, &QSlider::valueChanged, this, &AirportSliderAction::valueChanged);

  // Signal forwarded to MapAirportHandler::runwaySliderReleased()
  connect(slider, &QSlider::sliderReleased, this, &AirportSliderAction::sliderReleased);

  // Add to list (register)
  sliders.append(slider);
  return slider;
}

void AirportSliderAction::deleteWidget(QWidget *widget)
{
  QSlider *slider = dynamic_cast<QSlider *>(widget);
  if(slider != nullptr)
  {
    disconnect(slider, &QSlider::valueChanged, this, &AirportSliderAction::sliderValueChanged);
    disconnect(slider, &QSlider::valueChanged, this, &AirportSliderAction::valueChanged);
    disconnect(slider, &QSlider::sliderReleased, this, &AirportSliderAction::sliderReleased);
    sliders.removeAll(slider);
    delete widget;
  }
}

void AirportSliderAction::sliderValueChanged(int valueRaw)
{
  // Convert raw, possibly negative value to real
  sliderValueReal = rawToReal(valueRaw);

  // Limit to bounds as set by other slider to avoid overlapping ranges
  sliderValueReal = atools::minmax(sliderLimitMinReal, sliderLimitMaxReal, sliderValueReal);

  // Set to other sliders like ones on tearoff menus
  setValuesReal(sliderValueReal);
}

int AirportSliderAction::getMinValueReal() const
{
  switch(sliderDistUnit)
  {
    case opts::DIST_SHORT_FT:
      return MINIMUM_SLIDER_FT;

    case opts::DIST_SHORT_METER:
      return MINIMUM_SLIDER_METER;
  }
  return MINIMUM_SLIDER_FT;
}

int AirportSliderAction::getMaxValueReal() const
{
  switch(sliderDistUnit)
  {
    case opts::DIST_SHORT_FT:
      return MAXIMUM_SLIDER_FT;

    case opts::DIST_SHORT_METER:
      return MAXIMUM_SLIDER_METER;
  }
  return MAXIMUM_SLIDER_FT;
}

void AirportSliderAction::setMinMaxValue(QSlider *slider)
{
  // Min is left 0 to 140 from left to right
  // Max is right from -140 from right to left
  slider->setMinimum(realToRaw(minimumSlider ? getMinValueReal() : getMaxValueReal()));
  slider->setMaximum(realToRaw(minimumSlider ? getMaxValueReal() : getMinValueReal()));
}

void AirportSliderAction::setMinMaxValues()
{
  atools::gui::SignalBlocker blocker(sliders);
  for(QSlider *slider : std::as_const(sliders))
    setMinMaxValue(slider);
}

void AirportSliderAction::setValueReal(QSlider *slider, int valueReal)
{
  slider->setValue(realToRaw(valueReal)); // Convert to raw value
}

void AirportSliderAction::setValuesReal(int realValue)
{
  atools::gui::SignalBlocker blocker(sliders);
  for(QSlider *slider : std::as_const(sliders))
    setValueReal(slider, realValue);
}

void AirportSliderAction::reset()
{
  sliderValueReal = minimumSlider ? getMinValueReal() : getMaxValueReal();
  setValuesReal(sliderValueReal);
}

// AirportLabelAction =======================================================================================

void AirportLabelAction::setText(const QString& textParam)
{
  text = textParam;
  // Set text to all registered labels
  for(QLabel *label : std::as_const(labels))
    label->setText(QStringLiteral("  ") % text); // Add space for margin
}

QWidget *AirportLabelAction::createWidget(QWidget *parent)
{
  QLabel *label = new QLabel(text, parent);
  label->setWordWrap(true); // Set wordwrap to avoid menu resizing while changing text
  labels.append(label);
  return label;
}

void AirportLabelAction::deleteWidget(QWidget *widget)
{
  labels.removeAll(dynamic_cast<QLabel *>(widget));
  delete widget;
}

} // namespace internal

// MapAirportHandler =======================================================================================

MapAirportHandler::MapAirportHandler(QObject *parent)
  : QObject(parent)
{
}

MapAirportHandler::~MapAirportHandler()
{
  ATOOLS_DELETE_LOG(actionGroupAddon);
  ATOOLS_DELETE_LOG(toolButton);
}

void MapAirportHandler::saveState()
{
  sliderActionRunwayLengthMin->saveState();
  sliderActionRunwayLengthMax->saveState();
  actionsToFlags();
  atools::settings::Settings::instance().setValueVar(lnm::MAP_AIRPORT, airportTypes.asFlagType());
}

void MapAirportHandler::restoreState()
{
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_MAP_SETTINGS))
  {
    airportTypes = atools::settings::Settings::instance().valueVar(lnm::MAP_AIRPORT, map::AIRPORT_DEFAULT).value<map::MapTypes::FlagType>();
    sliderActionRunwayLengthMin->restoreState();
    sliderActionRunwayLengthMax->restoreState();
  }
  actionEmpty->setEnabled(OptionData::instance().getFlags().testFlag(opts::MAP_EMPTY_AIRPORTS));

  runwaySliderValueChanged();
  flagsToActions();
  updateButtons();
}

int MapAirportHandler::isMinimumRunwaySet() const
{
  return sliderActionRunwayLengthMin->getSliderValueReal() > sliderActionRunwayLengthMin->getMinValueReal();
}

int MapAirportHandler::isMaximumRunwaySet() const
{
  return sliderActionRunwayLengthMax->getSliderValueReal() < sliderActionRunwayLengthMax->getMaxValueReal();
}

int MapAirportHandler::getMinimumRunwayFt() const
{
  return atools::roundToInt(Unit::rev(sliderActionRunwayLengthMin->getSliderValueReal() * SLIDER_SCALE, Unit::distShortFeetF));
}

int MapAirportHandler::getMaximumRunwayFt() const
{
  return atools::roundToInt(Unit::rev(sliderActionRunwayLengthMax->getSliderValueReal() * SLIDER_SCALE, Unit::distShortFeetF));
}

void MapAirportHandler::resetSettingsToDefault()
{
  airportTypes = map::AIRPORT_DEFAULT;
  sliderActionRunwayLengthMin->reset();
  sliderActionRunwayLengthMax->reset();
  flagsToActions();
  runwaySliderValueChanged();
}

void MapAirportHandler::optionsChanged(const optc::OptionChangeFlags& changeFlags)
{
  actionEmpty->setEnabled(OptionData::instance().getFlags().testFlag(opts::MAP_EMPTY_AIRPORTS));
  sliderActionRunwayLengthMin->optionsChanged(changeFlags);
  sliderActionRunwayLengthMax->optionsChanged(changeFlags);
  runwaySliderValueChanged();
}

void MapAirportHandler::insertToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  toolButton = new QToolButton(ui->toolBarMapOptions);

  // Connect master switch button
  connect(ui->actionMapShowAirports, &QAction::toggled, this, &MapAirportHandler::toolbarActionTriggered);

  // Create and add toolbar button =====================================
  toolButton->setIcon(QIcon(":/littlenavmap/resources/icons/airportmenu.svg"));
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolButton->setToolTip(tr("Select airport types to show.\n"
                            "Button is highlighted if any filter is selected."));
  toolButton->setStatusTip(toolButton->toolTip());
  toolButton->setCheckable(true);

  // Add tear off menu to button =======
  toolButton->setMenu(new QMenu(toolButton));
  QMenu *buttonMenu = toolButton->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  ui->toolBarMapOptions->insertWidget(ui->actionMapShowAirportWeather, toolButton);

  // Create and add actions to toolbar and menu =================================
  actionReset = new QAction(tr("&Reset airport display options"), buttonMenu);
  actionReset->setIcon(QIcon(":/littlenavmap/resources/icons/clear.svg"));
  actionReset->setToolTip(tr("Reset to default and show all airport types"));
  actionReset->setStatusTip(actionReset->toolTip());
  buttonMenu->addAction(actionReset);
  ui->menuViewAirport->addAction(actionReset);
  connect(actionReset, &QAction::triggered, this, &MapAirportHandler::actionResetTriggered);

  actionAddonOnly = new QAction(tr("&Show only add-on airports"), buttonMenu);
  actionAddonOnly->setIcon(QIcon(":/littlenavmap/resources/icons/airportaddon.svg"));
  actionAddonOnly->setToolTip(tr("Change all settings to show only add-on airports at all zoom distances"));
  actionAddonOnly->setStatusTip(actionAddonOnly->toolTip());
  buttonMenu->addAction(actionAddonOnly);
  ui->menuViewAirport->addAction(actionAddonOnly);
  connect(actionAddonOnly, &QAction::triggered, this, &MapAirportHandler::actionOnlyAddonTriggered);

  ui->menuViewAirport->addSeparator();
  buttonMenu->addSeparator();

  // actionMapShowAirports Strg+Alt+H
  actionHard = addAction(":/littlenavmap/resources/icons/airport.svg", tr("&Hard surface"),
                         tr("Show airports with at least one hard surface runway"), QKeySequence(tr("Ctrl+Alt+J")));
  actionSoft = addAction(":/littlenavmap/resources/icons/airportsoft.svg", tr("&Soft surface"),
                         tr("Show airports with soft runway surfaces only"), QKeySequence(tr("Ctrl+Alt+S")));

  ui->menuViewAirport->addSeparator();
  buttonMenu->addSeparator();

  actionWater = addAction(":/littlenavmap/resources/icons/airportwater.svg", tr("&Seaplane Bases"),
                          tr("Show airports having only water runways"), QKeySequence(tr("Ctrl+Alt+U")));
  actionHelipad = addAction(":/littlenavmap/resources/icons/airporthelipad.svg", tr("&Heliports"),
                            tr("Show airports having only helipads"), QKeySequence(tr("Ctrl+Alt+X")));
  actionEmpty = addAction(":/littlenavmap/resources/icons/airportempty.svg", tr("&Empty"),
                          tr("Show airports having no special features"), QKeySequence(tr("Ctrl+Alt+E")));
  actionUnlighted = addAction(":/littlenavmap/resources/icons/airportlight.svg", tr("&Not lighted"),
                              tr("Show unlighted airports"), QKeySequence());
  actionNoProcedures = addAction(":/littlenavmap/resources/icons/airportproc.svg", tr("&No procedure"),
                                 tr("Show airports having no SID, STAR or approach procedure"), QKeySequence());
  actionClosed = addAction(":/littlenavmap/resources/icons/airportclosed.svg", tr("&Closed"), tr("Show closed airports"), QKeySequence());
  actionMil = addAction(":/littlenavmap/resources/icons/airportmil.svg", tr("&Military"), tr("Show military airports"), QKeySequence());

  ui->menuViewAirport->addSeparator();
  toolButton->menu()->addSeparator();

  actionGroupAddon = new QActionGroup(buttonMenu);
  actionAddonNone = addAction(":/littlenavmap/resources/icons/airportaddonnone.svg", tr("&Add-on no override"),
                              tr("Add-on airports are shown like normal airports"), QKeySequence(tr("Ctrl+Alt+O")));
  actionAddonNone->setActionGroup(actionGroupAddon);

  actionAddonZoom = addAction(":/littlenavmap/resources/icons/airportaddonzoom.svg", tr("Add-on override &zoom"),
                              tr("Add-on airports override zoom distance only"), QKeySequence(tr("Ctrl+Alt+Z")));
  actionAddonZoom->setActionGroup(actionGroupAddon);

  actionAddonZoomFilter = addAction(":/littlenavmap/resources/icons/airportaddon.svg", tr("Add-on &override zoom and filter"),
                                    tr("Add-on airports override zoom distance and filters"), QKeySequence(tr("Ctrl+Alt+Y")));
  actionAddonZoomFilter->setActionGroup(actionGroupAddon);

  // Create and add the wrapped actions ================
  buttonMenu->addSeparator();
  labelActionRunwayLength = new apinternal::AirportLabelAction(toolButton->menu());
  toolButton->menu()->addAction(labelActionRunwayLength);

  sliderActionRunwayLengthMin = new apinternal::AirportSliderAction(toolButton->menu(), true /* typeMinSlider */);
  toolButton->menu()->addAction(sliderActionRunwayLengthMin);

  sliderActionRunwayLengthMax = new apinternal::AirportSliderAction(toolButton->menu(), false /* typeMinSlider */);
  toolButton->menu()->addAction(sliderActionRunwayLengthMax);

  // All that are to be disabled if airport master button is off
  allActions.append({
    actionHard, actionSoft, actionEmpty, actionAddonNone, actionAddonZoom, actionAddonZoomFilter,
    actionUnlighted, actionNoProcedures, actionClosed, actionMil, actionWater, actionHelipad,
    sliderActionRunwayLengthMin, sliderActionRunwayLengthMax});

  connect(sliderActionRunwayLengthMin, &apinternal::AirportSliderAction::valueChanged, this, &MapAirportHandler::runwaySliderValueChanged);
  connect(sliderActionRunwayLengthMin, &apinternal::AirportSliderAction::sliderReleased, this, &MapAirportHandler::runwaySliderReleased);
  connect(sliderActionRunwayLengthMax, &apinternal::AirportSliderAction::valueChanged, this, &MapAirportHandler::runwaySliderValueChanged);
  connect(sliderActionRunwayLengthMax, &apinternal::AirportSliderAction::sliderReleased, this, &MapAirportHandler::runwaySliderReleased);
}

QAction *MapAirportHandler::addAction(const QString& icon, const QString& text, const QString& tooltip, const QKeySequence& shortcut)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QAction *action = new QAction(QIcon(icon), text, toolButton->menu());
  action->setToolTip(tooltip);
  action->setStatusTip(tooltip);
  action->setCheckable(true);
  action->setShortcut(shortcut);

  // Add to button and menu
  toolButton->menu()->addAction(action);
  ui->menuViewAirport->addAction(action);

  connect(action, &QAction::triggered, this, &MapAirportHandler::toolbarActionTriggered);

  return action;
}

void MapAirportHandler::actionOnlyAddonTriggered()
{
  // Save and restore master airport flag
  bool airportFlag = airportTypes.testFlag(map::AIRPORT);
  airportTypes = map::AIRPORT_ADDON_ZOOM_AND_FILTER;
  airportTypes.setFlag(map::AIRPORT, airportFlag);

  flagsToActions();
  updateButtons();
  emit updateAirportTypes();

  // Update label after emit to catch add-on override changes
  updateRunwayLabel();
}

void MapAirportHandler::actionResetTriggered()
{
  // Save and restore master airport flag
  bool airportFlag = airportTypes.testFlag(map::AIRPORT);
  airportTypes = map::AIRPORT_DEFAULT;
  airportTypes.setFlag(map::AIRPORT, airportFlag);

  flagsToActions();
  sliderActionRunwayLengthMin->reset();
  sliderActionRunwayLengthMax->reset();
  runwaySliderValueChanged();
  updateButtons();
  emit updateAirportTypes();

  // Update label after emit to catch add-on override changes
  updateRunwayLabel();
}

void MapAirportHandler::toolbarActionTriggered()
{
  actionsToFlags();
  updateButtons();
  emit updateAirportTypes();

  // Update label after emit to catch add-on override changes
  updateRunwayLabel();
}

void MapAirportHandler::flagsToActions()
{
  // Do not block members of group to allow status change
  atools::gui::SignalBlocker blocker({NavApp::getMainUi()->actionMapShowAirports, actionHard, actionSoft, actionWater, actionHelipad,
                                      actionUnlighted, actionNoProcedures, actionClosed, actionMil, actionEmpty});

  NavApp::getMainUi()->actionMapShowAirports->setChecked(airportTypes.testFlag(map::AIRPORT));
  actionHard->setChecked(airportTypes.testFlag(map::AIRPORT_HARD));
  actionSoft->setChecked(airportTypes.testFlag(map::AIRPORT_SOFT));
  actionWater->setChecked(airportTypes.testFlag(map::AIRPORT_WATER));
  actionHelipad->setChecked(airportTypes.testFlag(map::AIRPORT_HELIPAD));
  actionUnlighted->setChecked(airportTypes.testFlag(map::AIRPORT_UNLIGHTED));
  actionNoProcedures->setChecked(airportTypes.testFlag(map::AIRPORT_NO_PROCS));
  actionClosed->setChecked(airportTypes.testFlag(map::AIRPORT_CLOSED));
  actionMil->setChecked(airportTypes.testFlag(map::AIRPORT_MILITARY));
  actionEmpty->setChecked(airportTypes.testFlag(map::AIRPORT_EMPTY));

  // Signals not blocked to allow mutual exclusive group
  if(airportTypes.testFlag(map::AIRPORT_ADDON_ZOOM))
    actionAddonZoom->setChecked(true);
  else if(airportTypes.testFlag(map::AIRPORT_ADDON_ZOOM_AND_FILTER))
    actionAddonZoomFilter->setChecked(true);
  else
    actionAddonNone->setChecked(true);
}

void MapAirportHandler::actionsToFlags()
{
  airportTypes = map::NONE;

  airportTypes.setFlag(map::AIRPORT, NavApp::getMainUi()->actionMapShowAirports->isChecked());
  airportTypes.setFlag(map::AIRPORT_HARD, actionHard->isChecked());
  airportTypes.setFlag(map::AIRPORT_SOFT, actionSoft->isChecked());
  airportTypes.setFlag(map::AIRPORT_WATER, actionWater->isChecked());
  airportTypes.setFlag(map::AIRPORT_HELIPAD, actionHelipad->isChecked());
  airportTypes.setFlag(map::AIRPORT_UNLIGHTED, actionUnlighted->isChecked());
  airportTypes.setFlag(map::AIRPORT_NO_PROCS, actionNoProcedures->isChecked());
  airportTypes.setFlag(map::AIRPORT_CLOSED, actionClosed->isChecked());
  airportTypes.setFlag(map::AIRPORT_MILITARY, actionMil->isChecked());
  airportTypes.setFlag(map::AIRPORT_EMPTY, actionEmpty->isChecked());
  airportTypes.setFlag(map::AIRPORT_ADDON_ZOOM, actionAddonZoom->isChecked());
  airportTypes.setFlag(map::AIRPORT_ADDON_ZOOM_AND_FILTER, actionAddonZoomFilter->isChecked());
}

void MapAirportHandler::runwaySliderValueChanged()
{
  sliderActionRunwayLengthMin->setLimit(sliderActionRunwayLengthMax->getSliderValueReal());
  sliderActionRunwayLengthMax->setLimit(sliderActionRunwayLengthMin->getSliderValueReal());

  updateButtons();
  emit updateAirportTypes();

  // Update label after emit to catch add-on override changes
  updateRunwayLabel();
}

void MapAirportHandler::runwaySliderReleased()
{
  updateButtons();
  emit updateAirportTypes();

  // Update label after emit to catch add-on override changes
  updateRunwayLabel();
}

void MapAirportHandler::updateButtons()
{
  bool mainChecked = NavApp::getMainUi()->actionMapShowAirports->isChecked();
  toolButton->setEnabled(mainChecked);

  // Depress tool button if different from default
  bool noDefault = isMinimumRunwaySet() || isMaximumRunwaySet() || !airportTypes.testFlag(map::MapTypes(map::AIRPORT_DEFAULT));
  toolButton->setChecked(noDefault);

  // Reset and addon action
  if(actionReset->isEnabled() != noDefault && mainChecked)
    actionReset->setEnabled(noDefault && mainChecked);

  actionAddonOnly->setEnabled(airportTypes != (map::AIRPORT | map::AIRPORT_ADDON_ZOOM_AND_FILTER) && mainChecked);

  // Disable all other depending on main state
  for(QAction *action : std::as_const(allActions))
    action->setEnabled(mainChecked);
}

QString MapAirportHandler::getRunwayText() const
{
  int runwayLengthMin = getMinimumRunwayFt();
  int runwayLengthMax = getMaximumRunwayFt();

  bool addonOverride = NavApp::getShownMapTypes().testFlag(map::AIRPORT_ADDON_ZOOM_AND_FILTER);

  QString text;

  if(isMinimumRunwaySet() && isMaximumRunwaySet())
  {
    if(runwayLengthMin == runwayLengthMax)
    {
      if(addonOverride)
        text = tr("Only add-on airports.");
      else
        text = tr("No airports.");
    }
    else
      text = tr("Runway length between %1 and %2.").arg(Unit::distShortFeet(runwayLengthMin)).arg(Unit::distShortFeet(runwayLengthMax));
  }
  else if(isMinimumRunwaySet())
    text = tr("Minimum runway length %1.").arg(Unit::distShortFeet(runwayLengthMin));
  else if(isMaximumRunwaySet())
  {
    if(runwayLengthMax == 0)
    {
      if(addonOverride)
        text = tr("Only heliports and add-on airports.");
      else
        text = tr("Only heliports.");

    }
    else
      text = tr("Maximum runway length %1.").arg(Unit::distShortFeet(runwayLengthMax));
  }
  else
    text = tr("No runway length limit. Set minimum and maximum below.");

  return text;
}

void MapAirportHandler::updateRunwayLabel()
{
  labelActionRunwayLength->setText(getRunwayText());
}
