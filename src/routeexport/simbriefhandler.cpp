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

#include "routeexport/simbriefhandler.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/unit.h"
#include "fs/perf/aircraftperf.h"
#include "gui/desktopservices.h"
#include "gui/mainwindow.h"
#include "route/route.h"
#include "routeexport/simbriefexportdialog.h"
#include "routeexport/fetchroutedialog.h"
#include "routestring/routestringwriter.h"
#include "settings/settings.h"

#include <QApplication>
#include <QClipboard>
#include <QUrlQuery>
#include <QWidget>

SimBriefHandler::SimBriefHandler(MainWindow *parent) :
  QObject(parent), mainWindow(parent)
{
  // Allow user to override URL
  dispatchUrl = atools::settings::Settings::instance().valueStr(lnm::ROUTE_EXPORT_SIMBRIEF_DISPATCH_URL,
                                                                "https://dispatch.simbrief.com/options/custom");
}

SimBriefHandler::~SimBriefHandler()
{
}

void SimBriefHandler::sendRouteToSimBrief()
{
  qDebug() << Q_FUNC_INFO;
  const Route& route = NavApp::getRouteConst();

  // Create route description string ================================
  QString routeString = RouteStringWriter().
                        createStringForRoute(route.adjustedToOptions(rf::ADD_PROC_ENTRY_EXIT), 0.f, rs::SIMBRIEF_WRITE_DEFAULTS);
  QString aircraftType = NavApp::getAircraftPerformance().getAircraftType();
  SimBriefExportDialog dialog(mainWindow, routeString, aircraftType, Unit::altFeet(route.getCruiseAltitudeFt()));
  if(dialog.exec() != QDialog::Accepted)
    return;

  // Build URL ================================
  QUrl url = sendRouteUrl(route.getDepartureAirportLeg().getIdent(), route.getDestinationAirportLeg().getIdent(),
                          route.getAlternateIdents().value(0), // Send only first alternate
                          routeString, aircraftType, route.getCruiseAltitudeFt(),
                          dialog.getAirline(), dialog.getFlightNumber());

  qDebug() << Q_FUNC_INFO << "Encoded full URL" << url.toEncoded();

  if(dialog.getAction() == SimBriefExportDialog::Export)
  {
    atools::gui::DesktopServices::openUrl(mainWindow, url);
    NavApp::setStatusMessage(QString(tr("SimBrief flight plan sent to web browser.")));
  }
  else if(dialog.getAction() == SimBriefExportDialog::CopyToClipboard)
  {
    QApplication::clipboard()->setText(QString::fromUtf8(url.toEncoded()));
    NavApp::setStatusMessage(QString(tr("SimBrief address copied to clipboard.")));
  }
}

QUrl SimBriefHandler::sendRouteUrl(const QString& departure, const QString& destination, const QString& alternate, const QString& route,
                                   const QString& aircraftType, float cruisingAltitudeFeet,
                                   const QString& airline, const QString& flightNumber)
{
  // Dispatch redirect to open LNM route with all fields prefilled in SimBrief. This will transfer the current LNM route to SimBrief:
  // https://forum.navigraph.com/t/dispatch-redirect-guide/5299
  // Example EDDF LIRF with route: https://dispatch.simbrief.com/options/custom?
  // type=B738&orig=EDDF&dest=LIRF&route=EDDF%20BOMBI%20T721%20SUNEG%20L607%20UTABA%20M738%20LORLO%20Q49%20MO
  // VOR%20TAGHE%20LUTOR%20Z909%20BIKTU%20GAVRA%20Y345%20RITEB%20LIRF&airline=ABC&fltnum=1234

  QUrlQuery query;
  query.addQueryItem("type", aircraftType);
  query.addQueryItem("orig", departure);
  query.addQueryItem("dest", destination);
  query.addQueryItem("route", route);
  if(!alternate.isEmpty())
    query.addQueryItem("altn", alternate);
  query.addQueryItem("fl", QString::number(cruisingAltitudeFeet));
  if(!airline.trimmed().isEmpty())
    query.addQueryItem("airline", airline.trimmed());
  if(!flightNumber.trimmed().isEmpty())
    query.addQueryItem("fltnum", flightNumber.trimmed());

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
