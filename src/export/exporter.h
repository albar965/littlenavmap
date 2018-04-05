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

#ifndef LITTLELOGBOOK_EXPORTER_H
#define LITTLELOGBOOK_EXPORTER_H

#include <QApplication>

namespace atools {
namespace gui {
class Dialog;
class ErrorHandler;
}
}

class SqlController;
class QWidget;
class QSqlRecord;

/*
 * Base for all export classes.
 */
class Exporter
{
  Q_DECLARE_TR_FUNCTIONS(Exporter)

public:
  Exporter(QWidget *parentWidget, SqlController *controllerObj);
  virtual ~Exporter();

  // Disabled unused export functionality since it is not compatible with other classes
#ifdef ENABLE_BASE_EXPORT

  virtual int exportAll(bool open) = 0;
  virtual int exportSelected(bool open) = 0;

#endif

protected:
  QWidget *parentWidget = nullptr;
  SqlController *controller = nullptr;
  atools::gui::Dialog *dialog = nullptr;
  atools::gui::ErrorHandler *errorHandler = nullptr;

  /* Get table header names with any LF or dash cleaned out */
  QStringList headerNames(int cnt);
  QStringList headerNames(int cnt, const QVector<int>& visualToIndex);

  /* Open document with application */
  void openDocument(const QString& file);

  /* Create an index mapping physical to logical column numbers */
  void createVisualColumnIndex(int cnt, QVector<int>& visualToIndex);

  /* Create an SQL record from column names and values */
  void fillRecord(const QVariantList& values, const QStringList& cols, QSqlRecord& rec);

};

#endif // LITTLELOGBOOK_EXPORTER_H
