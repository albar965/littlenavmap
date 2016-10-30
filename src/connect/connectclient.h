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
#include <QTimer>

#include "fs/sc/simconnectdata.h"

class QTcpSocket;
class ConnectDialog;
class MainWindow;

namespace atools {
namespace fs {
namespace sc {
class DataReaderThread;
}
}
}

/*
 * Client for the Little Navconnect Simconnect agent/server. Receives data and passes it around by emitting a signal.
 * Does not use multithreading - runs completely in the event loop.
 */
class ConnectClient :
  public QObject
{
  Q_OBJECT

public:
  ConnectClient(MainWindow *parent);
  virtual ~ConnectClient();

  /* Opens the connect dialog and depending on result connects to the server/agent */
  void connectToServerDialog();

  /* Connects directly if the connect on startup option is set */
  void tryConnectOnStartup();

  /* true if connected to Little Navconnect */
  bool isConnected() const;

  /* Just saves and restores the state of the dialog */
  void saveState();
  void restoreState();

signals:
  /* Emitted when a new SimConnect data was received from the server (Little Navconnect) */
  void dataPacketReceived(atools::fs::sc::SimConnectData simConnectData);

  /* Emitted when a connection was established */
  void connectedToSimulator();

  /* Emitted when disconnected manually or due to error */
  void disconnectedFromSimulator();

private:
  const int SOCKET_RECONNECT_SEC = 5;
  const int DIRECT_RECONNECT_SEC = 5;
  const unsigned int DIRECT_UPDATE_RATE_MS = 200;

  void readFromSocket();
  void readFromSocketError(QAbstractSocket::SocketError error);
  void connectedToServerSocket();
  void closeSocket(bool allowRestart);
  void connectInternal();
  void writeReplyToSocket();
  void disconnectClicked();
  void postSimConnectData(atools::fs::sc::SimConnectData dataPacket);
  void postLogMessage(QString message, bool warning);
  void connectedToSimulatorDirect();
  void disconnectedFromSimulatorDirect();

  bool silent = false, manualDisconnect = false;
  ConnectDialog *dialog = nullptr;

  /* Does automatic reconnect */
  atools::fs::sc::DataReaderThread *dataReader = nullptr;

  /* Have to keep it since it is read multiple times */
  atools::fs::sc::SimConnectData *simConnectData = nullptr;

  QTcpSocket *socket = nullptr;
  /* Used to trigger reconnects on socket base connections */
  QTimer reconnectNetworkTimer;
  MainWindow *mainWindow;
};

#endif // LITTLENAVMAP_CONNECTCLIENT_H
