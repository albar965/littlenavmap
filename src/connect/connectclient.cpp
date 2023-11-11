/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
#include "common/constants.h"
#include "fs/sc/simconnectreply.h"
#include "fs/sc/datareaderthread.h"
#include "gui/dialog.h"
#include "online/onlinedatacontroller.h"
#include "gui/mainwindow.h"
#include "geo/calculations.h"
#include "settings/settings.h"
#include "fs/sc/simconnecthandler.h"
#include "fs/sc/xpconnecthandler.h"
#include "fs/scenery/languagejson.h"
#include "fs/scenery/aircraftindex.h"

#include <QDataStream>
#include <QTcpSocket>
#include <QWidget>
#include <QCoreApplication>
#include <QThread>
#include <QDir>
#include <QHostAddress>

using atools::fs::sc::DataReaderThread;

const static int FLUSH_QUEUE_MS = 50;

/* Any metar fetched from the Simulator will time out in 15 seconds */
const static int WEATHER_TIMEOUT_FS_SECS = 15;
const static int NOT_AVAILABLE_TIMEOUT_FS_SECS = 300;

ConnectClient::ConnectClient(MainWindow *parent)
  : QObject(parent), mainWindow(parent), metarIdentCache(WEATHER_TIMEOUT_FS_SECS), notAvailableStations(NOT_AVAILABLE_TIMEOUT_FS_SECS)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  verbose = settings.getAndStoreValue(lnm::OPTIONS_CONNECTCLIENT_DEBUG, false).toBool();

  errorMessageBox = new QMessageBox(QMessageBox::Critical, QApplication::applicationName(),
                                    QString(), QMessageBox::Ok, mainWindow);

  // Create FSX/P3D handler for SimConnect
  simConnectHandler = new atools::fs::sc::SimConnectHandler(verbose);
  simConnectHandler->loadSimConnect(QApplication::applicationDirPath() +
                                    atools::SEP + "simconnect" + atools::SEP + "simconnect.manifest");

  // Create X-Plane handler for shared memory
  xpConnectHandler = new atools::fs::sc::XpConnectHandler();

  // Create thread class that reads data from handler
  dataReader = new DataReaderThread(mainWindow, settings.getAndStoreValue(lnm::OPTIONS_DATAREADER_DEBUG, false).toBool());

  directReconnectSimSec = settings.getAndStoreValue(lnm::OPTIONS_DATAREADER_RECONNECT_SIM, 15).toInt();
  directReconnectXpSec = settings.getAndStoreValue(lnm::OPTIONS_DATAREADER_RECONNECT_XP, 5).toInt();
  socketReconnectSec = settings.getAndStoreValue(lnm::OPTIONS_DATAREADER_RECONNECT_SOCKET, 10).toInt();

  if(simConnectHandler->isLoaded())
  {
    dataReader->setHandler(simConnectHandler);
    dataReader->setReconnectRateSec(directReconnectSimSec);
  }
  else
  {
    dataReader->setHandler(xpConnectHandler);
    dataReader->setReconnectRateSec(directReconnectXpSec);
  }

  connect(dataReader, &DataReaderThread::postSimConnectData, this, &ConnectClient::postSimConnectData);
  connect(dataReader, &DataReaderThread::postStatus, this, &ConnectClient::statusPosted);
  connect(dataReader, &DataReaderThread::connectedToSimulator, this, &ConnectClient::connectedToSimulatorDirect);
  connect(dataReader, &DataReaderThread::disconnectedFromSimulator, this, &ConnectClient::disconnectedFromSimulatorDirect);

  connectDialog = new ConnectDialog(mainWindow, simConnectHandler->isLoaded());
  connect(connectDialog, &ConnectDialog::updateRateChanged, this, &ConnectClient::updateRateChanged);
  connect(connectDialog, &ConnectDialog::aiFetchRadiusChanged, this, &ConnectClient::aiFetchRadiusChanged);
  connect(connectDialog, &ConnectDialog::fetchOptionsChanged, this, &ConnectClient::fetchOptionsChanged);

  connect(connectDialog, &ConnectDialog::disconnectClicked, this, &ConnectClient::disconnectClicked);
  connect(connectDialog, &ConnectDialog::autoConnectToggled, this, &ConnectClient::autoConnectToggled);

  reconnectNetworkTimer.setSingleShot(true);
  connect(&reconnectNetworkTimer, &QTimer::timeout, this, &ConnectClient::connectInternalAuto);

  flushQueuedRequestsTimer.setInterval(FLUSH_QUEUE_MS);
  connect(&flushQueuedRequestsTimer, &QTimer::timeout, this, &ConnectClient::flushQueuedRequests);
  flushQueuedRequestsTimer.start();
}

