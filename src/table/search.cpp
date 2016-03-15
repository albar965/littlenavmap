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

#include <QMessageBox>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>

Search::Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
               atools::sql::SqlDatabase *sqlDb, int tabWidgetIndex)
  : QObject(parent), db(sqlDb), columns(columnList), view(tableView), parentWidget(parent),
    tabIndex(tabWidgetIndex)
{

  // Avoid stealing of Ctrl-C from other default menus
  parentWidget->getUi()->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  boolIcon = new QIcon(":/littlenavmap/resources/icons/checkmark.svg");
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
  controller = new Controller(parentWidget, db, columns, view);
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
  connect(view, &QTableView::customContextMenuRequested, this, &Search::tableContextMenu);

  reconnectSelectionModel();

  connect(controller->getModel(), &SqlModel::modelReset, this, &Search::reconnectSelectionModel);

  void (Search::*selChangedPtr)() = &Search::tableSelectionChanged;
  connect(controller->getModel(), &SqlModel::fetchedMore, this, selChangedPtr);
}

void Search::reconnectSelectionModel()
{
  void (Search::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &Search::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, selChangedPtr);
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

void Search::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
  {
    int id = controller->getIdForRow(index);

    QSqlRecord rec = db->record(columns->getTablename());

    bool hasBounding = rec.contains("left_lonx") &&
                       rec.contains("top_laty") &&
                       rec.contains("right_lonx") &&
                       rec.contains("bottom_laty");

    atools::sql::SqlQuery query(db);
    if(hasBounding)
      // Has bounding rectangle
      query.prepare("select left_lonx, top_laty, right_lonx, bottom_laty "
                    "from " + columns->getTablename() +
                    " where " + columns->getIdColumnName() + " = :id");

    else
      query.prepare("select lonx, laty from " + columns->getTablename() +
                    " where " + columns->getIdColumnName() + " = :id");

    query.bindValue(":id", id);
    query.exec();

    if(query.next())
    {
      if(hasBounding)
      {
        qDebug() << "emit showRect";
        emit showRect(atools::geo::Rect(query.value("left_lonx").toFloat(),
                                        query.value("top_laty").toFloat(),
                                        query.value("right_lonx").toFloat(),
                                        query.value("bottom_laty").toFloat()));
      }
      else
      {
        qDebug() << "emit showPos";
        emit showPos(atools::geo::Pos(query.value("lonx").toFloat(),
                                      query.value("laty").toFloat()), 2700);
      }
    }
    qDebug() << "id" << id;
  }
}

void Search::preDatabaseLoad()
{
  // controller->resetSearch();
  controller->clearModel();
}

void Search::postDatabaseLoad()
{
  controller->prepareModel();
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
  controller->loadAllRows();
  parentWidget->getUi()->statusBar->showMessage(tr("All logbook entries read."));
}

void Search::tableContextMenu(const QPoint& pos)
{
  QObject *localSender = sender();
  qDebug() << localSender->metaObject()->className() << localSender->objectName();

  Ui::MainWindow *ui = parentWidget->getUi();
  QString header, fieldData;
  bool columnCanFilter = false, columnCanGroup = false;

  QModelIndex index = controller->getModelIndexAt(pos);
  if(index.isValid())
  {
    const Column *columnDescriptor = controller->getColumn(index.column());
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
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Build the menu
  QMenu menu;

  menu.addAction(ui->actionSearchSetMark);
  menu.addSeparator();

  menu.addAction(ui->actionSearchTableCopy);
  ui->actionSearchTableCopy->setEnabled(index.isValid());

  menu.addAction(ui->actionSearchTableSelectAll);
  ui->actionSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);

  menu.addSeparator();
  menu.addAction(ui->actionSearchResetView);
  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionSearchShowAll);

  QString actionFilterIncludingText, actionFilterExcludingText;
  actionFilterIncludingText = ui->actionSearchFilterIncluding->text();
  actionFilterExcludingText = ui->actionSearchFilterExcluding->text();

  // Add data to menu item text
  ui->actionSearchFilterIncluding->setText(ui->actionSearchFilterIncluding->text().arg(
                                             fieldData));
  ui->actionSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionSearchFilterExcluding->setText(ui->actionSearchFilterExcluding->text().arg(
                                             fieldData));
  ui->actionSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  menu.addSeparator();
  menu.addAction(ui->actionSearchFilterIncluding);
  menu.addAction(ui->actionSearchFilterExcluding);
  menu.addSeparator();

  QAction *a = menu.exec(QCursor::pos());
  if(a != nullptr)
  {
    // A menu item was selected
    if(a == ui->actionSearchResetSearch)
      resetSearch();
    else if(a == ui->actionSearchResetView)
      resetView();
    else if(a == ui->actionSearchTableCopy)
      tableCopyCipboard();
    else if(a == ui->actionSearchShowAll)
      loadAllRowsIntoView();
    else if(a == ui->actionSearchFilterIncluding)
      controller->filterIncluding(index);
    else if(a == ui->actionSearchFilterExcluding)
      controller->filterExcluding(index);
    else if(a == ui->actionSearchTableSelectAll)
      controller->selectAll();
    else if(a == ui->actionSearchSetMark)
      emit changeMark(controller->getGeoPos(index));
    // else if(a == ui->actionTableCopy) this is alread covered by the connected action (view->setAction())
  }

  // Restore old menu texts
  ui->actionSearchFilterIncluding->setText(actionFilterIncludingText);
  ui->actionSearchFilterIncluding->setEnabled(true);

  ui->actionSearchFilterExcluding->setText(actionFilterExcludingText);
  ui->actionSearchFilterExcluding->setEnabled(true);
}
