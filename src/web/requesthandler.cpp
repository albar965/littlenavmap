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

#include "web/requesthandler.h"

#include "httpserver/staticfilecontroller.h"
#include "httpserver/httpsessionstore.h"
#include "httpserver/httpsession.h"
#include "templateengine/templatecache.h"
#include "mapgui/mappaintwidget.h"
#include "app/navapp.h"
#include "info/infocontroller.h"
#include "route/routecontroller.h"
#include "web/webmapcontroller.h"
#include "webapi/webapicontroller.h"
#include "web/webtools.h"
#include "web/webapp.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "common/htmlinfobuilder.h"
#include "util/htmlbuilder.h"
#include "gui/helphandler.h"
#include "common/constants.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QPainter>
#include <QtWidgets/QApplication>

using namespace stefanfrings;

RequestHandler::RequestHandler(QObject *parent, WebMapController *webMapController,WebApiController *webApiController,
                               HtmlInfoBuilder *htmlInfoBuilderParam, bool verboseParam)
  : HttpRequestHandler(parent), webApiController(webApiController), htmlInfoBuilder(htmlInfoBuilderParam), verbose(verboseParam)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;

  /* Fetch data through methods asynchronously to separate the call from this thread and run it in the main thread.
   * It has to wait for the main event queue to finish the request but saves a lot of synchronization through mutexes. */
  connect(this, &RequestHandler::getUserAircraft,
          NavApp::getMapPaintWidgetGui(), &MapPaintWidget::getUserAircraft, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getRoute,
          NavApp::getRouteController(),
          static_cast<const Route& (RouteController::*)() const>(&RouteController::getRouteConst),
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getFlightplanTableAsHtml,
          NavApp::getRouteController(), &RouteController::getFlightplanTableAsHtml, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getAirportText,
          NavApp::getInfoController(), &InfoController::getAirportTextFull, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getCurrentMapWidgetPos,
          NavApp::getMapPaintWidgetGui(), &MapPaintWidget::getCurrentViewCenterPos, Qt::BlockingQueuedConnection);

  connect(this, &RequestHandler::getPixmap, webMapController, &WebMapController::getPixmap,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapObject, webMapController, &WebMapController::getPixmapObject,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapPosDistance, webMapController, &WebMapController::getPixmapPosDistance,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapRect, webMapController, &WebMapController::getPixmapRect,
          Qt::BlockingQueuedConnection);

  /* Connect WebApiController to serviceWebApi signal */
  connect(this,&RequestHandler::serviceWebApi, webApiController, &WebApiController::service,Qt::BlockingQueuedConnection);
}

RequestHandler::~RequestHandler()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;
}

void RequestHandler::service(HttpRequest& request, HttpResponse& response)
{
  QString path = QString::fromUtf8(request.getPath());

  if(verbose)
    qDebug() << "RequestHandler::service(): path" << path << request.getMethod()
             << "header" << endl << request.getHeaderMap()
             << "parameter" << endl << request.getParameterMap();

  if(path == QLatin1String("/mapimage"))
    // ===========================================================================
    // Requests for map images only - either with or without session
    handleMapImage(request, response);
  else if(path.startsWith(webApiController->webApiPathPrefix))
    // ===========================================================================
    // Requests for web api - either with or without session
    handleWebApiRequest(request, response);
  else
  {
    Parameter params(request);

    HttpSession session = getSession(request, response);
    if(path == QLatin1String("/refresh"))
    {
      // ===========================================================================
      // Remember the refresh values in the session. This is called when changing the refresh drop down boxes and
      // is used to keep the value when the page is reloaded

      if(params.has(QStringLiteral(u"aircraftrefresh")))
        session.set("aircraftrefresh", params.asInt(QStringLiteral(u"aircraftrefresh")));
      if(params.has(QStringLiteral(u"flightplanrefresh")))
        session.set("flightplanrefresh", params.asInt(QStringLiteral(u"flightplanrefresh")));
      if(params.has(QStringLiteral(u"maprefresh")))
        session.set("maprefresh", params.asInt(QStringLiteral(u"maprefresh")));
      if(params.has(QStringLiteral(u"progressrefresh")))
        session.set("progressrefresh", params.asInt(QStringLiteral(u"progressrefresh")));
    }
    else if(path == QLatin1String("/plugins"))
    {
      response.setHeader("Content-Type", "text/plain");
      response.write(QDir(WebApp::getDocroot() + path).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::LocaleAware).join("/").toUtf8(), true);
    }
    else // all other paths
    {
      if(!path.contains(QStringLiteral(u"..")))
      {

        // mapcmd are for path /mapimage

        if(!request.getParameter("airportident").isEmpty())
          // Remember the ident in the session. This is used to keep the value when the page is reloaded.
          session.set("airport_ident", request.getParameter("airportident"));

        // Build file url
        const QString docroot = WebApp::getDocroot();
        const QString extension = WebApp::getHtmlExtension();

        QString file = path;
        QFileInfo fi(docroot + path);

        if(verbose)
          qDebug() << Q_FUNC_INFO << "file path" << fi.filePath();

        if(fi.exists())
        {
          if(fi.isDir())
          {
            // is a directory - use index.html
            file = file + (file.endsWith("/") ? QLatin1String("") : QStringLiteral(u"/")) + QStringLiteral(u"index") + extension;
            fi = QFileInfo(docroot + QStringLiteral(u"/") + file);
          }

          if(fi.filePath().startsWith(docroot))
          {
            // Check if path is not outside the document root for whatever reason for security

            // ===========================================================================
            // Serve file using templates for .html and static for the rest

            if(file.endsWith(extension))
            {
              handleHtmlFileRequest(request, response, session, file, extension);
            }
            else
              // ===========================================================================
              // Serve all files not ending with .html as is (css, svg, etc.)
              WebApp::getStaticFileController()->service(request, response);
          }
          else
            showError(request, response, 403, QStringLiteral(u"Forbidden."));
        }
        else
          // File or folder does not exist
          showError(request, response, 404, QStringLiteral(u"Not found."));
      }
      else
        // Not allowed to go parent for security
        showError(request, response, 403, QStringLiteral(u"Forbidden."));
    } // else other paths
  } // else mapimage
}

