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

#include "common/mapcolors.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "options/optiondata.h"
#include "settings/settings.h"
#include "app/navapp.h"

#include <QPen>
#include <QString>
#include <QCoreApplication>
#include <QPalette>
#include <QSettings>
#include <QPainter>

namespace mapcolors {

/* Colors that are updated from confguration file */

QPen taxiwayLinePen(QColor(200, 200, 0), 1.5, Qt::DashLine, Qt::FlatCap);
QColor taxiwayNameBackgroundColor(255, 255, 120);
QColor taxiwayNameColor(Qt::black);

QColor airportDetailBackColor(255, 255, 255);
QColor airportEmptyColor(130, 130, 130);
QColor toweredAirportColor(15, 70, 130);
QColor unToweredAirportColor(126, 58, 91);
QColor addonAirportBackgroundColor(Qt::yellow);
QColor addonAirportFrameColor(Qt::darkGray);
QColor vorSymbolColor(Qt::darkBlue);
QColor ndbSymbolColor(Qt::darkRed);
QColor markerSymbolColor(Qt::darkMagenta);

QColor ilsFillColor("#40008000");
QColor ilsTextColor(0, 30, 0);
QPen ilsCenterPen(Qt::darkGreen, 1.5, Qt::DashLine);
QColor ilsSymbolColor(Qt::darkGreen);

QColor glsFillColor("#20808080");
QColor glsTextColor(20, 20, 20);
QPen glsCenterPen(Qt::darkGray, 1.5, Qt::DashLine);
QColor glsSymbolColor(Qt::darkGray);

QColor msaFillColor("#a0ffffff");
QColor msaTextColor(Qt::darkGray);
QColor msaSymbolColor(Qt::darkGray);

QColor textBoxColorAirspace(255, 255, 255, 180);

QPen msaDiagramLinePen(QColor("#000000"), 2.);
QColor msaDiagramNumberColor("#70000000");
QPen msaDiagramLinePenDark(QColor("#808080"), 2.);
QColor msaDiagramNumberColorDark("#70a0a0a0");

QColor msaDiagramFillColor("#10000000");
QColor msaDiagramFillColorDark("#10ffffff");

QColor waypointSymbolColor(200, 0, 200);

QColor airwayVictorColor("#969696"); // 1.
QColor airwayJetColor("#000080"); // 1.
QColor airwayBothColor("#646464"); // 1.
QColor airwayTrackColor("#101010"); // 1.5
static QColor airwayTrackColorEast("#a01010"); // 1.5
static QColor airwayTrackColorWest("#1010a0"); // 1.5
QColor airwayTextColor(80, 80, 80);

QColor holdingColor(50, 50, 50);

QColor rangeRingColor(Qt::red);
QColor rangeRingTextColor(Qt::black);

QColor weatherWindGustColor("#ff8040");
QColor weatherWindColor(Qt::black);
QColor weatherBackgoundColor(Qt::white);

QColor weatherLifrColor(QColor("#d000d0"));
QColor weatherIfrColor(QColor("#d00000"));
QColor weatherMvfrColor(QColor("#0000d0"));
QColor weatherVfrColor(QColor("#00b000"));

QPen minimumAltitudeGridPen(QColor("#a0a0a0"), 1.);
QColor minimumAltitudeNumberColor(QColor("#70000000"));

QPen minimumAltitudeGridPenDark(QColor("#808080"), 1.);
QColor minimumAltitudeNumberColorDark(QColor("#70a0a0a0"));

QColor compassRoseColor(Qt::darkRed);
QColor compassRoseTextColor(Qt::black);

/* Elevation profile colors and pens */
QColor profileSkyColor(QColor(204, 204, 255));
QColor profileLandColor(QColor(0, 128, 0));
QColor profileLabelColor(QColor(0, 0, 0));

QColor profileVasiAboveColor(QColor("#70ffffff"));
QColor profileVasiBelowColor(QColor("#70ff0000"));

QColor profileAltRestrictionFill(QColor(255, 255, 90));
QColor profileAltRestrictionOutline(Qt::black);

QPen profileVasiCenterPen(Qt::darkGray, 1.5, Qt::DashLine);
QPen profileLandOutlinePen(Qt::black, 1., Qt::SolidLine);
QPen profileWaypointLinePen(Qt::gray, 1., Qt::SolidLine, Qt::FlatCap);
QPen profileElevationScalePen(Qt::darkGray, 1., Qt::SolidLine, Qt::FlatCap);
QPen profileSafeAltLinePen(Qt::red, 4., Qt::SolidLine, Qt::FlatCap);
QPen profileSafeAltLegLinePen(QColor(255, 100, 0), 3., Qt::SolidLine, Qt::FlatCap);

/* Objects highlighted because of selection in search */
QColor highlightBackColor(Qt::black);

/* Objects highlighted because of selection in route table */
QColor routeHighlightBackColor(Qt::black);

/* Objects highlighted because of selection in route profile */
QColor profileHighlightBackColor(Qt::black);

QColor distanceMarkerTextBackgroundColor(255, 255, 255, 220);
QColor distanceMarkerTextColor(Qt::black);

QPen markEndurancePen(Qt::black, 2., Qt::DotLine, Qt::FlatCap, Qt::MiterJoin);
QPen markSelectedAltitudeRangePen(Qt::darkGreen, 1.5, Qt::SolidLine, Qt::FlatCap);
QPen markTurnPathPen(Qt::darkGreen, 1.5, Qt::SolidLine, Qt::FlatCap);

/* Map print colors */
QColor mapPrintRowColor(250, 250, 250);
QColor mapPrintRowColorAlt(240, 240, 240);
QColor mapPrintHeaderColor(220, 220, 220);

QPen searchCenterBackPen(QColor(0, 0, 0), 6., Qt::SolidLine, Qt::FlatCap);
QPen searchCenterFillPen(QColor(255, 255, 0), 2., Qt::SolidLine, Qt::FlatCap);
QPen touchMarkBackPen(QColor(0, 0, 0), 4., Qt::SolidLine, Qt::FlatCap);
QPen touchMarkFillPen(QColor(255, 255, 255), 2., Qt::SolidLine, Qt::FlatCap);
QColor touchRegionFillColor("#40888888");

QColor aircraftUserLabelColor(0, 0, 0);
QColor aircraftUserLabelColorBg(255, 255, 150);
QColor aircraftAiLabelColor(0, 0, 0);
QColor aircraftAiLabelColorBg(255, 255, 255);

/* Text along route and approach segments */
QColor routeTextColor(0, 0, 0);
QColor routeTextColorGray(140, 140, 140);
QColor routeTextBackgroundColor(255, 255, 255, 220);
QColor routeProcedureMissedTextColor(90, 90, 90);
QColor routeProcedureTextColor(0, 0, 0);
QColor routeProcedurePointColor(90, 90, 90);
QColor routeProcedurePointFlyoverColor(Qt::black);
QColor routeUserPointColor(Qt::darkYellow);
QColor routeInvalidPointColor(Qt::red);

/* Procedure colors */
QColor routeProcedureMissedTableColorDark(240, 170, 120);
QColor routeProcedureMissedTableColor(Qt::darkRed);

QColor routeProcedureTableColorDark(140, 200, 240);
QColor routeProcedureTableColor(Qt::darkBlue);

/* Alternate airport leg colors */
QColor routeAlternateTableColor(Qt::darkGray);
QColor routeAlternateTableColorDark(Qt::gray);
QColor routeInvalidTableColor(Qt::red);
QColor routeInvalidTableColorDark(Qt::red);

QColor nextWaypointColor(255, 100, 255);
QColor nextWaypointColorDark(150, 20, 150);

// Surface colors for runways, aprons and taxiways
static QColor surfaceConcrete("#888888");
static QColor surfaceGrass("#00a000");
static QColor surfaceWater("#808585ff");
static QColor surfaceAsphalt("#707070");
static QColor surfaceCement("#a0a0a0");
static QColor surfaceClay("#DEB887");
static QColor surfaceSnow("#dbdbdb");
static QColor surfaceIce("#d0d0ff");
static QColor surfaceDirt("#CD853F");
static QColor surfaceCoral("#FFE4C4");
static QColor surfaceGravel("#c0c0c0");
static QColor surfaceOilTreated("#2F4F4F");
static QColor surfaceSteelMats("#a0f0ff");
static QColor surfaceBituminous("#505050");
static QColor surfaceBrick("#A0522D");
static QColor surfaceMacadam("#c8c8c8");
static QColor surfacePlanks("#8B4513");
static QColor surfaceSand("#F4A460");
static QColor surfaceShale("#F5DEB3");
static QColor surfaceTarmac("#909090");
static QColor surfaceUnknown("#ffffff");
static QColor surfaceTransparent("#e0e0e0");

/* Alternating colors */
static QColor rowBgColor;
static QColor rowAltBgColor;

/* Slightly darker background for sort column */
static QColor rowSortBgColor;
static QColor rowSortAltBgColor;

void styleChanged()
{
  rowBgColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  rowAltBgColor = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase);
  rowSortBgColor = rowBgColor.darker(106);
  rowSortAltBgColor = rowAltBgColor.darker(106);
}

