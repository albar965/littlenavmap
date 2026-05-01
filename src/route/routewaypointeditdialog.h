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

#ifndef LITTLENAVMAP_USERWAYPOINTDIALOG_H
#define LITTLENAVMAP_USERWAYPOINTDIALOG_H

#include "options/optionchangeflags.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>

class Route;
namespace Ui {
class RouteWaypointEditDialog;
}

namespace atools {

namespace gui {
class WidgetTool;
}
namespace fs {
namespace pln {
class FlightplanEntry;
}
}
namespace geo {
class Pos;
}
}

class RouteWaypointEditEventFilter;
class QAbstractButton;

/*
 * Edit coordinates or the name of a user defined flight plan position.
 * Also used to edit remarks for a flight plan waypoint.
 *
 * Changes are applied immediately in case of focus change or after a delay.
 */
class RouteWaypointEditDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RouteWaypointEditDialog(QWidget *parent);
  virtual ~RouteWaypointEditDialog() override;

  /* No copying */
  RouteWaypointEditDialog(const RouteWaypointEditDialog& other) = delete;
  RouteWaypointEditDialog& operator=(const RouteWaypointEditDialog& other) = delete;

  /* Save, load and reset dialog size and position */
  void saveState();
  void restoreState();
  void resetWindowLayout();

  /* Update coordinate units */
  void optionsChanged(const optc::OptionChangeFlags& changeFlags);

  /* Update font for whole dialog */
  void fontChanged(const QFont& font);

  /* Entry is copied. Get changed copy here */
  const atools::fs::pln::FlightplanEntry& getEntry() const
  {
    return *flightplanEntry;
  }

  /* Set flight plan entry and adjust widgets in dialog to entry type.
   * Sends waypointEdited() is there are current changes. */
  void setEntryAndShow(int index, const atools::fs::pln::FlightplanEntry& entryParam);

  /* Index in flight plan as set in setEntry() */
  int getEntryIndex() const
  {
    return entryIndex;
  }

  /* Clear entry contents, disable all dialog widgets and show a message about editing.
   * Saves a copy of the current entry. */
  void clearAndSave();

  /* Looks though all route legs trying to identify the entry saved by clearAndSave() */
  void updateFromRoute(const Route& route);

signals:
  /* Send on focus out or after a delay in case entry was changed */
  void waypointEdited();

private:
  friend class RouteWaypointEditEventFilter;

  virtual void closeEvent(QCloseEvent *) override;
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

  void buttonBoxClicked(QAbstractButton *button);
  void coordsEdited(const QString& = QString());
  void entryToWidgets(const atools::fs::pln::FlightplanEntry& entry);
  void widgetsToEntry(atools::fs::pln::FlightplanEntry& entry) const;
  void updateHeaderLabel();

  void setEntryInternal(int index, const atools::fs::pln::FlightplanEntry& entryParam);

  /* Called by editTimer of on focus out */
  void fieldsEdited();

  /* Starts timer for delayed message fieldsEdited() */
  void fieldsEditedDelayed();

  atools::fs::pln::FlightplanEntry *flightplanEntry, *flightplanEntrySaved;
  int entryIndex;

  /* Dialog default size as set in Qt Designer */
  QSize defaultSize;

  /* Triggers fieldsEdited() after delay */
  QTimer editTimer;

  RouteWaypointEditEventFilter *eventFilter = nullptr;

  /* Used for bulk changes in widgets */
  atools::gui::WidgetTool *widgets = nullptr;
  Ui::RouteWaypointEditDialog *ui;
};

// =================================================================================================
/* Event filter to catch focus out for QPlainTextEdit */
class RouteWaypointEditEventFilter :
  public QObject
{
  Q_OBJECT

public:
  RouteWaypointEditEventFilter(RouteWaypointEditDialog *parent)
    : QObject(parent), dialog(parent)
  {
  }

  virtual ~RouteWaypointEditEventFilter() override
  {

  }

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  RouteWaypointEditDialog *dialog;
};

#endif // LITTLENAVMAP_USERWAYPOINTDIALOG_H
