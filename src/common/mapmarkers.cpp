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

#include "common/mapmarkers.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/mapmarkertypes.h"
#include "common/unit.h"
#include "fs/db/databasemeta.h"
#include "io/fileroller.h"
#include "settings/settings.h"
#include "util/xmlstreamreader.h"
#include "util/xmlstreamwriter.h"

#include <QFile>

template<typename TYPE>
void assignIdAndInsert(MapMarkers *markers, const QString& settingsName, QMap<int, TYPE>& hash)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  for(auto obj :settings.valueVar(settingsName).value<QList<TYPE> >())
  {
    obj.id = markers->getNextMapMarkerId();
    hash.insert(obj.id, obj);
  }
}

template<typename TYPE>
void assignIdAndCopy(MapMarkers *markers, const QMap<int, TYPE>& markersFrom, QMap<int, TYPE>& markersTo)
{
  markersTo.clear();
  for(auto obj : markersFrom)
  {
    obj.id = markers->getNextMapMarkerId();
    markersTo.insert(obj.id, obj);
  }
}

template<typename TYPE>
void assignIdAndAppend(MapMarkers *markers, const QMap<int, TYPE>& markersFrom, QMap<int, TYPE>& markersTo)
{
  for(auto obj : markersFrom)
  {
    obj.id = markers->getNextMapMarkerId();
    markersTo.insert(obj.id, obj);
  }
}

/* Write list element and all items from the list as child elements */
template<typename TYPE>
void writeMarkerList(atools::util::XmlStreamWriter& writer, const QMap<int, TYPE>& markers, const QString& listElementName,
                     const QString& elementName)
{
  writer.writeStartElement(listElementName);
  for(const TYPE& marker : markers)
  {
    writer.writeStartElement(elementName);
    marker.save(writer);
    writer.writeEndElement(); // elementName
  }
  writer.writeEndElement(); // listElementName
}

/* Read all child elements from current, assign id and add to the list */
template<typename TYPE>
void readMarkerList(MapMarkers *markers, atools::util::XmlStreamReader& reader, QMap<int, TYPE>& markerHash, const QString& elementName)
{
  while(reader.readNextStartElement())
  {
    if(reader.name() == elementName)
    {
      TYPE marker;
      marker.restore(reader);
      marker.id = markers->getNextMapMarkerId();
      markerHash.insert(marker.id, marker);
    }
    else
      reader.skipCurrentElement(true /* warning */);
  }
}

