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

#include "export/csvexporter.h"

#include "common/constants.h"
#include "gui/errorhandler.h"
#include "search/sqlmodel.h"
#include "gui/dialog.h"
#include "sql/sqlexport.h"
#include "search/sqlcontroller.h"
#include "options/optiondata.h"

#include "sql/sqlrecord.h"

#include <QDebug>
#include <QFile>
#include <QTextCodec>
#include <QIODevice>
#include <QHeaderView>
#include <QTableView>

using atools::gui::ErrorHandler;
using atools::gui::Dialog;
using atools::sql::SqlQuery;
using atools::sql::SqlExport;

CsvExporter::CsvExporter(QWidget *parent, SqlController *controllerParam)
  : Exporter(parent, controllerParam)
{
}

CsvExporter::~CsvExporter()
{
}

QString CsvExporter::saveCsvFileDialog()
{
  return dialog->saveFileDialog(tr("Export CSV Document"),
                                tr("CSV Documents (*.csv);;All Files (*)"),
                                "csv", lnm::EXPORT_FILEDIALOG,
                                QString(), QString(), false);
}

int CsvExporter::selectionAsCsv(QTableView *view, bool header, bool rows, QString& result,
                                const QStringList& additionalHeader,
                                std::function<QStringList(int index)> additionalFields,
                                std::function<QVariant(int row, int col)> dataCallback)
{
  int exported = 0;
  atools::sql::SqlExport exporter;
  exporter.setSeparatorChar(';');
  exporter.setEndline(false);

  QAbstractItemModel *model = view->model();

  QItemSelectionModel *selection = view->selectionModel();
  if(selection != nullptr && selection->hasSelection())
  {
    if(rows)
    {
      // Copy full selected rows ==========================================
      QTextStream stream(&result, QIODevice::WriteOnly);
      QHeaderView *headerView = view->horizontalHeader();
      if(header)
        stream << buildHeader(view, exporter, additionalHeader, additionalFields) << endl;

      for(QItemSelectionRange rng : selection->selection())
      {
        // Add data
        for(int row = rng.top(); row <= rng.bottom(); ++row)
        {
          QVariantList vars;
          for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
          {
            // Convert view position to model position - needed to keep order
            int logicalCol = headerView->logicalIndex(viewCol);

            if(logicalCol == -1)
              continue;

            if(!view->isColumnHidden(logicalCol) &&
               view->columnWidth(logicalCol) > view->horizontalHeader()->minimumSectionSize())
            {
              if(dataCallback)
                vars.append(dataCallback(row, logicalCol));
              else
                vars.append(model->data(model->index(row, logicalCol)));
            }
          }

          stream << exporter.getResultSetRow(vars) +
          (additionalHeader.isEmpty() || !additionalFields ? QString() : ";" + additionalFields(row).join(";")) << endl;

          exported++;
        }
      }
      stream.flush();
    }
    else
    {
      // Copy selected fields - not a real CSV ==========================================
      QStringList resultList;
      for(const QModelIndex& index : selection->selectedIndexes())
      {
        if(dataCallback)
          resultList.append(dataCallback(index.row(), index.column()).toString());
        else
          resultList.append(model->data(index).toString());
        exported++;
      }
      result = resultList.join(";");
    }
  }
  return exported;
}

int CsvExporter::tableAsCsv(QTableView *view, bool header, QString& result,
                            const QStringList& additionalHeader,
                            std::function<QStringList(int index)> additionalFields)
{
  int exported = 0;
  atools::sql::SqlExport exporter;
  exporter.setSeparatorChar(';');
  exporter.setEndline(false);

  QAbstractItemModel *model = view->model();

  // Copy full rows
  QTextStream stream(&result, QIODevice::WriteOnly);
  QHeaderView *headerView = view->horizontalHeader();
  if(header)
    stream << buildHeader(view, exporter, additionalHeader, additionalFields) << endl;

  for(int row = 0; row < model->rowCount(); row++)
  {
    QVariantList vars;
    for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
    {
      // Convert view position to model position - needed to keep order
      int logicalCol = headerView->logicalIndex(viewCol);

      if(logicalCol == -1)
        continue;

      if(!view->isColumnHidden(logicalCol) &&
         view->columnWidth(logicalCol) > view->horizontalHeader()->minimumSectionSize())
        vars.append(model->data(model->index(row, logicalCol)));
    }

    stream << exporter.getResultSetRow(vars) +
    (additionalHeader.isEmpty() || !additionalFields ? QString() : ";" + additionalFields(row).join(";")) << endl;

    exported++;
  }
  stream.flush();
  return exported;
}

QString CsvExporter::buildHeader(QTableView *view, atools::sql::SqlExport& exporter,
                                 const QStringList& additionalHeader,
                                 std::function<QStringList(int index)> additionalFields)
{
  QAbstractItemModel *model = view->model();
  QStringList headers;
  for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
  {
    // Convert view position to model position - needed to keep order
    int logicalCol = view->horizontalHeader()->logicalIndex(viewCol);

    if(logicalCol == -1)
      continue;

    if(!view->isColumnHidden(logicalCol) &&
       view->columnWidth(logicalCol) > view->horizontalHeader()->minimumSectionSize())
      headers.append(model->headerData(logicalCol, Qt::Horizontal).toString().replace("-\n", "").replace("\n", " "));
  }

  return exporter.getResultSetHeader(headers) +
         (additionalHeader.isEmpty() || !additionalFields ? QString() : ";" + additionalHeader.join(";"));

}
