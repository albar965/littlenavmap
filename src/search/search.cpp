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

#include "search/search.h"
#include "logging/loggingdefs.h"
#include "gui/tablezoomhandler.h"
#include "sql/sqldatabase.h"
#include "search/controller.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "search/column.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "geo/pos.h"
#include "mapgui/mapwidget.h"
#include "geo/calculations.h"
#include "atools.h"

#include <QMessageBox>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>
#include <QTimer>
#include <QClipboard>
#include <QStandardItemModel>

#include "gui/actiontextsaver.h"
#include <export/csvexporter.h>

#include "mapgui/mapquery.h"

const int DISTANCE_EDIT_UPDATE_TIMEOUT_MS = 600;

Search::Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
               MapQuery *mapQuery, int tabWidgetIndex)
  : QObject(parent), query(mapQuery), columns(columnList), view(tableView), mainWindow(parent),
    tabIndex(tabWidgetIndex)
{
  Ui::MainWindow *ui = mainWindow->getUi();
  // Avoid stealing of Ctrl - C from other default menus
  ui->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchResetSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  boolIcon = new QIcon(":/littlenavmap/resources/icons/checkmark.svg");

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionSearchTableCopy, &QAction::triggered, this, &Search::tableCopyClipboard);

  ui->dockWidgetSearch->addActions({ui->actionSearchResetSearch, ui->actionSearchShowAll});

  tableView->addActions({ui->actionSearchTableCopy, ui->actionSearchShowInformation,
                         ui->actionSearchShowOnMap});

  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &Search::editTimeout);

  connect(ui->actionSearchShowInformation, &QAction::triggered, this, &Search::showInformationMenu);
  connect(ui->actionSearchShowOnMap, &QAction::triggered, this, &Search::showOnMapMenu);
}

Search::~Search()
{
  delete boolIcon;
  delete csvExporter;
  delete updateTimer;
}

void Search::tableCopyClipboard()
{
  if(view->isVisible())
  {
    QString csv;
    int exported = CsvExporter::selectionAsCsv(view, true, csv);

    if(!csv.isEmpty())
      QApplication::clipboard()->setText(csv);

    mainWindow->getUi()->statusBar->showMessage(
      QString(tr("Copied %1 entries to clipboard.")).arg(exported));
  }
}

void Search::initViewAndController()
{
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);
  controller = new Controller(mainWindow, query->getDatabase(), columns, view);
  controller->prepareModel();

  csvExporter = new CsvExporter(mainWindow, controller);
}

void Search::filterByIdent(const QString& ident, const QString& region, const QString& airportIdent)
{
  controller->filterByIdent(ident, region, airportIdent);
}

void Search::markChanged(const atools::geo::Pos& mark)
{
  mapMark = mark;
  qDebug() << "new mark" << mark;
  if(columns->getDistanceCheckBox()->isChecked() && mark.isValid())
  {
    QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
    QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();

    controller->filterByDistance(mapMark,
                                 static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->
                                                                             currentIndex()),
                                 minDistanceWidget->value(), maxDistanceWidget->value());
    controller->loadAllRowsForRectQuery();
  }
}

