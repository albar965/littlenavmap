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

#include "web/webcontroller.h"

#include "settings/settings.h"
#include "web/requesthandler.h"
#include "web/webmapcontroller.h"
#include "webapi/webapicontroller.h"
#include "web/webapp.h"
#include "gui/helphandler.h"
#include "httpserver/httplistener.h"
#include "common/htmlinfobuilder.h"
#include "common/constants.h"

#include <QSettings>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QWidget>
#include <QHostInfo>
#include <QNetworkInterface>

#include <options/optiondata.h>

using namespace stefanfrings;

WebController::WebController(QWidget *parent) :
  QObject(parent), parentWidget(parent)
{
  qDebug() << Q_FUNC_INFO;

  // Find the configuration file and create settings
  QString confPath = ":/littlenavmap/resources/config/webserver.cfg";
  configFileName = atools::settings::Settings::getOverloadedPath(confPath);

  atools::io::IniReader reader;
  reader.setCommentCharacters({";", "#"});
  reader.setPreserveCase(true);
  reader.read(configFileName);
  listenerSettings = reader.getKeyValuePairs("listener");

  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_WEBSERVER_DEBUG, false).toBool();

  // Remember any custom set certificates for later
  sslKeyFile = listenerSettings.value("sslKeyFile").toString();
  sslCertFile = listenerSettings.value("sslCertFile").toString();

  mapController = new WebMapController(parentWidget, verbose);
  apiController = new WebApiController(parentWidget, verbose);

  htmlInfoBuilder = new HtmlInfoBuilder(parent, mapController->getMapPaintWidget(), true /*info*/, true /*print*/);
  updateSettings();
}

WebController::~WebController()
{
  qDebug() << Q_FUNC_INFO;

  stopServer();

  delete mapController;
  delete apiController;
  delete htmlInfoBuilder;
}

void WebController::startServer()
{
  qDebug() << Q_FUNC_INFO;

  if(isRunning())
  {
    qWarning() << Q_FUNC_INFO << "Web server already running";
    return;
  }

  QString docroot = documentRoot;
  if(docroot.isEmpty())
    // Build full canonical path with / as separator only on all systems and no "/" at the end
    docroot = QDir::fromNativeSeparators(getDefaultDocumentRoot());

  if(docroot.endsWith("/"))
    docroot.chop(1);

  // Initialize global settings and caches
  WebApp::init(this, configFileName, docroot);

  qDebug() << Q_FUNC_INFO << "Docroot" << docroot << "port" << port << "SSL" << encrypted;

  // Start map
  mapController->init();

  requestHandler = new RequestHandler(this, mapController, apiController, htmlInfoBuilder, verbose);

  // Set port - always override configuration file
  listenerSettings.insert("port", port);

  if(encrypted)
  {
    listenerSettings.insert("sslKeyFile", sslKeyFile.isEmpty() ?
                            ":/littlenavmap/resources/config/ssl/lnm.key" : sslKeyFile);
    listenerSettings.insert("sslCertFile", sslCertFile.isEmpty() ?
                            ":/littlenavmap/resources/config/ssl/lnm.cert" : sslCertFile);
  }
  else
  {
    listenerSettings.remove("sslKeyFile");
    listenerSettings.remove("sslCertFile");
  }

  listenerSettings.insert("filename", configFileName);

  qDebug() << "Using" <<
    "sslKeyFile" << listenerSettings.value("sslKeyFile").toString() <<
    "sslCertFile" << listenerSettings.value("sslCertFile").toString();

  listener = new HttpListener(listenerSettings, requestHandler, this);

  if(!listener->isListening())
  {
    // Not listening - display error dialog ====================================================
    QMessageBox::warning(parentWidget, QCoreApplication::applicationName(),
                         tr("Unable to start the server. Error:\n%1.").arg(listener->errorString()));
    stopServer();
  }
  else
  {
    // Listening ====================================================
    qInfo() << Q_FUNC_INFO << "Listening on" << listener->serverAddress().toString() << listener->serverPort();

    QString scheme = encrypted ? "https" : "http";
    QHostAddress listenerAddress = listener->serverAddress(), localhostIpv4, localhostIpv6;
    QUrl urlName;
    urlName.setScheme(scheme);
    urlName.setPort(port);

    if(listenerAddress == QHostAddress::Any)
    {
      // Collect hostnames and IPs from all interfaces
      const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
      for(const QHostAddress& hostAddr : addresses)
      {
        if(hostAddr.isNull())
          continue;

        if(hostAddr.isLoopback())
        {
          // Add loopback addresses later if nothing else was found
          if(hostAddr.protocol() == QAbstractSocket::IPv4Protocol)
            localhostIpv4 = hostAddr;
          else if(hostAddr.protocol() == QAbstractSocket::IPv6Protocol)
            localhostIpv6 = hostAddr;
          continue;
        }

        QString ipStr = hostAddr.toString();
        if(hostAddr.protocol() == QAbstractSocket::IPv4Protocol || hostAddr.protocol() == QAbstractSocket::IPv6Protocol)
        {
          QString name = QHostInfo::fromName(ipStr).hostName();
          qDebug() << "Found valid IP" << ipStr << "name" << name
                   << "isLinkLocal" << hostAddr.isLinkLocal() << "isSiteLocal" << hostAddr.isSiteLocal()
                   << "isUniqueLocalUnicast" << hostAddr.isUniqueLocalUnicast() << "isGlobal" << hostAddr.isGlobal();

          if(!name.isEmpty() && name != ipStr)
            urlName.setHost(name);
          else
            urlName.setHost(ipStr);

          QUrl urlIp(urlName);
          if(hostAddr.protocol() == QAbstractSocket::IPv6Protocol && hostAddr.isLinkLocal())
            urlIp.setHost(ipStr.section('%', 0, 0)); // Remove interface from local IPv6 addresses
          else
            urlIp.setHost(ipStr);

          hosts.append(Host(urlName, urlIp, hostAddr.protocol() == QAbstractSocket::IPv6Protocol));
        }
        else
          qDebug() << "Found IP" << ipStr;
      }
    }

    // Add IPv4 localhost if nothing was found =================================
    if(hosts.isEmpty() && !localhostIpv4.isNull())
      hosts.append(Host(QString("%1://localhost:%2").arg(scheme).arg(port),
                        QString("%1://%2:%3").arg(scheme).arg(QHostAddress(QHostAddress::LocalHost).toString()).arg(port), false));

    // Ensure IPv4 in front
    std::sort(hosts.begin(), hosts.end(), [](const Host& host1, const Host& host2) {
      return host1.ipv6 < host2.ipv6;
    });
  }

  emit webserverStatusChanged(true);
}

