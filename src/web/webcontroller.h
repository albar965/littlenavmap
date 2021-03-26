/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_WEBCONTROLLER_H
#define LNM_WEBCONTROLLER_H

#include "io/inireader.h"

#include <QObject>
#include <QUrl>
#include <QVector>

namespace stefanfrings {
class HttpListener;
}

class RequestHandler;
class WebMapController;
class HtmlInfoBuilder;
class QSettings;

/*
 * Facade that hides the internal HTTP web server and keeps track of all global caches and the listener.
 *
 * Reads configuration from :/littlenavmap/resources/config/webserver.cfg or settings path.
 */
class WebController :
  public QObject
{
  Q_OBJECT

public:
  /* Create instance and load configuration files */
  explicit WebController(QWidget *parent = nullptr);
  virtual ~WebController() override;

  /* Start the server if not already done. Ignored if already runnging. */
  void startServer();

  /* Stop server if running. Ignored if already stopped. */
  void stopServer();
  void restartServer(bool force);

  /* Open the server address in the default web browser */
  void openPage();

  /* True if server is listening */
  bool isRunning() const;

  /* Get the default url. Usually hostname and port or IP and port as fallback. */
  QUrl getUrl(bool useIpAddress) const
  {
    return useIpAddress ? urlIpList.value(0) : urlList.value(0);
  }

  /* Get list of bound URLs (IPs) for display */
  QStringList getUrlStr() const;

  /* Update settings and probably restart server. */
  void optionsChanged();

  /* Update settings from option data but do not restart. Returns true if any changes. */
  bool updateSettings();

  /* Get canonical path of document root with system separators for display */
  QString getAbsoluteWebrootFilePath() const;

  void setDocumentRoot(const QString& value)
  {
    documentRoot = value;
  }

  void setPort(int value)
  {
    port = value;
  }

  QString getDefaultDocumentRoot() const;

  bool isEncrypted() const
  {
    return encrypted;
  }

  void setEncrypted(bool value)
  {
    encrypted = value;
  }

signals:
  /* Send after server is started or before server is shutdown */
  void webserverStatusChanged(bool running);

private:
  stefanfrings::HttpListener *listener = nullptr;

  /* Map painter */
  WebMapController *mapController = nullptr;

  /* Handles all HTTP requests using templates or static */
  RequestHandler *requestHandler = nullptr;

  /* Used to build airport and other HTML information texts. */
  HtmlInfoBuilder *htmlInfoBuilder = nullptr;

  /* Configuration file and file name. Default is :/littlenavmap/resources/config/webserver.cfg */
  atools::io::IniKeyValues listenerSettings;
  QString configFileName;

  /* Full canonical path containing only "/" as separator */
  QString documentRoot;
  int port = 8965;

  /* use HTTPS / SSL */
  bool encrypted = false;

  QWidget *parentWidget;
  bool verbose = false;

  QVector<QUrl> urlList, urlIpList;

  /* Remember custom certifiates. */
  QString sslKeyFile, sslCertFile;
};

#endif // LNM_WEBCONTROLLER_H
