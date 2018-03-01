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
#include "settings/settings.h"
#include "options/optiondata.h"

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

QString UserdataController::getDefaultType(const QString& type)
{
  return icons->getDefaultType(type);
}

void UserdataController::addToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  QToolButton *button = new QToolButton(ui->toolbarMapOptions);

  // Create and add toolbar button =====================================
  button->setIcon(QIcon(":/littlenavmap/resources/icons/userdata_POI.svg"));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(tr("Select userpoints for display"));
  button->setStatusTip(button->toolTip());
  button->setCheckable(true);
  userdataToolButton = button;

  // Insert before show route
  ui->toolbarMapOptions->insertWidget(ui->actionMapShowRoute, button);
  ui->toolbarMapOptions->insertSeparator(ui->actionMapShowRoute);

  // Create and add select all action =====================================
  actionAll = new QAction(tr("All"), button);
  actionAll->setToolTip(tr("Enable all userpoints"));
  actionAll->setStatusTip(actionAll->toolTip());
  button->addAction(actionAll);
  connect(actionAll, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select none action =====================================
  actionNone = new QAction(tr("None"), button);
  actionNone->setToolTip(tr("Disable all userpoints"));
  actionNone->setStatusTip(actionAll->toolTip());
  button->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select unknown action =====================================
  actionUnknown = new QAction(tr("Unknown Types"), button);
  actionUnknown->setToolTip(tr("Enable or disable unknown userpoint types"));
  actionUnknown->setStatusTip(tr("Enable or disable unknown userpoint types"));
  actionUnknown->setCheckable(true);
  button->addAction(actionUnknown);
  ui->menuViewUserpoints->addAction(actionUnknown);
  ui->menuViewUserpoints->addSeparator();
  connect(actionUnknown, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select an action for each registered type =====================================
  for(const QString& type : icons->getAllTypes())
  {
    QIcon icon(icons->getIconPath(type));
    QAction *action = new QAction(icon, type, button);
    action->setData(QVariant(type));
    action->setCheckable(true);
    button->addAction(action);
    ui->menuViewUserpoints->addAction(action);
    connect(action, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);
    actions.append(action);
  }
}

void UserdataController::toolbarActionTriggered()
{
  qDebug() << Q_FUNC_INFO;

  QAction *action = dynamic_cast<QAction *>(sender());
  if(action != nullptr)
  {
    if(action == actionAll)
    {
      // Select all buttons
      actionUnknown->setChecked(true);
      for(QAction *a : actions)
        if(a->data().type() == QVariant::String)
          a->setChecked(true);
    }
    else if(action == actionNone)
    {
      // Deselect all buttons
      actionUnknown->setChecked(false);
      for(QAction *a : actions)
        if(a->data().type() == QVariant::String)
          a->setChecked(false);
    }
    // Copy action state to class data
    actionsToTypes();
  }
  emit userdataChanged();
}

void UserdataController::actionsToTypes()
{
  // Copy state for known types
  selectedTypes.clear();
  for(QAction *action : actions)
  {
    if(action->isChecked())
      selectedTypes.append(action->data().toString());
  }

  selectedUnknownType = actionUnknown->isChecked();
  userdataToolButton->setChecked(!selectedTypes.isEmpty() || selectedUnknownType);
  qDebug() << Q_FUNC_INFO << selectedTypes;
}

void UserdataController::typesToActions()
{
  // Copy state for known types
  for(QAction *action : actions)
    action->setChecked(selectedTypes.contains(action->data().toString()));
  actionUnknown->setChecked(selectedUnknownType);
  userdataToolButton->setChecked(!selectedTypes.isEmpty() || selectedUnknownType);
}

void UserdataController::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA, selectedTypes);
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA_UNKNOWN, selectedUnknownType);
}

void UserdataController::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    QStringList list = atools::settings::Settings::instance().valueStrList(lnm::MAP_USERDATA);
    selectedUnknownType = atools::settings::Settings::instance().valueBool(lnm::MAP_USERDATA_UNKNOWN);

    // Remove all types from the restored list which were not found in the list of registered types
    const QStringList& availableTypes = icons->getAllTypes();
    for(const QString& type : list)
    {
      if(availableTypes.contains(type))
        selectedTypes.append(type);
    }
  }
  else
    resetSettingsToDefault();
  typesToActions();
}

void UserdataController::resetSettingsToDefault()
{
  selectedTypes.append(icons->getAllTypes());
  selectedUnknownType = true;
  typesToActions();
}

QStringList UserdataController::getAllTypes() const
{
  return icons->getAllTypes();
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
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("Userpoint added."));
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
      emit userdataChanged();
      mainWindow->setStatusMessage(tr("%n userpoint(s) updated.", "", ids.size()));
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
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("%n userpoint(s) deleted.", "", ids.size()));
  }
}

void UserdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;

  QStringList files = dialog->openFileDialogMulti(
    tr("Open Userpoint CSV File(s)"),
    tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_CSV), "Userdata/Csv");

  for(const QString& file:files)
  {
    if(!file.isEmpty())
      manager->importCsv(file, atools::fs::userdata::NONE, ',', '"');
  }

  if(!files.isEmpty())
    emit refreshUserdataSearch();
}

void UserdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::importUserFixDat()
{
  qDebug() << Q_FUNC_INFO;

}

void UserdataController::exportUserFixDat()
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
                          tr("Really delete all userpoints?\n\nThis cannot be undone."));

  if(retval == QMessageBox::Yes)
  {
    manager->clearData();
    emit refreshUserdataSearch();
  }
}
