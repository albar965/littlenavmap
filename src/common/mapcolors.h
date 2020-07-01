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

#ifndef LITTLENAVMAP_MAPCOLORS_H
#define LITTLENAVMAP_MAPCOLORS_H

#include <QColor>
#include <QPen>
#include <QStyle>

namespace map {
struct MapAirport;

struct MapAirspace;
struct MapAirway;

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
extern QColor vorSymbolColor;
extern QColor ndbSymbolColor;
extern QColor markerSymbolColor;
extern QColor ilsSymbolColor;
extern QColor ilsFillColor;
extern QColor ilsTextColor;
extern QColor waypointSymbolColor;
extern QColor airwayVictorColor;
extern QColor airwayJetColor;
extern QColor airwayBothColor;
extern QColor airwayTrackColor;
extern QColor airwayTextColor;
extern QColor rangeRingColor;
extern QColor rangeRingTextColor;
extern QColor compassRoseColor;
extern QColor compassRoseTextColor;
extern QColor distanceColor;

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
extern QPen profileLandOutlinePen;
extern QPen profileWaypointLinePen;
extern QPen profileElevationScalePen;
extern QPen profileSafeAltLinePen;
extern QPen profileSafeAltLegLinePen;

/* Objects highlighted because of selection in search */
extern QColor highlightBackColor;
extern QColor highlightColor;
extern QColor highlightColorFast;

/* Objects highlighted because of selection in route table */
extern QColor routeHighlightBackColor;
extern QColor routeHighlightColor;
extern QColor routeHighlightColorFast;

/* Objects highlighted because of selection in route profile */
extern QColor profileHighlightBackColor;
extern QColor profileHighlightColor;
extern QColor profileHighlightColorFast;

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

const QColor highlightApproachColor = QColor(150, 150, 255);
const QColor highlightApproachColorFast = QColor(0, 0, 150);

/* Flight plan or userpoint cross line colors */
const QColor mapDragColor = QColor(Qt::darkYellow);

/* Flight plan line colors */
const QColor routeOutlineColor = QColor(Qt::black);
const QColor routeAlternateOutlineColor = QColor(Qt::darkGray);

const QColor routeProcedureOutlineColor = QColor(Qt::black);

const QColor routeLogEntryColor = QColor(50, 100, 255);
const QColor routeLogEntryOutlineColor = QColor(Qt::black);

const QColor routeProcedurePreviewColor = QColor(0, 180, 255);
const QColor routeProcedurePreviewMissedColor = QColor(0, 180, 255);

/* Text along route and approach segments */
const QColor routeTextColor = QColor(0, 0, 0);
const QColor routeTextBackgroundColor = QColor(255, 255, 255, 180);

const QColor routeProcedureMissedTextColor = QColor(90, 90, 90);
const QColor routeProcedureTextColor = QColor(0, 0, 0);

const QColor routeProcedurePointColor = QColor(90, 90, 90);
const QColor routeProcedurePointFlyoverColor = QColor(Qt::black);
const QColor routeUserPointColor = QColor(Qt::darkYellow);
/* Point not found in database */
const QColor routeInvalidPointColor = QColor(Qt::red);

/* Procedure colors */
const QColor routeProcedureMissedTableColor = QColor(Qt::darkRed);
const QColor routeProcedureMissedTableColorDark = QColor(240, 170, 120);
const QColor routeProcedureTableColor = QColor(Qt::darkBlue);
const QColor routeProcedureTableColorDark = QColor(140, 200, 240);

/* Alternate airport leg colors */
const QColor routeAlternateTableColor = QColor(Qt::darkGray);
const QColor routeAlternateTableColorDark = QColor(Qt::gray);

const QColor routeInvalidTableColor = QColor(Qt::red);
const QColor routeInvalidTableColorDark = QColor(Qt::red);

const QColor nextWaypointColor(QColor(255, 100, 255));
const QColor nextWaypointColorDark(QColor(150, 20, 150));

/* Get an icon for the start type (RUNWAY, HELIPAD or WATER) */
const QIcon& iconForStartType(const QString& type);

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

const QPen aircraftTrailPen(float size);

const QPen& penForAirspace(const map::MapAirspace& airspace);
const QColor& colorForAirspaceFill(const map::MapAirspace& airspace);

const QColor& colorForAirwayTrack(const map::MapAirway& airway);

/* Convert current pen into dotted pen leaving style and color as is */
void adjustPenForCircleToLand(QPainter *painter);
void adjustPenForVectors(QPainter *painter);
void adjustPenForAlternate(QPainter *painter);
void adjustPenForManual(QPainter *painter);

/* Scale current font in painter. Uses defaultFont as a base otherwise current font in painter. */
void scaleFont(QPainter *painter, float scale, const QFont *defaultFont = nullptr);

void darkenPainterRect(QPainter& painter);

} // namespace mapcolors

#endif // LITTLENAVMAP_MAPCOLORS_H
