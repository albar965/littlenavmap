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

#include "airspace/airspacetoolbarhandler.h"

#include "common/maptypes.h"
#include "common/unit.h"
#include "navapp.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QToolButton>
#include <QDebug>

namespace asinternal {

AirspaceAltSliderAction::AirspaceAltSliderAction(QObject *parent, bool maxSliderParam)
  : QWidgetAction(parent), maxSlider(maxSliderParam)
{
  sliderValue = minValue();
  setSliderValue(sliderValue);
}

int AirspaceAltSliderAction::getAltitudeFt() const
{
  return (maxSlider ? maxValue() - sliderValue : sliderValue) * SLIDER_STEP_ALT_FT;
}

void AirspaceAltSliderAction::setAltitudeFt(int altitude)
{
  int value = altitude / SLIDER_STEP_ALT_FT;
  value = maxSlider ? maxValue() - value : value;
  setSliderValue(value);
}

QWidget *AirspaceAltSliderAction::createWidget(QWidget *parent)
{
  QSlider *slider = new QSlider(Qt::Horizontal, parent);
  slider->setToolTip(maxSlider ? tr("Maximum altitude for airspace display") : tr("Minimum altitude for airspace display"));
  slider->setStatusTip(slider->toolTip());
  slider->setMinimum(minValue());
  slider->setMaximum(maxValue());

  slider->setTickPosition(maxSlider ? QSlider::TicksBelow : QSlider::TicksAbove);
  slider->setInvertedAppearance(maxSlider);
  slider->setInvertedControls(maxSlider);
  slider->setTickInterval(5000 / SLIDER_STEP_ALT_FT);
  slider->setPageStep(1000 / SLIDER_STEP_ALT_FT);
  slider->setSingleStep(500 / SLIDER_STEP_ALT_FT);
  slider->setTracking(true);
  slider->setValue(sliderValue);

  connect(slider, &QSlider::valueChanged, this, &AirspaceAltSliderAction::setSliderValue);
  connect(slider, &QSlider::valueChanged, this, &AirspaceAltSliderAction::valueChanged);
  connect(slider, &QSlider::sliderReleased, this, &AirspaceAltSliderAction::sliderReleased);

  // Add to list (register)
  sliders.append(slider);
  return slider;
}

void AirspaceAltSliderAction::deleteWidget(QWidget *widget)
{
  QSlider *slider = dynamic_cast<QSlider *>(widget);
  if(slider != nullptr)
  {
    disconnect(slider, &QSlider::valueChanged, this, &AirspaceAltSliderAction::setSliderValue);
    disconnect(slider, &QSlider::valueChanged, this, &AirspaceAltSliderAction::valueChanged);
    disconnect(slider, &QSlider::sliderReleased, this, &AirspaceAltSliderAction::sliderReleased);
    sliders.removeAll(slider);
    delete widget;
  }
}

int AirspaceAltSliderAction::minValue() const
{
  return map::MapAirspaceFilter::MIN_AIRSPACE_ALT / SLIDER_STEP_ALT_FT;
}

int AirspaceAltSliderAction::maxValue() const
{
  return map::MapAirspaceFilter::MAX_AIRSPACE_ALT / SLIDER_STEP_ALT_FT;
}

void AirspaceAltSliderAction::setSliderValue(int value)
{
  sliderValue = value;
  for(QSlider *slider : sliders)
  {
    slider->blockSignals(true);
    slider->setValue(sliderValue);
    slider->blockSignals(false);
  }
}

// =======================================================================================

/*
 * Wrapper for label action. Does not send any signals. No Q_OBJECT needed.
 * Labels are disabled too if wrapper widget is disabled.
 */
class AirspaceLabelAction
  : public QWidgetAction
{
public:
  AirspaceLabelAction(QObject *parent) : QWidgetAction(parent)
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

void AirspaceLabelAction::setText(const QString& textParam)
{
  text = textParam;
  // Set text to all registered labels
  for(QLabel *label : labels)
    label->setText(text);
}

QWidget *AirspaceLabelAction::createWidget(QWidget *parent)
{
  QLabel *label = new QLabel(parent);
  label->setMargin(4);
  label->setText(text);
  label->setEnabled(isEnabled());
  labels.append(label);
  return label;
}

void AirspaceLabelAction::deleteWidget(QWidget *widget)
{
  labels.removeAll(dynamic_cast<QLabel *>(widget));
  delete widget;
}

} // namespace internal

// =======================================================================================

AirspaceToolBarHandler::AirspaceToolBarHandler(MainWindow *parent)
  : mainWindow(parent)
{
  connect(NavApp::getMainUi()->actionShowAirspaces, &QAction::toggled, this, &AirspaceToolBarHandler::allAirspacesToggled);
}

AirspaceToolBarHandler::~AirspaceToolBarHandler()
{

}

void AirspaceToolBarHandler::allAirspacesToggled()
{
  updateAll();
}

void AirspaceToolBarHandler::updateToolButtons()
{
  map::MapAirspaceFilter shown = NavApp::getShownMapAirspaces();

  QAction *airspaceAction = NavApp::getMainUi()->actionShowAirspaces;

  bool hasAirspaces = NavApp::hasAnyAirspaces();
  airspaceAction->setEnabled(hasAirspaces);

  // Enable or disable all tool buttons
  for(int i = 0; i < airspaceToolButtons.size(); i++)
  {
    if(airspaceToolGroups.at(i) == nullptr)
      // Depress the button if the state is not default
      airspaceToolButtons.at(i)->setChecked(airspaceToolButtonFilters.at(i).types & shown.types ||
                                            airspaceToolButtonFilters.at(i).flags & shown.flags);
    else
      // Depress button if the first is not selected in groups
      airspaceToolButtons.at(i)->setChecked(!airspaceToolGroups.at(i)->actions().constFirst()->isChecked());

    // Enable button if any airspace type is enabled
    airspaceToolButtons.at(i)->setEnabled((airspaceAction->isChecked() && hasAirspaces));
  }
}

void AirspaceToolBarHandler::updateToolActions()
{
  map::MapAirspaceFilter shown = NavApp::getShownMapAirspaces();

  for(QAction *action : airspaceActions)
  {
    map::MapAirspaceFilter filterFromAction = action->data().value<map::MapAirspaceFilter>();

    if(!(filterFromAction.flags & map::AIRSPACE_ALL_ON) && !(filterFromAction.flags & map::AIRSPACE_ALL_OFF))
    {
      action->blockSignals(true);
      action->setChecked(filterFromAction.types & shown.types || filterFromAction.flags & shown.flags);
      action->blockSignals(false);
    }
  }

  for(QAction *action : airspaceActions)
    action->setEnabled(NavApp::getMainUi()->actionShowAirspaces->isChecked());
}

void AirspaceToolBarHandler::updateSliders()
{
  bool show = NavApp::getShownMapAirspaces().flags.testFlag(map::AIRSPACE_ALTITUDE_SET);
  labelActionAirspace->setEnabled(show);
  sliderActionAltMax->setEnabled(show);
  sliderActionAltMin->setEnabled(show);
}

void AirspaceToolBarHandler::updateAll()
{
  // Copy currently selected altitude values to sliders
  map::MapAirspaceFilter filter = NavApp::getShownMapAirspaces();
  sliderActionAltMin->setAltitudeFt(filter.minAltitudeFt);
  sliderActionAltMax->setAltitudeFt(filter.maxAltitudeFt);

  /* Check or uncheck menu actions with blocked signal based on NavApp::getShownMapAirspaces() */
  updateToolActions();

  /* Update button depressed state or not */
  updateToolButtons();

  /* Enable or disable sliders based on NavApp::getShownMapAirspaces() */
  updateSliders();

  /* Update altitude label from slider values */
  updateSliderLabel();
}

void AirspaceToolBarHandler::actionAllNoneOrTypeTriggered()
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
    updateAll();
  }
}

