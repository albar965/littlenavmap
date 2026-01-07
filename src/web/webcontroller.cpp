/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/mappaintwidget.h"
#include "settings/settings.h"
#include "web/requesthandler.h"
#include "web/webmapcontroller.h"
#include "webapi/webapicontroller.h"
#include "web/webapp.h"
#include "gui/desktopservices.h"
#include "httpserver/httplistener.h"
#include "common/htmlinfobuilder.h"
#include "common/constants.h"
#include "gui/dialog.h"
#include "options/optiondata.h"
#include "web/webtools.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QWidget>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QSettings>

using namespace stefanfrings;

struct Host
{
  Host(QHostAddress addrParam, QString nameParam = QString())
    : addr(addrParam), name(nameParam)
  {
  }

  QHostAddress addr;
  QString name;
};

// =================================================================
WebController::WebController(QWidget *parent) :
  QObject(parent), parentWidget(parent)
{
  qDebug() << Q_FUNC_INFO;

  // Find the configuration file and create settings
  QString confPath = ":/littlenavmap/resources/config/webserver.cfg";
  configFileName = atools::settings::Settings::getOverloadedPath(confPath);

  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_WEBSERVER_DEBUG, false).toBool();

  // Remember any custom set certificates for later

  // Copy from config file and remove group name
  QSettings settings(configFileName, QSettings::IniFormat);

  copyKeyValuesFromGroup(settings, "listener", listenerSettings);

  sslKeyFile = listenerSettings.value("sslKeyFile").toString();
  sslCertFile = listenerSettings.value("sslCertFile").toString();

  mapController = new WebMapController(parentWidget, verbose);
  apiController = new WebApiController(parentWidget, verbose);

  updateSettings();
}

WebController::~WebController()
{
  qDebug() << Q_FUNC_INFO;

  stopServer();

  ATOOLS_DELETE_LOG(mapController);
  ATOOLS_DELETE_LOG(apiController);
  ATOOLS_DELETE_LOG(htmlInfoBuilder);
}

void WebController::startServer()
{
  qDebug() << Q_FUNC_INFO;

  if(isListenerRunning())
  {
    qWarning() << Q_FUNC_INFO << "Web server already running";
    return;
  }

  QString docroot = documentRoot;
  if(docroot.isEmpty())
    // Build full canonical path with / as separator only on all systems and no "/" at the end
    docroot = atools::canonicalFilePath(getDefaultDocumentRoot());

  if(docroot.endsWith("/"))
    docroot.chop(1);

  // Initialize global settings and caches
  WebApp::init(this, configFileName, docroot);

  qDebug() << Q_FUNC_INFO << "Docroot" << docroot << "port" << port << "SSL" << encrypted;

  // Start map
  mapController->initMapPaintWidget();

  if(htmlInfoBuilder == nullptr)
    htmlInfoBuilder = new HtmlInfoBuilder(mapController->getMapPaintWidget()->getQueries(),
                                          true /* info */, true /* print */, true /* verbose */);

  requestHandler = new RequestHandler(this, mapController, apiController, htmlInfoBuilder, verbose);

  // Set port - always override configuration file
  listenerSettings.insert("port", port);

  if(encrypted)
  {
    listenerSettings.insert("sslKeyFile", sslKeyFile.isEmpty() ? ":/littlenavmap/resources/config/ssl/lnm.key" : sslKeyFile);
    listenerSettings.insert("sslCertFile", sslCertFile.isEmpty() ? ":/littlenavmap/resources/config/ssl/lnm.cert" : sslCertFile);
  }
  else
  {
    listenerSettings.remove("sslKeyFile");
    listenerSettings.remove("sslCertFile");
  }

  qDebug() << "Using" << "sslKeyFile" << listenerSettings.value("sslKeyFile").toString()
           << "sslCertFile" << listenerSettings.value("sslCertFile").toString();

  listener = new HttpListener(listenerSettings, requestHandler, this);

  if(!listener->isListening())
  {
    // Not listening - display error dialog ====================================================
    atools::gui::Dialog::warning(parentWidget, tr("Unable to start the server. Error:\n%1.").arg(listener->errorString()));
    stopServer();
  }
  else
  {
    // Listening ====================================================
    qInfo() << Q_FUNC_INFO << "Listening on" << listener->serverAddress().toString() << listener->serverPort();

    QHostAddress listenerAddress = listener->serverAddress(), localhostIpv4, localhostIpv6;

    if(listenerAddress == QHostAddress::Any)
    {
      // Collect hostnames and IPs from all interfaces
      const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
      for(const QHostAddress& hostAddr : addresses)
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << hostAddr << hostAddr.protocol()
                 << "global" << hostAddr.isGlobal() << "link local" << hostAddr.isLinkLocal();
#endif

        // Ignore null, broad- and multicas and IPv6 link local
        if(hostAddr.isNull() || hostAddr.isBroadcast() || hostAddr.isMulticast() ||
           (hostAddr.protocol() == QAbstractSocket::IPv6Protocol && hostAddr.isLinkLocal()))
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
          qDebug() << "Found valid IP" << ipStr
                   << "isLinkLocal" << hostAddr.isLinkLocal() << "isSiteLocal" << hostAddr.isSiteLocal()
                   << "isUniqueLocalUnicast" << hostAddr.isUniqueLocalUnicast() << "isGlobal" << hostAddr.isGlobal();

          hosts.append(Host(hostAddr));
        }
        else
          qDebug() << "Found IP" << ipStr;
      }
    }

    // Add IPv4 localhost if nothing was found =================================
    if(hosts.isEmpty() && !localhostIpv4.isNull())
      hosts.append(Host(QHostAddress(QHostAddress::LocalHost), "localhost"));

    // Ensure IPv4 in front
    std::sort(hosts.begin(), hosts.end(), [](const Host& host1, const Host& host2) {
      return host1.addr.protocol() < host2.addr.protocol();
    });
  }

  emit webserverStatusChanged(true);
}

