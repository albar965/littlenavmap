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

#include "print/printdialog.h"
#include "ui_printdialog.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/widgetstate.h"
#include "gui/helphandler.h"
#include "common/constants.h"
#include "settings/settings.h"

#include <QPushButton>
#include <QTimer>

PrintDialog::PrintDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::PrintDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Change default button box texts
  ui->buttonBoxPrint->button(QDialogButtonBox::Ok)->setText(tr("&Print"));
  ui->buttonBoxPrint->button(QDialogButtonBox::Yes)->setText(tr("Print Pre&view"));

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
  connect(ui->checkBoxPrintDestinationDetailRunways, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationSoftRunways, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationCom, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationAppr, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintDestinationWeather, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintFlightplan, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);
  connect(ui->checkBoxPrintFuel, &QCheckBox::toggled, this, &PrintDialog::updateButtonStates);

  // Use saved font size for table view
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableWidgetPrintFlightplanCols);
}

PrintDialog::~PrintDialog()
{
  atools::gui::WidgetState(lnm::ROUTE_PRINT_DIALOG).save(this);

  delete ui;
  delete zoomHandler;
}

void PrintDialog::updateButtonStates()
{
  // Disable or enable widgets based on check boxes
  ui->checkBoxPrintDepartureDetailRunways->setEnabled(ui->checkBoxPrintDepartureRunways->isChecked());
  ui->checkBoxPrintDepartureSoftRunways->setEnabled(ui->checkBoxPrintDepartureRunways->isChecked());

  ui->checkBoxPrintDestinationDetailRunways->setEnabled(ui->checkBoxPrintDestinationRunways->isChecked());
  ui->checkBoxPrintDestinationSoftRunways->setEnabled(ui->checkBoxPrintDestinationRunways->isChecked());

  ui->spinBoxPrintTextSizeFlightplan->setEnabled(ui->checkBoxPrintFlightplan->isChecked());
  ui->tableWidgetPrintFlightplanCols->setEnabled(ui->checkBoxPrintFlightplan->isChecked());

  prt::PrintFlightplanOpts opts = getPrintOptions();
  bool canPrint = opts & prt::DEPARTURE_ANY || opts & prt::DESTINATION_ANY || opts & prt::FLIGHTPLAN;

  ui->buttonBoxPrint->button(QDialogButtonBox::Ok)->setEnabled(canPrint);
  ui->buttonBoxPrint->button(QDialogButtonBox::Yes)->setEnabled(canPrint);
}

prt::PrintFlightplanOpts PrintDialog::getPrintOptions() const
{
  prt::PrintFlightplanOpts opts = prt::NONE;

  opts |= ui->checkBoxPrintDepartureOverview->isChecked() ? prt::DEPARTURE_OVERVIEW : prt::NONE;
  opts |= ui->checkBoxPrintDepartureRunways->isChecked() ? prt::DEPARTURE_RUNWAYS : prt::NONE;
  opts |= ui->checkBoxPrintDepartureDetailRunways->isChecked() ? prt::DEPARTURE_RUNWAYS_DETAIL : prt::NONE;
  opts |= ui->checkBoxPrintDepartureSoftRunways->isChecked() ? prt::DEPARTURE_RUNWAYS_SOFT : prt::NONE;
  opts |= ui->checkBoxPrintDepartureCom->isChecked() ? prt::DEPARTURE_COM : prt::NONE;
  opts |= ui->checkBoxPrintDepartureAppr->isChecked() ? prt::DEPARTURE_APPR : prt::NONE;
  opts |= ui->checkBoxPrintDepartureWeather->isChecked() ? prt::DEPARTURE_WEATHER : prt::NONE;
  opts |= ui->checkBoxPrintDestinationOverview->isChecked() ? prt::DESTINATION_OVERVIEW : prt::NONE;
  opts |= ui->checkBoxPrintDestinationRunways->isChecked() ? prt::DESTINATION_RUNWAYS : prt::NONE;
  opts |= ui->checkBoxPrintDestinationDetailRunways->isChecked() ? prt::DESTINATION_RUNWAYS_DETAIL : prt::NONE;
  opts |= ui->checkBoxPrintDestinationSoftRunways->isChecked() ? prt::DESTINATION_RUNWAYS_SOFT : prt::NONE;
  opts |= ui->checkBoxPrintDestinationCom->isChecked() ? prt::DESTINATION_COM : prt::NONE;
  opts |= ui->checkBoxPrintDestinationAppr->isChecked() ? prt::DESTINATION_APPR : prt::NONE;
  opts |= ui->checkBoxPrintDestinationWeather->isChecked() ? prt::DESTINATION_WEATHER : prt::NONE;
  opts |= ui->checkBoxPrintFlightplan->isChecked() ? prt::FLIGHTPLAN : prt::NONE;
  opts |= ui->checkBoxPrintNewPage->isChecked() ? prt::NEW_PAGE : prt::NONE;
  opts |= ui->checkBoxPrintFuel->isChecked() ? prt::FUEL_REPORT : prt::NONE;
  opts |= ui->checkBoxPrintHeader->isChecked() ? prt::HEADER : prt::NONE;
  return opts;
}

