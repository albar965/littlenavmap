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

#include "logbook/logdataconverter.h"

#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "fs/userdata/logdatamanager.h"
#include "geo/pos.h"
#include "common/formatter.h"
#include "geo/calculations.h"
#include "query/airportquery.h"
#include "sql/sqltransaction.h"
#include "userdata/userdatacontroller.h"
#include "common/unit.h"

#include <QRegularExpression>

using atools::sql::SqlDatabase;
using atools::sql::SqlQuery;
using atools::sql::SqlRecord;
using atools::sql::SqlTransaction;
using atools::geo::Pos;

const float INVALID_FLOAT = std::numeric_limits<float>::max();

LogdataConverter::LogdataConverter(atools::sql::SqlDatabase *userDbParam,
                                   atools::fs::userdata::LogdataManager *logManagerParam,
                                   AirportQuery *airportQueryParam)
  : userDb(userDbParam), logManager(logManagerParam), airportQuery(airportQueryParam)
{
  localeEn = QLocale::English;
  locale = QLocale::system();

  qDebug() << Q_FUNC_INFO;
  qDebug() << "locale" << locale << "localeEn" << localeEn;
  qDebug() << "groupSeparator" << locale.groupSeparator()
           << "decimalPoint" << locale.decimalPoint();

  qDebug() << "EN locale" << localeEn;
  qDebug() << "EN groupSeparator" << localeEn.groupSeparator()
           << "decimalPoint" << localeEn.decimalPoint();
}

LogdataConverter::~LogdataConverter()
{

}

int LogdataConverter::convertFromUserdata()
{
  int numCreated = 0;
  errors.clear();

  enum Type
  {
    NONE, DEPARTURE, DESTINATION
  };

  SqlTransaction transaction(logManager->getDatabase());

  // Source data query - order by ID is safest since it is not messed up by editing
  SqlQuery query("select userdata_id, type, name, ident, region, description, tags, last_edit_timestamp, "
                 "import_file_path, visible_from, altitude, temp, lonx, laty "
                 "from userdata where type = 'Logbook' order by userdata_id", userDb);

  query.exec();
  atools::sql::SqlRecord rec = logManager->getEmptyRecord();
  Type lasttype = NONE;
  while(query.next())
  {
    QString ident = query.valueStr("ident");
    QString name = query.valueStr("name");
    QString region = query.valueStr("region");
    QString tags = query.valueStr("tags");

    // Build error message ================================
#ifdef DEBUG_INFORMATION
    message = tr(
      "<i>Ident &quot;%1&quot;, name &quot;%2&quot;, region &quot;%3&quot;, tags &quot;%4&quot;, userdata_id %5</i>").
              arg(ident).arg(name).arg(region).arg(tags).arg(query.valueInt("userdata_id"));
#else
    message = tr("<i>Ident &quot;%1&quot;, name &quot;%2&quot;, region &quot;%3&quot;, tags &quot;%4&quot;</i>").
              arg(ident).arg(name).arg(region).arg(tags);
#endif

    // Read entry type ================================================
    Type type = NONE;

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    if(tags.contains(UserdataController::tr("Departure"),
                     Qt::CaseInsensitive) || tags.contains("Departure", Qt::CaseInsensitive))
      type = DEPARTURE;

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    if(tags.contains(UserdataController::tr("Arrival"),
                     Qt::CaseInsensitive) || tags.contains("Arrival", Qt::CaseInsensitive))
    {
      if(type == DEPARTURE)
      {
        errors.append(tr("Entry has both departure and destination tag: %1.").arg(message));
        lasttype = NONE;
        continue;
      }
      type = DESTINATION;
    }

    if(type == NONE)
    {
      errors.append(tr("Entry has neither departure nor destination tag: %1.").arg(message));
      lasttype = NONE;
      continue;
    }

    if(type == DEPARTURE && lasttype == DEPARTURE)
    {
      errors.append(tr("Additional departure found: %1.").arg(message));
      lasttype = NONE;
      continue;
    }

    if(type == DESTINATION && lasttype == DESTINATION)
    {
      errors.append(tr("Additional destination found: %1.").arg(message));
      lasttype = NONE;
      continue;
    }

    // Set ident and try to resolve airports in database =========================
    QString prefix = type == DEPARTURE ? "departure_" : "destination_";
    rec.setValue(prefix + "ident", ident);

    map::MapAirport airport = airportQuery->getAirportByIdent(ident);
    if(airport.isValid())
    {
      // Use values found in database ==========================
      rec.setValue(prefix + "name", airport.name);
      rec.setValue(prefix + "lonx", airport.position.getLonX());
      rec.setValue(prefix + "laty", airport.position.getLatY());
      rec.setValue(prefix + "alt", airport.position.getAltitude());
    }
    else
    {
      // Airport not found - use values from source record ==========================
      rec.setValue(prefix + "name", name);
      rec.setValue(prefix + "lonx", query.valueStr("lonx"));
      rec.setValue(prefix + "laty", query.valueStr("laty"));
      rec.setValue(prefix + "alt", query.valueStr("altitude"));
    }

    // Extract values from description =================
    convertFromDescription(query, rec, type == DEPARTURE);

    // Add new description =================
    QString descriptionSrc = query.valueStr("description");
    QString descriptionTo = rec.valueStr("description");
    if(type == DEPARTURE)
      /*: The text "Converted from userdata" has to match the one in LogdataController::convertUserdata */
      rec.setValue("description", tr("Converted from userdata\n\n"
                                     "==== Original departure description:\n") + descriptionSrc);
    else
      rec.setValue("description", descriptionTo + tr("\n\n==== Original arrival description:\n") + descriptionSrc);

    lasttype = type;

    if(type == DESTINATION)
    {
      // Save entry =====================
      if(!rec.isNull("departure_ident"))
      {
        // Fill null fields with empty strings to avoid issue when searching
        atools::fs::userdata::LogdataManager::fixEmptyFields(rec);

        logManager->insertByRecord(rec);
        rec.clearValues();
        type = NONE;
        numCreated++;
      }
    }
  }

  transaction.commit();
  return numCreated;
}

