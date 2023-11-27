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

#include "search/userdatasearch.h"

#include "atools.h"
#include "common/constants.h"
#include "common/mapcolors.h"
#include "common/mapflags.h"
#include "common/mapresult.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/unit.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "app/navapp.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/sqlcontroller.h"
#include "search/usericondelegate.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "userdata/userdataicons.h"

UserdataSearch::UserdataSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("userdata", "userdata_id"), tabWidgetIndex)
{
  /* *INDENT-OFF* */
  ui->pushButtonNavHelpSearch->setToolTip(
    "<p>All set search conditions have to match.</p>"
    "<p>Search tips for text fields: </p>"
    "<ul>"
      "<li>Default is search for userpoints that contain the entered text.</li>"
      "<li>Use &quot;*&quot; as a placeholder for any text. </li>"
      "<li>Prefix with &quot;-&quot; as first character to negate search.</li>"
      "<li>Use double quotes like &quot;TAU&quot; or &quot;Sindal&quot; to force exact search.</li>"
      "<li>Only ident field: Enter a space separated list of idents to look for more than one navaid.</li>"
    "</ul>");
  /* *INDENT-ON* */

  // All widgets that will have their state and visibility saved and restored
  userdataSearchWidgets =
  {
    ui->horizontalLayoutUserdata,
    ui->horizontalLayoutUserdataMore,
    ui->lineUserdataMore,
    ui->actionUserdataSearchShowMoreOptions,
    ui->actionSearchUserdataFollowSelection
  };

  // All drop down menu actions
  userdataSearchMenuActions =
  {
    ui->actionUserdataSearchShowMoreOptions
  };

  UserdataIcons *icons = NavApp::getUserdataIcons();

  int size = ui->comboBoxUserdataType->fontMetrics().height();
  for(const QString& type : icons->getAllTypes())
    ui->comboBoxUserdataType->addItem(QIcon(*icons->getIconPixmap(type, size - 2)), type);
  ui->comboBoxUserdataType->lineEdit()->setPlaceholderText(tr("Type"));
  ui->comboBoxUserdataType->lineEdit()->setClearButtonEnabled(true);

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("userdata_id").hidden()).
  append(Column("type", ui->comboBoxUserdataType, tr("Type")).filter()).
  append(Column("last_edit_timestamp", tr("Last Change")).defaultSort().defaultSortOrder(Qt::DescendingOrder)).
  append(Column("ident", ui->lineEditUserdataIdent, tr("Ident")).filter()).
  append(Column("region", ui->lineEditUserdataRegion, tr("Region")).filter()).
  append(Column("name", ui->lineEditUserdataName, tr("Name")).filter(true, ui->actionUserdataSearchShowMoreOptions)).
  append(Column("tags", ui->lineEditUserdataTags, tr("Tags")).filter(true, ui->actionUserdataSearchShowMoreOptions)).
  append(Column("description", ui->lineEditUserdataDescription, tr("Remarks")).filter(true, ui->actionUserdataSearchShowMoreOptions)).
  append(Column("temp").hidden()).
  append(Column("visible_from", tr("Visible from\n%dist%")).convertFunc(Unit::distNmF)).
  append(Column("lonx", tr("Longitude"))).
  append(Column("laty", tr("Latitude"))).
  append(Column("altitude", tr("Elevation\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("import_file_path", ui->lineEditUserdataFilepath,
                tr("Imported\nfrom File")).filter(true, ui->actionUserdataSearchShowMoreOptions))
  ;

  // Add icon delegate for the ident column
  iconDelegate = new UserIconDelegate(columns, NavApp::getUserdataIcons());
  view->setItemDelegateForColumn(columns->getColumn("type")->getIndex(), iconDelegate);

  SearchBaseTable::initViewAndController(NavApp::getDatabaseUser());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

UserdataSearch::~UserdataSearch()
{
  delete iconDelegate;
}

void UserdataSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // Small push buttons on top
  connect(ui->pushButtonUserdataClearSelection, &QPushButton::clicked,
          this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonUserdataReset, &QPushButton::clicked, this, &SearchBaseTable::resetSearch);

  installEventFilterForWidget(ui->lineEditUserdataDescription);
  installEventFilterForWidget(ui->lineEditUserdataFilepath);
  installEventFilterForWidget(ui->lineEditUserdataIdent);
  installEventFilterForWidget(ui->lineEditUserdataName);
  installEventFilterForWidget(ui->lineEditUserdataTags);
  installEventFilterForWidget(ui->comboBoxUserdataType);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
  ui->toolButtonUserdata->addActions({ui->actionUserdataSearchShowMoreOptions});

  // Drop down menu actions
  connect(ui->actionUserdataSearchShowMoreOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutUserdataMore, ui->lineUserdataMore, state, false /* distanceSearch */);
  });

  ui->actionUserdataEdit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionUserdataAdd->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionUserdataDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionUserdataCleanup->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  ui->tableViewUserdata->addActions({ui->actionUserdataEdit, ui->actionUserdataAdd, ui->actionUserdataDelete, ui->actionUserdataCleanup});

  connect(ui->pushButtonUserdataEdit, &QPushButton::clicked, this, &UserdataSearch::editUserpointsTriggered);
  connect(ui->actionUserdataEdit, &QAction::triggered, this, &UserdataSearch::editUserpointsTriggered);

  connect(ui->pushButtonUserdataDel, &QPushButton::clicked, this, &UserdataSearch::deleteUserpointsTriggered);
  connect(ui->actionUserdataDelete, &QAction::triggered, this, &UserdataSearch::deleteUserpointsTriggered);

  connect(ui->pushButtonUserdataAdd, &QPushButton::clicked, this, &UserdataSearch::addUserpointTriggered);
  connect(ui->actionUserdataAdd, &QAction::triggered, this, &UserdataSearch::addUserpointTriggered);

  connect(ui->actionUserdataCleanup, &QAction::triggered, this, &UserdataSearch::cleanupUserdata);
}

