/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "atools.h"
#include "common/constants.h"
#include "common/mapresult.h"
#include "common/maptypesfactory.h"
#include "db/undoredoprogress.h"
#include "exception.h"
#include "fs/userdata/userdatamanager.h"
#include "geo/pos.h"
#include "gui/actionbuttonhandler.h"
#include "gui/choicedialog.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "app/navapp.h"
#include "gui/sqlquerydialog.h"
#include "options/optiondata.h"
#include "search/searchcontroller.h"
#include "search/userdatasearch.h"
#include "settings/settings.h"
#include "sql/sqlcolumn.h"
#include "sql/sqlrecord.h"
#include "sql/sqltransaction.h"
#include "ui_mainwindow.h"
#include "userdata/userdatadialog.h"
#include "userdata/userdataicons.h"
#include "common/unit.h"

#include <QDebug>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QScreen>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::sql::SqlColumn;
using atools::geo::Pos;

UserdataController::UserdataController(atools::fs::userdata::UserdataManager *userdataManager, MainWindow *parent)
  : manager(userdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  buttonHandler = new atools::gui::ActionButtonHandler(mainWindow);
  icons = new UserdataIcons();
  icons->loadIcons();
  lastAddedRecord = new SqlRecord();

  connect(this, &UserdataController::userdataChanged, manager, &atools::sql::DataManagerBase::updateUndoRedoActions);

  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionSearchUserpointUndo, &QAction::triggered, this, &UserdataController::undoTriggered);
  connect(ui->actionSearchUserpointRedo, &QAction::triggered, this, &UserdataController::redoTriggered);

  manager->setMaximumUndoSteps(50);
  manager->setTextSuffix(tr("Userpoint", "Userpoint singular"), tr("Userpoints", "Userpoint plural"));
  manager->setActions(ui->actionSearchUserpointUndo, ui->actionSearchUserpointRedo);
}

UserdataController::~UserdataController()
{
  delete dialog;
  delete buttonHandler;
  delete icons;
  delete lastAddedRecord;
}

void UserdataController::undoTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(manager->canUndo())
  {
    SqlTransaction transaction(manager->getDatabase());
    UndoRedoProgress progress(mainWindow, tr("Little Navmap - Undoing Userpoint changes"), tr("Undoing changes ..."));
    manager->setProgressCallback(std::bind(&UndoRedoProgress::callback, &progress, std::placeholders::_1, std::placeholders::_2));

    progress.start();
    manager->undo();
    progress.stop();

    manager->setProgressCallback(nullptr);
    if(!progress.isCanceled())
    {
      qDebug() << Q_FUNC_INFO << "Committing";
      transaction.commit();
      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      emit userdataChanged();
    }
    else
    {
      qDebug() << Q_FUNC_INFO << "Rolling back";
      transaction.rollback();
    }
  }
}

void UserdataController::redoTriggered()
{
  qDebug() << Q_FUNC_INFO;
  if(manager->canRedo())
  {
    SqlTransaction transaction(manager->getDatabase());
    UndoRedoProgress progress(mainWindow, tr("Little Navmap - Redoing Userpoint changes"), tr("Redoing changes ..."));
    manager->setProgressCallback(std::bind(&UndoRedoProgress::callback, &progress, std::placeholders::_1, std::placeholders::_2));

    progress.start();
    manager->redo();
    progress.stop();

    manager->setProgressCallback(nullptr);
    if(!progress.isCanceled())
    {
      qDebug() << Q_FUNC_INFO << "Committing";
      transaction.commit();
      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      emit userdataChanged();
    }
    else
    {
      qDebug() << Q_FUNC_INFO << "Rolling back";
      transaction.rollback();
    }
  }
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

void UserdataController::enableCategoryOnMap(const QString& category)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "category" << category;
#endif

  if(icons->hasType(category))
  {
    if(!selectedTypes.contains(category))
      selectedTypes.append(category);
  }
  else
    selectedUnknownType = true;
  typesToActions();
}

