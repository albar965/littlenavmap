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

#include "routeexport/routeexportformat.h"

#include "routeexport/routeexport.h"
#include "common/constants.h"
#include "atools.h"
#include "settings/settings.h"
#include "navapp.h"
#include "gui/errorhandler.h"

#include <QDataStream>
#include <QDir>
#include <QProcessEnvironment>
#include <exception.h>

using atools::settings::Settings;

// Simply log warnings instead of throwing exceptions on read errors
bool RouteExportFormatMap::exceptionOnReadError = false;

QVector<RouteExportFormat> RouteExportFormatMap::getSelected() const
{
  QVector<RouteExportFormat> retval;
  for(const RouteExportFormat& fmt : (*this))
  {
    if(fmt.isSelected())
      retval.append(fmt);
  }
  return retval;
}

void RouteExportFormatMap::clearPath(rexp::RouteExportFormatType type)
{
  (*this)[type].setPath((*this)[type].getDefaultPath());
}

void RouteExportFormatMap::updatePath(rexp::RouteExportFormatType type, const QString& path)
{
  (*this)[type].setPath(QDir::toNativeSeparators(path));
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

  // Load selection status and user updated paths from settings
  atools::settings::Settings& settings = Settings::instance();
  RouteExportFormatMap formatList;
  try
  {
    formatList = settings.valueVar(lnm::ROUTE_EXPORT_FORMATS).value<RouteExportFormatMap>();
  }
  catch(atools::Exception& e)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(NavApp::getQMainWidget()).handleException(e);
  }
  catch(...)
  {
    NavApp::deleteSplashScreen();
    atools::gui::ErrorHandler(NavApp::getQMainWidget()).handleUnknownException();
  }

  for(const RouteExportFormat& loadedFmt : formatList)
  {
    RouteExportFormat& f = (*this)[loadedFmt.getType()];

    if(contains(loadedFmt.getType()))
      loadedFmt.copyLoadedData(f);
    else
      qWarning() << Q_FUNC_INFO << "Type not found in internal list" << loadedFmt.getTypeAsInt();

    f.updatePathError();
  }
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
  (*this)[LNMPLN      ].CB(bind(&RouteExport::routeExportLnm,               routeExport, _1));
  (*this)[PLN         ].CB(bind(&RouteExport::routeExportPln,               routeExport, _1));
  (*this)[PLNMSFS     ].CB(bind(&RouteExport::routeExportPlnMsfs,           routeExport, _1));
  (*this)[PLNANNOTATED].CB(bind(&RouteExport::routeExportPlnAnnotatedMulti, routeExport, _1));
  (*this)[FMS3        ].CB(bind(&RouteExport::routeExportFms3Multi,         routeExport, _1));
  (*this)[FMS11       ].CB(bind(&RouteExport::routeExportFms11,             routeExport, _1));
  (*this)[FLP         ].CB(bind(&RouteExport::routeExportFlpMulti,          routeExport, _1));
  (*this)[FLPCRJ      ].CB(bind(&RouteExport::routeExportFlpCrjMulti,       routeExport, _1));
  (*this)[FLIGHTGEAR  ].CB(bind(&RouteExport::routeExportFlightgear,        routeExport, _1));
  (*this)[GFP         ].CB(bind(&RouteExport::routeExportGfpMulti,          routeExport, _1));
  (*this)[GFPUWP      ].CB(bind(&RouteExport::routeExportGfpMulti,          routeExport, _1));
  (*this)[TXT         ].CB(bind(&RouteExport::routeExportTxtMulti,          routeExport, _1));
  (*this)[RTE         ].CB(bind(&RouteExport::routeExportRteMulti,          routeExport, _1));
  (*this)[GPX         ].CB(bind(&RouteExport::routeExportGpx,               routeExport, _1));
  (*this)[HTML        ].CB(bind(&RouteExport::routeExportHtml,              routeExport, _1));
  (*this)[FPR         ].CB(bind(&RouteExport::routeExportFprMulti,          routeExport, _1));
  (*this)[FPL         ].CB(bind(&RouteExport::routeExportFplMulti,          routeExport, _1));
  (*this)[CORTEIN     ].CB(bind(&RouteExport::routeExportCorteInMulti,      routeExport, _1));
  (*this)[RXPGNS      ].CB(bind(&RouteExport::routeExportRxpGnsMulti,       routeExport, _1));
  (*this)[RXPGNSUWP   ].CB(bind(&RouteExport::routeExportRxpGnsMulti,       routeExport, _1));
  (*this)[RXPGTN      ].CB(bind(&RouteExport::routeExportRxpGtnMulti,       routeExport, _1));
  (*this)[RXPGTNUWP   ].CB(bind(&RouteExport::routeExportRxpGtnMulti,       routeExport, _1));
  (*this)[FLTPLAN     ].CB(bind(&RouteExport::routeExportFltplanMulti,      routeExport, _1));
  (*this)[XFMC        ].CB(bind(&RouteExport::routeExportXFmcMulti,         routeExport, _1));
  (*this)[UFMC        ].CB(bind(&RouteExport::routeExportUFmcMulti,         routeExport, _1));
  (*this)[PROSIM      ].CB(bind(&RouteExport::routeExportProSimMulti,       routeExport, _1));
  (*this)[BBS         ].CB(bind(&RouteExport::routeExportBbsMulti,          routeExport, _1));
  (*this)[VFP         ].CB(bind(&RouteExport::routeExportVfp,               routeExport, _1));
  (*this)[IVAP        ].CB(bind(&RouteExport::routeExportIvap,              routeExport, _1));
  (*this)[XIVAP       ].CB(bind(&RouteExport::routeExportXIvap,             routeExport, _1));
  (*this)[FEELTHEREFPL].CB(bind(&RouteExport::routeExportFeelthereFplMulti, routeExport, _1));
  (*this)[LEVELDRTE   ].CB(bind(&RouteExport::routeExportLeveldRteMulti,    routeExport, _1));
  (*this)[EFBR        ].CB(bind(&RouteExport::routeExportEfbrMulti,         routeExport, _1));
  (*this)[QWRTE       ].CB(bind(&RouteExport::routeExportQwRteMulti,        routeExport, _1));
  (*this)[MDR         ].CB(bind(&RouteExport::routeExportMdrMulti,          routeExport, _1));
  (*this)[TFDI        ].CB(bind(&RouteExport::routeExportTfdiMulti,         routeExport, _1));
  /* *INDENT-ON* */