inline void RequestHandler::handleMapImage(HttpRequest& request, HttpResponse& response)
{
  Parameter params(request);

  // Extract values from parameter list ===========================================
  int width = params.asInt(QStringLiteral(u"width"), 0);
  int height = params.asInt(QStringLiteral(u"height"), 0);

  MapPixmap mapPixmap;

  if(params.has("session"))
  {
    // ===========================================================================
    // Stateful handling using a session which has the last zoom and position
    HttpSession session = getSession(request, response);

    // Session already contains distance and position values from an earlier call
    // Values are also initialized from visible map display when creating session

    // Distance as KM
    float requestedDistanceKm;
    float requestedDistance = params.asFloat(QStringLiteral(u"distance"), -1.0f);
    if(requestedDistance == -1.0f) {
      requestedDistanceKm = session.get("requested_distance").toFloat();
    }
    else
    {
      requestedDistanceKm = atools::geo::nmToKm(requestedDistance);
    }

    QString mapcmd = params.asStr(QStringLiteral(u"mapcmd"), QLatin1String(""));

    if(mapcmd == QLatin1String("user"))
      // Show user aircraft
      mapPixmap = emit getPixmapObject(width, height, web::USER_AIRCRAFT, QLatin1String(""), requestedDistanceKm);
    else if(mapcmd == QLatin1String("route"))
      // Center flight plan
      mapPixmap = emit getPixmapObject(width, height, web::ROUTE, QLatin1String(""), requestedDistanceKm);
    else if(mapcmd == QLatin1String("airport"))
      // Show an airport by ident
      mapPixmap = emit getPixmapObject(width, height, web::AIRPORT, params.asStr(
                                         QStringLiteral(u"airport")).toUpper(), requestedDistanceKm);
    else
    {
        // When zooming in or out use the last corrected distance (i.e. actual distance) as a base
        // Zoom or move map
        mapPixmap = emit getPixmapPosDistance(width, height,
                                              atools::geo::Pos(session.get("lon").toFloat(),
                                                               session.get("lat").toFloat()),
                                              (mapcmd == QLatin1String("in") || mapcmd == QLatin1String("out")) ?
                                                                       session.get("corrected_distance").toFloat() : requestedDistanceKm, mapcmd);
    }

    if(mapPixmap.hasNoError())
    {
      // Push results from last map call into session
      session.set("requested_distance", QVariant(mapPixmap.requestedDistanceKm));
      session.set("corrected_distance", QVariant(mapPixmap.correctedDistanceKm));
      session.set("lon", mapPixmap.pos.getLonX());
      session.set("lat", mapPixmap.pos.getLatY());
    }
    else
      return showErrorPixmap(response, width, height, 404, mapPixmap.error);
  }
  else
  {
    // Distance as KM
    float requestedDistanceKm = atools::geo::nmToKm(params.asFloat(QStringLiteral(u"distance"), 32.0f));     // set default as value which JS delivers as default on opening from default HTML value

    // ============================================================================
    // Session-less / state-less calls ============================================
    if(params.has(QStringLiteral(u"user")))
      // User aircraft =======================
      mapPixmap = emit getPixmapObject(width, height, web::USER_AIRCRAFT, QLatin1String(""), requestedDistanceKm);
    else if(params.has(QStringLiteral(u"route")))
      // Center flight plan =======================
      mapPixmap = emit getPixmapObject(width, height, web::ROUTE, QLatin1String(""), requestedDistanceKm);
    else if(params.has(QStringLiteral(u"airport")))
      // Show airport =======================
      mapPixmap = emit getPixmapObject(width, height, web::AIRPORT, params.asStr("airport"), requestedDistanceKm);
    else if(params.has(QStringLiteral(u"leftlon")) && params.has(QStringLiteral(u"toplat")) && params.has(QStringLiteral(u"rightlon")) && params.has(QStringLiteral(u"bottomlat")))
    {
      // Show rectangle =======================
      atools::geo::Rect rect(params.asFloat(QStringLiteral(u"leftlon")), params.asFloat(QStringLiteral(u"toplat")),
                             params.asFloat(QStringLiteral(u"rightlon")), params.asFloat(QStringLiteral(u"bottomlat")));
      mapPixmap = emit getPixmapRect(width, height, rect);
    }
    else if(params.has(QStringLiteral(u"distance")) || (params.has(QStringLiteral(u"lon")) && params.has(QStringLiteral(u"lat"))))
    {
      // Show position =======================
      atools::geo::Pos pos;
      if(params.has(QStringLiteral(u"lon")) && params.has(QStringLiteral(u"lat")))
      {
        pos.setLonX(params.asFloat(QStringLiteral(u"lon")));
        pos.setLatY(params.asFloat(QStringLiteral(u"lat")));
      }

      mapPixmap = emit getPixmapPosDistance(width, height, pos, requestedDistanceKm, QLatin1String(""));
    }
    else
      // Show current map view =======================
      mapPixmap = emit getPixmap(width, height);

    if(mapPixmap.hasError())
      // Show error message as image
      return showErrorPixmap(response, width, height, 404, mapPixmap.error);
  }

  if(mapPixmap.isValid())
  {
    // ===========================================================================
    // Write pixmap as image
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);

    int quality = params.asInt(QStringLiteral(u"quality"), -1);

    // Image format, jpg is default and only jpg and png allowed ===========================================
    QString format = params.asEnum(QStringLiteral(u"format"), QStringLiteral(u"jpg"), {QStringLiteral(u"jpg"), QStringLiteral(u"png")});

    if(format == QLatin1String("jpg"))
    {
      response.setHeader("Content-Type", "image/jpeg");
      mapPixmap.pixmap.save(&buffer, "JPG", quality);
    }
    else if(format == QLatin1String("png"))
    {
      response.setHeader("Content-Type", "image/png");
      mapPixmap.pixmap.save(&buffer, "PNG", quality);
    }
    else
      // Should never happen
      qWarning() << Q_FUNC_INFO << "invalid format";

    response.write(bytes);
  }
  else
    // Show error message as image
    showErrorPixmap(response, width, height, 404, QStringLiteral(u"invalid pixmap"));
}