void UserdataController::addToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  userdataToolButton = new QToolButton(ui->toolBarMapOptions);

  // Create and add toolbar button =====================================
  userdataToolButton->setIcon(QIcon(":/littlenavmap/resources/icons/userpoint_POI.svg"));
  userdataToolButton->setPopupMode(QToolButton::InstantPopup);
  userdataToolButton->setToolTip(tr("Select userpoints for display"));
  userdataToolButton->setStatusTip(userdataToolButton->toolTip());
  userdataToolButton->setCheckable(true);

  // Add tear off menu to button =======
  userdataToolButton->setMenu(new QMenu(userdataToolButton));
  QMenu *buttonMenu = userdataToolButton->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  // Insert before show route
  ui->toolBarMapOptions->insertWidget(ui->actionMapShowRoute, userdataToolButton);
  ui->toolBarMapOptions->insertSeparator(ui->actionMapShowRoute);

  // Create and add select all action =====================================
  actionAll = new QAction(tr("&All Userpoints"), buttonMenu);
  actionAll->setToolTip(tr("Toggle all / current selection of userpoints"));
  actionAll->setStatusTip(actionAll->toolTip());
  buttonMenu->addAction(actionAll);
  ui->menuViewUserpoints->addAction(actionAll);
  buttonHandler->setAllAction(actionAll);

  // Create and add select none action =====================================
  actionNone = new QAction(tr("&No Userpoints"), buttonMenu);
  actionNone->setToolTip(tr("Toggle none / current selection of userpoints"));
  actionNone->setStatusTip(actionNone->toolTip());
  buttonMenu->addAction(actionNone);
  ui->menuViewUserpoints->addAction(actionNone);
  buttonHandler->setNoneAction(actionNone);

  buttonMenu->addSeparator();
  ui->menuViewUserpoints->addSeparator();

  // Create and add select unknown action =====================================
  actionUnknown = new QAction(tr("&Unknown Types"), buttonMenu);
  actionUnknown->setToolTip(tr("Show or hide unknown userpoint types"));
  actionUnknown->setStatusTip(actionUnknown->toolTip());
  actionUnknown->setCheckable(true);
  buttonMenu->addAction(actionUnknown);
  ui->menuViewUserpoints->addAction(actionUnknown);
  ui->menuViewUserpoints->addSeparator();
  buttonHandler->addOtherAction(actionUnknown);

  buttonMenu->addSeparator();
  ui->menuViewUserpoints->addSeparator();

  // Create and add select an action for each registered type =====================================
  QMenu *menu = buttonMenu;
  int screenHeight = std::max(800, mainWindow->screen()->geometry().height());
  for(const QString& type : icons->getAllTypes())
  {
    QIcon icon(icons->getIconPath(type));
    QAction *action = new QAction(icon, type, menu);
    action->setData(QVariant(type));
    action->setCheckable(true);
    action->setToolTip(tr("Show or hide %1 userpoints").arg(type));
    action->setStatusTip(action->toolTip());
    menu->addAction(action);
    ui->menuViewUserpoints->addAction(action);
    buttonHandler->addOtherAction(action);
    actions.append(action);

    // Create an overflow menu item if the menu exceeds the screen height
    if(menu->sizeHint().height() > screenHeight * 9 / 10)
    {
      menu = menu->addMenu(tr("More ..."));
      menu->setToolTipsVisible(true);
      menu->setTearOffEnabled(true);
    }
  }

  // Add all signals from actions to the same handler method
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionAllTriggered, this, &UserdataController::toolbarActionTriggered);
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionNoneTriggered, this, &UserdataController::toolbarActionTriggered);
  connect(buttonHandler, &atools::gui::ActionButtonHandler::actionOtherTriggered, this, &UserdataController::toolbarActionTriggered);
}

void UserdataController::toolbarActionTriggered(QAction *)
{
  qDebug() << Q_FUNC_INFO;

  // Copy action state to class data
  actionsToTypes();

  // Update map
  emit userdataChanged();
}

void UserdataController::actionsToTypes()
{
  // Copy state for known types
  selectedTypes.clear();
  for(QAction *action : qAsConst(actions))
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
  for(QAction *action : qAsConst(actions))
    action->setChecked(selectedTypes.contains(action->data().toString()));
  actionUnknown->setChecked(selectedUnknownType);
  userdataToolButton->setChecked(!selectedTypes.isEmpty() || selectedUnknownType);
}