ConnectClient::~ConnectClient()
{
  qDebug() << Q_FUNC_INFO;

  flushQueuedRequestsTimer.stop();
  reconnectNetworkTimer.stop();

  disconnectClicked();

  ATOOLS_DELETE_LOG(dataReader);
  ATOOLS_DELETE_LOG(simConnectHandler);
  ATOOLS_DELETE_LOG(xpConnectHandler);
  ATOOLS_DELETE_LOG(connectDialog);
  ATOOLS_DELETE_LOG(errorMessageBox);
}

void ConnectClient::flushQueuedRequests()
{
  if(!queuedRequests.isEmpty())
  {
    atools::fs::sc::WeatherRequest req = queuedRequests.takeLast();
    queuedRequestIdents.remove(req.getStation());
    requestWeather(req.getStation(), req.getPosition(), false /* station only */);
  }
}

void ConnectClient::connectToServerDialog()
{
  connectDialog->setConnected(isConnected());

  // Show dialog and determine which tab to open
  cd::ConnectSimType type = cd::UNKNOWN;
  if(isNetworkConnect() && isConnected())
    type = cd::REMOTE;
  else if(isSimConnect() && isConnected())
    type = cd::FSX_P3D_MSFS;
  else if(isXpConnect() && isConnected())
    type = cd::XPLANE;

  NavApp::setStayOnTop(connectDialog);
  int retval = connectDialog->execConnectDialog(type);
  connectDialog->hide();

  if(retval == QDialog::Accepted)
  {
    errorState = false;
    silent = false;
    closeSocket(false /* allow restart */);

    dataReader->terminateThread();

    // Let the dataReader send its message to the statusbar so it does not overwrite the socket message
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    connectInternal();
  }
}

void ConnectClient::tryConnectOnStartup()
{
  if(connectDialog->isAutoConnect())
  {
    reconnectNetworkTimer.stop();

    // Do not show an error dialog
    silent = true;
    connectInternal();
  }
}

QString ConnectClient::simName() const
{
  if(connectDialog->isAnyConnectDirect())
  {
    if(dataReader->getHandler() == xpConnectHandler)
      return tr("X-Plane");
    else if(dataReader->getHandler() == simConnectHandler)
#if defined(SIMCONNECT_BUILD_WIN64)
      return tr("MSFS");

#elif defined(SIMCONNECT_BUILD_WIN32)
      return tr("FSX or Prepar3D");

#else
      return tr("FSX, Prepar3D or MSFS");

#endif
  }
  return QString();
}

QString ConnectClient::simShortName() const
{
  if(connectDialog->isAnyConnectDirect())
  {
    if(dataReader->getHandler() == xpConnectHandler)
      return tr("XP");
    else if(dataReader->getHandler() == simConnectHandler)
    {
#if defined(SIMCONNECT_BUILD_WIN64)
      return tr("MSFS");

#elif defined(SIMCONNECT_BUILD_WIN32)
      return tr("FSX/P3D");

#else
      return tr("FSX/P3D/MSFS");

#endif
    }
  }
  return QString();
}

void ConnectClient::connectedToSimulatorDirect()
{
  qDebug() << Q_FUNC_INFO;

  mainWindow->setConnectionStatusMessageText(tr("Connected (%1)").arg(simShortName()),
                                             tr("Connected to local flight simulator (%1).").arg(simName()));
  connectDialog->setConnected(isConnected());
  mainWindow->setStatusMessage(tr("Connected to simulator."), true /* addLog */);
  emit connectedToSimulator();
  emit weatherUpdated();
}

