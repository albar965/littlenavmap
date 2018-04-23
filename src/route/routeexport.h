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
#include <functional>

namespace atools {

namespace fs {
namespace pln {
class Flightplan;
class FlightplanIO;
}
}
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

  /* Rotate MD-80 and others */
  bool routeExportTxt();

  /* PMDG RTE format */
  bool routeExportRte();

  /* Garmin exchange format. Not a flight plan format.  */
  bool routeExportGpx();

  /* Majestic Dash binary format */
  bool routeExportFpr();

  /* IXEG 737 */
  bool routeExportFpl();

  /* Flight factor airbus */
  bool routeExportCorteIn();

  /* Reality XP GNS */
  bool routeExportRxpGns();

  /* Reality XP GTN */
  bool routeExportRxpGtn();

  /* iFly */
  bool routeExportFltplan();

  /* X-FMC */
  bool routeExportXFmc();

  /* UFMC */
  bool routeExportUFmc();

  /* ProSim */
  bool routeExportProSim();

  /* BlackBox Simulations Airbus */
  bool routeExportBbs();

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

  /* Show parking selection dialog */
  void  selectDepartureParking();

private:
  bool exportFlighplanAsGfp(const QString& filename);
  bool exportFlighplanAsTxt(const QString& filename);
  bool exportFlighplanAsCorteIn(const QString& filename);
  bool exportFlighplanAsProSim(const QString& filename);
  bool exportFlighplanAsUFmc(const QString& filename);

  bool exportFlightplanAsGpx(const QString& filename);

  bool exportFlighplanAsRxpGns(const QString& filename);
  bool exportFlighplanAsRxpGtn(const QString& filename);

  bool exportFlighplan(const QString& filename, std::function<void(const atools::fs::pln::Flightplan&,
                                                                   const QString &)> exportFunc);

  MainWindow *mainWindow;
  atools::gui::Dialog *dialog;
  atools::fs::pln::FlightplanIO *flightplanIO;
};

#endif // LNM_ROUTEEXPORT_H