// ==========================================================
// Departure at Airport Brilon-Hochsauerland (EDKO) runway 07
// Simulator Date and Time: 11.04.2018 04:32 UTC, 05:32 UTC+01:00
// Date and Time: Di Apr 17 21:35:01 2018
//
// Flight Plan:
// EDKOEDXH.fms
// From: Airport Brilon-Hochsauerland (EDKO) to Airport Helgoland-Dune (EDXH)
// Cruising altitude: 6.000 ft
//
// Aircraft:
// Title: 1964 PA30 Twin Comanche with Lycoming IO-320 engines
// Model: PA30
// Registration: D-GIRL
// ==========================================================
// Arrival at Airport Helgoland-Dune (EDXH) runway 15
// Simulator Date and Time: 11.04.2018 05:43 UTC, 06:43 UTC+01:00
// Date and Time: Di Apr 17 22:00:24 2018
//
// Flight Plan:
// EDKOEDXH.fms
// From: Airport Brilon-Hochsauerland (EDKO) to Airport Helgoland-Dune (EDXH)
// Cruising altitude: 6.500 ft
//
// Aircraft:
// Title: 1964 PA30 Twin Comanche with Lycoming IO-320 engines
// Model: PA30
// Registration: D-GIRL
//
// Trip:
// Time: 1 h 11 m
// Flight Plan Distance: 170 nm
// Flown Distance: 179 nm
// Average Groundspeed: 151 kts
// Average True Airspeed: 154 kts
// Fuel consumed: 132 lbs, 22 gal
// Average fuel flow: 111 pph, 19 gph
void LogdataConverter::convertFromDescription(atools::sql::SqlQuery& queryFrom, atools::sql::SqlRecord& recTo,
                                              bool departure)
{
  // Read description line by line into a list ======================
  QString description = queryFrom.valueStr("description");
  QStringList lines;
  QTextStream in(&description, QIODevice::ReadOnly);
  while(!in.atEnd())
  {
    QString line = in.readLine().simplified();
    if(!line.isEmpty())
      lines.append(line);
  }

  // Use UserdataController::tr for translating and original texts including placeholders
  // to avoid changed translations and mismatches
  // Leave a comment for translators
  if(departure)
  {
    // Departure - set values into record ==================================
    /* *INDENT-OFF* */
    // Ignore runway for now
    // find(lines, UserdataController::tr(" runway %1"), " runway ");

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "departure_time_sim", findDateTime(lines, UserdataController::tr("Simulator Date and Time: %1 %2, %3 %4"), "Simulator Date and Time: "));

    // set(recTo, "departure_time", findDateTime(lines, UserdataController::tr("Date and Time: %1"), "Date and Time: "));
    // Cannot parse departure time - use last edit time instead
    set(recTo, "departure_time", queryFrom.valueDateTime("last_edit_timestamp"));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "flightplan_file",  find(lines, UserdataController::tr("Flight Plan:"), "Flight Plan:", true));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "performance_file",  find(lines, UserdataController::tr("Aircraft Performance:"), "Aircraft Performance:", true));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "flightplan_cruise_altitude", findFt(lines, UserdataController::tr("Cruising altitude: %1"), "Cruising altitude: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "flightplan_number",  find(lines, UserdataController::tr("Flight Number: %1"), "Flight Number: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "aircraft_type",  find(lines, UserdataController::tr("Model: %1"), "Model: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "aircraft_registration",  find(lines, UserdataController::tr("Registration: %1"), "Registration: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "aircraft_name",  find(lines, UserdataController::tr("Type: %1"), "Type: "));

    /* *INDENT-ON* */
  }
  else
  {
    // Destination - update values into record ==================================
    /* *INDENT-OFF* */
    // Ignore runway for now
    // find(lines, UserdataController::tr(" runway %1"), " runway ");

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    set(recTo, "destination_time_sim", findDateTime(lines, UserdataController::tr("Simulator Date and Time: %1 %2, %3 %4"), "Simulator Date and Time: "));

    // Cannot parse real arrival time
    // set(recTo, "destination_time", findDateTime(lines, UserdataController::tr("Date and Time: %1"), "Date and Time: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "flightplan_file",  find(lines, UserdataController::tr("Flight Plan:"), "Flight Plan:", true));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "performance_file",  find(lines, UserdataController::tr("Aircraft Performance:"), "Aircraft Performance:", true));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "flightplan_cruise_altitude", findFt(lines, UserdataController::tr("Cruising altitude: %1"), "Cruising altitude: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "flightplan_number",  find(lines, UserdataController::tr("Flight Number: %1"), "Flight Number: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "aircraft_type",  find(lines, UserdataController::tr("Model: %1"), "Model: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "aircraft_registration",  find(lines, UserdataController::tr("Registration: %1"), "Registration: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "aircraft_name",  find(lines, UserdataController::tr("Type: %1"), "Type: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "distance", findNm(lines, UserdataController::tr("Flight Plan Distance: %1"), "Flight Plan Distance: "));

    /*: The translated texts in this method should not be changed to avoid issues with the logbook conversion */
    update(recTo, "distance_flown", findNm(lines, UserdataController::tr("Flown Distance: %1"), "Flown Distance: "));
    /* *INDENT-ON* */

    // Ignore time, speed and fuel values ===========================
    // findParam(lines, UserdataController::tr("Time: %1"), "Time: ");
    // findParam(lines, UserdataController::tr("Average Groundspeed: %1"), "Average Groundspeed: ");
    // findParam(lines, UserdataController::tr("Average True Airspeed: %1"), "Average True Airspeed: ");
    // findParam(lines, UserdataController::tr("Fuel consumed: %1"), "Fuel consumed: ");
    // findParam(lines, UserdataController::tr("Average fuel flow: %1"), "Average fuel flow: ");
  }

  // Ignored columns in logbook table
  // block_fuel, trip_fuel, used_fuel, is_jetfuel, grossweight,
  // departure_runway, destination_runway,
}

