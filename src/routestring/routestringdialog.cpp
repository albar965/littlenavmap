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

#include "routestring/routestringdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "navapp.h"
#include "route/routecontroller.h"
#include "routestring/routestringreader.h"
#include "routestring/routestringwriter.h"
#include "settings/settings.h"

#include "ui_routestringdialog.h"

#include <QClipboard>
#include <QMenu>
#include <QButtonGroup>
#include <QScrollBar>

const static int TEXT_CHANGE_DELAY_MS = 500;

using atools::gui::HelpHandler;
namespace apln = atools::fs::pln;

RouteStringDialog::RouteStringDialog(QWidget *parent, const QString& settingsSuffixParam)
  : QDialog(parent), ui(new Ui::RouteStringDialog), settingsSuffix(settingsSuffixParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  nonModal = parent == nullptr;

  // Use non-modal if parent is not given
  setWindowModality(nonModal ? Qt::NonModal : Qt::ApplicationModal);

  controller = NavApp::getRouteController();

  ui->setupUi(this);

  // Need to set text programatically since Qt Designer add garbage which messes up formatting on other platforms
  ui->textEditSyntaxHelp->setText(
    "<p><b><big>Quick Help</big></b><br/>"
      "<b>Format:</b> FROM[ETD] [SPEEDALT] [SIDTRANS] [ENROUTE] [STARTRANS] TO[ETA] [ALTERNATES]<br/>"
      "<b>Optional fields</b> are enclosed with <b>&quot;[]&quot;</b>.<br/>"
      "<b>FROM</b> is the required departure airport. Departure time <b>ETD</b> is ignored.<br/>"
      "<b>SPEEDALT</b> describes flight plan cruise altitude and speed. See manual for format details.<br/>"
      "<b>SIDTRANS</b> is a SID and an optional transition which can be given as <b>&quot;SID.TRANS&quot;</b> or <b>&quot;SID TRANS&quot;</b><br/>"
      "<b>ENROUTE</b> is a space separated list of navaids, navaid/airway/navaid combinations or user defined waypoints as coordinates.<br/>"
      "<b>STARTRANS</b> is a STAR and an optional transition which can be given as <b>&quot;STAR.TRANS&quot;</b>, "
        "<b>&quot;STAR TRANS&quot;</b>, <b>&quot;TRANS.STAR&quot;</b> or <b>&quot;TRANS STAR&quot;</b><br/>"
        "<b>TO</b> is the required destination airport. Arrival time <b>ETA</b> is ignored.<br/>"
        "<b>ALTERNATES</b> is a list of alternate or enroute airports depending on selected option.<br/>"
        "<b>Press the help button to open the online manual for more information.</b></p>");

  // Copy main menu actions to allow using shortcuts in the non-modal dialog too
  addActions(NavApp::getMainWindowActions());

  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#if !defined(Q_OS_LINUX) || defined(DEBUG_INFORMATION)
  // Make the splitter handle better visible
  ui->splitterRouteString->setStyleSheet(QString("QSplitter::handle { "
                                                 "background: %1;"
                                                 "image: url(:/littlenavmap/resources/icons/splitterhandvert.png); "
                                                 "}").
                                         arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif

  // Disallow collapsing of the upper and middle view
  ui->splitterRouteString->setCollapsible(0, false);
  ui->splitterRouteString->setCollapsible(1, false);

  // Set tooltips on splitter
  if(ui->splitterRouteString->handle(1) != nullptr)
    ui->splitterRouteString->handle(1)->setToolTip(tr("Resize upper and middle part."));
  if(ui->splitterRouteString->handle(2) != nullptr)
    ui->splitterRouteString->handle(2)->setToolTip(tr("Resize, open or close the quick help."));

  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#if defined(Q_OS_MACOS)
  fixedFont.setPointSizeF(fixedFont.pointSizeF() * 1.2);
#endif
  ui->plainTextEditRouteString->setFont(fixedFont);
  ui->plainTextEditRouteString->setWordWrapMode(QTextOption::WrapAnywhere);

  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create &Flight Plan"));

  // Non-modal dialog has a close button and modal dialog a cancel button
  if(parent == nullptr)
    ui->buttonBoxRouteString->button(QDialogButtonBox::Cancel)->hide();
  else
    ui->buttonBoxRouteString->button(QDialogButtonBox::Close)->hide();

  ui->pushButtonRouteStringFromClipboard->setVisible(parent == nullptr);
  ui->pushButtonRouteStringUpdate->setVisible(parent == nullptr);

  flightplan = new apln::Flightplan;
  flightplan->setFlightplanType(atools::fs::pln::NO_TYPE); // Set type to none to take it from GUI when creating plan

  routeStringWriter = new RouteStringWriter();
  routeStringReader = new RouteStringReader(controller->getFlightplanEntryBuilder());

  buildButtonMenu();

  connect(ui->pushButtonRouteStringFromClipboard, &QPushButton::clicked, this, &RouteStringDialog::fromClipboardClicked);
  connect(ui->pushButtonRouteStringToClipboard, &QPushButton::clicked, this, &RouteStringDialog::toClipboardClicked);
  connect(ui->plainTextEditRouteString, &QPlainTextEdit::textChanged, this, &RouteStringDialog::updateButtonState);
  connect(ui->plainTextEditRouteString, &QPlainTextEdit::textChanged, this, &RouteStringDialog::textChanged);
  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &RouteStringDialog::updateButtonState);
  connect(ui->buttonBoxRouteString, &QDialogButtonBox::clicked, this, &RouteStringDialog::buttonBoxClicked);
  connect(ui->toolButtonRouteStringOptions->menu(), &QMenu::triggered, this, &RouteStringDialog::toolButtonOptionTriggered);
  connect(ui->pushButtonRouteStringUpdate, &QPushButton::clicked, this, &RouteStringDialog::updateButtonClicked);

  connect(ui->pushButtonRouteStringShowHelp, &QPushButton::toggled, this, &RouteStringDialog::showHelpButtonToggled);
  connect(ui->splitterRouteString, &QSplitter::splitterMoved, this, &RouteStringDialog::splitterMoved);

  connect(&textUpdateTimer, &QTimer::timeout, this, &RouteStringDialog::textChangedDelayed);
  textUpdateTimer.setSingleShot(true);
}

