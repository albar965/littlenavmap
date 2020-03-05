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

#include "gui/textdialog.h"
#include "ui_textdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"

#include <QMimeData>
#include <QPushButton>
#include <QClipboard>
#include <QDebug>
#include <QTextDocumentFragment>

TextDialog::TextDialog(QWidget *parent, const QString& title, const QString& helpBaseUrlParam)
  : QDialog(parent), ui(new Ui::TextDialog), helpBaseUrl(helpBaseUrlParam)
{
  ui->setupUi(this);
  setWindowTitle(title);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  // Copy to clipboard button in button bar ============================
  QPushButton *button = ui->buttonBox->addButton(tr("&Copy to Clipboard"), QDialogButtonBox::NoRole);
  button->setToolTip(tr("Copies message as formatted text to the clipboard"));

  if(helpBaseUrl.isEmpty())
    ui->buttonBox->removeButton(ui->buttonBox->button(QDialogButtonBox::Help));

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &TextDialog::buttonBoxClicked);
  connect(ui->textBrowser, &QTextBrowser::anchorClicked, this, &TextDialog::anchorClicked);
}

TextDialog::~TextDialog()
{
  delete ui;
}

void TextDialog::buttonBoxClicked(QAbstractButton *button)
{
  QDialogButtonBox::StandardButton buttonType = ui->buttonBox->standardButton(button);

  if(buttonType == QDialogButtonBox::Ok)
    accept();
  else if(buttonType == QDialogButtonBox::Help)
  {
    if(!helpBaseUrl.isEmpty())
      atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + helpBaseUrl, lnm::helpLanguageOnline());
  }
  else if(buttonType == QDialogButtonBox::NoButton)
  {
    // Only non-standard button is copy to clipboard
    // Copy formatted and plain text from text browser to clipboard
    QMimeData *data = new QMimeData;
    data->setHtml(ui->textBrowser->toHtml());
    data->setText(ui->textBrowser->toPlainText());
    QGuiApplication::clipboard()->setMimeData(data);
  }
}

void TextDialog::setHtmlMessage(const QString& messages, bool printToLog)
{
  ui->textBrowser->setHtml(messages);

  if(printToLog)
    qInfo().noquote().nospace() << QTextDocumentFragment::fromHtml(messages).toPlainText();
}

void TextDialog::anchorClicked(const QUrl& url)
{
  atools::gui::HelpHandler::openUrl(this, url);
}
