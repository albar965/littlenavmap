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

#ifndef LITTLENAVMAP_UPDATEDIALOG_H
#define LITTLENAVMAP_UPDATEDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QUrl>

namespace Ui {
class UpdateDialog;
}

/*
 * Shows the update HTML text from the downloaded file and allows the user to download,
 * ignore or remind later for updates.
 *
 * Handles all actions except ignoring a specific update.
 */
class UpdateDialog :
  public QDialog
{
  Q_OBJECT

public:
  /*
   * manualCheck = user checked manually
   * downloadAvailable = download is available
   */
  explicit UpdateDialog(QWidget *parent, bool manualCheck, bool downloadAvailable);
  ~UpdateDialog();

  /* HTML text and URL for the download button */
  void setMessage(const QString& text, const QUrl& url);
  QDialogButtonBox *getButtonBox();

  /* true if user clicked "ignore this update" */
  bool isIgnoreThisUpdate() const;

private:
  void anchorClicked(const QUrl& url);
  void buttonBoxClicked(QAbstractButton *button);

  QUrl downloadUrl;
  QDialogButtonBox::ButtonRole buttonClickedRole = QDialogButtonBox::InvalidRole;
  Ui::UpdateDialog *ui;
  bool manual, hasDownload;
};

#endif // LITTLENAVMAP_UPDATEDIALOG_H