void AirspaceToolBarHandler::actionRadioGroupTriggered(QAction *action)
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
  updateToolButtons();
  updateSliders();
  updateSliderLabel();
}

void AirspaceToolBarHandler::updateSliderLabel()
{
  int min = sliderActionAltMin->getAltitudeFt();
  int max = sliderActionAltMax->getAltitudeFt();

  map::MapAirspaceFilter filter = NavApp::getShownMapAirspaces();
  QString disabledText;

  // Get note text if slider is disabled =============
  if(filter.flags.testFlag(map::AIRSPACE_ALTITUDE_FLIGHTPLAN))
    disabledText = tr(" (showing for flight plan)");
  else if(filter.flags.testFlag(map::AIRSPACE_ALTITUDE_ALL))
    disabledText = tr(" (showing for all)");

  if(min == max)
    labelActionAirspace->setText(tr("At %1%2").arg(Unit::altFeet(min)).arg(disabledText));
  else
  {
    QString maxText;
    bool addUnit = false;
    if(max == map::MapAirspaceFilter::MAX_AIRSPACE_ALT)
    {
      // Treat the maximum value as unlimited
      maxText = tr("unlimited");
      addUnit = true;
    }
    else
      maxText = Unit::altFeet(max);

    labelActionAirspace->setText(tr("From %1 to %2%3").arg(Unit::altFeet(min, addUnit)).arg(maxText).arg(disabledText));
  }
}

