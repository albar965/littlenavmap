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
#include <QDebug>
#include <QStringBuilder>

static const int MIN_SLIDER_ALL_FT = 0;
static const int MAX_SLIDER_FT = 140;

static const int MIN_SLIDER_ALL_METER = 0;
static const int MAX_SLIDER_METER = 50;

namespace apinternal {

AirportSliderAction::AirportSliderAction(QObject *parent) : QWidgetAction(parent)
{
  sliderValue = minValue();
  setValue(sliderValue);
}

int AirportSliderAction::getSliderValue() const
{
  // -1 is unlimited
  return sliderValue == minValue() ? -1 : sliderValue;
}

void AirportSliderAction::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::MAP_AIRPORT_RUNWAY_LENGTH, sliderValue);
}

void AirportSliderAction::restoreState()
{
  sliderValue = atools::settings::Settings::instance().valueInt(lnm::MAP_AIRPORT_RUNWAY_LENGTH, minValue());
  setValue(sliderValue);
  sliderDistUnit = Unit::getUnitShortDist();
}

void AirportSliderAction::optionsChanged()
{
  // Set all sliders to new range for given unit and reset to unlimited
  // Block signals to avoid recursion

  if(Unit::getUnitShortDist() != sliderDistUnit)
  {
    // Units have changed
    switch(sliderDistUnit)
    {
      case opts::DIST_SHORT_FT:
        // Old was ft. Convert to new local unit.
        sliderValue = atools::roundToInt(Unit::distShortFeetF(sliderValue));
        break;
      case opts::DIST_SHORT_METER:
        // Old was meter. Convert to new local unit.
        sliderValue = atools::roundToInt(Unit::distShortMeterF(sliderValue));
        break;
    }
    sliderDistUnit = Unit::getUnitShortDist();
    sliderValue = atools::minmax(minValue(), maxValue(), sliderValue);
  }

  atools::gui::SignalBlocker blocker(sliders);
  for(QSlider *slider : sliders)
  {
    slider->setValue(sliderValue);
    slider->setMinimum(minValue());
    slider->setMaximum(maxValue());
  }
}

QWidget *AirportSliderAction::createWidget(QWidget *parent)
{
  QSlider *slider = new QSlider(Qt::Horizontal, parent);
  slider->setMinimum(minValue());
  slider->setMaximum(maxValue());
  slider->setTickPosition(QSlider::TicksBothSides);
  slider->setTickInterval(10);
  slider->setPageStep(10);
  slider->setSingleStep(10);
  slider->setTracking(true);
  slider->setValue(sliderValue);
  slider->setToolTip(tr("Set minimum runway length for airports to display.\n"
                        "Runway length might be also affected by zoom distance."));

  connect(slider, &QSlider::valueChanged, this, &AirportSliderAction::sliderValueChanged);
  connect(slider, &QSlider::valueChanged, this, &AirportSliderAction::valueChanged);
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

void AirportSliderAction::sliderValueChanged(int value)
{
  sliderValue = value;
  setValue(value);
}

int AirportSliderAction::minValue() const
{
  switch(sliderDistUnit)
  {
    case opts::DIST_SHORT_FT:
      return MIN_SLIDER_ALL_FT;

    case opts::DIST_SHORT_METER:
      return MIN_SLIDER_ALL_METER;
  }
  return MIN_SLIDER_ALL_FT;
}

int AirportSliderAction::maxValue() const
{
  switch(sliderDistUnit)
  {
    case opts::DIST_SHORT_FT:
      return MAX_SLIDER_FT;

    case opts::DIST_SHORT_METER:
      return MAX_SLIDER_METER;
  }
  return MAX_SLIDER_FT;
}

void AirportSliderAction::setValue(int value)
{
  for(QSlider *slider : sliders)
  {
    slider->blockSignals(true);
    slider->setValue(value);
    slider->blockSignals(false);
  }
}

void AirportSliderAction::reset()
{
  sliderValue = minValue();
  setValue(sliderValue);
}

// =======================================================================================

/*
 * Wrapper for label action.
 */
class AirportLabelAction
  : public QWidgetAction
{
public:
  AirportLabelAction(QObject *parent) : QWidgetAction(parent)
  {
  }

  void setText(const QString& textParam);

protected:
  /* Create a delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* List of created/registered labels */
  QVector<QLabel *> labels;
  QString text;
};

void AirportLabelAction::setText(const QString& textParam)
{
  text = textParam;
  // Set text to all registered labels
  for(QLabel *label : labels)
    label->setText(text);
}

QWidget *AirportLabelAction::createWidget(QWidget *parent)
{
  QLabel *label = new QLabel(parent);
  label->setMargin(4);
  label->setText(text);
  labels.append(label);
  return label;
}

void AirportLabelAction::deleteWidget(QWidget *widget)
{
  labels.removeAll(dynamic_cast<QLabel *>(widget));
  delete widget;
}

} // namespace internal