int PrintDialog::getPrintTextSize() const
{
  return ui->spinBoxPrintTextSize->value();
}

int PrintDialog::getPrintTextSizeFlightplan() const
{
  return ui->spinBoxPrintTextSizeFlightplan->value();
}

void PrintDialog::setRouteTableColumns(const QStringList& columns)
{
  ui->tableWidgetPrintFlightplanCols->clear();
  ui->tableWidgetPrintFlightplanCols->setColumnCount(1);
  ui->tableWidgetPrintFlightplanCols->setRowCount(columns.size());

  int idx = 0;
  for(const QString& col : columns)
    ui->tableWidgetPrintFlightplanCols->setItem(idx++, 0, new QTableWidgetItem(col));

  // Load selection
  if(selectedRows.size() == ui->tableWidgetPrintFlightplanCols->rowCount())
  {
    // Reverse order to keep last selected on top, i.e. scroll to top
    for(int i = selectedRows.size() - 1; i >= 0; i--)
    {
      if(selectedRows.at(i))
        ui->tableWidgetPrintFlightplanCols->selectRow(i);
    }
  }
  else
  {
    // Size does not match - set all to selected
    ui->tableWidgetPrintFlightplanCols->selectAll();
    ui->tableWidgetPrintFlightplanCols->scrollToTop();
  }
}

const QBitArray& PrintDialog::getSelectedRouteTableColumns() const
{
  return selectedRows;
}

void PrintDialog::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_PRINT_DIALOG).save(QList<const QObject *>({
    this, ui->checkBoxPrintDepartureOverview, ui->checkBoxPrintDepartureRunways, ui->checkBoxPrintDepartureDetailRunways,
    ui->checkBoxPrintDepartureSoftRunways, ui->checkBoxPrintDepartureCom, ui->checkBoxPrintDepartureAppr, ui->checkBoxPrintDepartureWeather,
    ui->checkBoxPrintDestinationOverview, ui->checkBoxPrintDestinationRunways, ui->checkBoxPrintDestinationDetailRunways,
    ui->checkBoxPrintDestinationSoftRunways, ui->checkBoxPrintDestinationCom, ui->checkBoxPrintDestinationAppr,
    ui->checkBoxPrintDestinationWeather, ui->checkBoxPrintFlightplan, ui->checkBoxPrintHeader, ui->checkBoxPrintFuel,
    ui->checkBoxPrintNewPage, ui->spinBoxPrintTextSize, ui->spinBoxPrintTextSizeFlightplan}));

  // Save selection to bitarray
  selectedRows = QBitArray(ui->tableWidgetPrintFlightplanCols->rowCount());
  for(int i = 0; i < ui->tableWidgetPrintFlightplanCols->rowCount(); i++)
    selectedRows.setBit(i, ui->tableWidgetPrintFlightplanCols->item(i, 0)->isSelected());
  atools::settings::Settings::instance().setValueVar(lnm::ROUTE_PRINT_DIALOG + "Selection", selectedRows);
}

void PrintDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_PRINT_DIALOG).restore(
  {
    this,
    ui->checkBoxPrintDepartureOverview,
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
    ui->checkBoxPrintHeader,
    ui->checkBoxPrintFuel,
    ui->checkBoxPrintNewPage,
    ui->spinBoxPrintTextSize,
    ui->spinBoxPrintTextSizeFlightplan
  });

  selectedRows = atools::settings::Settings::instance().valueVar(lnm::ROUTE_PRINT_DIALOG + "Selection").toBitArray();
  updateButtonStates();
}

void PrintDialog::buttonBoxClicked(QAbstractButton *button)
{
  saveState();
  if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Ok))
    emit printClicked();
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Yes))
    emit printPreviewClicked();
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "PRINT.html#printing-the-flight-plan",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxPrint->button(QDialogButtonBox::Close))
    QDialog::reject();
}

void PrintDialog::accept()
{
  /* Override to avoid closing the dialog */
}