void Search::connectSearchWidgets()
{
  void (QComboBox::*curIndexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;

  for(const Column *col : columns->getColumns())
  {
    /* *INDENT-OFF* */
    if(col->getLineEditWidget() != nullptr)
    {
      connect(col->getLineEditWidget(), &QLineEdit::textChanged, [=](const QString &text)
      {
        controller->filterByLineEdit(col, text);
        editStarted();
      });
    }
    else if(col->getComboBoxWidget() != nullptr)
    {
      connect(col->getComboBoxWidget(), curIndexChangedPtr, [=](int index)
      {
        controller->filterByComboBox(col, index, index == 0);
        editStarted();
      });
    }
    else if(col->getCheckBoxWidget() != nullptr)
    {
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged, [=](int state)
      {
        controller->filterByCheckbox(col, state, col->getCheckBoxWidget()->isTristate());
        editStarted();
      });
    }
    else if(col->getSpinBoxWidget() != nullptr)
    {
      connect(col->getSpinBoxWidget(), valueChangedPtr, [=](int value)
      {
        controller->filterBySpinBox(col, value);
        editStarted();
      });
    }
    else if(col->getMinSpinBoxWidget() != nullptr && col->getMaxSpinBoxWidget() != nullptr)
    {
      connect(col->getMinSpinBoxWidget(), valueChangedPtr, [=](int value)
      {
        controller->filterByMinMaxSpinBox(col, value, col->getMaxSpinBoxWidget()->value());
        editStarted();
      });

      connect(col->getMaxSpinBoxWidget(), valueChangedPtr, [=](int value)
      {
        controller->filterByMinMaxSpinBox(col, col->getMinSpinBoxWidget()->value(), value);
        editStarted();
      });
    }
    /* *INDENT-ON* */
  }

  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();
  QCheckBox *distanceCheckBox = columns->getDistanceCheckBox();

  if(minDistanceWidget != nullptr && maxDistanceWidget != nullptr &&
     distanceDirWidget != nullptr && distanceCheckBox != nullptr)
  {
    /* *INDENT-OFF* */
    connect(distanceCheckBox, &QCheckBox::stateChanged, [=](int state)
    {
      controller->filterByDistance(
            state == Qt::Checked ? mapMark : atools::geo::Pos(),
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            minDistanceWidget->value(), maxDistanceWidget->value());

      minDistanceWidget->setEnabled(state == Qt::Checked);
      maxDistanceWidget->setEnabled(state == Qt::Checked);
      distanceDirWidget->setEnabled(state == Qt::Checked);
      if(state == Qt::Checked)
        controller->loadAllRowsForRectQuery();
    });

    connect(minDistanceWidget, valueChangedPtr, [=](int value)
    {
      controller->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            value, maxDistanceWidget->value());
      maxDistanceWidget->setMinimum(value > 10 ? value : 10);
      editStarted();
    });

    connect(maxDistanceWidget, valueChangedPtr, [=](int value)
    {
      controller->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            minDistanceWidget->value(), value);
      minDistanceWidget->setMaximum(value);
      editStarted();
    });

    connect(distanceDirWidget, curIndexChangedPtr, [=](int index)
    {
      controller->filterByDistanceUpdate(static_cast<sqlproxymodel::SearchDirection>(index),
                                                minDistanceWidget->value(),
                                                maxDistanceWidget->value());
      editStarted();
    });
    /* *INDENT-ON* */
  }
}

void Search::editStarted()
{
  qDebug() << "editStarted";
  updateTimer->start(DISTANCE_EDIT_UPDATE_TIMEOUT_MS);
}

void Search::editTimeout()
{
  qDebug() << "editTimeout";
  controller->loadAllRowsForRectQuery();
}

void Search::connectSlots()
{
  connect(view, &QTableView::doubleClicked, this, &Search::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &Search::contextMenu);

  Ui::MainWindow *ui = mainWindow->getUi();

  connect(ui->actionSearchShowAll, &QAction::triggered, this, &Search::loadAllRowsIntoView);
  connect(ui->actionSearchResetSearch, &QAction::triggered, this, &Search::resetSearch);

  connectModelSlots();
}

void Search::connectModelSlots()
{
  reconnectSelectionModel();

  connect(controller->getModel(), &SqlModel::modelReset, this, &Search::reconnectSelectionModel);
  void (Search::*selChangedPtr)() = &Search::tableSelectionChanged;
  connect(controller->getModel(), &SqlModel::fetchedMore, this, selChangedPtr);
}

void Search::reconnectSelectionModel()
{
  void (Search::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &Search::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);
}

void Search::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);

  tableSelectionChanged();
}

void Search::tableSelectionChanged()
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  emit selectionChanged(this, selectedRows, controller->getVisibleRowCount(), controller->getTotalRowCount());
}

void Search::preDatabaseLoad()
{
  saveState();
  controller->preDatabaseLoad();
}

void Search::postDatabaseLoad()
{
  controller->postDatabaseLoad();
  restoreState();
}

void Search::resetView()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    atools::gui::Dialog dlg(mainWindow);
    int result = dlg.showQuestionMsgBox("Actions/ShowResetView",
                                        tr("Reset sort order, column order and column sizes to default?"),
                                        tr("Do not &show this dialog again."),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::Yes, QMessageBox::Yes);

    if(result == QMessageBox::Yes)
    {
      controller->resetView();
      mainWindow->getUi()->statusBar->showMessage(tr("View reset to default."));
    }
  }
}

void Search::resetSearch()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetSearch();
    mainWindow->getUi()->statusBar->showMessage(tr("Search filters cleared."));
  }
}

