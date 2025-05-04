/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_PRINTDIALOG_H
#define LITTLENAVMAP_PRINTDIALOG_H

#include "print/printdialogflags.h"

#include <QBitArray>
#include <QDialog>

namespace Ui {
class PrintDialog;
}
class QAbstractButton;

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

/*
 * Provides a dialog that allows to select flight plan documents for printing and various other configuration options.
 * Has a list widget to select flight plan table columns.
 */
class PrintDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit PrintDialog(QWidget *parent);
  virtual ~PrintDialog() override;

  PrintDialog(const PrintDialog& other) = delete;
  PrintDialog& operator=(const PrintDialog& other) = delete;

  /* Get options as set in the dialog */
  prt::PrintFlightplanOpts getPrintOptions() const;

  /* Save and restore dialog size and selected options */
  void saveState();
  void restoreState();

  /* Text size in percent of default font */
  int getPrintTextSize() const;
  int getPrintTextSizeFlightplan() const;

  /* Fill column selection widget with texts.
   * All route table columns are used. This is independent if a column is hidden or not.
   * Uses logical and not view order. */
  void setRouteTableColumns(const QStringList& columns);

  /* Bits for selected colums with size equal to number of columns as set above */
  const QBitArray& getSelectedRouteTableColumns() const;

signals:
  /* Preview or print clicked - dialog is not closed */
  void printPreviewClicked();
  void printClicked();

private:
  void buttonBoxClicked(QAbstractButton *button);
  void updateButtonStates();

  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  Ui::PrintDialog *ui;
  QBitArray selectedRows;
  /* Override to avoid closing the dialog */
  virtual void accept() override;

};

#endif // LITTLENAVMAP_PRINTDIALOG_H
