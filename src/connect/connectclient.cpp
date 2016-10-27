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

#include "connect/connectclient.h"

#include "common/constants.h"
#include "connect/connectdialog.h"
#include "fs/sc/simconnectreply.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/widgetstate.h"

#include <QDataStream>
#include <QTcpSocket>
#include <QWidget>
#include <QApplication>
#include <QThread>

ConnectClient::ConnectClient(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  dialog = new ConnectDialog(mainWindow);
  connect(dialog, &ConnectDialog::disconnectClicked, this, &ConnectClient::disconnectClicked);
}

ConnectClient::~ConnectClient()
{
  closeSocket();
  delete simConnectData;
  delete dialog;
}

/* Called by signal QTcpSocket::readyRead - read data from socket */
void ConnectClient::readFromSocket()
{
  if(simConnectData == nullptr)
    simConnectData = new atools::fs::sc::SimConnectData;

  bool read = simConnectData->read(socket);
  if(simConnectData->getStatus() != atools::fs::sc::OK)
  {
    // Something went wrong - shutdown
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString(tr("Error reading data  from Little Navconnect: %1.")).
                          arg(simConnectData->getStatusText()));
    closeSocket();
    return;
  }

  if(read)
  {
    // Data was read completely and successfully - reply to server
    writeReply();

    if(simConnectData->getUserAircraft().getPosition().isValid())
      emit dataPacketReceived(*simConnectData);

    delete simConnectData;
    simConnectData = nullptr;
  }
}

void ConnectClient::writeReply()
{
  atools::fs::sc::SimConnectReply reply;
  reply.write(socket);

  if(reply.getStatus() != atools::fs::sc::OK)
  {
    // Something went wrong - shutdown
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString(tr("Error writing reply to Little Navconnect: %1.")).
                          arg(reply.getStatusText()));
    closeSocket();
    return;
  }

  if(!socket->flush())
    qWarning() << "Reply to server not flushed";
}

/* Called by signal ConnectDialog::disconnectClicked */
void ConnectClient::disconnectClicked()
{
  closeSocket();
  dialog->setConnected(false);
}

/* Called by signal QTcpSocket::connected */
void ConnectClient::connectedToServer()
{
  qInfo() << "Connected to" << socket->peerName() << ":" << socket->peerPort();
  silent = false;

  // Let other program parts know about the new connection
  emit connectedToSimulator();
  mainWindow->setStatusMessage(tr("Connected to %1.").arg(socket->peerName()));
}

/* Called by signal QAbstractSocket::error */
void ConnectClient::readFromSocketError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error);

  qWarning() << "Error connecting to" << socket->peerName() << ":" << dialog->getPort()
             << socket->errorString();

  if(!silent)
  {
    if(socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
      // Nicely closed on the other end
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWDISCONNECTINFO,
                                                     tr("Little Navconnect closed connection."),
                                                     tr("Do not &show this dialog again."));
    }
    else
      // Closed due to error
      QMessageBox::critical(mainWindow, QApplication::applicationName(),
                            tr("Error in server connection: \"%1\" (%2)").
                            arg(socket->errorString()).arg(socket->error()),
                            QMessageBox::Close, QMessageBox::NoButton);
  }

  closeSocket();
  silent = false;
}

void ConnectClient::connectToServer()
{
  dialog->setConnected(isConnected());

  // Show dialog
  int retval = dialog->exec();
  dialog->hide();

  if(retval == QDialog::Accepted)
  {
    silent = false;
    closeSocket();
    connectInternal();
  }
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
    // Create new socket and connect signals
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConnectClient::readFromSocket);
    connect(socket, &QTcpSocket::connected, this, &ConnectClient::connectedToServer);
    connect(socket,
            static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &ConnectClient::readFromSocketError);

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

  delete simConnectData;
  simConnectData = nullptr;
  emit disconnectedFromSimulator();

  if(peer.isEmpty())
    mainWindow->setStatusMessage(tr("Disconnected."));
  else
    mainWindow->setStatusMessage(tr("Disconnected from %1.").arg(peer));
}
