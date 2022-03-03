/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_AIRSPACEDIALOG_H
#define LNM_AIRSPACEDIALOG_H

#include <QDialog>

namespace Ui {
class AirspaceDialog;
}

class QAbstractButton;

/*
 * Dialog for loading user airspaces from a folder hierarchy containing the files.
 * Allows to select the path and the file patterns.
 */
class AirspaceDialog
  : public QDialog
{
  Q_OBJECT

public:
  explicit AirspaceDialog(QWidget *parent);
  virtual ~AirspaceDialog() override;

  /* Base path for airspace files */
  QString getAirspacePath() const;

  /* File patterns like "*.txt *.json *.geojson" */
  QString getAirspaceFilePatterns() const;

private:
  void saveState();
  void restoreState();
  void buttonBoxClicked(QAbstractButton *button);

  /* User clicked "select path" */
  void airspacePathSelectClicked();

  /* Check path and show error messages */
  void updateAirspaceStates();

  QWidget *parentWidget;
  Ui::AirspaceDialog *ui;
};

#endif // LNM_AIRSPACEDIALOG_H
