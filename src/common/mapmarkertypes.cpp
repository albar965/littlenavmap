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

#include "common/mapmarkertypes.h"

#include "common/unit.h"
#include "fs/common/binarymsageometry.h"
#include "util/xmlstreamreader.h"
#include "util/xmlstreamwriter.h"

namespace map {

// PatternMarker ######################################################################################
// .   <AirportICAO>EDDL</AirportICAO>
// .   <RunwayName>R10L</RunwayName>
// .   <Color>#101010</Color>
// .   <TurnRight>true</TurnRight>
// .   <Base45Degree>true</Base45Degree>
// .   <ShowEntryExit>true</ShowEntryExit>
// .   <RunwayLengthFt>10000</RunwayLengthFt>
// .   <DownwindParallelDistanceNM>2</DownwindParallelDistanceNM>
// .   <FinalDistanceNM>3.5</FinalDistanceNM>
// .   <DepartureDistanceNM>1.5</DepartureDistanceNM>
// .   <CourseTrueDeg>10.8</CourseTrueDeg>
// .   <MagVarDeg>1.5</MagVarDeg>
// .   <Pos Lon="13.368729" Lat="55.534626" Alt="231.00"/>
void PatternMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("AirportICAO"), airportIcao);
  stream.writeTextElement(QStringLiteral("RunwayName"), runwayName);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("TurnRight"), turnRight);
  stream.writeTextElement(QStringLiteral("Base45Degree"), base45Degree);
  stream.writeTextElement(QStringLiteral("ShowEntryExit"), showEntryExit);
  stream.writeTextElement(QStringLiteral("RunwayLengthFt"), runwayLength);
  stream.writeTextElement(QStringLiteral("DownwindParallelDistanceNM"), downwindParallelDistance);
  stream.writeTextElement(QStringLiteral("FinalDistanceNM"), finalDistance);
  stream.writeTextElement(QStringLiteral("DepartureDistanceNM"), departureDistance);
  stream.writeTextElement(QStringLiteral("CourseTrueDeg"), courseTrue);
  stream.writeTextElement(QStringLiteral("MagVarDeg"), magvar);
  stream.writeTextElement(QStringLiteral("Pos"), position, true /* writeAltitude */);
}

void PatternMarker::restore(atools::util::XmlStreamReader& stream)
{
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("AirportICAO"))
      airportIcao = stream.readElementTextStr();
    else if(name == QStringLiteral("RunwayName"))
      runwayName = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("TurnRight"))
      turnRight = stream.readElementTextBool();
    else if(name == QStringLiteral("Base45Degree"))
      base45Degree = stream.readElementTextBool();
    else if(name == QStringLiteral("ShowEntryExit"))
      showEntryExit = stream.readElementTextBool();
    else if(name == QStringLiteral("RunwayLengthFt"))
      runwayLength = stream.readElementTextInt();
    else if(name == QStringLiteral("DownwindParallelDistanceNM"))
      downwindParallelDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("FinalDistanceNM"))
      finalDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("DepartureDistanceNM"))
      departureDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("CourseTrueDeg"))
      courseTrue = stream.readElementTextFloat();
    else if(name == QStringLiteral("MagVarDeg"))
      magvar = stream.readElementTextFloat();
    else if(name == QStringLiteral("Pos"))
      position = stream.readElementPos();
    else
      stream.skipCurrentElement(true /* warning */);
  }

  objType = map::MARK_PATTERNS;
}

QString PatternMarker::displayText() const
{
  if(airportIcao.isEmpty())
    return QObject::tr("Traffic Pattern %1 RW %2").arg(turnRight ?
                                                       QObject::tr("R", "Pattern direction") :
                                                       QObject::tr("L", "Pattern direction")).arg(runwayName);
  else
    return QObject::tr("Traffic Pattern %1 %2 RW %3").
           arg(airportIcao).arg(turnRight ?
                                QObject::tr("R", "Pattern direction") :
                                QObject::tr("L", "Pattern direction")).arg(runwayName);
}

