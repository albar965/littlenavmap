/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include <QHash>
#include <QObject>
#include <QVector>

namespace stefanfrings {
class HttpListener;
}

class QHostAddress;
class RequestHandler;
class WebMapController;
class WebApiController;
class HtmlInfoBuilder;
class QSettings;
struct Host;

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

  WebController(const WebController& other) = delete;
  WebController& operator=(const WebController& other) = delete;

  /* Start the server if not already done. Ignored if already runnging. */
  void startServer();

  /* Stop server if running. Ignored if already stopped. */
  void stopServer();
  void restartServer(bool force);

  /* Open the server address in the default web browser */
  void openPage();

  /* True if server is listening */
  bool isListenerRunning() const;

  /* Get the default url. Usually hostname and port or IP and port as fallback.
   * Hostname lookup is slow. */
  QUrl getUrl(bool useIpAddress);

  /* Get list of bound URLs (IPs) for display.
   * Hostname lookup is slow.  */
  QStringList getUrlStr();

  /* Update settings and probably restart server. */
  void optionsChanged();

  /* Update settings from option data but do not restart. Returns true if any changes. */
  bool updateSettings();

  void setDocumentRoot(const QString& value)
  {
    documentRoot = value;
  }

  const QString& getDocumentRoot() const
  {
    return documentRoot;
  }

  void setPort(int value)
  {
    port = value;
  }

  int getPort() const
  {
    return port;
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

  WebMapController *getWebMapController() const;

  /* Need to clear caches and tear down queries in map widget before switching database */
  void preDatabaseLoad();

  /* Initialize queries again after a database change */
  void postDatabaseLoad();

signals:
  /* Send after server is started or before server is shutdown */
  void webserverStatusChanged(bool running);

private:
  /* Host name lookup using cache */
  QString hostName(const QHostAddress& hostAddr);

  stefanfrings::HttpListener *listener = nullptr;

  /* Map painter */
  WebMapController *mapController = nullptr;

  /* Web API controller */
  WebApiController *apiController = nullptr;

  /* Handles all HTTP requests using templates or static */
  RequestHandler *requestHandler = nullptr;

  /* Used to build airport and other HTML information texts. */
  HtmlInfoBuilder *htmlInfoBuilder = nullptr;

  /* Configuration file and file name. Default is :/littlenavmap/resources/config/webserver.cfg */
  QHash<QString, QVariant> listenerSettings;
  QString configFileName;

  /* Full canonical path containing only "/" as separator */
  QString documentRoot;
  int port = 8965;

  /* use HTTPS / SSL */
  bool encrypted = false;

  QWidget *parentWidget;
  bool verbose = false;

  /* Caches host names sorted by IPv4 and IPv6 */
  QVector<Host> hosts;

  /* Remember custom certifiates. */
  QString sslKeyFile, sslCertFile;
};

#endif // LNM_WEBCONTROLLER_H
