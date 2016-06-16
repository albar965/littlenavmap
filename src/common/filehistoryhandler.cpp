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

#include "common/filehistoryhandler.h"

#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include "settings/settings.h"

FileHistoryHandler::FileHistoryHandler(QObject *parent, const QString& settingsName,
                                       QMenu *recentMenuList, QAction *clearMenuAction) :
  QObject(parent), recentMenu(recentMenuList), clearAction(clearMenuAction), settings(settingsName)
{
  connect(recentMenu, &QMenu::triggered, this, &FileHistoryHandler::itemTriggered);
}

FileHistoryHandler::~FileHistoryHandler()
{

}

void FileHistoryHandler::saveState()
{
  atools::settings::Settings::instance().setValue(settings, files);
}

void FileHistoryHandler::restoreState()
{
  files = atools::settings::Settings::instance().valueStrList(settings);
  updateMenu();
}

void FileHistoryHandler::addFile(const QString& filename)
{
  if(!filename.isEmpty())
  {
    if(files.contains(filename))
      files.removeAll(filename);
    files.prepend(filename);

    while(files.size() > maxEntries)
      files.removeLast();

    updateMenu();
  }
}

void FileHistoryHandler::removeFile(const QString& filename)
{
  files.removeAll(filename);
  updateMenu();
}

void FileHistoryHandler::itemTriggered(QAction *action)
{
  if(action == clearAction)
    clearMenu();
  else
  {
    // Move file up in the list
    QString fname = action->data().toString();
    files.removeAll(fname);
    files.prepend(fname);
    updateMenu();

    emit fileSelected(fname);
  }
}

void FileHistoryHandler::updateMenu()
{
  recentMenu->clear();

  int i = 1;
  for(const QString& filepath : files)
  {
    QString fname = QFileInfo(filepath).fileName();
    if(i < 10)
      fname = "&" + QString::number(i) + " " + fname;
    else if(i == 10)
      fname = "&0 " + fname;

    QAction *fileAction = recentMenu->addAction(fname);
    fileAction->setToolTip(filepath);
    fileAction->setStatusTip(filepath);
    fileAction->setData(filepath);
    i++;
  }

  recentMenu->addSeparator();
  clearAction->setEnabled(!files.isEmpty());
  recentMenu->addAction(clearAction);
}

void FileHistoryHandler::clearMenu()
{
  files.clear();
  updateMenu();
}
