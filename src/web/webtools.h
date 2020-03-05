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

#ifndef LNM_WEBTOOLS_H
#define LNM_WEBTOOLS_H

#include <QHash>

namespace stefanfrings {
class HttpRequest;
}

/*
 * Wraps parameters of a HTTP request and provides typed accessors for reading.
 */
class Parameter
{
public:
  /* Copies the parameters internally */
  Parameter(const stefanfrings::HttpRequest& request);

  /* Get key value. Returns default value if parameters do not contain
   * key or value does not fit in list of allowed. */
  QString asEnum(const QString& key, const QString& defaultValue, const QStringList& allowed) const;

  /* Get key value as integer. Returns default value if parameters do not contain
   * key or value is no valid integer. */
  int asInt(const QString& key, int defaultValue = 0) const;

  /* Get key value as float. Returns default value if parameters do not contain
   * key or value is no valid float. */
  float asFloat(const QString& key, float defaultValue = 0.f) const;

  /* Get key value as string. Returns default value if parameters do not contain key. */
  QString asStr(const QString& key, const QString& defaultValue = QString()) const;

  /* true if key is in the list */
  bool has(const QString& key) const;

private:
  QHash<QString, QString> params;
};

#endif // LNM_WEBTOOLS_H
