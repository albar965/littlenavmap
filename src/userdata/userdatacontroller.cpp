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

#include "userdata/userdatacontroller.h"

#include "fs/userdata/userdatamanager.h"
#include "common/constants.h"
#include "sql/sqlrecord.h"
#include "navapp.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "userdata/userdatadialog.h"
#include "userdata/userdataicons.h"

#include <QDebug>

UserdataController::UserdataController(atools::fs::userdata::UserdataManager *userdataManager, MainWindow *parent)
  : manager(userdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  icons = new UserdataIcons(mainWindow);
  icons->loadIcons();
}

UserdataController::~UserdataController()
{
  delete dialog;
  delete icons;
}

void UserdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  ui->tabWidgetSearch->setCurrentIndex(3);
}

void UserdataController::addUserpoint()
{
  qDebug() << Q_FUNC_INFO;

  UserdataDialog dlg(mainWindow, ud::ADD, icons);
  dlg.setRecord(manager->emptyRecord());
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    // Add to database
    manager->insertByRecord(dlg.getRecord());
    manager->commit();
    emit refreshUserdataSearch();
    mainWindow->setStatusMessage(tr("User defined waypoint added."));
  }
}

void UserdataController::editUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  atools::sql::SqlRecord rec = manager->record(ids.first());
  if(!rec.isEmpty())
  {
    UserdataDialog dlg(mainWindow, ids.size() > 1 ? ud::EDIT_MULTIPLE : ud::EDIT_ONE, icons);
    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      manager->updateByRecord(dlg.getRecord(), ids);
      manager->commit();

      emit refreshUserdataSearch();
      mainWindow->setStatusMessage(tr("%n user defined waypoint(s) updated.", "", ids.size()));
    }
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void UserdataController::deleteUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Delete %n defined waypoint(s)?", "", ids.size()));

  if(retval == QMessageBox::Yes)
  {
    manager->removeRows(ids);
    manager->commit();

    emit refreshUserdataSearch();
    mainWindow->setStatusMessage(tr("%n user defined waypoint(s) deleted.", "", ids.size()));
  }
}

void UserdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;

  QString file = dialog->openFileDialog(
    tr("Open User defined Waypoint CSV File"),
    tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_CSV), "Userdata/Csv");

  if(!file.isEmpty())
  {
    manager->importCsv(file, atools::fs::userdata::NONE, ',', '"');
    emit refreshUserdataSearch();
  }
}

void UserdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::importXplane()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::exportXplane()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::importGarmin()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::exportGarmin()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::exportBgl()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::clearDatabase()
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Really delete all user defined waypoints?\n\nThis cannot be undone."));

  if(retval == QMessageBox::Yes)
  {
    manager->clearData();
    emit refreshUserdataSearch();
  }
}
