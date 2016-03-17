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

#include "table/navsearch.h"
#include "logging/loggingdefs.h"
#include "gui/tablezoomhandler.h"
#include "sql/sqldatabase.h"
#include "table/controller.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "table/column.h"
#include "navicondelegate.h"
#include "ui_mainwindow.h"
#include "table/columnlist.h"
#include "geo/pos.h"
#include "gui/widgettools.h"
#include "gui/widgetstate.h"
#include "table/formatter.h"
#include "common/mapcolors.h"
#include "common/maptypes.h"

#include <QMessageBox>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>

NavSearch::NavSearch(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                     atools::sql::SqlDatabase *sqlDb, int tabWidgetIndex)
  : Search(parent, tableView, columnList, sqlDb, tabWidgetIndex)
{
  Ui::MainWindow *ui = parentWidget->getUi();

  navSearchWidgets =
  {
    ui->tableViewNavSearch,
    ui->horizontalLayoutNavNameSearch,
    ui->horizontalLayoutNavExtSearch,
    ui->horizontalLayoutNavDistanceSearch,
    ui->horizontalLayoutNavScenerySearch,
    ui->lineNavDistSearch,
    ui->lineNavScenerySearch,
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  navSearchMenuActions =
  {
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  /* *INDENT-OFF* */
  connect(ui->actionNavSearchShowAllOptions, &QAction::toggled, [=](bool state)
  {
    for(QAction *a : navSearchMenuActions)
      a->setChecked(state);
  });
  /* *INDENT-ON* */

  QStringList typeCondMap;
  typeCondMap << QString()
              << "= 'HIGH'"
              << "= 'LOW'"
              << "= 'TERMINAL'"
              << "= 'HH'"
              << "= 'H'"
              << "= 'MH'"
              << "= 'COMPASS_POINT'"
              << "= 'NAMED'"
              << "= 'UNNAMED'"
              << "= 'VOR'"
              << "= 'NDB'";

  QStringList navTypeCondMap;
  navTypeCondMap << QString()
                 << "in ('VOR', 'VORDME', 'DME')"
                 << "in ('VOR', 'VORDME', 'DME', 'NDB')"
                 << "= 'VORDME'"
                 << "= 'VOR'"
                 << "= 'DME'"
                 << "= 'NDB'"
                 << "= 'WAYPOINT'";

  // Default view column descriptors
  columns->
  append(Column("nav_search_id").hidden()).
  append(Column("distance", tr("Distance")).distanceCol()).
  append(Column("ident", ui->lineEditNavIcaoSearch, tr("ICAO")).filter().defaultSort()).
  append(Column("nav_type", ui->comboBoxNavNavAidSearch, tr("Nav Aid\nType")).indexCondMap(navTypeCondMap)).
  append(Column("type", ui->comboBoxNavTypeSearch, tr("Type")).indexCondMap(typeCondMap)).
  append(Column("name", ui->lineEditNavNameSearch, tr("Name")).filter()).
  append(Column("region", ui->lineEditNavRegionSearch, tr("Region")).filter()).
  append(Column("airport_ident", ui->lineEditNavAirportIcaoSearch, tr("Airport\nICAO")).filter()).
  append(Column("frequency", tr("Frequency"))).
  append(Column("range", ui->spinBoxNavMaxRangeSearch, tr("Range")).filter().condition(">")).
  append(Column("mag_var", tr("Mag\nVar"))).
  append(Column("altitude", tr("Altitude"))).
  append(Column("scenery_local_path", ui->lineEditNavScenerySearch, tr("Scenery")).filter()).
  append(Column("bgl_filename", ui->lineEditNavFileSearch, tr("File")).filter()).
  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  // TODO delete old and new delegate
  view->setItemDelegateForColumn(2, new NavIconDelegate(columns));

  Search::initViewAndController();

  using namespace std::placeholders;

  controller->setDataCallback(std::bind(&NavSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6));
  controller->setHandlerRoles({Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole});
}

NavSearch::~NavSearch()
{
}

void NavSearch::connectSlots()
{
  Search::connectSlots();

  Ui::MainWindow *ui = parentWidget->getUi();

  // Distance
  columns->assignDistanceSearchWidgets(ui->pushButtonNavDistSearch,
                                       ui->checkBoxNavDistSearch,
                                       ui->comboBoxNavDistDirectionSearch,
                                       ui->spinBoxNavDistMinSearch,
                                       ui->spinBoxNavDistMaxSearch);

  // Connect widgets to the controller
  Search::connectSearchWidgets();
  ui->toolButtonNavSearch->addActions({ui->actionNavSearchShowAllOptions,
                                       ui->actionNavSearchShowDistOptions,
                                       ui->actionNavSearchShowSceneryOptions});

  // Drop down menu actions
  using atools::gui::WidgetTools;
  /* *INDENT-OFF* */
  connect(ui->actionNavSearchShowDistOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutNavDistanceSearch}, state, {ui->lineNavDistSearch}); });
  connect(ui->actionNavSearchShowSceneryOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutNavScenerySearch}, state, {ui->lineNavScenerySearch}); });
  /* *INDENT-ON* */
}

