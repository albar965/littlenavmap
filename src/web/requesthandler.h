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

#ifndef LNM_REQUESTMAPPER_H
#define LNM_REQUESTMAPPER_H

#include "httpserver/httprequesthandler.h"

#include "web/webflags.h"
#include "web/webmapcontroller.h"

#include <QPixmap>

#include "geo/pos.h"
#include "geo/rect.h"
#include "route/route.h"

namespace stefanfrings {
class StaticFileController;
class TemplateCache;
class HttpSession;
}

class HtmlInfoBuilder;

/*
 * Handles all HTTP server requests including stateless and stateful. Maintains a session for the stateful page.
 * The method service() is called in a separate thread.
 *
 * Uses global session and file caches from WebApp class.
 */
class RequestHandler :
  public stefanfrings::HttpRequestHandler
{
  Q_OBJECT
  Q_DISABLE_COPY(RequestHandler)

public:
  /* Prepare connections to other objects. Handler is ready to accept connections when instantiated. */
  RequestHandler(QObject *parent, WebMapController *webMapController, HtmlInfoBuilder *htmlInfoBuilderParam,
                 bool verboseParam);
  virtual ~RequestHandler() override;

  /* Doing all the work right here. */
  void service(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response) override;

signals:
  /* Calls to the MapPaintWidget have to run in the main event queue and thread.
   * Therefore, it is necessary to use queued signals to separate
   * a thread from the HTTP server.*/
  MapPixmap getPixmap(int width, int height);
  MapPixmap getPixmapObject(int width, int height, web::ObjectType type, QString ident, float distanceKm);
  MapPixmap getPixmapPosDistance(int width, int height, atools::geo::Pos pos, float distanceKm, QString mapCommand);
  MapPixmap getPixmapRect(int width, int height, atools::geo::Rect rect);

  atools::fs::sc::SimConnectUserAircraft getUserAircraft();
  Route getRoute();
  QString getFlightplanTableAsHtml(int iconSize, bool print);
  QStringList getAirportText(QString ident);
  atools::geo::Pos getCurrentMapWidgetPos();

private:
  /* fetch parameters as a string map from request */
  QHash<QString, QString> parameters(stefanfrings::HttpRequest& request) const;

  /* Show error using the error.html template and return set it into the response. */
  void showError(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response, int status,
                 const QString& text);

  /* Show error using a JPG image for image requests and return set it into the response. */
  void showErrorPixmap(stefanfrings::HttpResponse& response, int width, int height, int status, const QString& text);

  /* Handle stateful and stateless map image requests. */
  void handleMapImage(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response);

  /* Build the select dropdown box HTML code with the default value pre-selected. */
  QString buildRefreshSelect(int defaultValue);

  /* Create and prepare a session and set the cookie or return current session */
  stefanfrings::HttpSession getSession(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response);

  HtmlInfoBuilder *htmlInfoBuilder;

  bool verbose = false;
};

#endif // LNM_REQUESTMAPPER_H