void UserdataController::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA, selectedTypes);
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA_ALL, allLastFoundTypes);
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA_UNKNOWN, selectedUnknownType);
}

void UserdataController::restoreState()
{
  const QStringList allTypes = getAllTypes();

  // Get the list of icons found the last time which allows to identify new types and enable them per default
  allLastFoundTypes = atools::settings::Settings::instance().valueStrList(lnm::MAP_USERDATA_ALL);
  if(allLastFoundTypes.isEmpty())
    allLastFoundTypes = allTypes;

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    atools::settings::Settings& settings = atools::settings::Settings::instance();

    // Get list of enabled. Enable all as default
    const QStringList list = settings.valueStrList(lnm::MAP_USERDATA, allTypes);
    selectedUnknownType = settings.valueBool(lnm::MAP_USERDATA_UNKNOWN, true);

    // Remove all types from the restored list of enabled which were not found in the new list of registered types
    // in case some were removed
    for(const QString& type : list)
    {
      if(allTypes.contains(type))
        selectedTypes.append(type);
    }

    // Now check for types that are new and not included in the last saved list and enable them
    for(const QString& type : allTypes)
    {
      if(!allLastFoundTypes.contains(type))
        selectedTypes.append(type);
    }

    allLastFoundTypes = allTypes;
  }
  else
    // Enable all
    resetSettingsToDefault();

  manager->updateUndoRedoActions();
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

