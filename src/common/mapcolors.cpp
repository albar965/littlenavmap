/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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
#include <QStringBuilder>

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

QColor ilsFillColor(QStringLiteral("#40008000"));
QColor ilsTextColor(0, 30, 0);
QPen ilsCenterPen(Qt::darkGreen, 1.5, Qt::DashLine);
QColor ilsSymbolColor(Qt::darkGreen);

QColor glsFillColor(QStringLiteral("#20808080"));
QColor glsTextColor(20, 20, 20);
QPen glsCenterPen(Qt::darkGray, 1.5, Qt::DashLine);
QColor glsSymbolColor(Qt::darkGray);

QColor msaFillColor(QStringLiteral("#a0ffffff"));
QColor msaTextColor(Qt::darkGray);
QColor msaSymbolColor(Qt::darkGray);

QColor textBoxColorAirspace(255, 255, 255, 180);

QPen msaDiagramLinePen(QColor(QStringLiteral("#000000")), 2.);
QColor msaDiagramNumberColor(QStringLiteral("#70000000"));
QPen msaDiagramLinePenDark(QColor(QStringLiteral("#808080")), 2.);
QColor msaDiagramNumberColorDark(QStringLiteral("#70a0a0a0"));

QColor msaDiagramFillColor(QStringLiteral("#10000000"));
QColor msaDiagramFillColorDark(QStringLiteral("#10ffffff"));

QColor waypointSymbolColor(200, 0, 200);

QColor airwayVictorColor(QStringLiteral("#969696")); // 1.
QColor airwayJetColor(QStringLiteral("#000080")); // 1.
QColor airwayBothColor(QStringLiteral("#646464")); // 1.
QColor airwayTrackColor(QStringLiteral("#101010")); // 1.5
static QColor airwayTrackColorEast(QStringLiteral("#a01010")); // 1.5
static QColor airwayTrackColorWest(QStringLiteral("#1010a0")); // 1.5
QColor airwayTextColor(80, 80, 80);

QColor holdingColor(50, 50, 50);

QColor rangeRingColor(Qt::red);
QColor rangeRingTextColor(Qt::black);

QColor weatherWindGustColor(QStringLiteral("#ff8040"));
QColor weatherWindColor(Qt::black);
QColor weatherBackgoundColor(Qt::white);

QColor weatherLifrColor(QColor(QStringLiteral("#d000d0")));
QColor weatherIfrColor(QColor(QStringLiteral("#d00000")));
QColor weatherMvfrColor(QColor(QStringLiteral("#0000d0")));
QColor weatherVfrColor(QColor(QStringLiteral("#00b000")));
QColor weatherErrorColor(QColor(QStringLiteral("#808080")));

QPen minimumAltitudeGridPen(QColor(QStringLiteral("#a0a0a0")), 1.);
QColor minimumAltitudeNumberColor(QColor(QStringLiteral("#70000000")));

QPen minimumAltitudeGridPenDark(QColor(QStringLiteral("#808080")), 1.);
QColor minimumAltitudeNumberColorDark(QColor(QStringLiteral("#70a0a0a0")));

QColor compassRoseColor(Qt::darkRed);
QColor compassRoseTextColor(Qt::black);

/* Elevation profile colors and pens */
QColor profileSkyColor(QColor(204, 204, 255));
QColor profileLandColor(QColor(0, 128, 0));
QColor profileLabelColor(QColor(0, 0, 0));

QColor profileVasiAboveColor(QColor(QStringLiteral("#70ffffff")));
QColor profileVasiBelowColor(QColor(QStringLiteral("#70ff0000")));

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
QPen markEnduranceCritPen(Qt::red, 2., Qt::DotLine, Qt::FlatCap, Qt::MiterJoin);
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
QColor touchRegionFillColor(QStringLiteral("#40888888"));

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

QColor routeDirectToDepartureBackgroundColor(255, 255, 255);
QPen routeDirectToDeparturePen(QColor(0, 0, 0), 3., Qt::DotLine, Qt::FlatCap, Qt::MiterJoin);

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

