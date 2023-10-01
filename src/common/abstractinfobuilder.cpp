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
#include "abstractinfobuilder.h"

#include "common/infobuildertypes.h"
#include "geo/calculations.h"
#include "fs/util/fsutil.h"
#include "sql/sqlrecord.h"
#include "common/formatter.h"
#include "common/unit.h"

using formatter::courseTextFromTrue;
using atools::geo::opposedCourseDeg;
using atools::geo::Pos;
using atools::fs::util::roundComFrequency;

using InfoBuilderTypes::AirportInfoData;

AbstractInfoBuilder::AbstractInfoBuilder(QObject *parent)
  : QObject(parent)
{
  contentTypeHeader = "text/plain";
}

AbstractInfoBuilder::~AbstractInfoBuilder()
{

}

QByteArray AbstractInfoBuilder::airport(AirportInfoData airportInfoData) const
{
  Q_UNUSED(airportInfoData);
    return "not implemented";
}

QByteArray AbstractInfoBuilder::siminfo(SimConnectInfoData simConnectInfoData) const
{
  Q_UNUSED(simConnectInfoData);
    return "not implemented";
}

QByteArray AbstractInfoBuilder::uiinfo(UiInfoData uiInfoData) const
{
  Q_UNUSED(uiInfoData);
    return "not implemented";
}

QByteArray AbstractInfoBuilder::features(MapFeaturesData mapFeaturesData) const
{
  Q_UNUSED(mapFeaturesData);
    return "not implemented";
}

QByteArray AbstractInfoBuilder::feature(MapFeaturesData mapFeaturesData) const
{
  Q_UNUSED(mapFeaturesData);
    return "not implemented";
}


QString AbstractInfoBuilder::getHeadingsStringByMagVar(float heading, float magvar) const {

    return courseTextFromTrue(heading, magvar) +", "+ courseTextFromTrue(opposedCourseDeg(heading), magvar);

}

QString AbstractInfoBuilder::formatComFrequency(int frequency) const {

    return locale.toString(roundComFrequency(frequency), 'f', 3) + tr(" MHz");

}

QString AbstractInfoBuilder::getCoordinatesString(const atools::sql::SqlRecord *rec) const
{
  if(rec != nullptr && rec->contains("lonx") && rec->contains("laty"))
    return getCoordinatesString(Pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), rec->valueFloat("altitude", 0.f)));
  return QString();
}

QString AbstractInfoBuilder::getCoordinatesString(const Pos& pos) const
{
  if(pos.isValid())
    return QString::number(pos.getLatY()) +" "+ QString::number(pos.getLonX());
  return QString();
}

QMap<QString,float> AbstractInfoBuilder::getCoordinates(const atools::sql::SqlRecord *rec) const
{
  if(rec != nullptr && rec->contains("lonx") && rec->contains("laty"))
    return getCoordinates(Pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), rec->valueFloat("altitude", 0.f)));
  return QMap<QString,float>();
}

QMap<QString,float> AbstractInfoBuilder::getCoordinates(const Pos& pos) const
{
  QMap<QString,float> map = QMap<QString,float>();
  if(pos.isValid()){
      map.insert("lat",pos.getLatY());
      map.insert("lon",pos.getLonX());
  }
  return map;
}

QString AbstractInfoBuilder::getLocalizedCoordinatesString(const Pos& pos) const
{
  if(pos.isValid())
    return Unit::coords(pos);
  return QString();
}
