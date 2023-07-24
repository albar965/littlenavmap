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
    QHostAddress addr = listener->serverAddress();
    QUrl url;
    url.setScheme(scheme);
    url.setPort(port);

    if(addr == QHostAddress::Any)
    {
      // Collect hostnames and IPs from all interfaces
      for(const QHostAddress& hostAddr : QNetworkInterface::allAddresses())
      {
        QString ipStr = hostAddr.toString();
        if(!hostAddr.isLoopback() && !hostAddr.isNull() &&
           (hostAddr.protocol() == QAbstractSocket::IPv4Protocol || hostAddr.protocol() == QAbstractSocket::IPv6Protocol))
        {
          QString name = QHostInfo::fromName(ipStr).hostName();
          qDebug() << "Found valid IP" << ipStr << "name" << name
                   << "isLinkLocal" << hostAddr.isLinkLocal() << "isSiteLocal" << hostAddr.isSiteLocal()
                   << "isUniqueLocalUnicast" << hostAddr.isUniqueLocalUnicast() << "isGlobal" << hostAddr.isGlobal();

          if(!name.isEmpty() && name != ipStr)
          {
            url.setHost(name);
            urlList.append(url);
          }
          else
          {
            url.setHost(ipStr);
            urlList.append(url);
          }

          if(hostAddr.protocol() == QAbstractSocket::IPv6Protocol && hostAddr.isLinkLocal())
            url.setHost(ipStr.section('%', 0, 0)); // Remove interface from local IPv6 addresses
          else
            url.setHost(ipStr);

          urlIpList.append(url);
        }
        else
          qDebug() << "Found IP" << ipStr;
      }
    }

    // Add IPv4 localhost if nothing was found =================================
    if(urlList.isEmpty())
      urlList.append(QString(QString(scheme) + "://localhost:%1").arg(port));

    if(urlIpList.isEmpty())
      urlIpList.append(QString(QString(scheme) + "://%1:%2").arg(QHostAddress(QHostAddress::LocalHost).toString()).arg(port));
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

  urlList.clear();
  urlIpList.clear();

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
  if(!getUrl(false).isEmpty())
    atools::gui::HelpHandler::openUrl(parentWidget, getUrl(false));
}

bool WebController::isRunning() const
{
  return listener != nullptr && listener->isListening();
}

QStringList WebController::getUrlStr() const
{
  QStringList retval;
  for(int i = 0; i < urlList.size(); i++)
    retval.append(tr("<a href=\"%1\"><b>%2:%3</b></a> (<a href=\"%4\">%5:%6</a>)").
                  arg(urlList.value(i).toString(QUrl::None).toHtmlEscaped()).
                  arg(urlList.value(i).host().toHtmlEscaped()).
                  arg(urlList.value(i).port()).
                  arg(urlIpList.value(i).toString(QUrl::None).toHtmlEscaped()).
                  arg(urlIpList.value(i).host().toHtmlEscaped()).
                  arg(urlIpList.value(i).port())
                  );
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
