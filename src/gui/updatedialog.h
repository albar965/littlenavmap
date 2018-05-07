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

#ifndef LITTLENAVMAP_UPDATEDIALOG_H
#define LITTLENAVMAP_UPDATEDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QUrl>

namespace Ui {
class UpdateDialog;
}

/*
 * Shows the update HTML text from the downloaded file
 */
class UpdateDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit UpdateDialog(QWidget *parent, bool manualParam, bool hasDownloadParam);
  ~UpdateDialog();

  /* HTML text and URL for the download button */
  void setMessage(const QString& text, const QUrl& url);
  QDialogButtonBox *getButtonBox();

  /*
   * DestructiveRole = ignore
   * NoRole = later
   * RejectRole = Close (manual only)
   * YesRole = download (has download only)
   */
  QDialogButtonBox::ButtonRole getButtonClickedRole() const
  {
    return buttonClickedRole;
  }

private:
  void anchorClicked(const QUrl& url);
  void buttonBoxClicked(QAbstractButton *button);

  QUrl downloadUrl;
  QDialogButtonBox::ButtonRole buttonClickedRole = QDialogButtonBox::InvalidRole;
  Ui::UpdateDialog *ui;
  bool manual, hasDownload;
};

#endif // LITTLENAVMAP_UPDATEDIALOG_H
