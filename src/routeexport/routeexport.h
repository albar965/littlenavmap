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

#ifndef LNM_ROUTEEXPORT_H
#define LNM_ROUTEEXPORT_H

#include "route/routeflags.h"
#include "routeexport/routeexportflags.h"

#include <QHash>
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
  virtual ~RouteExport() override;

  RouteExport(const RouteExport& other) = delete;
  RouteExport& operator=(const RouteExport& other) = delete;

  /* Save and restore state of multiexport dialog and format map (custom paths and selection state) */
  void saveState();
  void restoreState();

  /* Update simulator dependent default paths. */
  void postDatabaseLoad();

  /* Run export to all selected formats. */
  void routeMultiExport();

  /* Open multiexport dialog */
  void routeMultiExportOptions();

  /* Methods called by multiexport or menu actions ========================================================= */

  /* LNMPLN own format */
  bool routeExportLnm(const RouteExportFormat& format);

  /* FSX/P3D XML PLN format */
  /* Also used for manual export */
  bool routeExportPlnMan(); /* Called by action */
  bool routeExportPlnMsfsMan(); /* Called by action */
  bool routeExportPln(const RouteExportFormat& format);
  bool routeExportPlnMsfs(const RouteExportFormat& format);

  /* New X-Plane FMS 11 */
  /* Also used for manual export */
  bool routeExportFms11Man(); /* Called by action */
  bool routeExportFms11(const RouteExportFormat& format);

  /* FlightGear XML */
  /* Also used for manual export */
  bool routeExportFlightgearMan(); /* Called by action */
  bool routeExportFlightgear(const RouteExportFormat& format);

  /* vPilot/xPilot VATSIM */
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

  /* X-Plane CIVA Navigation System */
  bool routeExportCivaFmsMulti(const RouteExportFormat& format);

  /* IniBuilds */
  bool routeExportFms3IniBuildsMulti(const RouteExportFormat& format);

  /* Aerosoft airbus FLP */
  bool routeExportFlpMulti(const RouteExportFormat& format);

  /* Aerosoft CRJ FLP */
  bool routeExportFlpCrjMulti(const RouteExportFormat& format);

  /* Flight plan export functions */
  bool routeExportGfpMulti(const RouteExportFormat& format);

  /* TDS GTNXi - GFP */
  bool routeExportTdsGtnXiMulti(const RouteExportFormat& format);

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

  /* Reality XP GTN - GFP */
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

  /* iFly Jets Advanced Series */
  bool routeExportIflyMulti(const RouteExportFormat& format);

  /* Integrated Simavionics / ISG - FSX/PLN format with modified user waypoints */
  bool routeExportIsgMulti(const RouteExportFormat& format);

  /* PMS50 GTN750 fpl.pln - MSFS PLN format */
  bool routeExportPms50Multi(const RouteExportFormat& format);

  /* End of methods for multiexport ========================================================= */

  /* Check if route has valid departure  and destination and departure parking.
   * Also updates the navdata cycle properties in the global route before checking.
   *  @return true if route can be saved anyway */
  bool routeValidate(const QVector<RouteExportFormat>& formats, bool multi = false);

  /* Validates only if this is a manual save */
  bool routeValidateMulti(const RouteExportFormat& format);

  /* Return a copy of the route that has procedures replaced with waypoints depending on selected options in the menu.
   *  Also sets altitude into FlightplanEntry position. */
  Route buildAdjustedRoute(rf::RouteAdjustOptions options);

  /* true if any formats are selected for multiexport */
  bool hasSelected() const
  {
    return selected;
  }

  /* Warning dialog when changing export options */
  void warnExportOptionsFromMenu(bool checked);

  /* Set current and default path for the LNMPLN export */
  void setLnmplnExportDir(const QString& dir);

signals:
  /* Show airport on map to allow parking selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show parking selection dialog. Returns true if something was selected. */
  bool selectDepartureParking();

  /* Number of selected has changed */
  void  optionsUpdated();

private:
  /* Warning dialog when changing export options */
  void warnExportOptions();

  /* Build filename according to pattern set in options. Uses pattern including file suffix from format.
   * Special characters are removed if normalize is true */
  QString buildDefaultFilename(const RouteExportFormat& format, bool normalize = false);

  /* Saves all FSX/P3D PLN based formats */
  bool routeExportInternalPln(const RouteExportFormat& format);

  /* Saves all Aerosoft FLP variations */
  bool routeExportInternalFlp(const RouteExportFormat& format, bool crj, bool msfs);

  /* Formats that have no export method in FlightplanIO */
  bool exportFlighplanAsGfp(const QString& filename, bool saveAsUserWaypoints, bool procedures, bool gfpCoordinates);
  bool exportFlighplanAsTxt(const QString& filename);
  bool exportFlighplanAsCorteIn(const QString& filename);
  bool exportFlighplanAsProSim(const QString& filename);
  bool exportFlighplanAsUFmc(const QString& filename);
  bool exportFlightplanAsGpx(const QString& filename);
  bool exportFlighplanAsRxpGns(const QString& filename, bool saveAsUserWaypoints);
  bool exportFlighplanAsRxpGtn(const QString& filename, bool saveAsUserWaypoints, bool gfpCoordinates);

  /* Generic export using callback and also doing exception handling. */
  bool exportFlighplan(const QString& filename, rf::RouteAdjustOptions options,
                       std::function<void(const atools::fs::pln::Flightplan&, const QString&)> exportFunc);

  /* Shows dialog for IVAP data before exporting */
  bool routeExportIvapInternal(re::RouteExportType type, const RouteExportFormat& format,
                               const QString& settingsSuffix);

  bool routeExportVfpInternal(const RouteExportFormat& format, const QString& settingsSuffix);

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
  void writeIvapLine(QTextStream& stream, const QString& string, re::RouteExportType type);

  /* Called by all export functions that are used only in multiexport.
   * Uses all paths, descriptions, etc. from given RouteExportFormat. */
  QString exportFileMulti(const RouteExportFormat& format, const QString& filename);
  QString exportFileMulti(const RouteExportFormat& format);

  /* Called by all export functions that are used in multiexport and manual export.
   * Saves file dialog location and state. */
  QString exportFile(const RouteExportFormat& format, const QString& settingsPrefix, const QString& path,
                     const QString& filename, bool dontComfirmOverwrite = false);

  /* called for each exported file with format and filename which are collected in "exported" */
  void formatExportedCallback(const RouteExportFormat& format, const QString& filename);

  /* Create a list of backups */
  void rotateFile(const QString& filename);

  MainWindow *mainWindow;
  atools::gui::Dialog *dialog;
  RouteMultiExportDialog *multiExportDialog;
  RouteExportFormatMap *exportFormatMap;
  atools::fs::pln::FlightplanIO *flightplanIO;

  /* Filled by "formatExportedCallback" when doing a multi export using routeMultiExport() */
  QHash<int, QString> exported;

  /* true if any formats are selected for multiexport */
  bool selected = false;

  /* Show warning dialog about wrong format selection each session */
  bool warnedFormatOptions = false;
};

#endif // LNM_ROUTEEXPORT_H