void WebController::stopServer()
{
  qDebug() << Q_FUNC_INFO;

  if(!isRunning())
    return;

  mapController->deInit();

  if(listener != nullptr)
    listener->close();

  delete listener;
  listener = nullptr;

  delete requestHandler;
  requestHandler = nullptr;

  hosts.clear();

  WebApp::deinit();

  emit webserverStatusChanged(false);
}

void WebController::restartServer(bool force)
{
  qDebug() << Q_FUNC_INFO << "isRunning()" << isRunning() << "force" << force;

  if(isRunning() || force)
  {
    stopServer();
    startServer();
  }
}

void WebController::openPage()
{
  if(!getUrl(false /* useIpAddress */).isEmpty())
    atools::gui::HelpHandler::openUrl(parentWidget, getUrl(false));
  else
    qWarning() << Q_FUNC_INFO << "No valid URL found";
}

bool WebController::isRunning() const
{
  return listener != nullptr && listener->isListening();
}

QUrl WebController::getUrl(bool useIpAddress) const
{
  if(hosts.isEmpty())
    return QUrl();
  else
    return useIpAddress ? hosts.constFirst().urlIp : hosts.constFirst().urlName;
}

QStringList WebController::getUrlStr() const
{
  QStringList retval;
  int num = 1;
  for(const Host& host : hosts)
  {
    const QUrl& hostname = host.urlName;
    const QUrl& ip = host.urlIp;

    retval.append(tr("%1. %2 <a href=\"%3\"><b>%4:%5</b></a> <small>(<a href=\"%6\">%7:%8</a>)</small>").
                  arg(num++).
                  arg(host.ipv6 ? tr("IPv6") : tr("IPv4")).
                  arg(hostname.toString(QUrl::None).toHtmlEscaped()).arg(hostname.host().toHtmlEscaped()).arg(hostname.port()).
                  arg(ip.toString(QUrl::None).toHtmlEscaped()).arg(ip.host().toHtmlEscaped()).arg(ip.port()));
  }
  return retval;
}

void WebController::optionsChanged()
{
  if(updateSettings())
    restartServer(true);
}

bool WebController::updateSettings()
{
  bool changed = false;
  int newport = OptionData::instance().getWebPort();
  if(port != newport)
  {
    qDebug() << Q_FUNC_INFO << "Port changed from " << port << "to" << newport;
    port = newport;
    changed = true;
  }

  int newencrypted = OptionData::instance().isWebEncrypted();
  if(encrypted != newencrypted)
  {
    qDebug() << Q_FUNC_INFO << "encrypted changed from " << encrypted << "to" << newencrypted;
    encrypted = newencrypted;
    changed = true;
  }

  QString newdocumentRoot =
    QDir::fromNativeSeparators(QFileInfo(OptionData::instance().getWebDocumentRoot()).canonicalFilePath());

  if(QDir::fromNativeSeparators(QFileInfo(documentRoot).canonicalFilePath()) != newdocumentRoot)
  {
    qDebug() << Q_FUNC_INFO << "Document Root changed from " << documentRoot << "to" << newdocumentRoot;
    documentRoot = newdocumentRoot;
    changed = true;
  }

  return changed;
}

QString WebController::getAbsoluteWebrootFilePath() const
{
  if(documentRoot.isEmpty())
    return QDir::toNativeSeparators(getDefaultDocumentRoot());
  else
    return QDir::toNativeSeparators(QFileInfo(documentRoot).canonicalFilePath());
}

QString WebController::getDefaultDocumentRoot() const
{
  return QFileInfo(QCoreApplication::applicationDirPath() + QDir::separator() + "web").canonicalFilePath();
}

WebMapController *WebController::getWebMapController() const
{
  return mapController;

}

void WebController::preDatabaseLoad()
{
  mapController->preDatabaseLoad();
}

void WebController::postDatabaseLoad()
{
  mapController->postDatabaseLoad();
}
