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

#ifndef LITTLENAVMAP_CONNECTCLIENT_H
#define LITTLENAVMAP_CONNECTCLIENT_H

#include <QAbstractSocket>

#include "fs/sc/simconnectdata.h"

class QTcpSocket;
class ConnectDialog;
class MainWindow;

class ConnectClient :
  public QObject
{
  Q_OBJECT

public:
  ConnectClient(MainWindow *parent);
  virtual ~ConnectClient();

  void connectToServer();
  void tryConnect();

  bool isConnected() const;

  void saveState();
  void restoreState();

signals:
  void dataPacketReceived(atools::fs::sc::SimConnectData data);
  void connectedToSimulator();
  void disconnectedFromSimulator();

private:
  void readFromServer();
  void readFromServerError(QAbstractSocket::SocketError error);
  void connectedToServer();
  void closeSocket();
  void connectInternal();
  void writeReply();

  bool silent = false;
  ConnectDialog *dialog = nullptr;
  atools::fs::sc::SimConnectData *data = nullptr;
  QTcpSocket *socket = nullptr;
  MainWindow *mainWindow;

};

#endif // LITTLENAVMAP_CONNECTCLIENT_H
