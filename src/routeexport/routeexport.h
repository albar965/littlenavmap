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

#ifndef LNM_ROUTEEXPORT_H
#define LNM_ROUTEEXPORT_H

#include "routeexport/routeexportdialog.h"
#include "route/routeflags.h"

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
class RouteExportData;
class QTextStream;

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

  /* FSX/P3D XML PLN format */
  bool routeExportPln();

  /* Save as above but with annotations for proceduresas used by LNM before 2.4.5 */
  bool routeExportPlnAnnotated();

  /* Old X-Plane FMS 3 */
  bool routeExportFms3();

  /* New X-Plane FMS 11 */
  bool routeExportFms11();

  /* Aerosoft airbus FLP */
  bool routeExportFlp();

  /* FlightGear XML */
  bool routeExportFlightgear();

  /* Flight plan export functions */
  bool routeExportGfp();

  /* Rotate MD-80 and others */
  bool routeExportTxt();

  /* PMDG RTE format */
  bool routeExportRte();

  /* Garmin exchange format. Not a flight plan format.  */
  bool routeExportGpx();

  /* Export as HTML page */
  bool routeExportHtml();

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

  /* vPilot VATSIM */
  bool routeExportVfp();

  /* IVAP or X-IVAP for IVAO */
  bool routeExportIvap();
  bool routeExportXIvap();

  /* FeelThere or Wilco aircraft */
  bool routeExportFeelthereFpl();

  /* Level-D */
  bool routeExportLeveldRte();

  /* AivlaSoft EFB */
  bool routeExportEfbr();

  /* QualityWings Aircraft RTE */
  bool routeExportQwRte();

  /* Leonardo Maddog X */
  bool routeExportMdr();

  /* TFDi Design 717 */
  bool routeExportTfdi();

  /* Check if route has valid departure  and destination and departure parking.
   *  @return true if route can be saved anyway */
  bool routeValidate(bool validateParking, bool validateDepartureAndDestination);

  /* Build short or long filename depending on settings.
   * Short schema is ICAO{sep}ICAO{extension}{suffix}
   *  Long schema is ICAO (NAME) ICAO (NAME){extension}{suffix}*/
  QString buildDefaultFilename(const QString& sep = "_", const QString& suffix = ".lnmpln",
                               const QString& extension = QString()) const;

  /* Create a default filename based on departure and destination names. Suffix includes dot. */
  static QString buildDefaultFilenameLong(const QString& extension = QString(), const QString& suffix = ".lnmpln");
  static QString buildDefaultFilenameShort(const QString& sep, const QString& suffix);

  /* Return a copy of the route that has procedures replaced with waypoints depending on selected options in the menu.
   *  Also sets altitude into FlightplanEntry position. */
  static Route buildAdjustedRoute(const Route& route, rf::RouteAdjustOptions options);
  Route buildAdjustedRoute(rf::RouteAdjustOptions options);

signals:
  /* Show airport on map to allow parking selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show parking selection dialog */
  void  selectDepartureParking();

private:
  bool routeExportInternalPln(bool annotated);

  bool exportFlighplanAsGfp(const QString& filename);
  bool exportFlighplanAsTxt(const QString& filename);
  bool exportFlighplanAsCorteIn(const QString& filename);
  bool exportFlighplanAsProSim(const QString& filename);
  bool exportFlighplanAsUFmc(const QString& filename);

  bool exportFlightplanAsGpx(const QString& filename);

  bool exportFlighplanAsRxpGns(const QString& filename);
  bool exportFlighplanAsRxpGtn(const QString& filename);

  bool exportFlighplan(const QString& filename, rf::RouteAdjustOptions options,
                       std::function<void(const atools::fs::pln::Flightplan&, const QString&)> exportFunc);
  bool routeExportIvapInternal(re::RouteExportType type);

  /* Show online network dialog which allows the user to enter needed data */
  bool routeExportDialog(RouteExportData& exportData, re::RouteExportType flightplanType);

  /* Prefill online data with speed, cruise, etc.  */
  RouteExportData createRouteExportData(re::RouteExportType flightplanType);

  /* Export vRoute */
  bool exportFlighplanAsVfp(const RouteExportData& exportData, const QString& filename);

  /* Export IVAP or X-IVAP */
  bool exportFlighplanAsIvap(const RouteExportData& exportData, const QString& filename, re::RouteExportType type);
  QString minToHourMinStr(int minutes);

  void writeIvapLine(QTextStream& stream, const QString& key, const QString& value, re::RouteExportType type);
  void writeIvapLine(QTextStream& stream, const QString& key, int value, re::RouteExportType type);

  bool routeSaveCheckFMS11Warnings();

  MainWindow *mainWindow;
  atools::gui::Dialog *dialog;
  atools::fs::pln::FlightplanIO *flightplanIO;
  QString documentsLocation;
};

#endif // LNM_ROUTEEXPORT_H
