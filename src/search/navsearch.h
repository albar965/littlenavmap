/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_NAVSEARCH_H
#define LITTLENAVMAP_NAVSEARCH_H

#include "search/searchbasetable.h"

#include <QObject>

class QWidget;
class QTableView;
class SqlController;
class ColumnList;
class QAction;
class QMainWindow;
class Column;
class NavIconDelegate;
class QueryBuilderResult;

class QueryWidget;
namespace atools {
namespace sql {
class SqlDatabase;
}
}

/*
 * Navaid (VOR, NDB, and waypoint) search tab including all search widgets and the result table view.
 */
class NavSearch :
  public SearchBaseTable
{
  Q_OBJECT

public:
  explicit NavSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex);
  virtual ~NavSearch() override;

  NavSearch(const NavSearch& other) = delete;
  NavSearch& operator=(const NavSearch& other) = delete;

  /* All state saving is done through the widget state */
  virtual void saveState() override;
  virtual void restoreState() override;

  virtual void getSelectedMapObjects(map::MapResult& result) const override;
  virtual void connectSearchSlots() override;
  virtual void postDatabaseLoad() override;

private:
  virtual void updateButtonMenu() override;
  virtual void saveViewState(bool distanceSearchState) override;
  virtual void restoreViewState(bool distanceSearchState) override;
  virtual void updatePushButtons() override;
  QAction *followModeAction() override;

  void setCallbacks();
  QVariant modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                            const QVariant& displayRoleValue, Qt::ItemDataRole role) const;

  /* Formats the QVariant to a QString depending on column name */
  QString formatModelData(const Column *col, int row, const QVariant& displayRoleValue) const;

  /* All layouts, lines and drop down menu items */
  QList<QObject *> navSearchWidgets;

  /* All drop down menu actions */
  QList<QAction *> navSearchMenuActions;

  /* Draw navaid icon into ident table column */
  NavIconDelegate *iconDelegate = nullptr;

};

#endif // LITTLENAVMAP_NAVSEARCH_H