void Search::loadAllRowsIntoView()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->loadAllRows();
    mainWindow->getUi()->statusBar->showMessage(tr("All logbook entries read."));
  }
}

void Search::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
  {
    // Check if the used table has bounding rectangle columns
    bool hasBounding = columns->hasColumn("left_lonx") &&
                       columns->hasColumn("top_laty") &&
                       columns->hasColumn("right_lonx") &&
                       columns->hasColumn("bottom_laty");

    if(hasBounding)
    {
      float leftLon = controller->getRawData(index.row(), "left_lonx").toFloat();
      float topLat = controller->getRawData(index.row(), "top_laty").toFloat();
      float rightLon = controller->getRawData(index.row(), "right_lonx").toFloat();
      float bottomLat = controller->getRawData(index.row(), "bottom_laty").toFloat();

      if(atools::almostEqual(leftLon, rightLon) && atools::almostEqual(topLat, bottomLat))
      {
        atools::geo::Pos p(leftLon, topLat);
        qDebug() << "emit showPos" << p;
        emit showPos(p, 2700);
      }
      else
      {
        atools::geo::Rect r(leftLon, topLat, rightLon, bottomLat);
        qDebug() << "emit showRect" << r;
        emit showRect(r);
      }
    }
    else
    {
      atools::geo::Pos p(controller->getRawData(index.row(), "lonx").toFloat(),
                         controller->getRawData(index.row(), "laty").toFloat());
      qDebug() << "emit showPos" << p;
      emit showPos(p, 2700);
    }

    maptypes::MapObjectTypes navType = maptypes::NONE;
    int id = -1;
    getNavTypeAndId(index.row(), navType, id);

    maptypes::MapSearchResult result;
    query->getMapObjectById(result, navType, id);

    emit showInformation(result);
  }
}