// HoldingMarker ######################################################################################
// .   <NavIdent>BOMBI</NavIdent>
// .   <NavType>VOR</NavType> AIRPORT, VOR, NDB or WAYPOINT
// .   <VorDmeOnly>true</VorDmeOnly>
// .   <VorHasDme>false</VorHasDme>
// .   <VorTacan>false</VorTacan>
// .   <VorVortac>false</VorVortac>
// .   <Color>#101010</Color>
// .   <TurnLeft>true</TurnLeft>
// .   <TimeMin>4.5</TimeMin>
// .   <SpeedKts>200</SpeedKts>
// .   <CourseTrueDeg>145</CourseTrueDeg>
// .   <MagvarDeg>1.5</MagvarDeg>
// .   <Pos Lon="13.368729" Lat="55.534626" Alt="231.00"/>
void HoldingMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("NavIdent"), holding.navIdent);
  stream.writeTextElement(QStringLiteral("NavType"), map::mapTypeToString(holding.navType).value(0));

  stream.writeStartElement(QStringLiteral("Vor"));
  stream.writeAttribute(QStringLiteral("DmeOnly"), holding.vorDmeOnly);
  stream.writeAttribute(QStringLiteral("HasDme"), holding.vorHasDme);
  stream.writeAttribute(QStringLiteral("Tacan"), holding.vorTacan);
  stream.writeAttribute(QStringLiteral("Vortac"), holding.vorVortac);
  stream.writeEndElement(); // Vor

  stream.writeTextElement(QStringLiteral("Color"), holding.color);
  stream.writeTextElement(QStringLiteral("TurnLeft"), holding.turnLeft);
  stream.writeTextElement(QStringLiteral("TimeMin"), holding.time);
  stream.writeTextElement(QStringLiteral("SpeedKts"), holding.speedKts);
  stream.writeTextElement(QStringLiteral("CourseTrueDeg"), holding.courseTrue);
  stream.writeTextElement(QStringLiteral("MagvarDeg"), holding.magvar);
  stream.writeTextElement(QStringLiteral("Pos"), holding.position, true /* writeAltitude */);
}

void HoldingMarker::restore(atools::util::XmlStreamReader& stream)
{
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("NavIdent"))
      holding.navIdent = stream.readElementTextStr();
    else if(name == QStringLiteral("NavType"))
      holding.navType = map::mapTypeFromString(QStringList({stream.readElementTextStr()}));
    else if(name == QStringLiteral("Vor"))
    {
      holding.vorDmeOnly = stream.readAttributeBool(QStringLiteral("DmeOnly"));
      holding.vorHasDme = stream.readAttributeBool(QStringLiteral("HasDme"));
      holding.vorTacan = stream.readAttributeBool(QStringLiteral("Tacan"));
      holding.vorVortac = stream.readAttributeBool(QStringLiteral("Vortac"));
      stream.skipCurrentElement();
    }
    else if(name == QStringLiteral("Color"))
      holding.color = stream.readElementTextColor();
    else if(name == QStringLiteral("TurnLeft"))
      holding.turnLeft = stream.readElementTextBool();
    else if(name == QStringLiteral("TimeMin"))
      holding.time = stream.readElementTextFloat();
    else if(name == QStringLiteral("SpeedKts"))
      holding.speedKts = stream.readElementTextFloat();
    else if(name == QStringLiteral("CourseTrueDeg"))
      holding.courseTrue = stream.readElementTextFloat();
    else if(name == QStringLiteral("MagvarDeg"))
      holding.magvar = stream.readElementTextFloat();
    else if(name == QStringLiteral("Pos"))
      position = holding.position = stream.readElementPos();
    else
      stream.skipCurrentElement(true /* warning */);
  }

  // Not used in user feature
  holding.length = holding.speedLimit = holding.minAltititude = holding.maxAltititude = 0.f;
  objType = map::MARK_HOLDING;
}

QString HoldingMarker::displayText() const
{
  if(holding.navIdent.isEmpty())
    return QObject::tr("User Holding %1 %2").
           arg(holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(holding.position.getAltitude()));
  else
    return QObject::tr("User Holding %1 %2 %3").
           arg(holding.navIdent).
           arg(holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(holding.position.getAltitude()));
}

