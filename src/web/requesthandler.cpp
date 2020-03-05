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
#include "navapp.h"
#include "info/infocontroller.h"
#include "route/routecontroller.h"
#include "web/webmapcontroller.h"
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
#include <QPainter>
#include <QtWidgets/QApplication>

using namespace stefanfrings;

RequestHandler::RequestHandler(QObject *parent, WebMapController *webMapController,
                               HtmlInfoBuilder *htmlInfoBuilderParam, bool verboseParam)
  : HttpRequestHandler(parent), htmlInfoBuilder(htmlInfoBuilderParam), verbose(verboseParam)
{
  qDebug() << Q_FUNC_INFO;

  /* Fetch data through methods asynchronously to separate the call from this thread and run it in the main thread.
   * It has to wait for the main event queue to finish the request but saves a lot of synchronization through mutexes. */
  connect(this, &RequestHandler::getUserAircraft,
          NavApp::getMapPaintWidget(), &MapPaintWidget::getUserAircraft, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getRoute,
          NavApp::getRouteController(),
          static_cast<const Route& (RouteController::*)() const>(&RouteController::getRoute),
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getFlightplanTableAsHtml,
          NavApp::getRouteController(), &RouteController::getFlightplanTableAsHtml, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getAirportText,
          NavApp::getInfoController(), &InfoController::getAirportTextFull, Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getCurrentMapWidgetPos,
          NavApp::getMapPaintWidget(), &MapPaintWidget::getCurrentViewCenterPos, Qt::BlockingQueuedConnection);

  connect(this, &RequestHandler::getPixmap, webMapController, &WebMapController::getPixmap,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapObject, webMapController, &WebMapController::getPixmapObject,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapPosDistance, webMapController, &WebMapController::getPixmapPosDistance,
          Qt::BlockingQueuedConnection);
  connect(this, &RequestHandler::getPixmapRect, webMapController, &WebMapController::getPixmapRect,
          Qt::BlockingQueuedConnection);
}

RequestHandler::~RequestHandler()
{
  qDebug() << Q_FUNC_INFO;
}

