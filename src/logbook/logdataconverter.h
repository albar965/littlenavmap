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

#ifndef LNM_LOGDATACONVERTER_H
#define LNM_LOGDATACONVERTER_H

#include <QApplication>

namespace atools {
namespace fs {
namespace userdata {
class LogdataManager;
}
}
namespace sql {
class SqlDatabase;
class SqlRecord;
class SqlQuery;
}
}

class AirportQuery;
class QDateTime;

/*
 * Converts userdata entries of type "Logbook" to real logbook entries for the new database.
 * It tries to decode numbers, date and time in various formats and languages
 */
class LogdataConverter
{
  Q_DECLARE_TR_FUNCTIONS(LogdataConverter)

public:
  LogdataConverter(atools::sql::SqlDatabase *userDbParam, atools::fs::userdata::LogdataManager *logManagerParam,
                   AirportQuery *airportQueryParam);
  ~LogdataConverter();

  /* Convert data from one database to another, parse the description fiels as good as possible
   * and store new entries in table "logbook" */
  int convertFromUserdata();

  /* Get a list of collected errors not in HTML. */
  const QStringList& getErrors() const
  {
    return errors;
  }

private:
  /* Extract data from description field */
  void convertFromDescription(atools::sql::SqlQuery& queryFrom, atools::sql::SqlRecord& recTo, bool departure);

  /* Set value in a record if not empty */
  void set(atools::sql::SqlRecord& recTo, const QString& name, const QString& str);
  void set(atools::sql::SqlRecord& recTo, const QString& name, float value);
  void set(atools::sql::SqlRecord& recTo, const QString& name, QDateTime value);

  /* Update a value in a record if not empty */
  void update(atools::sql::SqlRecord& recTo, const QString& name, const QString& str);
  void update(atools::sql::SqlRecord& recTo, const QString& name, float value);
  void update(atools::sql::SqlRecord& recTo, const QString& name, QDateTime value);

  /* Find values after the either translated or not translated names.
   * Parses numeric and datetime values with either system or English locale */
  QString find(const QStringList& lines, QString nameTr, QString name, bool nextLine = false);
  float findFloat(const QStringList& lines, QString nameTr, QString name, QString& unit);
  float findFt(const QStringList& lines, QString nameTr, QString name);
  float findNm(const QStringList& lines, QString nameTr, QString name);
  QDateTime findDateTime(const QStringList& lines, QString nameTr, QString name);

  /* Collected errors while reading */
  QStringList errors;

  /* Source database */
  atools::sql::SqlDatabase *userDb;

  /* Log manager for destination */
  atools::fs::userdata::LogdataManager *logManager;

  /* Needed to resolve airports */
  AirportQuery *airportQuery;

  /* Current id for error message */
  QString message;

  /* System and English locale */
  QLocale locale, localeEn;
};

#endif // LNM_LOGDATACONVERTER_H
