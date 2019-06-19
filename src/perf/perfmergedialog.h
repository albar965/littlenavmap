/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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

#ifndef PERFMERGEDIALOG_H
#define PERFMERGEDIALOG_H

#include <QDialog>

namespace Ui {
class PerfMergeDialog;
}

namespace atools {
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

class QAbstractButton;
class QComboBox;

/*
 * Provide a dialog that allows a user to merge or overwrite aircraft performance data.
 */
class PerfMergeDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* Create dialog with source and destination peformance data. showAll = true allows to merge all fields,
   * otherwise shows only fields relevant for performance collection */
  explicit PerfMergeDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& sourcePerf,
                           atools::fs::perf::AircraftPerf& destPerf, bool showAll);
  virtual ~PerfMergeDialog() override;

  /* True if a value has changed independent if user clicked OK or Cancel */
  bool hasChanged() const
  {
    return changed;
  }

private:
  /* Indexes of all combo boxes */
  enum ComboBoxIndex
  {
    IGNORE,
    COPY,
    MERGE
  };

  void restoreState();
  void saveState();

  void buttonBoxClicked(QAbstractButton *button);

  /* Copy, merge or ignore values between from and to */
  void process();

  /* Copy, merge or ignore values based on combo box index and update changed flag */
  float proc(QComboBox *combo, float fromValue, float toValue);
  QString proc(QComboBox *combo, QString fromValue, QString toValue);

  /* Set all combo boxes to the given index */
  void updateComboBoxWidgets(ComboBoxIndex idx);

  /* Update labels showing the current values */
  void updateWidgetValues();

  /* Push buttons to set status of all combo boxes */
  void copyClicked();
  void mergeClicked();
  void ignoreClicked();

  Ui::PerfMergeDialog *ui;

  const atools::fs::perf::AircraftPerf& from;
  atools::fs::perf::AircraftPerf& to;

  bool showAllWidgets = true, changed = false;
};

#endif // PERFMERGEDIALOG_H
