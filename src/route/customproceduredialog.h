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

#ifndef LNM_CUSTOMPROCEDUREDIALOG_H
#define LNM_CUSTOMPROCEDUREDIALOG_H

#include <QDialog>

namespace map {
struct MapAirport;
struct MapRunway;
struct MapRunwayEnd;
}

namespace Ui {
class CustomProcedureDialog;
}

class RunwaySelection;
class QAbstractButton;
class UnitStringTool;

/*
 * Shows airport and runway information and allows to configure a custom approach procedure for a selected runway.
 *
 * Reads state on intiantiation and saves it on destruction
 */
class CustomProcedureDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit CustomProcedureDialog(QWidget *parent, const map::MapAirport& mapAirport, bool departureParam, const QString& dialogHeader,
                                 int preselectRunwayEndSim);
  virtual ~CustomProcedureDialog() override;

  CustomProcedureDialog(const CustomProcedureDialog& other) = delete;
  CustomProcedureDialog& operator=(const CustomProcedureDialog& other) = delete;

  /* Selected runway and end or invalid if none */
  void getSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const;

  /* Distance to runway threshold in NM */
  float getLegDistance() const;

  /* Offset to runway heading in degree. Positive is CW and negative is CCW. */
  float getLegOffsetAngle() const;

  /* Altitude at entry point above airport elevation in feet */
  float getEntryAltitude() const;

  bool isShowProceduresSelected() const
  {
    return showProceduresSelected;
  }

private:
  void restoreState();
  void saveState() const;

  void buttonBoxClicked(QAbstractButton *button);
  void updateWidgets();
  void doubleClicked();

  Ui::CustomProcedureDialog *ui;
  RunwaySelection *runwaySelection = nullptr;

  UnitStringTool *units = nullptr;
  bool departure = false, showProceduresSelected = false;
};

#endif // LNM_CUSTOMPROCEDUREDIALOG_H
