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

#ifndef LITTLENAVMAP_MAPCOLORS_H
#define LITTLENAVMAP_MAPCOLORS_H

#include <QColor>
#include <QPen>
#include <QStyle>

namespace map {
struct MapAirport;

struct MapAirspace;
struct MapAirway;
struct MapStart;

}

class QSettings;

/* All colors and pens used on the map and profile display
 * Colors are partially loaded from a configuration file. */
namespace mapcolors {

/* Load/save colors from/to configuration file */
void syncColors();

/* Update some colors on GUI style change */
void styleChanged();

/* Initialize some color after applying style initially */
void init();

// ==========================================================================
/* Colors and pens that are updated from confguration file by syncColors */
extern QColor airportDetailBackColor;

extern QColor airportEmptyColor;
extern QColor toweredAirportColor;
extern QColor unToweredAirportColor;
extern QColor addonAirportBackgroundColor;
extern QColor addonAirportFrameColor;
extern QColor vorSymbolColor;
extern QColor ndbSymbolColor;
extern QColor markerSymbolColor;

extern QColor ilsSymbolColor;
extern QColor ilsFillColor;
extern QColor ilsTextColor;

extern QColor glsSymbolColor;
extern QColor glsFillColor;
extern QColor glsTextColor;
extern QColor waypointSymbolColor;
extern QColor airwayVictorColor;
extern QColor airwayJetColor;
extern QColor airwayBothColor;
extern QColor airwayTrackColor;
extern QColor airwayTextColor;
extern QColor holdingColor;
extern QColor rangeRingColor;
extern QColor rangeRingTextColor;
extern QColor compassRoseColor;
extern QColor compassRoseTextColor;

/* MSA symbol */
extern QColor msaFillColor;
extern QColor msaTextColor;
extern QColor msaSymbolColor;

/* MSA large diagram */
extern QPen msaDiagramLinePen;
extern QColor msaDiagramNumberColor;
extern QPen msaDiagramLinePenDark;
extern QColor msaDiagramNumberColorDark;
extern QColor msaDiagramFillColor;
extern QColor msaDiagramFillColorDark;

/* Weather icon colors */
extern QColor weatherBackgoundColor;
extern QColor weatherWindColor;
extern QColor weatherWindGustColor;
extern QColor weatherLifrColor;
extern QColor weatherIfrColor;
extern QColor weatherMvfrColor;
extern QColor weatherVfrColor;

/* Minimum off route altitude (MORA) */
extern QPen minimumAltitudeGridPen;
extern QColor minimumAltitudeNumberColor;

/* For dark map themes */
extern QPen minimumAltitudeGridPenDark;
extern QColor minimumAltitudeNumberColorDark;

/* Elevation profile colors and pens */
extern QColor profileSkyColor;
extern QColor profileLandColor;
extern QColor profileLabelColor;
extern QColor profileVasiAboveColor;
extern QColor profileVasiBelowColor;
extern QColor profileAltRestrictionFill;
extern QColor profileAltRestrictionOutline;
extern QPen profileVasiCenterPen;
extern QPen ilsCenterPen;
extern QPen glsCenterPen;
extern QPen profileLandOutlinePen;
extern QPen profileWaypointLinePen;
extern QPen profileSafeAltLinePen;
extern QPen profileSafeAltLegLinePen;
extern QPen profileElevationScalePen;

extern QColor distanceMarkerTextColor;
extern QColor distanceMarkerTextBackgroundColor;

/* Objects highlighted because of selection in search */
extern QColor highlightBackColor;

/* Objects highlighted because of selection in route table */
extern QColor routeHighlightBackColor;

/* Objects highlighted because of selection in route profile */
extern QColor profileHighlightBackColor;

/* Endurance and "green banana" */
extern QPen markEndurancePen;
extern QPen markSelectedAltitudeRangePen;
extern QPen markTurnPathPen;

/* Map print colors */
extern QColor mapPrintRowColor;
extern QColor mapPrintRowColorAlt;
extern QColor mapPrintHeaderColor;

/* Marks, crosses and other colors */
extern QPen searchCenterBackPen;
extern QPen searchCenterFillPen;
extern QPen touchMarkBackPen;
extern QPen touchMarkFillPen;
extern QColor touchRegionFillColor;

extern QColor aircraftUserLabelColor;
extern QColor aircraftUserLabelColorBg;

extern QColor aircraftAiLabelColor;
extern QColor aircraftAiLabelColorBg;

// ==========================================================================

/* Web page table background colors */
const QColor webTableBackgroundColor("#f3f3f3");
const QColor webTableAltBackgroundColor("#eceae8");

extern QPen taxiwayLinePen;
extern QColor taxiwayNameColor;
extern QColor taxiwayNameBackgroundColor;

const QBrush taxiwayClosedBrush = QBrush(QColor(255, 255, 0), Qt::BDiagPattern);

const QColor runwayOutlineColor = QColor(Qt::black);
const QColor runwayOffsetColor = QColor(Qt::white);
const QColor runwayOffsetColorDark = QColor(Qt::black);
const QBrush runwayBlastpadBrush = QBrush(Qt::yellow, Qt::DiagCrossPattern);
const QBrush runwayOverrunBrush = QBrush(QColor(180, 180, 0), Qt::DiagCrossPattern);

const QColor helipadOutlineColor = QColor(Qt::black);
const QColor helipadMedicalOutlineColor = QColor(200, 0, 0);
const QColor activeTowerColor = QColor(Qt::red);
const QColor activeTowerOutlineColor = QColor(Qt::black);
const QColor inactiveTowerColor = QColor(Qt::lightGray);
const QColor inactiveTowerOutlineColor = QColor(Qt::darkGray);

const QColor parkingUnknownOutlineColor = QColor(40, 40, 40);
const QColor parkingOutlineColor = QColor(80, 80, 80);

const QColor darkParkingTextColor = QColor(Qt::black);
const QColor brightParkingTextColor = QColor(235, 235, 235);

const QColor towerTextColor = QColor(Qt::black);
const QColor runwayDimsTextColor = QColor(Qt::black);
const QColor runwayTextBackgroundColor = QColor(255, 255, 255, 170);

/* Text background color */
const QColor textBoxColor = QColor(Qt::white);

/* Text background color for flight plan waypoints */
const QColor routeTextBoxColor = QColor(255, 255, 150);
const QColor logTextBoxColor = QColor(150, 240, 255);

const QColor airportSymbolFillColor = QColor(Qt::white);

const QPen aircraftBackPen = QPen(Qt::black, 7, Qt::SolidLine, Qt::RoundCap);
const QPen aircraftFillPen = QPen(Qt::yellow, 4, Qt::SolidLine, Qt::RoundCap);
const QPen aircraftGroundBackPen = QPen(Qt::darkGray, 7, Qt::SolidLine, Qt::RoundCap);
const QPen aircraftGroundFillPen = QPen(Qt::yellow, 4, Qt::SolidLine, Qt::RoundCap);

/* Procedure preview line colors */
const QColor highlightProcedureColor = QColor(80, 80, 255);

/* Use rotating colors for procedure multi preview */
const QVector<QColor> highlightProcedureColorTable(
  {QColor(Qt::black), QColor(Qt::red), QColor(Qt::green), QColor(Qt::blue), QColor(Qt::cyan), QColor(Qt::magenta),
   QColor(Qt::yellow), QColor(Qt::darkRed), QColor(Qt::darkGreen), QColor(Qt::darkBlue), QColor(Qt::darkCyan), QColor(Qt::darkMagenta),
   QColor(Qt::darkYellow) /*, QColor(Qt::white)*/, QColor(Qt::darkGray), QColor(Qt::gray) /*, QColor(Qt::lightGray)*/});

/* Flight plan or userpoint cross line colors */
const QColor mapDragColor = QColor(Qt::darkYellow);

/* Flight plan line colors */
const QColor routeLogEntryColor = QColor(50, 100, 255);
const QColor routeLogEntryOutlineColor = QColor(Qt::black);

/* Procedure preview default */
const QColor routeProcedurePreviewColor = QColor(0, 120, 230);
const QColor routeProcedurePreviewMissedColor = QColor(0, 120, 230);

/* Text along route and approach segments */
extern QColor routeTextColor;
extern QColor routeTextColorGray;
extern QColor routeTextBackgroundColor;
extern QColor routeProcedureMissedTextColor;
extern QColor routeProcedureTextColor;
extern QColor routeProcedurePointColor;
extern QColor routeProcedurePointFlyoverColor;
extern QColor routeUserPointColor;
extern QColor routeInvalidPointColor;

/* Procedure colors */
extern QColor routeProcedureMissedTableColor;
extern QColor routeProcedureMissedTableColorDark;
extern QColor routeProcedureTableColor;
extern QColor routeProcedureTableColorDark;

/* Alternate airport leg colors */
extern QColor routeAlternateTableColor;
extern QColor routeAlternateTableColorDark;
extern QColor routeInvalidTableColor;
extern QColor routeInvalidTableColorDark;
extern QColor nextWaypointColor;
extern QColor nextWaypointColorDark;

extern QColor textBoxColorAirspace;

/* Get an icon for the start type (RUNWAY, HELIPAD or WATER) */
const QIcon& iconForStart(const map::MapStart& start);

/* General icon for parking (RAMP_MIL_*, GATE_*, RAMP_GA_* or RAMP_CARGO */
const QIcon& iconForParkingType(const QString& type);

/* Color for runway, apron and taxiway surface */
const QColor& colorForSurface(const QString& surface);

/* Color for detailed parking symbol in airport diagram (RAMP_MIL_*, GATE_*, RAMP_GA_* or RAMP_CARGO */
const QColor& colorForParkingType(const QString& type);
const QColor& colorTextForParkingType(const QString& type);
const QColor& colorOutlineForParkingType(const QString& type);

/* Color for airport symbol */
const QColor& colorForAirport(const map::MapAirport& ap);

/* Alternating row background color for search tables */
const QColor& alternatingRowColor(int row, bool isSort);

/* Aircraft trail either by set style or gradient for given altitudes */
const QPen aircraftTrailPen(float size, float minAlt = 0.f, float maxAlt = 0.f, float alt = 0.f);

/* Aircraft trail for elevation profile. Adheres to style settgings and not gradient. */
const QPen aircraftTrailPenProfile(float size);

/* Outline for gradient aircraft trail */
const QPen aircraftTrailPenOuter(float size);

/* lineThickness = 20 to 300 and default 100 equal to a scale factor of 0.2 to 3.0 to default line width. */
QPen penForAirspace(const map::MapAirspace& airspace, int lineThickness);

/* Lower values make the airspace more opaque and higher values more transparent. Default is 80. */
QColor colorForAirspaceFill(const map::MapAirspace& airspace, int transparency);

/* Color for airway or track */
const QColor& colorForAirwayOrTrack(const map::MapAirway& airway);

/* Convert current pen into dotted pen leaving style and color as is */
void adjustPenForCircleToLand(QPainter *painter);
void adjustPenForVectors(QPainter *painter);
void adjustPenForAlternate(QPainter *painter);
void adjustPenForManual(QPainter *painter);

/* Value 0.0 (transparent) to 1.0 (opaque) */
QColor adjustAlphaF(QColor color, float alpha);
QPen adjustAlphaF(QPen pen, float alpha);

QPen adjustWidth(QPen pen, float width);

/* Scale current font in painter. Uses defaultFont as a base otherwise current font in painter. */
void scaleFont(QPainter *painter, float scale, const QFont *defaultFont = nullptr);

void darkenPainterRect(QPainter& painter);

} // namespace mapcolors

#endif // LITTLENAVMAP_MAPCOLORS_H
