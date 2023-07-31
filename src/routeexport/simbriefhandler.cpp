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

#include "routeexport/simbriefhandler.h"

#include "common/constants.h"
#include "common/unit.h"
#include "fs/perf/aircraftperf.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "gui/mainwindow.h"
#include "app/navapp.h"
#include "route/route.h"
#include "routeexport/fetchroutedialog.h"
#include "routestring/routestringwriter.h"
#include "settings/settings.h"

#include <QUrlQuery>
#include <QWidget>
#include <QClipboard>

SimBriefHandler::SimBriefHandler(MainWindow *parent) :
  QObject(parent), mainWindow(parent)
{
  // Allow user to override URL
  dispatchUrl = atools::settings::Settings::instance().valueStr(lnm::ROUTE_EXPORT_SIMBRIEF_DISPATCH_URL,
                                                                "https://www.simbrief.com/system/dispatch.php");
}

SimBriefHandler::~SimBriefHandler()
{
}

void SimBriefHandler::sendRouteToSimBrief()
{
  qDebug() << Q_FUNC_INFO;
  const Route& route = NavApp::getRouteConst();

  // Create route description string ================================
  QString routeString = RouteStringWriter().createStringForRoute(route, 0.f, rs::SIMBRIEF_WRITE_DEFAULTS);
  QString aircraftType = NavApp::getAircraftPerformance().getAircraftType();

  // Ask user ================================
  const static atools::gui::DialogButtonList BUTTONS({
    atools::gui::DialogButton(QString(), QMessageBox::Cancel),
    atools::gui::DialogButton(tr("&Export"), QMessageBox::Yes),
    atools::gui::DialogButton(tr("&Help"), QMessageBox::Help),
    atools::gui::DialogButton(tr("&Copy web address to clipboard"), QMessageBox::YesToAll)
  });

  QString message = tr("<p><b>Export this flight plan to SimBrief?</b></p>"
                         "<p>The information below will be sent:</p>"
                           "<table><tbody><tr><td>Route description:</td><td>%1</td></tr>"
                             "<tr><td>Cruise altitide:</td><td>%2</td></tr>"
                               "<tr><td>Aircraft type:</td><td>%3</td></tr></tbody></table>"
                                 "<p>Open your web browser and log into SimBrief before exporting the flight plan.</p>").
                    arg(routeString).arg(Unit::altFeet(route.getCruiseAltitudeFt())).arg(aircraftType);

  int result = atools::gui::Dialog(mainWindow).showQuestionMsgBox(lnm::ACTIONS_SHOW_SEND_SIMBRIEF, message,
                                                                  tr("Do not &show this dialog again and "
                                                                     "open address in the browser instead."),
                                                                  BUTTONS, QMessageBox::Yes, QMessageBox::Yes);

  // Build URL ================================
  QUrl url = sendRouteUrl(route.getDepartureAirportLeg().getIdent(), route.getDestinationAirportLeg().getIdent(),
                          route.getAlternateIdents().value(0), // Send only first alternate
                          routeString, aircraftType, route.getCruiseAltitudeFt());

  qDebug() << Q_FUNC_INFO << "Encoded full URL" << url.toEncoded();

  if(result == QMessageBox::Yes)
  {
    atools::gui::HelpHandler::openUrl(mainWindow, url);
    NavApp::setStatusMessage(QString(tr("SimBrief flight plan sent to web browser.")));
  }
  else if(result == QMessageBox::YesToAll)
  {
    QApplication::clipboard()->setText(url.toEncoded());
    NavApp::setStatusMessage(QString(tr("SimBrief address copied to clipboard.")));
  }
  else if(result == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "LOADSIMBRIEF.html", lnm::helpLanguageOnline());
}

QUrl SimBriefHandler::sendRouteUrl(const QString& departure, const QString& destination, const QString& alternate, const QString& route,
                                   const QString& aircraftType, float cruisingAltitudeFeet)
{
  // Dispatch redirect to open LNM route with all fields prefilled in SimBrief. This will transfer the current LNM route to SimBrief:
  // https://forum.navigraph.com/t/dispatch-redirect-guide/5299
  // Example EDDF LIRF with route: https://www.simbrief.com/system/dispatch.php?
  // type=B738&orig=EDDF&dest=LIRF&route=EDDF%20BOMBI%20T721%20SUNEG%20L607%20UTABA%20M738%20LORLO%20Q49%20MO
  // VOR%20TAGHE%20LUTOR%20Z909%20BIKTU%20GAVRA%20Y345%20RITEB%20LIRF

  QUrlQuery query;
  query.addQueryItem("type", aircraftType);
  query.addQueryItem("orig", departure);
  query.addQueryItem("dest", destination);
  query.addQueryItem("route", route);
  if(!alternate.isEmpty())
    query.addQueryItem("altn", alternate);
  query.addQueryItem("fl", QString::number(cruisingAltitudeFeet));

  QUrl url(dispatchUrl);
  url.setQuery(query);
  return url;
}

void SimBriefHandler::fetchRouteFromSimBrief()
{
  qDebug() << Q_FUNC_INFO;

  FetchRouteDialog dialog(mainWindow);
  connect(&dialog, &FetchRouteDialog::routeNewFromFlightplan, mainWindow, &MainWindow::routeFromFlightplan);
  connect(&dialog, &FetchRouteDialog::routeNewFromString, mainWindow, &MainWindow::routeFromStringSimBrief);
  dialog.exec();
}