RouteStringDialog::~RouteStringDialog()
{
  textUpdateTimer.stop();
  delete routeStringWriter;
  delete routeStringReader;
  delete procActionGroup;
  delete ui;
  delete flightplan;
}

void RouteStringDialog::buildButtonMenu()
{
  // Build options dropdown menu ====================================================
  // Add tear off menu =======
  ui->toolButtonRouteStringOptions->setMenu(new QMenu(ui->toolButtonRouteStringOptions));
  QMenu *buttonMenu = ui->toolButtonRouteStringOptions->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  // Writing to string ===========================================
  QAction *action;
  action = new QAction(tr("Write departure and destination airport"), buttonMenu);
  action->setObjectName("actionDepartDest");
  action->setToolTip(tr("Omit departure and destination airport ident.\n"
                        "Note that the resulting description cannot be read into a flight plan."));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::START_AND_DEST));
  buttonMenu->addAction(action);

  action = new QAction(tr("Write DCT (direct) instructions"), buttonMenu);
  action->setObjectName("actionDct");
  action->setToolTip(tr("Fill direct connections between waypoints with a \"DCT\""));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::DCT));
  buttonMenu->addAction(action);

  action = new QAction(tr("Write cruise speed and altitude instruction"), buttonMenu);
  action->setObjectName("actionSpeedAlt");
  action->setToolTip(tr("Add cruise speed and altitude to description.\n"
                        "Speed is ignored in favor to currently loaded aircraft performance\n"
                        "when reading a description into a flight plan."));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::ALT_AND_SPEED));
  buttonMenu->addAction(action);

  action = new QAction(tr("Write Waypoints instead of Airways"), buttonMenu);
  action->setObjectName("actionWaypoints");
  action->setToolTip(tr("Ignore airways and add all waypoints instead"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::NO_AIRWAYS));
  buttonMenu->addAction(action);

  action = new QAction(tr("Write Alternates"), buttonMenu);
  action->setObjectName("actionAlternates");
  action->setToolTip(tr("Add the ICAO code for all alternate airports to the end of the description"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::ALTERNATES));
  buttonMenu->addAction(action);

  buttonMenu->addSeparator();

  // SID/STAR group ===========================================
  procActionGroup = new QActionGroup(buttonMenu);
  if(NavApp::hasSidStarInDatabase())
  {
    action = new QAction(tr("Write SID and STAR"), buttonMenu);
    action->setObjectName("actionSidStar");
    action->setToolTip(tr("Write SID, STAR and the respective transitions to the description"));
    action->setCheckable(true);
    action->setData(static_cast<int>(rs::SID_STAR));
    buttonMenu->addAction(action);
    procActionGroup->addAction(action);
  }

  action = new QAction(tr("Write generic SID and STAR"), buttonMenu);
  action->setObjectName("actionGenericSidStar");
  action->setToolTip(tr("Add \"SID\" and \"STAR\" words only instead of the real procedure names"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::SID_STAR_GENERIC));
  buttonMenu->addAction(action);
  procActionGroup->addAction(action);

  action = new QAction(tr("Write no SID and STAR"), buttonMenu);
  action->setObjectName("actionNoSidStar");
  action->setToolTip(tr("Add neither SID nor STAR to the description"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::SID_STAR_NONE));
  buttonMenu->addAction(action);
  procActionGroup->addAction(action);

  buttonMenu->addSeparator();

  action = new QAction(tr("Write STAR and transition reversed"), buttonMenu);
  action->setObjectName("actionReversedStar");
  action->setToolTip(tr("Write \"TRANS.STAR\" instead of \"STAR.TRANS\""));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::STAR_REV_TRANSITION));
  buttonMenu->addAction(action);

  action = new QAction(tr("Write SID/STAR and transition space separated"), buttonMenu);
  action->setObjectName("actionSpaceSidStar");
  action->setToolTip(tr("Use a space to separate SID, STAR and transition"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::SID_STAR_SPACE));
  buttonMenu->addAction(action);

  buttonMenu->addSeparator();

  // Reading from string ===========================================
  action = new QAction(tr("Read trailing Airports as Alternates"), buttonMenu);
  action->setObjectName("actionTrailingAlternates");
  action->setToolTip(tr("A list of airports at the end of the description will be read as alternate "
                        "airports when reading if checked.\n"
                        "Otherwise airports are added as waypoints."));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_ALTERNATES));
  buttonMenu->addAction(action);

  action = new QAction(tr("Read first and last item as Navaid"), buttonMenu);
  action->setObjectName("actionNavaid");
  action->setToolTip(tr("Does not expect the first and last string item to be an airport ICAO ident if checked"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_NO_AIRPORTS));
  buttonMenu->addAction(action);

  action = new QAction(tr("Read: Match coordinates to Waypoints"), buttonMenu);
  action->setObjectName("actionMatchCoords");
  action->setToolTip(tr("Coordinates will be converted to navaids if nearby"));
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_MATCH_WAYPOINTS));
  buttonMenu->addAction(action);
}