// MsaMarker ######################################################################################
// .   <AirportIdent>EDDF</AirportIdent>
// .   <NavIdent>BOMBI</NavIdent>
// .   <Region>ED</Region>
// .   <MultipleCode></MultipleCode>
// .   <VorType>DME</VorType>
// .   <NavType>VOR</NavType> AIRPORT, VOR, NDB, ILS or WAYPOINT
// .   <VorDmeOnly>true</VorDmeOnly>
// .   <VorHasDme>true</VorHasDme>
// .   <VorTacan>false</VorTacan>
// .   <VorVortac>false</VorVortac>
// .   <RadiusNM>20</RadiusNM>
// .   <MagvarDeg>1.5</MagvarDeg>
// .   <Bearings>
// .     <BearingDeg>120</BearingDeg>
// .     <BearingDeg>180</BearingDeg>
// .   </Bearings>
// .   <Altitudes>
// .     <AltitudeFt>120</AltitudeFt>
// .     <AltitudeFt>180</AltitudeFt>
// .   </Altitudes>
// .   <TrueBearing>false</TrueBearing>
// .   <Pos Lon="13.368729" Lat="55.534626"/>
void MsaMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("AirportIdent"), msa.airportIdent);
  stream.writeTextElement(QStringLiteral("NavIdent"), msa.navIdent);
  stream.writeTextElement(QStringLiteral("Region"), msa.region);
  stream.writeTextElement(QStringLiteral("MultipleCode"), msa.multipleCode);
  stream.writeTextElement(QStringLiteral("VorType"), msa.vorType);
  stream.writeTextElement(QStringLiteral("NavType"), map::mapTypeToString(msa.navType).value(0));

  stream.writeStartElement(QStringLiteral("Vor"));
  stream.writeAttribute(QStringLiteral("DmeOnly"), msa.vorDmeOnly);
  stream.writeAttribute(QStringLiteral("HasDme"), msa.vorHasDme);
  stream.writeAttribute(QStringLiteral("Tacan"), msa.vorTacan);
  stream.writeAttribute(QStringLiteral("Vortac"), msa.vorVortac);
  stream.writeEndElement(); // Vor

  stream.writeTextElement(QStringLiteral("RadiusNM"), msa.radius);
  stream.writeTextElement(QStringLiteral("MagvarDeg"), msa.magvar);
  stream.writeTextElement(QStringLiteral("TrueBearing"), msa.trueBearing);
  stream.writeTextElement(QStringLiteral("Bearings"), QStringLiteral("BearingDeg"), msa.bearings);
  stream.writeTextElement(QStringLiteral("Altitudes"), QStringLiteral("AltitudeFt"), msa.altitudes);
  stream.writeTextElement(QStringLiteral("Pos"), msa.position);
}

void MsaMarker::restore(atools::util::XmlStreamReader& stream)
{
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("AirportIdent"))
      msa.airportIdent = stream.readElementTextStr();
    else if(name == QStringLiteral("NavIdent"))
      msa.navIdent = stream.readElementTextStr();
    else if(name == QStringLiteral("Region"))
      msa.region = stream.readElementTextStr();
    else if(name == QStringLiteral("MultipleCode"))
      msa.multipleCode = stream.readElementTextStr();
    else if(name == QStringLiteral("VorType"))
      msa.vorType = stream.readElementTextStr();
    else if(name == QStringLiteral("NavType"))
      msa.navType = map::mapTypeFromString(QStringList({stream.readElementTextStr()}));
    else if(name == QStringLiteral("Vor"))
    {
      msa.vorDmeOnly = stream.readAttributeBool(QStringLiteral("DmeOnly"));
      msa.vorHasDme = stream.readAttributeBool(QStringLiteral("HasDme"));
      msa.vorTacan = stream.readAttributeBool(QStringLiteral("Tacan"));
      msa.vorVortac = stream.readAttributeBool(QStringLiteral("Vortac"));
      stream.skipCurrentElement();
    }
    else if(name == QStringLiteral("RadiusNM"))
      msa.radius = stream.readElementTextFloat();
    else if(name == QStringLiteral("MagvarDeg"))
      msa.magvar = stream.readElementTextFloat();
    else if(name == QStringLiteral("TrueBearing"))
      msa.trueBearing = stream.readElementTextBool();
    else if(name == QStringLiteral("Bearings"))
      msa.bearings = stream.readElementListFloat(QStringLiteral("BearingDeg"));
    else if(name == QStringLiteral("Altitudes"))
      msa.altitudes = stream.readElementListFloat(QStringLiteral("AltitudeFt"));
    else if(name == QStringLiteral("Pos"))
      position = msa.position = stream.readElementPos();
    else
      stream.skipCurrentElement(true /* warning */);
  }

  // Calculate geometry and labels ===================
  if(msa.bearings.size() == msa.altitudes.size())
  {
    atools::fs::common::BinaryMsaGeometry geo;
    for(int i = 0; i < msa.bearings.size(); i++)
      geo.addSector(msa.bearings.at(i), msa.altitudes.at(i));

    geo.calculate(msa.position, msa.radius, msa.magvar, msa.trueBearing);
    msa.bearingEndPos = geo.getBearingEndPositions();
    msa.labelPositions = geo.getLabelPositions();
    msa.geometry = geo.getGeometry();
    msa.bounding = geo.getBoundingRect();
  }
  else
    qWarning() << Q_FUNC_INFO << msa.bearings.size() << "!=" << msa.altitudes.size();

  objType = map::MARK_MSA;
}

