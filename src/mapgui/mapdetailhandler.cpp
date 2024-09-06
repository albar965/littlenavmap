/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/mapdetailhandler.h"

#include "common/constants.h"
#include "app/navapp.h"
#include "options/optiondata.h"
#include "settings/settings.h"
#include "mapgui/maplayersettings.h"
#include "ui_mainwindow.h"
#include "gui/signalblocker.h"

#include <QWidgetAction>

namespace mdinternal {

DetailSliderAction::DetailSliderAction(QObject *parent, const QString& settingsKeyParam, int minimumValue, int maximumValue)
  : QWidgetAction(parent), minValue(minimumValue), maxValue(maximumValue), settingsKey(settingsKeyParam)
{
  sliderValue = minValue;
  setSliderValue(sliderValue);
}

int DetailSliderAction::getSliderValue() const
{
  return sliderValue;
}

void DetailSliderAction::saveState() const
{
  atools::settings::Settings::instance().setValue(settingsKey, sliderValue);
}

void DetailSliderAction::restoreState()
{
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_MAP_SETTINGS))
    sliderValue = atools::settings::Settings::instance().valueInt(settingsKey, MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL);
  else
    sliderValue = MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL;

  setSliderValue(sliderValue);
}

QWidget *DetailSliderAction::createWidget(QWidget *parent)
{
  QSlider *slider = new QSlider(Qt::Horizontal, parent);
  slider->setMinimum(minValue);
  slider->setMaximum(maxValue);
  slider->setTickPosition(QSlider::TicksBothSides);
  slider->setTickInterval(1);
  slider->setPageStep(1);
  slider->setSingleStep(1);
  slider->setTracking(true);
  slider->setValue(sliderValue);
  slider->setToolTip(tr("Set detail level for map display (also \"Ctrl++\", \"Ctrl+-\" or \"Ctrl+Wheel\")."));

  connect(slider, &QSlider::valueChanged, this, &DetailSliderAction::setSliderValue);
  connect(slider, &QSlider::valueChanged, this, &DetailSliderAction::valueChanged);
  connect(slider, &QSlider::sliderReleased, this, &DetailSliderAction::sliderReleased);

  // Add to list (register)
  sliders.append(slider);
  return slider;
}

void DetailSliderAction::deleteWidget(QWidget *widget)
{
  QSlider *slider = dynamic_cast<QSlider *>(widget);
  if(slider != nullptr)
  {
    disconnect(slider, &QSlider::valueChanged, this, &DetailSliderAction::setSliderValue);
    disconnect(slider, &QSlider::valueChanged, this, &DetailSliderAction::valueChanged);
    disconnect(slider, &QSlider::sliderReleased, this, &DetailSliderAction::sliderReleased);
    sliders.removeAll(slider);
    delete widget;
  }
}

void DetailSliderAction::setSliderValue(int value)
{
  sliderValue = value;
  atools::gui::SignalBlocker blocker(sliders);
  for(QSlider *slider : qAsConst(sliders))
    slider->setValue(value);
}

void DetailSliderAction::reset()
{
  sliderValue = MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL;
  setSliderValue(sliderValue);
}

// =======================================================================================

/*
 * Wrapper for label action.
 */
class DetailLabelAction
  : public QWidgetAction
{
public:
  DetailLabelAction(QObject *parent) : QWidgetAction(parent)
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

void DetailLabelAction::setText(const QString& textParam)
{
  text = textParam;
  // Set text to all registered labels
  for(QLabel *label : qAsConst(labels))
    label->setText(text);
}

QWidget *DetailLabelAction::createWidget(QWidget *parent)
{
  QLabel *label = new QLabel(parent);
  label->setMargin(4);
  label->setText(text);
  labels.append(label);
  return label;
}

void DetailLabelAction::deleteWidget(QWidget *widget)
{
  labels.removeAll(dynamic_cast<QLabel *>(widget));
  delete widget;
}

} // namespace internal

