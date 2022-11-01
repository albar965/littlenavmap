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

#include "route/runwayselectiondialog.h"

#include "ui_runwayselectiondialog.h"
#include "common/constants.h"
#include "gui/runwayselection.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"

#include <QPushButton>
#include <QStringBuilder>

RunwaySelectionDialog::RunwaySelectionDialog(QWidget *parent, const map::MapAirport& mapAirport,
                                             const QStringList& runwayNameFilter, const QString& header)
  : QDialog(parent), ui(new Ui::RunwaySelectionDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Set SID or STAR name in header label
  ui->labelRunwaySelectionHeader->setText(header);

  // Create runway table handler
  runwaySelection = new RunwaySelection(parent, mapAirport, ui->tableWidgetRunwaySelection);
  runwaySelection->setAirportLabel(ui->labelRunwaySelectionAirport);
  runwaySelection->setRunwayNameFilter(runwayNameFilter);

  connect(runwaySelection, &RunwaySelection::doubleClicked, this, &RunwaySelectionDialog::doubleClicked);
  connect(runwaySelection, &RunwaySelection::itemSelectionChanged, this, &RunwaySelectionDialog::updateWidgets);
  connect(ui->buttonBoxRunwaySelection, &QDialogButtonBox::clicked, this, &RunwaySelectionDialog::buttonBoxClicked);

  restoreState();

  setWindowTitle(QApplication::applicationName() % tr(" - Select Runway"));
}

RunwaySelectionDialog::~RunwaySelectionDialog()
{
  atools::gui::WidgetState(lnm::RUNWAY_SELECTION_DIALOG).save(this);

  delete runwaySelection;
  delete ui;
}

void RunwaySelectionDialog::doubleClicked()
{
  saveState();
  QDialog::accept();
}

void RunwaySelectionDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::RUNWAY_SELECTION_DIALOG, false);
  // Angle not saved on purpose
  widgetState.restore(this);

  runwaySelection->restoreState();
  updateWidgets();

  ui->tableWidgetRunwaySelection->setFocus();
}

void RunwaySelectionDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::RUNWAY_SELECTION_DIALOG, false);
  widgetState.save(this);
}

QString RunwaySelectionDialog::getSelectedName() const
{
  return runwaySelection->getCurrentSelectedName();
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
  ui->buttonBoxRunwaySelection->button(QDialogButtonBox::Ok)->setEnabled(runwaySelection->hasRunways());
}
