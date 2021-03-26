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

#ifndef LNM_AIRPORTFILES_H
#define LNM_AIRPORTFILES_H

#include <QFileInfoList>
#include <QApplication>

/*
 * Class that does simple caching of a list of files found in the translated folder "Documents/Little Navmap Files/ICAO"
 * for an airport.
 *
 * Keeps only one entry to allow file system updates by changing airport information by clicking on a different airport.
 */
class AirportFiles
{
  Q_DECLARE_TR_FUNCTIONS(AirportFiles)

public:
  /* Get all files related to this airport ICAO */
  static QFileInfoList getAirportFiles(const QString& airportIdent);

  /* Get base folder "Documents/Little Navmap Files/ICAO" for this airport ICAO
   *  Looks up folders using translated names and untranslated names. */
  static QStringList getAirportFilesBase(const QString& airportIdent);

private:
  AirportFiles();

  /* Look for files in documents */
  static void updateAirportFiles(const QString& airportIdent);

  static QString airportFilesIdent;
  static QFileInfoList airportFiles;
};

#endif // LNM_AIRPORTFILES_H