void LogdataConverter::set(atools::sql::SqlRecord& recTo, const QString& name, const QString& str)
{
  if(str.isEmpty())
    recTo.setNull(name);
  else
    recTo.setValue(name, str);
}

void LogdataConverter::update(atools::sql::SqlRecord& recTo, const QString& name, const QString& str)
{
  if(!str.isEmpty())
    recTo.setValue(name, str);
}

void LogdataConverter::set(atools::sql::SqlRecord& recTo, const QString& name, float value)
{
  if(value > INVALID_FLOAT / 2.f)
    recTo.setNull(name);
  else
    recTo.setValue(name, value);
}

void LogdataConverter::update(atools::sql::SqlRecord& recTo, const QString& name, float value)
{
  if(value < INVALID_FLOAT / 2.f)
    recTo.setValue(name, value);
}

void LogdataConverter::set(atools::sql::SqlRecord& recTo, const QString& name, QDateTime value)
{
  if(!value.isValid())
    recTo.setNull(name);
  else
    recTo.setValue(name, value);
}

void LogdataConverter::update(atools::sql::SqlRecord& recTo, const QString& name, QDateTime value)
{
  if(value.isValid())
    recTo.setValue(name, value);
}

float LogdataConverter::findFt(const QStringList& lines, QString nameTr, QString name)
{
  QString unit;
  float value = findFloat(lines, nameTr, name, unit);

  if(value < INVALID_FLOAT / 2.f)
  {
    // Try to extract unit and convert value
    if(unit == Unit::getSuffixAltFt())
      return value;
    else if(unit == Unit::getSuffixAltMeter())
      return atools::geo::meterToFeet(value);
    else
      errors.append(tr("Invalid altitude unit \"%1\" for %2.").arg(unit).arg(message));
  }
  return INVALID_FLOAT;
}