void ConnectClient::disconnectedFromSimulatorDirect()
{
  qDebug() << Q_FUNC_INFO;

  showTerminalError();

  // Try to reconnect if it was not unlinked by using the disconnect button
  if(!errorState && connectDialog->isAutoConnect() &&
     connectDialog->isAnyConnectDirect() && !manualDisconnect)
    connectInternal();
  else
    mainWindow->setConnectionStatusMessageText(tr("Disconnected"), tr("Disconnected from local flight simulator."));
  connectDialog->setConnected(isConnected());

  metarIdentCache.clear();
  outstandingReplies.clear();
  queuedRequests.clear();
  queuedRequestIdents.clear();
  notAvailableStations.clear();

  if(!NavApp::isShuttingDown())
  {
    mainWindow->setStatusMessage(tr("Disconnected from simulator."), true /* addLog */);
    emit disconnectedFromSimulator();
    emit weatherUpdated();
  }

  manualDisconnect = false;
}

/* Posts data received directly from simconnect or the socket and caches any metar reports */
void ConnectClient::postSimConnectData(atools::fs::sc::SimConnectData dataPacket)
{
  if(dataPacket.getStatus() == atools::fs::sc::OK)
  {
    // Check for empty weather replies or metar replys. Aircraft is not valid in this case.
    if(!dataPacket.isEmptyReply())
    {
      // AI list does not include user aircraft
      dataPacket.updateIndexesAndKeys();

      atools::fs::sc::SimConnectUserAircraft& userAircraft = dataPacket.getUserAircraft();
      // Workaround for MSFS sending wrong positions around 0/0 while in menu
      if(!userAircraft.isFullyValid())
      {
        if(verbose)
          qDebug() << Q_FUNC_INFO << "User aircraft not fully valid";
        // Invalidate position at the 0,0 position if no groundspeed
        userAircraft.setCoordinates(atools::geo::EMPTY_POS);
      }

      // Modify AI aircraft and set shadow flag if a online network aircraft is registered as shadowed in the index
      NavApp::getOnlinedataController()->updateAircraftShadowState(dataPacket);

      // Update the MSFS translated aircraft names and types ===================================
      /* Mooney, Boeing, Actually aircraft model. */
      // const QString& getAirplaneType() const
      // const QString& getAirplaneAirline() const
      /* Beech Baron 58 Paint 1 */
      // const QString& getAirplaneTitle() const
      /* Short ICAO code MD80, BE58, etc. Actually type designator. */
      // const QString& getAirplaneModel() const
      const atools::fs::scenery::LanguageJson& languageIndex = NavApp::getLanguageIndex();
      if(!languageIndex.isEmpty())
      {
        // Change user aircraft names
        userAircraft.updateAircraftNames(languageIndex.getName(userAircraft.getAirplaneType()),
                                         languageIndex.getName(userAircraft.getAirplaneAirline()),
                                         languageIndex.getName(userAircraft.getAirplaneTitle()),
                                         languageIndex.getName(userAircraft.getAirplaneModel()));

        // Change AI names
        for(atools::fs::sc::SimConnectAircraft& ac : dataPacket.getAiAircraft())
          ac.updateAircraftNames(languageIndex.getName(ac.getAirplaneType()),
                                 languageIndex.getName(ac.getAirplaneAirline()),
                                 languageIndex.getName(ac.getAirplaneTitle()),
                                 languageIndex.getName(ac.getAirplaneModel()));
      }

      // Update ICAO aircraft designator from aircraft.cfg for MSFS ===================================
      QString aircraftCfgKey = userAircraft.getProperties().value(atools::fs::sc::PROP_AIRCRAFT_CFG).getValueString();
      if(!aircraftCfgKey.isEmpty())
        // Has property - fetch from index by loaded aircraft.cfg values
        userAircraft.setAirplaneModel(NavApp::getAircraftIndex().getIcaoTypeDesignator(aircraftCfgKey));

      // Fix incorrect on-ground status which appears from some traffic tools =======================
      for(atools::fs::sc::SimConnectAircraft& ac : dataPacket.getAiAircraft())
      {
        // Ground speed given and too high for ground operations
        bool gsFlying = ac.getGroundSpeedKts() < map::INVALID_SPEED_VALUE && ac.getGroundSpeedKts() > 40.f;

        // Vertical speed given and too high for ground
        bool vsFlying = ac.getVerticalSpeedFeetPerMin() < map::INVALID_SPEED_VALUE &&
                        (ac.getVerticalSpeedFeetPerMin() > 100.f || ac.getVerticalSpeedFeetPerMin() < -100.f);

        if(ac.isOnGround() && (gsFlying || vsFlying))
          ac.setFlag(atools::fs::sc::ON_GROUND);
      }

      emit dataPacketReceived(dataPacket);
    } // if(!dataPacket.isEmptyReply())

    if(!dataPacket.getMetars().isEmpty())
    {
      if(verbose)
        qDebug() << "Metars number" << dataPacket.getMetars().size();

      for(atools::fs::weather::MetarResult metar : dataPacket.getMetars())
      {
        QString ident = metar.requestIdent;
        if(verbose)
        {
          qDebug() << "ConnectClient::postSimConnectData metar ident to cache ident"
                   << ident << "pos" << metar.requestPos.toString();
          qDebug() << "Station" << metar.metarForStation;
          qDebug() << "Nearest" << metar.metarForNearest;
          qDebug() << "Interpolated" << metar.metarForInterpolated;
        }

        if(metar.metarForStation.isEmpty())
        {
          if(verbose)
            qDebug() << "Station" << metar.requestIdent << "not available";

          // Remember airports that have no station reports to avoid recurring requests by airport weather
          notAvailableStations.insert(metar.requestIdent, metar.requestIdent);
        }
        else if(notAvailableStations.contains(metar.requestIdent))
          // Remove from blacklist since it now has a station report
          notAvailableStations.remove(metar.requestIdent);

        metar.simulator = true;
        metarIdentCache.insert(ident, metar);
      } // for(atools::fs::weather::MetarResult metar : dataPacket.getMetars())

      if(!dataPacket.getMetars().isEmpty())
        emit weatherUpdated();
    } // if(!dataPacket.getMetars().isEmpty())
  } // if(dataPacket.getStatus() == atools::fs::sc::OK)
  else
  {
    bool xplane = dataReader != nullptr ? dataReader->isXplaneHandler() : false, network = isNetworkConnect();
    atools::fs::sc::SimConnectStatus status = dataPacket.getStatus();
    QString statusText = simConnectData->getStatusText();

    disconnectClicked();
    handleError(status, statusText, xplane, network);
  }
}