/* Surface colors for runways, aprons and taxiways */
/* These are defaults and are overridden by little_navmap_mapstyle.ini */
static QColor surfaceConcrete(QStringLiteral("#888888"));
static QColor surfaceGrass(QStringLiteral("#00a000"));
static QColor surfaceWater(QStringLiteral("#808585ff"));
static QColor surfaceAsphalt(QStringLiteral("#707070"));
static QColor surfaceCement(QStringLiteral("#a0a0a0"));
static QColor surfaceClay(QStringLiteral("#DEB887"));
static QColor surfaceSnow(QStringLiteral("#dbdbdb"));
static QColor surfaceIce(QStringLiteral("#d0d0ff"));
static QColor surfaceDirt(QStringLiteral("#CD853F"));
static QColor surfaceCoral(QStringLiteral("#FFE4C4"));
static QColor surfaceGravel(QStringLiteral("#c0c0c0"));
static QColor surfaceOilTreated(QStringLiteral("#2F4F4F"));
static QColor surfaceSteelMats(QStringLiteral("#a0f0ff"));
static QColor surfaceBituminous(QStringLiteral("#505050"));
static QColor surfaceBrick(QStringLiteral("#A0522D"));
static QColor surfaceMacadam(QStringLiteral("#c8c8c8"));
static QColor surfacePlanks(QStringLiteral("#8B4513"));
static QColor surfaceSand(QStringLiteral("#F4A460"));
static QColor surfaceShale(QStringLiteral("#F5DEB3"));
static QColor surfaceTarmac(QStringLiteral("#909090"));
static QColor surfaceUnknown(QStringLiteral("#d0d0d0")); // Ensure visibility for dark and bright maps
static QColor surfaceTransparent(QStringLiteral("#d0d0d0"));

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
  if(type == QStringLiteral("RMCB") || type == QStringLiteral("RMC") || type.startsWith(QStringLiteral("G")) ||
     type.startsWith(QStringLiteral("RGA")) || type.startsWith(QStringLiteral("DGA")) ||
     type.startsWith(QStringLiteral("RC")) || type.startsWith(QStringLiteral("FUEL")) || type == (QStringLiteral("H")) ||
     type == (QStringLiteral("T")) || type == (QStringLiteral("RE")) ||
     type == (QStringLiteral("GE")))
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
  static const QColor unknown(QStringLiteral("#808080"));

  if(type == QStringLiteral("RM"))
    return rampMil;
  else if(type == QStringLiteral("RMC"))
    return rampMilCargo;
  else if(type == QStringLiteral("RMCB"))
    return rampMilCombat;
  else if(type.startsWith(QStringLiteral("G")))
    return gate;
  else if(type.startsWith(QStringLiteral("RGA")) || type.startsWith(QStringLiteral("DGA")) || type.startsWith(QStringLiteral("RE")))
    return rampGa;
  else if(type.startsWith(QStringLiteral("RC")))
    return rampCargo;
  else if(type.startsWith(QStringLiteral("FUEL")))
    return fuel;
  else if(type == (QStringLiteral("H")))
    return hangar;
  else if(type == (QStringLiteral("T")))
    return tiedown;
  else
    return unknown;
}

const QColor& colorTextForParkingType(const QString& type)
{
  if(type == QStringLiteral("RMCB"))
    return mapcolors::brightParkingTextColor;
  else if(type == QStringLiteral("RMC"))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith(QStringLiteral("G")))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith(QStringLiteral("RGA")) || type.startsWith(QStringLiteral("DGA")) || type.startsWith(QStringLiteral("RE")))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith(QStringLiteral("RC")))
    return mapcolors::brightParkingTextColor;
  else if(type.startsWith(QStringLiteral("FUEL")))
    return mapcolors::darkParkingTextColor;
  else if(type == (QStringLiteral("H")))
    return brightParkingTextColor;
  else if(type == (QStringLiteral("T")))
    return brightParkingTextColor;
  else
    return brightParkingTextColor;
}

const QIcon& iconForStart(const map::MapStart& start)
{
  static const QIcon runway(QStringLiteral(":/littlenavmap/resources/icons/startrunway.svg"));
  static const QIcon helipad(QStringLiteral(":/littlenavmap/resources/icons/starthelipad.svg"));
  static const QIcon water(QStringLiteral(":/littlenavmap/resources/icons/startwater.svg"));

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
  static const QIcon cargo(QStringLiteral(":/littlenavmap/resources/icons/parkingrampcargo.svg"));
  static const QIcon ga(QStringLiteral(":/littlenavmap/resources/icons/parkingrampga.svg"));
  static const QIcon mil(QStringLiteral(":/littlenavmap/resources/icons/parkingrampmil.svg"));
  static const QIcon gate(QStringLiteral(":/littlenavmap/resources/icons/parkinggate.svg"));
  static const QIcon fuel(QStringLiteral(":/littlenavmap/resources/icons/parkingfuel.svg"));
  static const QIcon hangar(QStringLiteral(":/littlenavmap/resources/icons/parkinghangar.svg"));
  static const QIcon tiedown(QStringLiteral(":/littlenavmap/resources/icons/parkingtiedown.svg"));
  static const QIcon unknown(QStringLiteral(":/littlenavmap/resources/icons/parkingunknown.svg"));

  if(type.startsWith(QStringLiteral("RM")))
    return mil;
  else if(type.startsWith(QStringLiteral("G")))
    return gate;
  else if(type.startsWith(QStringLiteral("RGA")) || type.startsWith(QStringLiteral("DGA")) || type == QStringLiteral("RE"))
    return ga;
  else if(type.startsWith(QStringLiteral("RC")))
    return cargo;
  else if(type.startsWith(QStringLiteral("FUEL")))
    return fuel;
  else if(type == (QStringLiteral("H")))
    return hangar;
  else if(type == (QStringLiteral("T")))
    return tiedown;
  else
    return unknown;
}

