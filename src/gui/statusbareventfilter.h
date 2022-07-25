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

#ifndef LNM_STATUSBAREVENTFILTER_H
#define LNM_STATUSBAREVENTFILTER_H

#include <QObject>

class QStatusBar;
class QLabel;

/*
 * Catches events from the status bar to show a tooltip on click in the region to the left of the first label.
 */
class StatusBarEventFilter :
  public QObject
{
public:
  explicit StatusBarEventFilter(QStatusBar *parentStatusBar, QLabel *firstLabel);
  virtual ~StatusBarEventFilter() override;

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  QStatusBar *statusBar;
  QLabel *firstWidget;
};

#endif // LNM_STATUSBAREVENTFILTER_H
