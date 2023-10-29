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

#include "common/formatter.h"

#include "fs/util/fsutil.h"

#include "atools.h"
#include "common/mapflags.h"
#include "fs/util/coordinates.h"
#include "geo/calculations.h"
#include "geo/pos.h"
#include "unit.h"
#include "util/htmlbuilder.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QLocale>
#include <QRegularExpression>
#include <QStringBuilder>

namespace formatter {

static QStringList dateTimeFormats;

QString formatMinutesHours(double timeHours)
{
  int hours = static_cast<int>(timeHours);
  int minutes = atools::roundToInt((timeHours - hours) * 60.);
  if(minutes == 60)
  {
    hours++;
    minutes = 0;
  }
  return QObject::tr("%L1:%L2").arg(hours).arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursLong(double timeHours)
{
  int hours = static_cast<int>(timeHours);
  int minutes = atools::roundToInt((timeHours - hours) * 60);
  if(minutes == 60)
  {
    hours++;
    minutes = 0;
  }

  return QObject::tr("%L1 h %L2 m").arg(hours).arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursDays(double timeHours)
{
  int days = static_cast<int>(timeHours) / 24;
  int hours = static_cast<int>(timeHours) - (days * 24);
  int minutes = atools::roundToInt((timeHours - std::floor(timeHours)) * 60.);
  return QObject::tr("%L1:%L2:%L3").arg(days).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0'));
}

QString formatMinutesHoursDaysLong(double timeHours)
{
  int days = static_cast<int>(timeHours) / 24;
  int hours = static_cast<int>(timeHours) - (days * 24);
  int minutes = atools::roundToInt((timeHours - std::floor(timeHours)) * 60.);
  QString retval;
  if(days > 0)
    retval += QObject::tr("%L1 d").arg(days);

  if(hours > 0)
  {
    if(!retval.isEmpty())
      retval += QObject::tr(" %L1 h").arg(hours, 2, 10, QChar('0'));
    else
      retval += QObject::tr("%L1 h").arg(hours);
  }

  if(!retval.isEmpty())
    retval += QObject::tr(" %L1 m").arg(minutes, 2, 10, QChar('0'));
  else
    retval += QObject::tr("%L1 m").arg(minutes);

  return retval;
}

QString formatElapsed(const QElapsedTimer& timer)
{
  int secs = static_cast<int>(timer.elapsed() / 1000L);
  if(secs < 60)
    return QObject::tr("%L1 %2").arg(secs).arg(secs == 1 ? QObject::tr("second") : QObject::tr("seconds"));
  else
  {
    int mins = secs / 60;
    secs = secs - mins * 60;

    return QObject::tr("%L1 %2 %L3 %4").
           arg(mins).arg(mins == 1 ? QObject::tr("minute") : QObject::tr("minutes")).
           arg(secs).arg(secs == 1 ? QObject::tr("second") : QObject::tr("seconds"));
  }
}

QString capNavString(const QString& str)
{
  return atools::fs::util::capNavString(str);
}

bool checkCoordinates(QString& message, const QString& text, atools::geo::Pos *pos)
{
  bool hemisphere = false;
  atools::geo::Pos readPos = atools::fs::util::fromAnyFormat(text, &hemisphere);

  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
    // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
    atools::fs::util::maybeSwapOrdinates(readPos, text);

  if(pos != nullptr)
    *pos = readPos;

  if(readPos.isValidRange())
  {
    QString coords = Unit::coords(readPos);
    if(coords.simplified() != text.simplified())
    {
      if(Unit::getUnitCoords() == opts::COORDS_LATY_LONX || Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
        message = QObject::tr("Coordinates are valid: %1 (%2)").arg(coords).arg(Unit::coords(readPos, opts::COORDS_DMS));
      else
        message = QObject::tr("Coordinates are valid: %1").arg(coords);
    }
    else
      // Same as in line edit. No need to show again
      message = QObject::tr("Coordinates are valid.");
    return true;
  }
  else
    // Show red warning
    message = atools::util::HtmlBuilder::errorMessage(QObject::tr("Coordinates are not valid."));
  return false;
}

QString yearVariant(QString dateTimeFormat)
{
  const static QRegularExpression YEAR_REGEXP("\\byy\\b");
  if(dateTimeFormat.contains("yyyy"))
    return dateTimeFormat.replace("yyyy", "yy");
  else if(dateTimeFormat.contains(YEAR_REGEXP))
    return dateTimeFormat.replace("yy", "yyyy");

  return dateTimeFormat;
}

void initTranslateableTexts()
{
  // Create a list of date time formats for system and English locales with all formats,
  // long short year variants and with time zone or not

  // System locale ==================================
  // This is independent from locale overridden in the options dialog
  QLocale locale;
  dateTimeFormats.clear();
  dateTimeFormats.append(locale.dateTimeFormat(QLocale::ShortFormat));
  dateTimeFormats.append(locale.dateTimeFormat(QLocale::LongFormat));
  dateTimeFormats.append(locale.dateTimeFormat(QLocale::NarrowFormat));

  // Replace yyyy with yy and vice versa
  dateTimeFormats.append(yearVariant(locale.dateTimeFormat(QLocale::ShortFormat)));
  dateTimeFormats.append(yearVariant(locale.dateTimeFormat(QLocale::LongFormat)));
  dateTimeFormats.append(yearVariant(locale.dateTimeFormat(QLocale::NarrowFormat)));

  // English locale ==================================
  QLocale localeEn = QLocale::English;
  dateTimeFormats.append(localeEn.dateTimeFormat(QLocale::ShortFormat));
  dateTimeFormats.append(localeEn.dateTimeFormat(QLocale::LongFormat));
  dateTimeFormats.append(localeEn.dateTimeFormat(QLocale::NarrowFormat));

  // Replace yyyy with yy and vice versa
  dateTimeFormats.append(yearVariant(localeEn.dateTimeFormat(QLocale::ShortFormat)));
  dateTimeFormats.append(yearVariant(localeEn.dateTimeFormat(QLocale::LongFormat)));
  dateTimeFormats.append(yearVariant(localeEn.dateTimeFormat(QLocale::NarrowFormat)));

  // Add variants with time zone ===========================
  const QStringList temp(dateTimeFormats);
  for(const QString& t : temp)
  {
    if(!t.endsWith("t"))
    {
      dateTimeFormats.append(t + " t");
      dateTimeFormats.append(t + "t");
    }
  }
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << dateTimeFormats;
#endif
}

QDateTime readDateTime(QString str)
{
  QDateTime retval;

  // This is independent from locale overridden in the options dialog
  QLocale locale;

  // if(str.endsWith("UTC"))
  // str.chop(3);

  str = str.simplified();

  for(const QString& format : qAsConst(dateTimeFormats))
  {
    retval = locale.toDateTime(str, format);
    if(retval.isValid())
      break;
  }
  return retval;
}

QString windInformationTailHead(float headWindKts, bool addUnit)
{
  QString windPtr;
  if(std::abs(headWindKts) >= 1.0f)
  {
    windPtr += Unit::speedKts(std::abs(headWindKts), addUnit);

    if(headWindKts <= -1.f)
      windPtr += QObject::tr(" ▲"); // Tailwind
    else
      windPtr += QObject::tr(" ▼"); // Headwind
  }
  return windPtr;
}

QString windInformationCross(float crossWindKts, bool addUnit)
{
  QString windPtr;
  if(std::abs(crossWindKts) >= 0.5f) // Rounded up to 1
  {
    windPtr += Unit::speedKts(std::abs(crossWindKts), addUnit);

    if(crossWindKts > 0.f)
      windPtr += QObject::tr(" ◄");
    else if(crossWindKts < 0.f)
      windPtr += QObject::tr(" ►");
  }
  return windPtr;
}

QString windInformation(float headWindKts, float crossWindKts, const QString& separator, bool addUnit)
{
  QStringList windTxt;
  windTxt.append(windInformationTailHead(headWindKts, addUnit));
  windTxt.append(windInformationCross(crossWindKts, addUnit));
  windTxt.removeAll(QString());
  return windTxt.join(separator);
}

QString windInformationShort(int windDirectionDeg, float windSpeedKts, float runwayEndHeading)
{
  QString windStr;
  if(windDirectionDeg != -1 && windSpeedKts >= 1.f && windSpeedKts < map::INVALID_METAR_VALUE)
  {
    float headWindKts, crossWindKts;
    atools::geo::windForCourse(headWindKts, crossWindKts, windSpeedKts, windDirectionDeg, runwayEndHeading);

    // Only show for head wind > 1
    if(headWindKts >= 1.f)
      windStr = formatter::windInformation(headWindKts, crossWindKts, QObject::tr(" "), false /* addUnit */);
  }
  return windStr;
}

QString courseTextFromTrue(float trueCourse, float magvar, bool magBold, bool magBig, bool trueSmall, bool narrow, bool forceBoth)
{
  // true to magnetic
  return courseText(atools::geo::normalizeCourse(trueCourse - magvar), trueCourse, magBold, magBig, trueSmall, narrow, forceBoth);
}

QString courseTextFromMag(float magCourse, float magvar, bool magBold, bool magBig, bool trueSmall, bool narrow, bool forceBoth)
{
  // magnetic to true
  return courseText(magCourse, atools::geo::normalizeCourse(magCourse + magvar), magBold, magBig, trueSmall, narrow, forceBoth);
}

QString courseSuffix()
{
  return OptionData::instance().getFlags2().testFlag(opts2::UNIT_TRUE_COURSE) ? QObject::tr("°M/T") : QObject::tr("°M");
}

QString courseText(float magCourse, float trueCourse, bool magBold, bool magBig, bool trueSmall, bool narrow, bool forceBoth)
{
  QString magStr, trueStr;
  if(magCourse < map::INVALID_COURSE_VALUE / 2.f)
    magStr = QLocale().toString(magCourse, 'f', 0);

  if(forceBoth || OptionData::instance().getFlags2().testFlag(opts2::UNIT_TRUE_COURSE) || magStr.isEmpty())
  {
    if(trueCourse < map::INVALID_COURSE_VALUE / 2.f)
      trueStr = QLocale().toString(trueCourse, 'f', 0);
  }

  // Formatting for magnetic course
  QString style, styleEnd;
  if(magBold)
    style.append("<b>");
  if(magBig)
    style.append("<big>");

  if(magBig)
    styleEnd.append("</big>");
  if(magBold)
    styleEnd.append("</b>");

  if(atools::almostEqual(magCourse, trueCourse, 1.5f))
  {
    if(magStr.isEmpty())
      return QString();

    // Values are close - display only magnetic
    return QObject::tr("%1%2°M%3").arg(style).arg(magStr).arg(styleEnd);
  }
  else
  {
    if(!magStr.isEmpty() && !trueStr.isEmpty())
    {
      // Formatting for true course
      QLatin1String small = trueSmall ? QLatin1String("<span style=\"font-size: small;\">") : QLatin1String();
      QLatin1String smallEnd = trueSmall ? QLatin1String("</span>") : QLatin1String();

      // Values differ and both are valid - display magnetic and true
      return QObject::tr("%1%2°M%3,%4%5%6°T%7").
             arg(style).arg(magStr).arg(styleEnd).
             arg(narrow ? QString() : QObject::tr(" ", "Separator for mag/true course text")).
             arg(small).arg(trueStr).arg(smallEnd);
    }
    else if(!magStr.isEmpty())
      // Only mag value is valid
      return QObject::tr("%1%2°M%3").arg(style).arg(magStr).arg(styleEnd);
    else if(!trueStr.isEmpty())
      // Only true value is valid
      return QObject::tr("%1%2°T%3").arg(style).arg(trueStr).arg(styleEnd);
  }
  return QString();
}

QString courseTextNarrow(float magCourse, float trueCourse)
{
  QString initTrueText, initMagText;

  if(trueCourse < map::INVALID_COURSE_VALUE)
    initTrueText = QString::number(trueCourse, 'f', 0);

  if(magCourse < map::INVALID_COURSE_VALUE)
    initMagText = QString::number(magCourse, 'f', 0);

  QString initText;
  if(!initTrueText.isEmpty() && !initMagText.isEmpty())
  {
    if(initTrueText == initMagText)
      initText = QObject::tr("%1°M/T").arg(initMagText);
    else
      initText = QObject::tr("%1°M %2°T").arg(initMagText).arg(initTrueText);
  }
  else if(!initMagText.isEmpty())
    initText = QObject::tr("%1°M").arg(initMagText);
  else if(!initTrueText.isEmpty())
    initText = QObject::tr("%1°T").arg(initTrueText);

  return initText;
}

QString formatDateTimeSeconds(const QDateTime& datetime, bool overrideLocale)
{
  QString dateTimeStr = overrideLocale ?
                        "MM/dd/yy h:mm:ss AP" :
                        QObject::tr("dd.MM.yy hh:mm:ss", "Translate to short date and time format in your language with seconds."
                                                         "See https://doc.qt.io/qt-5/qtime.html#toString and "
                                                         "https://doc.qt.io/qt-5/qdate.html#toString-2 "
                                                         "Look at your operating system settings to find suitable format");
  return QLocale().toString(datetime, dateTimeStr);
}

} // namespace formatter
