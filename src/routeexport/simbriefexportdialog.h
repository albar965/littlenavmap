/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_SIMBRIEFEXPORTDIALOG_H
#define LNM_SIMBRIEFEXPORTDIALOG_H

#include <QDialog>

namespace Ui {
class SimBriefExportDialog;
}

class QAbstractButton;

/*
 * Displays the current route summary and allows entering optional SimBrief fields.
 *
 * Loads and saves state automatically.
 */
class SimBriefExportDialog :
  public QDialog
{
  Q_OBJECT

public:
  enum Action
  {
    None,
    Export,
    CopyToClipboard
  };

  explicit SimBriefExportDialog(QWidget *parent, const QString& routeDescription, const QString& aircraftType,
                                const QString& cruiseAltitude);
  virtual ~SimBriefExportDialog() override;

  SimBriefExportDialog(const SimBriefExportDialog& other) = delete;
  SimBriefExportDialog& operator=(const SimBriefExportDialog& other) = delete;

  Action getAction() const;
  QString getAirline() const;
  QString getFlightNumber() const;

private:
  void restoreState();
  void saveState() const;
  void buttonBoxClicked(QAbstractButton *button);

  Ui::SimBriefExportDialog *ui;
  Action action = None;
};

#endif // LNM_SIMBRIEFEXPORTDIALOG_H
