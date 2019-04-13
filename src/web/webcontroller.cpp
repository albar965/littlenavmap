/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "web/webapp.h"
#include "gui/helphandler.h"

#include "templateengine/templatecache.h"
#include "httpserver/httplistener.h"
#include "httpserver/httpsessionstore.h"
#include "common/htmlinfobuilder.h"
#include "common/constants.h"

#include <QSettings>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QWidget>
#include <QHostInfo>

using namespace stefanfrings;

WebController::WebController(QWidget *parent) :
  QObject(parent), parentWidget(parent)
{
  qDebug() << Q_FUNC_INFO;

  // Find the configuration file and create settings
  configFileName = atools::settings::Settings::getOverloadedLocalPath(":/littlenavmap/resources/config/webserver.cfg");

  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_WEBSERVER_DEBUG, false).toBool();

  listenerSettings = new QSettings(configFileName, QSettings::IniFormat, this);
  mapController = new WebMapController(parentWidget, verbose);
  htmlInfoBuilder = new HtmlInfoBuilder(parent, true /*info*/, true /*print*/);
}

WebController::~WebController()
{
  qDebug() << Q_FUNC_INFO;

  stopServer();

  delete mapController;
  delete listenerSettings;
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

  // Build full canonical path with / as separator only on all systems and no "/" at the end
  documentRoot = QFileInfo(QCoreApplication::applicationDirPath() + QDir::separator() + "web").canonicalFilePath();
  documentRoot.replace('\\', '/');
  if(documentRoot.endsWith("/"))
    documentRoot.chop(1);

  // Initialize global settings and caches
  WebApp::init(this, configFileName, documentRoot);

  // QString docroot = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first() + QDir::separator() +
  // QApplication::applicationName() + QDir::separator() + "Webserver";

  qDebug() << Q_FUNC_INFO << "Docroot" << documentRoot << "port" << port;

  // Start map
  mapController->init();

  requestHandler = new RequestHandler(this, mapController, htmlInfoBuilder, verbose);

  // Configure and start the TCP listener
  listenerSettings->beginGroup("listener");

  // Override port value in configuration file
  listenerSettings->setValue("port", port);

  listener = new HttpListener(listenerSettings, requestHandler, this);

  qInfo() << Q_FUNC_INFO << "Listening on" << listener->serverAddress().toString() << listener->serverPort();

  emit webserverStatusChanged(true);
}

void WebController::stopServer()
{
  qDebug() << Q_FUNC_INFO;

  if(!isRunning())
  {
    qWarning() << Q_FUNC_INFO << "Web server already stopped";
    return;
  }

  emit webserverStatusChanged(false);

  mapController->deInit();

  if(listener != nullptr)
    listener->close();

  delete listener;
  listener = nullptr;

  delete requestHandler;
  requestHandler = nullptr;

  WebApp::deinit();
}

void WebController::openPage()
{
  if(listener != nullptr && listener->isListening())
  {
    QUrl url;
    QHostAddress addr = listener->serverAddress();
    if(addr == QHostAddress::Any || addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6)
      url.setHost("localhost");
    else
      url.setHost(QHostInfo::fromName(addr.toString()).hostName());
    url.setPort(listener->serverPort());
    url.setScheme("http");

    atools::gui::HelpHandler::openUrl(parentWidget, url);
  }
  else
    qWarning() << Q_FUNC_INFO << "Not listening";
}

bool WebController::isRunning() const
{
  return listener != nullptr && listener->isListening();
}
