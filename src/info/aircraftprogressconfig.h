/*****************************************************************************
* Copyright 2015-2021 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_AIRCRAFTPROGRESSCONFIG_H
#define LNM_AIRCRAFTPROGRESSCONFIG_H

#include "common/htmlinfobuilderflags.h"

#include <QApplication>
#include <QBitArray>

/*
 * Stores and saves data field visibility flags in aircraft progress dialog.
 * Uses a tree dialog to show fields and description.
 */
class AircraftProgressConfig
{
  Q_DECLARE_TR_FUNCTIONS(AircraftProgressConfig)

public:
  explicit AircraftProgressConfig(QWidget *parentWidget);

  /* Show configuration dialog. Saves settigs on exit. */
  void progressConfiguration();

  /* Save ids of the objects shown in the tabs to content can be restored on startup */
  void saveState();
  void restoreState();

  /* Get a bit field with check state for all tree elements. Index corresponds to pid::ProgressConfId enum values */
  const QBitArray& getEnabledBits() const
  {
    return enabledBits;
  }

private:
  void updateBits();

  /* List of enums which are enabled */
  QVector<pid::ProgressConfId> enabledIds;

  /* Bit field with check state according to enum */
  QBitArray enabledBits;

  QWidget *parent;
};

#endif // LNM_AIRCRAFTPROGRESSCONFIG_H
