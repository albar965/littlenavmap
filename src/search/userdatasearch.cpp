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

#include "search/userdatasearch.h"

#include "navapp.h"
#include "common/maptypes.h"
#include "common/mapflags.h"
#include "common/constants.h"
#include "search/sqlcontroller.h"
#include "search/column.h"
#include "search/usericondelegate.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "atools.h"
#include "common/maptypesfactory.h"
#include "userdata/userdatacontroller.h"
#include "sql/sqlrecord.h"
#include "userdata/userdataicons.h"

UserdataSearch::UserdataSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("userdata", "userdata_id"), tabWidgetIndex)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // All widgets that will have their state and visibility saved and restored
  userdataSearchWidgets =
  {
    ui->horizontalLayoutUserdata,
    ui->horizontalLayoutUserdataMore,
    ui->lineUserdataMore,
    ui->actionUserdataSearchShowMoreOptions
  };

  // All drop down menu actions
  userdataSearchMenuActions =
  {
    ui->actionUserdataSearchShowMoreOptions
  };

  UserdataIcons *icons = NavApp::getUserdataIcons();

  int size = ui->comboBoxUserdataType->fontMetrics().height();
  for(const QString& type : icons->getAllTypes())
    ui->comboBoxUserdataType->addItem(QIcon(*icons->getIconPixmap(type, size)), type);
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
  append(Column("name", ui->lineEditUserdataName, tr("Name")).filter()).
  append(Column("tags", ui->lineEditUserdataTags, tr("Tags")).filter()).
  append(Column("description", ui->lineEditUserdataDescription, tr("Description")).filter()).
  append(Column("temp").hidden()).

  append(Column("visible_from", tr("Visible from\n%dist%")).convertFunc(Unit::distNmF)).
  append(Column("altitude", tr("Elevation\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("import_file_path", ui->lineEditUserdataFilepath, tr("Imported\nfrom File")).filter()).

  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  ui->labelUserdataOverride->hide();

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

void UserdataSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(overrideColumnTitles.isEmpty())
  {
    ui->labelUserdataOverride->hide();
    ui->labelUserdataOverride->clear();
  }
  else
  {
    ui->labelUserdataOverride->show();
    ui->labelUserdataOverride->setText(tr("%1 overriding all other search options.").
                                       arg(overrideColumnTitles.join(" and ")));
  }
}

void UserdataSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  Ui::MainWindow *ui = NavApp::getMainUi();

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
  connect(ui->actionUserdataSearchShowMoreOptions, &QAction::toggled, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutUserdataMore}, state,
                                              {ui->lineUserdataMore});
    updateButtonMenu();
  });

  ui->actionUserdataEdit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionUserdataAdd->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionUserdataDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  ui->tableViewUserdata->addActions({ui->actionUserdataEdit, ui->actionUserdataAdd, ui->actionUserdataDelete});

  connect(controller->getSqlModel(), &SqlModel::overrideMode, this, &UserdataSearch::overrideMode);

  connect(ui->pushButtonUserdataEdit, &QPushButton::clicked, this, &UserdataSearch::editUserpointsTriggered);
  connect(ui->actionUserdataEdit, &QAction::triggered, this, &UserdataSearch::editUserpointsTriggered);

  connect(ui->pushButtonUserdataDel, &QPushButton::clicked, this, &UserdataSearch::deleteUserpointsTriggered);
  connect(ui->actionUserdataDelete, &QAction::triggered, this, &UserdataSearch::deleteUserpointsTriggered);

  connect(ui->pushButtonUserdataAdd, &QPushButton::clicked, this, &UserdataSearch::addUserpointTriggered);
  connect(ui->actionUserdataAdd, &QAction::triggered, this, &UserdataSearch::addUserpointTriggered);
}

void UserdataSearch::addUserpointTriggered()
{
  QVector<int> ids = getSelectedIds();
  emit addUserpoint(ids.isEmpty() ? -1 : ids.first(), atools::geo::EMPTY_POS);
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

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgetState.save({ui->horizontalLayoutUserdata, ui->horizontalLayoutUserdataMore});
}

void UserdataSearch::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    atools::gui::WidgetState widgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET);
    widgetState.restore(userdataSearchWidgets);

    restoreViewState(false);

    // Need to block signals here to avoid unwanted behavior (will enable
    // distance search and avoid saving of wrong view widget state)
    widgetState.setBlockSignals(true);
    Ui::MainWindow *ui = NavApp::getMainUi();
    widgetState.restore({ui->horizontalLayoutUserdata, ui->horizontalLayoutUserdataMore});
  }
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).restore(NavApp::getMainUi()->tableViewUserdata);
}

void UserdataSearch::saveViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).save(NavApp::getMainUi()->tableViewUserdata);
}

void UserdataSearch::restoreViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_USERDATA_VIEW_WIDGET).restore(NavApp::getMainUi()->tableViewUserdata);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant UserdataSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                          const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(roleValue);

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
           Unit::altFeet(displayRoleValue.toFloat(), false) : QString();
  else if(col->getColumnName() == "last_edit_timestamp")
    return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);
  else if(col->getColumnName() == "description")
    return displayRoleValue.toString().simplified();
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void UserdataSearch::getSelectedMapObjects(map::MapSearchResult& result) const
{
  if(!NavApp::getMainUi()->dockWidgetSearch->isVisible())
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
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::util::changeStarIndication(ui->actionUserdataSearchShowMoreOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->horizontalLayoutUserdataMore}));
}

void UserdataSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->pushButtonUserdataClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonUserdataDel->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonUserdataEdit->setEnabled(sm != nullptr && sm->hasSelection());

  // Update actions and keys too
  ui->actionUserdataEdit->setEnabled(sm != nullptr && sm->hasSelection());
  ui->actionUserdataDelete->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *UserdataSearch::followModeAction()
{
  return NavApp::getMainUi()->actionSearchUserdataFollowSelection;
}
