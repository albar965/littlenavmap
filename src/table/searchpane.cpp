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

#include "table/searchpane.h"
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

SearchPane::SearchPane(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                       atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), columns(columnList), view(tableView), parentWidget(parent)
{
}

SearchPane::~SearchPane()
{

}

void SearchPane::initViewAndController()
{
  view->horizontalHeader()->setSectionsMovable(true);

  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);
  controller = new Controller(parentWidget, db, columns, view);
  controller->prepareModel();
}

void SearchPane::markChanged(const atools::geo::Pos& mark)
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

void SearchPane::connectSearchWidgets()
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

void SearchPane::connectSlots()
{
  connect(view, &QTableView::doubleClicked,
          this, &SearchPane::doubleClick);
  connect(view, &QTableView::customContextMenuRequested,
          this, &SearchPane::tableContextMenu);
}

void SearchPane::doubleClick(const QModelIndex& index)
{
  qDebug() << "double click";

  qDebug() << "total" << controller->getTotalRowCount() << "visible"
           << controller->getVisibleRowCount();

  if(index.isValid())
  {
    int id = controller->getIdForRow(index);

    atools::sql::SqlQuery query(db);
    query.prepare("select lonx, laty from " + columns->getTablename() +
                  " where " + columns->getIdColumnName() + " = :id");
    query.bindValue(":id", id);
    query.exec();
    if(query.next())
      emit showPoint(query.value("lonx").toDouble(), query.value("laty").toDouble(), 2700);
    qDebug() << "id" << id;
  }
}

void SearchPane::preDatabaseLoad()
{
  controller->resetSearch();
  controller->clearModel();
}

void SearchPane::postDatabaseLoad()
{
  controller->prepareModel();
  // connectControllerSlots();
  // assignSearchFieldsToController();

  // updateWidgetsOnSelection();
  // updateWidgetStatus();
  // updateGlobalStats();
}

void SearchPane::resetView()
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

void SearchPane::resetSearch()
{
  controller->resetSearch();
  parentWidget->getUi()->statusBar->showMessage(tr("Search filters cleared."));
}

void SearchPane::tableCopyCipboard()
{
  // QString rows;
  int exported = 0; // csvExporter->exportSelectedToString(&rows);
  // QApplication::clipboard()->setText(rows);
  parentWidget->getUi()->statusBar->showMessage(
    QString(tr("Copied %1 logbook entries to clipboard.")).arg(exported));
}

void SearchPane::loadAllRowsIntoView()
{
  controller->loadAllRows();
  parentWidget->getUi()->statusBar->showMessage(tr("All logbook entries read."));
}