void RequestHandler::service(HttpRequest& request, HttpResponse& response)
{
  QString path = QString::fromUtf8(request.getPath());

  qDebug() << Q_FUNC_INFO << "path" << path << request.getMethod()
           << "header" << endl << request.getHeaderMap()
           << "parameter" << endl << request.getParameterMap();

  if(path.contains(".."))
  {
    // Not allowed to go parent for security
    showError(request, response, 403, "Forbidden.");
    return;
  }

  Parameter params(request);
  if(path == "/mapimage")
    // ===========================================================================
    // Requests for map images only - either with or without session
    handleMapImage(request, response);
  else
  {
    HttpSession session = getSession(request, response);
    if(path == "/refresh")
    {
      // ===========================================================================
      // Remember the refresh values in the session. This is called when changing the refresh drop down boxes and
      // is used to keep the value when the page is reloaded
      session = getSession(request, response);

      if(params.has("aircraftrefresh"))
        session.set("aircraftrefresh", params.asInt("aircraftrefresh"));
      if(params.has("flightplanrefresh"))
        session.set("flightplanrefresh", params.asInt("flightplanrefresh"));
      if(params.has("maprefresh"))
        session.set("maprefresh", params.asInt("maprefresh"));
      if(params.has("progressrefresh"))
        session.set("progressrefresh", params.asInt("progressrefresh"));
    }
    else // all other paths
    {
      if(params.has("mapcmd"))
        // All map commands like "in", "out", "left" or "right" need a session
        getSession(request, response).set("mapcmd", params.asStr("mapcmd"));

      if(!request.getParameter("airportident").isEmpty())
        // Remember the ident in the session. This is used to keep the value when the page is reloaded.
        session.set("airport_ident", request.getParameter("airportident"));

      // Build file url
      const QString docroot = WebApp::getDocroot();
      const QString extension = WebApp::getHtmlExtension();

      QString file = path;
      QFileInfo fi(docroot + path);

      if(fi.exists() && fi.isDir())
      {
        // is a directory - use index.html
        file = file + (file.endsWith("/") ? QString() : "/") + "index" + extension;
        fi = QFileInfo(docroot + "/" + file);
      }

      qDebug() << Q_FUNC_INFO << "file path" << fi.filePath();

      if(!fi.filePath().startsWith(docroot))
      {
        // Check if path is outside the document root for whatever reason for security
        showError(request, response, 403, "Forbidden.");
        return;
      }

      if(fi.exists())
      {
        // ===========================================================================
        // Serve file using templates for .html and static for the rest

        if(file.endsWith(extension))
        {
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
            t.setVariable("applicationName", QApplication::applicationName());
            t.setVariable("applicationVersion", QApplication::applicationVersion());
            t.setVariable("helpUrl", atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineUrl + "WEBSERVER.html",
                                                                             lnm::helpLanguageOnline()).toString());

            // Put refresh values back in page by inserting select control ==============================
            if(t.contains("{aircraftrefreshsel}"))
              t.setVariable("aircraftrefreshsel",
                            buildRefreshSelect(getSession(request, response).get("aircraftrefresh").toInt()));

            if(t.contains("{flightplanrefreshsel}"))
              t.setVariable("flightplanrefreshsel",
                            buildRefreshSelect(getSession(request, response).get("flightplanrefresh").toInt()));

            if(t.contains("{maprefreshsel}"))
              t.setVariable("maprefreshsel",
                            buildRefreshSelect(getSession(request, response).get("maprefresh").toInt()));

            if(t.contains("{progressrefreshsel}"))
              t.setVariable("progressrefreshsel",
                            buildRefreshSelect(getSession(request, response).get("progressrefresh").toInt()));

            // ===========================================================================
            // Aircraft registration, weight, etc.
            atools::util::HtmlBuilder html(mapcolors::webTableBackgroundColor, mapcolors::webTableAltBackgroundColor);

            atools::fs::sc::SimConnectUserAircraft userAircraft;
            if(t.contains("{aircraftProgressText}") || t.contains("{aircraftText}"))
              userAircraft = emit getUserAircraft();

            if(t.contains("{aircraftText}"))
            {
              html.clear();
              htmlInfoBuilder->aircraftText(userAircraft, html);
              htmlInfoBuilder->aircraftTextWeightAndFuel(userAircraft, html);
              t.setVariable("aircraftText", html.getHtml());
            }

            // ===========================================================================
            // Aircraft progress
            if(t.contains("{aircraftProgressText}"))
            {
              Route route = emit getRoute();
              html.clear();
              htmlInfoBuilder->aircraftProgressText(userAircraft, html, route,
                                                    false /* show more/less switch */, false /* less */);
              t.setVariable("aircraftProgressText", html.getHtml());
            }

            // ===========================================================================
            // Flight plan
            if(t.contains("{flightplanText}"))
              t.setVariable("flightplanText", emit getFlightplanTableAsHtml(20, false));

            // ===========================================================================
            // Airport information
            if(t.contains("{airportText}"))
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
                  t.setCondition("hasAirport", true);
                  t.setCondition("hasError", false);

                  t.setVariable("airportText", airportTexts.at(0));
                  t.setVariable("airportRunwayText", airportTexts.at(1));
                  t.setVariable("airportComText", airportTexts.at(2));
                  t.setVariable("airportProcedureText", airportTexts.at(3));
                  t.setVariable("airportWeatherText", airportTexts.at(4));
                }
                else
                {
                  // Error - not found
                  t.setCondition("hasAirport", false);
                  errMessage = tr("No airport found for %1.").arg(ident);
                }
              }
              else
                // Nothing to display - leave page empty
                t.setCondition("hasAirport", false);
            }

            if(!errMessage.isEmpty())
            {
              // No airport found - display error message ==========
              t.setCondition("hasError", true);
              t.setVariable("errorText", errMessage);
            }
            else
              t.setCondition("hasError", false);

            // ===========================================================================
            // Write resonse
            response.write(t.toUtf8(), true);
          }
          else
            showError(request, response, 500, "Internal server error. Template empty.");
        }
        else
          // ===========================================================================
          // Serve all files not ending with .html as is (css, svg, etc.)
          WebApp::getStaticFileController()->service(request, response);
      }
      else
        // File or folder does not exist
        showError(request, response, 404, "Not found.");
    } // else other paths
  } // else mapimage
}