inline void RequestHandler::handleWebApiRequest(HttpRequest& request, HttpResponse& response)
{
  // Map API request
  WebApiRequest apiRequest;

  /* Remove common webApiPathPrefix from apiRequest path */
  apiRequest.path = request.getPath().remove(0, webApiController->webApiPathPrefix.length());

  apiRequest.method = request.getMethod();
  apiRequest.headers = request.getHeaderMap();
  apiRequest.parameters = request.getParameterMap();
  apiRequest.body = request.getBody();

  // Call API in-sync
  WebApiResponse result = emit serviceWebApi(apiRequest);

  // Map API response
  response.setStatus(result.status);
  QMultiMap<QByteArray, QByteArray>::iterator i;
  for (auto it = result.headers.constBegin(); it != result.headers.constEnd(); ++it)
      response.setHeader(it.key(),it.value());

  // Write output
  response.write(result.body, true);
}


inline void RequestHandler::handleHtmlFileRequest(HttpRequest& request, HttpResponse& response, HttpSession& session, QString& file, const QString& extension)
{

    if(verbose)
      qDebug() << Q_FUNC_INFO << " for " << file;

    // Use all .html files as templates - template handler wants file without extension
    file.chop(extension.size());

    // Get HTML template from cache
    response.setHeader("Content-Type", "text/html; charset=UTF-8");
    Template t = WebApp::getTemplateCache()->getTemplate(file, request.getHeader("Accept-Language"));
    QString errMessage;

    if(!t.isEmpty())
    {
      if(verbose)
        t.enableWarnings();

      // Set general variables ==============================
      t.setVariable(QStringLiteral(u"applicationName"), QApplication::applicationName());
      t.setVariable(QStringLiteral(u"applicationVersion"), QApplication::applicationVersion());
      t.setVariable(QStringLiteral(u"helpUrl"), atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineUrl + QStringLiteral(u"WEBSERVER.html"),
                                                                       lnm::helpLanguageOnline()).toString());

      // Put refresh values back in page by inserting select control ==============================
      if(t.contains(QStringLiteral(u"{aircraftrefreshsel}")))
        t.setVariable(QStringLiteral(u"aircraftrefreshsel"),
                      buildRefreshSelect(session.get("aircraftrefresh").toInt()));      // does session entry exist?

      if(t.contains(QStringLiteral(u"{flightplanrefreshsel}")))
        t.setVariable(QStringLiteral(u"flightplanrefreshsel"),
                      buildRefreshSelect(session.get("flightplanrefresh").toInt()));    // does session entry exist?

      if(t.contains(QStringLiteral(u"{maprefreshsel}")))
        t.setVariable(QStringLiteral(u"maprefreshsel"),
                      buildRefreshSelect(session.get("maprefresh").toInt()));           // does session entry exist?

      if(t.contains(QStringLiteral(u"{progressrefreshsel}")))
        t.setVariable(QStringLiteral(u"progressrefreshsel"),
                      buildRefreshSelect(session.get("progressrefresh").toInt()));      // does session entry exist?

      // ===========================================================================
      // Aircraft registration, weight, etc.
      atools::util::HtmlBuilder html(mapcolors::webTableBackgroundColor, mapcolors::webTableAltBackgroundColor);

      atools::fs::sc::SimConnectUserAircraft userAircraft;
      if(t.contains(QStringLiteral(u"{aircraftProgressText}")) || t.contains(QStringLiteral(u"{aircraftText}")))
        userAircraft = emit getUserAircraft();

      if(t.contains(QStringLiteral(u"{aircraftText}")))
      {
        html.clear();
        htmlInfoBuilder->aircraftText(userAircraft, html);
        htmlInfoBuilder->aircraftTextWeightAndFuel(userAircraft, html);
        t.setVariable(QStringLiteral(u"aircraftText"), html.getHtml());
      }

      // ===========================================================================
      // Aircraft progress
      if(t.contains(QStringLiteral(u"{aircraftProgressText}")))
      {
        Route route = emit getRoute();
        html.clear();

        // Additional required progress fields are defined in aircraftprogressconfig.cpp in vector ADDITIONAL_WEB_IDS
        html.setIdBits(NavApp::getInfoController()->getEnabledProgressBitsWeb());

        htmlInfoBuilder->aircraftProgressText(userAircraft, html, route);
        t.setVariable(QStringLiteral(u"aircraftProgressText"), html.getHtml());
      }

      // ===========================================================================
      // Flight plan
      if(t.contains(QStringLiteral(u"{flightplanText}")))
        t.setVariable(QStringLiteral(u"flightplanText"), emit getFlightplanTableAsHtml(20, false));

      // ===========================================================================
      // Airport information
      if(t.contains(QStringLiteral(u"{airportText}")))
      {
        // Reload airport ident from session
        QString ident = session.get("airport_ident").toString().toUpper();

        if(ident.isEmpty())
          ident = request.getParameter("airportident");

        if(!ident.isEmpty())
        {
          // Get airport information as HTML in the string list. Order is main, runway, com, procedure and weather.
          QStringList airportTexts = emit getAirportText(ident);

          if(airportTexts.size() == 5)
          {
            t.setCondition(QStringLiteral(u"hasAirport"), true);
            t.setCondition(QStringLiteral(u"hasError"), false);

            t.setVariable(QStringLiteral(u"airportText"), airportTexts.at(0));
            t.setVariable(QStringLiteral(u"airportRunwayText"), airportTexts.at(1));
            t.setVariable(QStringLiteral(u"airportComText"), airportTexts.at(2));
            t.setVariable(QStringLiteral(u"airportProcedureText"), airportTexts.at(3));
            t.setVariable(QStringLiteral(u"airportWeatherText"), airportTexts.at(4));
          }
          else
          {
            // Error - not found
            t.setCondition(QStringLiteral(u"hasAirport"), false);
            errMessage = tr("No airport found for %1.").arg(ident);
          }
        }
        else
          // Nothing to display - leave page empty
          t.setCondition(QStringLiteral(u"hasAirport"), false);
      }

      if(!errMessage.isEmpty())
      {
        // No airport found - display error message ==========
        t.setCondition(QStringLiteral(u"hasError"), true);
        t.setVariable(QStringLiteral(u"errorText"), errMessage);
      }
      else
        t.setCondition(QStringLiteral(u"hasError"), false);

      // ===========================================================================
      // Write resonse
      response.write(t.toUtf8(), true);
    }
    else
      showError(request, response, 500, QStringLiteral(u"Internal server error. Template empty."));
}

