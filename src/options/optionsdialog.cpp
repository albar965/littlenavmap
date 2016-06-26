/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "options/optionsdialog.h"

#include "common/constants.h"
#include "gui/mainwindow.h"
#include "ui_options.h"

#include <gui/widgetstate.h>

#include <QMessageBox>

OptionsDialog::OptionsDialog(MainWindow *parentWindow, OptionData *optionDataParam)
  : QDialog(parentWindow), ui(new Ui::Options), mainWindow(parentWindow), optionData(optionDataParam)
{
  ui->setupUi(this);

  widgets.append(ui->checkBoxOptionsGuiCenterKml);
  widgets.append(ui->checkBoxOptionsGuiCenterRoute);
  widgets.append(ui->checkBoxOptionsMapEmptyAirports);
  widgets.append(ui->checkBoxOptionsRouteEastWestRule);
  widgets.append(ui->checkBoxOptionsRoutePreferNdb);
  widgets.append(ui->checkBoxOptionsRoutePreferVor);
  widgets.append(ui->checkBoxOptionsStartupLoadKml);
  widgets.append(ui->checkBoxOptionsStartupLoadMapSettings);
  widgets.append(ui->checkBoxOptionsStartupLoadRoute);
  widgets.append(ui->checkBoxOptionsWeatherInfoAsn);
  widgets.append(ui->checkBoxOptionsWeatherInfoNoaa);
  widgets.append(ui->checkBoxOptionsWeatherInfoVatsim);
  widgets.append(ui->checkBoxOptionsWeatherTooltipAsn);
  widgets.append(ui->checkBoxOptionsWeatherTooltipNoaa);
  widgets.append(ui->checkBoxOptionsWeatherTooltipVatsim);
  widgets.append(ui->lineEditOptionsMapRangeRings);
  widgets.append(ui->lineEditOptionsWeatherAsnPath);
  widgets.append(ui->listWidgetOptionsDatabaseAddon);
  widgets.append(ui->listWidgetOptionsDatabaseExclude);
  widgets.append(ui->radioButtonOptionsMapScrollFull);
  widgets.append(ui->radioButtonOptionsMapScrollNone);
  widgets.append(ui->radioButtonOptionsMapScrollNormal);
  widgets.append(ui->radioButtonOptionsMapSimUpdateFast);
  widgets.append(ui->radioButtonOptionsMapSimUpdateLow);
  widgets.append(ui->radioButtonOptionsMapSimUpdateMedium);
  widgets.append(ui->radioButtonOptionsStartupShowHome);
  widgets.append(ui->radioButtonOptionsStartupShowLast);
  widgets.append(ui->spinBoxOptionsCacheDiskSize);
  widgets.append(ui->spinBoxOptionsCacheMemorySize);
  widgets.append(ui->spinBoxOptionsGuiInfoText);
  widgets.append(ui->spinBoxOptionsGuiRouteText);
  widgets.append(ui->spinBoxOptionsGuiSearchText);
  widgets.append(ui->spinBoxOptionsGuiSimInfoText);
  widgets.append(ui->spinBoxOptionsMapClickRect);
  widgets.append(ui->spinBoxOptionsMapSymbolSize);
  widgets.append(ui->spinBoxOptionsMapTextSize);
  widgets.append(ui->spinBoxOptionsMapTooltipRect);
  widgets.append(ui->doubleSpinBoxOptionsMapZoomShowMap);
  widgets.append(ui->spinBoxOptionsRouteGroundBuffer);

  rangeRingValidator.setRegularExpression(QRegularExpression("^([1-9]\\d{0,3} )*[1-9]\\d{1,3}$"));
  ui->lineEditOptionsMapRangeRings->setValidator(&rangeRingValidator);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &OptionsDialog::buttonBoxClicked);

  connect(ui->pushButtonOptionsStartupResetDefault, &QPushButton::clicked,
          this, &OptionsDialog::resetDefaultClicked);
  connect(ui->pushButtonOptionsWeatherAsnPathSelect, &QPushButton::clicked,
          this, &OptionsDialog::selectAsnPathClicked);

  connect(ui->pushButtonOptionsCacheClearDisk, &QPushButton::clicked,
          this, &OptionsDialog::clearDiskCachedClicked);
  connect(ui->pushButtonOptionsCacheClearMemory, &QPushButton::clicked,
          this, &OptionsDialog::clearMemCachedClicked);
}

