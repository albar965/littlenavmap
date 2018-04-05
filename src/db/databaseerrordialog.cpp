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

#include "db/databaseerrordialog.h"
#include "ui_databaseerrordialog.h"

#include "gui/helphandler.h"

DatabaseErrorDialog::DatabaseErrorDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::DatabaseErrorDialog)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  connect(ui->textBrowserDatabaseErrors, &QTextBrowser::anchorClicked, this, &DatabaseErrorDialog::anchorClicked);
}

DatabaseErrorDialog::~DatabaseErrorDialog()
{
  delete ui;
}

void DatabaseErrorDialog::setErrorMessages(const QString& messages)
{
  ui->textBrowserDatabaseErrors->setHtml(messages);
}

void DatabaseErrorDialog::anchorClicked(const QUrl& url)
{
  atools::gui::HelpHandler::openUrl(this, url);
}
