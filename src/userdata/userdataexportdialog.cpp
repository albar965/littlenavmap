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

#include "userdata/userdataexportdialog.h"
#include "ui_userdataexportdialog.h"

#include "common/constants.h"
#include "gui/widgetstate.h"

#include <QCheckBox>

UserdataExportDialog::UserdataExportDialog(QWidget *parent, bool disableExportSelected, bool disableAppend) :
  QDialog(parent),
  ui(new Ui::UserdataExportDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->checkBoxUserpointExportAppend->setDisabled(disableAppend);
  ui->checkBoxUserpointExportSelected->setDisabled(disableExportSelected);
}

UserdataExportDialog::~UserdataExportDialog()
{
  delete ui;
}

bool UserdataExportDialog::isAppendToFile() const
{
  return ui->checkBoxUserpointExportAppend->isChecked();
}

bool UserdataExportDialog::isExportSelected() const
{
  return ui->checkBoxUserpointExportSelected->isChecked();
}

void UserdataExportDialog::saveState()
{
  atools::gui::WidgetState state(lnm::USERDATA_EXPORT_DIALOG);
  state.save({this, ui->checkBoxUserpointExportAppend, ui->checkBoxUserpointExportSelected});
}

void UserdataExportDialog::restoreState()
{
  atools::gui::WidgetState state(lnm::USERDATA_EXPORT_DIALOG);
  state.restore({this, ui->checkBoxUserpointExportAppend, ui->checkBoxUserpointExportSelected});
}
