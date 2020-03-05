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

#ifndef LNM_AIRCRAFTPERFDIALOG_H
#define LNM_AIRCRAFTPERFDIALOG_H

#include <QDialog>
#include <functional>

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
  explicit AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance,
                              const QString& modeText);
  virtual ~AircraftPerfDialog();

  /* Get edited performance data */
  atools::fs::perf::AircraftPerf getAircraftPerf() const;

private:
  /* comboBoxFuelUnit index */
  enum FuelUnit
  {
    WEIGHT_LBS,
    VOLUME_GAL,
    WEIGHT_KG,
    VOLUME_LITER
  };

  /* Restore and save dialog state and size */
  void restoreState();
  void saveState();

  /* Copy from AircraftPerf to dialog */
  void fromPerfToDialog(const atools::fs::perf::AircraftPerf *aircraftPerf);

  /* Copy from dialog to AircraftPerf */
  void fromDialogToPerf(atools::fs::perf::AircraftPerf *aircraftPerf) const;

  /* Change volume/weight units */
  void fuelUnitChanged();
  void buttonBoxClicked(QAbstractButton *button);

  /* Update vertical speed descent rule */
  void vertSpeedChanged();

  /* Update range label */
  void updateRange();

  /* Get conversion functions depending on fuel type combo box setting*/
  static std::function<float(float value, bool fuelAsVolume)> fuelUnitsToDialogFunc(FuelUnit unit);
  static std::function<float(float value, bool fuelAsVolume)> fuelUnitsFromDialogFunc(FuelUnit unit);

  /* is current fuel type combo box setting metric and/or volume */
  static bool isMetric(FuelUnit unit);
  static bool isVol(FuelUnit unit);

  Ui::AircraftPerfDialog *ui;
  UnitStringTool *units = nullptr;
  atools::fs::perf::AircraftPerf *perf = nullptr, *perfBackup = nullptr;

  /* All widgets that need their unit placeholders replaced on unit changes */
  QList<QWidget *> unitWidgets;

  /* Set based on the fuel unit combo box */
  FuelUnit fuelUnit = WEIGHT_LBS;
};

#endif // LNM_AIRCRAFTPERFDIALOG_H
