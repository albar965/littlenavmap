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

/*
 * Simulator connection dialog.
 */
class ConnectDialog :
  public QDialog
{
  Q_OBJECT

public:
  ConnectDialog(QWidget *parent);
  ~ConnectDialog();

  /* Get hostname as entered in the edit field */
  QString getHostname() const;

  /* Port number as set in the spin box */
  quint16 getPort() const;

  /* Saves and restores all values */
  void saveState();
  void restoreState();

  /* Set status to connected */
  void setConnected(bool connected);

  /* true if the connect on startup checkbox was checked */
  bool isAutoConnect() const;
  bool isConnectDirect() const;

signals:
  void disconnectClicked();
  void autoConnectToggled(bool state);

private:
  void buttonBoxClicked(QAbstractButton *button);
  void connectDirectToggled(bool state);

  Ui::ConnectDialog *ui;

};

#endif // LITTLENAVMAP_CONNECTDIALOG_H
