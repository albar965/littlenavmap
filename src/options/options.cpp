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

#include "options/options.h"

#include "common/constants.h"
#include "gui/mainwindow.h"
#include "ui_options.h"

#include <gui/widgetstate.h>

Options::Options(MainWindow *parentWindow)
  : QDialog(parentWindow), ui(new Ui::Options), mainWindow(parentWindow)
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
  widgets.append(ui->spinBoxOptionsMapZoomShowMap);
  widgets.append(ui->spinBoxOptionsRouteGroundBuffer);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxOptions, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &Options::clicked);
}

Options::~Options()
{
  delete ui;
}

void Options::clicked(QAbstractButton *button)
{
  qDebug() << "Clicked" << button->text();
}

void Options::saveState()
{
  atools::gui::WidgetState saver(lnm::OPTIONS_DIALOG_WIDGET, false, true);
  saver.save(widgets);
}

void Options::restoreState()
{
  atools::gui::WidgetState saver(lnm::OPTIONS_DIALOG_WIDGET, false, true);
  saver.restore(widgets);
}