QString MsaMarker::displayText() const
{
  return airportMsaText(msa, true);
}

// RangeMarker ######################################################################################
// .   <Text>EDDF</Text>
// .   <Ranges>
// .     <RangeNM>2</RangeNM>
// .     <RangeNM>4</RangeNM>
// .   </Ranges>
// .   <Pos Lon="13.368729" Lat="55.534626"/>
// .   <Color>#101010</Color>
void RangeMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("Text"), text);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("Ranges"), QStringLiteral("RangeNM"), ranges);
  stream.writeTextElement(QStringLiteral("Pos"), position);
}

void RangeMarker::restore(atools::util::XmlStreamReader& stream)
{
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("Text"))
      text = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("Ranges"))
      ranges = stream.readElementListFloat(QStringLiteral("RangeNM"));
    else if(name == QStringLiteral("Pos"))
      position = stream.readElementPos();
    else
      stream.skipCurrentElement(true /* warning */);
  }

  objType = map::MARK_RANGE;
}

QString RangeMarker::displayText() const
{
  if(text.isEmpty())
    return QObject::tr("Range Rings");
  else
    return QObject::tr("Range Rings %1").arg(text);
}

// DistanceMarker ######################################################################################
// .   <Text>EDDF</Text>
// .   <Color>#101010</Color>
// .   <MagVarDeg>1.5</MagVarDeg>
// .   <Flags>RADIAL|CALIBRATED</Flags>
// .   <PosFrom Lon="13.368729" Lat="55.534626"/>
// .   <PosTo Lon="13.368729" Lat="55.534626"/>
void DistanceMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("Text"), text);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("MagvarDeg"), magvar);
  stream.writeTextElement(QStringLiteral("Radial"), flags.testFlag(DIST_MARK_RADIAL));
  stream.writeTextElement(QStringLiteral("Magvar"), flags.testFlag(DIST_MARK_MAGVAR));
  stream.writeTextElement(QStringLiteral("PosFrom"), to);
  stream.writeTextElement(QStringLiteral("PosTo"), from);
}

void DistanceMarker::restore(atools::util::XmlStreamReader& stream)
{
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("Text"))
      text = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("MagvarDeg"))
      magvar = stream.readElementTextFloat();
    else if(name == QStringLiteral("Radial"))
      flags.setFlag(map::DIST_MARK_RADIAL, stream.readElementTextBool());
    else if(name == QStringLiteral("Magvar"))
      flags.setFlag(map::DIST_MARK_MAGVAR, stream.readElementTextBool());
    else if(name == QStringLiteral("PosFrom"))
      from = stream.readElementPos();
    else if(name == QStringLiteral("PosTo"))
      to = stream.readElementPos();
    else
      stream.skipCurrentElement(true /* warning */);
  }

  objType = map::MARK_DISTANCE;
  position = to;
}

QString DistanceMarker::displayText() const
{
  float distanceMeter = getDistanceMeter();
  QString distStr(distanceMeter < map::INVALID_DISTANCE_VALUE ? Unit::distMeter(distanceMeter) : QStringLiteral());

  if(text.isEmpty())
    return QObject::tr("Measurement %1").arg(distStr);
  else
    return QObject::tr("Measurement %1 %2").arg(text).arg(distStr);
}

// Binary legacy functions ######################################################################################