void RequestHandler::handleMapImage(HttpRequest& request, HttpResponse& response)
{
  Parameter params(request);

  // Extract values from parameter list ===========================================
  int quality = params.asInt("quality", -1);
  int width = params.asInt("width", 0);
  int height = params.asInt("height", 0);

  // Image format, jpg is default and only jpg and png allowed ===========================================
  QString format = params.asEnum("format", "jpg", {"jpg", "png"});

  MapPixmap mapPixmap;

  // Distance as KM
  float requestedDistanceKm = atools::geo::nmToKm(params.asFloat("distance", 100.0f));

  if(params.has("session"))
  {
    // ===========================================================================
    // Stateful handling using a session which has the last zoom and position
    HttpSession session = getSession(request, response);

    if(session.contains("lon") && session.contains("lat") &&
       session.contains("requested_distance") && session.contains("corrected_distance"))
    {
      // Session already contains distance and position values from an earlier call
      // Values are also initialized from visible map display when creating session
      QString mapcmd = params.asStr("mapcmd");

      if(mapcmd == "user")
        // Show user aircraft
        mapPixmap = emit getPixmapObject(width, height, web::USER_AIRCRAFT, QString(), requestedDistanceKm);
      else if(mapcmd == "route")
        // Center flight plan
        mapPixmap = emit getPixmapObject(width, height, web::ROUTE, QString(), requestedDistanceKm);
      else if(mapcmd == "airport")
        // Show an airport by ident
        mapPixmap = emit getPixmapObject(width, height, web::AIRPORT, params.asStr(
                                           "airport").toUpper(), requestedDistanceKm);
      else
      {
        // When zooming in or out use the last corrected distance (i.e. actual distance) as a base
        float distance = (mapcmd == "in" || mapcmd == "out") ?
                         session.get("corrected_distance").toFloat() : session.get("requested_distance").toFloat();

        // Zoom or move map
        mapPixmap = emit getPixmapPosDistance(width, height,
                                              atools::geo::Pos(session.get("lon").toFloat(),
                                                               session.get("lat").toFloat()),
                                              distance, mapcmd);
      }

      if(!mapPixmap.hasError())
      {
        // Push results from last map call into session
        session.set("requested_distance", QVariant(mapPixmap.requestedDistanceKm));
        session.set("corrected_distance", QVariant(mapPixmap.correctedDistanceKm));
        session.set("lon", mapPixmap.pos.getLonX());
        session.set("lat", mapPixmap.pos.getLatY());
      }
    }
    else
      showError(request, response, 500, "Internal server error. Incomplete session.");
  }
  // ============================================================================
  // Session-less / state-less calls ============================================
  else if(params.has("user"))
    // User aircraft =======================
    mapPixmap = emit getPixmapObject(width, height, web::USER_AIRCRAFT, QString(), requestedDistanceKm);
  else if(params.has("route"))
    // Center flight plan =======================
    mapPixmap = emit getPixmapObject(width, height, web::ROUTE, QString(), requestedDistanceKm);
  else if(params.has("airport"))
    // Show airport =======================
    mapPixmap = emit getPixmapObject(width, height, web::AIRPORT, params.asStr("airport"), requestedDistanceKm);
  else if(params.has("leftlon") && params.has("toplat") && params.has("rightlon") && params.has("bottomlat"))
  {
    // Show rectangle =======================
    atools::geo::Rect rect(params.asFloat("leftlon"), params.asFloat("toplat"),
                           params.asFloat("rightlon"), params.asFloat("bottomlat"));
    mapPixmap = emit getPixmapRect(width, height, rect);
  }
  else if(params.has("distance") || (params.has("lon") && params.has("lat")))
  {
    // Show position =======================
    atools::geo::Pos pos;
    if(params.has("lon") && params.has("lat"))
    {
      pos.setLonX(params.asFloat("lon"));
      pos.setLatY(params.asFloat("lat"));
    }

    mapPixmap = emit getPixmapPosDistance(width, height, pos, requestedDistanceKm, QString());
  }
  else
    // Show current map view =======================
    mapPixmap = emit getPixmap(width, height);

  if(mapPixmap.isValid() && !mapPixmap.hasError())
  {
    // ===========================================================================
    // Write pixmap as image
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);

    if(format == "jpg")
    {
      response.setHeader("Content-Type", "image/jpeg");
      mapPixmap.pixmap.save(&buffer, "JPG", quality);
    }
    else if(format == "png")
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
    showErrorPixmap(response, width, height, 404, mapPixmap.error);
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
  if(!session.contains("lon") || !session.contains("lat"))
  {
    // Session does not exist - initialize with defaults from current map view
    atools::geo::Pos pos = emit getCurrentMapWidgetPos();
    session.set("requested_distance", pos.getAltitude());
    session.set("corrected_distance", pos.getAltitude());
    session.set("lon", pos.getLonX());
    session.set("lat", pos.getLatY());
    qInfo() << Q_FUNC_INFO << "Created session" << session.getAll();
  }
  else
    qInfo() << Q_FUNC_INFO << "Found session" << session.getAll();
  return session;
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
