/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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
  static const QRegularExpression REGEXP("[^A-Z0-9/\\.\\-]");

  // Replace all non word characters except A-Z, 0-9, ".", and "-" with space
  // Remove - which are not part of an approach description
  return line.toUpper().replace(REGEXP, " ").replace(" -", " ").replace("- ", " ").simplified();
}

QStringList cleanRouteStringList(const QString& string)
{
  // Read line by line
  QStringList list;
  const QStringList split = string.split('\n');
  for(const QString& line : split)
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

QStringList routeStringOptionsToStr(rs::RouteStringOptions flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(NONE);
  ATOOLS_FLAGS_TO_STR(DCT);
  ATOOLS_FLAGS_TO_STR(START_AND_DEST);
  ATOOLS_FLAGS_TO_STR(ALT_AND_SPEED);
  ATOOLS_FLAGS_TO_STR(SID_STAR);
  ATOOLS_FLAGS_TO_STR(SID_STAR_GENERIC);
  ATOOLS_FLAGS_TO_STR(GFP);
  ATOOLS_FLAGS_TO_STR(NO_AIRWAYS);
  ATOOLS_FLAGS_TO_STR(SID_STAR_SPACE);
  ATOOLS_FLAGS_TO_STR(CORTEIN_DEPARTURE_RUNWAY);
  ATOOLS_FLAGS_TO_STR(CORTEIN_APPROACH);
  ATOOLS_FLAGS_TO_STR(FLIGHTLEVEL);
  ATOOLS_FLAGS_TO_STR(GFP_COORDS);
  ATOOLS_FLAGS_TO_STR(USR_WPT);
  ATOOLS_FLAGS_TO_STR(SKYVECTOR_COORDS);
  ATOOLS_FLAGS_TO_STR(NO_FINAL_DCT);
  ATOOLS_FLAGS_TO_STR(ALTERNATES);
  ATOOLS_FLAGS_TO_STR(READ_ALTERNATES);
  ATOOLS_FLAGS_TO_STR(READ_NO_AIRPORTS);
  ATOOLS_FLAGS_TO_STR(READ_MATCH_WAYPOINTS);
  ATOOLS_FLAGS_TO_STR(NO_TRACKS);
  ATOOLS_FLAGS_TO_STR(SID_STAR_NONE);
  ATOOLS_FLAGS_TO_STR(STAR_REV_TRANSITION);
  ATOOLS_FLAGS_TO_STR(REPORT);
  ATOOLS_FLAGS_TO_STR(ALT_AND_SPEED_METRIC);
  ATOOLS_FLAGS_TO_STR(WRITE_RUNWAYS);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, const rs::RouteStringOption& type)
{
  out << rs::RouteStringOptions(type);
  return out;
}

QDebug operator<<(QDebug out, const rs::RouteStringOptions& options)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << routeStringOptionsToStr(options).join("|");
  return out;
}

} // namespace rs
