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

#include "route/routewaypointeditdialog.h"
#include "ui_routewaypointeditdialog.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/unit.h"
#include "fs/pln/flightplanentry.h"
#include "fs/util/coordinates.h"
#include "geo/pos.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "gui/tools.h"
#include "gui/widgetstate.h"
#include "gui/widgettool.h"
#include "gui/widgetzoomhandler.h"
#include "route/route.h"

#include <QPushButton>

// Send signal after editing timeout
const int EDIT_TIMEOUT_MS = 2000;

/* Compares only members which identify a flight plan leg */
inline bool operator==(const atools::fs::pln::FlightplanEntry& entry1, const atools::fs::pln::FlightplanEntry& entry2)
{
  // Waypoint type cannot be edited

  if(entry1.getWaypointType() == atools::fs::pln::entry::USER && entry2.getWaypointType() == atools::fs::pln::entry::USER)
  {
    return entry1.getIdent() == entry2.getIdent() &&
           entry1.getRegion() == entry2.getRegion() &&
           entry1.getName() == entry2.getName() &&
           entry1.getComment() == entry2.getComment() &&
           entry1.getPosition().almostEqual(entry2.getPosition());
  }
  else
    return entry1.getComment() == entry2.getComment();
}

inline bool operator!=(const atools::fs::pln::FlightplanEntry& entry1, const atools::fs::pln::FlightplanEntry& entry2)
{
  return !operator==(entry1, entry2);
}

// Compare by fields and position to find an entry in the Route
inline bool isEqual(const atools::fs::pln::FlightplanEntry& entry1, const atools::fs::pln::FlightplanEntry& entry2)
{
  return (entry1.getIdent() == entry2.getIdent() ||
          (Route::isStandardWaypointIdent(entry1.getIdent()) && Route::isStandardWaypointIdent(entry2.getIdent()))) &&
         entry1.getRegion() == entry2.getRegion() &&
         entry1.getName() == entry2.getName() &&
         entry1.getComment() == entry2.getComment() &&
         entry1.getPosition().almostEqual(entry2.getPosition());
}

// ===========================================================================================

bool RouteWaypointEditEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::FocusOut)
  {
    // Send signal on focus change
    dialog->fieldsEdited();
    return true;
  }

  return QObject::eventFilter(object, event);
}

// ===========================================================================================

RouteWaypointEditDialog::RouteWaypointEditDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::RouteWaypointEditDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowModality(Qt::NonModal);
  ui->setupUi(this);
  defaultSize = size();

  // Copy main menu actions to allow using shortcuts in the non-modal dialog too
  addActions(NavApp::getMainWindowActions());

  flightplanEntry = new atools::fs::pln::FlightplanEntry;
  flightplanEntrySaved = new atools::fs::pln::FlightplanEntry;

  // Widget list for bulk changes
  widgets = new atools::gui::WidgetTool({ui->lineEditRouteUserWaypointIdent, ui->labelRouteUserWaypointIdent,
                                         ui->lineEditRouteUserWaypointRegion, ui->labelRouteUserWaypointRegion,
                                         ui->lineEditRouteUserWaypointName, ui->labelRouteUserWaypointName,
                                         ui->lineEditRouteUserWaypointLatLon, ui->labelRouteUserWaypointLatLon,
                                         ui->labelRouteUserWaypointCoordStatus, ui->plainTextEditRouteUserWaypointComment,
                                         ui->labelRouteUserWaypointComment, ui->labelRouteUserWaypointHeaderWaypointInfo,
                                         ui->labelRouteUserWaypointHeaderHint});

  restoreState();

  connect(ui->lineEditRouteUserWaypointLatLon, &QLineEdit::textChanged, this, &RouteWaypointEditDialog::coordsEdited);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &RouteWaypointEditDialog::buttonBoxClicked);

  connect(ui->plainTextEditRouteUserWaypointComment, &QPlainTextEdit::textChanged, this, &RouteWaypointEditDialog::fieldsEditedDelayed);
  eventFilter = new RouteWaypointEditEventFilter(this);
  ui->plainTextEditRouteUserWaypointComment->installEventFilter(eventFilter);

  connect(ui->lineEditRouteUserWaypointIdent, &QLineEdit::textChanged, this, &RouteWaypointEditDialog::fieldsEditedDelayed);
  connect(ui->lineEditRouteUserWaypointIdent, &QLineEdit::editingFinished, this, &RouteWaypointEditDialog::fieldsEdited);
  connect(ui->lineEditRouteUserWaypointRegion, &QLineEdit::textChanged, this, &RouteWaypointEditDialog::fieldsEditedDelayed);
  connect(ui->lineEditRouteUserWaypointRegion, &QLineEdit::editingFinished, this, &RouteWaypointEditDialog::fieldsEdited);
  connect(ui->lineEditRouteUserWaypointName, &QLineEdit::textChanged, this, &RouteWaypointEditDialog::fieldsEditedDelayed);
  connect(ui->lineEditRouteUserWaypointName, &QLineEdit::editingFinished, this, &RouteWaypointEditDialog::fieldsEdited);
  connect(ui->lineEditRouteUserWaypointLatLon, &QLineEdit::textChanged, this, &RouteWaypointEditDialog::fieldsEditedDelayed);
  connect(ui->lineEditRouteUserWaypointLatLon, &QLineEdit::editingFinished, this, &RouteWaypointEditDialog::fieldsEdited);

  // Setup editing timeout
  editTimer.setSingleShot(true);
  editTimer.setInterval(EDIT_TIMEOUT_MS);
  connect(&editTimer, &QTimer::timeout, this, &RouteWaypointEditDialog::fieldsEdited);
}