QDataStream& operator>>(QDataStream& dataStream, PatternMarker& obj)
{
  dataStream >> obj.airportIcao >> obj.runwayName >> obj.color >> obj.turnRight >> obj.base45Degree >> obj.showEntryExit
  >> obj.runwayLength >> obj.downwindParallelDistance >> obj.finalDistance >> obj.departureDistance
  >> obj.courseTrue >> obj.magvar >> obj.position;
  obj.objType = map::MARK_PATTERNS;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const PatternMarker& obj)
{
  dataStream << obj.airportIcao << obj.runwayName << obj.color << obj.turnRight << obj.base45Degree << obj.showEntryExit
             << obj.runwayLength << obj.downwindParallelDistance << obj.finalDistance << obj.departureDistance
             << obj.courseTrue << obj.magvar << obj.position;

  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, HoldingMarker& obj)
{
  dataStream >> obj.holding.navIdent >> obj.holding.navType >> obj.holding.vorDmeOnly >> obj.holding.vorHasDme
  >> obj.holding.vorTacan >> obj.holding.vorVortac >> obj.holding.color >> obj.holding.turnLeft >> obj.holding.time
  >> obj.holding.speedKts >> obj.holding.courseTrue >> obj.holding.magvar >> obj.holding.position;

  obj.holding.length = obj.holding.speedLimit = obj.holding.minAltititude = obj.holding.maxAltititude = 0.f;
  obj.holding.airportIdent.clear();

  obj.position = obj.holding.position;
  obj.objType = map::MARK_HOLDING;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const HoldingMarker& obj)
{
  dataStream << obj.holding.navIdent << obj.holding.navType << obj.holding.vorDmeOnly << obj.holding.vorHasDme
             << obj.holding.vorTacan << obj.holding.vorVortac << obj.holding.color << obj.holding.turnLeft
             << obj.holding.time << obj.holding.speedKts << obj.holding.courseTrue << obj.holding.magvar << obj.holding.position;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::MsaMarker& obj)
{
  dataStream >> obj.msa.airportIdent >> obj.msa.navIdent >> obj.msa.region >> obj.msa.multipleCode >> obj.msa.vorType;
  dataStream >> obj.msa.navType >> obj.msa.vorDmeOnly >> obj.msa.vorHasDme >> obj.msa.vorTacan >> obj.msa.vorVortac;
  dataStream >> obj.msa.radius >> obj.msa.magvar;
  dataStream >> obj.msa.bearings >> obj.msa.altitudes >> obj.msa.trueBearing
  >> obj.msa.geometry >> obj.msa.labelPositions >> obj.msa.bearingEndPos >> obj.msa.bounding >> obj.msa.position;

  obj.position = obj.msa.position;
  obj.objType = map::MARK_MSA;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::MsaMarker& obj)
{
  dataStream << obj.msa.airportIdent << obj.msa.navIdent << obj.msa.region << obj.msa.multipleCode << obj.msa.vorType;
  dataStream << obj.msa.navType << obj.msa.vorDmeOnly << obj.msa.vorHasDme << obj.msa.vorTacan << obj.msa.vorVortac;
  dataStream << obj.msa.radius << obj.msa.magvar;
  dataStream << obj.msa.bearings << obj.msa.altitudes << obj.msa.trueBearing
             << obj.msa.geometry << obj.msa.labelPositions << obj.msa.bearingEndPos << obj.msa.bounding << obj.msa.position;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj)
{
  dataStream >> obj.text >> obj.ranges >> obj.position >> obj.color;
  obj.objType = map::MARK_RANGE;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj)
{
  dataStream << obj.text << obj.ranges << obj.position << obj.color;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj)
{
  dataStream >> obj.text >> obj.color >> obj.from >> obj.to >> obj.magvar >> obj.flags;
  obj.objType = map::MARK_DISTANCE;
  obj.position = obj.to;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj)
{
  dataStream << obj.text << obj.color << obj.from << obj.to << obj.magvar << obj.flags;
  return dataStream;
}

float PatternMarker::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - magvar);
}

float DistanceMarker::getDistanceNm() const
{
  return atools::geo::meterToNm(getDistanceMeter());
}

void registerMarkerMetaTypes()
{
  qRegisterMetaType<map::RangeMarker>();
  qRegisterMetaType<QList<map::RangeMarker> >();
  qRegisterMetaType<map::DistanceMarker>();
  qRegisterMetaType<QList<map::DistanceMarker> >();
  qRegisterMetaType<map::PatternMarker>();
  qRegisterMetaType<QList<map::PatternMarker> >();
  qRegisterMetaType<map::HoldingMarker>();
  qRegisterMetaType<QList<map::HoldingMarker> >();
  qRegisterMetaType<map::MsaMarker>();
  qRegisterMetaType<QList<map::MsaMarker> >();
}

} // namespace types
