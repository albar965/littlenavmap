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

#ifndef LITTLENAVMAP_LOGDATASEARCH_H
#define LITTLENAVMAP_LOGDATASEARCH_H

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
 * Search tab for logbook entries including all search widgets and the result table view.
 */
class LogdataSearch :
  public SearchBaseTable
{
  Q_OBJECT

public:
  LogdataSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex);
  virtual ~LogdataSearch() override;

  /* All state saving is done through the widget state */
  virtual void saveState() override;
  virtual void restoreState() override;

  virtual void getSelectedMapObjects(map::MapSearchResult& result) const override;
  virtual void connectSearchSlots() override;
  virtual void postDatabaseLoad() override;

signals:
  void addLogEntry();
  void editLogEntries(const QVector<int>& ids);
  void deleteLogEntries(const QVector<int>& ids);

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

  /* Edit button clicked */
  void editLogEntriesTriggered();

  /* Delete button clicked */
  void deleteLogEntriesTriggered();

  /* Add button */
  void addLogEntryTriggered();

  /* Callback function which creates a where clause using destination and departure ident. */
  QString airportQueryBuilderFunc(const QVector<QWidget *> widgets);

  /* All layouts, lines and drop down menu items */
  QList<QObject *> logdataSearchWidgets;

  /* All drop down menu actions */
  QList<QAction *> logdataSearchMenuActions;

};

#endif // LITTLENAVMAP_LOGDATASEARCH_H
