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

#ifndef APSEARCHPANE_H
#define APSEARCHPANE_H

#include "geo/pos.h"

#include "table/searchpane.h"

#include <QObject>

class QWidget;
class QTableView;
class Controller;
class ColumnList;
class QAction;
class MainWindow;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class ApSearchPane :
  public SearchPane
{
  Q_OBJECT

public:
  explicit ApSearchPane(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                        atools::sql::SqlDatabase *sqlDb);
  virtual ~ApSearchPane();

  virtual void saveState() override;
  virtual void restoreState() override;

private:
  void connectSlots() override;
  void tableContextMenu(const QPoint& pos) override;

  QList<QObject *> airportSearchWidgets;
  QList<QAction *> airportSearchMenuActions;

};

#endif // APSEARCHPANE_H
