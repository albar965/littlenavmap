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

#ifndef LITTLENAVMAP_ONLINECLIENT_SEARCH_H
#define LITTLENAVMAP_ONLINECLIENT_SEARCH_H

#include "search/searchbasetable.h"

#include <QObject>

class QTableView;
class QAction;
class QMainWindow;
class Column;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

/*
 * Search tab for online network clients/pilots.
 */
class OnlineClientSearch :
  public SearchBaseTable
{
  Q_OBJECT

public:
  OnlineClientSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex);
  virtual ~OnlineClientSearch();

  /* All state saving is done through the widget state */
  virtual void saveState() override;
  virtual void restoreState() override;

  virtual void getSelectedMapObjects(map::MapSearchResult& result) const override;
  virtual void connectSearchSlots() override;
  virtual void postDatabaseLoad() override;

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

  QList<QObject *> onlineClientSearchWidgets;

};

#endif // LITTLENAVMAP_ONLINECLIENT_SEARCH_H
