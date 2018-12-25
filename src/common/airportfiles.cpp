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

#include "common/airportfiles.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

QString AirportFiles::airportFilesIdent;
QFileInfoList AirportFiles::airportFiles;

AirportFiles::AirportFiles()
{

}

QFileInfoList AirportFiles::getAirportFiles(const QString& airportIdent)
{
  updateAirportFiles(airportIdent);
  return airportFiles;
}

QString AirportFiles::getAirportFilesBase(const QString& airportIdent)
{
  /*: Important path parts "Files/Airports" to airport and files (".../Documents/Litte Navmap Files/Airports"). */
  return QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first() +
         QDir::separator() + QApplication::applicationName() +
         tr(" Files") +
         QDir::separator() +
         tr("Airports") +
         QDir::separator() + airportIdent;
}

void AirportFiles::updateAirportFiles(const QString& airportIdent)
{
  if(airportFilesIdent != airportIdent)
  {
    airportFilesIdent = airportIdent;
    airportFiles.clear();

    // Collect from translated airports name
    QDir airportTrDir = getAirportFilesBase(airportIdent);
    if(airportTrDir.exists())
      airportFiles.append(airportTrDir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                                                     QDir::Name | QDir::LocaleAware));
  }
}
