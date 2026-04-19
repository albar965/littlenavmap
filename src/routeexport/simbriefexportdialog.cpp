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

#include "routeexport/simbriefexportdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "ui_simbriefexportdialog.h"

#include <QAbstractButton>
#include <QPushButton>

SimBriefExportDialog::SimBriefExportDialog(QWidget *parent, const QString& routeDescription, const QString& aircraftType,
                                           const QString& cruiseAltitude)
  : QDialog(parent), ui(new Ui::SimBriefExportDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  ui->lineEditAircraftType->setText(aircraftType);
  ui->lineEditCruiseAltitude->setText(cruiseAltitude);
  ui->textEditRouteDescription->setPlainText(routeDescription);

  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));
  ui->buttonBox->button(QDialogButtonBox::Ok)->setToolTip(tr("Send the flight plan to SimBrief and open it in a web browser"));

  ui->buttonBox->button(QDialogButtonBox::YesToAll)->setText(tr("&Copy Web Address to Clipboard"));
  ui->buttonBox->button(QDialogButtonBox::YesToAll)->setToolTip(tr("Copy the generated SimBrief web address to the clipboard"));

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &SimBriefExportDialog::buttonBoxClicked);

  restoreState();
  ui->lineEditAirline->setFocus();
}

SimBriefExportDialog::~SimBriefExportDialog()
{
  saveState();
  delete ui;
}

SimBriefExportDialog::Action SimBriefExportDialog::getAction() const
{
  return action;
}

QString SimBriefExportDialog::getAirline() const
{
  return ui->lineEditAirline->text();
}

QString SimBriefExportDialog::getFlightNumber() const
{
  return ui->lineEditFlightNumber->text();
}

void SimBriefExportDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_SIMBRIEF_DIALOG);
  widgetState.restore({this, ui->lineEditAirline, ui->lineEditFlightNumber});
}

void SimBriefExportDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_SIMBRIEF_DIALOG);
  widgetState.save({this, ui->lineEditAirline, ui->lineEditFlightNumber});
}

void SimBriefExportDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    action = Export;
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::YesToAll))
  {
    action = CopyToClipboard;
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
  {
    saveState();
    QDialog::reject();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl % "SENDSIMBRIEF.html", lnm::helpLanguageOnline());
}
