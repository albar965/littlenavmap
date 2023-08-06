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

#include "web/webtools.h"

#include <QDebug>

#include "httpserver/httprequest.h"

using namespace stefanfrings;

Parameter::Parameter(const HttpRequest& request)
{
  /*
   * QMap is faster than QHash for "few" elements
   * https://woboq.com/blog/qmap_qhash_benchmark.html
   *
   * Thus do not convert to QList, read QList, lookup QMap, insert QHash.
   * Keep delivered QMultiMap.
   *
   * Possible improvement: act on number of elements, use method pointers to
   * set getters to functionality looking up on a QHash or the QMultiMap.
   */
  params = request.getParameterMap();
}

QString Parameter::asEnum(const QString& key, const QString& defaultValue, const QStringList& allowed) const
{
  // value supports defaultValue return, saves "contains"-lookup (which essentially equals value without return)
  QString str = QString::fromUtf8(params.value(key.toUtf8(), defaultValue.toUtf8()));

  // put wished/expected result first: no jump
  // put wished/expected comparison first: early success
  if(allowed.contains(str) || allowed.isEmpty())
    // instantly return (ignore any possibly same outcome from other branch possibly allowing a combined return afterwards but which is a jump after the else which a compiler might and might not translate to same code as implied here)
    return str;
  else
  {
    qWarning() << Q_FUNC_INFO << "Wrong enum value for" << key << ":" << str << "allowed" << allowed;
    return str;
  }
}

int Parameter::asInt(const QString& key, int defaultValue) const
{
  // value returns default-constructed value when none provided, will be catched by toInt conversion, saves "contains"-lookup (which essentially equals value without return)
  QByteArray str = params.value(key.toUtf8());

  bool ok = true;
  // use QByteArray's conversion. Is in "default C locale" !!
  int value = str.toInt(&ok);

  // put wished/expected result first: no jump
  if(ok)
    // instantly return (ignore any possibly same outcome from other branch possibly allowing a combined return afterwards but which is a jump after the else which a compiler might and might not translate to same code as implied here)
    return value;
  else
  {
    qWarning() << Q_FUNC_INFO << "Wrong int value for" << key << ":" << str;
    return defaultValue;
  }
}

float Parameter::asFloat(const QString& key, float defaultValue) const
{
  // value returns default-constructed value when none provided, will be catched by toInt conversion, saves "contains"-lookup (which essentially equals value without return)
  QByteArray str = params.value(key.toUtf8());

  bool ok = true;
  // use QByteArray's conversion. Is in "default C locale" !!
  float value = str.toFloat(&ok);

  // put wished/expected result first: no jump
  if(ok)
    // instantly return (ignore any possibly same outcome from other branch possibly allowing a combined return afterwards but which is a jump after the else which a compiler might and might not translate to same code as implied here)
    return value;
  else
  {
#ifdef DEBUG_INFORMATION_WEB
    qWarning() << Q_FUNC_INFO << "Wrong float value for" << key << ":" << str;
#endif
    return defaultValue;
  }
}

QString Parameter::asStr(const QString& key, const QString& defaultValue) const
{
  // value supports defaultValue return, saves "contains"-lookup (which essentially equals value without return)
  return QString::fromUtf8(params.value(key.toUtf8(), defaultValue.toUtf8()));
}

bool Parameter::has(const QString& key) const
{
  return params.contains(key.toUtf8());
}
