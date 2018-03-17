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

#include "print/printdialog.h"
#include "ui_printdialog.h"
#include "gui/widgetstate.h"
#include "gui/helphandler.h"
#include "common/constants.h"
#include "weather/weatherreporter.h"
#include "connect/connectclient.h"
#include "options/optiondata.h"
#include "gui/mainwindow.h"
#include "route/routecontroller.h"

#include <QPushButton>

#include "route/route.h"

PrintDialog::PrintDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::PrintDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->buttonBoxPrint->button(QDialogButtonBox::Ok)->setText(tr("&Print"));
  ui->buttonBoxPrint->button(QDialogButtonBox::Yes)->setText(tr("&Print Preview"));

  connect(ui->buttonBoxPrint, &QDialogButtonBox::clicked, this, &PrintDialog::buttonBoxClicked);

  connect(ui->checkBoxPrintDepartureOverview, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureRunways, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureDetailRunways, &QCheckBox::toggled, this,
          &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureSoftRunways, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureCom, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureAppr, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDepartureWeather, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationOverview, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationRunways, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationDetailRunways, &QCheckBox::toggled, this,
          &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationSoftRunways, &QCheckBox::toggled, this,
          &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationCom, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationAppr, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationWeather, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintFlightplan, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
}

PrintDialog::~PrintDialog()
{
  delete ui;
}

void PrintDialog::updateButtonStates()
{
  ui->checkBoxPrintDepartureDetailRunways->setEnabled(ui->checkBoxPrintDepartureRunways->isChecked());
  ui->checkBoxPrintDepartureSoftRunways->setEnabled(ui->checkBoxPrintDepartureRunways->isChecked());

  ui->checkBoxPrintDestinationDetailRunways->setEnabled(ui->checkBoxPrintDestinationRunways->isChecked());
  ui->checkBoxPrintDestinationSoftRunways->setEnabled(ui->checkBoxPrintDestinationRunways->isChecked());

  prt::PrintFlightPlanOpts opts = getPrintOptions();
  bool canPrint = opts & prt::DEPARTURE_ANY || opts & prt::DESTINATION_ANY || opts & prt::FLIGHTPLAN;

  ui->buttonBoxPrint->button(QDialogButtonBox::Ok)->setEnabled(canPrint);
  ui->buttonBoxPrint->button(QDialogButtonBox::Yes)->setEnabled(canPrint);
}

prt::PrintFlightPlanOpts PrintDialog::getPrintOptions() const
{
  prt::PrintFlightPlanOpts opts = prt::NONE;

  opts |= ui->checkBoxPrintDepartureOverview->isChecked() ? prt::DEPARTURE_OVERVIEW : prt::NONE;
  opts |= ui->checkBoxPrintDepartureRunways->isChecked() ? prt::DEPARTURE_RUNWAYS : prt::NONE;
  opts |= ui->checkBoxPrintDepartureDetailRunways->isChecked() ? prt::DEPARTURE_RUNWAYS_DETAIL : prt::NONE;
  opts |= ui->checkBoxPrintDepartureSoftRunways->isChecked() ? prt::DEPARTURE_RUNWAYS_SOFT : prt::NONE;
  opts |= ui->checkBoxPrintDepartureCom->isChecked() ? prt::DEPARTURE_COM : prt::NONE;
  opts |= ui->checkBoxPrintDepartureAppr->isChecked() ? prt::DEPARTURE_APPR : prt::NONE;
  opts |= ui->checkBoxPrintDepartureWeather->isChecked() ? prt::DEPARTURE_WEATHER : prt::NONE;
  opts |= ui->checkBoxPrintDestinationOverview->isChecked() ? prt::DESTINATION_OVERVIEW : prt::NONE;
  opts |= ui->checkBoxPrintDestinationRunways->isChecked() ? prt::DESTINATION_RUNWAYS : prt::NONE;
  opts |=
    ui->checkBoxPrintDestinationDetailRunways->isChecked() ? prt::DESTINATION_RUNWAYS_DETAIL : prt::NONE;
  opts |= ui->checkBoxPrintDestinationSoftRunways->isChecked() ? prt::DESTINATION_RUNWAYS_SOFT : prt::NONE;
  opts |= ui->checkBoxPrintDestinationCom->isChecked() ? prt::DESTINATION_COM : prt::NONE;
  opts |= ui->checkBoxPrintDestinationAppr->isChecked() ? prt::DESTINATION_APPR : prt::NONE;
  opts |= ui->checkBoxPrintDestinationWeather->isChecked() ? prt::DESTINATION_WEATHER : prt::NONE;

  opts |= ui->checkBoxPrintFlightplan->isChecked() ? prt::FLIGHTPLAN : prt::NONE;
  return opts;
}

int PrintDialog::getPrintTextSize() const
{
  return ui->spinBoxPrintTextSize->value();
}

void PrintDialog::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_PRINT_DIALOG).save(
    {ui->checkBoxPrintDepartureOverview,
     ui->checkBoxPrintDepartureRunways,
     ui->checkBoxPrintDepartureDetailRunways,
     ui->checkBoxPrintDepartureSoftRunways,
     ui->checkBoxPrintDepartureCom,
     ui->checkBoxPrintDepartureAppr,
     ui->checkBoxPrintDepartureWeather,
     ui->checkBoxPrintDestinationOverview,
     ui->checkBoxPrintDestinationRunways,
     ui->checkBoxPrintDestinationDetailRunways,
     ui->checkBoxPrintDestinationSoftRunways,
     ui->checkBoxPrintDestinationCom,
     ui->checkBoxPrintDestinationAppr,
     ui->checkBoxPrintDestinationWeather,
     ui->checkBoxPrintFlightplan,
     ui->spinBoxPrintTextSize});

}

void PrintDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_PRINT_DIALOG).restore(
    {ui->checkBoxPrintDepartureOverview,
     ui->checkBoxPrintDepartureRunways,
     ui->checkBoxPrintDepartureDetailRunways,
     ui->checkBoxPrintDepartureSoftRunways,
     ui->checkBoxPrintDepartureCom,
     ui->checkBoxPrintDepartureAppr,
     ui->checkBoxPrintDepartureWeather,
     ui->checkBoxPrintDestinationOverview,
     ui->checkBoxPrintDestinationRunways,
     ui->checkBoxPrintDestinationDetailRunways,
     ui->checkBoxPrintDestinationSoftRunways,
     ui->checkBoxPrintDestinationCom,
     ui->checkBoxPrintDestinationAppr,
     ui->checkBoxPrintDestinationWeather,
     ui->checkBoxPrintFlightplan,
     ui->spinBoxPrintTextSize});
  updateButtonStates();
}

void PrintDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Ok))
    emit printClicked();
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Yes))
    emit printPreviewClicked();
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrl(this, lnm::HELP_ONLINE_URL + "PRINT.html#printing-the-flight-plan",
                                          lnm::helpLanguagesOnline());
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Close))
    QDialog::reject();
}

void PrintDialog::accept()
{
}