#undef CB
}

void RouteExportFormatMap::init()
{
  using namespace rexp;
  using rexp::FILE;
  using rexp::NONE;

  QString rxptooltip = tr("\nExport navaids and airports as user defined waypoints avoid locked waypoints due to different AIRAC cycles.\n"
                          "This saves all waypoints as user defined waypoints when exporting flight plans.\n"
                          "Note that is not possible to export procedures if this is enabled.");

#define RF RouteExportFormat
#define INS insert

  /* *INDENT-OFF* */
  //  type            (type          flags             format                 category          comment all after \n also used as tooltip
  INS(LNMPLN,       RF(LNMPLN,       NONE,             tr("lnmpln"),          tr("Little Navmap"), tr("Little Navmap native flight plan format\n"
                                                                                                      "Use this format to save and backup your plans since it covers "
                                                                                                      "all features like remarks and more.\n"
                                                                                                      "Note that using this option is the same as using "
                                                                                                      "\"Save\" or \"Save as\" in the file menu.")                   ));
  INS(PLN,          RF(PLN,          AIRPORTS|PARKING, tr("pln"),             tr("Simulator"), tr("FSX and Prepar3D")                                                ));
  INS(PLNMSFS,      RF(PLNMSFS,      AIRPORTS/*|PARKING*/, tr("pln"),         tr("Simulator"), tr("Microsoft Flight Simulator 2020")                                 ));
  INS(PLNANNOTATED, RF(PLNANNOTATED, AIRPORTS|PARKING, tr("pln"),             tr("Simulator"), tr("FSX and Prepar3D annotated\nOnly for old Little Navmap versions.")));
  INS(FMS3,         RF(FMS3,         AIRPORTS,         tr("fms"),             tr("Simulator"), tr("X-Plane FMS 3\nOld limited format.")                              ));
  INS(FMS11,        RF(FMS11,        AIRPORTS|CYCLE,   tr("fms"),             tr("Simulator"), tr("X-Plane FMS 11")                                                  ));
  INS(FLP,          RF(FLP,          AIRPORTS,         tr("flp"),             tr("Aircraft"),  tr("Aerosoft Airbus and others")                                      ));
  INS(FLPCRJ,       RF(FLPCRJ,       AIRPORTS,         tr("flp"),             tr("Aircraft"),  tr("Aerosoft CRJ")                                                    ));
  INS(FLIGHTGEAR,   RF(FLIGHTGEAR,   AIRPORTS,         tr("fgfp"),            tr("Simulator"), tr("FlightGear")                                                      ));
  INS(GFP,          RF(GFP,          AIRPORTS,         tr("gfp"),             tr("Garmin"),    tr("Flight1 Garmin GTN 650/750")                                      ));
  INS(GFPUWP,       RF(GFPUWP,       AIRPORTS|GARMIN_AS_WAYPOINTS, tr("gfp"), tr("Garmin"),    tr("Flight1 Garmin GTN 650/750 with user defined waypoints") +
                                                                                               rxptooltip                                                            ));
  INS(TXT,          RF(TXT,          AIRPORTS,         tr("txt"),             tr("Aircraft"),  tr("Rotate MD-80, JARDesign and others")                              ));
  INS(RTE,          RF(RTE,          AIRPORTS,         tr("rte"),             tr("Aircraft"),  tr("PMDG aircraft")                                                   ));
  INS(GPX,          RF(GPX,          NONE,             tr("gpx"),             tr("Other"),     tr("Garmin exchange format for Google Earth and others\n"
                                                                                                  "Exported with aircraft track and flight plan.")                   ));
  INS(HTML,         RF(HTML,         NONE,             tr("html"),            tr("Other"),     tr("HTML flight plan web page")                                       ));
  INS(FPR,          RF(FPR,          AIRPORTS,         tr("fpr"),             tr("Aircraft"),  tr("Majestic Dash MJC8 Q400")                                         ));
  INS(FPL,          RF(FPL,          AIRPORTS,         tr("fpl"),             tr("Aircraft"),  tr("IXEG Boeing 737")                                                 ));
  INS(CORTEIN,      RF(CORTEIN,      FILE|AIRPORTS,    tr("corte.in"),        tr("Aircraft"),  tr("Flight Factor Airbus")                                            ));
  INS(RXPGNS,       RF(RXPGNS,       AIRPORTS,         tr("fpl"),             tr("Garmin"),    tr("Reality XP GNS 530W/430W V2")                                     ));
  INS(RXPGNSUWP,    RF(RXPGNSUWP,    AIRPORTS|GARMIN_AS_WAYPOINTS, tr("fpl"), tr("Garmin"),    tr("Reality XP GNS 530W/430W V2 with user defined waypoints") +
                                                                                               rxptooltip                                                            ));
  INS(RXPGTN,       RF(RXPGTN,       AIRPORTS,         tr("gfp"),             tr("Garmin"),    tr("Reality XP GTN 750/650 Touch")                                    ));
  INS(RXPGTNUWP,    RF(RXPGTNUWP,    AIRPORTS|GARMIN_AS_WAYPOINTS, tr("gfp"), tr("Garmin"),    tr("Reality XP GTN 750/650 Touch with user defined waypoints") +
                                                                                               rxptooltip                                                            ));
  INS(FLTPLAN,      RF(FLTPLAN,      AIRPORTS,         tr("fltplan"),         tr("Aircraft"),  tr("iFly")                                                            ));
  INS(XFMC,         RF(XFMC,         AIRPORTS,         tr("fpl"),             tr("FMC"),       tr("X-FMC")                                                           ));
  INS(UFMC,         RF(UFMC,         AIRPORTS,         tr("ufmc"),            tr("FMC"),       tr("UFMC")                                                            ));
  INS(PROSIM,       RF(PROSIM,       FILE|AIRPORTS,    tr("companyroutes.xml"), tr("Simulator"), tr("ProSim")                                                        ));
  INS(BBS,          RF(BBS,          AIRPORTS,         tr("pln"),             tr("Aircraft"),  tr("BlackBox Simulations Airbus")                                     ));
  INS(VFP,          RF(VFP,          AIRPORTS,         tr("vfp"),             tr("Online"),    tr("VATSIM vPilot or SWIFT")                                          ));
  INS(IVAP,         RF(IVAP,         AIRPORTS,         tr("fpl"),             tr("Online"),    tr("IvAp for IVAO")                                                   ));
  INS(XIVAP,        RF(XIVAP,        AIRPORTS,         tr("fpl"),             tr("Online"),    tr("X-IVAP for IVAO")                                                 ));
  INS(FEELTHEREFPL, RF(FEELTHEREFPL, AIRPORTS,         tr("fpl"),             tr("Aircraft"),  tr("FeelThere or Wilco")                                              ));
  INS(LEVELDRTE,    RF(LEVELDRTE,    AIRPORTS,         tr("rte"),             tr("Aircraft"),  tr("Level-D")                                                         ));
  INS(EFBR,         RF(EFBR,         AIRPORTS,         tr("efbr"),            tr("Other"),     tr("AivlaSoft EFB")                                                   ));
  INS(QWRTE,        RF(QWRTE,        AIRPORTS,         tr("rte"),             tr("Aircraft"),  tr("QualityWings")                                                    ));
  INS(MDR,          RF(MDR,          AIRPORTS,         tr("mdr"),             tr("Aircraft"),  tr("Leonardo Maddog X")                                               ));
  INS(TFDI,         RF(TFDI,         AIRPORTS,         tr("xml"),             tr("Aircraft"),  tr("TFDi Design 717")                                                 ));
  /* *INDENT-ON* */

#undef RF
#undef INS
}

