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
#include "fs/sc/datareaderthread.h"
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
  dataReader = new atools::fs::sc::DataReaderThread(mainWindow, false);

  // TODO settings
  dataReader->setUpdateRate(DIRECT_UPDATE_RATE_MS);
  dataReader->setReconnectRateSec(DIRECT_RECONNECT_SEC);

  connect(dataReader, &atools::fs::sc::DataReaderThread::postSimConnectData, this,
          &ConnectClient::postSimConnectData);
  connect(dataReader, &atools::fs::sc::DataReaderThread::postLogMessage, this,
          &ConnectClient::postLogMessage);

  connect(dataReader, &atools::fs::sc::DataReaderThread::connectedToSimulator, this,
          &ConnectClient::connectedToSimulatorDirect);
  connect(dataReader, &atools::fs::sc::DataReaderThread::disconnectedFromSimulator, this,
          &ConnectClient::disconnectedFromSimulatorDirect);

  connect(dialog, &ConnectDialog::disconnectClicked, this, &ConnectClient::disconnectClicked);

  reconnectNetworkTimer.setSingleShot(true);
  connect(&reconnectNetworkTimer, &QTimer::timeout, this, &ConnectClient::connectInternal);
}

ConnectClient::~ConnectClient()
{
  reconnectNetworkTimer.stop();
  closeSocket(false);

  dataReader->setTerminate();
  dataReader->wait();

  delete dataReader;
  delete dialog;
}

void ConnectClient::connectToServerDialog()
{
  dialog->setConnected(isConnected());

  // Show dialog
  int retval = dialog->exec();
  dialog->hide();

  if(retval == QDialog::Accepted)
  {
    silent = false;
    closeSocket(false);

    dataReader->setTerminate(true);
    dataReader->wait();
    dataReader->setTerminate(false);

    connectInternal();
  }
}

void ConnectClient::tryConnectOnStartup()
{
  if(dialog->isAutoConnect())
  {
    reconnectNetworkTimer.stop();
    silent = true;
    connectInternal();
  }
}

void ConnectClient::connectedToSimulatorDirect()
{
  mainWindow->setConnectionStatusMessageText(tr("Connected"),
                                             tr("Connected to local flight simulator."));
  dialog->setConnected(isConnected());
  emit connectedToSimulator();
}

void ConnectClient::disconnectedFromSimulatorDirect()
{
  mainWindow->setConnectionStatusMessageText(tr("Disconnected"),
                                             tr("Disconnected from local flight simulator."));
  dialog->setConnected(isConnected());
  emit disconnectedFromSimulator();
}

void ConnectClient::postSimConnectData(atools::fs::sc::SimConnectData dataPacket)
{
  emit dataPacketReceived(dataPacket);
}

void ConnectClient::postLogMessage(QString message, bool warning)
{
  Q_UNUSED(message);
  Q_UNUSED(warning);

  // mainWindow->setStatusMessage(message);
}

void ConnectClient::saveState()
{
  dialog->saveState();
}

void ConnectClient::restoreState()
{
  dialog->restoreState();
}

/* Called by signal ConnectDialog::disconnectClicked */
void ConnectClient::disconnectClicked()
{
  reconnectNetworkTimer.stop();

  dataReader->setTerminate(true);
  dataReader->wait();
  dataReader->setTerminate(false);

  // Close but do not allow reconnect
  closeSocket(false);
}

void ConnectClient::connectInternal()
{
  if(dialog->isConnectDirect())
  {
    qDebug() << "Starting direct connection";
    // Datareader has its own reconnect mechanism
    dataReader->start();

    mainWindow->setConnectionStatusMessageText(tr("Connecting..."),
                                               tr("Connecting to local flight simulator."));
  }
  else if(socket == nullptr)
  {
    qDebug() << "Starting network connection";

    // Create new socket and connect signals
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConnectClient::readFromSocket);
    connect(socket, &QTcpSocket::connected, this, &ConnectClient::connectedToServerSocket);
    connect(socket,
            static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &ConnectClient::readFromSocketError);

    qDebug() << "Connecting to" << dialog->getHostname() << ":" << dialog->getPort();
    socket->connectToHost(dialog->getHostname(), dialog->getPort(), QAbstractSocket::ReadWrite);

    mainWindow->setConnectionStatusMessageText(tr("Connecting..."),
                                               tr("Connecting to remote flight simulator on \"%1\".").
                                               arg(dialog->getHostname()));
  }
}