void ConnectClient::handleError(atools::fs::sc::SimConnectStatus status, const QString& error, bool xplane, bool network)
{
  QString hint, program;

  if(xplane)
    program = tr("Little Xpconnect");
  if(network)
    program = tr("Little Navconnect");

  switch(status)
  {
    case atools::fs::sc::OK:
      break;

    case atools::fs::sc::INVALID_MAGIC_NUMBER:
    case atools::fs::sc::VERSION_MISMATCH:
      hint = tr("Update %1 to the latest version.<br/>"
                "The installed version of %1<br/>"
                "is not compatible with this version of Little Navmap.<br/><br/>"
                "Install the latest version of %1 which you can always find in the Little Navmap installation directory.").arg(program);
      errorState = true;
      break;

    case atools::fs::sc::INSUFFICIENT_WRITE:
    case atools::fs::sc::WRITE_ERROR:
      hint = tr("The connection is not reliable.<br/><br/>"
                "Check your network or installation.");
      errorState = true;
      break;
  }

  errorMessageBox->setText(tr("<p>Error receiving data from %1:</p><p>%2</p><p>%3</p>").arg(program).arg(error).arg(hint));
  errorMessageBox->show();
}

void ConnectClient::statusPosted(atools::fs::sc::SimConnectStatus status, QString statusText)
{
  qDebug() << Q_FUNC_INFO << status << statusText;

  if(status != atools::fs::sc::OK)
    handleError(status, statusText, dataReader->isXplaneHandler(), isNetworkConnect());
  else
    mainWindow->setConnectionStatusMessageText(QString(), statusText);
}

void ConnectClient::saveState()
{
  connectDialog->saveState();
}

