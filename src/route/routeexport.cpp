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
#include "io/fileroller.h"
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
#include <QXmlStreamReader>
#include <exception.h>

RouteExport::RouteExport(MainWindow *parent)
  : mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  flightplanIO = new atools::fs::pln::FlightplanIO;
}

RouteExport::~RouteExport()
{
  delete dialog;
  delete flightplanIO;
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
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveRte, flightplanIO, _1, _2)))
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
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveFpr, flightplanIO, _1, _2)))
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
        mainWindow->setStatusMessage(tr("Flight plan added to corte.in."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFltplan()
{
  qDebug() << Q_FUNC_INFO;
  // Default directory for the iFly stored Flight plans is Prepar3D/iFly/737NG/navdata/FLTPLAN
  // <FSX/P3D>/iFly/737NG/navdata/FLTPLAN.
  // YSSYYMML.FLTPLAN

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FLTPLAN for iFly"),
      tr("iFly FLTPLAN Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLTPLAN), "fltplan", "Route/Fltplan",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "iFly" +
      QDir::separator() + "737NG" +
      QDir::separator() + "navdata" +
      QDir::separator() + "FLTPLAN",
      buildDefaultFilenameShort(QString(), ".fltplan"));

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveFltplan, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FLTPLAN for iFly."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportXFmc()
{
  qDebug() << Q_FUNC_INFO;
  // The plans have an extension of *.FPL and are saved in for following Folder : -
  // Path to Root X-Plane 11 Folder\Resources\plugins\XFMC\FlightPlans
  // LFLLEHAM.FPL
  // Same as TXT but FPL

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FPL for X-FMC"),
      tr("X-FMC Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL), "fpl", "Route/XFmc",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Resources" +
      QDir::separator() + "plugins" +
      QDir::separator() + "XFMC" +
      QDir::separator() + "FlightPlans",
      buildDefaultFilenameShort(QString(), ".fpl"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL for X-FMC."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportUFmc()
{
  qDebug() << Q_FUNC_INFO;
  // EDDHLIRF.ufmc
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for UFMC"),
      tr("UFMC Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_UFMC), "ufmc", "Route/UFmc",
      NavApp::getCurrentSimulatorBasePath(),
      buildDefaultFilenameShort(QString(), ".ufmc"));

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsUFmc(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for UFMC."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportProSim()
{
  // companyroutes.xml
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan to companyroutes.xml for ProSim"),
      tr("companyroutes.xml Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_COMPANYROUTES_XML),
      ".xml", "Route/CompanyRoutesXml",
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first(), "companyroutes.xml",
      true /* dont confirm overwrite */);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsProSim(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan added to companyroutes.xml."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportBbs()
{
  qDebug() << Q_FUNC_INFO;
  // P3D Root Folder \BlackBox Simulation\Airbus A330
  // <FSX/P3D>/Blackbox Simulation/Company Routes.
  // Uses FS9 PLN format.   EDDHLIRF.pln

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for BBS Airbus"),
      tr("PLN Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_BBS_PLN), "pln", "Route/BbsPln",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Blackbox Simulation" +
      QDir::separator() + "Company Routes",
      buildDefaultFilenameShort(QString(), ".pln"));

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveBbsPln, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for BBS."));
        return true;
      }
    }
  }
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
  QString gfp = RouteString::createGfpStringForRoute(routeAdjustedToProcedureOptions(), false /* procedures */,
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
  QString txt = RouteString::createStringForRoute(routeAdjustedToProcedureOptions(),
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
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving TXT or FPL file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsUFmc(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QStringList list = RouteString::createStringForRouteList(routeAdjustedToProcedureOptions(), 0.f,
                                                           rs::DCT | rs::START_AND_DEST);

  // Remove last DCT
  if(list.size() - 2 >= 0 && list.at(list.size() - 2) == "DCT")
    list.removeAt(list.size() - 2);

  // Replace DCT with DIRECT
  list.replaceInStrings(QRegularExpression("^DCT$"), "DIRECT");
  // KJFK
  // CYYZ
  // DIRECT
  // GAYEL
  // Q818
  // WOZEE
  // 99
  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QTextStream stream(&file);
    // Save start and destination
    stream << list.first() << endl << list.last() << endl;

    // Waypoints and airways
    for(int i = 1; i < list.size() - 1; i++)
      stream << list.at(i) << endl;

    // File end
    stream << "99" << endl;

    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving UFMC file:"));
    return false;
  }
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
  QString gfp = RouteString::createGfpStringForRoute(routeAdjustedToProcedureOptions(), true /* procedures */,
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

bool RouteExport::exportFlighplan(const QString& filename,
                                  std::function<void(const atools::fs::pln::Flightplan& plan,
                                                     const QString &file)> exportFunc)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    exportFunc(routeAdjustedToProcedureOptions().getFlightplan(), filename);
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
  QString txt = RouteString::createStringForRoute(routeAdjustedToProcedureOptions(), 0.f,
                                                  rs::DCT | rs::START_AND_DEST | rs::SID_STAR | rs::SID_STAR_SPACE |
                                                  rs::RUNWAY | rs::APPROACH | rs::FLIGHTLEVEL);

  txt.prepend(QString("RTE %1%2 ").
              arg(NavApp::getRouteConst().getFlightplan().getDepartureIdent()).
              arg(NavApp::getRouteConst().getFlightplan().getDestinationIdent()));

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

bool RouteExport::exportFlighplanAsProSim(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  // <?xml version="1.0" encoding="UTF-8"?>
  // <companyroutes>
  // <route name="KDSMKOKC">KDSM DSM J25 TUL KOKC </route>
  // <route name="EDDHEDDS">EDDH IDEKO Y900 TIMEN UL126 WRB UN850 KRH T128 BADSO EDDS</route>
  // <route name="EDDSEDDH">EDDS KRH UZ210 NOSPA EDDL</route>
  // </companyroutes>

  // Read the XML file and keep all routes ===========================================
  QVector<std::pair<QString, QString> > routes;
  QSet<QString> routeNames;

  QFile file(filename);
  if(file.exists() && file.size() > 0)
  {
    if(file.open(QFile::ReadOnly | QIODevice::Text))
    {
      QXmlStreamReader reader(&file);

      while(!reader.atEnd())
      {
        if(reader.error() != QXmlStreamReader::NoError)
          throw atools::Exception("Error reading \"" + filename + "\": " + reader.errorString());

        QXmlStreamReader::TokenType token = reader.readNext();

        if(token == QXmlStreamReader::StartElement && reader.name() == "route")
        {
          QString name = reader.attributes().value("name").toString();
          QString route = reader.readElementText();
          routes.append(std::make_pair(name, route));
          routeNames.insert(name);
        }
      }
      file.close();
    }
    else
    {
      atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While reading from companyroutes.xml file:"));
      return false;
    }
  }

  // Create maximum of two backup files
  QString backupFile = filename + "_lnm_backup";
  atools::io::FileRoller roller(1);
  roller.rollFile(backupFile);

  // Copy file to backup before opening
  bool result = QFile(filename).copy(backupFile);
  qDebug() << Q_FUNC_INFO << "Copied" << filename << "to" << backupFile << result;

  // Create route string
  QString route = RouteString::createStringForRoute(routeAdjustedToProcedureOptions(), 0.f, rs::START_AND_DEST);
  QString name = buildDefaultFilenameShort(QString(), QString());

  // Find a unique name between all loaded
  QString newname = name;
  int i = 1;
  while(routeNames.contains(newname) && i < 99)
    newname = QString("%1%2").arg(name).arg(i++, 2, 10, QChar('0'));

  // Add new route
  routes.append(std::make_pair(newname, route));

  // Save and overwrite new file ====================================================
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);
    writer.writeStartDocument();

    writer.writeStartElement("companyroutes");

    for(const std::pair<QString, QString>& entry : routes)
    {
      // <route name="KDSMKOKC">KDSM DSM J25 TUL KOKC </route>
      writer.writeStartElement("route");
      writer.writeAttribute("name", entry.first);
      writer.writeCharacters(entry.second);
      writer.writeEndElement(); // route
    }

    writer.writeEndElement(); // companyroutes
    writer.writeEndDocument();
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving to companyroutes.xml file:"));
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
