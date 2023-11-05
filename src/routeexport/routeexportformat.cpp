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

#include "routeexport/routeexportformat.h"

#include "atools.h"
#include "common/constants.h"
#include "fs/pln/flightplan.h"
#include "gui/errorhandler.h"
#include "app/navapp.h"
#include "routeexport/routeexport.h"
#include "settings/settings.h"

#include <QDataStream>
#include <QDir>
#include <QProcessEnvironment>
#include <QStringBuilder>
#include <exception.h>

using atools::settings::Settings;
using atools::fs::FsPaths;

quint16 RouteExportFormatMap::version = 0;

// Simply log warnings instead of throwing exceptions on read errors
bool RouteExportFormatMap::exceptionOnReadError = false;

const QVector<RouteExportFormat> RouteExportFormatMap::getSelected() const
{
  QVector<RouteExportFormat> retval;
  for(const RouteExportFormat& fmt : (*this))
  {
    if(fmt.isSelected())
      retval.append(fmt);
  }
  return retval;
}

#ifdef DEBUG_INFORMATION_MULTIEXPORT

void RouteExportFormatMap::setDebugOptions(rexp::RouteExportFormatType type)
{
  RouteExportFormat& format = (*this)[type];

  QString defaultPath = QDir::homePath() + "/Temp/Little Navmap Export";
  QString path = Settings::instance().getAndStoreValue(lnm::OPTIONS_MULTIEXPORT_DEBUG_PATH, defaultPath).toString();

  format.setPath(path + "/" + format.getCategory());

  QDir().mkpath(format.getPath());
  format.setPattern(QFileInfo(format.getDefaultPattern()).baseName() + " " + format.getComment().replace("/", "-") +
                    "." + QFileInfo(format.getDefaultPattern()).completeSuffix());

  if(format.isAppendToFile())
    atools::strToFile(format.getPath() + "/" + format.getPattern(), " XXX  ");

  if(format.getCategory() != "Online" && format.getCategory() != "Little Navmap")
    format.setFlag(rexp::SELECTED, true);
}

#endif

void RouteExportFormatMap::clearPath(rexp::RouteExportFormatType type)
{
  (*this)[type].setPath((*this)[type].getDefaultPath());
}

void RouteExportFormatMap::updatePath(rexp::RouteExportFormatType type, const QString& path)
{
  (*this)[type].setPath(atools::nativeCleanPath(path));
}

void RouteExportFormatMap::clearPattern(rexp::RouteExportFormatType type)
{
  (*this)[type].setPattern((*this)[type].getDefaultPattern());
}

void RouteExportFormatMap::updatePattern(rexp::RouteExportFormatType type, const QString& filePattern)
{
  (*this)[type].setPattern(filePattern);
}

void RouteExportFormatMap::setSelected(rexp::RouteExportFormatType type, bool selected)
{
  (*this)[type].setFlag(rexp::SELECTED, selected);
}

void RouteExportFormatMap::saveState()
{
  atools::settings::Settings& settings = Settings::instance();
  settings.setValueVar(lnm::ROUTE_EXPORT_FORMATS, QVariant::fromValue<RouteExportFormatMap>(*this));
}

void RouteExportFormatMap::restoreState()
{
  // Enable exceptions when loading
  exceptionOnReadError = true;

  clear();

  // Initialize with defaults
  init();

  // Update simulator dependent default paths
  updateDefaultPaths();

  // Load selection status and user updated paths from settings file ==================================================
  atools::settings::Settings& settings = Settings::instance();
  RouteExportFormatMap loadedFormats;
  try
  {
    loadedFormats = settings.valueVar(lnm::ROUTE_EXPORT_FORMATS).value<RouteExportFormatMap>();
  }
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(NavApp::getQMainWidget()).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(NavApp::getQMainWidget()).handleUnknownException();
  }

  // Copy loaded states into stock formats ==================================================
  for(const RouteExportFormat& loadedFmt : qAsConst(loadedFormats))
  {
    if(contains(loadedFmt.getType()))
    {
      RouteExportFormat& stockFmt = (*this)[loadedFmt.getType()];

      if(contains(loadedFmt.getType()))
        loadedFmt.copyLoadedDataTo(stockFmt);
      else
        qWarning() << Q_FUNC_INFO << "Type not found in internal list" << loadedFmt.getTypeAsInt();

      if(RouteExportFormatMap::getVersion() < RouteExportFormatMap::FILE_VERSION_CURRENT)
      {
        qDebug() << Q_FUNC_INFO << "Migrating previous settings" << stockFmt.getComment();

        // Copy default pattern if updated from previous version
        stockFmt.setPattern(stockFmt.getDefaultPattern());

        // Adjust file based paths from previous version
        if(stockFmt.getPath().endsWith("corte.in", Qt::CaseInsensitive))
        {
          qDebug() << Q_FUNC_INFO << stockFmt.getPath();
          stockFmt.setPattern("corte.in"); // Filename
          stockFmt.setPath(QFileInfo(stockFmt.getPath()).path()); // Directory
        }

        if(stockFmt.getPath().endsWith("companyroutes.xml", Qt::CaseInsensitive))
        {
          qDebug() << Q_FUNC_INFO << stockFmt.getPath();
          stockFmt.setPattern("companyroutes.xml");
          stockFmt.setPath(QFileInfo(stockFmt.getPath()).path());
        }
      }

      stockFmt.correctPattern();
      stockFmt.updatePathError();
    }
    else
      // Saved format not found in default list
      qWarning() << Q_FUNC_INFO << "Stock format not found" << loadedFmt.getType();
  }
}

