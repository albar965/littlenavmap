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

#include "search/searcheventfilter.h"

#include "search/abstractsearch.h"

#include <QKeyEvent>

SearchViewEventFilter::SearchViewEventFilter(AbstractSearch *parent)
  : QObject(parent), searchBase(parent)
{
}

bool SearchViewEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if(keyEvent != nullptr && keyEvent->key() == Qt::Key_Return)
    {
      searchBase->showSelectedEntry();
      return true;
    }
  }

  return QObject::eventFilter(object, event);
}

SearchWidgetEventFilter::SearchWidgetEventFilter(AbstractSearch *parent)
  : QObject(parent), searchBase(parent)
{
}

bool SearchWidgetEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if(keyEvent != nullptr)
    {
      switch(keyEvent->key())
      {
        case Qt::Key_Down:
          searchBase->activateView();
          return true;

        case Qt::Key_Return:
          searchBase->showFirstEntry();
          return true;
      }
    }
  }

  return QObject::eventFilter(object, event);
}
