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

#ifndef CONNECTCLIENT_H
#define CONNECTCLIENT_H

#include <QAbstractSocket>

class QTcpSocket;

class ConnectClient :
  public QObject
{
  Q_OBJECT

public:
  ConnectClient(QWidget *parent);
  virtual ~ConnectClient();

  void connectToServer();

signals:
  void dataPacketReceived();

private:
  void readFromServer();
  void readFromServerError(QAbstractSocket::SocketError error);
  void connectedToServer();
  void closeSocket();

  QTcpSocket *socket = nullptr;
  QWidget *parentWidget;

};

#endif // CONNECTCLIENT_H