void UserdataSearch::addUserpointTriggered()
{
  QVector<int> ids = getSelectedIds();
  emit addUserpoint(ids.isEmpty() ? -1 : ids.constFirst(), atools::geo::EMPTY_POS);
}

void UserdataSearch::editUserpointsTriggered()
{
  emit editUserpoints(getSelectedIds());
}

void UserdataSearch::deleteUserpointsTriggered()
{
  emit deleteUserpoints(getSelectedIds());
}

void UserdataSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET);
  widgetState.save(userdataSearchWidgets);
}

void UserdataSearch::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !NavApp::isSafeMode())
  {
    widgetState.restore(userdataSearchWidgets);

    if(!widgetState.contains(ui->comboBoxUserdataType))
    {
      // Have to reset to invalid and empty text on first startup to avoid index 0 ("Addon")
      ui->comboBoxUserdataType->clearEditText();
      ui->comboBoxUserdataType->setCurrentIndex(-1);
    }
  }
  else
  {
    QList<QObject *> objList;
    atools::convertList(objList, userdataSearchMenuActions);
    widgetState.restore(objList);

    atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).restore(ui->tableViewUserdata);
    ui->comboBoxUserdataType->setCurrentIndex(0);
    ui->comboBoxUserdataType->clearEditText();
  }

  finishRestore();
}

void UserdataSearch::saveViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).save(ui->tableViewUserdata);
}

void UserdataSearch::restoreViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).restore(ui->tableViewUserdata);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant UserdataSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                                          const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::TextAlignmentRole:
      if(col->getColumnName() == "ident" ||
         displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt ||
         displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong ||
         displayRoleValue.type() == QVariant::Double)
        // Align all numeric columns right
        return Qt::AlignRight;

      break;
    case Qt::BackgroundRole:
      if(colIndex == controller->getSortColumnIndex())
        // Use another alternating color if this is a field in the sort column
        return mapcolors::alternatingRowColor(rowIndex, true);

      break;
    case Qt::ToolTipRole:
      if(col->getColumnName() == "description")
        return atools::elideTextLinesShort(displayRoleValue.toString(), 40);

      break;
    default:
      break;
  }

  return QVariant();
}

/* Formats the QVariant to a QString depending on column name */
QString UserdataSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  // Called directly by the model for export functions
  if(col->getColumnName() == "altitude")
    return !displayRoleValue.isNull() && displayRoleValue.toFloat() < map::INVALID_ALTITUDE_VALUE ?
           Unit::altFeet(displayRoleValue.toFloat(), false /* addUnit */, false /* narrow */, 1.f) : QString();
  else if(col->getColumnName() == "visible_from")
    return !displayRoleValue.isNull() && displayRoleValue.toFloat() < map::INVALID_DISTANCE_VALUE ?
           Unit::distNm(displayRoleValue.toFloat(), false /* addUnit */, false /* narrow */, 1.f) : QString();
  else if(col->getColumnName() == "lonx")
    return Unit::coordsLonX(atools::geo::Pos(displayRoleValue.toFloat(), 0.f));
  else if(col->getColumnName() == "laty")
    return Unit::coordsLatY(atools::geo::Pos(0.f, displayRoleValue.toFloat()));
  else if(col->getColumnName() == "last_edit_timestamp")
    return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);
  else if(col->getColumnName() == "description")
    return atools::elideTextShort(displayRoleValue.toString().simplified(), 80);
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void UserdataSearch::getSelectedMapObjects(map::MapResult& result) const
{
  if(!ui->dockWidgetSearch->isVisible())
    return;

  // Build a SQL record with all available fields
  atools::sql::SqlRecord rec;
  controller->initRecord(rec);
  // qDebug() << Q_FUNC_INFO << rec;

  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      if(controller->hasRow(row))
      {
        controller->fillRecord(row, rec);
        // qDebug() << Q_FUNC_INFO << rec;

        map::MapUserpoint obj;
        MapTypesFactory().fillUserdataPoint(rec, obj);
        result.userpoints.append(obj);
      }
    }
  }
}

void UserdataSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void UserdataSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&UserdataSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole, Qt::ToolTipRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void UserdataSearch::updateButtonMenu()
{
  atools::gui::util::changeIndication(ui->actionUserdataSearchShowMoreOptions,
                                      atools::gui::util::anyWidgetChanged({ui->horizontalLayoutUserdataMore}));
}

void UserdataSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  ui->pushButtonUserdataClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonUserdataDel->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonUserdataEdit->setEnabled(sm != nullptr && sm->hasSelection());

  // Update actions and keys too
  ui->actionUserdataEdit->setEnabled(sm != nullptr && sm->hasSelection());
  ui->actionUserdataDelete->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *UserdataSearch::followModeAction()
{
  return ui->actionSearchUserdataFollowSelection;
}
