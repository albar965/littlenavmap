/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef MAPCOLORS_H
#define MAPCOLORS_H

#include <QColor>
#include <QPen>
#include <QStyle>

namespace maptypes {
class MapAirport;
}

namespace mapcolors {

extern const QPen textBackgroundPen;
extern const QPen textPen;
extern const QColor airportDetailBackColor;
extern const QColor taxiwayNameColor;
extern const QColor runwayOutlineColor;
extern const QColor runwayOffsetColor;
extern const QColor parkingOutlineColor;
extern const QColor helipadOutlineColor;
extern const QColor activeTowerColor;
extern const QColor activeTowerOutlineColor;
extern const QColor inactiveTowerColor;
extern const QColor inactiveTowerOutlineColor;
extern const QColor darkParkingTextColor;
extern const QColor brightParkingTextColor;
extern const QColor towerTextColor;
extern const QColor runwayDimsTextColor;
extern const QColor textBoxColor;
extern const QColor airportSymbolFillColor;
extern const QPen markBackPen;
extern const QPen markFillPen;
extern const QPen homeBackPen;
extern const QColor homeFillColor;
extern const QColor highlightColorFast;
extern const QColor highlightColor;
extern const QColor highlightBackColor;

extern const QColor rangeRingColor;
extern const QColor rangeRingTextColor;

extern const QColor vorSymbolColor;
extern const QColor ndbSymbolColor;
extern const QColor waypointSymbolColor;
extern const QColor markerSymbolColor;
extern const QColor ilsSymbolColor;

const QColor& colorForSurface(const QString& surface);
const QColor& colorForParkingType(const QString& type);
const QColor& colorForAirport(const maptypes::MapAirport& ap);
const QColor& alternatingRowColor(int row, bool isSort, QStyle::State state = QStyle::State_None);

} // namespace mapcolors

#endif // MAPCOLORS_H
