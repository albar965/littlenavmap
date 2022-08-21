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

#include "db/databaseprogressdialog.h"
#include "ui_databaseprogressdialog.h"
#include "gui/widgetstate.h"
#include "common/constants.h"

#include <QPushButton>

DatabaseProgressDialog::DatabaseProgressDialog(QWidget *parent, const QString& simulatorName)
  : QDialog(parent), ui(new Ui::DatabaseProgressDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::NonModal);

  ui->setupUi(this);

  setWindowTitle(tr("%1 - Loading %2").arg(QApplication::applicationName()).arg(simulatorName));

  connect(ui->buttonBoxDatabaseProgress, &QDialogButtonBox::clicked, this, &DatabaseProgressDialog::buttonBoxClicked);
  restoreState();
}

DatabaseProgressDialog::~DatabaseProgressDialog()
{
  delete ui;
}

void DatabaseProgressDialog::buttonBoxClicked(QAbstractButton *button)
{
  saveState();

  if(button == ui->buttonBoxDatabaseProgress->button(QDialogButtonBox::Ok))
    QDialog::accept();
  else if(button == ui->buttonBoxDatabaseProgress->button(QDialogButtonBox::Cancel))
  {
    if(finishedState)
      // Close dialog once progress is finished and asking for use or cancel
      QDialog::reject();
    else
    {
      canceled = true;
      button->setDisabled(true);
    }
  }
}

void DatabaseProgressDialog::reject()
{
  canceled = true;
  QPushButton *cancelButton = ui->buttonBoxDatabaseProgress->button(QDialogButtonBox::Cancel);
  if(cancelButton != nullptr)
    cancelButton->setDisabled(true);

  if(ui->buttonBoxDatabaseProgress->button(QDialogButtonBox::Ok) != nullptr)
    QDialog::reject();
}

void DatabaseProgressDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::OPTIONS_DIALOG_DB_PROGRESS_DLG, false);
  widgetState.restore(this);
}

void DatabaseProgressDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::OPTIONS_DIALOG_DB_PROGRESS_DLG, false);
  widgetState.save(this);
}

void DatabaseProgressDialog::setLabelText(const QString& text)
{
  ui->labelDatabaseProgress->setText(text);
}

void DatabaseProgressDialog::setMinimum(int value)
{
  ui->progressBarDatabaseProgress->setMinimum(value);
}

void DatabaseProgressDialog::setMaximum(int value)
{
  ui->progressBarDatabaseProgress->setMaximum(value);
}

void DatabaseProgressDialog::setValue(int value)
{
  ui->progressBarDatabaseProgress->setValue(value);
}

void DatabaseProgressDialog::setFinishedState()
{
  finishedState = true;

  // Add use button and enable cancel again
  QPushButton *okButton = ui->buttonBoxDatabaseProgress->addButton(QDialogButtonBox::Ok);
  okButton->setText(tr("&Use this database"));
  okButton->setDefault(true);
  okButton->setAutoDefault(true);

  QPushButton *cancelButton = ui->buttonBoxDatabaseProgress->button(QDialogButtonBox::Cancel);
  cancelButton->setText(tr("&Discard"));
  cancelButton->setEnabled(true);
  cancelButton->setDefault(false);
  cancelButton->setAutoDefault(false);

  raise();
  activateWindow();
}
