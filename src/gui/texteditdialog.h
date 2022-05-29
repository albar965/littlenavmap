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

#ifndef LITTLENAVMAP_TEXTEDITDIALOG_H
#define LITTLENAVMAP_TEXTEDITDIALOG_H

#include <QDialog>

namespace Ui {
class TextEditDialog;
}

class QAbstractButton;

/*
 * Simple and general text dialog that shows a line edit for text input.
 * Label allows to open links.
 */
class TextEditDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* settingsPrefixParam is used to save the dialog and checkbox state.
   * helpBaseUrlParam is the base URL of the help system. Help button will be hidden if empty.*/
  explicit TextEditDialog(QWidget *parent, const QString& title, const QString& labelText, const QString& labelText2,
                          const QString& helpBaseUrlParam = QString());
  virtual ~TextEditDialog() override;

  TextEditDialog(const TextEditDialog& other) = delete;
  TextEditDialog& operator=(const TextEditDialog& other) = delete;

  void setText(const QString& text);
  void setText2(const QString& text);

  QString getText() const;
  QString getText2() const;

private:
  void buttonBoxClicked(QAbstractButton *button);

  Ui::TextEditDialog *ui;
  QString helpBaseUrl;

};

#endif // LITTLENAVMAP_TEXTEDITDIALOG_H
