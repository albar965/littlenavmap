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

const QPen textBackgroundPen = QPen(QBrush(QColor(Qt::lightGray)), 1, Qt::SolidLine, Qt::FlatCap);
const QPen textPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 1, Qt::SolidLine, Qt::FlatCap);

const QColor airportDetailBackColor = QColor(Qt::white);

const QColor taxiwayNameColor = QColor(Qt::black);
const QColor runwayOutlineColor = QColor(Qt::black);
const QColor runwayOffsetColor = QColor(Qt::white);
const QColor parkingOutlineColor = QColor::fromRgb(80, 80, 80);
const QColor helipadOutlineColor = QColor(Qt::black);
const QColor activeTowerColor = QColor(Qt::red);
const QColor activeTowerOutlineColor = QColor(Qt::black);
const QColor inactiveTowerColor = QColor(Qt::lightGray);
const QColor inactiveTowerOutlineColor = QColor(Qt::darkGray);
const QColor darkParkingTextColor = QColor(Qt::black);
const QColor brightParkingTextColor = QColor(Qt::white);
const QColor towerTextColor = QColor(Qt::black);
const QColor runwayDimsTextColor = QColor(Qt::black);

const QColor textBoxColor = QColor(Qt::white);
const QColor routeTextBoxColor = QColor::fromRgb(255, 255, 150);

const QColor airportSymbolFillColor = QColor(Qt::white);

const QPen markBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);
const QPen markFillPen = QPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);

const QPen homeBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 2, Qt::SolidLine, Qt::FlatCap);
const QColor homeFillColor = QColor(Qt::yellow);

const QColor highlightBackColor = QColor(Qt::black);
const QColor highlightColor = QColor(Qt::yellow);
const QColor highlightColorFast = QColor(Qt::darkYellow);

const QColor routeHighlightBackColor = QColor(Qt::black);
const QColor routeHighlightColor = QColor(Qt::green);
const QColor routeHighlightColorFast = QColor(Qt::darkGreen);

const QColor routeUserPointColor = QColor(Qt::darkYellow);
const QColor routeInvalidPointColor = QColor(Qt::red);

const QColor rangeRingColor = QColor(Qt::red);
const QColor rangeRingTextColor = QColor(Qt::black);

const QColor distanceColor = QColor(Qt::black);
const QColor distanceRhumbColor = QColor::fromRgb(80, 80, 80);

const QColor vorSymbolColor = QColor(Qt::darkBlue);
const QColor ndbSymbolColor = QColor(Qt::darkRed);
const QColor markerSymbolColor = QColor(Qt::darkMagenta);
const QColor ilsSymbolColor = QColor(Qt::darkGreen);
const QColor ilsTextColor = QColor::fromRgb(0, 70, 0);
const QColor waypointSymbolColor = QColor(Qt::magenta);

const QColor& colorForSurface(const QString& surface);
const QColor& colorForParkingType(const QString& type);
const QColor& colorForAirport(const maptypes::MapAirport& ap);
const QColor& alternatingRowColor(int row, bool isSort);

} // namespace mapcolors

#endif // MAPCOLORS_H
