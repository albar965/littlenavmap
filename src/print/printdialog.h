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

#ifndef LITTLENAVMAP_PRINTDIALOG_H
#define LITTLENAVMAP_PRINTDIALOG_H

#include <QBitArray>
#include <QDialog>

namespace Ui {
class PrintDialog;
}
class QAbstractButton;

namespace prt {
enum PrintFlightPlanOpt
{
  NONE = 0,
  DEPARTURE_OVERVIEW = 1 << 0,
  DEPARTURE_RUNWAYS = 1 << 1,
  DEPARTURE_RUNWAYS_SOFT = 1 << 2,
  DEPARTURE_RUNWAYS_DETAIL = 1 << 3,
  DEPARTURE_COM = 1 << 4,
  DEPARTURE_APPR = 1 << 5,
  DESTINATION_OVERVIEW = 1 << 6,
  DESTINATION_RUNWAYS = 1 << 7,
  DESTINATION_RUNWAYS_SOFT = 1 << 8,
  DESTINATION_RUNWAYS_DETAIL = 1 << 9,
  DESTINATION_COM = 1 << 10,
  DESTINATION_APPR = 1 << 11,
  FLIGHTPLAN = 1 << 12, /* Print flight plan */
  NEW_PAGE = 1 << 13, /* New page after each chapter */
  FUEL_REPORT = 1 << 14, /* Print fuel and aircraft performance report */
  HEADER = 1 << 15, /* Print a detailed header as on top of the flight plan dock */

  DEPARTURE_WEATHER = 1 << 16,
  DESTINATION_WEATHER = 1 << 17,

  DEPARTURE_ANY = DEPARTURE_OVERVIEW | DEPARTURE_RUNWAYS |
                  DEPARTURE_COM | DEPARTURE_APPR | DEPARTURE_WEATHER,
  DESTINATION_ANY = DESTINATION_OVERVIEW | DESTINATION_RUNWAYS |
                    DESTINATION_COM | DESTINATION_APPR | DESTINATION_WEATHER
};

Q_DECLARE_FLAGS(PrintFlightPlanOpts, PrintFlightPlanOpt);
Q_DECLARE_OPERATORS_FOR_FLAGS(prt::PrintFlightPlanOpts);
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

  /* Get options as set in the dialog */
  prt::PrintFlightPlanOpts getPrintOptions() const;

  /* Save and restore dialog size and selected options */
  void saveState();
  void restoreState();

  /* Text size in percent of default font */
  int getPrintTextSize() const;
  int getPrintTextSizeFlightplan() const;

  /* Fill column selection widget with texts */
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

  Ui::PrintDialog *ui;
  QBitArray selectedRows;
  /* Override to avoid closing the dialog */
  virtual void accept() override;

};

#endif // LITTLENAVMAP_PRINTDIALOG_H
