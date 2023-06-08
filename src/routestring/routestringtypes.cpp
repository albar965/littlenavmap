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

#include "routestring/routestringtypes.h"

#include <QRegularExpression>

namespace rs {

QString cleanRouteStringLine(const QString& line)
{
  static const QRegularExpression REGEXP("[^A-Z0-9/\\.]");

  // Replace all non word characters with space
  return line.toUpper().replace(REGEXP, " ").simplified();
}

QStringList cleanRouteStringList(const QString& string)
{
  // Read line by line
  QStringList list;
  for(const QString& line : string.split('\n'))
  {
    if(line.simplified().isEmpty() && !list.isEmpty())
      // Found text already and now an empty line - stop here
      break;

    QString str(cleanRouteStringLine(line));

    if(!str.isEmpty())
      list.append(str.split(' '));
  }
  return list;
}

QString cleanRouteString(const QString& string)
{
  return cleanRouteStringList(string).join(' ');
}

} // namespace rs