RouteWaypointEditDialog::~RouteWaypointEditDialog()
{
  delete widgets;
  editTimer.stop();
  ui->plainTextEditRouteUserWaypointComment->removeEventFilter(eventFilter);
  saveState();
  delete flightplanEntry;
  delete flightplanEntrySaved;
  delete ui;
}

void RouteWaypointEditDialog::saveState()
{
  atools::gui::WidgetState state(lnm::ROUTE_USERWAYPOINT_DIALOG);
  state.setDialogOptions(true /* position */, true /* size */);
  state.save(this);
}

void RouteWaypointEditDialog::restoreState()
{
  atools::gui::WidgetState state(lnm::ROUTE_USERWAYPOINT_DIALOG);
  state.setDialogOptions(true /* position */, true /* size */);
  state.restore(this);
}

void RouteWaypointEditDialog::resetWindowLayout()
{
  atools::gui::WidgetState state(lnm::ROUTE_USERWAYPOINT_DIALOG);
  state.clear(this);
  state.syncSettings();

  atools::gui::centerWidgetOnScreen(this, defaultSize);
}

void RouteWaypointEditDialog::optionsChanged(const optc::OptionChangeFlags& changeFlags)
{
  if(changeFlags.testFlag(optc::OPTION_CHANGE_UNITS))
    coordsEdited();
}

void RouteWaypointEditDialog::fontChanged(const QFont& font)
{
  atools::gui::updateAllFonts(this, font, atools::gui::WidgetZoomHandler::getRegisteredWidgets());
}

void RouteWaypointEditDialog::setEntryAndShow(int index, const atools::fs::pln::FlightplanEntry& entryParam)
{
  // Send signal in case there are changes
  fieldsEdited();
  setEntryInternal(index, entryParam);

  // Restore window
  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  show();
  raise();
  activateWindow();
  ui->plainTextEditRouteUserWaypointComment->setFocus();
}

void RouteWaypointEditDialog::setEntryInternal(int index, const atools::fs::pln::FlightplanEntry& entryParam)
{
  *flightplanEntry = entryParam;
  entryIndex = index;

  // Hide some widgets when editing waypoint or when flightplanEntry was reset by clear()
  bool waypointEdit = flightplanEntry->getWaypointType() != atools::fs::pln::entry::USER &&
                      flightplanEntry->getWaypointType() != atools::fs::pln::entry::UNKNOWN;
  widgets->setHidden(waypointEdit);

  // Comment/remarks is always visible
  ui->plainTextEditRouteUserWaypointComment->setHidden(false);
  ui->labelRouteUserWaypointComment->setHidden(false);
  ui->labelRouteUserWaypointHeaderWaypointInfo->setHidden(!waypointEdit);

  if(flightplanEntry->getWaypointType() == atools::fs::pln::entry::UNKNOWN)
  {
    // Cleared / not valid - show editing hint
    widgets->setDisabled(true);
    ui->labelRouteUserWaypointHeaderHint->setHidden(false);
    ui->labelRouteUserWaypointHeaderHint->setDisabled(false);
  }
  else
  {
    widgets->setEnabled(true);

    // Valid - hide editing hint
    ui->labelRouteUserWaypointHeaderHint->setHidden(true);

    // Update dialog title
    if(waypointEdit)
    {
      setWindowTitle(QCoreApplication::applicationName() + tr(" - Edit Flight Plan Position Remarks"));

      // Update header with waypoint information
      updateHeaderLabel();
    }
    else
      setWindowTitle(QCoreApplication::applicationName() + tr(" - Edit Flight Plan Position"));
  }

  // Fill UI
  entryToWidgets(*flightplanEntry);
}

void RouteWaypointEditDialog::clearAndSave()
{
  if(isVisible())
  {
    fieldsEdited();
    *flightplanEntrySaved = *flightplanEntry;
    setEntryInternal(-1, atools::fs::pln::FlightplanEntry());
  }
}

void RouteWaypointEditDialog::updateFromRoute(const Route& route)
{
  if(isVisible())
  {
    const atools::fs::pln::Flightplan& plan = route.getFlightplanConst();
    for(int i = 0; i < plan.size(); i++)
    {
      if(isEqual(plan.at(i), *flightplanEntrySaved))
      {
        setEntryInternal(i, plan.at(i));
        break;
      }
    }
  }
}

void RouteWaypointEditDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Close))
  {
    // Send change signal and close window
    editTimer.stop();

    fieldsEdited();
    saveState();
    accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
  {
    if(flightplanEntry->getWaypointType() == atools::fs::pln::entry::USER)
      atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "EDITFPPOSITION.html", lnm::helpLanguageOnline());
    else
      atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "EDITFPREMARKS.html", lnm::helpLanguageOnline());
  }
}

void RouteWaypointEditDialog::fieldsEdited()
{
  if(flightplanEntry->getWaypointType() != atools::fs::pln::entry::UNKNOWN)
  {
    // Entry is valid - check for changes
    atools::fs::pln::FlightplanEntry entry;
    entry.setWaypointType(flightplanEntry->getWaypointType());
    widgetsToEntry(entry);

    if(entry != *flightplanEntry)
    {
      // Changed - send signal
      widgetsToEntry(*flightplanEntry);
      emit waypointEdited();
    }
  }
}

void RouteWaypointEditDialog::fieldsEditedDelayed()
{
  if(flightplanEntry->getWaypointType() != atools::fs::pln::entry::UNKNOWN)
    editTimer.start();
}

void RouteWaypointEditDialog::closeEvent(QCloseEvent *)
{
  // Closed by Esc or clicking X
  editTimer.stop();
  fieldsEdited();
}

void RouteWaypointEditDialog::entryToWidgets(const atools::fs::pln::FlightplanEntry& entry)
{
  if(entry.getWaypointType() == atools::fs::pln::entry::UNKNOWN)
  {
    // Entry not valid - clear all
    ui->lineEditRouteUserWaypointIdent->clear();
    ui->lineEditRouteUserWaypointRegion->clear();
    ui->lineEditRouteUserWaypointName->clear();
    ui->plainTextEditRouteUserWaypointComment->clear();
    ui->lineEditRouteUserWaypointLatLon->clear();
    coordsEdited(QStringLiteral());
  }
  else
  {
    // Change label depending on order
    if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
      ui->labelRouteUserWaypointLatLon->setText(tr("&Longitude and Latitude:"));
    else
      ui->labelRouteUserWaypointLatLon->setText(tr("&Latitude and Longitude:"));

    if(entry.getWaypointType() == atools::fs::pln::entry::USER)
    {
      // Copy all userpoint fields
      ui->lineEditRouteUserWaypointIdent->setText(entry.getIdent());
      ui->lineEditRouteUserWaypointRegion->setText(entry.getRegion());
      ui->lineEditRouteUserWaypointName->setText(entry.getName());
      ui->lineEditRouteUserWaypointLatLon->setText(Unit::coords(entry.getPosition()));
      coordsEdited(QStringLiteral());
    }

    // Commend is always copied
    ui->plainTextEditRouteUserWaypointComment->setPlainText(entry.getComment());
  }
}

void RouteWaypointEditDialog::widgetsToEntry(atools::fs::pln::FlightplanEntry& entry) const
{
  if(entry.getWaypointType() == atools::fs::pln::entry::USER)
  {
    // User defined point ============
    entry.setIdent(ui->lineEditRouteUserWaypointIdent->text());
    entry.setRegion(ui->lineEditRouteUserWaypointRegion->text());
    entry.setName(ui->lineEditRouteUserWaypointName->text());

    if(formatter::checkCoordinates(ui->lineEditRouteUserWaypointLatLon->text()))
    {
      bool hemisphere = false;
      atools::geo::Pos pos = atools::fs::util::fromAnyFormat(ui->lineEditRouteUserWaypointLatLon->text(), &hemisphere);

      if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
        // Parsing uses lat/lon - swap for lon/lat
        // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
        atools::fs::util::maybeSwapOrdinates(pos, ui->lineEditRouteUserWaypointLatLon->text());

      if(pos.isValid())
        entry.setPosition(pos);
    }
  }

  if(entry.getWaypointType() != atools::fs::pln::entry::UNKNOWN)
    // Comment is valid for all types
    entry.setComment(ui->plainTextEditRouteUserWaypointComment->toPlainText());
}

void RouteWaypointEditDialog::updateHeaderLabel()
{
  ui->labelRouteUserWaypointHeaderWaypointInfo->setText(tr("<b>%1 %2 region %3</b>").
                                                        arg(flightplanEntry->getWaypointTypeAsDisplayString()).
                                                        arg(flightplanEntry->getIdent()).
                                                        arg(flightplanEntry->getRegion()));
}

void RouteWaypointEditDialog::coordsEdited(const QString&)
{
  if(flightplanEntry->getWaypointType() == atools::fs::pln::entry::UNKNOWN)
    ui->labelRouteUserWaypointCoordStatus->setText(tr("Coordinates not set."));
  else if(flightplanEntry->getWaypointType() == atools::fs::pln::entry::USER)
  {
    QString message;
    formatter::checkCoordinates(message, ui->lineEditRouteUserWaypointLatLon->text());
    ui->labelRouteUserWaypointCoordStatus->setText(message);
  }
}

void RouteWaypointEditDialog::showEvent(QShowEvent *)
{
  QTimer::singleShot(10, this, &RouteWaypointEditDialog::restoreState);
}

void RouteWaypointEditDialog::hideEvent(QHideEvent *)
{
  saveState();
}