void AirspaceToolBarHandler::altSliderChanged()
{
  asinternal::AirspaceAltSliderAction *slider = dynamic_cast<asinternal::AirspaceAltSliderAction *>(sender());
  if(slider != nullptr)
  {
    int min = sliderActionAltMin->getAltitudeFt();
    int max = sliderActionAltMax->getAltitudeFt();

    if(slider->isMaxSlider())
    {
      if(max < min)
      {
        sliderActionAltMax->setAltitudeFt(min);
        max = min;
      }
    }
    else
    {
      if(min > max)
      {
        sliderActionAltMin->setAltitudeFt(max);
        min = max;
      }
    }

    updateSliderLabel();

    map::MapAirspaceFilter shown = NavApp::getShownMapAirspaces();
    shown.minAltitudeFt = min;
    shown.maxAltitudeFt = max;
    shown.flags.setFlag(map::AIRSPACE_ALTITUDE_ALL, false);
    shown.flags.setFlag(map::AIRSPACE_ALTITUDE_FLIGHTPLAN, false);
    shown.flags.setFlag(map::AIRSPACE_ALTITUDE_SET, true);
    emit updateAirspaceTypes(shown);
    updateSliderLabel();
  }
}

void AirspaceToolBarHandler::createToolButtons()
{
  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceicao.svg",
                           tr("Select ICAO airspaces"),
                           {map::CLASS_A, map::CLASS_B, map::CLASS_C, map::CLASS_D, map::CLASS_E,
                            map::CLASS_F, map::CLASS_G}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacefir.svg",
                           tr("Select FIR airspaces"),
                           {map::FIR, map::UIR}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacerestr.svg",
                           tr("Select MOA, restricted, prohibited and danger airspaces"),
                           {map::MOA, map::RESTRICTED, map::PROHIBITED, map::GLIDERPROHIBITED, map::DANGER}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacespec.svg",
                           tr("Select warning, alert and training airspaces"),
                           {map::WARNING, map::CAUTION, map::ALERT, map::TRAINING}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspaceother.svg",
                           tr("Select centers and other airspaces"),
                           {map::CENTER, map::TOWER, map::GCA, map::MCTR, map::TRSA, map::CLEARANCE, map::GROUND,
                            map::DEPARTURE, map::APPROACH, map::NATIONAL_PARK, map::MODEC, map::RADAR, map::WAVEWINDOW,
                            map::ONLINE_OBSERVER}, {});

  createAirspaceToolButton(":/littlenavmap/resources/icons/airspacealt.svg", tr("Select altitude limitations for airspace display"),
                           {}, {map::AIRSPACE_ALTITUDE_ALL, map::AIRSPACE_ALTITUDE_FLIGHTPLAN, map::AIRSPACE_ALTITUDE_SET},
                           true /* groupActions */, true /* minMaxAltitude */);
}

