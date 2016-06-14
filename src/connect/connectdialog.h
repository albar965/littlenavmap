/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_CONNECTDIALOG_H
#define LITTLENAVMAP_CONNECTDIALOG_H

#include <QDialog>

namespace Ui {
class ConnectDialog;
}

class QAbstractButton;

class ConnectDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit ConnectDialog(QWidget *parent = 0);
  ~ConnectDialog();

  QString getHostname() const;
  quint16 getPort() const;

  void saveState();
  void restoreState();

  void setConnected(bool connected);

  bool isDisconnectClicked() const
  {
    return disconnectClicked;
  }

  void setDisconnectClicked(bool value)
  {
    disconnectClicked = value;
  }

  bool isConnectOnStartup() const;

private:
  bool disconnectClicked = false;
  Ui::ConnectDialog *ui;
  void buttonClicked(QAbstractButton *button);

};

#endif // LITTLENAVMAP_CONNECTDIALOG_H