void ConnectClient::restoreState()
{
  connectDialog->restoreState();

  dataReader->setHandler(handlerByDialogSettings());

  if(isXpConnect())
    dataReader->setReconnectRateSec(directReconnectXpSec);
  else if(isSimConnect())
    dataReader->setReconnectRateSec(directReconnectSimSec);

  dataReader->setUpdateRate(connectDialog->getUpdateRateMs(connectDialog->getCurrentSimType()));
  dataReader->setAiFetchRadius(atools::geo::nmToKm(connectDialog->getAiFetchRadiusNm(connectDialog->getCurrentSimType())));
  fetchOptionsChanged(connectDialog->getCurrentSimType());
  aiFetchRadiusChanged(connectDialog->getCurrentSimType());
}

atools::fs::sc::ConnectHandler *ConnectClient::handlerByDialogSettings()
{
  if(connectDialog->getCurrentSimType() == cd::FSX_P3D_MSFS)
    return simConnectHandler;
  else
    return xpConnectHandler;
}

void ConnectClient::aiFetchRadiusChanged(cd::ConnectSimType type)
{
  if(dataReader->getHandler() == simConnectHandler && type == cd::FSX_P3D_MSFS)
    // The currently active value has changed
    dataReader->setAiFetchRadius(atools::geo::nmToKm(connectDialog->getAiFetchRadiusNm(type)));
}

void ConnectClient::updateRateChanged(cd::ConnectSimType type)
{
  if((dataReader->getHandler() == simConnectHandler && type == cd::FSX_P3D_MSFS) ||
     (dataReader->getHandler() == xpConnectHandler && type == cd::XPLANE))
    // The currently active value has changed
    dataReader->setUpdateRate(connectDialog->getUpdateRateMs(type));
}

void ConnectClient::fetchOptionsChanged(cd::ConnectSimType type)
{
  if((dataReader->getHandler() == simConnectHandler && type == cd::FSX_P3D_MSFS) ||
     (dataReader->getHandler() == xpConnectHandler && type == cd::XPLANE))
  {
    // The currently active value has changed
    atools::fs::sc::Options options = atools::fs::sc::NO_OPTION;
    if(connectDialog->isFetchAiAircraft(type))
      options |= atools::fs::sc::FETCH_AI_AIRCRAFT;
    if(connectDialog->isFetchAiShip(type))
      options |= atools::fs::sc::FETCH_AI_BOAT;

    dataReader->setSimconnectOptions(options);

    emit aiFetchOptionsChanged();
  }
}

atools::fs::weather::MetarResult ConnectClient::requestWeather(const QString& station, const atools::geo::Pos& pos,
                                                               bool onlyStation)
{
  if(verbose)
    qDebug() << "ConnectClient::requestWeather" << station << "onlyStation" << onlyStation;

  atools::fs::weather::MetarResult retval;

  if(!isConnected())
    // Ignore cache if not connected
    return retval;

  if(isSimConnect() && !dataReader->canFetchWeather())
    // MSFS cannot fetch weather - disable to avoid stutters
    return retval;

  if(onlyStation && notAvailableStations.contains(station))
  {
    // No nearest or interpolated and airport is in blacklist
    if(verbose)
      qDebug() << "Station" << station << "in negative cache for only station";
    return retval;
  }

  // Get the old value without triggering the timeout dependent delete
  atools::fs::weather::MetarResult *result = metarIdentCache.valueNoTimeout(station);

  if(result != nullptr)
    // Return old result if there is any
    retval = *result;

  // Check if the airport is already in the queue
  if(!queuedRequestIdents.contains(station))
  {
    // Check if it is cached already or timed out
    if(!metarIdentCache.containsNoTimeout(station) || metarIdentCache.isTimedOut(station))
    {
      if(verbose)
        qDebug() << "ConnectClient::requestWeather timed out" << station;

      if((socket != nullptr && socket->isOpen()) || (dataReader->isSimConnectHandler() && dataReader->isConnected()))
      {
        atools::fs::sc::WeatherRequest weatherRequest;
        weatherRequest.setStation(station);
        weatherRequest.setPosition(pos);

        if(outstandingReplies.isEmpty())
          // Nothing waiting for reply - request now from network or data reader from sim
          requestWeather(weatherRequest);
        else if(!outstandingReplies.contains(weatherRequest.getStation()))
        {
          // Not an outstanding reply for this airport - queue request
          queuedRequests.append(weatherRequest);
          queuedRequestIdents.insert(station);
        }

        if(verbose)
        {
          qDebug() << "requestWeather === queuedRequestIdents" << queuedRequestIdents;
          qDebug() << "requestWeather === outstandingReplies" << outstandingReplies;
        }
      }
    }
  }
  return retval;
}

