/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "gui/texteditdialog.h"
#include "ui_texteditdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"

#include <QMimeData>
#include <QPushButton>
#include <QClipboard>
#include <QDebug>
#include <QLabel>
#include <QUrl>

TextEditDialog::TextEditDialog(QWidget *parent, const QString& title, const QString& labelText, const QString& labelText2,
                               const QString& helpBaseUrlParam)
  : QDialog(parent), ui(new Ui::TextEditDialog), helpBaseUrl(helpBaseUrlParam)
{
  ui->setupUi(this);
  setWindowTitle(title);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->label->setVisible(!labelText.isEmpty());
  ui->label->setText(labelText);

  ui->lineEdit2->setVisible(!labelText2.isEmpty());
  ui->label2->setVisible(!labelText2.isEmpty());
  ui->label2->setText(labelText2);

  if(helpBaseUrl.isEmpty())
    ui->buttonBox->button(QDialogButtonBox::Help)->hide();

  ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &TextEditDialog::buttonBoxClicked);
}

void TextEditDialog::setText(const QString& text)
{
  ui->lineEdit->setText(text);
}

void TextEditDialog::setText2(const QString& text)
{
  ui->lineEdit2->setText(text);
}

QString TextEditDialog::getText() const
{
  return ui->lineEdit->text();
}

QString TextEditDialog::getText2() const
{
  return ui->lineEdit2->text();
}

TextEditDialog::~TextEditDialog()
{
  delete ui;
}

void TextEditDialog::buttonBoxClicked(QAbstractButton *button)
{
  QDialogButtonBox::StandardButton buttonType = ui->buttonBox->standardButton(button);

  if(buttonType == QDialogButtonBox::Ok)
    accept();
  else if(buttonType == QDialogButtonBox::Help)
  {
    if(!helpBaseUrl.isEmpty())
      atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + helpBaseUrl, lnm::helpLanguageOnline());
  }
  else
    reject();
}