void RouteStringDialog::splitterMoved()
{
  ui->pushButtonRouteStringShowHelp->blockSignals(true);
  ui->pushButtonRouteStringShowHelp->setChecked(ui->splitterRouteString->sizes().value(2) > 0);
  ui->pushButtonRouteStringShowHelp->blockSignals(false);
}

void RouteStringDialog::showHelpButtonToggled(bool checked)
{
  QList<int> sizes = ui->splitterRouteString->sizes();
  int total = 0;
  for(int size : sizes)
    total += size;

  if(sizes.size() == 3)
  {
    int topSize = sizes.at(0);
    if(checked)
    {
      // Make space for lower widget and reduce only the size of the middle one - leave upper as is
      sizes[1] = (total - topSize) / 2;
      sizes[2] = (total - topSize) / 2;
    }
    else
    {
      // Enlarge middle widget and collapse lower one
      sizes[1] = total - topSize;
      sizes[2] = 0;
    }
    ui->splitterRouteString->setSizes(sizes);

    if(checked)
      // Scroll to top
      ui->textEditSyntaxHelp->verticalScrollBar()->setValue(0);
  }
}

void RouteStringDialog::updateButtonClicked()
{
  plainTextEditRouteStringSet(routeStringWriter->createStringForRoute(NavApp::getRouteConst(), NavApp::getRouteCruiseSpeedKts(), options));
}

void RouteStringDialog::toolButtonOptionTriggered(QAction *action)
{
  Q_UNUSED(action)

  if(updatingActions)
    return;

  qDebug() << Q_FUNC_INFO << action->objectName() << action->data();

  // Copy menu state for options bitfield
  for(const QAction *act : ui->toolButtonRouteStringOptions->menu()->actions())
  {
    rs::RouteStringOptions opts(act->data().toInt());
    if(act->isChecked())
      options |= opts;
    else
      options &= ~opts;
  }
  textChangedDelayed();
}

const atools::fs::pln::Flightplan& RouteStringDialog::getFlightplan() const
{
  return *flightplan;
}

void RouteStringDialog::saveState()
{
  saveStateWidget();
  atools::settings::Settings::instance().setValue(lnm::ROUTE_STRING_DIALOG_DESCR + settingsSuffix,
                                                  ui->plainTextEditRouteString->toPlainText());
}

void RouteStringDialog::saveStateWidget()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER + settingsSuffix).save({this, ui->splitterRouteString});
  atools::settings::Settings::instance().setValue(lnm::ROUTE_STRING_DIALOG_OPTIONS + settingsSuffix, static_cast<int>(options));
}