// =======================================================================================

MapAirportHandler::MapAirportHandler(QWidget *parent)
  : QObject(parent)
{

}

MapAirportHandler::~MapAirportHandler()
{
  delete toolButton;
}

void MapAirportHandler::saveState()
{
  sliderActionRunwayLength->saveState();
  actionsToFlags();
  atools::settings::Settings::instance().setValueVar(lnm::MAP_AIRPORT, airportTypes.asFlagType());
}

void MapAirportHandler::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    QVariant defaultValue = static_cast<atools::util::FlagType>(map::AIRPORT_ALL_AND_ADDON);
    airportTypes = atools::settings::Settings::instance().valueVar(lnm::MAP_AIRPORT, defaultValue).value<atools::util::FlagType>();
    sliderActionRunwayLength->restoreState();
  }
  actionEmpty->setEnabled(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS);

  runwaySliderValueChanged();
  flagsToActions();
  updateToolbutton();
}

int MapAirportHandler::getMinimumRunwayFt() const
{
  if(sliderActionRunwayLength->getSliderValue() == -1)
    return -1;
  else
    return atools::roundToInt(Unit::rev(sliderActionRunwayLength->getSliderValue() * 100.f, Unit::distShortFeetF));
}

void MapAirportHandler::resetSettingsToDefault()
{
  airportTypes = map::AIRPORT_ALL_AND_ADDON;
  sliderActionRunwayLength->reset();
  flagsToActions();
  runwaySliderValueChanged();
}

void MapAirportHandler::optionsChanged()
{
  actionEmpty->setEnabled(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS);
  sliderActionRunwayLength->optionsChanged();
  runwaySliderValueChanged();
}

void MapAirportHandler::addToolbarButton()
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

  ui->toolBarMapOptions->insertWidget(ui->actionMapShowVor, toolButton);
  ui->toolBarMapOptions->insertSeparator(ui->actionMapShowVor);

  // Create and add actions to toolbar and menu =================================
  actionReset = new QAction(tr("&Reset airport display options"), buttonMenu);
  actionReset->setIcon(QIcon(":/littlenavmap/resources/icons/clear.svg"));
  actionReset->setToolTip(tr("Reset to default and show all airport types"));
  actionReset->setStatusTip(actionReset->toolTip());
  buttonMenu->addAction(actionReset);
  ui->menuViewAirport->addAction(actionReset);
  connect(actionReset, &QAction::triggered, this, &MapAirportHandler::actionResetTriggered);

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

  actionAddon = addAction(":/littlenavmap/resources/icons/airportaddon.svg", tr("&Add-on"),
                          tr("Force visibility of add-on airports for all zoom distances"), QKeySequence(tr("Ctrl+Alt+O")));

  // Create and add the wrapped actions ================
  buttonMenu->addSeparator();
  labelActionRunwayLength = new apinternal::AirportLabelAction(toolButton->menu());
  toolButton->menu()->addAction(labelActionRunwayLength);
  sliderActionRunwayLength = new apinternal::AirportSliderAction(toolButton->menu());
  toolButton->menu()->addAction(sliderActionRunwayLength);

  connect(sliderActionRunwayLength, &apinternal::AirportSliderAction::valueChanged, this, &MapAirportHandler::runwaySliderValueChanged);
  connect(sliderActionRunwayLength, &apinternal::AirportSliderAction::sliderReleased, this, &MapAirportHandler::runwaySliderReleased);
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

