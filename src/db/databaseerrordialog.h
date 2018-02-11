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

#ifndef LITTLENAVMAP_DATABASEERRORS_H
#define LITTLENAVMAP_DATABASEERRORS_H

#include <QDialog>

namespace Ui {
class DatabaseErrorDialog;
}

class DatabaseErrorDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit DatabaseErrorDialog(QWidget *parent = 0);
  ~DatabaseErrorDialog();

  void setErrorMessages(const QString& messages);

private:
  void anchorClicked(const QUrl& url);

  Ui::DatabaseErrorDialog *ui;

};

#endif // LITTLENAVMAP_DATABASEERRORS_H