void init()
{
  styleChanged();
}

const QColor& colorForAirport(const map::MapAirport& ap)
{
  if(ap.emptyDraw())
    return airportEmptyColor;
  else if(ap.tower())
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

const QColor& alternatingRowColor(int row, bool isSort)
{
  if((row % 2) == 0)
    return isSort ? rowSortBgColor : rowBgColor;
  else
    return isSort ? rowSortAltBgColor : rowAltBgColor;
}

const QColor& colorOutlineForParkingType(const QString& type)
{
  if(type == "RMCB" || type == "RMC" || type.startsWith("G") || type.startsWith("RGA") || type.startsWith("DGA") ||
     type.startsWith("RC") || type.startsWith("FUEL") || type == ("H") || type == ("T") || type == ("RE") ||
     type == ("GE"))
    return parkingOutlineColor;
  else
    return parkingUnknownOutlineColor;
}

const QColor& colorForParkingType(const QString& type)
{
  static const QColor rampMilCargo(190, 0, 0);
  static const QColor rampMilCombat(Qt::red);
  static const QColor rampMil(190, 0, 0);
  static const QColor gate(100, 100, 255);
  static const QColor rampGa(0, 200, 0);
  static const QColor tiedown(0, 150, 0);
  static const QColor hangar(Qt::darkYellow);
  static const QColor rampCargo(Qt::darkGreen);
  static const QColor fuel(Qt::yellow);
  static const QColor unknown("#808080");

  if(type == "RM")
    return rampMil;
  else if(type == "RMC")
    return rampMilCargo;
  else if(type == "RMCB")
    return rampMilCombat;
  else if(type.startsWith("G"))
    return gate;
  else if(type.startsWith("RGA") || type.startsWith("DGA") || type.startsWith("RE"))
    return rampGa;
  else if(type.startsWith("RC"))
    return rampCargo;
  else if(type.startsWith("FUEL"))
    return fuel;
  else if(type == ("H"))
    return hangar;
  else if(type == ("T"))
    return tiedown;
  else
    return unknown;
}

const QColor& colorTextForParkingType(const QString& type)
{
  if(type == "RMCB")
    return mapcolors::brightParkingTextColor;
  else if(type == "RMC")
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith("G"))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith("RGA") || type.startsWith("DGA") || type.startsWith("RE"))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith("RC"))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith("FUEL"))
    return mapcolors::darkParkingTextColor;
  else if(type == ("H"))
    return brightParkingTextColor;
  else if(type == ("T"))
    return brightParkingTextColor;
  else
    return brightParkingTextColor;
}