void WebController::stopServer()
{
  qDebug() << Q_FUNC_INFO;

  if(!isListenerRunning())
    return;

  if(listener != nullptr)
    listener->close();

  ATOOLS_DELETE_LOG(listener);
  ATOOLS_DELETE_LOG(requestHandler);

  hosts.clear();

  WebApp::deinit();

  mapController->deInitMapPaintWidget();

  emit webserverStatusChanged(false);
}

void WebController::restartServer(bool force)
{
  qDebug() << Q_FUNC_INFO << "isRunning()" << isListenerRunning() << "force" << force;

  if(isListenerRunning() || force)
  {
    stopServer();
    startServer();
  }
}

void WebController::openPage()
{
  const QUrl url = getUrl(false /* useIpAddress */);

  if(!url.isEmpty())
    atools::gui::DesktopServices::openUrl(parentWidget, url);
  else if(verbose)
    qWarning() << Q_FUNC_INFO << "No valid URL found";
}

bool WebController::isListenerRunning() const
{
  return listener != nullptr && listener->isListening();
}

QUrl WebController::getUrl(bool useIpAddress)
{
  QUrl url;
  if(!hosts.isEmpty())
  {
    const QHostAddress& addr = hosts.constFirst().addr;
    if(useIpAddress)
    {
      url.setScheme(encrypted ? "https" : "http");
      url.setHost(addr.toString());
      url.setPort(port);
    }
    else
    {
      url.setScheme(encrypted ? "https" : "http");
      url.setHost(hostName(addr));
      url.setPort(port);
    }
  }
  return url;
}

QStringList WebController::getUrlStr()
{
  QStringList retval;
  int num = 1;

  for(const Host& host : std::as_const(hosts))
  {
    const QHostAddress& addr = host.addr;
    QUrl nameUrl, ipUrl;
    nameUrl.setScheme(encrypted ? "https" : "http");
    nameUrl.setHost(hostName(addr));
    nameUrl.setPort(port);

    ipUrl.setScheme(encrypted ? "https" : "http");
    ipUrl.setHost(addr.toString());
    ipUrl.setPort(port);

    retval.append(tr("%1. %2 <a href=\"%3\"><b>%4:%5</b></a> <small>(<a href=\"%6\">%7:%8</a>)</small>").
                  arg(num++).
                  arg(addr.protocol() == QAbstractSocket::IPv6Protocol ? tr("IPv6") : tr("IPv4")).
                  arg(nameUrl.toString(QUrl::None).toHtmlEscaped()).arg(nameUrl.host().toHtmlEscaped()).arg(nameUrl.port()).
                  arg(ipUrl.toString(QUrl::None).toHtmlEscaped()).arg(ipUrl.host().toHtmlEscaped()).arg(ipUrl.port()));
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

QString WebController::getDefaultDocumentRoot() const
{
  return atools::nativeCleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + "web");
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

QString WebController::hostName(const QHostAddress& hostAddr)
{
  // Look for host in list
  auto it = std::find_if(hosts.begin(), hosts.end(), [hostAddr](const Host& host) -> bool {
    return host.addr == hostAddr;
  });

  if(it != hosts.end())
  {
    // Found host
    if(it->name.isEmpty())
      // Name is empty - update
      it->name = QHostInfo::fromName(hostAddr.toString()).hostName();

    return it->name;
  }

  return QString();
}