void Search::contextMenu(const QPoint& pos)
{
  QObject *localSender = sender();
  qDebug() << localSender->metaObject()->className() << localSender->objectName();

  Ui::MainWindow *ui = mainWindow->getUi();
  QString header, fieldData = "Data";
  bool columnCanFilter = false, columnCanGroup = false;
  maptypes::MapObjectTypes navType = maptypes::NONE;
  int id = -1;

  atools::gui::ActionTextSaver saver({ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
                                      ui->actionRouteAirportDest, ui->actionRouteAirportStart,
                                      ui->actionRouteAdd});
  Q_UNUSED(saver);

  atools::geo::Pos position;
  QModelIndex index = controller->getModelIndexAt(pos);
  if(index.isValid())
  {
    const Column *columnDescriptor = columns->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();
    columnCanGroup = columnDescriptor->isGroup();

    if(columnCanGroup)
    {
      header = controller->getHeaderNameAt(index);
      Q_ASSERT(!header.isNull());
      // strip LF and other from header name
      header.replace("-\n", "").replace("\n", " ");
    }

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = controller->getFieldDataAt(index);

    // Check if the used table has all that is needed to display a navaid range ring
    position = atools::geo::Pos(controller->getRawData(index.row(), "lonx").toFloat(),
                                controller->getRawData(index.row(), "laty").toFloat());

    getNavTypeAndId(index.row(), navType, id);
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Add data to menu item text
  ui->actionSearchFilterIncluding->setText(ui->actionSearchFilterIncluding->text().
                                           arg("\"" + fieldData + "\""));
  ui->actionSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionSearchFilterExcluding->setText(ui->actionSearchFilterExcluding->text().
                                           arg("\"" + fieldData + "\""));
  ui->actionSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionMapNavaidRange->setEnabled(navType == maptypes::VOR || navType == maptypes::NDB);

  ui->actionRouteAdd->setEnabled(navType == maptypes::VOR || navType == maptypes::NDB ||
                                 navType == maptypes::WAYPOINT || navType == maptypes::AIRPORT);

  ui->actionRouteAirportDest->setEnabled(navType == maptypes::AIRPORT);
  ui->actionRouteAirportStart->setEnabled(navType == maptypes::AIRPORT);

  ui->actionMapRangeRings->setEnabled(index.isValid());
  ui->actionMapHideRangeRings->setEnabled(!mainWindow->getMapWidget()->getRangeRings().isEmpty());

  ui->actionSearchSetMark->setEnabled(index.isValid());

  ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));
  ui->actionRouteAdd->setText(tr("Add to Flight Plan"));
  ui->actionRouteAirportStart->setText(tr("Set as Flight Plan Departure"));
  ui->actionRouteAirportDest->setText(tr("Set as Flight Plan Destination"));

  ui->actionSearchTableCopy->setEnabled(index.isValid());
  ui->actionSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);

  // Build the menu
  QMenu menu;
  menu.addAction(ui->actionSearchShowInformation);
  menu.addAction(ui->actionSearchShowOnMap);
  menu.addSeparator();

  menu.addAction(ui->actionSearchFilterIncluding);
  menu.addAction(ui->actionSearchFilterExcluding);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionSearchShowAll);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideRangeRings);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteAirportDest);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAdd);
  menu.addSeparator();

  menu.addAction(ui->actionSearchTableCopy);
  menu.addAction(ui->actionSearchTableSelectAll);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetView);
  menu.addSeparator();

  menu.addAction(ui->actionSearchSetMark);

  QAction *action = menu.exec(QCursor::pos());
  if(action != nullptr)
  {
    // A menu item was selected
    // Other actions with shortcuts are connectied directly to methods
    if(action == ui->actionSearchResetView)
      resetView();
    else if(action == ui->actionSearchTableCopy)
      tableCopyClipboard();
    else if(action == ui->actionSearchFilterIncluding)
      controller->filterIncluding(index);
    else if(action == ui->actionSearchFilterExcluding)
      controller->filterExcluding(index);
    else if(action == ui->actionSearchTableSelectAll)
      controller->selectAll();
    else if(action == ui->actionSearchSetMark)
      emit changeMark(controller->getGeoPos(index));
    else if(action == ui->actionMapRangeRings)
      mainWindow->getMapWidget()->addRangeRing(position);
    else if(action == ui->actionMapNavaidRange)
    {
      int frequency = controller->getRawData(index.row(), "frequency").toInt();
      if(navType == maptypes::VOR)
        // Adapt scaled frequency from nav_search table
        frequency /= 10;

      mainWindow->getMapWidget()->addNavRangeRing(position, navType,
                                                  controller->getRawData(index.row(), "ident").toString(),
                                                  frequency,
                                                  controller->getRawData(index.row(), "range").toInt());
    }
    else if(action == ui->actionMapHideRangeRings)
      mainWindow->getMapWidget()->clearRangeRings();
    else if(action == ui->actionRouteAdd)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, -1);
    else if(action == ui->actionRouteAirportStart)
    {
      maptypes::MapAirport ap;
      query->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetStart(ap);
    }
    else if(action == ui->actionRouteAirportDest)
    {
      maptypes::MapAirport ap;
      query->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDest(ap);
    }
  }
}

void Search::showInformationMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    maptypes::MapObjectTypes navType = maptypes::NONE;
    int id = -1;
    getNavTypeAndId(index.row(), navType, id);

    maptypes::MapSearchResult result;
    query->getMapObjectById(result, navType, id);
    emit showInformation(result);
  }
}

void Search::showOnMapMenu()
{
  if(view->isVisible())
  {
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      maptypes::MapObjectTypes navType = maptypes::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      maptypes::MapSearchResult result;
      query->getMapObjectById(result, navType, id);

      if(!result.airports.isEmpty() && !result.airports.first().bounding.isPoint())
        emit showRect(result.airports.first().bounding);
      else if(!result.vors.isEmpty())
        emit showPos(result.vors.first().getPosition(), 2700);
      else if(!result.ndbs.isEmpty())
        emit showPos(result.ndbs.first().getPosition(), 2700);
      else if(!result.waypoints.isEmpty())
        emit showPos(result.waypoints.first().getPosition(), 2700);
    }
  }
}

void Search::getNavTypeAndId(int row, maptypes::MapObjectTypes& navType, int& id)
{
  navType = maptypes::NONE;
  id = -1;

  if(columns->getTablename() == "airport")
  {
    navType = maptypes::AIRPORT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else
  {
    navType = maptypes::navTypeToMapObjectType(controller->getRawData(row, "nav_type").toString());

    if(navType == maptypes::VOR)
      id = controller->getRawData(row, "vor_id").toInt();
    else if(navType == maptypes::NDB)
      id = controller->getRawData(row, "ndb_id").toInt();
    else if(navType == maptypes::WAYPOINT)
      id = controller->getRawData(row, "waypoint_id").toInt();
  }
}