const QIcon& iconForStart(const map::MapStart& start)
{
  static const QIcon runway(":/littlenavmap/resources/icons/startrunway.svg");
  static const QIcon helipad(":/littlenavmap/resources/icons/starthelipad.svg");
  static const QIcon water(":/littlenavmap/resources/icons/startwater.svg");

  static const QIcon empty;
  if(start.isRunway())
    return runway;
  else if(start.isHelipad())
    return helipad;
  else if(start.isWater())
    return water;
  else
    return empty;
}

const QIcon& iconForParkingType(const QString& type)
{
  static const QIcon cargo(":/littlenavmap/resources/icons/parkingrampcargo.svg");
  static const QIcon ga(":/littlenavmap/resources/icons/parkingrampga.svg");
  static const QIcon mil(":/littlenavmap/resources/icons/parkingrampmil.svg");
  static const QIcon gate(":/littlenavmap/resources/icons/parkinggate.svg");
  static const QIcon fuel(":/littlenavmap/resources/icons/parkingfuel.svg");
  static const QIcon hangar(":/littlenavmap/resources/icons/parkinghangar.svg");
  static const QIcon tiedown(":/littlenavmap/resources/icons/parkingtiedown.svg");
  static const QIcon unknown(":/littlenavmap/resources/icons/parkingunknown.svg");

  if(type.startsWith("RM"))
    return mil;
  else if(type.startsWith("G"))
    return gate;
  else if(type.startsWith("RGA") || type.startsWith("DGA") || type == "RE")
    return ga;
  else if(type.startsWith("RC"))
    return cargo;
  else if(type.startsWith("FUEL"))
    return fuel;
  else if(type == ("H"))
    return hangar;
  else if(type == ("T"))
    return tiedown;
  else
    return unknown;
}

const QColor& colorForSurface(const QString& surface)
{
  if(surface == "A")
    return surfaceAsphalt;
  else if(surface == "G")
    return surfaceGrass;
  else if(surface == "D")
    return surfaceDirt;
  else if(surface == "C")
    return surfaceConcrete;
  else if(surface == "GR")
    return surfaceGravel;
  else if(surface == "W")
    return surfaceWater;
  else if(surface == "CE")
    return surfaceCement;
  else if(surface == "CL")
    return surfaceClay;
  else if(surface == "SN")
    return surfaceSnow;
  else if(surface == "I")
    return surfaceIce;
  else if(surface == "CR")
    return surfaceCoral;
  else if(surface == "OT")
    return surfaceOilTreated;
  else if(surface == "SM")
    return surfaceSteelMats;
  else if(surface == "B")
    return surfaceBituminous;
  else if(surface == "BR")
    return surfaceBrick;
  else if(surface == "M")
    return surfaceMacadam;
  else if(surface == "PL")
    return surfacePlanks;
  else if(surface == "S")
    return surfaceSand;
  else if(surface == "SH")
    return surfaceShale;
  else if(surface == "T")
    return surfaceTarmac;
  else if(surface == "TR")
    return surfaceTransparent;

  // else if(surface == "NONE" || surface == "UNKNOWN" || surface == "INVALID") || TR
  return surfaceUnknown;
}