void RouteExportFormatMap::updatePathErrors()
{
  for(RouteExportFormat& format : *this)
  {
    if(format.isSelected())
      format.updatePathError();
  }
}

void RouteExportFormatMap::setLnmplnExportDir(const QString& dir)
{
  (*this)[rexp::LNMPLN].setPath(dir);
  (*this)[rexp::LNMPLN].setDefaultPath(dir);
}

bool RouteExportFormatMap::hasSelected() const
{
  for(const RouteExportFormat& fmt : (*this))
  {
    if(fmt.isSelected())
      return true;
  }
  return false;
}

void RouteExportFormatMap::initCallbacks(RouteExport *routeExport)
{
  using namespace std::placeholders;
  using std::bind;
  using namespace rexp;

#define CB setExportCallback

  // Assign callbacks from route export instance
  /* *INDENT-OFF* */
  (*this)[LNMPLN      ].CB(bind(&RouteExport::routeExportLnm,                routeExport, _1));
  (*this)[PLN         ].CB(bind(&RouteExport::routeExportPln,                routeExport, _1));
  (*this)[PLNMSFS     ].CB(bind(&RouteExport::routeExportPlnMsfs,            routeExport, _1));
  (*this)[PLNANNOTATED].CB(bind(&RouteExport::routeExportPlnAnnotatedMulti,  routeExport, _1));
  (*this)[FMS3        ].CB(bind(&RouteExport::routeExportFms3Multi,          routeExport, _1));
  (*this)[CIVAFMS     ].CB(bind(&RouteExport::routeExportCivaFmsMulti,       routeExport, _1));
  (*this)[FMS11       ].CB(bind(&RouteExport::routeExportFms11,              routeExport, _1));
  (*this)[FMS12       ].CB(bind(&RouteExport::routeExportFms11,              routeExport, _1));
  (*this)[FLP         ].CB(bind(&RouteExport::routeExportFlpMulti,           routeExport, _1));
  (*this)[FLPCRJ      ].CB(bind(&RouteExport::routeExportFlpCrjMulti,        routeExport, _1));
  (*this)[FLPCRJMSFS  ].CB(bind(&RouteExport::routeExportFlpCrjMulti,        routeExport, _1));
  (*this)[FLIGHTGEAR  ].CB(bind(&RouteExport::routeExportFlightgear,         routeExport, _1));
  (*this)[GFP         ].CB(bind(&RouteExport::routeExportGfpMulti,           routeExport, _1));
  (*this)[GFPUWP      ].CB(bind(&RouteExport::routeExportGfpMulti,           routeExport, _1));
  (*this)[TXT         ].CB(bind(&RouteExport::routeExportTxtMulti,           routeExport, _1));
  (*this)[TXTJAR      ].CB(bind(&RouteExport::routeExportTxtMulti,           routeExport, _1));
  (*this)[RTE         ].CB(bind(&RouteExport::routeExportRteMulti,           routeExport, _1));
  (*this)[RTEMSFS     ].CB(bind(&RouteExport::routeExportRteMulti,           routeExport, _1));
  (*this)[GPX         ].CB(bind(&RouteExport::routeExportGpx,                routeExport, _1));
  (*this)[HTML        ].CB(bind(&RouteExport::routeExportHtml,               routeExport, _1));
  (*this)[FPR         ].CB(bind(&RouteExport::routeExportFprMulti,           routeExport, _1));
  (*this)[FPL         ].CB(bind(&RouteExport::routeExportFplMulti,           routeExport, _1));
  (*this)[CORTEIN     ].CB(bind(&RouteExport::routeExportCorteInMulti,       routeExport, _1));
  (*this)[RXPGNS      ].CB(bind(&RouteExport::routeExportRxpGnsMulti,        routeExport, _1));
  (*this)[RXPGNSUWP   ].CB(bind(&RouteExport::routeExportRxpGnsMulti,        routeExport, _1));
  (*this)[RXPGTN      ].CB(bind(&RouteExport::routeExportRxpGtnMulti,        routeExport, _1));
  (*this)[RXPGTNUWP   ].CB(bind(&RouteExport::routeExportRxpGtnMulti,        routeExport, _1));
  (*this)[FLTPLAN     ].CB(bind(&RouteExport::routeExportFltplanMulti,       routeExport, _1));
  (*this)[XFMC        ].CB(bind(&RouteExport::routeExportXFmcMulti,          routeExport, _1));
  (*this)[UFMC        ].CB(bind(&RouteExport::routeExportUFmcMulti,          routeExport, _1));
  (*this)[PROSIM      ].CB(bind(&RouteExport::routeExportProSimMulti,        routeExport, _1));
  (*this)[BBS         ].CB(bind(&RouteExport::routeExportBbsMulti,           routeExport, _1));
  (*this)[VFP         ].CB(bind(&RouteExport::routeExportVfp,                routeExport, _1));
  (*this)[IVAP        ].CB(bind(&RouteExport::routeExportIvap,               routeExport, _1));
  (*this)[XIVAP       ].CB(bind(&RouteExport::routeExportXIvap,              routeExport, _1));
  (*this)[FEELTHEREFPL].CB(bind(&RouteExport::routeExportFeelthereFplMulti,  routeExport, _1));
  (*this)[LEVELDRTE   ].CB(bind(&RouteExport::routeExportLeveldRteMulti,     routeExport, _1));
  (*this)[EFBR        ].CB(bind(&RouteExport::routeExportEfbrMulti,          routeExport, _1));
  (*this)[QWRTE       ].CB(bind(&RouteExport::routeExportQwRteMulti,         routeExport, _1));
  (*this)[MDR         ].CB(bind(&RouteExport::routeExportMdrMulti,           routeExport, _1));
  (*this)[TFDI        ].CB(bind(&RouteExport::routeExportTfdiMulti,          routeExport, _1));
  (*this)[IFLY        ].CB(bind(&RouteExport::routeExportIflyMulti,          routeExport, _1));
  (*this)[INIBUILDS   ].CB(bind(&RouteExport::routeExportFms3IniBuildsMulti, routeExport, _1));
  (*this)[PLNISG      ].CB(bind(&RouteExport::routeExportIsgMulti,           routeExport, _1));
  (*this)[PMS50       ].CB(bind(&RouteExport::routeExportPms50Multi,         routeExport, _1));
  (*this)[TDSGTNXI    ].CB(bind(&RouteExport::routeExportTdsGtnXiMulti,      routeExport, _1));
  (*this)[TDSGTNXIWP  ].CB(bind(&RouteExport::routeExportTdsGtnXiMulti,      routeExport, _1));
  /* *INDENT-ON* */

#undef CB
}