OptionsDialog::~OptionsDialog()
{
  delete ui;
}

int OptionsDialog::exec()
{
  fromOptionData();
  return QDialog::exec();
}

void OptionsDialog::resetDefaultClicked()
{
  qDebug() << "OptionsDialog::resetDefaultClicked";

  QMessageBox::StandardButton result = QMessageBox::question(this, QApplication::applicationName(),
                                                             tr("Reset all options to default?"));

  if(result == QMessageBox::Yes)
  {
    *optionData = OptionData();
    fromOptionData();
    saveState();
    emit optionsChanged(optionData);
  }
}

void OptionsDialog::selectAsnPathClicked()
{
  qDebug() << "OptionsDialog::selectAsnPathClicked";
}

void OptionsDialog::clearMemCachedClicked()
{
  qDebug() << "OptionsDialog::clearMemCachedClicked";
}

void OptionsDialog::clearDiskCachedClicked()
{
  qDebug() << "OptionsDialog::clearDiskCachedClicked";
}

void OptionsDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "Clicked" << button->text();

  if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Apply))
  {
    toOptionData();
    saveState();
    emit optionsChanged(optionData);
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Ok))
  {
    toOptionData();
    saveState();
    emit optionsChanged(optionData);
    accept();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Cancel))
    reject();
}

void OptionsDialog::saveState()
{
  fromOptionData();

  atools::gui::WidgetState saver(lnm::OPTIONS_DIALOG_WIDGET, false, true);
  saver.save(widgets);
}

void OptionsDialog::restoreState()
{
  atools::gui::WidgetState saver(lnm::OPTIONS_DIALOG_WIDGET, false /*visibility*/, true /*block signals*/);
  saver.restore(widgets);

  toOptionData();
}

void OptionsDialog::toOptionData()
{
  toFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  toFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  toFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  toFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  toFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  toFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  toFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  toFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  toFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_EAST_WEST_RULE);
  toFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  toFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  toFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ASN);
  toFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  toFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  toFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ASN);
  toFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  toFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);

  optionData->mapRangeRings.clear();
  for(const QString& str :  ui->lineEditOptionsMapRangeRings->text().split(" "))
    optionData->mapRangeRings.append(str.toInt());

  optionData->weatherActiveSkyPath = ui->lineEditOptionsWeatherAsnPath->text();

  optionData->databaseAddonExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    optionData->databaseAddonExclude.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());

  optionData->databaseExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    optionData->databaseExclude.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());

  if(ui->radioButtonOptionsMapScrollFull->isChecked())
    optionData->mapScrollDetail = opts::FULL;
  else if(ui->radioButtonOptionsMapScrollNone->isChecked())
    optionData->mapScrollDetail = opts::NONE;
  else if(ui->radioButtonOptionsMapScrollNormal->isChecked())
    optionData->mapScrollDetail = opts::NORMAL;

  if(ui->radioButtonOptionsMapSimUpdateFast->isChecked())
    optionData->mapSimUpdateRate = opts::FAST;
  else if(ui->radioButtonOptionsMapSimUpdateLow->isChecked())
    optionData->mapSimUpdateRate = opts::LOW;
  else if(ui->radioButtonOptionsMapSimUpdateMedium->isChecked())
    optionData->mapSimUpdateRate = opts::MEDIUM;

  optionData->cacheSizeDisk = ui->spinBoxOptionsCacheDiskSize->value();
  optionData->cacheSizeMemory = ui->spinBoxOptionsCacheMemorySize->value();
  optionData->guiInfoTextSize = ui->spinBoxOptionsGuiInfoText->value();
  optionData->guiRouteTableTextSize = ui->spinBoxOptionsGuiRouteText->value();
  optionData->guiSearchTableTextSize = ui->spinBoxOptionsGuiSearchText->value();
  optionData->guiInfoSimSize = ui->spinBoxOptionsGuiSimInfoText->value();
  optionData->mapClickSensitivity = ui->spinBoxOptionsMapClickRect->value();
  optionData->mapTooltipSensitivity = ui->spinBoxOptionsMapTooltipRect->value();
  optionData->mapSymbolSize = ui->spinBoxOptionsMapSymbolSize->value();
  optionData->mapTextSize = ui->spinBoxOptionsMapTextSize->value();
  optionData->mapZoomShow = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMap->value());
  optionData->routeGroundBuffer = ui->spinBoxOptionsRouteGroundBuffer->value();
}

