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
  QMultiMap<QByteArray, QByteArray> parameterMap = request.getParameterMap();
  for(const QByteArray& key : parameterMap.keys())
    params.insert(QString::fromUtf8(key), QString::fromUtf8(parameterMap.value(key)));
}

QString Parameter::asEnum(const QString& key, const QString& defaultValue, const QStringList& allowed) const
{
  if(!params.contains(key))
    return defaultValue;
  else
  {
    QString str = params.value(key);

    if(!allowed.isEmpty() && !allowed.contains(str))
    {
      qWarning() << Q_FUNC_INFO << "Wrong enum value for" << key << ":" << str << "allowed" << allowed;
      return defaultValue;
    }
    else
      return str;
  }
}

int Parameter::asInt(const QString& key, int defaultValue) const
{
  if(!params.contains(key))
    return defaultValue;
  else
  {
    QString str = params.value(key);
    bool ok = true;
    int value = str.toInt(&ok);
    if(!ok)
    {
      qWarning() << Q_FUNC_INFO << "Wrong int value for" << key << ":" << str;
      return defaultValue;
    }
    else
      return value;
  }
}

float Parameter::asFloat(const QString& key, float defaultValue) const
{
  if(!params.contains(key))
    return defaultValue;
  else
  {
    QString str = params.value(key);
    bool ok = true;
    float value = str.toFloat(&ok);
    if(!ok)
    {
      qWarning() << Q_FUNC_INFO << "Wrong float value for" << key << ":" << str;
      return defaultValue;
    }
    else
      return value;
  }
}

QString Parameter::asStr(const QString& key, const QString& defaultValue) const
{
  if(!params.contains(key))
    return defaultValue;
  else
    return params.value(key);
}

bool Parameter::has(const QString& key) const
{
  return params.contains(key);
}