void RouteExportFormatMap::init()
{
  using namespace rexp;
  namespace ap = atools::fs::pln::pattern;

  // All text after the first linefeed is used as tooltip
  const QString rxpTooltip = tr("\nExport navaids and airports as user defined waypoints to avoid locked waypoints due to different AIRAC cycles.\n"
                                "This saves all waypoints as user defined waypoints when exporting flight plans.\n"
                                "Note that is not possible to export procedures if this is enabled.");

  const QString gpxTooltip = tr("\nGPX is exported with aircraft trail and flight plan.");

  const QString civaTooltip = tr("\nFlight plan is split up and exported into several numbered files.");

  const QString lnmTooltip = tr("\nUse this format to save and backup your plans since it covers all features like remarks and more.\n"
                                "Note that using this option is the same as using \"Save\" or \"Save as\" in the main menu \"File\".");

  const QString mainMenu = tr("\nThe given filename pattern is also used when exporting flight plans from the main menu \"File\".");

  const QString xp12 = tr("\nThe same format as FMS 11 but saved to the X-Plane 12 folder.");

  // Short "DEPARTIDENT DESTIDENT"
  // Long "PLANTYPE DEPARTNAME (DEPARTIDENT) to DESTNAME (DESTIDENT)"

  // Default format as set in options dialog
  const QString DF(ap::PLANTYPE % " " % ap::DEPARTNAME % " (" % ap::DEPARTIDENT % ") to " % ap::DESTNAME % " (" % ap::DESTIDENT % ").");

  // Departure ident and destination ident without dot
  const QString S(ap::DEPARTIDENT % ap::DESTIDENT);

  // Departure ident and destination ident plus dot
  const QString S0(ap::DEPARTIDENT % ap::DESTIDENT % ".");

  // Departure ident and destination ident separated by dash plus dot
  const QString SD(ap::DEPARTIDENT % "-" % ap::DESTIDENT % ".");

  // Departure ident and destination ident separated by underline plus dot
  const QString SU(ap::DEPARTIDENT % "_" % ap::DESTIDENT % ".");

#define FMT(type, flags, format, cat, comment) insertFmt(RouteExportFormat(type, flags, format, cat, comment))

  /* *INDENT-OFF* */
  //   type           flags            format              category             comment all after \n also used as tooltip
  FMT(LNMPLN,       NONE,                 DF % tr("lnmpln"),  tr("Little Navmap"), tr("Little Navmap native flight plan format") % lnmTooltip        );
  FMT(PLN,          AIRPORTS|PARKING,     DF % tr("pln"),     tr("Simulator"), tr("FSX and Prepar3D") % mainMenu                                     );
  FMT(PLNMSFS,      AIRPORTS|PARKING|VFR, DF % tr("pln"),     tr("Simulator"), tr("Microsoft Flight Simulator 2020") % mainMenu                      );
  FMT(PLNANNOTATED, AIRPORTS|PARKING,     DF % tr("pln"),     tr("Simulator"), tr("FSX and Prepar3D annotated\nOnly for old Little Navmap versions."));
  FMT(FMS3,         AIRPORTS,             SD % tr("fms"),     tr("Simulator"), tr("X-Plane FMS 3\nOld and limited format.")                          );
  FMT(FMS11,        AIRPORTS|CYCLE|NDALL, SD % tr("fms"),     tr("Simulator"), tr("X-Plane FMS 11") % mainMenu                                       );
  FMT(FMS12,        AIRPORTS|CYCLE|NDALL, SD % tr("fms"),     tr("Simulator"), tr("X-Plane FMS 12") % xp12                                           );
  FMT(CIVAFMS,      AIRPORTS,             S0 % tr("fms"),     tr("FMC"),       tr("X-Plane CIVA Navigation System") % civaTooltip                    );
  FMT(FLP,          AIRPORTS,             S0 % tr("flp"),     tr("Aircraft"),  tr("Aerosoft Airbus and others")                                      );
  FMT(FLPCRJ,       AIRPORTS,             S %  tr("01.flp"),  tr("Aircraft"),  tr("Aerosoft CRJ")                                                    );
  FMT(FLPCRJMSFS,   AIRPORTS,             S %  tr("01.flp"),  tr("Aircraft"),  tr("Aerosoft CRJ for MSFS")                                           );
  FMT(FLIGHTGEAR,   AIRPORTS,             DF % tr("fgfp"),    tr("Simulator"), tr("FlightGear") % mainMenu                                           );
  FMT(GFP,          AIRPORTS,             SD % tr("gfp"),     tr("Garmin"),    tr("Flight1 Garmin GTN 650/750")                                      );
  FMT(GFPUWP,       AIRPORTS|GARMIN_WP,   SD % tr("gfp"),     tr("Garmin"),    tr("Flight1 Garmin GTN 650/750 with user defined waypoints") % rxpTooltip);
  FMT(TXT,          AIRPORTS,             S0 % tr("txt"),     tr("Aircraft"),  tr("Rotate MD-80, MD-11 and others")                                  );
  FMT(TXTJAR,       AIRPORTS,             S0 % tr("txt"),     tr("Aircraft"),  tr("JARDesign aircraft")                                              );
  FMT(RTE,          AIRPORTS,             S0 % tr("rte"),     tr("Aircraft"),  tr("PMDG aircraft")                                                   );
  FMT(RTEMSFS,      AIRPORTS,             S0 % tr("rte"),     tr("Aircraft"),  tr("PMDG aircraft for MSFS")                                          );
  FMT(GPX,          NONE,                 DF % tr("gpx"),     tr("Garmin"),    tr("Garmin GPX exchange format for Google Earth and others") % gpxTooltip % mainMenu );
  FMT(HTML,         NONE,                 DF % tr("html"),    tr("Other"),     tr("HTML flight plan web page") % mainMenu                            );
  FMT(FPR,          AIRPORTS,             S0 % tr("fpr"),     tr("Aircraft"),  tr("Majestic Dash MJC8 Q400")                                         );
  FMT(FPL,          AIRPORTS,             S0 % tr("fpl"),     tr("Aircraft"),  tr("IXEG Boeing 737")                                                 );
  FMT(CORTEIN,      AIRPORTS|FILEAPP,     tr("corte.in"),     tr("Aircraft"),  tr("Flight Factor Airbus")                                            );
  FMT(RXPGNS,       AIRPORTS,             S0 % tr("fpl"),     tr("Garmin"),    tr("Reality XP GNS 530W/430W V2")                                     );
  FMT(RXPGNSUWP,    AIRPORTS|GARMIN_WP,   S0 % tr("fpl"),     tr("Garmin"),    tr("Reality XP GNS 530W/430W V2 with user defined waypoints") % rxpTooltip);
  FMT(RXPGTN,       AIRPORTS,             SU % tr("gfp"),     tr("Garmin"),    tr("Reality XP GTN 750/650 Touch")                                    );
  FMT(RXPGTNUWP,    AIRPORTS|GARMIN_WP,   SU % tr("gfp"),     tr("Garmin"),    tr("Reality XP GTN 750/650 Touch with user defined waypoints") % rxpTooltip);
  FMT(FLTPLAN,      AIRPORTS,             S0 % tr("fltplan"), tr("Aircraft"),  tr("iFly")                                                            );
  FMT(XFMC,         AIRPORTS,             S0 % tr("fpl"),     tr("FMC"),       tr("X-FMC")                                                           );
  FMT(UFMC,         AIRPORTS,             S0 % tr("ufmc"),    tr("FMC"),       tr("UFMC")                                                            );
  FMT(PROSIM,       AIRPORTS|FILEAPP,     tr("companyroutes.xml"), tr("Simulator"), tr("ProSim")                                                     );
  FMT(BBS,          AIRPORTS,             S0 % tr("pln"),     tr("Aircraft"),  tr("BlackBox Simulations Airbus")                                     );
  FMT(VFP,          AIRPORTS,             S0 % tr("vfp"),     tr("Online"),    tr("VATSIM vPilot, xPilot or SWIFT") % mainMenu                       );
  FMT(IVAP,         AIRPORTS,             S0 % tr("fpl"),     tr("Online"),    tr("IvAp for IVAO") % mainMenu                                        );
  FMT(XIVAP,        AIRPORTS,             S0 % tr("fpl"),     tr("Online"),    tr("X-IVAP for IVAO") % mainMenu                                      );
  FMT(FEELTHEREFPL, AIRPORTS,             SU % tr("fpl"),     tr("Aircraft"),  tr("FeelThere or Wilco")                                              );
  FMT(LEVELDRTE,    AIRPORTS,             SU % tr("rte"),     tr("Aircraft"),  tr("Level-D")                                                         );
  FMT(EFBR,         AIRPORTS,             SU % tr("efbr"),    tr("Other"),     tr("AivlaSoft EFB")                                                   );
  FMT(QWRTE,        AIRPORTS,             S0 % tr("rte"),     tr("Aircraft"),  tr("QualityWings")                                                    );
  FMT(MDR,          AIRPORTS,             S0 % tr("mdr"),     tr("Aircraft"),  tr("Leonardo Maddog X")                                               );
  FMT(TFDI,         AIRPORTS,             S0 % tr("xml"),     tr("Aircraft"),  tr("TFDi Design 717")                                                 );
  FMT(IFLY,         AIRPORTS,             S0 % tr("route"),   tr("Aircraft"),  tr("iFly Jets Advanced Series")                                       );
  FMT(INIBUILDS,    AIRPORTS,             S0 % tr("fpl"),     tr("Aircraft"),  tr("IniBuilds Airbus for MSFS")                                       );
  FMT(PLNISG,       AIRPORTS,             S0 % tr("pln"),     tr("FMS"),       tr("ISG Integrated Simavionics gauges")                               );
  FMT(PMS50,        FILEREP|AIRPORTS,     tr("fpl.pln"),      tr("Garmin"),    tr("PMS50 GTN750")                                                    );
  FMT(TDSGTNXI,     AIRPORTS,             SU % tr("gfp"),     tr("Garmin"),    tr("TDS GTNXi")                                                       );
  FMT(TDSGTNXIWP,   AIRPORTS|GARMIN_WP,   SU % tr("gfp"),     tr("Garmin"),    tr("TDS GTNXi with user defined waypoints")                           );
  /* *INDENT-ON* */

#undef FMT
}

