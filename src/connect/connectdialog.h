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

#ifndef LITTLENAVMAP_CONNECTDIALOG_H
#define LITTLENAVMAP_CONNECTDIALOG_H

#include <QDialog>

namespace Ui {
class ConnectDialog;
}

class QAbstractButton;

namespace cd {
enum ConnectSimType
{
  UNKNOWN,
  REMOTE,
  FSX_P3D,
  XPLANE
};

}

/*
 * Simulator connection dialog.
 */
class ConnectDialog :
  public QDialog
{
  Q_OBJECT

public:
  ConnectDialog(QWidget *parent, bool simConnectAvailable);
  ~ConnectDialog();

  /* Get hostname as entered in the edit field */
  QString getRemoteHostname() const;

  /* Port number as set in the spin box */
  quint16 getRemotePort() const;

  /* Saves and restores all values */
  void saveState();
  void restoreState();

  /* Set status to connected */
  void setConnected(bool connected);

  /* true if the connect on startup checkbox was checked */
  bool isAutoConnect() const;
  bool isAnyConnectDirect() const;
  bool isFetchAiAircraft(cd::ConnectSimType type) const;
  bool isFetchAiShip(cd::ConnectSimType type) const;

  cd::ConnectSimType getCurrentSimType() const;

  unsigned int getDirectUpdateRateMs(cd::ConnectSimType type);

signals:
  void disconnectClicked();
  void autoConnectToggled(bool state);

  void directUpdateRateChanged(cd::ConnectSimType type);
  void fetchOptionsChanged(cd::ConnectSimType type);

private:
  void directUpdateRateClicked();
  void fetchOptionsClicked();

  void buttonBoxClicked(QAbstractButton *button);
  void deleteClicked();
  void updateButtonStates();

  Ui::ConnectDialog *ui;
  bool simConnect = false;
};

#endif // LITTLENAVMAP_CONNECTDIALOG_H