void OptionsDialog::fromOptionData()
{
  fromFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  fromFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  fromFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  fromFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  fromFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  fromFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  fromFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  fromFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  fromFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_EAST_WEST_RULE);
  fromFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  fromFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  fromFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ASN);
  fromFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  fromFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ASN);
  fromFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);

  QString txt;
  for(int val : optionData->mapRangeRings)
  {
    if(!txt.isEmpty())
      txt += " ";
    txt += QString::number(val);
  }
  ui->lineEditOptionsMapRangeRings->setText(txt);
  ui->lineEditOptionsWeatherAsnPath->setText(optionData->weatherActiveSkyPath);

  ui->listWidgetOptionsDatabaseAddon->clear();
  for(const QString& str : optionData->databaseAddonExclude)
    ui->listWidgetOptionsDatabaseAddon->addItem(str);

  ui->listWidgetOptionsDatabaseExclude->clear();
  for(const QString& str : optionData->databaseExclude)
    ui->listWidgetOptionsDatabaseExclude->addItem(str);

  switch(optionData->mapScrollDetail)
  {
    case opts::FULL:
      ui->radioButtonOptionsMapScrollFull->setChecked(true);
      break;
    case opts::NORMAL:
      ui->radioButtonOptionsMapScrollNormal->setChecked(true);
      break;
    case opts::NONE:
      ui->radioButtonOptionsMapScrollNone->setChecked(true);
      break;
  }

  switch(optionData->mapSimUpdateRate)
  {
    case opts::FAST:
      ui->radioButtonOptionsMapSimUpdateFast->setChecked(true);
      break;
    case opts::MEDIUM:
      ui->radioButtonOptionsMapSimUpdateMedium->setChecked(true);
      break;
    case opts::LOW:
      ui->radioButtonOptionsMapSimUpdateLow->setChecked(true);
      break;
  }

  ui->spinBoxOptionsCacheDiskSize->setValue(optionData->cacheSizeDisk);
  ui->spinBoxOptionsCacheMemorySize->setValue(optionData->cacheSizeMemory);
  ui->spinBoxOptionsGuiInfoText->setValue(optionData->guiInfoTextSize);
  ui->spinBoxOptionsGuiRouteText->setValue(optionData->guiRouteTableTextSize);
  ui->spinBoxOptionsGuiSearchText->setValue(optionData->guiSearchTableTextSize);
  ui->spinBoxOptionsGuiSimInfoText->setValue(optionData->guiInfoSimSize);
  ui->spinBoxOptionsMapClickRect->setValue(optionData->mapClickSensitivity);
  ui->spinBoxOptionsMapTooltipRect->setValue(optionData->mapTooltipSensitivity);
  ui->spinBoxOptionsMapSymbolSize->setValue(optionData->mapSymbolSize);
  ui->spinBoxOptionsMapTextSize->setValue(optionData->mapTextSize);
  ui->doubleSpinBoxOptionsMapZoomShowMap->setValue(optionData->mapZoomShow);
  ui->spinBoxOptionsRouteGroundBuffer->setValue(optionData->routeGroundBuffer);
}

void OptionsDialog::toFlags(QCheckBox *checkBox, opts::Flags flag)
{
  if(checkBox->isChecked())
    optionData->flags |= flag;
  else
    optionData->flags &= ~flag;
}

void OptionsDialog::toFlags(QRadioButton *checkBox, opts::Flags flag)
{
  if(checkBox->isChecked())
    optionData->flags |= flag;
  else
    optionData->flags &= ~flag;
}

void OptionsDialog::fromFlags(QCheckBox *checkBox, opts::Flags flag)
{
  checkBox->setChecked(optionData->flags & flag);
}

void OptionsDialog::fromFlags(QRadioButton *checkBox, opts::Flags flag)
{
  checkBox->setChecked(optionData->flags & flag);
}