void RouteStringDialog::restoreState(const QString& routeString)
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER + settingsSuffix).restore({this, ui->splitterRouteString});
  ui->splitterRouteString->setHandleWidth(6);
  options = getOptionsFromSettings();
  updateButtonState();

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(routeString.isEmpty())
    // Load from settings
    plainTextEditRouteStringSet(settings.valueStr(lnm::ROUTE_STRING_DIALOG_DESCR + settingsSuffix));
  else
    plainTextEditRouteStringSet(routeString);

  splitterMoved();
}

rs::RouteStringOptions RouteStringDialog::getOptionsFromSettings()
{
  return rs::RouteStringOptions(atools::settings::Settings::instance().valueInt(lnm::ROUTE_STRING_DIALOG_OPTIONS, rs::DEFAULT_OPTIONS));
}

void RouteStringDialog::textChanged()
{
  if(immediateUpdate)
  {
    // Update without delay timer
    textUpdateTimer.stop();
    immediateUpdate = false;
    textChangedDelayed();
  }
  else
    // Calls RouteStringDialog::textChangedDelayed()
    textUpdateTimer.start(TEXT_CHANGE_DELAY_MS);
}

void RouteStringDialog::textChangedDelayed()
{
  qDebug() << Q_FUNC_INFO;

  flightplan->clear();
  flightplan->setFlightplanType(atools::fs::pln::NO_TYPE); // Set type to none to take it from GUI when creating plan

  QString errorString;
  if(!ui->plainTextEditRouteString->toPlainText().isEmpty())
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    routeStringReader->createRouteFromString(ui->plainTextEditRouteString->toPlainText(), options | rs::REPORT, flightplan, nullptr,
                                             &speedKts, &altitudeIncluded);
    QGuiApplication::restoreOverrideCursor();

    // Fill report into widget
    errorString.append(routeStringReader->getMessages().join("<br/>"));
  }

  atools::gui::util::updateTextEdit(ui->textEditRouteStringErrors, errorString, false /* scrollToTop */, true /* keepSelection */);

  // Avoid update issues with macOS and mac style - force repaint
  ui->textEditRouteStringErrors->repaint();
  updateButtonState();
}

void RouteStringDialog::fromClipboardClicked()
{
  plainTextEditRouteStringSet(rs::cleanRouteString(QGuiApplication::clipboard()->text()).join(" "));
}

void RouteStringDialog::toClipboardClicked()
{
  QGuiApplication::clipboard()->setText(ui->plainTextEditRouteString->toPlainText());
}

/* A button box button was clicked */
void RouteStringDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Ok))
  {
    if(nonModal)
      // Create a new flight plan and use undo/redo - keep non-modal dialog open - do not mark plan as changed
      emit routeFromFlightplan(*flightplan, isAltitudeIncluded(), false /* changed */, true /* undo */);
    else
      // Return QDialog::Accepted and close
      QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "ROUTEDESCR.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Close) ||
          button == ui->buttonBoxRouteString->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void RouteStringDialog::plainTextEditRouteStringSet(const QString& text)
{
  QTextDocument *doc = ui->plainTextEditRouteString->document();

  if(doc != nullptr)
  {
    // Do not delay update in RouteStringDialog::textChanged()
    immediateUpdate = true;

    // Remember cursor position
    QCursor currentCursor = ui->plainTextEditRouteString->cursor();

    // Select whole document and replace text - this keeps the undo stack
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(text);
    cursor.endEditBlock();

    // Restore cursor position
    ui->plainTextEditRouteString->setCursor(currentCursor);
  }
}

void RouteStringDialog::updateButtonState()
{
  ui->pushButtonRouteStringUpdate->setEnabled(!NavApp::getRouteConst().isEmpty());
  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setDisabled(flightplan->getEntries().isEmpty());
  ui->pushButtonRouteStringToClipboard->setDisabled(ui->plainTextEditRouteString->toPlainText().isEmpty());
  ui->pushButtonRouteStringFromClipboard->setDisabled(QGuiApplication::clipboard()->text().simplified().isEmpty());

  // Copy option flags to dropdown menu items
  updatingActions = true;
  for(QAction *act : ui->toolButtonRouteStringOptions->menu()->actions())
    act->setChecked(rs::RouteStringOptions(act->data().toInt()) & options);
  updatingActions = false;
}

void RouteStringDialog::showEvent(QShowEvent *)
{
  if(!position.isNull())
    move(position);
}

void RouteStringDialog::hideEvent(QHideEvent *)
{
  position = geometry().topLeft();
}
