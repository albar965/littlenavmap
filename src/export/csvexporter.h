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

#ifndef LITTLELOGBOOK_CSVEXPORTER_H
#define LITTLELOGBOOK_CSVEXPORTER_H

#include "export/exporter.h"

#include <QObject>

class SqlController;
class QWidget;
class QTextStream;
class QTableView;

/*
 * Allows to export the table content or the selected table content from the
 * given controller into CSV files.
 * Separator is ';' and quotation is '"'.
 *
 * All columns are exported in default order regardless what is shown or ordered
 * in the table view.
 */
class CsvExporter :
  public Exporter
{
public:
  CsvExporter(QWidget *parentWidget, SqlController *controller);
  virtual ~CsvExporter();

  // Disabled unused export functionality since it is not compatible with other classes
#ifdef ENABLE_CSV_EXPORT
  /* Export all rows.
   *
   * @param open Open file in default application after export.
   * @return number of rows exported.
   */
  virtual int exportAll(bool open) override;

  /* Export only selected rows.
   *
   * @param open Open file in default application after export.
   * @return number of rows exported.
   */
  virtual int exportSelected(bool open) override;

#endif

  static int selectionAsCsv(QTableView *view, bool header, bool rows, QString& result,
                            const QStringList& additionalHeader = QStringList(),
                            std::function<QStringList(int)> additionalFields = nullptr);

private:
  /* Get file from save dialog */
  QString saveCsvFileDialog();

};

#endif // LITTLELOGBOOK_CSVEXPORTER_H
