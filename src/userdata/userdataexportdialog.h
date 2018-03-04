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

#ifndef USERDATAEXPORTDIALOG_H
#define USERDATAEXPORTDIALOG_H

#include <QDialog>

namespace Ui {
class UserdataExportDialog;
}

/* Simple dialog asking for export options before exporting userdata to the various formats */
class UserdataExportDialog :
  public QDialog
{
  Q_OBJECT

public:
  UserdataExportDialog(QWidget *parent, bool disableExportSelected, bool disableAppend);
  ~UserdataExportDialog();

  bool isAppendToFile() const;
  bool isExportSelected() const;

  void saveState();
  void restoreState();

private:
  Ui::UserdataExportDialog *ui;
};

#endif // USERDATAEXPORTDIALOG_H
