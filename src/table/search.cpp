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

#include "table/search.h"
#include "logging/loggingdefs.h"
#include "gui/tablezoomhandler.h"
#include "sql/sqldatabase.h"
#include "table/controller.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "table/column.h"
#include "ui_mainwindow.h"
#include "table/columnlist.h"
#include "geo/pos.h"
#include "mapgui/navmapwidget.h"

#include <QMessageBox>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>

#include <gui/actiontextsaver.h>

#include <mapgui/mapquery.h>

Search::Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
               MapQuery *query, int tabWidgetIndex)
  : QObject(parent), mapQuery(query), columns(columnList), view(tableView), parentWidget(parent),
    tabIndex(tabWidgetIndex)
{

  Ui::MainWindow *ui = parentWidget->getUi();
  // Avoid stealing of Ctrl-C from other default menus
  ui->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchResetSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  boolIcon = new QIcon(":/littlenavmap/resources/icons/checkmark.svg");

  tableView->addActions({ui->actionSearchResetSearch, ui->actionSearchShowAll});
}

Search::~Search()
{
  delete boolIcon;
}

void Search::initViewAndController()
{
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);
  controller = new Controller(parentWidget, mapQuery->getDatabase(), columns, view);
  controller->prepareModel();
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
      {controller->filterByLineEdit(col, text); });

      connect(col->getLineEditWidget(), &QLineEdit::editingFinished,
              controller, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getComboBoxWidget() != nullptr)
    {
      connect(col->getComboBoxWidget(), curIndexChangedPtr, [=](int index)
      {controller->filterByComboBox(col, index, index == 0); });

      connect(col->getComboBoxWidget(), curIndexChangedPtr,
              controller, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getCheckBoxWidget() != nullptr)
    {
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged, [=](int state)
      {controller->filterByCheckbox(col, state, col->getCheckBoxWidget()->isTristate()); });

      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged,
              controller, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getSpinBoxWidget() != nullptr)
    {
      connect(col->getSpinBoxWidget(), valueChangedPtr, [=](int value)
      {controller->filterBySpinBox(col, value); });

      connect(col->getSpinBoxWidget(), &QSpinBox::editingFinished,
              controller, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getMinSpinBoxWidget() != nullptr && col->getMaxSpinBoxWidget() != nullptr)
    {
      connect(col->getMinSpinBoxWidget(), valueChangedPtr, [=](int value)
      {controller->filterByMinMaxSpinBox(col, value, col->getMaxSpinBoxWidget()->value()); });

      connect(col->getMinSpinBoxWidget(), &QSpinBox::editingFinished,
              controller, &Controller::loadAllRowsForRectQuery);

      connect(col->getMaxSpinBoxWidget(), valueChangedPtr, [=](int value)
      {controller->filterByMinMaxSpinBox(col, col->getMinSpinBoxWidget()->value(), value); });

      connect(col->getMaxSpinBoxWidget(), &QSpinBox::editingFinished,
              controller, &Controller::loadAllRowsForRectQuery);
    }
    /* *INDENT-ON* */
  }

  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();
  QCheckBox *distanceCheckBox = columns->getDistanceCheckBox();
  QPushButton *distanceUpdate = columns->getDistanceUpdateButton();

  if(minDistanceWidget != nullptr && maxDistanceWidget != nullptr &&
     distanceDirWidget != nullptr && distanceCheckBox != nullptr && distanceUpdate != nullptr)
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
      distanceUpdate->setEnabled(state == Qt::Checked);
      if(state == Qt::Checked)
        controller->loadAllRowsForRectQuery();
    });

    connect(minDistanceWidget, valueChangedPtr, [=](int value)
    {
      controller->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            value, maxDistanceWidget->value());
      maxDistanceWidget->setMinimum(value > 10 ? value : 10);
    });

    connect(maxDistanceWidget, valueChangedPtr, [=](int value)
    {
      controller->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            minDistanceWidget->value(), value);
      minDistanceWidget->setMaximum(value);
    });

    connect(distanceDirWidget, curIndexChangedPtr, [=](int index)
    {
      controller->filterByDistanceUpdate(static_cast<sqlproxymodel::SearchDirection>(index),
                                                minDistanceWidget->value(),
                                                maxDistanceWidget->value());
      controller->loadAllRowsForRectQuery();
    });

    connect(distanceUpdate, &QPushButton::clicked, controller, &Controller::loadAllRowsForRectQuery);
    connect(minDistanceWidget, &QSpinBox::editingFinished, controller, &Controller::loadAllRowsForRectQuery);
    connect(maxDistanceWidget, &QSpinBox::editingFinished, controller, &Controller::loadAllRowsForRectQuery);
    /* *INDENT-ON* */
  }
}

void Search::connectSlots()
{
  connect(view, &QTableView::doubleClicked, this, &Search::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &Search::contextMenu);

  Ui::MainWindow *ui = parentWidget->getUi();

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
  // controller->resetSearch();
  controller->clearModel();
}

void Search::postDatabaseLoad()
{
  controller->prepareModel();
  connectModelSlots();


  // TODO fix after reload database
  // connectControllerSlots();
  // assignSearchFieldsToController();

  // updateWidgetsOnSelection();
  // updateWidgetStatus();
  // updateGlobalStats();
}