bool ConnectClient::isFetchAiShip() const
{
  return connectDialog->isFetchAiShip(connectDialog->getCurrentSimType());
}

bool ConnectClient::isFetchAiAircraft() const
{
  return connectDialog->isFetchAiAircraft(connectDialog->getCurrentSimType());
}

void ConnectClient::connectToggle(bool checked)
{
  qDebug() << Q_FUNC_INFO << checked;

  if(isConnected())
    disconnectClicked();
  else
    connectInternal();
}

void ConnectClient::requestWeather(const atools::fs::sc::WeatherRequest& weatherRequest)
{
  if(dataReader->isConnected() && dataReader->canFetchWeather())
    dataReader->setWeatherRequest(weatherRequest);

  if(socket != nullptr && socketConnected && socket->isOpen() && outstandingReplies.isEmpty())
  {
    if(verbose)
      qDebug() << "requestWeather" << weatherRequest.getStation();
    atools::fs::sc::SimConnectReply reply;
    reply.setCommand(atools::fs::sc::CMD_WEATHER_REQUEST);
    reply.setWeatherRequest(weatherRequest);
    writeReplyToSocket(reply);
    outstandingReplies.insert(weatherRequest.getStation());
  }
}

void ConnectClient::autoConnectToggled(bool state)
{
  if(state == false)
  {
    reconnectNetworkTimer.stop();

    if(dataReader->isReconnecting())
    {
      qDebug() << "Stopping reconnect";
      dataReader->terminateThread();
      qDebug() << "Stopping reconnect done";
    }
    mainWindow->setConnectionStatusMessageText(tr("Disconnected"), tr("Autoconnect switched off."));
  }
}

/* Called by signal ConnectDialog::disconnectClicked */
void ConnectClient::disconnectClicked()
{
  qDebug() << Q_FUNC_INFO;
  errorState = false;

  reconnectNetworkTimer.stop();

  if(dataReader->isConnected())
    // Tell disconnectedFromSimulatorDirect not to reconnect
    manualDisconnect = true;

  dataReader->terminateThread();

  // Close but do not allow reconnect if auto is on
  closeSocket(false);
}

void ConnectClient::connectInternalAuto()
{
  if(!errorState)
    connectInternal();
}

void ConnectClient::showTerminalError()
{
  if(dataReader->isFailedTerminally())
  {
    if(!terminalErrorShown)
    {
      terminalErrorShown = true;
      QMessageBox::warning(mainWindow, QApplication::applicationName(),
                           tr("Too many errors when trying to connect to simulator.\n\n"
                              "Not matching simulator interface or other SimConnect problem.\n\n"
                              "Make sure to use the right version of %1 with the right simulator:\n"
                              "%1 32-bit: FSX and P3D\n"
                              "%1 64-bit: MSFS\n"
                              "You have to restart %1 to resume.").arg(QApplication::applicationName()));
    }
  }
}

