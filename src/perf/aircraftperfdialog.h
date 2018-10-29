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

#ifndef LNM_AIRCRAFTPERFDIALOG_H
#define LNM_AIRCRAFTPERFDIALOG_H

#include <QDialog>

namespace Ui {
class AircraftPerfDialog;
}

namespace atools {
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

class QAbstractButton;
class UnitStringTool;

/*
 * Edit dialog for aircraft performance.
 */
class AircraftPerfDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* Will create a copy of AircraftPerf for editing */
  explicit AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance);
  virtual ~AircraftPerfDialog();

  /* Get edited performance data */
  atools::fs::perf::AircraftPerf getAircraftPerf() const;

private:
  /* Restore and save dialog state and size */
  void restoreState();
  void saveState();

  /* Copy from AircraftPerf to dialog */
  void toDialog(const atools::fs::perf::AircraftPerf *aircraftPerf);

  /* Copy from dialog to AircraftPerf */
  void fromDialog(atools::fs::perf::AircraftPerf *aircraftPerf) const;

  /* Change volume/weight units */
  void fuelUnitChanged(int index);
  void buttonBoxClicked(QAbstractButton *button);

  /* Update vertical speed descent rule */
  void vertSpeedChanged();

  Ui::AircraftPerfDialog *ui;
  UnitStringTool *units = nullptr;
  atools::fs::perf::AircraftPerf *perf = nullptr, *perfBackup = nullptr;

};

#endif // LNM_AIRCRAFTPERFDIALOG_H