void RouteExportFormatMap::insertFmt(const RouteExportFormat& fmt)
{
  insertFmt(fmt.getType(), fmt);
}

void RouteExportFormatMap::insertFmt(rexp::RouteExportFormatType type, const RouteExportFormat& fmt)
{
  if(contains(type))
    qWarning() << Q_FUNC_INFO << "Duplicate format" << type << fmt.getComment();
  insert(type, fmt);
}

void RouteExportFormatMap::updateDefaultPaths()
{
  using atools::SEP;
  FsPaths::SimulatorType curDb = NavApp::getCurrentSimulatorDb();

  // Documents path as fallback or for unknown ===========================
  QString documents = atools::documentsDir();

  // Get X-Plane base path - first 12 if available then 11 ===========================
  QString xpBasePath12Or11 = NavApp::getSimulatorBasePathBest({FsPaths::XPLANE_12, FsPaths::XPLANE_11});

  // Files path for 12 or 11 =============
  QString xpFilesPath12Or11 = NavApp::getSimulatorFilesPathBest({FsPaths::XPLANE_12, FsPaths::XPLANE_11}, documents);

  // Files path for 12 only =============
  QString xpFilesPath12 = NavApp::getSimulatorFilesPathBest({FsPaths::XPLANE_12}, documents);

  // Files path for 11 only =============
  QString xpFilesPath11 = NavApp::getSimulatorFilesPathBest({FsPaths::XPLANE_11}, documents);

  // Get MSFS files path (LocalState) ===========================
  // C:\Users\USER\AppData\Local\Packages\Microsoft.FlightSimulator_8wekyb3d8bbwe\LocalState
  // Steam uses top level as path
  // C:\Users\USER\AppData\Roaming\Microsoft Flight Simulator
  QString msfsLocalStatePath = NavApp::getSimulatorFilesPathBest({FsPaths::MSFS}, documents);

  // .../Packages/Microsoft.FlightSimulator_8wekyb3d8bbwe/LocalCache/Packages/
  QString msfsBasePath = NavApp::getSimulatorBasePathBest({FsPaths::MSFS});
  if(msfsBasePath.isEmpty())
    msfsBasePath = documents;

  // Get base path of best MS simulator except MSFS - FSX and P3D ===========================
  QString fsxP3dBasePath;

  // Get for current database selection if not X-Plane or MSFS
  if(curDb != FsPaths::XPLANE_11 && curDb != FsPaths::XPLANE_12 && curDb != FsPaths::MSFS && curDb != FsPaths::NAVIGRAPH)
    fsxP3dBasePath = NavApp::getSimulatorFilesPathBest({curDb}, QString());

  // Get best installed simulator
  if(fsxP3dBasePath.isEmpty())
    fsxP3dBasePath = NavApp::getSimulatorFilesPathBest({FsPaths::P3D_V6, FsPaths::P3D_V5, FsPaths::P3D_V4, FsPaths::P3D_V3, FsPaths::FSX_SE,
                                                        FsPaths::FSX}, documents);

  // GNS path ===========================
  QString gns;
#ifdef Q_OS_WIN32
  QString gnsPath(QProcessEnvironment::systemEnvironment().value("GNSAPPDATA"));
  gns = gnsPath.isEmpty() ? QString("C:\\ProgramData\\Garmin\\GNS Trainer Data\\GNS\\FPL") : gnsPath % "\\FPL";
#elif DEBUG_INFORMATION
  gns = atools::buildPath({documents, "Garmin", "GNS Trainer Data", "GNS", "FPL"});
#else
  gns = documents;
#endif

  // GTN path ===========================
  QString gtn;
  // Location depends on trainer version - this is all above 6.41
#ifdef Q_OS_WIN32
  QString gtnPath(QProcessEnvironment::systemEnvironment().value("GTNSIMDATA"));
  gtn = gtnPath.isEmpty() ? QString("C:\\ProgramData\\Garmin\\Trainers\\Databases\\FPLN") : gtnPath % "\\Databases\\FPLN";
#elif DEBUG_INFORMATION
  gtn = atools::buildPath({documents, "Garmin", "Trainers", "GTN", "FPLN"});
#else
  gtn = documents;
#endif

  // TDS path ===========================
  QString tdsGtmGfp;
  // Location depends on trainer version - this is all above 6.41
#ifdef Q_OS_WIN32
  tdsGtmGfp = QString("C:\\ProgramData\\TDS\\GTNXi\\FPL");
#elif DEBUG_INFORMATION
  tdsGtmGfp = atools::buildPath({documents, "TDS", "GTNXi", "FPL"});
#else
  tdsGtmGfp = documents;
#endif

  // Normalize path endings for base paths
  if(xpBasePath12Or11.endsWith('\\') || xpBasePath12Or11.endsWith('/'))
    xpBasePath12Or11.chop(1);
  if(fsxP3dBasePath.endsWith('\\') || fsxP3dBasePath.endsWith('/'))
    fsxP3dBasePath.chop(1);
  if(msfsBasePath.endsWith('\\') || msfsBasePath.endsWith('/'))
    msfsBasePath.chop(1);

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  QString lnmplnFiles = settings.valueStr(lnm::ROUTE_LNMPLN_EXPORTDIR, documents);

  // Build IniBuilds export path
  QString iniBuildsMsfsPath;
  switch(FsPaths::getMsfsInstallType())
  {
    case atools::fs::FsPaths::MSFS_INSTALL_NONE:
      break;

    case atools::fs::FsPaths::MSFS_INSTALL_ONLINE:
    case atools::fs::FsPaths::MSFS_INSTALL_BOXED: // Unknown
      // C:\Users\YOURUSERNAME\AppData\Local\Packages\Microsoft.FlightSimulator_8wekyb3d8bbwe\LocalState\packages\microsoft-aircraft-a310-300\work\flightplans
      iniBuildsMsfsPath = msfsLocalStatePath % SEP % "packages" % SEP % "microsoft-aircraft-a310-300" % SEP % "work" % SEP % "flightplans";
      break;

    case atools::fs::FsPaths::MSFS_INSTALL_STEAM:
      // C:\Users\YOURUSERNAME\AppData\Roaming\Microsoft Flight Simulator\Packages\microsoft-aircraft-a310-300\work\flightplans
      iniBuildsMsfsPath = msfsLocalStatePath % SEP % "Packages" % SEP % "microsoft-aircraft-a310-300" % SEP % "work" % SEP % "flightplans";
      break;
  }

  using namespace rexp;

#define DP setDefaultPath

  // Fill default paths
  /* *INDENT-OFF* */
  (*this)[LNMPLN      ].DP(lnmplnFiles);
  (*this)[PLN         ].DP(fsxP3dBasePath);
  (*this)[PLNMSFS     ].DP(msfsLocalStatePath);
  (*this)[PLNANNOTATED].DP(fsxP3dBasePath);
  (*this)[FMS3        ].DP(xpFilesPath12Or11);
  (*this)[FMS11       ].DP(xpFilesPath11);
  (*this)[FMS12       ].DP(xpFilesPath12);
  (*this)[CIVAFMS     ].DP(xpFilesPath12Or11);
  (*this)[FLP         ].DP(documents);
  (*this)[FLPCRJ      ].DP(documents % SEP % "Aerosoft" % SEP % "Digital Aviation CRJ" % SEP % "FlightPlans");
  (*this)[FLPCRJMSFS  ].DP(documents);
  (*this)[FLIGHTGEAR  ].DP(documents);
  (*this)[GFP         ].DP(fsxP3dBasePath % SEP % "F1TGTN" % SEP % "FPL");
  (*this)[GFPUWP      ].DP(fsxP3dBasePath % SEP % "F1TGTN" % SEP % "FPL");
  (*this)[TXT         ].DP(xpBasePath12Or11 % SEP % "Aircraft");
  (*this)[TXTJAR      ].DP(xpBasePath12Or11 % SEP % "Aircraft");
  (*this)[RTE         ].DP(fsxP3dBasePath % SEP % "PMDG" % SEP % "FLIGHTPLANS");
  (*this)[RTEMSFS     ].DP(msfsLocalStatePath % SEP % "packages" % SEP % "pmdg-aircraft-737" % SEP % "work" % SEP % "Flightplans");
  (*this)[GPX         ].DP(documents);
  (*this)[HTML        ].DP(documents);
  (*this)[FPR         ].DP(fsxP3dBasePath % SEP % "SimObjects" % SEP % "Airplanes" % SEP % "mjc8q400" % SEP % "nav" % SEP % "routes");
  (*this)[FPL         ].DP(xpBasePath12Or11 % SEP % "Aircraft" % SEP % "X-Aviation" % SEP % "IXEG 737 Classic" % SEP % "coroutes");
  (*this)[CORTEIN     ].DP(xpBasePath12Or11 % SEP % "Aircraft");
  (*this)[RXPGNS      ].DP(gns);
  (*this)[RXPGNSUWP   ].DP(gns);
  (*this)[RXPGTN      ].DP(gtn);
  (*this)[RXPGTNUWP   ].DP(gtn);
  (*this)[FLTPLAN     ].DP(fsxP3dBasePath % SEP % "iFly" % SEP % "737NG" % SEP % "navdata" % SEP % "FLTPLAN");
  (*this)[XFMC        ].DP(xpFilesPath12Or11 % SEP % "Resources" % SEP % "plugins" % SEP % "XFMC" % SEP % "FlightPlans");
  (*this)[UFMC        ].DP(documents);
  (*this)[PROSIM      ].DP(documents);
  (*this)[BBS         ].DP(fsxP3dBasePath % SEP % "Blackbox Simulation" % SEP % "Company Routes");
  (*this)[VFP         ].DP(documents);
  (*this)[IVAP        ].DP(documents);
  (*this)[XIVAP       ].DP(documents);
  (*this)[FEELTHEREFPL].DP(fsxP3dBasePath);
  (*this)[LEVELDRTE   ].DP(fsxP3dBasePath % SEP % "Level-D Simulations" % SEP % "navdata" % SEP % "Flightplans");
  (*this)[EFBR        ].DP(documents);
  (*this)[QWRTE       ].DP(fsxP3dBasePath);
  (*this)[MDR         ].DP(fsxP3dBasePath);
  (*this)[TFDI        ].DP(fsxP3dBasePath % SEP % "SimObjects" % SEP % "Airplanes" % SEP % "TFDi_Design_717" % SEP % "Documents" % SEP % "Company Routes");
  (*this)[IFLY        ].DP(documents % SEP % "Prepar3D v5 Add-ons" % SEP % "iFlyData" % SEP % "navdata" % SEP % "FLTPLAN");
  (*this)[INIBUILDS   ].DP(iniBuildsMsfsPath);
  (*this)[PLNISG      ].DP(fsxP3dBasePath % SEP % "ISG" % SEP % "FlightPlans"); // C:\Program Files\Lockheed Martin\Prepar3D v4\ISG\FlightPlans
  (*this)[PMS50       ].DP(msfsBasePath % SEP % "Community" % SEP % "pms50-instrument-gtn750" % SEP % "fpl" % SEP % "gtn750");
  (*this)[TDSGTNXI    ].DP(tdsGtmGfp);
  (*this)[TDSGTNXIWP  ].DP(tdsGtmGfp);
  /* *INDENT-ON* */
#undef DP

  // Cleanup paths and convert to native notation
  for(RouteExportFormat& format : *this)
  {
    format.setDefaultPath(atools::nativeCleanPath(format.getDefaultPath()));
    if(format.getPath().isEmpty())
      format.setPath(format.getDefaultPath());
    else
      format.setPath(atools::nativeCleanPath(format.getPath()));
  }
}