void RouteExportFormatMap::updateDefaultPaths()
{
  QChar SEP = QDir::separator();

  // Documents path ===========================
  QString documents = atools::documentsDir();

  // Get X-Plane base path ===========================
  // routeExportFms11 and routeExportFms3
  QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
  if(xpBasePath.isEmpty())
    xpBasePath = documents;
  else
    xpBasePath = atools::buildPathNoCase({xpBasePath, "Output", "FMS plans"});

  // Get MSFS base path ===========================
  QString msfsBasePath = NavApp::getSimulatorFilesPath(atools::fs::FsPaths::MSFS);
  if(msfsBasePath.isEmpty())
    msfsBasePath = documents;

  // Get base path of best MS simulator except MSFS ===========================
  QString fsxP3dBasePath;
  switch(NavApp::getCurrentSimulatorDb())
  {
    case atools::fs::FsPaths::FSX:
    case atools::fs::FsPaths::FSX_SE:
    case atools::fs::FsPaths::P3D_V2:
    case atools::fs::FsPaths::P3D_V3:
    case atools::fs::FsPaths::P3D_V4:
    case atools::fs::FsPaths::P3D_V5:
      // Use file location for currently selected simulator
      fsxP3dBasePath = NavApp::getSimulatorFilesPath(NavApp::getCurrentSimulatorDb());
      break;

    case atools::fs::FsPaths::XPLANE11:
    case atools::fs::FsPaths::NAVIGRAPH:
    case atools::fs::FsPaths::MSFS:
    case atools::fs::FsPaths::ALL_SIMULATORS:
    case atools::fs::FsPaths::UNKNOWN:
      // Get file location for latest / best installed simulator
      fsxP3dBasePath = NavApp::getSimulatorFilesPathBestFsxP3d();
      break;
  }

  if(fsxP3dBasePath.isEmpty())
    fsxP3dBasePath = msfsBasePath;
  if(fsxP3dBasePath.isEmpty())
    fsxP3dBasePath = documents;

  // GNS path ===========================
  QString gns;
#ifdef Q_OS_WIN32
  QString gnsPath(QProcessEnvironment::systemEnvironment().value("GNSAPPDATA"));
  gns = gnsPath.isEmpty() ? "C:\\ProgramData\\Garmin\\GNS Trainer Data\\GNS\\FPL" : gnsPath + "\\FPL";
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
  gtn = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\Databases\\FPLN" : gtnPath + "\\Databases\\FPLN";
#elif DEBUG_INFORMATION
  gtn = atools::buildPath({documents, "Garmin", "Trainers", "GTN", "FPLN"});
#else
  gtn = documents;
#endif

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  QString lnmplnFiles = settings.valueStr("Route/LnmPlnFileDialogDir", documents);

  using namespace rexp;

#define DP setDefaultPath

  // Fill default paths
  /* *INDENT-OFF* */
  (*this)[LNMPLN      ].DP(lnmplnFiles);
  (*this)[PLN         ].DP(fsxP3dBasePath);
  (*this)[PLNMSFS     ].DP(msfsBasePath);
  (*this)[PLNANNOTATED].DP(fsxP3dBasePath);
  (*this)[FMS3        ].DP(xpBasePath);
  (*this)[FMS11       ].DP(xpBasePath);
  (*this)[FLP         ].DP(documents);
  (*this)[FLPCRJ      ].DP(documents + SEP + "Aerosoft" + SEP + "Digital Aviation CRJ" + SEP + "FlightPlans");
  (*this)[FLIGHTGEAR  ].DP(documents);
  (*this)[GFP         ].DP(fsxP3dBasePath + SEP + "F1TGTN" + SEP + "FPL");
  (*this)[GFPUWP      ].DP(fsxP3dBasePath + SEP + "F1TGTN" + SEP + "FPL");
  (*this)[TXT         ].DP(fsxP3dBasePath + SEP + "Aircraft");
  (*this)[RTE         ].DP(fsxP3dBasePath + SEP + "PMDG" + SEP + "FLIGHTPLANS");
  (*this)[GPX         ].DP(documents);
  (*this)[HTML        ].DP(documents);
  (*this)[FPR         ].DP(fsxP3dBasePath + SEP + "SimObjects" + SEP + "Airplanes" + SEP + "mjc8q400" + SEP + "nav" + SEP + "routes");
  (*this)[FPL         ].DP(xpBasePath + SEP + "Aircraft" + SEP + "X-Aviation" + SEP + "IXEG 737 Classic" + SEP + "coroutes");
  (*this)[CORTEIN     ].DP(xpBasePath + SEP + "Aircraft" + SEP + "corte.in");
  (*this)[RXPGNS      ].DP(gns);
  (*this)[RXPGNSUWP   ].DP(gns);
  (*this)[RXPGTN      ].DP(gtn);
  (*this)[RXPGTNUWP   ].DP(gtn);
  (*this)[FLTPLAN     ].DP(fsxP3dBasePath + SEP + "iFly" + SEP + "737NG" + SEP + "navdata" + SEP + "FLTPLAN");
  (*this)[XFMC        ].DP(xpBasePath + SEP + "Resources" + SEP + "plugins" + SEP + "XFMC" + SEP + "FlightPlans");
  (*this)[UFMC        ].DP(documents);
  (*this)[PROSIM      ].DP(documents + SEP + "companyroutes.xml");
  (*this)[BBS         ].DP(fsxP3dBasePath + SEP + "Blackbox Simulation" + SEP + "Company Routes");
  (*this)[VFP         ].DP(documents);
  (*this)[IVAP        ].DP(documents);
  (*this)[XIVAP       ].DP(documents);
  (*this)[FEELTHEREFPL].DP(fsxP3dBasePath);
  (*this)[LEVELDRTE   ].DP(fsxP3dBasePath + SEP + "Level-D Simulations" + SEP + "navdata" + SEP + "Flightplans");
  (*this)[EFBR        ].DP(documents);
  (*this)[QWRTE       ].DP(fsxP3dBasePath);
  (*this)[MDR         ].DP(fsxP3dBasePath);
  (*this)[TFDI        ].DP(fsxP3dBasePath + SEP + "SimObjects" + SEP + "Airplanes" + SEP + "TFDi_Design_717" + SEP + "Documents" + SEP + "Company Routes");
  /* *INDENT-ON* */
#undef DP

  for(RouteExportFormatType type: keys())
  {
    RouteExportFormat& format = (*this)[type];
    format.setDefaultPath(QDir::toNativeSeparators(value(type).getDefaultPath()));
    if(format.getPath().isEmpty())
      format.setPath(format.getDefaultPath());
  }
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
  if(QFile::exists(path))
  {
    if(isExportToFile())
    {
      // Export target is a file ===========
      if(!QFileInfo(path).isFile())
        pathError = tr("Expected file but given path is a directory");
    }
    else
    {
      // Export target is a folder ===========
      if(!QFileInfo(path).isDir())
        pathError = tr("Expected directory but given path is a file");
    }
  }
  else
  {
    if(isExportToFile())
      pathError = tr("File does not exist");
    else
      pathError = tr("Directory does not exist");
  }
}

