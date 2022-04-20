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

#ifndef LNM_RANGEMARKER_DIALOG_H
#define LNM_RANGEMARKER_DIALOG_H

#include <QDialog>
#include <QValidator>

namespace Ui {
class RangeMarkerDialog;
}

namespace map {
struct RangeMarker;
}

namespace atools {
namespace geo {
class Pos;
}
}

class QAbstractButton;
class UnitStringTool;
class RangeRingValidator;

/*
 * Shows a dialog where user can set radii, color or label for range rings.
 *
 * Reads state and defaults on intiantiation and saves it on destruction
 */
class RangeMarkerDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RangeMarkerDialog(QWidget *parent, const atools::geo::Pos& pos);
  virtual ~RangeMarkerDialog() override;

  RangeMarkerDialog(const RangeMarkerDialog& other) = delete;
  RangeMarkerDialog& operator=(const RangeMarkerDialog& other) = delete;

  /* Fill marker object. Label and aircraft endurance will be ignored if dialogOpened is true */
  void fillRangeMarker(map::RangeMarker& marker, bool dialogOpened);

  /* true if checkbox is clicked to avoid dialog on Shift-Click into the map  */
  bool isNoShowShiftClickEnabled();

private:
  void restoreState();
  void saveState();
  void buttonBoxClicked(QAbstractButton *button);
  void colorButtonClicked();
  void updateButtonColor();
  void coordinatesEdited(const QString&);

  /* Convert string to ranges. Falls back to defaults if invalid */
  QString rangeFloatToString(const QVector<float>& ranges) const;
  QVector<float> rangeStringToFloat(const QString& rangeStr) const;

  Ui::RangeMarkerDialog *ui;

  /* Color is set when clicking on button */
  QColor color;
  atools::geo::Pos *position;

  UnitStringTool *units = nullptr;

  // Validates the space separated list of ring sizes
  RangeRingValidator *rangeRingValidator;

  const static QVector<float> MAP_RANGERINGS_DEFAULT;
};

/* Validates the space separated list of range ring sizes */
class RangeRingValidator :
  public QValidator
{
  Q_OBJECT

public:
  RangeRingValidator();

private:
  virtual QValidator::State validate(QString& input, int&) const override;

  bool ringStrToVector(const QString& str) const;

};

#endif // LNM_RANGEMARKER_DIALOG_H
