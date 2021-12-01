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

#ifndef LNM_RUNWAY_SELECTION_DIALOG_H
#define LNM_RUNWAY_SELECTION_DIALOG_H

#include <QDialog>

namespace map {
struct MapAirport;
struct MapRunway;
struct MapRunwayEnd;
}

namespace Ui {
class RunwaySelectionDialog;
}

class RunwaySelection;
class QAbstractButton;

/*
 * Shows airport and runway information and allows to select a runway to be assigned to a multi runway SID or STAR.
 *
 * Reads state on intiantiation and saves it on destruction.
 */
class RunwaySelectionDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RunwaySelectionDialog(QWidget *parent, const map::MapAirport& mapAirport, const QStringList& runwayNames, const QString& header);
  virtual ~RunwaySelectionDialog() override;

  RunwaySelectionDialog(const RunwaySelectionDialog& other) = delete;
  RunwaySelectionDialog& operator=(const RunwaySelectionDialog& other) = delete;

  /* Selected runway name or empty if none */
  QString getSelectedName() const;

private:
  void restoreState();
  void saveState();

  void buttonBoxClicked(QAbstractButton *button);
  void updateWidgets();
  void doubleClicked();

  Ui::RunwaySelectionDialog *ui;
  RunwaySelection *runwaySelection = nullptr;
};

#endif // LNM_RUNWAY_SELECTION_DIALOG_H
