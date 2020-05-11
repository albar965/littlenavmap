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
class RouteExportFormat;
class RouteMultiExportDialog;
class RouteExportFormatMap;

/*
 * Covers all flight plan export and export related functions including validation and warning dialogs.
 *
 * Does not contain save, save as or open methods for LNMPLN.
 */
class RouteExport :
  public QObject
{
  Q_OBJECT

public:
  explicit RouteExport(MainWindow *parent = nullptr);
  virtual ~RouteExport();

  /* Save and restore state of multiexport dialog and format map (custom paths and selection state) */
  void saveState();
  void restoreState();

  /* Update simulator dependent default paths. */
  void postDatabaseLoad();

  /* Run export to all selected formats. */
  void routeMultiExport();

  /* Open multiexport dialog */
  void routeMulitExportOptions();

  /* Methods called by multiexport or menu actions ========================================================= */

  /* FSX/P3D XML PLN format */
  /* Also used for manual export */
  bool routeExportPlnMan(); /* Called by action */
  bool routeExportPln(const RouteExportFormat& format);

  /* New X-Plane FMS 11 */
  /* Also used for manual export */
  bool routeExportFms11Man(); /* Called by action */
  bool routeExportFms11(const RouteExportFormat& format);

  /* FlightGear XML */
  /* Also used for manual export */
  bool routeExportFlightgearMan(); /* Called by action */
  bool routeExportFlightgear(const RouteExportFormat& format);

  /* vPilot VATSIM */
  bool routeExportVfpMan(); /* Called by action */
  bool routeExportVfp(const RouteExportFormat& format);

  /* IVAP or X-IVAP for IVAO */
  /* Also used for manual export */
  bool routeExportIvapMan(); /* Called by action */
  bool routeExportIvap(const RouteExportFormat& format);
  bool routeExportXIvapMan(); /* Called by action */
  bool routeExportXIvap(const RouteExportFormat& format);

  /* Garmin exchange format. Not a flight plan format.  */
  /* Also used for manual export */
  bool routeExportGpxMan(); /* Called by action */
  bool routeExportGpx(const RouteExportFormat& format);

  /* Export as HTML page */
  /* Also used for manual export */
  bool routeExportHtmlMan(); /* Called by action */
  bool routeExportHtml(const RouteExportFormat& format);

  /* Methods for multiexport ========================================================= */

  /* Save as above but with annotations for proceduresas used by LNM before 2.4.5 */
  bool routeExportPlnAnnotatedMulti(const RouteExportFormat& format);

  /* Old X-Plane FMS 3 */
  bool routeExportFms3Multi(const RouteExportFormat& format);

  /* Aerosoft airbus FLP */
  bool routeExportFlpMulti(const RouteExportFormat& format);

  /* Flight plan export functions */
  bool routeExportGfpMulti(const RouteExportFormat& format);

  /* Rotate MD-80 and others */
  bool routeExportTxtMulti(const RouteExportFormat& format);

  /* PMDG RTE format */
  bool routeExportRteMulti(const RouteExportFormat& format);

  /* Majestic Dash binary format */
  bool routeExportFprMulti(const RouteExportFormat& format);

  /* IXEG 737 */
  bool routeExportFplMulti(const RouteExportFormat& format);

  /* Flight factor airbus */
  bool routeExportCorteInMulti(const RouteExportFormat& format);

  /* Reality XP GNS */
  bool routeExportRxpGnsMulti(const RouteExportFormat& format);

  /* Reality XP GTN */
  bool routeExportRxpGtnMulti(const RouteExportFormat& format);

  /* iFly */
  bool routeExportFltplanMulti(const RouteExportFormat& format);

  /* X-FMC */
  bool routeExportXFmcMulti(const RouteExportFormat& format);

  /* UFMC */
  bool routeExportUFmcMulti(const RouteExportFormat& format);

  /* ProSim */
  bool routeExportProSimMulti(const RouteExportFormat& format);

  /* BlackBox Simulations Airbus */
  bool routeExportBbsMulti(const RouteExportFormat& format);

  /* FeelThere or Wilco aircraft */
  bool routeExportFeelthereFplMulti(const RouteExportFormat& format);

  /* Level-D */
  bool routeExportLeveldRteMulti(const RouteExportFormat& format);

  /* AivlaSoft EFB */
  bool routeExportEfbrMulti(const RouteExportFormat& format);

  /* QualityWings Aircraft RTE */
  bool routeExportQwRteMulti(const RouteExportFormat& format);

  /* Leonardo Maddog X */
  bool routeExportMdrMulti(const RouteExportFormat& format);

  /* TFDi Design 717 */
  bool routeExportTfdiMulti(const RouteExportFormat& format);

  /* End of methods for multiexport ========================================================= */

  /* Check if route has valid departure  and destination and departure parking.
   *  @return true if route can be saved anyway */
  bool routeValidate(bool validateParking, bool validateDepartureAndDestination, bool multi = false);

  /* Validates only if this is a manual save */
  bool routeValidateMulti(const RouteExportFormat& format, bool validateParking, bool validateDepartureAndDestination);

  /* Build short or long filename depending on settings.
   * Short schema is ICAO{sep}ICAO{extension}{suffix}
   *  Long schema is ICAO (NAME) ICAO (NAME){extension}{suffix}*/
  QString buildDefaultFilename(const QString& sep = "_", const QString& suffix = ".lnmpln",
                               const QString& extension = QString()) const;

  /* Create a default filename based on departure and destination names. Suffix includes dot. */
  static QString buildDefaultFilenameLong(const QString& extension = QString(), const QString& suffix = ".lnmpln");

  /* Create a default filename based on departure and destination idents. Suffix includes dot. */
  static QString buildDefaultFilenameShort(const QString& sep, const QString& suffix);

  /* Return a copy of the route that has procedures replaced with waypoints depending on selected options in the menu.
   *  Also sets altitude into FlightplanEntry position. */
  static Route buildAdjustedRoute(const Route& route, rf::RouteAdjustOptions options);
  Route buildAdjustedRoute(rf::RouteAdjustOptions options);

  /* true if any formats are selected for multiexport */
  bool hasSelected() const
  {
    return selected;
  }

signals:
  /* Show airport on map to allow parking selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show parking selection dialog */
  void  selectDepartureParking();

  /* Number of selected has changed */
  void  optionsUpdated();

private:
  bool routeExportInternalPln(bool annotated, const RouteExportFormat& format);

  /* Formats that have no export method in FlightplanIO */
  bool exportFlighplanAsGfp(const QString& filename);
  bool exportFlighplanAsTxt(const QString& filename);
  bool exportFlighplanAsCorteIn(const QString& filename);
  bool exportFlighplanAsProSim(const QString& filename);
  bool exportFlighplanAsUFmc(const QString& filename);
  bool exportFlightplanAsGpx(const QString& filename);
  bool exportFlighplanAsRxpGns(const QString& filename);
  bool exportFlighplanAsRxpGtn(const QString& filename);

  /* Generic export using callback and also doing exception handling. */
  bool exportFlighplan(const QString& filename, rf::RouteAdjustOptions options,
                       std::function<void(const atools::fs::pln::Flightplan&, const QString&)> exportFunc);

  /* Shows dialog for IVAP data before exporting */
  bool routeExportIvapInternal(re::RouteExportType type, const RouteExportFormat& format);

  /* Show online network dialog which allows the user to enter needed data */
  bool routeExportDialog(RouteExportData& exportData, re::RouteExportType flightplanType);

  /* Run export for given type */
  void exportType(RouteExportFormat format);

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

  /* Called by all export functions that are used only in multiexport.
   * Uses all paths, descriptions, etc. from given RouteExportFormat. */
  QString exportFileMulti(const RouteExportFormat& format, const QString& filename);

  /* Called by all export functions that are used only in multiexport and manual export.
   * Saves file dialog location and state. */
  QString exportFile(const RouteExportFormat& format, const QString& settingsPrefix, const QString& path,
                     const QString& filename, bool dontComfirmOverwrite = false);

  /* Create a list of backups */
  void rotateFile(const QString& filename);

  MainWindow *mainWindow;
  atools::gui::Dialog *dialog;
  RouteMultiExportDialog *exportAllDialog;
  RouteExportFormatMap *exportFormatMap;
  atools::fs::pln::FlightplanIO *flightplanIO;

  /* true if any formats are selected for multiexport */
  bool selected = false;
};

#endif // LNM_ROUTEEXPORT_H
