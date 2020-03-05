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

#ifndef LITTLENAVMAP_TEXTDIALOG_H
#define LITTLENAVMAP_TEXTDIALOG_H

#include <QDialog>

namespace Ui {
class TextDialog;
}

class QAbstractButton;

/*
 * Simple and general text dialog that shows a text browser with error messages for example.
 */
class TextDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* settingsPrefixParam is used to save the dialog and checkbox state.
   * helpBaseUrlParam is the base URL of the help system. Help button will be hidden if empty.*/
  explicit TextDialog(QWidget *parent, const QString& title, const QString& helpBaseUrlParam = QString());
  virtual ~TextDialog() override;

  /* Set HTML text message to show.
   * Set printToLog to true to print the HTML as plain text into the log as info when calling this method. */
  void setHtmlMessage(const QString& messages, bool printToLog);

private:
  void buttonBoxClicked(QAbstractButton *button);
  void anchorClicked(const QUrl& url);

  Ui::TextDialog *ui;
  QString helpBaseUrl;

};

#endif // LITTLENAVMAP_TEXTDIALOG_H