// ######################################################################################
/*
<?xml version="1.0" encoding="UTF-8"?>
<LittleNavmap xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="https://www.littlenavmap.org/schema/lnmmarker.xsd">
  <MapMarkers>
    <Header>
      <CreationDate>2025-11-02T15:03:46+01:00</CreationDate>
      <FileVersion>1.2</FileVersion>
      <ProgramName>Little Navmap</ProgramName>
      <ProgramVersion>3.0.18</ProgramVersion>
      <Documentation>https://www.littlenavmap.org/lnmmarker.html</Documentation>
    </Header>
    <SimData Cycle="2509">XP12</SimData>
    <NavData Cycle="2509">NAVIGRAPH</NavData>
    <MapMarkerList>
      ...
    </MapMarkerList>
  </MapMarkers>
</LittleNavmap>
*/
void MapMarkers::save(const QString& filename, int numBackupFiles)
{
  qDebug() << Q_FUNC_INFO << filename;

  if(numBackupFiles > 0)
    atools::io::FileRoller(numBackupFiles).rollFile(filename);

  QFile file(filename);
  if(file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    atools::util::XmlStreamWriter writer(&file);

    writer.writeStartDocument(QStringLiteral("LittleNavmap"), QStringLiteral("https://www.littlenavmap.org/schema/lnmmarker.xsd"));
    writer.writeStartElement(QStringLiteral("MapMarkers"));

    // Save header and metadata =======================================================
    writer.writeStartElement(QStringLiteral("Header"));
    writer.writeTextElement(QStringLiteral("CreationDate"), atools::currentIsoWithOffset(false /* milliseconds */));
    writer.writeTextElement(QStringLiteral("FileVersion"), QStringLiteral("%1.%2").arg(MARKERS_VERSION_MAJOR).arg(MARKERS_VERSION_MINOR));
    writer.writeTextElement(QStringLiteral("ProgramName"), QCoreApplication::applicationName());
    writer.writeTextElement(QStringLiteral("ProgramVersion"), QCoreApplication::applicationVersion());
    writer.writeTextElement(QStringLiteral("Documentation"), QStringLiteral("https://www.littlenavmap.org/lnmmarker.html"));
    writer.writeEndElement(); // Header

    // Nav and sim metadata =======================================================
    QString cycle = NavApp::getDatabaseAiracCycleSim();
    QString source = NavApp::getDatabaseMetaSim()->getDataSource();
    writer.writeStartElement(QStringLiteral("SimData"));
    if(!cycle.isEmpty())
      writer.writeAttribute(QStringLiteral("Cycle"), cycle);
    if(!source.isEmpty())
      writer.writeCharacters(source);
    writer.writeEndElement(); // SimData

    cycle = NavApp::getDatabaseAiracCycleNav();
    source = NavApp::getDatabaseMetaNav()->getDataSource();
    writer.writeStartElement(QStringLiteral("NavData"));
    if(!cycle.isEmpty())
      writer.writeAttribute(QStringLiteral("Cycle"), cycle);
    if(!source.isEmpty())
      writer.writeCharacters(source);
    writer.writeEndElement(); // NavData

    // Marker list =======================================================
    writer.writeStartElement(QStringLiteral("MapMarkerList"));
    writeMarkerList(writer, distanceMarkers, QStringLiteral("MeasurementLineList"), QStringLiteral("MeasurementLine"));
    writeMarkerList(writer, rangeMarkers, QStringLiteral("RangeRingList"), QStringLiteral("RangeRing"));
    writeMarkerList(writer, patternMarkers, QStringLiteral("PatterMarkerList"), QStringLiteral("PatterMarker"));
    writeMarkerList(writer, holdingMarkers, QStringLiteral("HoldingList"), QStringLiteral("Holding"));
    writeMarkerList(writer, msaMarkers, QStringLiteral("MsaDiagramList"), QStringLiteral("MsaDiagram"));
    writer.writeEndElement(); // MapMarkerList

    writer.writeEndElement(); // MapMarkers
    writer.writeEndDocument(); // LittleNavmap
    file.close();
  }
  else
    qWarning() << Q_FUNC_INFO << "Error writing" << filename << file.errorString();
}

void MapMarkers::restore(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  // Read XML file ================================================================
  QFile file(filename);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    atools::util::XmlStreamReader reader(&file);

    // Skip header ==========
    reader.readUntilElement(QStringLiteral("MapMarkerList"));

    while(reader.readNextStartElement())
    {
      QStringView name = reader.name();
      if(name == QStringLiteral("MeasurementLineList"))
        readMarkerList(this, reader, distanceMarkers, QStringLiteral("MeasurementLine"));
      else if(name == QStringLiteral("RangeRingList"))
        readMarkerList(this, reader, rangeMarkers, QStringLiteral("RangeRing"));
      else if(name == QStringLiteral("PatterMarkerList"))
        readMarkerList(this, reader, patternMarkers, QStringLiteral("PatterMarker"));
      else if(name == QStringLiteral("HoldingList"))
        readMarkerList(this, reader, holdingMarkers, QStringLiteral("Holding"));
      else if(name == QStringLiteral("MsaDiagramList"))
        readMarkerList(this, reader, msaMarkers, QStringLiteral("MsaDiagram"));
      else
        reader.skipCurrentElement(true /* warning */);
    }
    file.close();
  }
  else
    qWarning() << Q_FUNC_INFO << "Error reading" << filename << file.errorString();
}

