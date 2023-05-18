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

#include "db/undoredoprogress.h"

#include <QProgressDialog>
#include <QDebug>
#include <QApplication>

UndoRedoProgress::UndoRedoProgress(QWidget *parent, const QString& title, const QString& text)
{
  progress = new QProgressDialog(text, tr("Cancel"), 0, 0, parent);
  progress->setWindowTitle(title);
  progress->setWindowFlags(progress->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  progress->setWindowModality(Qt::ApplicationModal);
  progress->setMinimumDuration(50);
}

UndoRedoProgress::~UndoRedoProgress()
{
  delete progress;
}

void UndoRedoProgress::start()
{
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
}

void UndoRedoProgress::stop()
{
  if(!dialogShown)
    QGuiApplication::restoreOverrideCursor();
}

bool UndoRedoProgress::callback(int totalNumber, int currentNumber)
{
  qDebug() << Q_FUNC_INFO << totalNumber << currentNumber;
  progress->setMaximum(totalNumber);
  progress->setValue(currentNumber);
  canceled = progress->wasCanceled();

  if(!dialogShown && progress->isVisible())
  {
    // Dialog is shown - remove wait cursor
    dialogShown = true;
    QGuiApplication::restoreOverrideCursor();
  }

  return !canceled;
}