void RouteExportFormat::setPath(const QString& value)
{
  path = value;
  updatePathError();
}

QString RouteExportFormat::getFilter() const
{
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  if(isExportToFile())
    return "(" + format + ")";
  else
    return "(*." + format + ")";

#else

  // Create an upper and capped string for Linux
  QStringList formats;
  formats << format << atools::capWord(format) << format.toUpper();

  if(isExportToFile())
    return "(" + formats.join(" ") + ")";
  else
    return "(*." + formats.join(" ") + ")";

#endif
}

void RouteExportFormat::copyLoadedData(RouteExportFormat& other) const
{
  other.path = QDir::toNativeSeparators(path);
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
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const RouteExportFormat& obj)
{
  dataStream << static_cast<quint16>(obj.type)
             << static_cast<quint16>(obj.flags & rexp::SAVED_FLAGS)
             << obj.path;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, RouteExportFormatMap& obj)
{
  quint32 magicNumber;
  quint16 version;
  dataStream >> magicNumber >> version;

  if(magicNumber != RouteExportFormatMap::FILE_MAGIC_NUMBER)
  {
    if(RouteExportFormatMap::exceptionOnReadError)
      throw atools::Exception(QObject::tr("Error reading multiexport configuration: Invalid magic number. "
                                          "Not a multiexport configuration."));
    else
      qWarning() << Q_FUNC_INFO << "Invalid magic number" << magicNumber
                 << "expected" << RouteExportFormatMap::FILE_MAGIC_NUMBER;
  }

  if(version != RouteExportFormatMap::FILE_VERSION)
  {
    if(RouteExportFormatMap::exceptionOnReadError)
      throw atools::Exception(QObject::tr("Error reading multiexport configuration: Invalid version. "
                                          "Incompatible multiexport configuration."));
    else
      qWarning() << Q_FUNC_INFO << "Invalid version number" << version
                 << "expected" << RouteExportFormatMap::FILE_VERSION;
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
  dataStream << RouteExportFormatMap::FILE_MAGIC_NUMBER << RouteExportFormatMap::FILE_VERSION;

  dataStream << static_cast<quint16>(obj.size());
  for(const RouteExportFormat& fmt : obj)
    dataStream << fmt;
  return dataStream;
}