const QColor& colorForSurface(const QString& surface)
{
  if(surface == QStringLiteral("A"))
    return surfaceAsphalt;
  else if(surface == QStringLiteral("G"))
    return surfaceGrass;
  else if(surface == QStringLiteral("D"))
    return surfaceDirt;
  else if(surface == QStringLiteral("C"))
    return surfaceConcrete;
  else if(surface == QStringLiteral("GR"))
    return surfaceGravel;
  else if(surface == QStringLiteral("W"))
    return surfaceWater;
  else if(surface == QStringLiteral("CE"))
    return surfaceCement;
  else if(surface == QStringLiteral("CL"))
    return surfaceClay;
  else if(surface == QStringLiteral("SN"))
    return surfaceSnow;
  else if(surface == QStringLiteral("I"))
    return surfaceIce;
  else if(surface == QStringLiteral("CR"))
    return surfaceCoral;
  else if(surface == QStringLiteral("OT"))
    return surfaceOilTreated;
  else if(surface == QStringLiteral("SM"))
    return surfaceSteelMats;
  else if(surface == QStringLiteral("B"))
    return surfaceBituminous;
  else if(surface == QStringLiteral("BR"))
    return surfaceBrick;
  else if(surface == QStringLiteral("M"))
    return surfaceMacadam;
  else if(surface == QStringLiteral("PL"))
    return surfacePlanks;
  else if(surface == QStringLiteral("S"))
    return surfaceSand;
  else if(surface == QStringLiteral("SH"))
    return surfaceShale;
  else if(surface == QStringLiteral("T"))
    return surfaceTarmac;
  else if(surface == QStringLiteral("TR"))
    return surfaceTransparent;

  // else if(surface == QStringLiteral("NONE") || surface == QStringLiteral("UNKNOWN") || surface == QStringLiteral("INVALID")) || TR
  return surfaceUnknown;
}

