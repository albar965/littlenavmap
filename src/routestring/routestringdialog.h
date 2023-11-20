/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_ROUTESTRINGDIALOG_H
#define LITTLENAVMAP_ROUTESTRINGDIALOG_H

#include "routestring/routestringtypes.h"

#include <QDialog>
#include <QTimer>

class QMenu;

namespace Ui {
class RouteStringDialog;
}

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
}
}
}

class MapQuery;
class QAbstractButton;
class QActionGroup;
class RouteController;
class RouteStringWriter;
class RouteStringReader;
class SyntaxHighlighter;

class RouteStringDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RouteStringDialog(QWidget *parent, const QString& settingsSuffixParam);
  virtual ~RouteStringDialog() override;

  RouteStringDialog(const RouteStringDialog& other) = delete;
  RouteStringDialog& operator=(const RouteStringDialog& other) = delete;

  const atools::fs::pln::Flightplan& getFlightplan() const
  {
    return *flightplan;
  }

  /* Saves dialog and menu action states */
  void saveStateWidget();

  /* Saves route string */
  void saveState();

  /* Restores last route string too */
  void restoreState();

  /* Update splitter and syntax highlighter */
  void styleChanged();

  /* Update buttons depending on route state */
  void updateButtonState();

  /* > 0 if speed was included in the string */
  float getSpeedKts() const
  {
    return speedKts;
  }

  /* True if the altitude was included in the string */
  bool isAltitudeIncluded() const
  {
    return altitudeIncluded;
  }

  /* Get options from default non-modal dialog */
  static rs::RouteStringOptions getOptionsFromSettings();

  /* Prepends to list */
  void addRouteDescription(const QString& routeString);

signals:
  /* Emitted when user clicks "Create flight plan" */
  void routeFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude, bool changed, bool undo);

private:
  void textChanged();
  void textChangedDelayed();
  void textChangedInternal(bool forceUpdate);
  void fromClipboardClicked();
  void toClipboardClicked();
  void buttonBoxClicked(QAbstractButton *button);
  void toolButtonOptionTriggered(QAction *);
  void loadFromFlightplanButtonClicked();
  void showHelpButtonToggled(bool checked);
  void splitterMoved();
  void buildButtonMenu();

  void updateTypeToFlightplan();
  void updateTypeFromFlightplan();

  /* Updates text edit and starts parser without delay */
  void textEditRouteStringPrepend(const QString& text, bool newline);

  /* Catch events to allow repositioning */
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

  QMenu *advancedMenu = nullptr;

  Ui::RouteStringDialog *ui;
  atools::fs::pln::Flightplan *flightplan = nullptr;
  MapQuery *mapQuery = nullptr;
  RouteController *controller = nullptr;
  RouteStringWriter *routeStringWriter = nullptr;
  RouteStringReader *routeStringReader = nullptr;
  QActionGroup *procActionGroup = nullptr;
  QTimer textUpdateTimer;

  float speedKts = 0.f;
  bool altitudeIncluded = false, updatingActions = false;
  rs::RouteStringOptions options = rs::DEFAULT_OPTIONS;

  // Save different settings depending on suffix
  QString settingsSuffix;

  // blocking if parent is not null
  bool blocking = false;

  // Notify RouteStringDialog::textChanged() to a direct update instead of a delayed one
  bool immediateUpdate = false;

  /* Remember dialog position when reopening */
  QPoint position;

  /* Remember to avoid unneeded parsing if user edits areas outside the active string. */
  QString lastCleanRouteString;

  /* Makes first section bold and other (scratchpad) gray */
  SyntaxHighlighter *sytaxHighlighter;
};

#endif // LITTLENAVMAP_ROUTESTRINGDIALOG_H
