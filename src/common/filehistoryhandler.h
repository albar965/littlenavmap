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

#ifndef LITTLENAVMAP_FILEHISTORYHANDLER_H
#define LITTLENAVMAP_FILEHISTORYHANDLER_H

#include <QObject>

class QMenu;
class QAction;

class FileHistoryHandler :
  public QObject
{
  Q_OBJECT

public:
  FileHistoryHandler(QObject *parent, const QString& settingsNamePrefix, QMenu *recentMenuList,
                     QAction *clearMenuAction);
  virtual ~FileHistoryHandler();

  void saveState();
  void restoreState();

  void addFile(const QString& filename);
  void removeFile(const QString& filename);

  int getMaxEntries() const
  {
    return maxEntries;
  }

  void setMaxEntries(int value)
  {
    maxEntries = value;
  }

signals:
  void fileSelected(const QString& filename);

private:
  void clearMenu();
  void itemTriggered(QAction *action);
  void updateMenu();

  QMenu *recentMenu;
  QAction *clearAction;
  QStringList files;
  QString settings;
  int maxEntries = 15;
};

#endif // LITTLENAVMAP_FILEHISTORYHANDLER_H