// =======================================================================================

MapDetailHandler::MapDetailHandler(QWidget *parent)
  : QObject(parent)
{

}

MapDetailHandler::~MapDetailHandler()
{
  delete toolButton;
}

void MapDetailHandler::saveState() const
{
  sliderActionDetailLevel->saveState();
  sliderActionDetailLevelText->saveState();
}

void MapDetailHandler::restoreState()
{
  sliderActionDetailLevel->restoreState();
  sliderActionDetailLevelText->restoreState();

  detailSliderChanged();
  updateActions();
}

int MapDetailHandler::getDetailLevel() const
{
  return sliderActionDetailLevel->getSliderValue();
}

int MapDetailHandler::getDetailLevelText() const
{
  return sliderActionDetailLevelText->getSliderValue();
}

void MapDetailHandler::setDetailLevel(int level)
{
  sliderActionDetailLevel->setSliderValue(level);
  detailSliderChanged();
}

void MapDetailHandler::setDetailLevelText(int level)
{
  sliderActionDetailLevelText->setSliderValue(level);
  detailSliderChanged();
}

void MapDetailHandler::insertToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  toolButton = new QToolButton(ui->toolBarMap);

  // Create and add toolbar button =====================================
  toolButton->setIcon(QIcon(":/littlenavmap/resources/icons/detailmap.svg"));
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolButton->setToolTip(tr("Select map detail level.\n"
                            "Button is highlighted if detail level is not default."));
  toolButton->setStatusTip(toolButton->toolTip());
  toolButton->setCheckable(true);

  // Add tear off menu to button =======
  toolButton->setMenu(new QMenu(toolButton));
  QMenu *buttonMenu = toolButton->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  // Replace dummy action with this widget
  ui->toolBarMap->insertWidget(ui->actionDetailHandlerDummy, toolButton);
  ui->toolBarMap->removeAction(ui->actionDetailHandlerDummy);

  // Create and add actions to toolbar and menu =================================
  buttonMenu->addAction(ui->actionMapDetailsDefault);
  buttonMenu->addSeparator();

  // Create and add the wrapped actions ================
  labelActionDetailLevel = new mdinternal::DetailLabelAction(toolButton->menu());
  toolButton->menu()->addAction(labelActionDetailLevel);
  sliderActionDetailLevel = new mdinternal::DetailSliderAction(toolButton->menu(), lnm::MAP_DETAIL_LEVEL,
                                                               MapLayerSettings::MAP_MIN_DETAIL_LEVEL,
                                                               MapLayerSettings::MAP_MAX_DETAIL_LEVEL);
  toolButton->menu()->addAction(sliderActionDetailLevel);

  connect(sliderActionDetailLevel, &mdinternal::DetailSliderAction::valueChanged, this, &MapDetailHandler::detailSliderChanged);
  connect(sliderActionDetailLevel, &mdinternal::DetailSliderAction::sliderReleased, this, &MapDetailHandler::detailSliderChanged);

  labelActionDetailLevelText = new mdinternal::DetailLabelAction(toolButton->menu());
  toolButton->menu()->addAction(labelActionDetailLevelText);
  sliderActionDetailLevelText = new mdinternal::DetailSliderAction(toolButton->menu(), lnm::MAP_DETAIL_LEVEL_TEXT,
                                                                   MapLayerSettings::MAP_MIN_DETAIL_LEVEL_TEXT,
                                                                   MapLayerSettings::MAP_MAX_DETAIL_LEVEL_TEXT);

  toolButton->menu()->addAction(sliderActionDetailLevelText);

  connect(sliderActionDetailLevelText, &mdinternal::DetailSliderAction::valueChanged, this, &MapDetailHandler::detailSliderChanged);
  connect(sliderActionDetailLevelText, &mdinternal::DetailSliderAction::sliderReleased, this, &MapDetailHandler::detailSliderChanged);
}

void MapDetailHandler::detailSliderChanged()
{
  updateActions();
  emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
}

