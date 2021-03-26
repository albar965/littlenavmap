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

#include "common/airportfiles.h"
#include "atools.h"

#include <QApplication>
#include <QDir>
#include <QSet>
#include <QStandardPaths>
#include <QDebug>

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

QStringList AirportFiles::getAirportFilesBase(const QString& airportIdent)
{
  QSet<QString> retval;

  /*: Important path parts "Files/Airports" to airport and files (".../Documents/Litte Navmap Files/Airports").
   *  Do not change after initial translation to avoid breaking the file lookup. */
  QFileInfo translatedPath = atools::documentsDir() +
                             QDir::separator() + QApplication::applicationName() +
                             tr(" Files") +
                             QDir::separator() +
                             tr("Airports") +
                             QDir::separator() + airportIdent;
  if(translatedPath.exists() && translatedPath.isDir())
    retval.insert(translatedPath.canonicalFilePath());

  QFileInfo path = atools::documentsDir() +
                   QDir::separator() + QApplication::applicationName() +
                   " Files" +
                   QDir::separator() +
                   "Airports" +
                   QDir::separator() + airportIdent;

  if(path.exists() && path.isDir())
    retval.insert(path.canonicalFilePath());
  return retval.toList();
}

void AirportFiles::updateAirportFiles(const QString& airportIdent)
{
  if(airportFilesIdent != airportIdent)
  {
    airportFilesIdent = airportIdent;
    airportFiles.clear();

    QSet<QString> filter;

    for(const QString& dir : getAirportFilesBase(airportIdent))
    {
      // Collect from translated airports name
      QDir airportDir = dir;
      if(airportDir.exists())
      {
        QFileInfoList entryInfoList = airportDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                                               QDir::Name | QDir::LocaleAware);

        for(const QFileInfo& file : entryInfoList)
        {
          if(!filter.contains(file.canonicalFilePath()))
          {
            filter.insert(file.canonicalFilePath());
            airportFiles.append(file);
          }
        }
      }
    }
  }
}