bool RouteExportFormat::isPatternValid(QString *errorMessage) const
{
  QString errors;
  atools::fs::pln::Flightplan::getFilenamePatternExample(pattern, QFileInfo(defaultPattern).suffix(), false /* html */, &errors);

  if(errorMessage != nullptr)
    *errorMessage = errors;

  return errors.isEmpty();
}

bool RouteExportFormat::isPathValid(QString *errorMessage) const
{
  if(errorMessage != nullptr)
    *errorMessage = pathError;
  return pathError.isEmpty();
}

void RouteExportFormat::updatePathError()
{
  pathError.clear();

  if(isAppendToFile())
  {
    QFileInfo file(path % QDir::separator() % pattern);

    if(!file.exists())
      pathError = tr("File \"%1\" does not exist.").
                  arg(atools::elideTextShortLeft(atools::nativeCleanPath(file.absoluteFilePath()), 100));
    else if(!file.isFile())
      pathError = tr("Expected file but \"%1\" is a directory.").
                  arg(atools::elideTextShortLeft(atools::nativeCleanPath(file.absoluteFilePath()), 100));
  }
  else
  {
    QFileInfo dir(path);

    if(!dir.exists())
      pathError = tr("Directory \"%1\" does not exist").
                  arg(atools::elideTextShortLeft(atools::nativeCleanPath(dir.absoluteFilePath()), 100));
    else if(!dir.isDir())
      pathError = tr("Expected directory but \"%1\" is a file.").
                  arg(atools::elideTextShortLeft(atools::nativeCleanPath(dir.absoluteFilePath()), 100));
  }
}

