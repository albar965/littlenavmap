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

#ifndef LITTLENAVMAP_ROUTESTRINGDIALOG_H
#define LITTLENAVMAP_ROUTESTRINGDIALOG_H

#include "routestring/routestringtypes.h"

#include <QDialog>
#include <QSyntaxHighlighter>
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
class TextEditEventFilter;

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
  void saveStateWidget() const;

  /* Saves route string */
  void saveState() const;

  /* Restores last route string too */
  void restoreState();
  void restoreStateWidget();

  /* Reset saved settings and position of RouteStringDialog. Also recenters if routeStringDialog is not null. */
  static void resetWindowLayout(RouteStringDialog *routeStringDialog, const QString& settingsSuffix);

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

  void fontChanged(const QFont& font);

  /* Create flight plan but keep dialog open */
  void createPlanAndKeepOpen();

  /* No-op for now */
  void preDatabaseLoad();

  /* Re-read string. */
  void postDatabaseLoad();
  void tracksChanged();

signals:
  /* Emitted when user clicks "Create flight plan" */
  void routeFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude, bool changed, bool undo,
                           bool correctProfile);

private:
  void textChanged();
  void textChangedDelayed();
  void textChangedInternal(bool forceUpdate);
  void fromClipboardClicked();
  void toClipboardClicked();
  void buttonBoxClicked(QAbstractButton *button);
  void toolButtonOptionTriggered(QAction *act);
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

  QMenu *advancedMenu = nullptr, *buttonMenu = nullptr;
  QList<QAction *> actions;

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

  // blocking if parent is not null, i.e. from SimBrief
  bool blocking = false;

  // Notify RouteStringDialog::textChanged() to a direct update instead of a delayed one
  bool immediateUpdate = false;

  /* Remember to avoid unneeded parsing if user edits areas outside the active string. */
  QString lastCleanRouteString;

  /* Makes first section bold and other (scratchpad) gray */
  SyntaxHighlighter *sytaxHighlighter;

  TextEditEventFilter *eventFilter = nullptr;

  /* Size as given in UI */
  QSize defaultSize;
};

// =================================================================================================
/* Makes first block bold and rest gray to indicate active description text. */
class SyntaxHighlighter :
  public QSyntaxHighlighter
{
  Q_OBJECT

public:
  SyntaxHighlighter(QObject *parent);

  virtual ~SyntaxHighlighter() override;

  void styleChanged();

private:
  virtual void highlightBlock(const QString& text) override;

  QTextCharFormat formatHighlight, formatNormal;

  enum
  {
    BEFORE_START = -1, // First line or empty lines before any text
    IN_HIGHLIGHT_BLOCK = 0, // Currently non-empty lines
    AFTER_HIGHLIGHT_BLOCK = 1 // After one filled block now coloring gray
  };

};

// =================================================================================================
class TextEditEventFilter :
  public QObject
{
  Q_OBJECT

public:
  TextEditEventFilter(RouteStringDialog *parent)
    : QObject(parent), dialog(parent)
  {
  }

  virtual ~TextEditEventFilter() override;

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  RouteStringDialog *dialog;
};

#endif // LITTLENAVMAP_ROUTESTRINGDIALOG_H