void ConnectClient::connectInternal()
{
  if(connectDialog->isAnyConnectDirect())
  {
    qDebug() << "Starting direct connection";

    // Datareader has its own reconnect mechanism
    dataReader->setHandler(handlerByDialogSettings());

    if(isXpConnect())
      dataReader->setReconnectRateSec(directReconnectXpSec);
    else if(isSimConnect())
    {
      dataReader->setReconnectRateSec(directReconnectSimSec);
      showTerminalError();
    }

    // Copy settings from dialog
    updateRateChanged(connectDialog->getCurrentSimType());
    fetchOptionsChanged(connectDialog->getCurrentSimType());
    aiFetchRadiusChanged(connectDialog->getCurrentSimType());

    if(dataReader->isFailedTerminally())
      mainWindow->setConnectionStatusMessageText(tr("Error (%1) ...").arg(simShortName()),
                                                 tr("Too many errors when trying to connect to simulator (%1).").arg(simName()));
    else
    {
      dataReader->start();
      mainWindow->setConnectionStatusMessageText(tr("Connecting (%1) ...").arg(simShortName()),
                                                 tr("Trying to connect to local flight simulator (%1).").arg(simName()));
    }
  }
  else if(socket == nullptr && !connectDialog->getRemoteHostname().isEmpty())
  {
    // qDebug() << "Starting network connection";

    // Create new socket and connect signals
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConnectClient::readFromSocket);
    connect(socket, &QTcpSocket::connected, this, &ConnectClient::connectedToServerSocket);
    connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, &ConnectClient::readFromSocketError);

    qDebug() << "Connecting to" << connectDialog->getRemoteHostname() << ":" << connectDialog->getRemotePort();
    socket->connectToHost(connectDialog->getRemoteHostname(), connectDialog->getRemotePort(),
                          QAbstractSocket::ReadWrite, QAbstractSocket::AnyIPProtocol);

    mainWindow->setConnectionStatusMessageText(tr("Connecting ..."),
                                               tr("Trying to connect to remote flight simulator on \"%1\".").
                                               arg(connectDialog->getRemoteHostname()));
  }
}

bool ConnectClient::isConnectedActive() const
{
  return (socket != nullptr && socket->isOpen() && socketConnected) || (dataReader != nullptr && dataReader->isConnected());
}

bool ConnectClient::isConnected() const
{
  // socket or SimConnect or Xpconnect
  return (socket != nullptr && socket->isOpen()) || (dataReader != nullptr && dataReader->isConnected());
}

bool ConnectClient::isSimConnect() const
{
  return dataReader != nullptr && dataReader->isSimConnectHandler();
}

bool ConnectClient::isXpConnect() const
{
  return dataReader != nullptr && dataReader->isXplaneHandler();
}

bool ConnectClient::isNetworkConnect() const
{
  return socket != nullptr && socket->isOpen();
}

/* Called by signal QAbstractSocket::error */
void ConnectClient::readFromSocketError(QAbstractSocket::SocketError)
{
  // qDebug() << Q_FUNC_INFO << error;

  reconnectNetworkTimer.stop();

  qWarning() << "Error reading from" << socket->peerName() << ":" << connectDialog->getRemotePort()
             << socket->errorString() << "open" << socket->isOpen() << "state" << socket->state();

  if(!silent)
  {
    if(socket->error() == QAbstractSocket::RemoteHostClosedError)
    {
      // Nicely closed on the other end
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOW_DISCONNECT_INFO,
                                                     tr("Remote end closed connection."),
                                                     tr("Do not &show this dialog again."));
    }
    else
    {
      QString msg = tr("Error in server connection: %1 (%2).%3").
                    arg(socket->errorString()).
                    arg(socket->error()).
                    arg(connectDialog->isAutoConnect() ? tr("\nWill retry to connect.") : QString());

      // Closed due to error
      QMessageBox::critical(mainWindow, QApplication::applicationName(), msg,
                            QMessageBox::Close, QMessageBox::NoButton);
    }
  }

  // Close and allow restart if auto is on
  closeSocket(true);
}

void ConnectClient::closeSocket(bool allowRestart)
{
  qDebug() << Q_FUNC_INFO;

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
      msg = tr("Connecting ...");
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
  connectDialog->setConnected(isConnected());

  metarIdentCache.clear();
  outstandingReplies.clear();
  queuedRequests.clear();
  queuedRequestIdents.clear();
  notAvailableStations.clear();

  if(socketConnected)
  {
    qDebug() << Q_FUNC_INFO << "emit disconnectedFromSimulator();";

    if(!NavApp::isShuttingDown())
    {
      mainWindow->setStatusMessage(tr("Disconnected from simulator."), true /* addLog */);
      emit disconnectedFromSimulator();
      emit weatherUpdated();
    }
    socketConnected = false;
  }

  if(!connectDialog->isAnyConnectDirect() && connectDialog->isAutoConnect() && allowRestart)
  {
    // For socket based connection use a timer - direct connection reconnects automatically
    silent = true;
    reconnectNetworkTimer.start(socketReconnectSec * 1000);
  }
  else
    silent = false;
}