void UserdataController::addUserpointFromMap(const map::MapResult& result, atools::geo::Pos pos, bool airportAddon)
{
  qDebug() << Q_FUNC_INFO;

  if(result.isEmpty(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT))
    // No prefill start empty dialog of with last added data
    addUserpointInternal(-1, pos, SqlRecord());
  else
  {
    // Prepare the dialog prefill data
    SqlRecord prefillRec = manager->getEmptyRecord();
    if(result.hasAirports())
    {
      const map::MapAirport& ap = result.airports.constFirst();

      prefillRec.appendFieldAndValue("ident", ap.displayIdent()).appendFieldAndValue("name", ap.name).
      appendFieldAndValue("region", ap.region);

      if(airportAddon)
        // Addon type to highlight add-on airports
        prefillRec.appendFieldAndValue("type", "Addon").
        appendFieldAndValue("visible_from", 5000.f /* NM */).
        appendFieldAndValueIf("tags", NavApp::getCurrentSimulatorShortName());
      else
        prefillRec.appendFieldAndValue("type", "Airport");

      pos = ap.position;
    }
    else if(result.hasVor())
    {
      const map::MapVor& vor = result.vors.constFirst();

      // Determine default type
      QString type = "VOR";
      if(vor.tacan)
        type = "TACAN";
      else if(vor.vortac)
        type = "VORTAC";
      else if(vor.dmeOnly)
        type = "DME";
      else if(vor.hasDme)
        type = "VORDME";
      else
        type = "VOR";

      prefillRec.appendFieldAndValue("ident", vor.ident)
      .appendFieldAndValue("name", map::vorText(vor))
      .appendFieldAndValue("type", type)
      .appendFieldAndValue("region", vor.region);
      pos = vor.position;
    }
    else if(result.hasNdb())
    {
      const map::MapNdb& ndb = result.ndbs.constFirst();
      prefillRec.appendFieldAndValue("ident", ndb.ident)
      .appendFieldAndValue("name", map::ndbText(ndb))
      .appendFieldAndValue("type", "NDB")
      .appendFieldAndValue("region", ndb.region);
      pos = ndb.position;
    }
    else if(result.hasWaypoints())
    {
      const map::MapWaypoint& wp = result.waypoints.constFirst();
      prefillRec.appendFieldAndValue("ident", wp.ident)
      .appendFieldAndValue("name", map::waypointText(wp))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", wp.region);
      pos = wp.position;
    }
    else if(result.hasUserpoints())
    {
      const map::MapUserpoint& up = result.userpoints.constFirst();
      prefillRec.appendFieldAndValue("ident", up.ident)
      .appendFieldAndValue("name", up.name)
      .appendFieldAndValue("type", up.type)
      .appendFieldAndValue("region", up.region)
      // .appendFieldAndValue("description", up.description)
      // .appendFieldAndValue("tags", up.tags)
      ;
      pos = up.position;
    }
    else
      prefillRec.appendFieldAndValue("type", UserdataDialog::DEFAULT_TYPE);

    prefillRec.appendFieldAndValue("altitude", pos.getAltitude());

    if(airportAddon)
      addUserpointInternalAddon(pos, prefillRec);
    else
      addUserpointInternal(-1, pos, prefillRec);
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
  manager->updateRecords(rec, {userpoint.id});
  transaction.commit();

  // No need to update search
  emit userdataChanged();
  mainWindow->setStatusMessage(tr("Userpoint moved."));
}

void UserdataController::clearTemporary()
{
  SqlTransaction transaction(manager->getDatabase());
  manager->clearTemporary();
  transaction.commit();
  manager->updateUndoRedoActions();
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

void UserdataController::editUserpointFromMap(const map::MapResult& result)
{
  qDebug() << Q_FUNC_INFO;
  editUserpoints({result.userpoints.constFirst().id});
}

void UserdataController::addUserpoint(int id, const atools::geo::Pos& pos)
{
  addUserpointInternal(id, pos, SqlRecord());
}

void UserdataController::addUserpointInternalAddon(const atools::geo::Pos& pos, const SqlRecord& rec)
{
  qDebug() << Q_FUNC_INFO;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << rec;
#endif

  *lastAddedRecord = rec;

  // Set id to null to let userdata manager select id
  if(lastAddedRecord->contains("userdata_id"))
    lastAddedRecord->setNull("userdata_id");

  lastAddedRecord->setEmptyStringsToNull();

  lastAddedRecord->appendFieldAndValue("last_edit_timestamp", atools::convertToIsoWithOffset(QDateTime::currentDateTime()));

  if(pos.isValid())
  {
    // Take coordinates for prefill if given
    lastAddedRecord->appendFieldAndValue("lonx", pos.getLonX()).appendFieldAndValue("laty", pos.getLatY());
    if(pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      lastAddedRecord->appendFieldAndValue("altitude", pos.getAltitude());
  }

  // Add to database
  SqlTransaction transaction(manager->getDatabase());
  manager->insertOneRecord(*lastAddedRecord);
  transaction.commit();

  // Enable category on map for new type
  enableCategoryOnMap(lastAddedRecord->valueStr("type"));

  emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
  emit userdataChanged();
  mainWindow->setStatusMessage(tr("Addon Userpoint added."));
}

void UserdataController::addUserpointInternal(int id, const atools::geo::Pos& pos, const SqlRecord& prefillRec)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec;

  if(id != -1 /*&& lastAddedRecord->isEmpty()*/)
    // Get prefill from given database id
    rec = manager->getRecord(id);
  else
    // Use last added dataset
    rec = *lastAddedRecord;

  if(!prefillRec.isEmpty())
    // Use given record
    rec = prefillRec;

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

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << rec;
#endif

  UserdataDialog dlg(mainWindow, ud::ADD, icons);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    *lastAddedRecord = dlg.getRecord();

    if(lastAddedRecord->contains("import_file_path"))
      lastAddedRecord->setNull("import_file_path");

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << rec;
#endif

    // Set id to null to let userdata manager select id
    if(lastAddedRecord->contains("userdata_id"))
      lastAddedRecord->setNull("userdata_id");

    lastAddedRecord->setEmptyStringsToNull();

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertOneRecord(*lastAddedRecord);
    transaction.commit();

    // Enable category on map for new type
    enableCategoryOnMap(lastAddedRecord->valueStr("type"));

    emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("Userpoint added."));
  }
  dlg.saveState();
}

void UserdataController::editUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getRecord(ids.constFirst());
  if(!rec.isEmpty())
  {
    UserdataDialog dlg(mainWindow, ids.size() > 1 ? ud::EDIT_MULTIPLE : ud::EDIT_ONE, icons);
    dlg.restoreState();

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlRecord editedRec = dlg.getRecord();
      editedRec.setEmptyStringsToNull();

      SqlTransaction transaction(manager->getDatabase());
      manager->updateRecords(editedRec, QSet<int>(ids.constBegin(), ids.constEnd()));
      transaction.commit();

      // Enable category on map if the type has changed
      if(editedRec.contains("type"))
        enableCategoryOnMap(editedRec.valueStr("type"));

      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
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

  SqlTransaction transaction(manager->getDatabase());
  manager->deleteRows(QSet<int>(ids.constBegin(), ids.constEnd()));
  transaction.commit();

  emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
  emit userdataChanged();
  mainWindow->setStatusMessage(tr("%n userpoint(s) deleted.", "", ids.size()));
}

void UserdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QStringList files = dialog->openFileDialogMulti(tr("Open Userpoint CSV File(s)"),
                                                    tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), "Userdata/Csv");
    files.removeAll(QString());

    if(!files.isEmpty())
    {
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      int numImported = manager->importCsv(files, atools::fs::userdata::NONE, ',', '"');
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();

      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      if(numImported > 0)
      {
        mainWindow->showUserpointSearch();
        manager->updateUndoRedoActions();
        emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      }
    }
  }
  catch(atools::Exception& e)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
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
      tr("X-Plane User Fix Files %1;;X-Plane Dat Files %2;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT).arg(lnm::FILE_PATTERN_DAT),
      "Userdata/UserFixDat", xplaneUserWptDatPath());

    if(!file.isEmpty())
    {
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      int numImported = manager->importXplane(file);
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();

      mainWindow->showUserpointSearch();
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      manager->updateUndoRedoActions();
      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
    }
  }
  catch(atools::Exception& e)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
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
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      int numImported = manager->importGarmin(file);
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();

      mainWindow->showUserpointSearch();
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      manager->updateUndoRedoActions();
      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
    }
  }
  catch(atools::Exception& e)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    QGuiApplication::restoreOverrideCursor();
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header, xp12;
    if(exportSelectedQuestion(selected, append, header, xp12, true /* appendAllowed */, true /* headerAllowed */, false /* xplane */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export Userpoint CSV File"),
        tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV),
        ".csv", "Userdata/Csv", QString(), QString(), append /* dont confirm overwrite */);

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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportXplaneUserFixDat()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header, xp12;
    if(exportSelectedQuestion(selected, append, header, xp12, true /* appendAllowed */, false /* headerAllowed */, true /* xplane */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export X-Plane user_fix.dat File"),
        tr("X-Plane User Fix Files %1;;X-Plane Dat Files %2;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT).arg(lnm::FILE_PATTERN_DAT),
        ".dat", "Userdata/UserFixDat", xplaneUserWptDatPath(), "user_fix.dat", append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(selected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported = manager->exportXplane(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE,
                                                xp12);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
      }
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportGarmin()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header, xp12;
    if(exportSelectedQuestion(selected, append, header, xp12, true /* appendAllowed */, false /* headerAllowed */, false /* xplane */))
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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void UserdataController::exportBglXml()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool selected, append, header, xp12;
    if(exportSelectedQuestion(selected, append, header, xp12, false /* appendAllowed */, false /* headerAllowed */, false /* xplane */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export XML File for FSX/P3D BGL Compiler"),
        tr("XML Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_BGL_XML),
        ".xml", "Userdata/BglXml", QString(), QString(), false);

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
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

QString UserdataController::xplaneUserWptDatPath()
{
  QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_12);
  if(xpBasePath.isEmpty())
    xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_11);

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
  QString gtnPath(QProcessEnvironment::systemEnvironment().value("GTNSIMDATA"));
  path = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\GTN" : gtnPath;
#elif defined(DEBUG_INFORMATION)
  path = atools::buildPath({atools::documentsDir(), "Garmin", "Trainers", "GTN"});
#else
  path = atools::documentsDir();
#endif
  return path;
}

bool UserdataController::exportSelectedQuestion(bool& selected, bool& append, bool& header, bool& xp12, bool appendAllowed,
                                                bool headerAllowed, bool xplane)
{
  int numSelected = NavApp::getUserdataSearch()->getSelectedRowCount();

  if(numSelected == 0 && !appendAllowed)
    // nothing select and not append option - do not show dialog
    return true;

  // Dialog options
  enum
  {
    SELECTED, APPEND, HEADER, XP12
  };

  atools::gui::ChoiceDialog choiceDialog(mainWindow, QApplication::applicationName() + tr(" - Userpoint Export Options"),
                                         tr("Select export options for userpoints.\n\n"
                                            "Note that the field \"Tags\" is used for the ID of the airport "
                                            "terminal area and the waypoint type.\n"
                                            "Click \"Help\" for more information."),
                                         lnm::USERDATA_EXPORT_CHOICE_DIALOG, "USERPOINT.html");
  choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

  if(appendAllowed)
    choiceDialog.addCheckBox(APPEND, tr("&Append to an already present file"),
                             tr("File header will be ignored if this is enabled."), false);
  else
    // Add a hidden dummy which still allows to save the settings to the same key/variable
    choiceDialog.addCheckBoxHidden(APPEND);

  choiceDialog.addCheckBox(SELECTED, tr("Export &selected entries only"), QString(), true,
                           numSelected == 0 /* disabled */);

  if(headerAllowed)
    choiceDialog.addCheckBox(HEADER, tr("Add a &header to the first line"), QString(), false);
  else
    choiceDialog.addCheckBoxHidden(HEADER);

  if(xplane)
    choiceDialog.addCheckBox(XP12, tr("Export for &X-Plane 12"), tr("File will be formatted for X-Plane 12 if selected.\n"
                                                                    "Otherwise X-Plane 11 format is used."), false);
  else
    choiceDialog.addCheckBoxHidden(XP12);

  choiceDialog.addSpacer();

  choiceDialog.restoreState();

  choiceDialog.getCheckBox(HEADER)->setDisabled(choiceDialog.isChecked(APPEND));
  atools::gui::ChoiceDialog *dlgPtr = &choiceDialog;
  connect(&choiceDialog, &atools::gui::ChoiceDialog::checkBoxToggled, this, [dlgPtr](int id, bool checked) {
    if(id == APPEND)
      dlgPtr->getCheckBox(HEADER)->setDisabled(checked);
  });

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    selected = choiceDialog.isChecked(SELECTED); // Only true if enabled too
    append = choiceDialog.isChecked(APPEND);
    header = choiceDialog.isChecked(HEADER) && !choiceDialog.isChecked(APPEND);
    xp12 = choiceDialog.isChecked(XP12);
    return true;
  }
  else
    return false;
}

void UserdataController::cleanupUserdata()
{
  qDebug() << Q_FUNC_INFO;

  enum
  {
    COMPARE, // Ident, Name, and Type
    REGION,
    DESCRIPTION,
    TAGS,
    COORDINATES,
    EMPTY,
    SHOW_PREVIEW
  };

  // Create a dialog with tree checkboxes =====================
  atools::gui::ChoiceDialog choiceDialog(mainWindow, QApplication::applicationName() + tr(" - Cleanup Userpoints"),
                                         tr("Select criteria for cleanup.\nNote that you can undo this change."),
                                         lnm::SEARCHTAB_USERDATA_CLEANUP_DIALOG, "USERPOINT.html#userpoint-cleanup");

  choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

  choiceDialog.addCheckBox(EMPTY, tr("Delete userpoints having no information\n"
                                     "except coordinates and type."), QString(), true);
  choiceDialog.addLine();

  choiceDialog.addLabel(tr("Remove duplicates using the additional fields below as criteria.\n"
                           "A userpoint will considered a duplicate to another if\n"
                           "all selected fields are equal."));
  choiceDialog.addCheckBox(COMPARE, tr("&Ident, Name, and Type"));
  choiceDialog.addCheckBox(REGION, tr("&Region"));
  choiceDialog.addCheckBox(DESCRIPTION, tr("&Remarks"));
  choiceDialog.addCheckBox(TAGS, tr("&Tags"));
  choiceDialog.addCheckBox(COORDINATES, tr("&Coordinates (similar)"));
  choiceDialog.addSpacer();
  choiceDialog.addLine();
  choiceDialog.addCheckBox(SHOW_PREVIEW, tr("Show a &preview before deleting userpoints"),
                           tr("Shows a dialog window with all userpoints to be deleted before removing them."), true /* checked */);

  // Disable duplicate cleanup parameters if top box is off
  connect(&choiceDialog, &atools::gui::ChoiceDialog::checkBoxToggled, [&choiceDialog](int id, bool checked) {
    if(id == COMPARE)
    {
      choiceDialog.getCheckBox(REGION)->setEnabled(checked);
      choiceDialog.getCheckBox(DESCRIPTION)->setEnabled(checked);
      choiceDialog.getCheckBox(TAGS)->setEnabled(checked);
      choiceDialog.getCheckBox(COORDINATES)->setEnabled(checked);
    }
  });

  // Disable the ok button if not at least one of these is checked
  choiceDialog.setRequiredAnyChecked({COMPARE, EMPTY});

  choiceDialog.restoreState();

  if(choiceDialog.exec() == QDialog::Accepted)
  {
    bool deleteEntries = false;

    // Create list for duplicate column check ===============================================
    // type name ident region description tags
    QStringList duplicateColumns;
    if(choiceDialog.isChecked(COMPARE))
    {
      duplicateColumns << "type" << "ident" << "name";

      if(choiceDialog.isChecked(REGION))
        duplicateColumns.append("region");
      if(choiceDialog.isChecked(DESCRIPTION))
        duplicateColumns.append("description");
      if(choiceDialog.isChecked(TAGS))
        duplicateColumns.append("tags");
    }

    // Prepare - replace null values with empty strings
    manager->preCleanup();

    // Show preview table ===============================================
    if(choiceDialog.isChecked(SHOW_PREVIEW))
    {
      // Columns to be shown in the preview
      QVector<atools::sql::SqlColumn> previewCols({
        SqlColumn("type", tr("Type")),
        SqlColumn("last_edit_timestamp", tr("Last Change")),
        SqlColumn("ident", tr("Ident")),
        SqlColumn("region", tr("Region")),
        SqlColumn("name", tr("Name")),
        SqlColumn("tags", tr("Tags")),
        SqlColumn("description", tr("Remarks")),
        SqlColumn("import_file_path", tr("Imported\nfrom File"))
      });

      // Get query for preview
      QString queryStr = manager->getCleanupPreview(duplicateColumns, choiceDialog.isChecked(COORDINATES), choiceDialog.isChecked(EMPTY),
                                                    previewCols);

      // Callback for data formatting
      atools::gui::SqlQueryDialogDataFunc dataFunc([&previewCols](int column, const QVariant& data, Qt::ItemDataRole role) -> QVariant {
                                                   if(previewCols.at(column).getName() == "last_edit_timestamp")
                                                   {
                                                     // Convert to time and align right
                                                     if(role == Qt::TextAlignmentRole)
                                                       return Qt::AlignRight;
                                                     else if(role == Qt::DisplayRole)
                                                       return data.toDateTime();
                                                   }
                                                   return data;
        });

      // Build preview dialog ===============================================
      atools::gui::SqlQueryDialog previewDialog(mainWindow, QCoreApplication::applicationName() + tr(" - Cleanup Preview"),
                                                tr("These userpoints will be deleted.\nNote that you can undo this change."),
                                                lnm::SEARCHTAB_USERDATA_CLEANUP_PREVIEW, "USERPOINT.html#cleanup-userpoints",
                                                tr("&Delete Userpoints"));
      previewDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
      previewDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());
      previewDialog.initQuery(manager->getDatabase(), queryStr, previewCols, dataFunc);
      if(previewDialog.exec() == QDialog::Accepted)
        deleteEntries = true;
    }
    else
      deleteEntries = true;

    int removed = 0;
    if(deleteEntries)
    {
      // Dialog ok - remove entries ===============================================
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      SqlTransaction transaction(manager->getDatabase());
      removed = manager->cleanupUserdata(duplicateColumns, choiceDialog.isChecked(COORDINATES), choiceDialog.isChecked(EMPTY));
      transaction.commit();
      QGuiApplication::restoreOverrideCursor();
    }

    // Undo prepare - replace empty strings with null values
    manager->postCleanup();

    // Send messages ===============================================
    if(deleteEntries)
    {
      if(removed > 0)
        emit userdataChanged();
      mainWindow->setStatusMessage(tr("%1 %2 deleted.").arg(removed).arg(removed == 1 ? tr("userpoint") : tr("userpoints")));

      emit refreshUserdataSearch(false /* loadAll */, false /* keepSelection */, true /* force */);
      emit userdataChanged();
    }
  }
}
