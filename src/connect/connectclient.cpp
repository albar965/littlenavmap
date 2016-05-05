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

#include "connectclient.h"
#include "connectdialog.h"
#include "gui/errorhandler.h"
#include "gui/dialog.h"

#include <QDataStream>
#include <QTcpSocket>
#include <QWidget>
#include <QApplication>

ConnectClient::ConnectClient(QWidget *parent)
  : QObject(parent), parentWidget(parent)
{

}

ConnectClient::~ConnectClient()
{
  closeSocket();
  delete data;
}

void ConnectClient::readFromServer()
{
  if(data == nullptr)
    data = new atools::fs::SimConnectData;

  if(data->read(socket))
  {
    qDebug() << "data" << data->getAirplaneName();
    emit dataPacketReceived(*data);
    delete data;
    data = nullptr;
  }
}

void ConnectClient::connectedToServer()
{
  qInfo() << "Connected to" << socket->peerName() << ":" << socket->peerPort();
}

void ConnectClient::readFromServerError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error);

  qWarning() << "Error connecting" << socket->errorString();

  QMessageBox::critical(parentWidget, QApplication::applicationName(),
                        QString("Error in server connection: \"%1\" (%2)").
                        arg(socket->errorString()).arg(socket->error()),
                        QMessageBox::Close, QMessageBox::NoButton);

  closeSocket();
}

void ConnectClient::connectToServer()
{
  ConnectDialog dlg(parentWidget);

  int retval = dlg.exec();
  dlg.hide();

  if(retval == QDialog::Accepted)
  {
    closeSocket();

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConnectClient::readFromServer);
    connect(socket, &QTcpSocket::connected, this, &ConnectClient::connectedToServer);
    connect(socket,
            static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &ConnectClient::readFromServerError);

    socket->connectToHost(dlg.getHostname(), dlg.getPort(), QAbstractSocket::ReadOnly);
  }
}

void ConnectClient::closeSocket()
{
  if(socket != nullptr)
  {
    socket->abort();
    socket->deleteLater();
    socket = nullptr;
  }
}
