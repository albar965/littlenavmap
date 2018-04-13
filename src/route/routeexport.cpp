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

#include "route/routeexport.h"

#include "gui/mainwindow.h"
#include "common/constants.h"
#include "common/aircrafttrack.h"
#include "route/route.h"
#include "options/optiondata.h"
#include "gui/dialog.h"
#include "ui_mainwindow.h"
#include "fs/pln/flightplanio.h"
#include "navapp.h"
#include "atools.h"
#include "route/routestring.h"
#include "gui/errorhandler.h"
#include "options/optiondata.h"
#include "gui/dialog.h"
#include "ui_mainwindow.h"
#include "fs/pln/flightplanio.h"

#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <exception.h>

RouteExport::RouteExport(MainWindow *parent)
  : mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);

}

RouteExport::~RouteExport()
{
  delete dialog;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportRxpGns()
{
  qDebug() << Q_FUNC_INFO;

  // Save flight plan as FPL file usable by the GNS 530W/430W V2 - XML format
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString path;

#ifdef Q_OS_WIN32
    QString gnsPath(qgetenv("GNSAPPDATA"));
    path = gnsPath.isEmpty() ? "C:\\ProgramData\\Garmin\\GNS Trainer Data\\GNS\\FPL" : gnsPath + "\\FPL";
#elif DEBUG_INFORMATION
    path = atools::buildPath({QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first(),
                              "Garmin", "GNS Trainer Data", "GNS", "FPL"});
#else
    path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
#endif

    bool mkdir = QDir(path).mkpath(path);
    qInfo() << "mkdir" << path << "result" << mkdir;

    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FPL for Reality XP GNS"),
      tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL),
      "fpl", "Route/RxpGns", path,
      buildDefaultFilenameShort("", ".fpl"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsRxpGns(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportRxpGtn()
{
  qDebug() << Q_FUNC_INFO;

  // Save flight plan as GFP file usable by the Reality XP GTN 750/650 Touch
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString path;

#ifdef Q_OS_WIN32
    QString gtnPath(qgetenv("GTNSIMDATA"));
    path = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\GTN\\FPLN" : gtnPath + "\\FPLN";
#elif DEBUG_INFORMATION
    path = atools::buildPath({QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first(),
                              "Garmin", "Trainers", "GTN", "FPLN"});
#else
    path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
#endif

    bool mkdir = QDir(path).mkpath(path);
    qInfo() << "mkdir" << path << "result" << mkdir;

    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as GFP for Reality XP GTN"),
      tr("Garmin GFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GFP),
      "gfp", "Route/RxpGfp", path,
      buildDefaultFilenameShort("_", ".gfp"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsRxpGtn(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as GFP."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportGfp()
{
  qDebug() << Q_FUNC_INFO;

  // <FSX/P3D>/F1GTN/FPL.
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as Garmin GFP Format"),
      tr("Garmin GFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GFP),
      "gfp", "Route/Gfp",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "F1GTN" + QDir::separator() + "FPL",
      buildDefaultFilenameShort("-", ".gfp"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsGfp(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as GFP."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportTxt()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as TXT Format"),
      tr("Text Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_TXT), "txt", "Route/Txt",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "Aircraft",
      buildDefaultFilenameShort(QString(), ".txt"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as TXT."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportRte()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as PMDG RTE Format"),
      tr("RTE Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_RTE),
      "rte", "Route/Rte",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "PMDG" + QDir::separator() + "FLIGHTPLANS",
      buildDefaultFilenameShort(QString(), ".rte"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsRte(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as RTE."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFpr()
{
  qDebug() << Q_FUNC_INFO;

  // <FSX/P3D>/SimObjects/Airplanes/mjc8q400/nav/routes.

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as Majestic Dash FPR..."),
      tr("FPR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPR),
      "fpr", "Route/Fpr",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "SimObjects" +
      QDir::separator() + "Airplanes" +
      QDir::separator() + "mjc8q400" +
      QDir::separator() + "nav" +
      QDir::separator() + "routes",
      buildDefaultFilenameShort(QString(), ".fpr"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsFpr(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPR."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFpl()
{
  qDebug() << Q_FUNC_INFO;

  // \X-Plane 11\Aircraft\X-Aviation\IXEG 737 Classic\coroutes
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as IXEG FPL Format"),
      tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL),
      "fpl", "Route/Fpl",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Aircraft" +
      QDir::separator() + "X-Aviation" +
      QDir::separator() + "IXEG 737 Classic" +
      QDir::separator() + "coroutes",
      buildDefaultFilenameShort(QString(), ".fpl"));

    if(!routeFile.isEmpty())
    {
      // Same format as txt
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportCorteIn()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan to corte.in for Flight Factor Airbus"),
      tr("corte.in Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_CORTEIN),
      ".in", "Route/CorteIn",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "Aircraft", "corte.in",
      true /* dont confirm overwrite */);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsCorteIn(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved to corte.in."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportIfly()
{
  qDebug() << Q_FUNC_INFO;
  return false;
}

bool RouteExport::routeExportXFmc()
{
  qDebug() << Q_FUNC_INFO;
  return false;
}

bool RouteExport::routeExportUFmc()
{
  qDebug() << Q_FUNC_INFO;
  return false;
}

bool RouteExport::routeExportProSim()
{
  qDebug() << Q_FUNC_INFO;
  return false;
}

bool RouteExport::routeExportBBsAirbus()
{
  qDebug() << Q_FUNC_INFO;
  return false;
}

bool RouteExport::routeExportGpx()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, false /* validate departure and destination */))
  {
    QString title = NavApp::getAircraftTrack().isEmpty() ?
                    tr("Save Flight Plan as GPX Format") : tr("Save Flightplan and Track as GPX Format");

    QString routeFile = dialog->saveFileDialog(
      title,
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "gpx", "Route/Gpx", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first(),
      buildDefaultFilename(QString(), ".gpx"));

    if(!routeFile.isEmpty())
    {
      if(exportFlightplanAsGpx(routeFile))
      {
        if(NavApp::getAircraftTrack().isEmpty())
          mainWindow->setStatusMessage(tr("Flight plan saved as GPX."));
        else
          mainWindow->setStatusMessage(tr("Flight plan and track saved as GPX."));
        return true;
      }
    }
  }
  return false;
}

/* Check if route has valid departure  and destination and departure parking.
 *  @return true if route can be saved anyway */
bool RouteExport::routeValidate(bool validateParking, bool validateDepartureAndDestination)
{
  if(!NavApp::getRouteConst().hasValidDeparture() || !NavApp::getRouteConst().hasValidDestination())
  {
    if(validateDepartureAndDestination)
    {
      NavApp::deleteSplashScreen();
      const static atools::gui::DialogButtonList BUTTONS({
        {QString(), QMessageBox::Cancel},
        {tr("Select Start &Position"), QMessageBox::Yes},
        {QString(), QMessageBox::Save}
      });
      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOWROUTE_WARNING,
                                              tr("Flight Plan must have a valid airport as "
                                                 "start and destination and "
                                                 "will not be usable by the Simulator."),
                                              tr("Do not show this dialog again and"
                                                 " save Flight Plan in the future."),
                                              QMessageBox::Cancel | QMessageBox::Save,
                                              QMessageBox::Cancel, QMessageBox::Save);

      if(result == QMessageBox::Save)
        // Save anyway
        return true;
      else if(result == QMessageBox::Cancel)
        return false;
    }
  }
  else
  {
    if(validateParking)
    {
      if(!NavApp::getRouteConst().hasValidParking())
      {
        NavApp::deleteSplashScreen();

        // Airport has parking but no one is selected
        const static atools::gui::DialogButtonList BUTTONS({
          {QString(), QMessageBox::Cancel},
          {tr("Select Start &Position"), QMessageBox::Yes},
          {tr("Show &Departure on Map"), QMessageBox::YesToAll},
          {QString(), QMessageBox::Save}
        });

        int result = dialog->showQuestionMsgBox(
          lnm::ACTIONS_SHOWROUTE_PARKING_WARNING,
          tr("The start airport has parking spots but no parking was selected for this Flight Plan"),
          tr("Do not show this dialog again and save Flight Plan in the future."),
          BUTTONS, QMessageBox::Yes, QMessageBox::Save);

        if(result == QMessageBox::Yes)
          // saving depends if user selects parking or cancels out of the dialog
          emit selectDepartureParking();
        else if(result == QMessageBox::YesToAll)
        {
          // Zoom to airport and cancel out
          emit showRect(NavApp::getRouteConst().first().getAirport().bounding, false);
          return false;
        }
        else if(result == QMessageBox::Save)
          // Save right away
          return true;
        else if(result == QMessageBox::Cancel)
          return false;
      }
    }
  }
  return true;
}

QString RouteExport::buildDefaultFilename(const QString& extension, const QString& suffix) const
{
  QString filename;

  const atools::fs::pln::Flightplan& flightplan = NavApp::getRouteConst().getFlightplan();

  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
    filename = "IFR ";
  else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
    filename = "VFR ";

  if(flightplan.getDepartureAiportName().isEmpty())
    filename += flightplan.getEntries().first().getIcaoIdent();
  else
    filename += flightplan.getDepartureAiportName() + " (" + flightplan.getDepartureIdent() + ")";

  filename += " to ";

  if(flightplan.getDestinationAiportName().isEmpty())
    filename += flightplan.getEntries().last().getIcaoIdent();
  else
    filename += flightplan.getDestinationAiportName() + " (" + flightplan.getDestinationIdent() + ")";

  filename += extension;
  filename += suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

QString RouteExport::buildDefaultFilenameShort(const QString& sep, const QString& suffix) const
{
  QString filename;

  const atools::fs::pln::Flightplan& flightplan = NavApp::getRouteConst().getFlightplan();

  filename += flightplan.getEntries().first().getIcaoIdent();
  filename += sep;

  filename += flightplan.getEntries().last().getIcaoIdent();
  filename += suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

bool RouteExport::exportFlighplanAsGfp(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString gfp = RouteString().createGfpStringForRoute(routeAdjustedToProcedureOptions(), false /* procedures */,
                                                      OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = gfp.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving GFP file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsTxt(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString txt = RouteString().createStringForRoute(routeAdjustedToProcedureOptions(),
                                                   0.f, rs::DCT | rs::START_AND_DEST | rs::SID_STAR_GENERIC);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = txt.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving TXT file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsRte(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    atools::fs::pln::FlightplanIO().saveRte(routeAdjustedToProcedureOptions().getFlightplan(), filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteExport::exportFlighplanAsRxpGns(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    atools::fs::pln::SaveOptions options = atools::fs::pln::SAVE_NO_OPTIONS;

    if(OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT)
      options |= atools::fs::pln::SAVE_GNS_USER_WAYPOINTS;

    // Regions are required for the export
    NavApp::getRoute().updateAirportRegions();
    atools::fs::pln::FlightplanIO().saveGarminGns(
      routeAdjustedToProcedureOptions().getFlightplan(), filename, options);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteExport::exportFlighplanAsRxpGtn(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString gfp = RouteString().createGfpStringForRoute(routeAdjustedToProcedureOptions(), true /* procedures */,
                                                      OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = gfp.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving GFP file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsFpr(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    atools::fs::pln::FlightplanIO().saveFpr(routeAdjustedToProcedureOptions().getFlightplan(), filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteExport::exportFlighplanAsCorteIn(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString txt = RouteString().createStringForRoute(routeAdjustedToProcedureOptions(), 0.f,
                                                   rs::DCT | rs::START_AND_DEST | rs::SID_STAR | rs::SID_STAR_SPACE |
                                                   rs::RUNWAY | rs::APPROACH | rs::FLIGHTLEVEL);

  txt.prepend(QString("RTE %1%2 ").
              arg(NavApp::getRouteConst().getFlightplan().getDepartureIdent()).arg(NavApp::getRouteConst().getFlightplan()
                                                                                   .
                                                                                   getDestinationIdent()));

  // Check if we have to insert an endl first
  bool endsWithEol = atools::fileEndsWithEol(filename);

  // Append string to file
  QFile file(filename);
  if(file.open(QFile::Append | QIODevice::Text))
  {
    QTextStream stream(&file);

    if(!endsWithEol)
      stream << endl;
    stream << txt;
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving to corte.in file:"));
    return false;
  }
}

bool RouteExport::exportFlightplanAsGpx(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  const AircraftTrack& aircraftTrack = NavApp::getAircraftTrack();
  atools::geo::LineString track;
  QVector<quint32> timestamps;

  for(const at::AircraftTrackPos& pos : aircraftTrack)
  {
    track.append(pos.pos);
    timestamps.append(pos.timestamp);
  }

  try
  {
    atools::fs::pln::FlightplanIO().saveGpx(
      routeAdjustedToProcedureOptions().getFlightplan(), filename, track, timestamps,
      static_cast<int>(NavApp::getRouteConst().getCruisingAltitudeFeet()));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

Route RouteExport::routeAdjustedToProcedureOptions()
{
  return routeAdjustedToProcedureOptions(NavApp::getRoute());
}

Route RouteExport::routeAdjustedToProcedureOptions(const Route& route)
{
  Route rt = route.adjustedToProcedureOptions(NavApp::getMainUi()->actionRouteSaveApprWaypoints->isChecked(),
                                              NavApp::getMainUi()->actionRouteSaveSidStarWaypoints->isChecked());

  // Update airway structures
  rt.updateAirwaysAndAltitude(false);

  return rt;
}