float LogdataConverter::findNm(const QStringList& lines, QString nameTr, QString name)
{
  QString unit;
  float value = findFloat(lines, nameTr, name, unit);

  if(value < INVALID_FLOAT / 2.f)
  {
    // Try to extract unit and convert value
    if(unit == Unit::getSuffixDistKm())
      return atools::geo::kmToNm(value);
    else if(unit == Unit::getSuffixDistNm())
      return value;
    else if(unit == Unit::getSuffixDistMi())
      return atools::geo::miToNm(value);
    else
      errors.append(tr("Invalid distance unit \"%1\" for %2.").arg(unit).arg(message));
  }
  return INVALID_FLOAT;
}

float LogdataConverter::findFloat(const QStringList& lines, QString nameTr, QString name, QString& unit)
{
  QString str = find(lines, nameTr, name, false).simplified();
  if(!str.isEmpty())
  {
    // Check if last is not a digit - must be a unit
    if(!str.at(str.length() - 1).isDigit())
    {
      unit = str.section(' ', -1).simplified().toLower();
      str = str.section(' ', 0, -2).simplified();
    }
    else
      unit.clear();

    bool ok;
    float retval = locale.toFloat(str, &ok);

    if(!ok)
      // Did not work using locale - try English again
      retval = localeEn.toFloat(str, &ok);

    if(ok)
      return retval;
    else
      errors.append(tr("Invalid number \"%1\" for %2.").arg(str).arg(message));
  }
  return INVALID_FLOAT;
}

QDateTime LogdataConverter::findDateTime(const QStringList& lines, QString nameTr, QString name)
{
  QDateTime retval;
  QString str = find(lines, nameTr, name, false).simplified();
  if(!str.isEmpty())
  {
    // Try various formats and locale to read date and time
    retval = formatter::readDateTime(str.section(',', 0, 0));

    if(!retval.isValid())
      errors.append(tr("Invalid date and time \"%1\" for %2.").arg(str).arg(message));
  }
  return retval;
}

QString LogdataConverter::find(const QStringList& lines, QString nameTr, QString name, bool nextLine)
{
  // Remove parameters
  int index = nameTr.indexOf(" %1");
  if(index >= 0)
    nameTr.truncate(index);

  // Look for keyword in lines
  for(int i = 0; i < lines.size(); i++)
  {
    const QString& line = lines.at(i);
    int idx = -1;

    // Check for translated word
    if(line.startsWith(nameTr, Qt::CaseInsensitive))
      idx = nameTr.size();

    // Check for English word
    if(line.startsWith(name, Qt::CaseInsensitive))
      idx = name.size();

    if(idx > 0)
    {
      if(nextLine)
        // Return line after header
        return lines.value(i + 1);
      else
        // Return all after colon
        return line.mid(idx).trimmed();
    }
  }
  return QString();
}
