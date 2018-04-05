/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_USERDATASEARCH_H
#define LITTLENAVMAP_USERDATASEARCH_H

#include "search/searchbasetable.h"

#include <QObject>

class QTableView;
class QAction;
class QMainWindow;
class Column;
class UserIconDelegate;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

/*
 * Search tab for user defined waypoints including all search widgets and the result table view.
 */
class UserdataSearch :
  public SearchBaseTable
{
  Q_OBJECT

public:
  UserdataSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex);
  virtual ~UserdataSearch();

  /* All state saving is done through the widget state */
  virtual void saveState() override;
  virtual void restoreState() override;

  virtual void getSelectedMapObjects(map::MapSearchResult& result) const override;
  virtual void connectSearchSlots() override;
  virtual void postDatabaseLoad() override;

signals:
  void addUserpoint(int id, const atools::geo::Pos& pos);
  void editUserpoints(const QVector<int>& ids);
  void deleteUserpoints(const QVector<int>& ids);

private:
  virtual void updateButtonMenu() override;
  virtual void saveViewState(bool distSearchActive) override;
  virtual void restoreViewState(bool distSearchActive) override;
  virtual void updatePushButtons() override;
  QAction *followModeAction() override;

  void setCallbacks();
  QVariant modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                            const QVariant& displayRoleValue, Qt::ItemDataRole role) const;
  QString formatModelData(const Column *col, const QVariant& displayRoleValue) const;
  void overrideMode(const QStringList& overrideColumnTitles);

  /* Edit button clicked */
  void editUserpointsTriggered();

  /* Delete button clicked */
  void deleteUserpointsTriggered();

  /* Add button */
  void addUserpointTriggered();

  /* All layouts, lines and drop down menu items */
  QList<QObject *> userdataSearchWidgets;

  /* All drop down menu actions */
  QList<QAction *> userdataSearchMenuActions;

  /* Draw type icon into type table column */
  UserIconDelegate *iconDelegate = nullptr;

};

#endif // LITTLENAVMAP_USERDATASEARCH_H