void ConnectClient::writeReplyToSocket(atools::fs::sc::SimConnectReply& reply)
{
  if(socket != nullptr && socketConnected)
  {
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
}

/* Called by signal QTcpSocket::connected */
void ConnectClient::connectedToServerSocket()
{
  qInfo() << Q_FUNC_INFO << "Connected to" << socket->peerName() << socket->peerPort() << socket->peerAddress().protocol();
  socketConnected = true;
  reconnectNetworkTimer.stop();

  mainWindow->setConnectionStatusMessageText(tr("Connected"),
                                             tr("Connected to remote flight simulator on \"%1\".").
                                             arg(socket->peerName()));

  silent = false;

  connectDialog->setConnected(isConnected());

  // Let other program parts know about the new connection
  mainWindow->setStatusMessage(tr("Connected to simulator."), true /* addLog */);
  emit connectedToSimulator();
  emit weatherUpdated();
}

/* Called by signal QTcpSocket::readyRead - read data from socket */
void ConnectClient::readFromSocket()
{
  if(socket != nullptr)
  {
    while(socket->bytesAvailable())
    {
      if(verbose)
        qDebug() << "readFromSocket" << socket->bytesAvailable();
      if(simConnectData == nullptr)
        // Need to keep the data in background since this method can be called multiple times until the data is filled
        simConnectData = new atools::fs::sc::SimConnectData;

      bool read = simConnectData->read(socket);
      if(simConnectData->getStatus() != atools::fs::sc::OK)
      {
        // Something went wrong - shutdown

        bool xplane = dataReader != nullptr ? dataReader->isXplaneHandler() : false, network = isNetworkConnect();
        atools::fs::sc::SimConnectStatus status = simConnectData->getStatus();
        QString statusText = simConnectData->getStatusText();

        closeSocket(false);
        handleError(status, statusText, xplane, network);
        return;
      }

      if(verbose)
        qDebug() << "readFromSocket 2" << socket->bytesAvailable();
      if(read)
      {
        if(verbose)
          qDebug() << "readFromSocket id " << simConnectData->getPacketId();

        if(simConnectData->getPacketId() > 0)
        {
          if(verbose)
            qDebug() << "readFromSocket id " << simConnectData->getPacketId() << "replying";

          // Data was read completely and successfully - reply to server
          atools::fs::sc::SimConnectReply reply;
          reply.setPacketId(simConnectData->getPacketId());
          writeReplyToSocket(reply);
        }
        else if(!simConnectData->getMetars().isEmpty())
        {
          if(verbose)
            qDebug() << "readFromSocket id " << simConnectData->getPacketId() << "metars";

          for(const atools::fs::weather::MetarResult& metar : simConnectData->getMetars())
            outstandingReplies.remove(metar.requestIdent);

          // Start request on next invocation of the event queue
          QTimer::singleShot(0, this, &ConnectClient::flushQueuedRequests);
        }

        // Send around in the application
        postSimConnectData(*simConnectData);
        delete simConnectData;
        simConnectData = nullptr;
      }
      else
        return;
    }
    if(verbose)
    {
      qDebug() << "readFromSocket === queuedRequestIdents" << queuedRequestIdents;
      qDebug() << "readFromSocket outstanding" << outstandingReplies;
    }
  }
}

void ConnectClient::debugDumpContainerSizes() const
{
  qDebug() << Q_FUNC_INFO << "metarIdentCache.size()" << metarIdentCache.size();
  qDebug() << Q_FUNC_INFO << "outstandingReplies.size()" << outstandingReplies.size();
  qDebug() << Q_FUNC_INFO << "queuedRequests.size()" << queuedRequests.size();
  qDebug() << Q_FUNC_INFO << "queuedRequestIdents.size()" << queuedRequestIdents.size();
  qDebug() << Q_FUNC_INFO << "notAvailableStations.size()" << notAvailableStations.size();
}