void MapMarkers::restoreFromSettings()
{
  // Read legacy from settings file ================================================================
  assignIdAndInsert<map::DistanceMarker>(this, lnm::MAP_DISTANCEMARKERS, distanceMarkers);
  assignIdAndInsert<map::RangeMarker>(this, lnm::MAP_RANGEMARKERS, rangeMarkers);
  assignIdAndInsert<map::PatternMarker>(this, lnm::MAP_PATTERNMARKERS, patternMarkers);
  assignIdAndInsert<map::HoldingMarker>(this, lnm::MAP_HOLDING_MARKERS, holdingMarkers);
  assignIdAndInsert<map::MsaMarker>(this, lnm::MAP_AIRPORT_MSA_MARKERS, msaMarkers);
}

void MapMarkers::removeRangeMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  rangeMarkers.remove(id);
}

void MapMarkers::removePatternMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  patternMarkers.remove(id);
}

void MapMarkers::removeDistanceMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  distanceMarkers.remove(id);
}

void MapMarkers::removeHoldingMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  holdingMarkers.remove(id);
}

void MapMarkers::removeMsaMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  msaMarkers.remove(id);
}

void MapMarkers::clear(map::MapTypes types)
{
  if(types.testFlag(map::MARK_RANGE))
    rangeMarkers.clear();

  if(types.testFlag(map::MARK_DISTANCE))
    distanceMarkers.clear();

  if(types.testFlag(map::MARK_PATTERNS))
    patternMarkers.clear();

  if(types.testFlag(map::MARK_HOLDING))
    holdingMarkers.clear();

  if(types.testFlag(map::MARK_MSA))
    msaMarkers.clear();
}

void MapMarkers::copy(const MapMarkers& other, map::MapTypes types)
{
  if(types.testFlag(map::MARK_RANGE))
    assignIdAndCopy(this, other.rangeMarkers, this->rangeMarkers);

  if(types.testFlag(map::MARK_DISTANCE))
    assignIdAndCopy(this, other.distanceMarkers, this->distanceMarkers);

  if(types.testFlag(map::MARK_PATTERNS))
    assignIdAndCopy(this, other.patternMarkers, this->patternMarkers);

  if(types.testFlag(map::MARK_HOLDING))
    assignIdAndCopy(this, other.holdingMarkers, this->holdingMarkers);

  if(types.testFlag(map::MARK_MSA))
    assignIdAndCopy(this, other.msaMarkers, this->msaMarkers);
}

void MapMarkers::append(const MapMarkers& other, map::MapTypes types)
{
  if(types.testFlag(map::MARK_RANGE))
    assignIdAndAppend(this, other.rangeMarkers, this->rangeMarkers);

  if(types.testFlag(map::MARK_DISTANCE))
    assignIdAndAppend(this, other.distanceMarkers, this->distanceMarkers);

  if(types.testFlag(map::MARK_PATTERNS))
    assignIdAndAppend(this, other.patternMarkers, this->patternMarkers);

  if(types.testFlag(map::MARK_HOLDING))
    assignIdAndAppend(this, other.holdingMarkers, this->holdingMarkers);

  if(types.testFlag(map::MARK_MSA))
    assignIdAndAppend(this, other.msaMarkers, this->msaMarkers);
}

void MapMarkers::updateDistanceMarkerFromPos(int id, const atools::geo::Pos& pos)
{
  if(distanceMarkers.contains(id))
    distanceMarkers[id].from = pos;
}

void MapMarkers::updateDistanceMarkerToPos(int id, const atools::geo::Pos& pos)
{
  if(distanceMarkers.contains(id))
    distanceMarkers[id].to = distanceMarkers[id].position = pos;
}

void MapMarkers::updateHoldingMarkerPosAndAlt(int id, const atools::geo::Pos& pos)
{
  if(holdingMarkers.contains(id))
    holdingMarkers[id].position = holdingMarkers[id].holding.position = pos;
}

void MapMarkers::updateRangeMarkerPos(int id, const atools::geo::Pos& pos)
{
  if(rangeMarkers.contains(id))
    rangeMarkers[id].position = pos;
}

void MapMarkers::updateDistanceMarker(const map::DistanceMarker& marker)
{
  if(distanceMarkers.contains(marker.id))
    distanceMarkers[marker.id] = marker;
}