const QPen aircraftTrailPenOuter(float size)
{
  return QPen(NavApp::isDarkMapTheme() ? Qt::white : Qt::black, size + 3., Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
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
    QPen pen(col, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
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
  {
    // Styled pens ===========================================================
    switch(optionData.getDisplayTrailType())
    {
      case opts::TRAIL_TYPE_DASHED:
        return QPen(optionData.getTrailColor(), size, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin);

      case opts::TRAIL_TYPE_DOTTED:
        return QPen(optionData.getTrailColor(), size, Qt::DotLine, Qt::FlatCap, Qt::MiterJoin);

      case opts::TRAIL_TYPE_SOLID:
        return QPen(optionData.getTrailColor(), size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    }
  }
  return QPen();
}

/* Default colors. Saved to little_navmap_mapstyle.ini and can be overridden there */
static QHash<map::MapAirspaceType, QColor> airspaceFillColors(
  {
    {map::AIRSPACE_NONE, QColor(QStringLiteral("#00000000"))},
    {map::CENTER, QColor(QStringLiteral("#30808080"))},
    {map::CLASS_A, QColor(QStringLiteral("#308d0200"))},
    {map::CLASS_B, QColor(QStringLiteral("#30902ece"))},
    {map::CLASS_C, QColor(QStringLiteral("#308594ec"))},
    {map::CLASS_D, QColor(QStringLiteral("#306c5bce"))},
    {map::CLASS_E, QColor(QStringLiteral("#30cc5060"))},
    {map::CLASS_F, QColor(QStringLiteral("#307d8000"))},
    {map::CLASS_G, QColor(QStringLiteral("#30cc8040"))},
    {map::FIR, QColor(QStringLiteral("#30606080"))},
    {map::UIR, QColor(QStringLiteral("#30404080"))},
    {map::TOWER, QColor(QStringLiteral("#300000f0"))},
    {map::CLEARANCE, QColor(QStringLiteral("#3060808a"))},
    {map::GROUND, QColor(QStringLiteral("#30000000"))},
    {map::DEPARTURE, QColor(QStringLiteral("#3060808a"))},
    {map::APPROACH, QColor(QStringLiteral("#3060808a"))},
    {map::MOA, QColor(QStringLiteral("#304485b7"))},
    {map::RESTRICTED, QColor(QStringLiteral("#30fd8c00"))},
    {map::PROHIBITED, QColor(QStringLiteral("#30f00909"))},
    {map::WARNING, QColor(QStringLiteral("#30fd8c00"))},
    {map::CAUTION, QColor(QStringLiteral("#50fd8c00"))},
    {map::ALERT, QColor(QStringLiteral("#30fd8c00"))},
    {map::DANGER, QColor(QStringLiteral("#30dd103d"))},
    {map::NATIONAL_PARK, QColor(QStringLiteral("#30509090"))},
    {map::MODEC, QColor(QStringLiteral("#30509090"))},
    {map::RADAR, QColor(QStringLiteral("#30509090"))},
    {map::GCA, QColor(QStringLiteral("#30902ece"))},
    {map::MCTR, QColor(QStringLiteral("#30f00b0b"))},
    {map::TRSA, QColor(QStringLiteral("#30902ece"))},
    {map::TRAINING, QColor(QStringLiteral("#30509090"))},
    {map::GLIDERPROHIBITED, QColor(QStringLiteral("#30fd8c00"))},
    {map::WAVEWINDOW, QColor(QStringLiteral("#304485b7"))},
    {map::ONLINE_OBSERVER, QColor(QStringLiteral("#3000a000"))}
  }
  );

/* Default colors. Saved to little_navmap_mapstyle.ini and can be overridden there */
static QHash<map::MapAirspaceType, QPen> airspacePens(
  {
    {map::AIRSPACE_NONE, QPen(QColor(QStringLiteral("#00000000")))},
    {map::CENTER, QPen(QColor(QStringLiteral("#808080")), 1.5)},
    {map::CLASS_A, QPen(QColor(QStringLiteral("#8d0200")), 2)},
    {map::CLASS_B, QPen(QColor(QStringLiteral("#902ece")), 2)},
    {map::CLASS_C, QPen(QColor(QStringLiteral("#8594ec")), 2)},
    {map::CLASS_D, QPen(QColor(QStringLiteral("#6c5bce")), 2)},
    {map::CLASS_E, QPen(QColor(QStringLiteral("#cc5060")), 2)},
    {map::CLASS_F, QPen(QColor(QStringLiteral("#7d8000")), 2)},
    {map::CLASS_G, QPen(QColor(QStringLiteral("#cc8040")), 2)},
    {map::FIR, QPen(QColor(QStringLiteral("#606080")), 1.5)},
    {map::UIR, QPen(QColor(QStringLiteral("#404080")), 1.5)},
    {map::TOWER, QPen(QColor(QStringLiteral("#6000a0")), 2)},
    {map::CLEARANCE, QPen(QColor(QStringLiteral("#60808a")), 2)},
    {map::GROUND, QPen(QColor(QStringLiteral("#000000")), 2)},
    {map::DEPARTURE, QPen(QColor(QStringLiteral("#60808a")), 2)},
    {map::APPROACH, QPen(QColor(QStringLiteral("#60808a")), 2)},
    {map::MOA, QPen(QColor(QStringLiteral("#4485b7")), 2)},
    {map::RESTRICTED, QPen(QColor(QStringLiteral("#fd8c00")), 2)},
    {map::PROHIBITED, QPen(QColor(QStringLiteral("#f00909")), 3)},
    {map::WARNING, QPen(QColor(QStringLiteral("#fd8c00")), 2)},
    {map::CAUTION, QPen(QColor(QStringLiteral("#ff6c00")), 2)},
    {map::ALERT, QPen(QColor(QStringLiteral("#fd8c00")), 2)},
    {map::DANGER, QPen(QColor(QStringLiteral("#dd103d")), 2)},
    {map::NATIONAL_PARK, QPen(QColor(QStringLiteral("#509090")), 2)},
    {map::MODEC, QPen(QColor(QStringLiteral("#509090")), 2)},
    {map::RADAR, QPen(QColor(QStringLiteral("#509090")), 2)},
    {map::GCA, QPen(QColor(QStringLiteral("#902ece")), 2)},
    {map::MCTR, QPen(QColor(QStringLiteral("#f00b0b")), 2)},
    {map::TRSA, QPen(QColor(QStringLiteral("#902ece")), 2)},
    {map::TRAINING, QPen(QColor(QStringLiteral("#509090")), 2)},
    {map::GLIDERPROHIBITED, QPen(QColor(QStringLiteral("#fd8c00")), 2)},
    {map::WAVEWINDOW, QPen(QColor(QStringLiteral("#4485b7")), 2)},
    {map::ONLINE_OBSERVER, QPen(QColor(QStringLiteral("#a000a000")), 1.5)}
  }
  );

/* Maps airspace types to color configuration options */
static QHash<QString, map::MapAirspaceType> airspaceConfigNames(
  {
    {QStringLiteral("Center"), map::CENTER},
    {QStringLiteral("ClassA"), map::CLASS_A},
    {QStringLiteral("ClassB"), map::CLASS_B},
    {QStringLiteral("ClassC"), map::CLASS_C},
    {QStringLiteral("ClassD"), map::CLASS_D},
    {QStringLiteral("ClassE"), map::CLASS_E},
    {QStringLiteral("ClassF"), map::CLASS_F},
    {QStringLiteral("ClassG"), map::CLASS_G},
    {QStringLiteral("FIR"), map::FIR},
    {QStringLiteral("UIR"), map::UIR},
    {QStringLiteral("Tower"), map::TOWER},
    {QStringLiteral("Clearance"), map::CLEARANCE},
    {QStringLiteral("Ground"), map::GROUND},
    {QStringLiteral("Departure"), map::DEPARTURE},
    {QStringLiteral("Approach"), map::APPROACH},
    {QStringLiteral("Moa"), map::MOA},
    {QStringLiteral("Restricted"), map::RESTRICTED},
    {QStringLiteral("Prohibited"), map::PROHIBITED},
    {QStringLiteral("Warning"), map::WARNING},
    {QStringLiteral("Caution"), map::CAUTION},
    {QStringLiteral("Alert"), map::ALERT},
    {QStringLiteral("Danger"), map::DANGER},
    {QStringLiteral("NationalPark"), map::NATIONAL_PARK},
    {QStringLiteral("Modec"), map::MODEC},
    {QStringLiteral("Radar"), map::RADAR},
    {QStringLiteral("Gca"), map::GCA},
    {QStringLiteral("Mctr"), map::MCTR},
    {QStringLiteral("Trsa"), map::TRSA},
    {QStringLiteral("Training"), map::TRAINING},
    {QStringLiteral("GliderProhibited"), map::GLIDERPROHIBITED},
    {QStringLiteral("WaveWindow"), map::WAVEWINDOW},
    {QStringLiteral("Observer"), map::ONLINE_OBSERVER}
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

const QColor colorForAirwayOrTrack(const map::MapAirway& airway, bool darkMap)
{
  static QColor EMPTY_COLOR;

  switch(airway.type)
  {
    case map::NO_AIRWAY:
      break;

    case map::TRACK_NAT:
    case map::TRACK_PACOTS:
      if(airway.westCourse)
        return airwayTrackColorWest.lighter(darkMap ? 200 : 100);
      else if(airway.eastCourse)
        return airwayTrackColorEast.lighter(darkMap ? 200 : 100);
      else
        return airwayTrackColor.lighter(darkMap ? 200 : 100);

    case map::AIRWAY_VICTOR:
      return airwayVictorColor.darker(darkMap ? 180 : 100);

    case map::AIRWAY_JET:
      return airwayJetColor.lighter(darkMap ? 180 : 100);

    case map::AIRWAY_BOTH:
      return airwayBothColor.lighter(darkMap ? 150 : 100);
  }
  return EMPTY_COLOR;
}

/* Read ARGB color if value exists in settings or update in settings with given value */
void loadColorArgb(const QSettings& settings, const QString& key, QColor& color)
{
  if(settings.contains(key))
    color = QColor::fromString(settings.value(key).toString());
}

/* Read color if value exists in settings or update in settings with given value */
void loadColor(const QSettings& settings, const QString& key, QColor& color)
{
  if(settings.contains(key))
    color = QColor::fromString(settings.value(key).toString());
}

/* Read color and pen width if value exists in settings or update in settings with values of given pen */
void loadPen(const QSettings& settings, const QString& key, QPen& pen)
{
  const static QHash<QString, Qt::PenStyle> PEN_TO_STYLE({
      {QStringLiteral("Solid"), Qt::SolidLine},
      {QStringLiteral("Dash"), Qt::DashLine},
      {QStringLiteral("Dot"), Qt::DotLine},
      {QStringLiteral("DashDot"), Qt::DashDotLine},
      {QStringLiteral("DashDotDot"), Qt::DashDotDotLine}
    });

  if(settings.contains(key))
  {
    const QStringList list = settings.value(key).toStringList();
    if(!list.isEmpty())
    {
      pen.setColor(QColor(list.at(0)));

      if(list.size() >= 2)
        pen.setWidthF(list.at(1).toFloat());

      if(list.size() >= 3)
        pen.setStyle(PEN_TO_STYLE.value(list.at(2), Qt::SolidLine));
    }
  }
}

void loadColors()
{
#ifndef DEBUG_DISABLE_SYNC_COLORS
  QString filename = atools::settings::Settings::getOverloadedPath(lnm::MAPSTYLE_CONFIG);
  qInfo() << Q_FUNC_INFO << "Loading mapstyle from" << filename;

  const QSettings colorSettings(filename, QSettings::IniFormat);

  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteProcedureMissedTableColorDark"), routeProcedureMissedTableColorDark);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteProcedureMissedTableColor"), routeProcedureMissedTableColor);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteProcedureTableColorDark"), routeProcedureTableColorDark);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteProcedureTableColor"), routeProcedureTableColor);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteAlternateTableColor"), routeAlternateTableColor);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteAlternateTableColorDark"), routeAlternateTableColorDark);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteInvalidTableColor"), routeInvalidTableColor);
  loadColor(colorSettings, QStringLiteral("FlightPlan/RouteInvalidTableColorDark"), routeInvalidTableColorDark);
  loadColor(colorSettings, QStringLiteral("FlightPlan/NextWaypointColor"), nextWaypointColor);
  loadColor(colorSettings, QStringLiteral("FlightPlan/NextWaypointColorDark"), nextWaypointColorDark);

  loadColorArgb(colorSettings, QStringLiteral("Aircraft/UserLabelColor"), aircraftUserLabelColor);
  loadColorArgb(colorSettings, QStringLiteral("Aircraft/UserLabelBackgroundColor"), aircraftUserLabelColorBg);
  loadColorArgb(colorSettings, QStringLiteral("Aircraft/AiLabelColor"), aircraftAiLabelColor);
  loadColorArgb(colorSettings, QStringLiteral("Aircraft/AiLabelBackgroundColor"), aircraftAiLabelColorBg);

  loadColor(colorSettings, QStringLiteral("Airport/DiagramBackgroundColor"), airportDetailBackColor);
  loadColor(colorSettings, QStringLiteral("Airport/EmptyColor"), airportEmptyColor);
  loadColor(colorSettings, QStringLiteral("Airport/ToweredColor"), toweredAirportColor);
  loadColor(colorSettings, QStringLiteral("Airport/UnToweredColor"), unToweredAirportColor);
  loadColor(colorSettings, QStringLiteral("Airport/AddonBackgroundColor"), addonAirportBackgroundColor);
  loadColor(colorSettings, QStringLiteral("Airport/AddonFrameColor"), addonAirportFrameColor);
  loadPen(colorSettings, QStringLiteral("Airport/TaxiwayLinePen"), taxiwayLinePen);
  loadColor(colorSettings, QStringLiteral("Airport/TaxiwayNameColor"), taxiwayNameColor);
  loadColor(colorSettings, QStringLiteral("Airport/TaxiwayNameBackgroundColor"), taxiwayNameBackgroundColor);

  loadColor(colorSettings, QStringLiteral("Navaid/VorColor"), vorSymbolColor);
  loadColor(colorSettings, QStringLiteral("Navaid/NdbColor"), ndbSymbolColor);
  loadColor(colorSettings, QStringLiteral("Navaid/MarkerColor"), markerSymbolColor);
  loadColor(colorSettings, QStringLiteral("Navaid/IlsColor"), ilsSymbolColor);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/IlsFillColor"), ilsFillColor);
  loadColor(colorSettings, QStringLiteral("Navaid/IlsTextColor"), ilsTextColor);
  loadPen(colorSettings, QStringLiteral("Navaid/IlsCenterPen"), ilsCenterPen);

  loadColor(colorSettings, QStringLiteral("Navaid/GlsGbasColor"), glsSymbolColor);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/GlsGbasFillColor"), glsFillColor);
  loadColor(colorSettings, QStringLiteral("Navaid/GlsTextColor"), glsTextColor);
  loadPen(colorSettings, QStringLiteral("Navaid/GlsCenterPen"), glsCenterPen);

  loadColor(colorSettings, QStringLiteral("Navaid/MsaTextColor"), msaTextColor);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/MsaFillColor"), msaFillColor);
  loadColor(colorSettings, QStringLiteral("Navaid/MsaSymbolColor"), msaSymbolColor);

  loadPen(colorSettings, QStringLiteral("Navaid/MsaDiagramLinePen"), msaDiagramLinePen);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/MsaDiagramNumberColor"), msaDiagramNumberColor);
  loadPen(colorSettings, QStringLiteral("Navaid/MsaDiagramLinePenDark"), msaDiagramLinePenDark);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/MsaDiagramNumberColorDark"), msaDiagramNumberColorDark);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/MsaDiagramFillColor"), msaDiagramFillColor);
  loadColorArgb(colorSettings, QStringLiteral("Navaid/MsaDiagramFillColorDark"), msaDiagramFillColorDark);

  loadColor(colorSettings, QStringLiteral("Navaid/WaypointColor"), waypointSymbolColor);
  loadColor(colorSettings, QStringLiteral("Navaid/HoldingColor"), holdingColor);

  loadColor(colorSettings, QStringLiteral("Airway/VictorColor"), airwayVictorColor);
  loadColor(colorSettings, QStringLiteral("Airway/JetColor"), airwayJetColor);
  loadColor(colorSettings, QStringLiteral("Airway/BothColor"), airwayBothColor);
  loadColor(colorSettings, QStringLiteral("Airway/TrackColor"), airwayTrackColor);
  loadColor(colorSettings, QStringLiteral("Airway/TrackColorEast"), airwayTrackColorEast);
  loadColor(colorSettings, QStringLiteral("Airway/TrackColorWest"), airwayTrackColorWest);
  loadColor(colorSettings, QStringLiteral("Airway/TextColor"), airwayTextColor);

  loadColor(colorSettings, QStringLiteral("Marker/RangeRingColor"), rangeRingColor);
  loadColor(colorSettings, QStringLiteral("Marker/RangeRingTextColor"), rangeRingTextColor);
  loadColor(colorSettings, QStringLiteral("Marker/CompassRoseColor"), compassRoseColor);
  loadColor(colorSettings, QStringLiteral("Marker/CompassRoseTextColor"), compassRoseTextColor);
  loadPen(colorSettings, QStringLiteral("Marker/SearchCenterBackPen"), searchCenterBackPen);
  loadPen(colorSettings, QStringLiteral("Marker/SearchCenterFillPen"), searchCenterFillPen);
  loadPen(colorSettings, QStringLiteral("Marker/TouchMarkBackPen"), touchMarkBackPen);
  loadPen(colorSettings, QStringLiteral("Marker/TouchMarkFillPen"), touchMarkFillPen);
  loadPen(colorSettings, QStringLiteral("Marker/EndurancePen"), markEndurancePen);
  loadPen(colorSettings, QStringLiteral("Marker/EnduranceCritPen"), markEnduranceCritPen);
  loadPen(colorSettings, QStringLiteral("Marker/SelectedAltitudeRangePen"), markSelectedAltitudeRangePen);
  loadPen(colorSettings, QStringLiteral("Marker/TurnPathPen"), markTurnPathPen);
  loadColorArgb(colorSettings, QStringLiteral("Marker/TouchRegionFillColor"), touchRegionFillColor);
  loadColor(colorSettings, QStringLiteral("Marker/DistanceMarkerTextColor"), distanceMarkerTextColor);
  loadColorArgb(colorSettings, QStringLiteral("Marker/DistanceMarkerTextBackgroundColor"), distanceMarkerTextBackgroundColor);

  loadColor(colorSettings, QStringLiteral("Highlight/HighlightBackColor"), highlightBackColor);
  loadColor(colorSettings, QStringLiteral("Highlight/RouteHighlightBackColor"), routeHighlightBackColor);
  loadColor(colorSettings, QStringLiteral("Highlight/ProfileHighlightBackColor"), profileHighlightBackColor);

  loadColor(colorSettings, QStringLiteral("Print/MapPrintRowColor"), mapPrintRowColor);
  loadColor(colorSettings, QStringLiteral("Print/MapPrintRowColorAlt"), mapPrintRowColorAlt);
  loadColor(colorSettings, QStringLiteral("Print/MapPrintHeaderColor"), mapPrintHeaderColor);

  loadColor(colorSettings, QStringLiteral("Weather/WeatherBackgoundColor"), weatherBackgoundColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherWindColor"), weatherWindColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherWindGustColor"), weatherWindGustColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherLifrColor"), weatherLifrColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherIfrColor"), weatherIfrColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherMvfrColor"), weatherMvfrColor);
  loadColor(colorSettings, QStringLiteral("Weather/WeatherVfrColor"), weatherVfrColor);

  loadPen(colorSettings, QStringLiteral("AltitudeGrid/MinimumAltitudeGridPen"), minimumAltitudeGridPen);
  loadColorArgb(colorSettings, QStringLiteral("AltitudeGrid/MinimumAltitudeNumberColor"), minimumAltitudeNumberColor);
  loadPen(colorSettings, QStringLiteral("AltitudeGrid/MinimumAltitudeGridPenDark"), minimumAltitudeGridPenDark);
  loadColorArgb(colorSettings, QStringLiteral("AltitudeGrid/MinimumAltitudeNumberColorDark"), minimumAltitudeNumberColorDark);

  loadColor(colorSettings, QStringLiteral("Profile/SkyColor"), profileSkyColor);
  loadColor(colorSettings, QStringLiteral("Profile/LandColor"), profileLandColor);
  loadColor(colorSettings, QStringLiteral("Profile/LabelColor"), profileLabelColor);
  loadColorArgb(colorSettings, QStringLiteral("Profile/VasiAboveColor"), profileVasiAboveColor);
  loadColorArgb(colorSettings, QStringLiteral("Profile/VasiBelowColor"), profileVasiBelowColor);
  loadColor(colorSettings, QStringLiteral("Profile/AltRestrictionFill"), profileAltRestrictionFill);
  loadColor(colorSettings, QStringLiteral("Profile/AltRestrictionOutline"), profileAltRestrictionOutline);
  loadPen(colorSettings, QStringLiteral("Profile/LandOutlinePen"), profileLandOutlinePen);
  loadPen(colorSettings, QStringLiteral("Profile/WaypointLinePen"), profileWaypointLinePen);
  loadPen(colorSettings, QStringLiteral("Profile/ElevationScalePen"), profileElevationScalePen);
  loadPen(colorSettings, QStringLiteral("Profile/SafeAltLinePen"), profileSafeAltLinePen);
  loadPen(colorSettings, QStringLiteral("Profile/SafeAltLegLinePen"), profileSafeAltLegLinePen);
  loadPen(colorSettings, QStringLiteral("Profile/VasiCenterPen"), profileVasiCenterPen);

  loadColor(colorSettings, QStringLiteral("Route/TextColor"), routeTextColor);
  loadColor(colorSettings, QStringLiteral("Route/TextColorGray"), routeTextColorGray);
  loadColor(colorSettings, QStringLiteral("Route/TextBackgroundColor"), routeTextBackgroundColor);
  loadColor(colorSettings, QStringLiteral("Route/ProcedureMissedTextColor"), routeProcedureMissedTextColor);
  loadColor(colorSettings, QStringLiteral("Route/ProcedureTextColor"), routeProcedureTextColor);
  loadColor(colorSettings, QStringLiteral("Route/ProcedurePointColor"), routeProcedurePointColor);
  loadColor(colorSettings, QStringLiteral("Route/ProcedurePointFlyoverColor"), routeProcedurePointFlyoverColor);
  loadColor(colorSettings, QStringLiteral("Route/UserPointColor"), routeUserPointColor);
  loadColor(colorSettings, QStringLiteral("Route/InvalidPointColor"), routeInvalidPointColor);

  loadPen(colorSettings, QStringLiteral("Route/RouteDirectToDeparturePen"), routeDirectToDeparturePen);
  loadColor(colorSettings, QStringLiteral("Route/RouteDirectToDepartureBackgroundColor"), routeDirectToDepartureBackgroundColor);

  loadColorArgb(colorSettings, QStringLiteral("Surface/Concrete"), surfaceConcrete);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Grass"), surfaceGrass);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Water"), surfaceWater);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Asphalt"), surfaceAsphalt);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Cement"), surfaceCement);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Clay"), surfaceClay);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Snow"), surfaceSnow);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Ice"), surfaceIce);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Dirt"), surfaceDirt);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Coral"), surfaceCoral);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Gravel"), surfaceGravel);
  loadColorArgb(colorSettings, QStringLiteral("Surface/OilTreated"), surfaceOilTreated);
  loadColorArgb(colorSettings, QStringLiteral("Surface/SteelMats"), surfaceSteelMats);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Bituminous"), surfaceBituminous);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Brick"), surfaceBrick);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Macadam"), surfaceMacadam);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Planks"), surfacePlanks);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Sand"), surfaceSand);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Shale"), surfaceShale);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Tarmac"), surfaceTarmac);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Unknown"), surfaceUnknown);
  loadColorArgb(colorSettings, QStringLiteral("Surface/Transparent"), surfaceTransparent);

  // Sync airspace colors ============================================
  loadColorArgb(colorSettings, QStringLiteral("Airspace/TextBoxColorAirspace"), textBoxColorAirspace);

  for(auto it = airspaceConfigNames.constBegin(); it != airspaceConfigNames.constEnd(); ++it)
  {
    const QString& name = it.key();
    map::MapAirspaceType type = it.value();
    loadPen(colorSettings, QStringLiteral("Airspace/") % name % QStringLiteral("Pen"), airspacePens[type]);
    loadColorArgb(colorSettings, QStringLiteral("Airspace/") % name % QStringLiteral("FillColor"), airspaceFillColors[type]);
  }
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
  if(NavApp::isGuiStyleDark())
  {
    int dim = OptionData::instance().getGuiStyleMapDimming();
    QColor col = QColor::fromRgb(0, 0, 0, 255 - (255 * dim / 100));
    painter.fillRect(QRect(0, 0, painter.device()->width(), painter.device()->height()), col);
  }
}

QPen adjustAlphaF(QPen pen, float alpha)
{
  QColor color = pen.color();
  color.setAlphaF(atools::minmax(0.f, 1.f, alpha));
  pen.setColor(color);
  return pen;
}

QColor adjustAlphaF(QColor color, float alpha)
{
  color.setAlphaF(atools::minmax(0.f, 1.f, alpha));
  return color;
}

QPen adjustAlpha(QPen pen, int alpha)
{
  QColor color = pen.color();
  color.setAlpha(atools::minmax(0, 255, alpha));
  pen.setColor(color);
  return pen;
}

QColor adjustAlpha(QColor color, int alpha)
{
  color.setAlpha(atools::minmax(0, 255, alpha));
  return color;
}

QPen adjustWidth(QPen pen, float width)
{
  pen.setWidthF(width);
  return pen;
}

} // namespace mapcolors