void MapDetailHandler::updateActions()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  int level = getDetailLevel();
  int levelText = getDetailLevelText();
  bool anyChanged = level != MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL || levelText != MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL;

  toolButton->setChecked(anyChanged);

  // Update menu actions
  ui->actionMapDetailsDefault->setEnabled(anyChanged);
  ui->actionMapDetailsMore->setEnabled(level < MapLayerSettings::MAP_MAX_DETAIL_LEVEL);
  ui->actionMapDetailsLess->setEnabled(level > MapLayerSettings::MAP_MIN_DETAIL_LEVEL);
  ui->actionMapDetailsTextMore->setEnabled(levelText < MapLayerSettings::MAP_MAX_DETAIL_LEVEL_TEXT);
  ui->actionMapDetailsTextLess->setEnabled(levelText > MapLayerSettings::MAP_MIN_DETAIL_LEVEL_TEXT);

  // Update label text
  QString text;
  if(level == MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("Normal map detail level");
  else if(level == MapLayerSettings::MAP_MIN_DETAIL_LEVEL)
    text = tr("Minimum map detail level %1").arg(level);
  else if(level == MapLayerSettings::MAP_MAX_DETAIL_LEVEL)
    text = tr("Maximum map detail level %1").arg(level);
  else if(level < MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("Lower map detail level %1").arg(level);
  else if(level > MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("Higher map detail level %1").arg(level);
  text += tr(" (Ctrl+Mouse Wheel)");
  labelActionDetailLevel->setText(text);

  if(levelText == MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("Normal map labels");
  else if(levelText == MapLayerSettings::MAP_MIN_DETAIL_LEVEL_TEXT)
    text = tr("Minimum map labels %1").arg(levelText);
  else if(levelText == MapLayerSettings::MAP_MAX_DETAIL_LEVEL_TEXT)
    text = tr("Maximum map labels %1").arg(levelText);
  else if(levelText < MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("Less map labels %1").arg(levelText);
  else if(levelText > MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
    text = tr("More map labels %1").arg(levelText);

  text += tr(" (Ctrl+Shift+Mouse Wheel)");
  labelActionDetailLevelText->setText(text);
}

void MapDetailHandler::defaultMapDetail()
{
  int curLevel = getDetailLevel();
  int curLevelText = getDetailLevelText();
  if(curLevel != MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL || curLevelText != MapLayerSettings::MAP_DEFAULT_DETAIL_LEVEL)
  {
    sliderActionDetailLevel->reset();
    sliderActionDetailLevelText->reset();
    updateActions();
    emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
  }
}

void MapDetailHandler::increaseMapDetailText()
{
  int curLevel = getDetailLevelText();
  if(curLevel < MapLayerSettings::MAP_MAX_DETAIL_LEVEL_TEXT)
  {
    sliderActionDetailLevelText->setSliderValue(curLevel + 1);
    updateActions();
    emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
  }
}

void MapDetailHandler::decreaseMapDetailText()
{
  int curLevel = getDetailLevelText();
  if(curLevel > MapLayerSettings::MAP_MIN_DETAIL_LEVEL_TEXT)
  {
    sliderActionDetailLevelText->setSliderValue(curLevel - 1);
    updateActions();
    emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
  }
}

void MapDetailHandler::increaseMapDetail()
{
  int curLevel = getDetailLevel();
  if(curLevel < MapLayerSettings::MAP_MAX_DETAIL_LEVEL)
  {
    sliderActionDetailLevel->setSliderValue(curLevel + 1);
    updateActions();
    emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
  }
}

void MapDetailHandler::decreaseMapDetail()
{
  int curLevel = getDetailLevel();
  if(curLevel > MapLayerSettings::MAP_MIN_DETAIL_LEVEL)
  {
    sliderActionDetailLevel->setSliderValue(curLevel - 1);
    updateActions();
    emit updateDetailLevel(getDetailLevel(), getDetailLevelText());
  }
}
