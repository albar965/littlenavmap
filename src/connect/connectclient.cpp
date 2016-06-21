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

#include "common/constants.h"
#include "connectdialog.h"
#include "gui/errorhandler.h"
#include "gui/dialog.h"
#include "fs/sc/simconnectreply.h"

#include <QDataStream>
#include <QTcpSocket>
#include <QWidget>
#include <QApplication>
#include <QThread>

#include "gui/widgetstate.h"

#include <gui/mainwindow.h>

ConnectClient::ConnectClient(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  dialog = new ConnectDialog(mainWindow);
}

ConnectClient::~ConnectClient()
{
  closeSocket();
  delete data;
  delete dialog;
}

void ConnectClient::readFromServer()
{
  if(data == nullptr)
    data = new atools::fs::sc::SimConnectData;

  bool read = data->read(socket);
  if(data->getStatus() != atools::fs::sc::OK)
  {
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString("Error reading data  from Little Navconnect: %1.").
                          arg(data->getStatusText()));
    closeSocket();
    return;
  }

  if(read)
  {
    writeReply();

    if(data->getPosition().isValid())
      emit dataPacketReceived(*data);

    delete data;
    data = nullptr;
  }
}

void ConnectClient::writeReply()
{
  // QThread::sleep(2);

  atools::fs::sc::SimConnectReply reply;
  reply.write(socket);

  if(reply.getStatus() != atools::fs::sc::OK)
  {
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString("Error writing reply to Little Navconnect: %1.").
                          arg(reply.getStatusText()));
    closeSocket();
    return;
  }

  if(!socket->flush())
    qWarning() << "Reply to server not flushed";
}

void ConnectClient::connectedToServer()
{
  qInfo() << "Connected to" << socket->peerName() << ":" << socket->peerPort();
  silent = false;
  emit connectedToSimulator();
  mainWindow->statusMessage(tr("Connected to %1.").arg(socket->peerName()));
}

void ConnectClient::readFromServerError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error);

  qWarning() << "Error connecting to" << socket->peerName() << ":" << dialog->getPort()
             << socket->errorString();

  if(!silent)
  {
    if(socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWDISCONNECTINFO,
                                                     "Little Navconnect closed connection.",
                                                     tr("Do not &show this dialog again."));
    }
    else
      QMessageBox::critical(mainWindow, QApplication::applicationName(),
                            QString("Error in server connection: \"%1\" (%2)").
                            arg(socket->errorString()).arg(socket->error()),
                            QMessageBox::Close, QMessageBox::NoButton);
  }

  closeSocket();
  silent = false;
}

void ConnectClient::connectToServer()
{
  dialog->setConnected(isConnected());

  int retval = dialog->exec();
  dialog->hide();

  if(retval == QDialog::Accepted)
  {
    silent = false;
    closeSocket();
    connectInternal();
  }
  else if(retval == QDialog::Rejected && dialog->isDisconnectClicked())
    closeSocket();
}

void ConnectClient::tryConnect()
{
  if(dialog->isConnectOnStartup())
  {
    silent = true;
    connectInternal();
  }
}

void ConnectClient::connectInternal()
{
  if(socket == nullptr)
  {
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConnectClient::readFromServer);
    connect(socket, &QTcpSocket::connected, this, &ConnectClient::connectedToServer);
    connect(socket,
            static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &ConnectClient::readFromServerError);

    qDebug() << "Connecting to" << dialog->getHostname() << ":" << dialog->getPort();
    socket->connectToHost(dialog->getHostname(), dialog->getPort(), QAbstractSocket::ReadWrite);
  }
}

bool ConnectClient::isConnected() const
{
  return socket != nullptr && socket->isOpen();
}

void ConnectClient::saveState()
{
  dialog->saveState();
}

void ConnectClient::restoreState()
{
  dialog->restoreState();
}

void ConnectClient::closeSocket()
{
  QString peer;
  if(socket != nullptr)
  {
    peer = socket->peerName();
    socket->abort();
    socket->deleteLater();
    socket = nullptr;
  }

  delete data;
  data = nullptr;
  emit disconnectedFromSimulator();

  if(peer.isEmpty())
    mainWindow->statusMessage(tr("Disconnected."));
  else
    mainWindow->statusMessage(tr("Disconnected from %1.").arg(peer));
}