const QPen aircraftTrailPenOuter(float size)
{
  return QPen(NavApp::isDarkMapTheme() ? Qt::white : Qt::black, size + 3., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

const QPen aircraftTrailPenProfile(float size)
{
  const OptionData& optionData = OptionData::instance();
  QPen pen(optionData.getTrailColor(), size, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);

  // Styled pens ===========================================================
  switch(optionData.getDisplayTrailType())
  {
    case opts::TRAIL_TYPE_DASHED:
      pen.setStyle(Qt::DashLine);
      break;

    case opts::TRAIL_TYPE_DOTTED:
      pen.setStyle(Qt::DotLine);
      break;

    case opts::TRAIL_TYPE_SOLID:
      pen.setStyle(Qt::SolidLine);
      break;
  }
  return pen;
}

const QPen aircraftTrailPen(float size, float minAlt, float maxAlt, float alt)
{
  const OptionData& optionData = OptionData::instance();
  if(optionData.getFlags().testFlag(opts::MAP_TRAIL_GRADIENT))
  {
    bool altValid = atools::almostNotEqual(minAlt, maxAlt, 100.f);

    // Gradient pens ===========================================================
    alt -= minAlt;
    int hue, saturation, value;
    QColor col;
    QPen pen(col, size, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    switch(optionData.getDisplayTrailGradientType())
    {
      case opts::TRAIL_GRADIENT_COLOR_YELLOW_BLUE:
        // Change hue depending on altitude. Yellow is 60 and last used value blue is 240
        col = Qt::yellow;
        col.getHsv(&hue, &saturation, &value);
        hue = atools::minmax(60 /* yellow */, 240 /* blue */, atools::roundToInt(60.f + alt / (maxAlt - minAlt) * 180.f));
        break;

      case opts::TRAIL_GRADIENT_COLOR_RAINBOW:
        // Change hue depending on altitude. Red is 0 and last used value magenta is 300
        col = Qt::red;
        col.getHsv(&hue, &saturation, &value);
        hue = atools::minmax(0 /* red */, 300 /* magenta */, atools::roundToInt(alt / (maxAlt - minAlt) * 300.f));
        break;

      case opts::TRAIL_GRADIENT_BLACKWHITE:
        // Change value depending on altitude and start with white = value 255
        col = Qt::white;
        col.getHsv(&hue, &saturation, &value);
        value = atools::minmax(0 /* black */, 255 /* white */, 255 - atools::roundToInt(alt / (maxAlt - minAlt) * 255.f));
        break;
    }

    if(altValid)
      col.setHsv(hue, saturation, value);

    pen.setColor(col);
    return pen;
  }
  else
    // Styled pens ===========================================================
    return aircraftTrailPenProfile(size);
}

/* Default colors. Saved to little_navmap_mapstyle.ini and can be overridden there */
static QHash<map::MapAirspaceTypes, QColor> airspaceFillColors(
  {
    {map::AIRSPACE_NONE, QColor("#00000000")},
    {map::CENTER, QColor("#30808080")},
    {map::CLASS_A, QColor("#308d0200")},
    {map::CLASS_B, QColor("#30902ece")},
    {map::CLASS_C, QColor("#308594ec")},
    {map::CLASS_D, QColor("#306c5bce")},
    {map::CLASS_E, QColor("#30cc5060")},
    {map::CLASS_F, QColor("#307d8000")},
    {map::CLASS_G, QColor("#30cc8040")},
    {map::FIR, QColor("#30606080")},
    {map::UIR, QColor("#30404080")},
    {map::TOWER, QColor("#300000f0")},
    {map::CLEARANCE, QColor("#3060808a")},
    {map::GROUND, QColor("#30000000")},
    {map::DEPARTURE, QColor("#3060808a")},
    {map::APPROACH, QColor("#3060808a")},
    {map::MOA, QColor("#304485b7")},
    {map::RESTRICTED, QColor("#30fd8c00")},
    {map::PROHIBITED, QColor("#30f00909")},
    {map::WARNING, QColor("#30fd8c00")},
    {map::CAUTION, QColor("#50fd8c00")},
    {map::ALERT, QColor("#30fd8c00")},
    {map::DANGER, QColor("#30dd103d")},
    {map::NATIONAL_PARK, QColor("#30509090")},
    {map::MODEC, QColor("#30509090")},
    {map::RADAR, QColor("#30509090")},
    {map::GCA, QColor("#30902ece")},
    {map::MCTR, QColor("#30f00b0b")},
    {map::TRSA, QColor("#30902ece")},
    {map::TRAINING, QColor("#30509090")},
    {map::GLIDERPROHIBITED, QColor("#30fd8c00")},
    {map::WAVEWINDOW, QColor("#304485b7")},
    {map::ONLINE_OBSERVER, QColor("#3000a000")}
  }
  );

/* Default colors. Saved to little_navmap_mapstyle.ini and can be overridden there */
static QHash<map::MapAirspaceTypes, QPen> airspacePens(
  {
    {map::AIRSPACE_NONE, QPen(QColor("#00000000"))},
    {map::CENTER, QPen(QColor("#808080"), 1.5)},
    {map::CLASS_A, QPen(QColor("#8d0200"), 2)},
    {map::CLASS_B, QPen(QColor("#902ece"), 2)},
    {map::CLASS_C, QPen(QColor("#8594ec"), 2)},
    {map::CLASS_D, QPen(QColor("#6c5bce"), 2)},
    {map::CLASS_E, QPen(QColor("#cc5060"), 2)},
    {map::CLASS_F, QPen(QColor("#7d8000"), 2)},
    {map::CLASS_G, QPen(QColor("#cc8040"), 2)},
    {map::FIR, QPen(QColor("#606080"), 1.5)},
    {map::UIR, QPen(QColor("#404080"), 1.5)},
    {map::TOWER, QPen(QColor("#6000a0"), 2)},
    {map::CLEARANCE, QPen(QColor("#60808a"), 2)},
    {map::GROUND, QPen(QColor("#000000"), 2)},
    {map::DEPARTURE, QPen(QColor("#60808a"), 2)},
    {map::APPROACH, QPen(QColor("#60808a"), 2)},
    {map::MOA, QPen(QColor("#4485b7"), 2)},
    {map::RESTRICTED, QPen(QColor("#fd8c00"), 2)},
    {map::PROHIBITED, QPen(QColor("#f00909"), 3)},
    {map::WARNING, QPen(QColor("#fd8c00"), 2)},
    {map::CAUTION, QPen(QColor("#ff6c00"), 2)},
    {map::ALERT, QPen(QColor("#fd8c00"), 2)},
    {map::DANGER, QPen(QColor("#dd103d"), 2)},
    {map::NATIONAL_PARK, QPen(QColor("#509090"), 2)},
    {map::MODEC, QPen(QColor("#509090"), 2)},
    {map::RADAR, QPen(QColor("#509090"), 2)},
    {map::GCA, QPen(QColor("#902ece"), 2)},
    {map::MCTR, QPen(QColor("#f00b0b"), 2)},
    {map::TRSA, QPen(QColor("#902ece"), 2)},
    {map::TRAINING, QPen(QColor("#509090"), 2)},
    {map::GLIDERPROHIBITED, QPen(QColor("#fd8c00"), 2)},
    {map::WAVEWINDOW, QPen(QColor("#4485b7"), 2)},
    {map::ONLINE_OBSERVER, QPen(QColor("#a000a000"), 1.5)}
  }
  );

/* Maps airspace types to color configuration options */
static QHash<QString, map::MapAirspaceTypes> airspaceConfigNames(
  {
    {"Center", map::CENTER},
    {"ClassA", map::CLASS_A},
    {"ClassB", map::CLASS_B},
    {"ClassC", map::CLASS_C},
    {"ClassD", map::CLASS_D},
    {"ClassE", map::CLASS_E},
    {"ClassF", map::CLASS_F},
    {"ClassG", map::CLASS_G},
    {"FIR", map::FIR},
    {"UIR", map::UIR},
    {"Tower", map::TOWER},
    {"Clearance", map::CLEARANCE},
    {"Ground", map::GROUND},
    {"Departure", map::DEPARTURE},
    {"Approach", map::APPROACH},
    {"Moa", map::MOA},
    {"Restricted", map::RESTRICTED},
    {"Prohibited", map::PROHIBITED},
    {"Warning", map::WARNING},
    {"Caution", map::CAUTION},
    {"Alert", map::ALERT},
    {"Danger", map::DANGER},
    {"NationalPark", map::NATIONAL_PARK},
    {"Modec", map::MODEC},
    {"Radar", map::RADAR},
    {"Gca", map::GCA},
    {"Mctr", map::MCTR},
    {"Trsa", map::TRSA},
    {"Training", map::TRAINING},
    {"GliderProhibited", map::GLIDERPROHIBITED},
    {"WaveWindow", map::WAVEWINDOW},
    {"Observer", map::ONLINE_OBSERVER}
  }
  );

QColor colorForAirspaceFill(const map::MapAirspace& airspace, int transparency)
{
  // Lower values make the airspace more opaque and higher values more transparent. Default is 80.
  QColor color = airspaceFillColors[airspace.type];

  // 0 = transparent, 1 = opaque
  color.setAlphaF(atools::minmax(0., 1., 1.5 * color.alphaF() * (1. - transparency / 100.)));
  return color;
}

QPen penForAirspace(const map::MapAirspace& airspace, int lineThickness)
{
  // lineThickness = 20 to 300 and default 100 equal to a scale factor of 0.2 to 3.0
  QPen pen = airspacePens[airspace.type];
  pen.setWidthF(pen.widthF() * lineThickness / 100.);
  return pen;
}

const QColor& colorForAirwayOrTrack(const map::MapAirway& airway)
{
  static QColor EMPTY_COLOR;

  switch(airway.type)
  {
    case map::NO_AIRWAY:
      break;

    case map::TRACK_NAT:
    case map::TRACK_PACOTS:
      if(airway.westCourse)
        return airwayTrackColorWest;
      else if(airway.eastCourse)
        return airwayTrackColorEast;
      else
        return airwayTrackColor;

    case map::TRACK_AUSOTS:
      return airwayTrackColor;

    case map::AIRWAY_VICTOR:
      return airwayVictorColor;

    case map::AIRWAY_JET:
      return airwayJetColor;

    case map::AIRWAY_BOTH:
      return airwayBothColor;
  }
  return EMPTY_COLOR;
}

/* Read ARGB color if value exists in settings or update in settings with given value */
void syncColorArgb(QSettings& settings, const QString& key, QColor& color)
{
  if(settings.contains(key))
    color.setNamedColor(settings.value(key).toString());
  else
    settings.setValue(key, color.name(QColor::HexArgb));
}

/* Read color if value exists in settings or update in settings with given value */
void syncColor(QSettings& settings, const QString& key, QColor& color)
{
  if(settings.contains(key))
    color.setNamedColor(settings.value(key).toString());
  else
    settings.setValue(key, color.name());
}

/* Read color and pen width if value exists in settings or update in settings with values of given pen */
void syncPen(QSettings& settings, const QString& key, QPen& pen)
{
  static QHash<QString, Qt::PenStyle> penToStyle(
    {
      {"Solid", Qt::SolidLine},
      {"Dash", Qt::DashLine},
      {"Dot", Qt::DotLine},
      {"DashDot", Qt::DashDotLine},
      {"DashDotDot", Qt::DashDotDotLine},
    });
  static QHash<Qt::PenStyle, QString> styleToPen(
    {
      {Qt::SolidLine, "Solid"},
      {Qt::DashLine, "Dash"},
      {Qt::DotLine, "Dot"},
      {Qt::DashDotLine, "DashDot"},
      {Qt::DashDotDotLine, "DashDotDot"},
    });

  if(settings.contains(key))
  {
    QStringList list = settings.value(key).toStringList();
    if(list.size() >= 1)
    {
      pen.setColor(QColor(list.at(0)));

      if(list.size() >= 2)
        pen.setWidthF(list.at(1).toFloat());

      if(list.size() >= 3)
        pen.setStyle(penToStyle.value(list.at(2), Qt::SolidLine));
    }
  }
  else
    settings.setValue(key, QStringList({pen.color().name(),
                                        QString::number(pen.widthF()),
                                        styleToPen.value(pen.style(), "Solid")}));
}

void syncColors()
{
#ifndef DEBUG_DISABLE_SYNC_COLORS
  QString filename = atools::settings::Settings::getConfigFilename(lnm::MAPSTYLE_INI_SUFFIX);

  QSettings colorSettings(filename, QSettings::IniFormat);
  colorSettings.setValue("Options/Version", QApplication::applicationVersion());

  colorSettings.beginGroup("FlightPlan");
  syncColor(colorSettings, "RouteProcedureMissedTableColorDark", routeProcedureMissedTableColorDark);
  syncColor(colorSettings, "RouteProcedureMissedTableColor", routeProcedureMissedTableColor);
  syncColor(colorSettings, "RouteProcedureTableColorDark", routeProcedureTableColorDark);
  syncColor(colorSettings, "RouteProcedureTableColor", routeProcedureTableColor);
  syncColor(colorSettings, "RouteAlternateTableColor", routeAlternateTableColor);
  syncColor(colorSettings, "RouteAlternateTableColorDark", routeAlternateTableColorDark);
  syncColor(colorSettings, "RouteInvalidTableColor", routeInvalidTableColor);
  syncColor(colorSettings, "RouteInvalidTableColorDark", routeInvalidTableColorDark);
  syncColor(colorSettings, "NextWaypointColor", nextWaypointColor);
  syncColor(colorSettings, "NextWaypointColorDark", nextWaypointColorDark);
  colorSettings.endGroup();

  colorSettings.beginGroup("Aircraft");
  syncColorArgb(colorSettings, "UserLabelColor", aircraftUserLabelColor);
  syncColorArgb(colorSettings, "UserLabelBackgroundColor", aircraftUserLabelColorBg);
  syncColorArgb(colorSettings, "AiLabelColor", aircraftAiLabelColor);
  syncColorArgb(colorSettings, "AiLabelBackgroundColor", aircraftAiLabelColorBg);
  colorSettings.endGroup();

  colorSettings.beginGroup("Airport");
  syncColor(colorSettings, "DiagramBackgroundColor", airportDetailBackColor);
  syncColor(colorSettings, "EmptyColor", airportEmptyColor);
  syncColor(colorSettings, "ToweredColor", toweredAirportColor);
  syncColor(colorSettings, "UnToweredColor", unToweredAirportColor);
  syncColor(colorSettings, "AddonBackgroundColor", addonAirportBackgroundColor);
  syncColor(colorSettings, "AddonFrameColor", addonAirportFrameColor);
  syncPen(colorSettings, "TaxiwayLinePen", taxiwayLinePen);
  syncColor(colorSettings, "TaxiwayNameColor", taxiwayNameColor);
  syncColor(colorSettings, "TaxiwayNameBackgroundColor", taxiwayNameBackgroundColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Navaid");
  syncColor(colorSettings, "VorColor", vorSymbolColor);
  syncColor(colorSettings, "NdbColor", ndbSymbolColor);
  syncColor(colorSettings, "MarkerColor", markerSymbolColor);
  syncColor(colorSettings, "IlsColor", ilsSymbolColor);
  syncColorArgb(colorSettings, "IlsFillColor", ilsFillColor);
  syncColor(colorSettings, "IlsTextColor", ilsTextColor);
  syncPen(colorSettings, "IlsCenterPen", ilsCenterPen);

  syncColor(colorSettings, "GlsGbasColor", glsSymbolColor);
  syncColorArgb(colorSettings, "GlsGbasFillColor", glsFillColor);
  syncColor(colorSettings, "GlsTextColor", glsTextColor);
  syncPen(colorSettings, "GlsCenterPen", glsCenterPen);

  syncColor(colorSettings, "MsaTextColor", msaTextColor);
  syncColorArgb(colorSettings, "MsaFillColor", msaFillColor);
  syncColor(colorSettings, "MsaSymbolColor", msaSymbolColor);

  syncPen(colorSettings, "MsaDiagramLinePen", msaDiagramLinePen);
  syncColorArgb(colorSettings, "MsaDiagramNumberColor", msaDiagramNumberColor);
  syncPen(colorSettings, "MsaDiagramLinePenDark", msaDiagramLinePenDark);
  syncColorArgb(colorSettings, "MsaDiagramNumberColorDark", msaDiagramNumberColorDark);
  syncColorArgb(colorSettings, "MsaDiagramFillColor", msaDiagramFillColor);
  syncColorArgb(colorSettings, "MsaDiagramFillColorDark", msaDiagramFillColorDark);

  syncColor(colorSettings, "WaypointColor", waypointSymbolColor);
  syncColor(colorSettings, "HoldingColor", holdingColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Airway");
  syncColor(colorSettings, "VictorColor", airwayVictorColor);
  syncColor(colorSettings, "JetColor", airwayJetColor);
  syncColor(colorSettings, "BothColor", airwayBothColor);
  syncColor(colorSettings, "TrackColor", airwayTrackColor);
  syncColor(colorSettings, "TrackColorEast", airwayTrackColorEast);
  syncColor(colorSettings, "TrackColorWest", airwayTrackColorWest);
  syncColor(colorSettings, "TextColor", airwayTextColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Marker");
  syncColor(colorSettings, "RangeRingColor", rangeRingColor);
  syncColor(colorSettings, "RangeRingTextColor", rangeRingTextColor);
  syncColor(colorSettings, "CompassRoseColor", compassRoseColor);
  syncColor(colorSettings, "CompassRoseTextColor", compassRoseTextColor);
  syncPen(colorSettings, "SearchCenterBackPen", searchCenterBackPen);
  syncPen(colorSettings, "SearchCenterFillPen", searchCenterFillPen);
  syncPen(colorSettings, "TouchMarkBackPen", touchMarkBackPen);
  syncPen(colorSettings, "TouchMarkFillPen", touchMarkFillPen);
  syncPen(colorSettings, "EndurancePen", markEndurancePen);
  syncPen(colorSettings, "SelectedAltitudeRangePen", markSelectedAltitudeRangePen);
  syncPen(colorSettings, "TurnPathPen", markTurnPathPen);
  syncColorArgb(colorSettings, "TouchRegionFillColor", touchRegionFillColor);
  syncColor(colorSettings, "DistanceMarkerTextColor", distanceMarkerTextColor);
  syncColorArgb(colorSettings, "DistanceMarkerTextBackgroundColor", distanceMarkerTextBackgroundColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Highlight");
  syncColor(colorSettings, "HighlightBackColor", highlightBackColor);
  syncColor(colorSettings, "RouteHighlightBackColor", routeHighlightBackColor);
  syncColor(colorSettings, "ProfileHighlightBackColor", profileHighlightBackColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Print");
  syncColor(colorSettings, "MapPrintRowColor", mapPrintRowColor);
  syncColor(colorSettings, "MapPrintRowColorAlt", mapPrintRowColorAlt);
  syncColor(colorSettings, "MapPrintHeaderColor", mapPrintHeaderColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Weather");
  syncColor(colorSettings, "WeatherBackgoundColor", weatherBackgoundColor);
  syncColor(colorSettings, "WeatherWindColor", weatherWindColor);
  syncColor(colorSettings, "WeatherWindGustColor", weatherWindGustColor);
  syncColor(colorSettings, "WeatherLifrColor", weatherLifrColor);
  syncColor(colorSettings, "WeatherIfrColor", weatherIfrColor);
  syncColor(colorSettings, "WeatherMvfrColor", weatherMvfrColor);
  syncColor(colorSettings, "WeatherVfrColor", weatherVfrColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("AltitudeGrid");
  syncPen(colorSettings, "MinimumAltitudeGridPen", minimumAltitudeGridPen);
  syncColorArgb(colorSettings, "MinimumAltitudeNumberColor", minimumAltitudeNumberColor);
  syncPen(colorSettings, "MinimumAltitudeGridPenDark", minimumAltitudeGridPenDark);
  syncColorArgb(colorSettings, "MinimumAltitudeNumberColorDark", minimumAltitudeNumberColorDark);
  colorSettings.endGroup();

  colorSettings.beginGroup("Profile");
  syncColor(colorSettings, "SkyColor", profileSkyColor);
  syncColor(colorSettings, "LandColor", profileLandColor);
  syncColor(colorSettings, "LabelColor", profileLabelColor);
  syncColorArgb(colorSettings, "VasiAboveColor", profileVasiAboveColor);
  syncColorArgb(colorSettings, "VasiBelowColor", profileVasiBelowColor);
  syncColor(colorSettings, "AltRestrictionFill", profileAltRestrictionFill);
  syncColor(colorSettings, "AltRestrictionOutline", profileAltRestrictionOutline);
  syncPen(colorSettings, "LandOutlinePen", profileLandOutlinePen);
  syncPen(colorSettings, "WaypointLinePen", profileWaypointLinePen);
  syncPen(colorSettings, "ElevationScalePen", profileElevationScalePen);
  syncPen(colorSettings, "SafeAltLinePen", profileSafeAltLinePen);
  syncPen(colorSettings, "SafeAltLegLinePen", profileSafeAltLegLinePen);
  syncPen(colorSettings, "VasiCenterPen", profileVasiCenterPen);
  colorSettings.endGroup();

  colorSettings.beginGroup("Route");
  syncColor(colorSettings, "TextColor", routeTextColor);
  syncColor(colorSettings, "TextColorGray", routeTextColorGray);
  syncColor(colorSettings, "TextBackgroundColor", routeTextBackgroundColor);
  syncColor(colorSettings, "ProcedureMissedTextColor", routeProcedureMissedTextColor);
  syncColor(colorSettings, "ProcedureTextColor", routeProcedureTextColor);
  syncColor(colorSettings, "ProcedurePointColor", routeProcedurePointColor);
  syncColor(colorSettings, "ProcedurePointFlyoverColor", routeProcedurePointFlyoverColor);
  syncColor(colorSettings, "UserPointColor", routeUserPointColor);
  syncColor(colorSettings, "InvalidPointColor", routeInvalidPointColor);
  colorSettings.endGroup();

  colorSettings.beginGroup("Surface");
  syncColorArgb(colorSettings, "Concrete", surfaceConcrete);
  syncColorArgb(colorSettings, "Grass", surfaceGrass);
  syncColorArgb(colorSettings, "Water", surfaceWater);
  syncColorArgb(colorSettings, "Asphalt", surfaceAsphalt);
  syncColorArgb(colorSettings, "Cement", surfaceCement);
  syncColorArgb(colorSettings, "Clay", surfaceClay);
  syncColorArgb(colorSettings, "Snow", surfaceSnow);
  syncColorArgb(colorSettings, "Ice", surfaceIce);
  syncColorArgb(colorSettings, "Dirt", surfaceDirt);
  syncColorArgb(colorSettings, "Coral", surfaceCoral);
  syncColorArgb(colorSettings, "Gravel", surfaceGravel);
  syncColorArgb(colorSettings, "OilTreated", surfaceOilTreated);
  syncColorArgb(colorSettings, "SteelMats", surfaceSteelMats);
  syncColorArgb(colorSettings, "Bituminous", surfaceBituminous);
  syncColorArgb(colorSettings, "Brick", surfaceBrick);
  syncColorArgb(colorSettings, "Macadam", surfaceMacadam);
  syncColorArgb(colorSettings, "Planks", surfacePlanks);
  syncColorArgb(colorSettings, "Sand", surfaceSand);
  syncColorArgb(colorSettings, "Shale", surfaceShale);
  syncColorArgb(colorSettings, "Tarmac", surfaceTarmac);
  syncColorArgb(colorSettings, "Unknown", surfaceUnknown);
  syncColorArgb(colorSettings, "Transparent", surfaceTransparent);
  colorSettings.endGroup();

  // Sync airspace colors ============================================
  colorSettings.beginGroup("Airspace");
  syncColorArgb(colorSettings, "TextBoxColorAirspace", textBoxColorAirspace);

  for(auto it = airspaceConfigNames.constBegin(); it != airspaceConfigNames.constEnd(); ++it)
  {
    const QString& name = it.key();
    map::MapAirspaceTypes type = it.value();
    syncPen(colorSettings, name + "Pen", airspacePens[type]);
    syncColorArgb(colorSettings, name + "FillColor", airspaceFillColors[type]);
  }
  colorSettings.endGroup();

  colorSettings.sync();
#endif
}

void adjustPenForCircleToLand(QPainter *painter)
{
  // Use different pattern and smaller line for circle-to-land approaches
  QPen pen = painter->pen();
  pen.setStyle(Qt::DotLine);
  pen.setCapStyle(Qt::FlatCap);
  painter->setPen(pen);
}

void adjustPenForVectors(QPainter *painter)
{
  // Use different pattern and smaller line for vector legs
  QPen pen = painter->pen();
  pen.setStyle(Qt::DashLine);
  pen.setCapStyle(Qt::FlatCap);
  painter->setPen(pen);
}

void adjustPenForManual(QPainter *painter)
{
  // Use different pattern and smaller line for vector legs
  QPen pen = painter->pen();
  // The pattern must be specified as an even number of positive entries
  // where the entries 1, 3, 5... are the dashes and 2, 4, 6... are the spaces.
  pen.setDashPattern({1., 3.});
  pen.setCapStyle(Qt::FlatCap);
  painter->setPen(pen);
}

void adjustPenForAlternate(QPainter *painter)
{
  // Use different pattern and smaller line for vector legs
  QPen pen = painter->pen();
  pen.setStyle(Qt::DotLine);
  pen.setCapStyle(Qt::FlatCap);
  painter->setPen(pen);
  painter->setBackground(adjustAlphaF(Qt::white, static_cast<float>(painter->pen().color().alphaF())));
  painter->setBackgroundMode(Qt::OpaqueMode);
}

void scaleFont(QPainter *painter, float scale, const QFont *defaultFont)
{
  QFont font = painter->font();
  const QFont *defFont = defaultFont == nullptr ? &font : defaultFont;
  if(font.pixelSize() == -1)
  {
    // Use point size if pixel is not available
    double size = scale * defFont->pointSizeF();
    if(atools::almostNotEqual(size, font.pointSizeF()))
    {
      font.setPointSizeF(size);
      painter->setFont(font);
    }
  }
  else
  {
    int size = atools::roundToInt(scale * defFont->pixelSize());
    if(size != defFont->pixelSize())
    {
      font.setPixelSize(size);
      painter->setFont(font);
    }
  }
}

void darkenPainterRect(QPainter& painter)
{
  // Dim the map by drawing a semi-transparent black rectangle
  if(NavApp::isCurrentGuiStyleNight())
  {
    int dim = OptionData::instance().getGuiStyleMapDimming();
    QColor col = QColor::fromRgb(0, 0, 0, 255 - (255 * dim / 100));
    painter.fillRect(QRect(0, 0, painter.device()->width(), painter.device()->height()), col);
  }
}

QPen adjustAlphaF(QPen pen, float alpha)
{
  QColor color = pen.color();
  color.setAlphaF(alpha);
  pen.setColor(color);
  return pen;
}

QColor adjustAlphaF(QColor color, float alpha)
{
  color.setAlphaF(alpha);
  return color;
}

QPen adjustWidth(QPen pen, float width)
{
  pen.setWidthF(width);
  return pen;
}

} // namespace mapcolors
