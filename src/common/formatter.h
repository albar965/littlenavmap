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

#ifndef LITTLENAVMAP_FORMATTER_H
#define LITTLENAVMAP_FORMATTER_H

#include <QString>

namespace atools {
namespace geo {
class Pos;
}
}

class QDateTime;

class QElapsedTimer;

namespace formatter {

void initTranslateableTexts();

/* try to read a date time string using local and English locale and yyyy/yy variants */
QDateTime readDateTime(QString str);

/* Checks if the lat long coordinate string is valid and returns an error message or a message for validity checking
 * Position is returned in pos if not null. */
bool checkCoordinates(QString& message, const QString& text, atools::geo::Pos *pos = nullptr);

/* Capitalize the string using exceptions for any aviation acronyms */
QString capNavString(const QString& str);

/* All formatters are locale aware */

/* Format a decimal time in hours to h:mm format */
QString formatMinutesHours(double timeHours);

/* Format a decimal time in hours to X h Y m format */
QString formatMinutesHoursLong(double timeHours);

/* Format a decimal time in hours to d:hh:mm format */
QString formatMinutesHoursDays(double timeHours);

/* Format a decimal time in hours to X d Y h Z m format */
QString formatMinutesHoursDaysLong(double timeHours);

/* Format elapsed time to minutes and seconds */
QString formatElapsed(const QElapsedTimer& timer);

/* Locale dependent date time with seconds. Override results in "10/26/23 8:21:00 PM" */
QString formatDateTimeSeconds(const QDateTime& datetime, bool overrideLocale);

/* Format wind as string with pointer. Tail head and crosswind. */
QString windInformation(float headWindKts, float crossWindKts, const QString& separator, bool addUnit = true);

/* Crosswind > 1 kts */
QString windInformationCross(float crossWindKts, bool addUnit = true);

/* Tail and headwind > 1 kts */
QString windInformationTailHead(float headWindKts, bool addUnit = true);

/* Only headwind > 1 kts and crosswind */
QString windInformationShort(int windDirectionDeg, float windSpeedKts, float runwayEndHeading);

/* Get course or heading text with magnetic and/or true course depending on settings */
QString courseText(float magCourse, float trueCourse, bool magBold = false, bool magBig = false, bool trueSmall = true, bool narrow = false,
                   bool forceBoth = false);
QString courseSuffix();
QString courseTextFromMag(float magCourse, float magvar, bool magBold = false, bool magBig = false, bool trueSmall = true,
                          bool narrow = false, bool forceBoth = false);
QString courseTextFromTrue(float trueCourse, float magvar, bool magBold = false, bool magBig = false, bool trueSmall = true,
                           bool narrow = false, bool forceBoth = false);
QString courseTextNarrow(float magCourse, float trueCourse);

} // namespace formatter

#endif // LITTLENAVMAP_FORMATTER_H
