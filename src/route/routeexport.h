/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LNM_ROUTEEXPORT_H
#define LNM_ROUTEEXPORT_H

#include <QObject>

namespace atools {
namespace geo {
class Rect;
}
namespace gui {
class Dialog;
}
}

class MainWindow;
class Route;

/*
 * Covers all flight plan export and export related functions including validation and warning dialogs.
 *
 * Does not contain save, save as or open methods.
 */
class RouteExport :
  public QObject
{
  Q_OBJECT

public:
  explicit RouteExport(MainWindow *parent = nullptr);
  virtual ~RouteExport();

  /* Flight plan export functions */
  bool routeExportGfp();
  bool routeExportTxt();
  bool routeExportRte();
  bool routeExportGpx();
  bool routeExportFpr();
  bool routeExportFpl();
  bool routeExportCorteIn();
  bool routeExportRxpGns();
  bool routeExportRxpGtn();
  bool routeExportIfly();
  bool routeExportXFmc();
  bool routeExportUFmc();
  bool routeExportProSim();
  bool routeExportBBsAirbus();

  /* Check if route has valid departure  and destination and departure parking.
   *  @return true if route can be saved anyway */
  bool routeValidate(bool validateParking, bool validateDepartureAndDestination);

  /* Create a default filename based on departure and destination names. Suffix includes dot. */
  QString buildDefaultFilename(const QString& extension = QString(), const QString& suffix = ".pln") const;
  QString buildDefaultFilenameShort(const QString& sep, const QString& suffix) const;

  /* Return a copy of the route that has procedures replaced with waypoints depending on selected options in the menu */
  static Route routeAdjustedToProcedureOptions(const Route& route);
  Route routeAdjustedToProcedureOptions();

signals:
  /* Show airport on map to allow parking selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show parking selection */
  void  selectDepartureParking();

private:
  bool exportFlighplanAsGfp(const QString& filename);
  bool exportFlighplanAsTxt(const QString& filename);
  bool exportFlighplanAsRte(const QString& filename);
  bool exportFlighplanAsFpr(const QString& filename);
  bool exportFlighplanAsCorteIn(const QString& filename);
  bool exportFlighplanAsCompanyroutesXml(const QString& filename);
  bool exportFlighplanAsUFmc(const QString& filename);

  bool exportFlightplanAsGpx(const QString& filename);

  bool exportFlighplanAsRxpGns(const QString& filename);
  bool exportFlighplanAsRxpGtn(const QString& filename);

  MainWindow *mainWindow;
  atools::gui::Dialog *dialog = nullptr;
};

#endif // LNM_ROUTEEXPORT_H