bool ConnectClient::isConnected() const
{
  return (socket != nullptr && socket->isOpen()) || dataReader->isConnected();
}

/* Called by signal QAbstractSocket::error */
void ConnectClient::readFromSocketError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error);

  reconnectNetworkTimer.stop();

  qWarning() << "Error reading from" << socket->peerName() << ":" << dialog->getPort()
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
    {
      QString msg = tr("Error in server connection: %1 (%2).%3").
                    arg(socket->errorString()).
                    arg(socket->error()).
                    arg(dialog->isAutoConnect() ? tr("\nWill retry to connect.") : QString());

      // Closed due to error
      QMessageBox::critical(mainWindow, QApplication::applicationName(), msg,
                            QMessageBox::Close, QMessageBox::NoButton);
    }
  }

  closeSocket(true);
}

void ConnectClient::closeSocket(bool allowRestart)
{
  QAbstractSocket::SocketError error = QAbstractSocket::SocketError::UnknownSocketError;
  QString peer("Unknown"), errorStr("No error");
  if(socket != nullptr)
  {
    error = socket->error();
    errorStr = socket->errorString();
    peer = socket->peerName();

    socket->abort();
    socket->deleteLater();
    socket = nullptr;
  }

  delete simConnectData;
  simConnectData = nullptr;

  QString msgTooltip, msg;
  if(error == QAbstractSocket::RemoteHostClosedError || error == QAbstractSocket::UnknownSocketError)
  {
    msg = tr("Disconnected");
    msgTooltip = tr("Disconnected from remote flight simulator on \"%1\".").arg(peer);
  }
  else
  {
    if(silent)
    {
      msg = tr("Connecting...");
      msgTooltip = tr("Error while trying to connect to \"%1\": %2 (%3).\nWill retry.").
                   arg(peer).arg(errorStr).arg(error);
    }
    else
    {
      msg = tr("Connect Error");
      msgTooltip = tr("Error in server connection to \"%1\": %2 (%3)").arg(peer).arg(errorStr).arg(error);
    }
  }

  mainWindow->setConnectionStatusMessageText(msg, msgTooltip);
  dialog->setConnected(isConnected());

  emit disconnectedFromSimulator();

  if(!dialog->isConnectDirect() && dialog->isAutoConnect() && allowRestart)
  {
    silent = true;
    reconnectNetworkTimer.start(SOCKET_RECONNECT_SEC * 1000);
  }
  else
    silent = false;
}

void ConnectClient::writeReplyToSocket()
{
  atools::fs::sc::SimConnectReply reply;
  reply.write(socket);

  if(reply.getStatus() != atools::fs::sc::OK)
  {
    // Something went wrong - shutdown
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString(tr("Error writing reply to Little Navconnect: %1.")).
                          arg(reply.getStatusText()));
    closeSocket(false);
    return;
  }

  if(!socket->flush())
    qWarning() << "Reply to server not flushed";
}

/* Called by signal QTcpSocket::connected */
void ConnectClient::connectedToServerSocket()
{
  qInfo() << "Connected to" << socket->peerName() << ":" << socket->peerPort();
  reconnectNetworkTimer.stop();

  mainWindow->setConnectionStatusMessageText(tr("Connected"),
                                             tr("Connected to remote flight simulator on \"%1\".").
                                             arg(socket->peerName()));

  silent = false;

  dialog->setConnected(isConnected());

  // Let other program parts know about the new connection
  emit connectedToSimulator();
}

/* Called by signal QTcpSocket::readyRead - read data from socket */
void ConnectClient::readFromSocket()
{
  if(simConnectData == nullptr)
    // Need to keep the data in background since this method can be called multiple times until the data is filled
    simConnectData = new atools::fs::sc::SimConnectData;

  bool read = simConnectData->read(socket);
  if(simConnectData->getStatus() != atools::fs::sc::OK)
  {
    // Something went wrong - shutdown
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          QString(tr("Error reading data from Little Navconnect: %1.")).
                          arg(simConnectData->getStatusText()));
    closeSocket(false);
    return;
  }

  if(read)
  {
    // Data was read completely and successfully - reply to server
    writeReplyToSocket();

    if(simConnectData->getUserAircraft().getPosition().isValid())
      emit dataPacketReceived(*simConnectData);

    delete simConnectData;
    simConnectData = nullptr;
  }
}