void NavSearch::saveState()
{
  atools::gui::WidgetState saver("SearchPaneNav/Widget");
  saver.save(navSearchWidgets);
}

void NavSearch::restoreState()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  atools::gui::WidgetState saver("SearchPaneNav/Widget");
  saver.restore(navSearchWidgets);
  ui->checkBoxNavDistSearch->setChecked(false);
}

QVariant NavSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& value,
                                     const QVariant& dataValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return modelFormatHandler(col, value, dataValue);

    case Qt::TextAlignmentRole:
      if(col->getColumnName() == "ident" || col->getColumnName() == "airport_ident" ||
         dataValue.type() == QVariant::Int || dataValue.type() == QVariant::UInt ||
         dataValue.type() == QVariant::LongLong || dataValue.type() == QVariant::ULongLong ||
         dataValue.type() == QVariant::Double)
        return Qt::AlignRight;

      break;
    case Qt::BackgroundRole:
      if(colIndex == controller->getSortColumnIndex())
        return mapcolors::alternatingRowColor(rowIndex, true);

      break;
    case Qt::CheckStateRole:
    case Qt::DecorationRole:
    case Qt::ForegroundRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::FontRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::SizeHintRole:
    case Qt::InitialSortOrderRole:
    case Qt::DisplayPropertyRole:
    case Qt::DecorationPropertyRole:
    case Qt::ToolTipPropertyRole:
    case Qt::StatusTipPropertyRole:
    case Qt::WhatsThisPropertyRole:
    case Qt::UserRole:
      break;
  }

  return QVariant();
}

QString NavSearch::modelFormatHandler(const Column *col, const QVariant& value,
                                      const QVariant& dataValue) const
{
  if(col->getColumnName() == "type")
    return maptypes::navTypeName(value.toString());
  else if(col->getColumnName() == "nav_type")
    return maptypes::navName(value.toString());
  else if(col->getColumnName() == "frequency" && !value.isNull())
  {
    double freq = value.toDouble();

    // VOR and DME are scaled up in nav_search to easily differentiate from NDB
    if(freq >= 1000000 && freq <= 1200000)
      return formatter::formatDoubleUnit(value.toDouble() / 10000., QString(), 2);
    else if(freq >= 10000 && freq <= 120000)
      return formatter::formatDoubleUnit(value.toDouble() / 100., QString(), 1);
    else
      return "Invalid";
  }
  else if(col->getColumnName() == "mag_var")
    return formatter::formatDoubleUnit(value.toDouble(), QString(), 1);
  else if(dataValue.type() == QVariant::Int || dataValue.type() == QVariant::UInt)
    return QLocale().toString(dataValue.toInt());
  else if(dataValue.type() == QVariant::LongLong || dataValue.type() == QVariant::ULongLong)
    return QLocale().toString(dataValue.toLongLong());
  else if(dataValue.type() == QVariant::Double)
    return QLocale().toString(dataValue.toDouble());

  return value.toString();
}

void NavSearch::getSelectedMapObjects(maptypes::MapSearchResult& result) const
{
  using namespace std::placeholders;
  QStringList cols;
  cols << columns->getIdColumnName() << "vor_id" << "ndb_id" << "waypoint_id"
       << "nav_type" << "lonx" << "laty";
  controller->getSelectedObjects(cols, std::bind(&NavSearch::fillSearchResult, this, _1, &result));
  result.needsDelete = true;
}

void NavSearch::fillSearchResult(const QVariantList& data, maptypes::MapSearchResult *result) const
{
  QString type = data.at(4).toString();
  if(type == "WAYPOINT")
  {
    maptypes::MapWaypoint *obj = new maptypes::MapWaypoint;
    obj->id = data.at(3).toInt();
    obj->position = atools::geo::Pos(data.at(5).toFloat(), data.at(6).toFloat());
    result->waypoints.append(obj);
  }
  else if(type == "NDB")
  {
    maptypes::MapNdb *obj = new maptypes::MapNdb;
    obj->id = data.at(2).toInt();
    obj->position = atools::geo::Pos(data.at(5).toFloat(), data.at(6).toFloat());
    result->ndbs.append(obj);
  }
  else if(type == "DME")
  {
    maptypes::MapVor *obj = new maptypes::MapVor;
    obj->id = data.at(1).toInt();
    obj->position = atools::geo::Pos(data.at(5).toFloat(), data.at(6).toFloat());
    result->vors.append(obj);
  }
  else if(type == "VORDME")
  {
    maptypes::MapVor *obj = new maptypes::MapVor;
    obj->id = data.at(1).toInt();
    obj->position = atools::geo::Pos(data.at(5).toFloat(), data.at(6).toFloat());
    result->vors.append(obj);
  }
  else if(type == "VOR")
  {
    maptypes::MapVor *obj = new maptypes::MapVor;
    obj->id = data.at(1).toInt();
    obj->position = atools::geo::Pos(data.at(5).toFloat(), data.at(6).toFloat());
    result->vors.append(obj);
  }
}