void RequestHandler::showErrorPixmap(HttpResponse& response, int width, int height, int status, const QString& text)
{
  qWarning() << Q_FUNC_INFO << "Error" << status << text;

  // Create pixmap
  QPixmap pixmap(width, height);
  pixmap.fill(QColor(Qt::white));

  // Prepare painter and font
  QPainter painter(&pixmap);
  QFont font = painter.font();
  font.setPixelSize(std::min(height / 10, 20));
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(Qt::red);

  // Paint text centered with a w/h-tenth distance to borders
  QString err = tr("Error %1 %2\nClick or reload to continue.").arg(status).arg(text);
  QRect textRect(width / 10, height / 10, width - width / 5, height - height / 5);

  painter.drawText(textRect, Qt::AlignCenter | Qt::AlignVCenter | Qt::TextDontClip | Qt::TextWordWrap, err);

  // Save image as JPG
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  pixmap.save(&buffer, "JPG", 100);

  // Write to response
  response.setHeader("Content-Type", "image/jpeg");
  response.setStatus(status);
  response.write(bytes);
}

void RequestHandler::showError(HttpRequest& request, HttpResponse& response, int status, const QString& text)
{
  qWarning() << Q_FUNC_INFO << "Error" << status << text;

  // Get error template and fill it ======================
  Template t = WebApp::getTemplateCache()->getTemplate("error", request.getHeader("Accept-Language"));
  t.setVariable("applicationName", QApplication::applicationName());
  t.setVariable("applicationVersion", QApplication::applicationVersion());
  t.setVariable("statuscode", QString::number(status));
  t.setVariable("statustext", text);

  // Write to response
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  response.setStatus(status);
  response.write(t.toUtf8(), true);
}

