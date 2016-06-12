/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "mapposhistory.h"
#include "settings/settings.h"
#include "logging/loggingdefs.h"
#include "geo/calculations.h"

#include <QSettings>
#include <QDateTime>
#include <QDataStream>

MapPosHistoryEntry::MapPosHistoryEntry()
{

}

MapPosHistoryEntry::MapPosHistoryEntry(const MapPosHistoryEntry& other)
{
  *this = other;
}

MapPosHistoryEntry::MapPosHistoryEntry(atools::geo::Pos position, double mapDistance, qint64 mapTimestamp)
  : timestamp(mapTimestamp), pos(position), distance(mapDistance)
{

}

MapPosHistoryEntry::~MapPosHistoryEntry()
{

}

bool MapPosHistoryEntry::operator==(const MapPosHistoryEntry& other) const
{
  return std::abs(distance - other.distance) < 0.001 &&
         pos == other.pos;
}

bool MapPosHistoryEntry::operator!=(const MapPosHistoryEntry& other) const
{
  return !operator==(other);
}

QDataStream& operator<<(QDataStream& out, const MapPosHistoryEntry& obj)
{
  out << obj.timestamp << obj.pos << obj.distance;
  return out;
}

QDataStream& operator>>(QDataStream& in, MapPosHistoryEntry& obj)
{
  in >> obj.timestamp >> obj.pos >> obj.distance;
  return in;
}

QDebug operator<<(QDebug debug, const MapPosHistoryEntry& entry)
{
  QDebugStateSaver save(debug);
  debug.nospace() << "MapPosHistoryEntry(" << entry.pos << ","
  << entry.distance << "," << entry.timestamp << ")";
  return debug;
}

// -----------------------------------------------------------------------

MapPosHistory::MapPosHistory(QObject *parent)
  : QObject(parent)
{
}

MapPosHistory::~MapPosHistory()
{
}

const MapPosHistoryEntry& MapPosHistory::next()
{
  if(currentIndex < entries.size() - 1)
  {
    currentIndex++;
    emit historyChanged(0, currentIndex, entries.size() - 1);
    return entries.at(currentIndex);
  }
  return EMPTY_MAP_POS;
}

const MapPosHistoryEntry& MapPosHistory::back()
{
  if(currentIndex > 0)
  {
    currentIndex--;
    emit historyChanged(0, currentIndex, entries.size() - 1);
    return entries.at(currentIndex);
  }
  return EMPTY_MAP_POS;
}

const MapPosHistoryEntry& MapPosHistory::current() const
{
  if(!entries.isEmpty())
    return entries.at(currentIndex);

  return EMPTY_MAP_POS;
}

void MapPosHistory::addEntry(atools::geo::Pos pos, double distance)
{
  MapPosHistoryEntry newEntry(pos, distance, QDateTime::currentMSecsSinceEpoch());
  const MapPosHistoryEntry& curEntry = current();

  if(newEntry == curEntry)
    return;

  if(curEntry.getTimestamp() > newEntry.getTimestamp() - MAX_MS_FOR_NEW_ENTRY)
  {
    entries[currentIndex] = newEntry;
  }
  else
  {
    if(currentIndex != -1)
    {
      int size = entries.size();
      for(int i = currentIndex + 1; i < size; i++)
        entries.removeLast();
    }

    entries.append(newEntry);
    currentIndex++;

    while(entries.size() > MAX_NUMBER_OF_ENTRIES)
    {
      entries.removeFirst();
      currentIndex--;
    }

    emit historyChanged(0, currentIndex, entries.size() - 1);
  }
}

void MapPosHistory::saveState(const QString& keyPrefix)
{
  using atools::settings::Settings;
  QVariant var = QVariant::fromValue<QList<MapPosHistoryEntry> >(entries);
  Settings::instance()->setValue(keyPrefix + "Entries",
                                 var);
  Settings::instance()->setValue(keyPrefix + "CurrentIndex", currentIndex);
}

void MapPosHistory::restoreState(const QString& keyPrefix)
{
  using atools::settings::Settings;
  entries = Settings::instance()->value(keyPrefix + "Entries").value<QList<MapPosHistoryEntry> >();
  currentIndex = Settings::instance()->value(keyPrefix + "CurrentIndex", -1).toInt();

  if(entries.isEmpty())
    emit historyChanged(0, 0, 0);
  else
    emit historyChanged(0, currentIndex, entries.size() - 1);
}
