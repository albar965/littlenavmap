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

#include "userdata/userdatacontroller.h"

#include "fs/userdata/userdatamanager.h"
#include "common/constants.h"
#include "sql/sqlrecord.h"
#include "navapp.h"
#include "atools.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "userdata/userdatadialog.h"
#include "search/searchcontroller.h"
#include "userdata/userdataicons.h"
#include "settings/settings.h"
#include "search/userdatasearch.h"
#include "sql/sqltransaction.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "common/unit.h"
#include "common/maptypesfactory.h"
#include "gui/choicedialog.h"

#include <QDebug>
#include <QStandardPaths>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::geo::Pos;

UserdataController::UserdataController(atools::fs::userdata::UserdataManager *userdataManager, MainWindow *parent)
  : manager(userdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  icons = new UserdataIcons();
  icons->loadIcons();
  lastAddedRecord = new SqlRecord();
}

UserdataController::~UserdataController()
{
  delete dialog;
  delete icons;
  delete lastAddedRecord;
}

void UserdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_USER);
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
  button->setIcon(QIcon(":/littlenavmap/resources/icons/userpoint_POI.svg"));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(tr("Select userpoints for display"));
  button->setStatusTip(button->toolTip());
  button->setCheckable(true);
  userdataToolButton = button;

  // Insert before show route
  ui->toolbarMapOptions->insertWidget(ui->actionMapShowRoute, button);
  ui->toolbarMapOptions->insertSeparator(ui->actionMapShowRoute);

  // Create and add select all action =====================================
  actionAll = new QAction(tr("&All"), button);
  actionAll->setToolTip(tr("Show all userpoints"));
  actionAll->setStatusTip(actionAll->toolTip());
  button->addAction(actionAll);
  ui->menuViewUserpoints->addAction(actionAll);
  connect(actionAll, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select none action =====================================
  actionNone = new QAction(tr("&None"), button);
  actionNone->setToolTip(tr("Hide all userpoints"));
  actionNone->setStatusTip(actionNone->toolTip());
  button->addAction(actionNone);
  ui->menuViewUserpoints->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select unknown action =====================================
  actionUnknown = new QAction(tr("&Unknown Types"), button);
  actionUnknown->setToolTip(tr("Show or hide unknown userpoint types"));
  actionUnknown->setStatusTip(actionUnknown->toolTip());
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
    action->setToolTip(tr("Show or hide %1 userpoints").arg(type));
    action->setStatusTip(action->toolTip());
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
    atools::settings::Settings& settings = atools::settings::Settings::instance();

    // Enable all as default
    QStringList list = settings.valueStrList(lnm::MAP_USERDATA, getAllTypes());
    selectedUnknownType = settings.valueBool(lnm::MAP_USERDATA_UNKNOWN, true);

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

void UserdataController::addUserpointFromMap(const map::MapSearchResult& result, atools::geo::Pos pos)
{
  qDebug() << Q_FUNC_INFO;
  if(result.isEmpty(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT))
    // No prefill start empty dialog of with last added data
    addUserpoint(-1, pos);
  else
  {
    // Prepare the dialog prefill data
    SqlRecord prefill = manager->getEmptyRecord();
    if(result.hasAirports())
    {
      const map::MapAirport& ap = result.airports.first();

      prefill.appendFieldAndValue("ident", ap.ident)
      .appendFieldAndValue("name", ap.name)
      .appendFieldAndValue("type", "Airport")
      .appendFieldAndValue("region", ap.region);
      pos = ap.position;
    }
    else if(result.hasVor())
    {
      const map::MapVor& vor = result.vors.first();
      prefill.appendFieldAndValue("ident", vor.ident)
      .appendFieldAndValue("name", map::vorText(vor))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", vor.region);
      pos = vor.position;
    }
    else if(result.hasNdb())
    {
      const map::MapNdb& ndb = result.ndbs.first();
      prefill.appendFieldAndValue("ident", ndb.ident)
      .appendFieldAndValue("name", map::ndbText(ndb))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", ndb.region);
      pos = ndb.position;
    }
    else if(result.hasWaypoints())
    {
      const map::MapWaypoint& wp = result.waypoints.first();
      prefill.appendFieldAndValue("ident", wp.ident)
      .appendFieldAndValue("name", map::waypointText(wp))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", wp.region);
      pos = wp.position;
    }
    else
      prefill.appendFieldAndValue("type", UserdataDialog::DEFAULT_TYPE);

    prefill.appendFieldAndValue("altitude", pos.getAltitude());

    addUserpointInternal(-1, pos, prefill);
  }
}

void UserdataController::deleteUserpointFromMap(int id)
{
  deleteUserpoints({id});
}

void UserdataController::moveUserpointFromMap(const map::MapUserpoint& userpoint)
{
  SqlRecord rec;
  rec.appendFieldAndValue("lonx", userpoint.position.getLonX());
  rec.appendFieldAndValue("laty", userpoint.position.getLatY());

  SqlTransaction transaction(manager->getDatabase());

  // Change coordinate columns for id
  manager->updateByRecord(rec, {userpoint.id});
  transaction.commit();

  // No need to update search
  emit userdataChanged();
  mainWindow->setStatusMessage(tr("Userpoint moved."));
}

void UserdataController::backup()
{
  manager->backup();
}

void UserdataController::clearTemporary()
{
  manager->clearTemporary();
}

map::MapUserpoint UserdataController::getUserpointById(int id)
{
  map::MapUserpoint obj;
  MapTypesFactory().fillUserdataPoint(manager->getRecord(id), obj);
  return obj;
}

void UserdataController::setMagDecReader(atools::fs::common::MagDecReader *magDecReader)
{
  manager->setMagDecReader(magDecReader);
}

void UserdataController::editUserpointFromMap(const map::MapSearchResult& result)
{
  qDebug() << Q_FUNC_INFO;
  editUserpoints({result.userpoints.first().id});
}

void UserdataController::addUserpoint(int id, const atools::geo::Pos& pos)
{
  addUserpointInternal(id, pos, SqlRecord());
}

void UserdataController::addUserpointInternal(int id, const atools::geo::Pos& pos, const SqlRecord& prefill)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec;

  if(id != -1 /*&& lastAddedRecord->isEmpty()*/)
    // Get prefill from given database id
    rec = manager->getRecord(id);
  else
    // Use last added dataset
    rec = *lastAddedRecord;

  if(!prefill.isEmpty())
    // Use given record
    rec = prefill;

  if(rec.isEmpty())
    // Otherwise fill nothing
    rec = manager->getEmptyRecord();

  if(rec.valueStr("type").isEmpty())
    rec.setValue("type", UserdataDialog::DEFAULT_TYPE);

  if(pos.isValid())
  {
    // Take coordinates for prefill if given
    rec.appendFieldAndValue("lonx", pos.getLonX()).appendFieldAndValue("laty", pos.getLatY());
    if(pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      rec.appendFieldAndValue("altitude", pos.getAltitude());
  }

  qDebug() << Q_FUNC_INFO << rec;

  UserdataDialog dlg(mainWindow, ud::ADD, icons);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    *lastAddedRecord = dlg.getRecord();

    if(lastAddedRecord->contains("import_file_path"))
      lastAddedRecord->setNull("import_file_path");

    qDebug() << Q_FUNC_INFO << rec;

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertByRecord(*lastAddedRecord);
    transaction.commit();
    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("Userpoint added."));
  }
  dlg.saveState();
}