void Search::resetView()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    atools::gui::Dialog dlg(parentWidget);
    int result = dlg.showQuestionMsgBox("Actions/ShowResetView",
                                        tr("Reset sort order, column order and column sizes to default?"),
                                        tr("Do not &show this dialog again."),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::Yes, QMessageBox::Yes);

    if(result == QMessageBox::Yes)
    {
      controller->resetView();
      parentWidget->getUi()->statusBar->showMessage(tr("View reset to default."));
    }
  }
}

void Search::resetSearch()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetSearch();
    parentWidget->getUi()->statusBar->showMessage(tr("Search filters cleared."));
  }
}

void Search::tableCopyCipboard()
{
  // QString rows;
  int exported = 0; // csvExporter->exportSelectedToString(&rows);
  // QApplication::clipboard()->setText(rows);
  parentWidget->getUi()->statusBar->showMessage(
    QString(tr("Copied %1 logbook entries to clipboard.")).arg(exported));
}

void Search::loadAllRowsIntoView()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->loadAllRows();
    parentWidget->getUi()->statusBar->showMessage(tr("All logbook entries read."));
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

      if(leftLon == rightLon && topLat == bottomLat)
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
  }
}

void Search::contextMenu(const QPoint& pos)
{
  QObject *localSender = sender();
  qDebug() << localSender->metaObject()->className() << localSender->objectName();

  Ui::MainWindow *ui = parentWidget->getUi();
  QString header, fieldData = "Data";
  bool columnCanFilter = false, columnCanGroup = false;
  maptypes::MapObjectTypes navType = maptypes::NONE;
  bool isAirport = false;

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

    navType = maptypes::navTypeToMapObjectType(controller->getRawData(index.row(), "nav_type").toString());

    isAirport = columns->getTablename() == "airport";
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
                                 navType == maptypes::WAYPOINT || isAirport);

  ui->actionRouteAirportDest->setEnabled(isAirport);
  ui->actionRouteAirportStart->setEnabled(isAirport);

  ui->actionMapRangeRings->setEnabled(index.isValid());
  ui->actionMapHideRangeRings->setEnabled(!parentWidget->getMapWidget()->getRangeRings().isEmpty());

  ui->actionSearchSetMark->setEnabled(index.isValid());

  // Build the menu
  QMenu menu;

  menu.addAction(ui->actionSearchSetMark);
  menu.addSeparator();

  menu.addAction(ui->actionSearchTableCopy);
  ui->actionSearchTableCopy->setEnabled(index.isValid());

  menu.addAction(ui->actionSearchTableSelectAll);
  ui->actionSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);

  ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));
  ui->actionRouteAdd->setText(tr("Add to Route"));
  ui->actionRouteAirportStart->setText(tr("Set as Route Start"));
  ui->actionRouteAirportDest->setText(tr("Set as Route Destination"));

  menu.addSeparator();
  menu.addAction(ui->actionSearchResetView);
  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionSearchShowAll);

  menu.addSeparator();
  menu.addAction(ui->actionSearchFilterIncluding);
  menu.addAction(ui->actionSearchFilterExcluding);

  menu.addSeparator();
  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideRangeRings);

  menu.addSeparator();
  menu.addAction(ui->actionRouteAdd);
  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteAirportDest);

  QAction *action = menu.exec(QCursor::pos());
  if(action != nullptr)
  {
    // A menu item was selected
    // Other actions with shortcuts are connectied directly to methods
    if(action == ui->actionSearchResetView)
      resetView();
    else if(action == ui->actionSearchTableCopy)
      tableCopyCipboard();
    else if(action == ui->actionSearchFilterIncluding)
      controller->filterIncluding(index);
    else if(action == ui->actionSearchFilterExcluding)
      controller->filterExcluding(index);
    else if(action == ui->actionSearchTableSelectAll)
      controller->selectAll();
    else if(action == ui->actionSearchSetMark)
      emit changeMark(controller->getGeoPos(index));
    else if(action == ui->actionMapRangeRings)
      parentWidget->getMapWidget()->addRangeRing(position);
    else if(action == ui->actionMapNavaidRange)
    {
      int frequency = controller->getRawData(index.row(), "frequency").toInt();
      if(navType == maptypes::VOR)
        // Adapt scaled frequency from nav_search table
        frequency /= 10;

      parentWidget->getMapWidget()->addNavRangeRing(position, navType,
                                                    controller->getRawData(index.row(), "ident").toString(),
                                                    frequency,
                                                    controller->getRawData(index.row(), "range").toInt());
    }
    else if(action == ui->actionMapHideRangeRings)
      parentWidget->getMapWidget()->clearRangeRings();
    else if(action == ui->actionRouteAdd)
    {
      int id = -1;
      if(isAirport)
        emit routeAdd(controller->getIdForRow(index), atools::geo::EMPTY_POS, maptypes::AIRPORT, -1);
      else if(navType == maptypes::VOR)
        id = controller->getRawData(index.row(), "vor_id").toInt();
      else if(navType == maptypes::NDB)
        id = controller->getRawData(index.row(), "ndb_id").toInt();
      else if(navType == maptypes::WAYPOINT)
        id = controller->getRawData(index.row(), "waypoint_id").toInt();

      if(id != -1)
        emit routeAdd(id, atools::geo::EMPTY_POS, navType, -1);
    }
    else if(action == ui->actionRouteAirportStart)
    {
      maptypes::MapAirport ap;
      mapQuery->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetStart(ap);
    }
    else if(action == ui->actionRouteAirportDest)
    {
      maptypes::MapAirport ap;
      mapQuery->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDest(ap);
    }
    // else if(a == ui->actionTableCopy) this is alread covered by the connected action (view->setAction())
  }
}
