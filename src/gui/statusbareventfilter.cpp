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

#include "gui/statusbareventfilter.h"

#include <QHelpEvent>
#include <QLabel>
#include <QStatusBar>
#include <QToolTip>

StatusBarEventFilter::StatusBarEventFilter(QStatusBar *parentStatusBar, QLabel *firstLabel)
  : QObject(parentStatusBar), statusBar(parentStatusBar), firstWidget(firstLabel)
{

}

StatusBarEventFilter::~StatusBarEventFilter()
{

}

bool StatusBarEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    QHelpEvent *mouseEvent = dynamic_cast<QHelpEvent *>(event);
    if(mouseEvent != nullptr)
    {
      // Allow tooltip events only on the left side of the first label widget
      QRect rect(0, 0, firstWidget->geometry().left(), statusBar->height());
      if(!rect.contains(mouseEvent->pos()))
        return true;
    }
  }
  else if(event->type() == QEvent::MouseButtonRelease)
  {
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if(mouseEvent != nullptr)
    {
      // Allow tooltips on click only on the left side of the first label widget
      QRect rect(0, 0, firstWidget->geometry().left(), statusBar->height());
      if(rect.contains(mouseEvent->pos()))
      {
        QToolTip::showText(QCursor::pos(), statusBar->toolTip(), statusBar);
        return true;
      }
    }
  }

  return QObject::eventFilter(object, event);
}
