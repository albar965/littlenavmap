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

#ifndef LITTLENAVMAP_PRINTDIALOG_H
#define LITTLENAVMAP_PRINTDIALOG_H

#include <QDialog>
#include <QTimer>

namespace Ui {
class PrintDialog;
}
class QAbstractButton;

namespace prt {
enum PrintFlightPlanOpt
{
  NONE = 0x0000,
  DEPARTURE_OVERVIEW = 0x0001,
  DEPARTURE_RUNWAYS = 0x0002,
  DEPARTURE_RUNWAYS_SOFT = 0x0004,
  DEPARTURE_RUNWAYS_DETAIL = 0x0008,
  DEPARTURE_COM = 0x0010,
  DEPARTURE_APPR = 0x0020,
  DESTINATION_OVERVIEW = 0x0040,
  DESTINATION_RUNWAYS = 0x0080,
  DESTINATION_RUNWAYS_SOFT = 0x0100,
  DESTINATION_RUNWAYS_DETAIL = 0x0200,
  DESTINATION_COM = 0x0400,
  DESTINATION_APPR = 0x0800,
  FLIGHTPLAN = 0x1000,

  DEPARTURE_WEATHER = 0x2000,
  DESTINATION_WEATHER = 0x4000,

  DEPARTURE_ANY = DEPARTURE_OVERVIEW | DEPARTURE_RUNWAYS |
                  DEPARTURE_COM | DEPARTURE_APPR | DEPARTURE_WEATHER,
  DESTINATION_ANY = DESTINATION_OVERVIEW | DESTINATION_RUNWAYS |
                    DESTINATION_COM | DESTINATION_APPR | DESTINATION_WEATHER
};

Q_DECLARE_FLAGS(PrintFlightPlanOpts, PrintFlightPlanOpt);
Q_DECLARE_OPERATORS_FOR_FLAGS(prt::PrintFlightPlanOpts);
}

/*
 * Provides a dialog that allows to select fligh plan documents for printing
 */
class PrintDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit PrintDialog(QWidget *parent = 0);
  ~PrintDialog();

  prt::PrintFlightPlanOpts getPrintOptions() const;
  int getPrintTextSize() const;

  void saveState();
  void restoreState();

signals:
  void printPreviewClicked();
  void printClicked();

private:
  void buttonBoxClicked(QAbstractButton *button);
  void updateButtonStates();

  Ui::PrintDialog *ui;

  /* Override to avoid closing the dialog */
  virtual void accept() override;

};

#endif // LITTLENAVMAP_PRINTDIALOG_H