stefanfrings::HttpSession RequestHandler::getSession(HttpRequest& request, HttpResponse& response)
{
  HttpSession session = WebApp::getSessionStore()->getSession(request, response);
  if(session.contains("session_populated"))
  {
    if(verbose)
      qInfo() << Q_FUNC_INFO << "Found session" << session.getAll();
    return session;
  }
  else
  {
    // Session does not exist - initialize with defaults from current map view
    atools::geo::Pos pos = emit getCurrentMapWidgetPos();
    session.set("lon", pos.getLonX());
    session.set("lat", pos.getLatY());
    session.set("requested_distance", QVariant(atools::geo::nmToKm(32.0f)));             // 32.0 is the default JS delivers from new web ui HTML default
    session.set("corrected_distance", QVariant(atools::geo::nmToKm(32.0f)));             // 32.0 is the default JS delivers from new web ui HTML default
    session.set("session_populated", true);
    if(verbose)
      qInfo() << Q_FUNC_INFO << "Created session" << session.getAll();
    return session;
  }
}

QString RequestHandler::buildRefreshSelect(int defaultValue)
{
  // Build the dowp down box to insert into the form.
  // Needed since there is no easy way in JS to set the default
  static const QVector<std::pair<int, QString> > rates(
  {
    {0, tr("Manual Reload")},
    {1, tr("1 Second")},
    {2, tr("2 Seconds")},
    {5, tr("5 Seconds")},
    {15, tr("15 Seconds")},
    {30, tr("30 Seconds")},
    {60, tr("60 Seconds")},
    {120, tr("120 Seconds")}
  });

  QString retval;
  retval.append("<select id=\"refreshselect\" onchange=\"refreshPage()\">");
  for(const std::pair<int, QString>& rate : rates)
    retval.append(QString("<option value=\"%1\" %2>%3</option>").
                  arg(rate.first).arg(rate.first == defaultValue ? "selected" : QString()).arg(rate.second));
  retval.append("</select>");
  return retval;
}
