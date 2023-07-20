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

#ifndef LNM_UNDOREDOPROGRESS_H
#define LNM_UNDOREDOPROGRESS_H

#include <QCoreApplication>

class QProgressDialog;
class QWidget;

/* Common class for long undo/redo operations. Shows a progress dialog after timeout and
 * provides callback for DataManagerBase and takes care of wait cursor. */
class UndoRedoProgress
{
  Q_DECLARE_TR_FUNCTIONS(UndoRedoProgress)

public:
  UndoRedoProgress(QWidget *parent, const QString& title, const QString& text);
  ~UndoRedoProgress();

  /* Start with wait cursor until dialog openes */
  void start();

  /* Remove wait cursor if dialog was not opened */
  void stop();

  /* callback for void DataManagerBase::setProgressCallback(UndoRedoCallbackType progressCallback) */
  bool callback(int totalNumber, int currentNumber);

  bool isCanceled() const
  {
    return canceled;
  }

private:
  bool dialogShown = false, canceled = false;

  QProgressDialog *progress;
};

#endif // LNM_UNDOREDOPROGRESS_H