void AirspaceToolBarHandler::createAirspaceToolButton(const QString& icon, const QString& buttonHelp,
                                                      const std::initializer_list<map::MapAirspaceTypes>& types,
                                                      const std::initializer_list<map::MapAirspaceFlags>& flags,
                                                      bool groupActions, bool minMaxAltitude)
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
  button->setToolTip(buttonHelp);
  button->setStatusTip(buttonHelp);
  button->setCheckable(true);
  airspaceToolButtons.append(button);

  // Add tear off menu =======
  button->setMenu(new QMenu(button));
  QMenu *buttonMenu = button->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  map::MapAirspaceFilter filter;
  filter.types = allTypes;
  filter.flags = allFlags;
  airspaceToolButtonFilters.append(filter);

  // Add all on / all off menu items ======================================
  if(!groupActions)
  {
    QAction *action = new QAction(tr("&All"), buttonMenu);
    action->setToolTip(tr("Show all airspaces in this category"));
    action->setStatusTip(action->toolTip());
    map::MapAirspaceFilter filterOn;
    filterOn.types = allTypes;
    filterOn.flags = map::AIRSPACE_ALL_ON | allFlags;
    action->setData(QVariant::fromValue(filterOn));
    buttonMenu->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionAllNoneOrTypeTriggered);

    action = new QAction(tr("&None"), buttonMenu);
    action->setToolTip(tr("Hide all airspaces in this category"));
    action->setStatusTip(action->toolTip());
    map::MapAirspaceFilter filterOff;
    filterOff.types = allTypes;
    filterOff.flags = map::AIRSPACE_ALL_OFF | allFlags;
    action->setData(QVariant::fromValue(filterOff));
    buttonMenu->addAction(action);
    airspaceActions.append(action);
    connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionAllNoneOrTypeTriggered);

    buttonMenu->addSeparator();
  }

  QActionGroup *group = nullptr;
  QObject *parent = buttonMenu;

  if(groupActions)
  {
    // Add radio button group (altitude actions) ======================================
    group = new QActionGroup(buttonMenu);
    connect(group, &QActionGroup::triggered, this, &AirspaceToolBarHandler::actionRadioGroupTriggered);
    parent = group;
    airspaceToolGroups.append(group);

    // Add radio button items based on flags
    for(const map::MapAirspaceFlags& flag: flags)
    {
      QAction *action = new QAction(map::airspaceFlagToString(flag), parent);
      action->setToolTip(map::airspaceFlagToStringLong(flag));
      action->setStatusTip(action->toolTip());
      action->setCheckable(true);

      map::MapAirspaceFilter f;
      f.types = map::AIRSPACE_NONE;
      f.flags = flag;

      action->setData(QVariant::fromValue(f));
      buttonMenu->addAction(action);
      airspaceActions.append(action);
      ui->menuViewAirspaces->addAction(action);
    }
  }
  else
  {
    // Not grouped actions (airspace types) ======================================

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

      buttonMenu->addAction(action);
      airspaceActions.append(action);
      connect(action, &QAction::triggered, this, &AirspaceToolBarHandler::actionAllNoneOrTypeTriggered);
      ui->menuViewAirspaces->addAction(action);
    }
  }

  if(minMaxAltitude)
  {
    // Add label and sliders for min/max altitude selection ==============================================
    buttonMenu->addSeparator();

    // Create and add the wrapped actions ================
    labelActionAirspace = new asinternal::AirspaceLabelAction(buttonMenu);
    buttonMenu->addAction(labelActionAirspace);

    sliderActionAltMin = new asinternal::AirspaceAltSliderAction(buttonMenu, false /* maxSliderParam */);
    buttonMenu->addAction(sliderActionAltMin);

    connect(sliderActionAltMin, &asinternal::AirspaceAltSliderAction::valueChanged, this, &AirspaceToolBarHandler::altSliderChanged);
    connect(sliderActionAltMin, &asinternal::AirspaceAltSliderAction::sliderReleased, this, &AirspaceToolBarHandler::altSliderChanged);

    sliderActionAltMax = new asinternal::AirspaceAltSliderAction(buttonMenu, true /* maxSliderParam */);
    buttonMenu->addAction(sliderActionAltMax);

    connect(sliderActionAltMax, &asinternal::AirspaceAltSliderAction::valueChanged, this, &AirspaceToolBarHandler::altSliderChanged);
    connect(sliderActionAltMax, &asinternal::AirspaceAltSliderAction::sliderReleased, this, &AirspaceToolBarHandler::altSliderChanged);
  }

  ui->toolBarAirspaces->addWidget(button);
}