void RouteExportFormat::correctPattern()
{
  if(pattern.simplified().isEmpty())
    pattern = defaultPattern;
}

void RouteExportFormat::setPath(const QString& value)
{
  path = value;
  updatePathError();
}

QString RouteExportFormat::getFilter() const
{
  if(isAppendToFile() || isReplaceFile())
    return "(" % pattern % ")";
  else
    return "(*." % pattern.section('.', -1, -1) % ")";
}

QString RouteExportFormat::getFormat() const
{
  if(isAppendToFile() || isReplaceFile())
    return pattern;
  else
    return pattern.section('.', -1, -1).toUpper();
}

QString RouteExportFormat::getSuffix() const
{
  if(isAppendToFile() || isReplaceFile())
    return pattern;
  else
  {
    QString suffix = QFileInfo(pattern).suffix();
    if(!suffix.isEmpty())
      suffix.prepend('.');

    return suffix;
  }
}

void RouteExportFormat::copyLoadedDataTo(RouteExportFormat& other) const
{
  other.path = atools::nativeCleanPath(path);
  other.pattern = pattern;
  other.flags.setFlag(rexp::SELECTED, flags.testFlag(rexp::SELECTED));
}

QDataStream& operator>>(QDataStream& dataStream, RouteExportFormat& obj)
{
  quint16 typeInt;
  dataStream >> typeInt;
  obj.type = static_cast<rexp::RouteExportFormatType>(typeInt);

  quint16 flagsInt;
  dataStream >> flagsInt;
  obj.flags |= rexp::RouteExportFormatFlags(flagsInt);

  dataStream >> obj.path;

  if(RouteExportFormatMap::getVersion() >= RouteExportFormatMap::FILE_VERSION_CURRENT)
    // Load pattern in new version
    dataStream >> obj.pattern;

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const RouteExportFormat& obj)
{
  dataStream << static_cast<quint16>(obj.type)
             << static_cast<quint16>(obj.flags & rexp::SAVED_FLAGS)
             << obj.path << obj.pattern;

  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, RouteExportFormatMap& obj)
{
  quint32 magicNumber;
  dataStream >> magicNumber >> RouteExportFormatMap::version;

  if(magicNumber != RouteExportFormatMap::FILE_MAGIC_NUMBER)
  {
    if(RouteExportFormatMap::exceptionOnReadError)
      throw atools::Exception(QObject::tr("Error reading multiexport configuration: Invalid magic number. "
                                          "Not a multiexport configuration."));
    else
      qWarning() << Q_FUNC_INFO << "Invalid magic number" << magicNumber << "expected" << RouteExportFormatMap::FILE_MAGIC_NUMBER;
  }

  if(RouteExportFormatMap::version < RouteExportFormatMap::FILE_VERSION_MIN ||
     RouteExportFormatMap::version > RouteExportFormatMap::FILE_VERSION_CURRENT)
  {
    if(RouteExportFormatMap::exceptionOnReadError)
      throw atools::Exception(QObject::tr("Error reading multiexport configuration: Invalid version. "
                                          "Incompatible multiexport configuration."));
    else
      qWarning() << Q_FUNC_INFO << "Invalid version number" << RouteExportFormatMap::version
                 << "expected" << RouteExportFormatMap::FILE_VERSION_MIN << "to" << RouteExportFormatMap::FILE_VERSION_CURRENT;
  }

  quint16 size;
  dataStream >> size;

  for(int i = 0; i < size; i++)
  {
    RouteExportFormat fmt;
    dataStream >> fmt;
    obj.insert(fmt.getType(), fmt);
  }

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const RouteExportFormatMap& obj)
{
  dataStream << RouteExportFormatMap::FILE_MAGIC_NUMBER << RouteExportFormatMap::FILE_VERSION_CURRENT;

  dataStream << static_cast<quint16>(obj.size());
  for(const RouteExportFormat& fmt : obj)
    dataStream << fmt;
  return dataStream;
}
