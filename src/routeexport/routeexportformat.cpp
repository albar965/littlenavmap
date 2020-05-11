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

#include <QDir>

using atools::settings::Settings;

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

void RouteExportFormatMap::updatePath(rexp::RouteExportFormatType type, const QString& path)
{
  (*this)[type].path = QDir::toNativeSeparators(path);
}

void RouteExportFormatMap::saveState()
{
  atools::settings::Settings& settings = Settings::instance();
  settings.setValueVar(lnm::ROUTE_EXPORT_FORMATS, QVariant::fromValue<RouteExportFormatMap>(*this));
}

void RouteExportFormatMap::restoreState()
{
  clear();

  // Initialize with defaults
  init();

  // Update simulator dependent default paths
  updateDefaultPaths();

  // Load selection status and user updated paths from settings
  atools::settings::Settings& settings = Settings::instance();
  RouteExportFormatMap formatList = settings.valueVar(lnm::ROUTE_EXPORT_FORMATS).value<RouteExportFormatMap>();
  for(const RouteExportFormat& fmt : formatList)
  {
    if(contains(fmt.getType()))
      fmt.copyLoadedData((*this)[fmt.getType()]);
    else
      qWarning() << Q_FUNC_INFO << "Type not found in internal list" << fmt.getType();
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

  // Assign callbacks from route export instance
  /* *INDENT-OFF* */
  (*this)[PLN         ].func = bind(&RouteExport::routeExportPln,               routeExport, _1);
  (*this)[PLNANNOTATED].func = bind(&RouteExport::routeExportPlnAnnotatedMulti, routeExport, _1);
  (*this)[FMS3        ].func = bind(&RouteExport::routeExportFms3Multi,         routeExport, _1);
  (*this)[FMS11       ].func = bind(&RouteExport::routeExportFms11,             routeExport, _1);
  (*this)[FLP         ].func = bind(&RouteExport::routeExportFlpMulti,          routeExport, _1);
  (*this)[FLIGHTGEAR  ].func = bind(&RouteExport::routeExportFlightgear,        routeExport, _1);
  (*this)[GFP         ].func = bind(&RouteExport::routeExportGfpMulti,          routeExport, _1);
  (*this)[TXT         ].func = bind(&RouteExport::routeExportTxtMulti,          routeExport, _1);
  (*this)[RTE         ].func = bind(&RouteExport::routeExportRteMulti,          routeExport, _1);
  (*this)[GPX         ].func = bind(&RouteExport::routeExportGpx,               routeExport, _1);
  (*this)[HTML        ].func = bind(&RouteExport::routeExportHtml,              routeExport, _1);
  (*this)[FPR         ].func = bind(&RouteExport::routeExportFprMulti,          routeExport, _1);
  (*this)[FPL         ].func = bind(&RouteExport::routeExportFplMulti,          routeExport, _1);
  (*this)[CORTEIN     ].func = bind(&RouteExport::routeExportCorteInMulti,      routeExport, _1);
  (*this)[RXPGNS      ].func = bind(&RouteExport::routeExportRxpGnsMulti,       routeExport, _1);
  (*this)[RXPGTN      ].func = bind(&RouteExport::routeExportRxpGtnMulti,       routeExport, _1);
  (*this)[FLTPLAN     ].func = bind(&RouteExport::routeExportFltplanMulti,      routeExport, _1);
  (*this)[XFMC        ].func = bind(&RouteExport::routeExportXFmcMulti,         routeExport, _1);
  (*this)[UFMC        ].func = bind(&RouteExport::routeExportUFmcMulti,         routeExport, _1);
  (*this)[PROSIM      ].func = bind(&RouteExport::routeExportProSimMulti,       routeExport, _1);
  (*this)[BBS         ].func = bind(&RouteExport::routeExportBbsMulti,          routeExport, _1);
  (*this)[VFP         ].func = bind(&RouteExport::routeExportVfp,               routeExport, _1);
  (*this)[IVAP        ].func = bind(&RouteExport::routeExportIvap,              routeExport, _1);
  (*this)[XIVAP       ].func = bind(&RouteExport::routeExportXIvap,             routeExport, _1);
  (*this)[FEELTHEREFPL].func = bind(&RouteExport::routeExportFeelthereFplMulti, routeExport, _1);
  (*this)[LEVELDRTE   ].func = bind(&RouteExport::routeExportLeveldRteMulti,    routeExport, _1);
  (*this)[EFBR        ].func = bind(&RouteExport::routeExportEfbrMulti,         routeExport, _1);
  (*this)[QWRTE       ].func = bind(&RouteExport::routeExportQwRteMulti,        routeExport, _1);
  (*this)[MDR         ].func = bind(&RouteExport::routeExportMdrMulti,          routeExport, _1);
  (*this)[TFDI        ].func = bind(&RouteExport::routeExportTfdiMulti,         routeExport, _1);
  /* *INDENT-ON* */
}

void RouteExportFormatMap::init()
{
  using namespace rexp;
  using rexp::FILE;
  using rexp::NONE;
  /* *INDENT-OFF* */
  //     type          Object           (type          flags format                   category         comment
  insert(PLN,          RouteExportFormat(PLN,          NONE, tr("pln"),               tr("Simulator"), tr("FSX and Prepar3D")                                            ));
  insert(PLNANNOTATED, RouteExportFormat(PLNANNOTATED, NONE, tr("pln"),               tr("Simulator"), tr("FSX and Prepar3D (annotated for old Little Navmap versions)") ));
  insert(FMS3,         RouteExportFormat(FMS3,         NONE, tr("fms"),               tr("Simulator"), tr("X-Plane FMS 3 (old limited format)")                          ));
  insert(FMS11,        RouteExportFormat(FMS11,        NONE, tr("fms"),               tr("Simulator"), tr("X-Plane FMS 11")                                              ));
  insert(FLP,          RouteExportFormat(FLP,          NONE, tr("flp"),               tr("Aircraft"),  tr("Aerosoft airbus and others")                                  ));
  insert(FLIGHTGEAR,   RouteExportFormat(FLIGHTGEAR,   NONE, tr("fgfp"),              tr("Simulator"), tr("FlightGear")                                                  ));
  insert(GFP,          RouteExportFormat(GFP,          NONE, tr("gfp"),               tr("Garmin"),    tr("Flight1 Garmin GTN 650/750")                                  ));
  insert(TXT,          RouteExportFormat(TXT,          NONE, tr("txt"),               tr("Aircraft"),  tr("Rotate MD-80, JARDesign and others")                          ));
  insert(RTE,          RouteExportFormat(RTE,          NONE, tr("rte"),               tr("Aircraft"),  tr("PMDG aircraft")                                               ));
  insert(GPX,          RouteExportFormat(GPX,          NONE, tr("gpx"),               tr("Other"),     tr("Garmin exchange format for Google Earth and others")          ));
  insert(HTML,         RouteExportFormat(HTML,         NONE, tr("html"),              tr("Other"),     tr("HTML flight plan web page")                                   ));
  insert(FPR,          RouteExportFormat(FPR,          NONE, tr("fpr"),               tr("Aircraft"),  tr("Majestic Dash MJC8 Q400")                                     ));
  insert(FPL,          RouteExportFormat(FPL,          NONE, tr("fpl"),               tr("Aircraft"),  tr("IXEG Boeing 737")                                             ));
  insert(CORTEIN,      RouteExportFormat(CORTEIN,      FILE, tr("corte.in"),          tr("Aircraft"),  tr("Flight Factor Airbus")                                        ));
  insert(RXPGNS,       RouteExportFormat(RXPGNS,       NONE, tr("fpl"),               tr("Garmin"),    tr("Reality XP GNS 530W/430W V2")                                 ));
  insert(RXPGTN,       RouteExportFormat(RXPGTN,       NONE, tr("gfp"),               tr("Garmin"),    tr("Reality XP GTN 750/650 Touch")                                ));
  insert(FLTPLAN,      RouteExportFormat(FLTPLAN,      NONE, tr("fltplan"),           tr("Aircraft"),  tr("iFly")                                                        ));
  insert(XFMC,         RouteExportFormat(XFMC,         NONE, tr("fpl"),               tr("FMC"),       tr("X-FMC")                                                       ));
  insert(UFMC,         RouteExportFormat(UFMC,         NONE, tr("ufmc"),              tr("FMC"),       tr("UFMC")                                                        ));
  insert(PROSIM,       RouteExportFormat(PROSIM,       FILE, tr("companyroutes.xml"), tr("Simulator"), tr("ProSim")                                                      ));
  insert(BBS,          RouteExportFormat(BBS,          NONE, tr("pln"),               tr("Aircraft"),  tr("BlackBox Simulations Airbus")                                 ));
  insert(VFP,          RouteExportFormat(VFP,          NONE, tr("vfp"),               tr("Online"),    tr("VATSIM vPilot or SWIFT")                                      ));
  insert(IVAP,         RouteExportFormat(IVAP,         NONE, tr("fpl"),               tr("Online"),    tr("IvAp for IVAO")                                               ));
  insert(XIVAP,        RouteExportFormat(XIVAP,        NONE, tr("fpl"),               tr("Online"),    tr("X-IVAP for IVAO")                                             ));
  insert(FEELTHEREFPL, RouteExportFormat(FEELTHEREFPL, NONE, tr("fpl"),               tr("Aircraft"),  tr("FeelThere or Wilco")                                          ));
  insert(LEVELDRTE,    RouteExportFormat(LEVELDRTE,    NONE, tr("rte"),               tr("Aircraft"),  tr("Level-D")                                                     ));
  insert(EFBR,         RouteExportFormat(EFBR,         NONE, tr("efbr"),              tr("Other"),     tr("AivlaSoft EFB")                                               ));
  insert(QWRTE,        RouteExportFormat(QWRTE,        NONE, tr("rte"),               tr("Aircraft"),  tr("QualityWings")                                                ));
  insert(MDR,          RouteExportFormat(MDR,          NONE, tr("mdr"),               tr("Aircraft"),  tr("Leonardo Maddog X")                                           ));
  insert(TFDI,         RouteExportFormat(TFDI,         NONE, tr("xml"),               tr("Aircraft"),  tr("TFDi Design 717")                                             ));
  /* *INDENT-ON* */
}

void RouteExportFormatMap::updateDefaultPaths()
{
  QChar SEP = QDir::separator();

  QString base = NavApp::getCurrentSimulatorBasePath();

  // Get X-Plane base path ===========================
  // routeExportFms11 and routeExportFms3
  QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
  if(xpBasePath.isEmpty())
    xpBasePath = atools::documentsDir();
  else
    xpBasePath = atools::buildPathNoCase({xpBasePath, "Output", "FMS plans"});

  // Documents path ===========================
  QString documents = atools::documentsDir();

  // Simulator files for P3D/FSX ===========================
  QString curSimFiles = NavApp::getCurrentSimulatorFilesPath();

  // GNS path ===========================
  QString gns;
#ifdef Q_OS_WIN32
  QString gnsPath(qgetenv("GNSAPPDATA"));
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
  QString gtnPath(qgetenv("GTNSIMDATA"));
  gtn = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\Databases\\FPLN" : gtnPath + "\\Databases\\FPLN";
#elif DEBUG_INFORMATION
  gtn = atools::buildPath({documents, "Garmin", "Trainers", "GTN", "FPLN"});
#else
  gtn = documents;
#endif

  using namespace rexp;

  // Fill default paths
  /* *INDENT-OFF* */
  (*this)[PLN         ].defaultPath = curSimFiles;
  (*this)[PLNANNOTATED].defaultPath = curSimFiles;
  (*this)[FMS3        ].defaultPath = xpBasePath;
  (*this)[FMS11       ].defaultPath = xpBasePath;
  (*this)[FLP         ].defaultPath = documents;
  (*this)[FLIGHTGEAR  ].defaultPath = documents;
  (*this)[GFP         ].defaultPath = base + SEP + "F1GTN" + SEP + "FPL";
  (*this)[TXT         ].defaultPath = base + SEP + "Aircraft";
  (*this)[RTE         ].defaultPath = base + SEP + "PMDG" + SEP + "FLIGHTPLANS";
  (*this)[GPX         ].defaultPath = documents;
  (*this)[HTML        ].defaultPath = documents;
  (*this)[FPR         ].defaultPath = base + SEP + "SimObjects" + SEP + "Airplanes" + SEP + "mjc8q400" + SEP + "nav" + SEP + "routes";
  (*this)[FPL         ].defaultPath = base + SEP + "Aircraft" + SEP + "X-Aviation" + SEP + "IXEG 737 Classic" + SEP + "coroutes";
  (*this)[CORTEIN     ].defaultPath = base + SEP + "Aircraft" + SEP + "corte.in";
  (*this)[RXPGNS      ].defaultPath = gns;
  (*this)[RXPGTN      ].defaultPath = gtn;
  (*this)[FLTPLAN     ].defaultPath = base + SEP + "iFly" + SEP + "737NG" + SEP + "navdata" + SEP + "FLTPLAN";
  (*this)[XFMC        ].defaultPath = base + SEP + "Resources" + SEP + "plugins" + SEP + "XFMC" + SEP + "FlightPlans";
  (*this)[UFMC        ].defaultPath = base;
  (*this)[PROSIM      ].defaultPath = documents + SEP + "companyroutes.xml";
  (*this)[BBS         ].defaultPath = base + SEP + "Blackbox Simulation" + SEP + "Company Routes";
  (*this)[VFP         ].defaultPath = documents;
  (*this)[IVAP        ].defaultPath = documents;
  (*this)[XIVAP       ].defaultPath = documents;
  (*this)[FEELTHEREFPL].defaultPath = base;
  (*this)[LEVELDRTE   ].defaultPath = base + SEP + "Level-D Simulations" + SEP + "navdata" + SEP + "Flightplans";
  (*this)[EFBR        ].defaultPath = documents;
  (*this)[QWRTE       ].defaultPath = base;
  (*this)[MDR         ].defaultPath = base;
  (*this)[TFDI        ].defaultPath = base + SEP + "SimObjects" + SEP + "Airplanes" + SEP + "TFDi_Design_717" + SEP + "Documents" + SEP + "Company Routes";
  /* *INDENT-ON* */

  for(RouteExportFormatType type: keys())
    (*this)[type].defaultPath = QDir::toNativeSeparators(value(type).defaultPath);
}

bool RouteExportFormat::isPathValid() const
{
  if(QFile::exists(getPathOrDefault()))
  {
    if(isFile())
      return QFileInfo(getPathOrDefault()).isFile();
    else
      return QFileInfo(getPathOrDefault()).isDir();
  }
  return false;
}

QString RouteExportFormat::getFilter() const
{
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  if(isFile())
    return "(" + format + ")";
  else
    return "(*." + format + ")";

#else

  // Create an upper and capped string for Linux
  QStringList formats;
  formats << format << atools::capWord(format) << format.toUpper();

  if(isFile())
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
  quint8 typeInt;
  dataStream >> typeInt;
  obj.type = static_cast<rexp::RouteExportFormatType>(typeInt);

  quint8 selectedInt;
  dataStream >> selectedInt;
  obj.flags.setFlag(rexp::SELECTED, selectedInt);

  dataStream >> obj.path;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const RouteExportFormat& obj)
{
  dataStream << static_cast<quint8>(obj.type) << static_cast<quint8>(obj.flags.testFlag(rexp::SELECTED)) << obj.path;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, RouteExportFormatMap& obj)
{
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
  dataStream << static_cast<quint16>(obj.size());
  for(const RouteExportFormat& fmt : obj)
    dataStream << fmt;
  return dataStream;
}
