/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "marker/markerdialog.h"

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
 * Label can be set automaticall from navaid or changed by user.
 * Reads state and defaults on intiantiation and saves it on destruction
 */
class RangeMarkerDialog :
  public MarkerDialog<map::RangeMarker>
{
  Q_OBJECT

public:
  /* Marker is copied to internal. Result is used to assign navaid. editMode false configures dialog for adding. */
  explicit RangeMarkerDialog(QWidget *parent, const map::RangeMarker& markerParam, const map::MapResult& result, bool editMode);
  virtual ~RangeMarkerDialog() override;

private:
  virtual void restoreState() override;
  virtual void saveState() const override;
  virtual void markerToWidgets() override;
  virtual void widgetsToMarker() override;

  void buttonBoxClicked(QAbstractButton *button);
  void coordinatesEdited(const QString&);

  /* Convert string to ranges. Falls back to defaults if invalid */
  QString rangeDoubleToString(const QList<double>& ranges) const;
  const QList<double> rangeStringToDouble(const QString& rangeStr) const;

  void fillMarkerFromResultAndWidgets();

  /* Text already given by navaid when editMode is false */
  bool textPreFilled = false;

  UnitStringTool *units = nullptr;

  // Validates the space separated list of ring sizes
  RangeRingValidator *rangeRingValidator;

  const static QList<double> MAP_RANGERINGS_DEFAULT;
  const static QString DEFAULT_COLOR_STR;

  Ui::RangeMarkerDialog *ui;
};

/* ==================================================================
 * Validates the space separated list of range ring sizes */
class RangeRingValidator :
  public QValidator
{
  Q_OBJECT

public:
  RangeRingValidator()
  {
  }

  virtual ~RangeRingValidator() override
  {

  }

private:
  virtual QValidator::State validate(QString& input, int&) const override;

  bool ringStrToVector(const QString& str) const;

};

#endif // LNM_RANGEMARKER_DIALOG_H
