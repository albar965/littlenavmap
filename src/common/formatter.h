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

#ifndef LITTLENAVMAP_FORMATTER_H
#define LITTLENAVMAP_FORMATTER_H

#include <QString>

class QElapsedTimer;

namespace formatter {

/* Checks if the lat long coordinate string is valid and returns an error message or a message for validity checking*/
bool checkCoordinates(QString& message, const QString& text);

/* Capitalize the string using exceptions for any aviation acronyms */
QString capNavString(const QString& str);

/* All formatters are locale aware */

/* Format time_t to short locale dependent date string */
QString formatDate(int timeT);

/* Format time_t to long locale dependent date string */
QString formatDateLong(int timeT);

/* Format a decimal time in hours to h:mm format */
QString formatMinutesHours(double time);

/* Format a decimal time in hours to X h Y m format */
QString formatMinutesHoursLong(double time);

/* Format a decimal time in hours to d:hh:mm format */
QString formatMinutesHoursDays(double time);

/* Format a decimal time in hours to X d Y h Z m format */
QString formatMinutesHoursDaysLong(double time);

/* Format a value to a x:xx nm string where nm is a unit */
QString formatDoubleUnit(double value, const QString& unit = QString(), int precision = 0);
QString formatFloatUnit(float value, const QString& unit = QString(), int precision = 0);

/* Format elapsed time to minutes and seconds */
QString formatElapsed(const QElapsedTimer& timer);

} // namespace formatter

#endif // LITTLENAVMAP_FORMATTER_H