void MapMarkers::updateHoldingMarker(const map::HoldingMarker& marker)
{
  if(holdingMarkers.contains(marker.id))
    holdingMarkers[marker.id] = marker;
}

void MapMarkers::updateRangeMarker(const map::RangeMarker& marker)
{
  if(rangeMarkers.contains(marker.id))
    rangeMarkers[marker.id] = marker;
}

map::DistanceMarker& MapMarkers::getDistanceMarkerRef(int id)
{
  return distanceMarkers[id];
}

map::HoldingMarker& MapMarkers::getHoldingMarkerRef(int id)
{
  return holdingMarkers[id];
}

map::RangeMarker& MapMarkers::getRangeMarkerRef(int id)
{
  return rangeMarkers[id];
}

map::PatternMarker& MapMarkers::getPatternMarkerRef(int id)
{
  return patternMarkers[id];
}

bool MapMarkers::hasRangeMarkers() const
{
  return !rangeMarkers.isEmpty();
}

bool MapMarkers::hasDistanceMarkers() const
{
  return !distanceMarkers.isEmpty();
}

bool MapMarkers::hasPatternMarkers() const
{
  return !patternMarkers.isEmpty();
}

bool MapMarkers::hasHoldingMarkers() const
{
  return !holdingMarkers.isEmpty();
}

bool MapMarkers::hasMsaMarkers() const
{
  return !msaMarkers.isEmpty();
}

bool MapMarkers::hasAnyMarkers() const
{
  return rangeMarkers.size() + distanceMarkers.size() + patternMarkers.size() + holdingMarkers.size() + msaMarkers.size() > 0;
}

int MapMarkers::getNextMapMarkerId()
{
  return ++currentMapMarkerId;
}

bool MapMarkers::isMarkersFile(const QString& filename)
{
  // Assume LNM XML files to be formatted
  const QStringList probe = atools::probeFile(filename, 30);
  return probe.at(0).startsWith(QStringLiteral("<?xml version")) &&
         probe.at(1).startsWith(QStringLiteral("<littlenavmap")) &&
         probe.at(2).startsWith(QStringLiteral("<mapmarkers>"));
}

const QList<const map::MapAirportMsa *> MapMarkers::getMsaMarksFiltered(int currentId) const
{
  // Sort markers into a list where the last one is the one currently edited and drawn on top
  QList<const map::MapAirportMsa *> markers;
  const map::MapAirportMsa *current = nullptr;

  for(const map::MsaMarker& marker : msaMarkers)
  {
    if(marker.id == currentId)
      current = &marker.msa;
    else
      markers.append(&marker.msa);
  }

  if(current != nullptr)
    markers.append(current);

  return markers;
}

const QList<const map::MapHolding *> MapMarkers::getHoldingMarksFiltered(int currentId, QStringList& texts) const
{
  // Sort markers into a list where the last one is the one currently edited and drawn on top
  QList<const map::MapHolding *> markers;
  const map::MapHolding *current = nullptr;
  QString currentText;

  for(const map::HoldingMarker& marker : holdingMarkers)
  {
    if(marker.id == currentId)
    {
      current = &marker.holding;
      currentText = marker.text;
    }
    else
    {
      markers.append(&marker.holding);
      texts.append(marker.text);
    }
  }

  if(current != nullptr)
  {
    markers.append(current);
    texts.append(currentText);
  }

  return markers;
}

void MapMarkers::addRangeMark(const map::RangeMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  rangeMarkers.insert(obj.id, obj);
}

void MapMarkers::addPatternMark(const map::PatternMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  patternMarkers.insert(obj.id, obj);
}

void MapMarkers::addDistanceMark(const map::DistanceMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  distanceMarkers.insert(obj.id, obj);
}

void MapMarkers::addHoldingMark(const map::HoldingMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  holdingMarkers.insert(obj.id, obj);
}

void MapMarkers::addMsaMark(const map::MsaMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  msaMarkers.insert(obj.id, obj);
}