void MapAirportHandler::actionResetTriggered()
{
  // Save and restore master airport flag
  bool airportFlag = airportTypes.testFlag(map::AIRPORT);
  airportTypes = map::AIRPORT_ALL_AND_ADDON;
  airportTypes.setFlag(map::AIRPORT, airportFlag);

  flagsToActions();
  sliderActionRunwayLength->reset();
  runwaySliderValueChanged();
  updateToolbutton();
  emit updateAirportTypes();
}

void MapAirportHandler::toolbarActionTriggered()
{
  actionsToFlags();
  updateToolbutton();
  emit updateAirportTypes();
}

void MapAirportHandler::flagsToActions()
{
  atools::gui::SignalBlocker blocker({NavApp::getMainUi()->actionMapShowAirports, actionHard, actionSoft, actionWater, actionHelipad,
                                      actionAddon, actionUnlighted, actionNoProcedures, actionClosed, actionMil, actionEmpty});

  NavApp::getMainUi()->actionMapShowAirports->setChecked(airportTypes.testFlag(map::AIRPORT));
  actionHard->setChecked(airportTypes.testFlag(map::AIRPORT_HARD));
  actionSoft->setChecked(airportTypes.testFlag(map::AIRPORT_SOFT));
  actionWater->setChecked(airportTypes.testFlag(map::AIRPORT_WATER));
  actionHelipad->setChecked(airportTypes.testFlag(map::AIRPORT_HELIPAD));
  actionAddon->setChecked(airportTypes.testFlag(map::AIRPORT_ADDON));
  actionUnlighted->setChecked(airportTypes.testFlag(map::AIRPORT_UNLIGHTED));
  actionNoProcedures->setChecked(airportTypes.testFlag(map::AIRPORT_NO_PROCS));
  actionClosed->setChecked(airportTypes.testFlag(map::AIRPORT_CLOSED));
  actionMil->setChecked(airportTypes.testFlag(map::AIRPORT_MILITARY));
  actionEmpty->setChecked(airportTypes.testFlag(map::AIRPORT_EMPTY));
}

void MapAirportHandler::actionsToFlags()
{
  airportTypes = map::NONE;

  if(NavApp::getMainUi()->actionMapShowAirports->isChecked())
    airportTypes |= map::AIRPORT;

  if(actionHard->isChecked())
    airportTypes |= map::AIRPORT_HARD;

  if(actionSoft->isChecked())
    airportTypes |= map::AIRPORT_SOFT;

  if(actionWater->isChecked())
    airportTypes |= map::AIRPORT_WATER;

  if(actionHelipad->isChecked())
    airportTypes |= map::AIRPORT_HELIPAD;

  if(actionAddon->isChecked())
    airportTypes |= map::AIRPORT_ADDON;

  if(actionUnlighted->isChecked())
    airportTypes |= map::AIRPORT_UNLIGHTED;

  if(actionNoProcedures->isChecked())
    airportTypes |= map::AIRPORT_NO_PROCS;

  if(actionClosed->isChecked())
    airportTypes |= map::AIRPORT_CLOSED;

  if(actionMil->isChecked())
    airportTypes |= map::AIRPORT_MILITARY;

  if(actionEmpty->isChecked())
    airportTypes |= map::AIRPORT_EMPTY;
}

void MapAirportHandler::runwaySliderValueChanged()
{
  updateToolbutton();
  updateRunwayLabel();
  emit updateAirportTypes();
}

void MapAirportHandler::runwaySliderReleased()
{
  updateToolbutton();
  emit updateAirportTypes();
}

void MapAirportHandler::updateToolbutton()
{
  bool noDefault = getMinimumRunwayFt() > 0 || !(airportTypes.testFlag(map::MapTypes(map::AIRPORT_FILTER_ALL)));
  toolButton->setChecked(noDefault);
  actionReset->setEnabled(noDefault);
}

void MapAirportHandler::updateRunwayLabel()
{
  int runwayLength = getMinimumRunwayFt();
  labelActionRunwayLength->setText(runwayLength > 0 ?
                                   tr("Minimum runway length %1.").arg(Unit::distShortFeet(runwayLength)) :
                                   tr("No runway length limit."));
}
