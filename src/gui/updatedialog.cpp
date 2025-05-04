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

#include "gui/updatedialog.h"
#include "ui_updatedialog.h"

#include "gui/desktopservices.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QClipboard>
#include <QMimeData>

static const QDialogButtonBox::ButtonRole IGNORE_THIS_UPDATE_ROLE = QDialogButtonBox::DestructiveRole;
static const QDialogButtonBox::ButtonRole REMIND_LATER_ROLE = QDialogButtonBox::NoRole;
static const QDialogButtonBox::ButtonRole CLOSE_ROLE = QDialogButtonBox::RejectRole;
static const QDialogButtonBox::ButtonRole COPY_ROLE = QDialogButtonBox::ActionRole;

UpdateDialog::UpdateDialog(QWidget *parent, bool manualCheck)
  : QDialog(parent), ui(new Ui::UpdateDialog), manual(manualCheck)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);
  setWindowTitle(tr("%1 - Updates Available").arg(QCoreApplication::applicationName()));

  if(!manual)
  {
    // Set ignore and remind me buttons
    ui->buttonBoxUpdate->addButton(tr("&Ignore this Update"), IGNORE_THIS_UPDATE_ROLE);
    QPushButton *later = ui->buttonBoxUpdate->addButton(tr("&Remind me Later"), REMIND_LATER_ROLE);
    later->setDefault(true);
  }
  else
  {
    QPushButton *close = ui->buttonBoxUpdate->addButton(tr("&Close"), CLOSE_ROLE);
    close->setDefault(true);
  }

  ui->buttonBoxUpdate->addButton(tr("&Copy to Clipboard"), COPY_ROLE);

  connect(ui->buttonBoxUpdate, &QDialogButtonBox::clicked, this, &UpdateDialog::buttonBoxClicked);
  connect(ui->textBrowserUpdate, &QTextBrowser::anchorClicked, this, &UpdateDialog::anchorClicked);
}

/* A button box button was clicked */
void UpdateDialog::buttonBoxClicked(QAbstractButton *button)
{
  buttonClickedRole = ui->buttonBoxUpdate->buttonRole(button);

  if(buttonClickedRole == COPY_ROLE)
  {
    // Copy formatted and plain text to clipboard
    QMimeData *data = new QMimeData;
    data->setHtml(ui->textBrowserUpdate->toHtml());
    data->setText(ui->textBrowserUpdate->toPlainText());
    QGuiApplication::clipboard()->setMimeData(data);
  }
  else
    QDialog::accept();
}

UpdateDialog::~UpdateDialog()
{
  delete ui;
}

void UpdateDialog::setMessage(const QString& text)
{
  ui->textBrowserUpdate->setText(text);
}

QDialogButtonBox *UpdateDialog::getButtonBox()
{
  return ui->buttonBoxUpdate;
}

bool UpdateDialog::isIgnoreThisUpdate() const
{
  return buttonClickedRole == IGNORE_THIS_UPDATE_ROLE;
}

void UpdateDialog::anchorClicked(const QUrl& url)
{
  atools::gui::DesktopServices::openUrl(this, url);
}
