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

#ifndef XPCONNECTINSTALLER_H
#define XPCONNECTINSTALLER_H

#include <QCoreApplication>

namespace atools {
namespace gui {
class Dialog;
}
}

/*
 * Provides a method to install the Xpconnect in the X-Plane plugins folder of the currently selected X-Plane installation.
 * Removes any previous installations and copies/overwrites the new plugin from the Little Navmap installation folder.
 */
class XpconnectInstaller
{
  Q_DECLARE_TR_FUNCTIONS(XpconnectInstaller)

public:
  /* Parent needed for dialogs */
  XpconnectInstaller(QWidget *parentWidget);
  ~XpconnectInstaller();

  /* Check destination plugins, remove any non-standard installations and copy
   * copy/overwrite the new plugin from the Little Navmap installation folder.
   * Returns true if successfull. */
  bool install();

private:
  QWidget *parent;
  atools::gui::Dialog *dialog;

  /* Plugin name which can differ between OS */
  QString xpconnectName;
};

#endif // XPCONNECTINSTALLER_H
