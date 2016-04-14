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

MapPosHistoryEntry::MapPosHistoryEntry()
{

}

MapPosHistoryEntry::MapPosHistoryEntry(atools::geo::Pos position, double mapDistance, qint64 mapTimestamp)
  : pos(position), distance(mapDistance), timestamp(mapTimestamp)
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
  QStringList list;
  list.append(QString::number(currentIndex));

  for(const MapPosHistoryEntry& e : entries)
  {
    list.append(QString::number(e.getPos().getLonX(), 'f'));
    list.append(QString::number(e.getPos().getLatY(), 'f'));
    list.append(QString::number(e.getDistance(), 'f'));
    list.append(QString::number(e.getTimestamp()));
  }

  using atools::settings::Settings;
  Settings::instance()->setValue(keyPrefix, list.join(";"));
}

void MapPosHistory::restoreState(const QString& keyPrefix)
{
  entries.clear();
  currentIndex = -1;

  using atools::settings::Settings;
  QString val = Settings::instance()->value(keyPrefix).toString();
  QStringList list = val.split(";", QString::SkipEmptyParts);

  if(!list.isEmpty())
  {
    currentIndex = list.at(0).toInt();
    list.removeFirst();
    for(int i = 0; i < list.size(); i += 4)
    {
      entries.append(MapPosHistoryEntry(
                       atools::geo::Pos(list.at(i).toFloat(), list.at(i + 1).toFloat()),
                       list.at(i + 2).toDouble(),
                       list.at(i + 3).toLongLong()));
    }
    emit historyChanged(0, currentIndex, entries.size() - 1);
  }
  else
    emit historyChanged(0, 0, 0);
}
