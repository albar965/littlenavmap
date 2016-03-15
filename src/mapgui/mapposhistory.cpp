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

#include <QSettings>
#include <QDateTime>

const int MAX_MS_FOR_NEW_ENTRY = 1000;
const int MAX_NUMBER_OF_ENTRIES = 20;

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
  else
    return empty;
}

const MapPosHistoryEntry& MapPosHistory::back()
{
  if(currentIndex > 0)
  {
    currentIndex--;
    emit historyChanged(0, currentIndex, entries.size() - 1);
    return entries.at(currentIndex);
  }
  else
    return empty;
}

const MapPosHistoryEntry& MapPosHistory::current() const
{
  if(currentIndex != -1)
    return entries.at(currentIndex);
  else
    return empty;
}

void MapPosHistory::addEntry(atools::geo::Pos pos, int zoom)
{
  const MapPosHistoryEntry& curEntry = current();
  if(curEntry.pos == pos && curEntry.zoom == zoom)
    return;

  qint64 now = QDateTime::currentMSecsSinceEpoch();

  if(curEntry.timestamp > now - MAX_MS_FOR_NEW_ENTRY)
    entries[currentIndex] = {pos, zoom, now};
  else
  {
    if(currentIndex != -1)
      for(int i = currentIndex + 1; i < entries.size(); i++)
        entries.removeLast();

    entries.append({pos, zoom, now});
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
    list.append(QString::number(e.pos.getLonX(), 'f'));
    list.append(QString::number(e.pos.getLatY(), 'f'));
    list.append(QString::number(e.zoom));
    list.append(QString::number(e.timestamp));
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
      entries.append({atools::geo::Pos(list.at(i).toFloat(), list.at(i + 1).toFloat()),
                      list.at(i + 2).toInt(),
                      list.at(i + 3).toLongLong()});
    emit historyChanged(0, currentIndex, entries.size() - 1);
  }
  else
    emit historyChanged(0, 0, 0);
}
