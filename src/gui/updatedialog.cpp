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

#include "gui/updatedialog.h"
#include "ui_updatedialog.h"

#include "gui/helphandler.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QPushButton>

UpdateDialog::UpdateDialog(QWidget *parent, bool manualParam, bool hasDownloadParam) :
  QDialog(parent), ui(new Ui::UpdateDialog), manual(manualParam), hasDownload(hasDownloadParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  if(!manual)
  {
    // Set ignore and remind me buttons
    ui->buttonBoxUpdate->addButton(tr("&Ignore this Update"), QDialogButtonBox::DestructiveRole);
    QPushButton *later = ui->buttonBoxUpdate->addButton(tr("&Remind me Later"), QDialogButtonBox::NoRole);
    later->setDefault(true);
  }
  else
  {
    QPushButton *close = ui->buttonBoxUpdate->addButton(QDialogButtonBox::Close); // RejectRole
    close->setDefault(true);
  }

  if(hasDownload)
    ui->buttonBoxUpdate->addButton(tr("&Download"), QDialogButtonBox::YesRole);

  connect(ui->buttonBoxUpdate, &QDialogButtonBox::clicked, this, &UpdateDialog::buttonBoxClicked);
  connect(ui->textBrowserUpdate, &QTextBrowser::anchorClicked, this, &UpdateDialog::anchorClicked);
}

/* A button box button was clicked */
void UpdateDialog::buttonBoxClicked(QAbstractButton *button)
{
  buttonClickedRole = ui->buttonBoxUpdate->buttonRole(button);

  if(buttonClickedRole == QDialogButtonBox::ButtonRole::YesRole)
    atools::gui::HelpHandler::openUrl(this, downloadUrl);

  QDialog::accept();
}

UpdateDialog::~UpdateDialog()
{
  delete ui;
}

void UpdateDialog::setMessage(const QString& text, const QUrl& url)
{
  ui->textBrowserUpdate->setText(text);
  downloadUrl = url;
}

QDialogButtonBox *UpdateDialog::getButtonBox()
{
  return ui->buttonBoxUpdate;
}

void UpdateDialog::anchorClicked(const QUrl& url)
{
  atools::gui::HelpHandler::openUrl(this, url);
}
