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

#include "route/runwayselectiondialog.h"

#include "ui_runwayselectiondialog.h"
#include "common/constants.h"
#include "gui/runwaytable.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"

#include <QPushButton>
#include <QStringBuilder>

RunwaySelectionDialog::RunwaySelectionDialog(QWidget *parent, const map::MapAirport& mapAirport, const QStringList& runwayNameFilter,
                                             const QString& header, bool navdata, int preselectRunwayEndNav)
  : QDialog(parent), ui(new Ui::RunwaySelectionDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Set SID or STAR name in header label
  ui->labelRunwaySelectionHeader->setText(header);

  // Create runway table handler
  runwayTable = new RunwayTable(parent, mapAirport, ui->tableWidgetRunwaySelection, navdata, false /* addAirportParam */);
  runwayTable->setAirportLabel(ui->labelRunwaySelectionAirport);
  runwayTable->setRunwayNameFilter(runwayNameFilter);
  runwayTable->setPreSelectedRunwayEnd(preselectRunwayEndNav);

  connect(runwayTable, &RunwayTable::doubleClicked, this, &RunwaySelectionDialog::doubleClicked);
  connect(runwayTable, &RunwayTable::itemSelectionChanged, this, &RunwaySelectionDialog::updateWidgets);
  connect(ui->buttonBoxRunwaySelection, &QDialogButtonBox::clicked, this, &RunwaySelectionDialog::buttonBoxClicked);

  restoreState();

  setWindowTitle(QCoreApplication::applicationName() % tr(" - Select Runway"));
}

RunwaySelectionDialog::~RunwaySelectionDialog()
{
  atools::gui::WidgetState(lnm::RUNWAY_SELECTION_DIALOG).save(this);

  delete runwayTable;
  delete ui;
}

void RunwaySelectionDialog::doubleClicked()
{
  saveState();
  QDialog::accept();
}

void RunwaySelectionDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::RUNWAY_SELECTION_DIALOG);
  // Angle not saved on purpose
  widgetState.restore(this);

  runwayTable->init();
  updateWidgets();

  ui->tableWidgetRunwaySelection->setFocus();
}

void RunwaySelectionDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::RUNWAY_SELECTION_DIALOG);
  widgetState.save(this);
}

QString RunwaySelectionDialog::getSelectedName() const
{
  return runwayTable->getCurrentSelectedName();
}

/* A button box button was clicked */
void RunwaySelectionDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRunwaySelection->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRunwaySelection->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "RUNWAYSELECTION.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRunwaySelection->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void RunwaySelectionDialog::updateWidgets()
{
  ui->buttonBoxRunwaySelection->button(QDialogButtonBox::Ok)->setEnabled(runwayTable->hasRunways());
}
