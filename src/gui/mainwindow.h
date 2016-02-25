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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <geo/pos.h>

#include "sql/sqldatabase.h"

class QProgressDialog;
class Controller;
class ColumnList;
class SearchPaneList;

namespace atools {
namespace fs {
class BglReaderProgressInfo;
}
namespace gui {
class Dialog;
class ErrorHandler;
}

}

namespace Ui {
class MainWindow;
}

class NavMapWidget;

class MainWindow :
  public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  void mapContextMenu(const QPoint& pos);

  Ui::MainWindow *getUi() const
  {
    return ui;
  }

  NavMapWidget *getMapWidget() const
  {
    return mapWidget;
  }

signals:
  /* Emitted when window is shown the first time */
  void windowShown();

private:
  SearchPaneList *searchPanes;

  /* Work on the close event that also catches clicking the close button
   * in the window frame */
  virtual void closeEvent(QCloseEvent *event) override;

  /* Emit a signal windowShown after first appearance */
  virtual void showEvent(QShowEvent *event) override;

  Ui::MainWindow *ui;
  NavMapWidget *mapWidget = nullptr;
  QProgressDialog *progressDialog = nullptr;

  bool hasDatabaseLoadStatus = false;

  atools::gui::Dialog *dialog = nullptr;
  atools::gui::ErrorHandler *errorHandler = nullptr;
  void openDatabase();
  void closeDatabase();

  atools::sql::SqlDatabase db;
  QString databaseFile;
  void connectAllSlots();
  void mainWindowShown();

  bool firstStart = true;
  void writeSettings();
  void readSettings();
  void updateActionStates();
  void setupUi();
  void loadScenery();
  bool progressCallback(const atools::fs::BglReaderProgressInfo& progress);

  void createNavMap();
  void options();
  void preDatabaseLoad();
  void postDatabaseLoad(bool force);
  void createEmptySchema();

};

#endif // MAINWINDOW_H