void UserdataController::editUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getRecord(ids.first());
  if(!rec.isEmpty())
  {
    UserdataDialog dlg(mainWindow, ids.size() > 1 ? ud::EDIT_MULTIPLE : ud::EDIT_ONE, icons);
    dlg.restoreState();

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(dlg.getRecord(), ids);
      transaction.commit();

      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
      emit userdataChanged();
      mainWindow->setStatusMessage(tr("%n userpoint(s) updated.", "", ids.size()));
    }
    dlg.saveState();
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void UserdataController::deleteUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  int retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          (ids.size() == 1 ? tr("Delete userpoint?") : tr("Delete %1 userpoints?").arg(ids.size())) +
                          tr("\n\nThis cannot be undone."), QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

  if(retval == QMessageBox::Yes)
  {
    SqlTransaction transaction(manager->getDatabase());
    manager->removeRows(ids);
    transaction.commit();

    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("%n userpoint(s) deleted.", "", ids.size()));
  }
}

void UserdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {

    QStringList files = dialog->openFileDialogMulti(
      tr("Open Userpoint CSV File(s)"),
      tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), "Userdata/Csv");

    int numImported = 0;
    for(const QString& file:files)
    {
      if(!file.isEmpty())
        numImported += manager->importCsv(file, atools::fs::userdata::NONE, ',', '"');
    }

    if(!files.isEmpty())
    {
      mainWindow->showUserpointSearch();
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::importXplaneUserFixDat()
{
  qDebug() << Q_FUNC_INFO;
  try
  {

    QString file = dialog->openFileDialog(
      tr("Open X-Plane user_fix.dat File"),
      tr("X-Plane User Fix Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT), "Userdata/UserFixDat",
      xplaneUserWptDatPath());

    if(!file.isEmpty())
    {
      int numImported = manager->importXplane(file);
      mainWindow->showUserpointSearch();
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::importGarmin()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString file = dialog->openFileDialog(
      tr("Open Garmin User Waypoint File"),
      tr("Garmin User Waypoint Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_WPT), "Userdata/UserWptDat",
      garminGtnUserWptPath());

    if(!file.isEmpty())
    {
      int numImported = manager->importGarmin(file);
      mainWindow->showUserpointSearch();
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header;
    if(exportSelectedQuestion(selected, append, header, true /* append allowed */, true /* header allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export Userpoint CSV File"),
        tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV),
        ".csv",
        "Userdata/Csv", QString(), QString(), append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        atools::fs::userdata::Flags flags = atools::fs::userdata::NONE;
        flags.setFlag(atools::fs::userdata::APPEND, append);
        flags.setFlag(atools::fs::userdata::CSV_HEADER, header);

        QVector<int> ids;
        if(selected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported = manager->exportCsv(file, ids, flags);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportXplaneUserFixDat()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header;
    if(exportSelectedQuestion(selected, append, header, true /* append allowed */, false /* header allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export X-Plane user_fix.dat File"),
        tr("X-Plane User Fix Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT),
        ".dat",
        "Userdata/UserFixDat",
        xplaneUserWptDatPath(), "user_fix.dat", append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(selected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportXplane(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportGarmin()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header;
    if(exportSelectedQuestion(selected, append, header, true /* append allowed */, false /* header allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export Garmin User Waypoint File"),
        tr("Garmin User Waypoint Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_WPT),
        ".dat",
        "Userdata/UserWptDat",
        xplaneUserWptDatPath(), "user.wpt", append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(selected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportGarmin(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportBglXml()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header;
    if(exportSelectedQuestion(selected, append, header, false /* append allowed */, false /* header allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export XML File for FSX/P3D BGL Compiler"),
        tr("XML Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_BGL_XML),
        ".xml",
        "Userdata/BglXml", QString(), QString(), false, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(selected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportBgl(file, ids);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::clearDatabase()
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Really delete all userpoints?\n\n"
                             "A backup will be created in\n"
                             "\"%1\"\n"
                             "before deleting.")
                          .arg(atools::settings::Settings::instance().getPath()),
                          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

  if(retval == QMessageBox::Yes)
  {
    manager->clearData();
    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
  }
}

QString UserdataController::xplaneUserWptDatPath()
{
  QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
  if(xpBasePath.isEmpty())
    xpBasePath = atools::documentsDir();
  else
    xpBasePath = atools::buildPathNoCase({xpBasePath, "Custom Data"});
  return xpBasePath;
}

QString UserdataController::garminGtnUserWptPath()
{
  QString path;
#ifdef Q_OS_WIN32
  QString gtnPath(qgetenv("GTNSIMDATA"));
  path = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\GTN" : gtnPath;
#elif DEBUG_INFORMATION
  path = atools::buildPath({atools::documentsDir(), "Garmin", "Trainers", "GTN"});
#else
  path = atools::documentsDir();
#endif
  return path;
}

bool UserdataController::exportSelectedQuestion(bool& selected, bool& append, bool& header, bool appendAllowed,
                                                bool headerAllowed)
{
  int numSelected = NavApp::getUserdataSearch()->getSelectedRowCount();

  if(numSelected == 0 && !appendAllowed)
    // nothing select and not append option - do not show dialog
    return true;

  enum
  {
    SELECTED, APPEND, HEADER
  };

  ChoiceDialog choiceDialog(mainWindow, QApplication::applicationName() + tr(" - Userpoint Export Options"),
                            QString(), tr("Select export options"),
                            lnm::USERDATA_EXPORT_CHOICE_DIALOG, "USERPOINT.html");

  if(appendAllowed)
    choiceDialog.add(APPEND, tr("&Append to an already present file"), QString(), false);
  else
    // Add a hidden dummy which still allows to save the settings to the same key/variable
    choiceDialog.addHidden(APPEND);

  choiceDialog.add(SELECTED, tr("Export &selected entries only"), QString(), true, numSelected == 0 /* disabled */);

  if(headerAllowed)
    choiceDialog.add(HEADER, tr("Add a &header to the first line"), QString(), false);
  else
    choiceDialog.addHidden(HEADER);

  choiceDialog.restoreState();

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    selected = choiceDialog.isChecked(SELECTED); // Only true if enabled too
    append = choiceDialog.isChecked(APPEND);
    header = choiceDialog.isChecked(HEADER);
    return true;
  }
  else
    return false;
}
