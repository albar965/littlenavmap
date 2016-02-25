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

#ifndef NAVMAPWIDGET_H
#define NAVMAPWIDGET_H

#include <marble/MarbleWidget.h>

#include <QWidget>

namespace atools {
namespace geo {
class Pos;
}
}

class QContextMenuEvent;
class MainWindow;

class NavMapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  NavMapWidget(MainWindow *parent);

  void mapContextMenu(const QPoint& pos);

  void saveState();
  void restoreState();

  // QWidget interface

  void showPoint(double lonX, double latY, int zoom);

private:
  MainWindow *parentWindow;
  Marble::GeoDataPlacemark *place;

signals:
  void markChanged(const atools::geo::Pos& mark);

};

#endif // NAVMAPWIDGET_H
