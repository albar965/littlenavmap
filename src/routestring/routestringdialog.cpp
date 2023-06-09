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

#include "routestring/routestringdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/tools.h"
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
#include <QFontDatabase>
#include <QSyntaxHighlighter>

const static int TEXT_CHANGE_DELAY_MS = 500;

using atools::gui::HelpHandler;
namespace apln = atools::fs::pln;

/* Makes first block bold and rest gray to indicate active description text. */
class SyntaxHighlighter :
  public QSyntaxHighlighter
{
public:
  SyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
  {
  }

  void styleChanged()
  {
    formatHighlight.setFontWeight(QFont::Bold);
    formatNormal.setForeground(NavApp::isCurrentGuiStyleNight() ? QColor(180, 180, 180) : QColor(100, 100, 100));
  }

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

void SyntaxHighlighter::highlightBlock(const QString& text)
{
  if(previousBlockState() == BEFORE_START)
    setCurrentBlockState(text.isEmpty() ? BEFORE_START : IN_HIGHLIGHT_BLOCK);
  else if(previousBlockState() == IN_HIGHLIGHT_BLOCK)
    setCurrentBlockState(text.isEmpty() ? AFTER_HIGHLIGHT_BLOCK : IN_HIGHLIGHT_BLOCK);
  else if(previousBlockState() == AFTER_HIGHLIGHT_BLOCK)
    setCurrentBlockState(AFTER_HIGHLIGHT_BLOCK);

  // In text block and have text - merge current formatting
  setFormat(0, text.size(), currentBlockState() == IN_HIGHLIGHT_BLOCK ? formatHighlight : formatNormal);
}

// =================================================================================================
// =================================================================================================
// =================================================================================================

RouteStringDialog::RouteStringDialog(QWidget *parent, const QString& settingsSuffixParam)
  : QDialog(parent), ui(new Ui::RouteStringDialog), settingsSuffix(settingsSuffixParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  blocking = parent != nullptr;

  // Use non-modal if parent is not given
  setWindowModality(blocking ? Qt::ApplicationModal : Qt::NonModal);

  controller = NavApp::getRouteController();

  ui->setupUi(this);

  // Need to set text programatically since Qt Designer add garbage which messes up formatting on other platforms
  ui->textEditSyntaxHelp->setText(
    tr("<p><b><big>Quick Help</big></b><br/>"
       "<b>Format:</b> FROM[ETD] [SPEEDALT] [SIDTRANS] [ENROUTE] [STARTRANS] TO[ETA] [ALTERNATES]<br/>"
       "<b>Optional fields</b> are enclosed with <b>&quot;[]&quot;</b>.<br/>"
       "<b>FROM</b> is the required departure airport. Departure time <b>ETD</b> is ignored.<br/>"
       "<b>SPEEDALT</b> describes flight plan cruise altitude and speed. See manual for format details.<br/>"
       "<b>SIDTRANS</b> is a SID and an optional transition which can be given as <b>&quot;SID.TRANS&quot;</b> or <b>&quot;SID TRANS&quot;</b>.<br/>"
       "<b>ENROUTE</b> is a space separated list of navaids, navaid/airway/navaid combinations or user defined waypoints as coordinates.<br/>"
       "<b>STARTRANS</b> is a STAR and an optional transition which can be given as <b>&quot;STAR.TRANS&quot;</b>, "
         "<b>&quot;STAR TRANS&quot;</b>, <b>&quot;TRANS.STAR&quot;</b> or <b>&quot;TRANS STAR&quot;</b><br/>"
         "<b>TO</b> is the required destination airport. Arrival time <b>ETA</b> is ignored.<br/>"
         "<b>ALTERNATES</b> is a list of alternate or en-route airports depending on selected option.<br/>"
         "<b>Press the help button to open the online manual for more information.</b></p>"));

  // Copy main menu actions to allow using shortcuts in the non-modal dialog too
  if(!blocking)
    addActions(NavApp::getMainWindowActions());

  // Disallow collapsing of the upper and middle view
  ui->splitterRouteString->setCollapsible(0, false);
  ui->splitterRouteString->setCollapsible(1, false);

  // Set tooltips on splitter
  if(ui->splitterRouteString->handle(1) != nullptr)
    ui->splitterRouteString->handle(1)->setToolTip(tr("Resize upper and middle part."));
  if(ui->splitterRouteString->handle(2) != nullptr)
    ui->splitterRouteString->handle(2)->setToolTip(tr("Resize, open or close the quick help."));

  // Need to clear, otherwise default font cannot be applied
  ui->textEditRouteString->clear();

  QFont fixedFont = atools::gui::getBestFixedFont();
  qDebug() << Q_FUNC_INFO << "fixedFont" << fixedFont;
  ui->textEditRouteString->document()->setDefaultFont(fixedFont);

  sytaxHighlighter = new SyntaxHighlighter(ui->textEditRouteString);

  if(blocking)
  {
    // Opened from SimBrief download
    ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create &Flight Plan and Close"));
    ui->buttonBoxRouteString->button(QDialogButtonBox::Apply)->hide();
  }
  else
  {
    // No parent and free floating
    ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create &Flight Plan"));
    ui->buttonBoxRouteString->button(QDialogButtonBox::Apply)->setText(tr("Create &Flight Plan and Close"));
  }

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
  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &RouteStringDialog::updateButtonState);
  connect(ui->buttonBoxRouteString, &QDialogButtonBox::clicked, this, &RouteStringDialog::buttonBoxClicked);
  connect(ui->toolButtonRouteStringOptions->menu(), &QMenu::triggered, this, &RouteStringDialog::toolButtonOptionTriggered);
  connect(ui->pushButtonRouteStringUpdate, &QPushButton::clicked, this, &RouteStringDialog::updateButtonClicked);

  connect(ui->textEditRouteString, &QTextEdit::textChanged, this, &RouteStringDialog::updateButtonState);
  connect(ui->textEditRouteString, &QTextEdit::textChanged, this, &RouteStringDialog::textChanged);
  connect(ui->textEditRouteString, &QTextEdit::undoAvailable, ui->pushButtonRouteStringUndo, &QPushButton::setEnabled);
  connect(ui->textEditRouteString, &QTextEdit::redoAvailable, ui->pushButtonRouteStringRedo, &QPushButton::setEnabled);
  connect(ui->pushButtonRouteStringUndo, &QPushButton::clicked, ui->textEditRouteString, &QTextEdit::undo);
  connect(ui->pushButtonRouteStringRedo, &QPushButton::clicked, ui->textEditRouteString, &QTextEdit::redo);

  connect(ui->pushButtonRouteStringShowHelp, &QPushButton::toggled, this, &RouteStringDialog::showHelpButtonToggled);
  connect(ui->splitterRouteString, &QSplitter::splitterMoved, this, &RouteStringDialog::splitterMoved);

  connect(&textUpdateTimer, &QTimer::timeout, this, &RouteStringDialog::textChangedDelayed);
  textUpdateTimer.setSingleShot(true);

  // Apply splitter and text formats
  styleChanged();
}

RouteStringDialog::~RouteStringDialog()
{
  sytaxHighlighter->setDocument(nullptr);
  delete sytaxHighlighter;

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

  if(!blocking) // Do not show write options if opened from SimBrief
  {
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
  }

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
  textEditRouteStringPrepend(routeStringWriter->createStringForRoute(NavApp::getRouteConst(), NavApp::getRouteCruiseSpeedKts(),
                                                                     options | rs::ALT_AND_SPEED_METRIC), true /* newline*/);
  ui->textEditRouteString->setFocus();
}

void RouteStringDialog::toolButtonOptionTriggered(QAction *)
{
  if(updatingActions)
    return;

  // Copy menu state for options bitfield
  for(const QAction *action : ui->toolButtonRouteStringOptions->menu()->actions())
    options.setFlag(static_cast<rs::RouteStringOption>(action->data().toInt()), action->isChecked());

  // Call immediately and update even if string is unchanged
  textChangedInternal(true /* forceUpdate */);
}

void RouteStringDialog::saveState()
{
  saveStateWidget();

  if(!blocking)
    // Do not save contents for modal dialog
    atools::settings::Settings::instance().setValue(lnm::ROUTE_STRING_DIALOG_DESCR + settingsSuffix,
                                                    ui->textEditRouteString->toPlainText());
}

void RouteStringDialog::saveStateWidget()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER + settingsSuffix).save({this, ui->splitterRouteString});
  atools::settings::Settings::instance().setValue(lnm::ROUTE_STRING_DIALOG_OPTIONS + settingsSuffix, static_cast<int>(options));
}

void RouteStringDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER + settingsSuffix).restore({this, ui->splitterRouteString});
  ui->splitterRouteString->setHandleWidth(6);
  options = getOptionsFromSettings();
  updateButtonState();

  // Do not restore contents for modal dialog
  if(!blocking)
  {
    atools::settings::Settings& settings = atools::settings::Settings::instance();
    // Load from settings
    textEditRouteStringPrepend(settings.valueStr(lnm::ROUTE_STRING_DIALOG_DESCR + settingsSuffix), false /* newline*/);

    // Avoid clean undo step
    ui->textEditRouteString->setUndoRedoEnabled(true);

    // Move cursor to top of document
    ui->textEditRouteString->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
  }

  splitterMoved();
}

void RouteStringDialog::addRouteDescription(const QString& routeString)
{
  // Load passed string from SimBrief or other
  textEditRouteStringPrepend(routeString, false /* newline*/);

  // Avoid clean undo step
  ui->textEditRouteString->setUndoRedoEnabled(true);

  ui->textEditRouteString->setFocus();
}

void RouteStringDialog::styleChanged()
{
  if(sytaxHighlighter != nullptr)
    sytaxHighlighter->styleChanged();

  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#if !defined(Q_OS_LINUX) || defined(DEBUG_INFORMATION)
  // Make the splitter handle better visible
  ui->splitterRouteString->setStyleSheet(QString("QSplitter::handle { "
                                                 "background: %1;"
                                                 "image: url(:/littlenavmap/resources/icons/splitterhandvert.png); "
                                                 "}").arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif
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
    textChangedInternal(false /* forceUpdate */);
  }
  else
    // Calls RouteStringDialog::textChangedDelayed()
    textUpdateTimer.start(TEXT_CHANGE_DELAY_MS);
}

void RouteStringDialog::textChangedDelayed()
{
  textChangedInternal(false /* forceUpdate */);
}

void RouteStringDialog::textChangedInternal(bool forceUpdate)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  QString currentRouteString = rs::cleanRouteString(ui->textEditRouteString->toPlainText());

  // Compare with last read string to avoid needless updates
  if(forceUpdate || currentRouteString != lastCleanRouteString)
  {
    QString errorString;
    lastCleanRouteString = currentRouteString;

    if(!currentRouteString.isEmpty())
    {
      if(currentRouteString.size() < 1024)
      {
        flightplan->clearAll();
        flightplan->setFlightplanType(atools::fs::pln::NO_TYPE); // Set type to none to take it from GUI when creating plan

        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        routeStringReader->createRouteFromString(currentRouteString, options | rs::REPORT, flightplan, nullptr, &speedKts,
                                                 &altitudeIncluded);
        QGuiApplication::restoreOverrideCursor();

        // Fill report into widget
        errorString.append(routeStringReader->getMessages().join("<br/>"));
        atools::gui::util::updateTextEdit(ui->textEditRouteStringErrors, errorString, false /* scrollToTop */, true /* keepSelection */);
      }
      else
        atools::gui::util::updateTextEdit(ui->textEditRouteStringErrors, tr("Description is too long."), false /* scrollToTop */,
                                          true /* keepSelection */);
    }
    else
      atools::gui::util::updateTextEdit(ui->textEditRouteStringErrors, QString(), false /* scrollToTop */, true /* keepSelection */);
  }

  // Avoid update issues with macOS and mac style - force repaint
  ui->textEditRouteStringErrors->repaint();
  updateButtonState();
}

void RouteStringDialog::fromClipboardClicked()
{
  // Clean and insert all text
  textEditRouteStringPrepend(rs::cleanRouteStringLine(QGuiApplication::clipboard()->text()), true /* newline*/);
  ui->textEditRouteString->setFocus();
}

void RouteStringDialog::toClipboardClicked()
{
  // Put only first block into clipboard
  QGuiApplication::clipboard()->setText(rs::cleanRouteString(ui->textEditRouteString->toPlainText()));
}

/* A button box button was clicked */
void RouteStringDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Ok))
  {
    if(blocking)
      // Opened from SimBrief or other dialogs - Return QDialog::Accepted and close
      QDialog::accept();
    else
      // Floating window - Create a new flight plan and use undo/redo - keep non-modal dialog open - do not mark plan as changed
      emit routeFromFlightplan(*flightplan, !isAltitudeIncluded() /* adjustAltitude */, false /* changed */, true /* undo */);
  }
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Apply))
  {
    // Only in floating window - create a new flight plan and use undo/redo - keep non-modal dialog open - do not mark plan as changed
    emit routeFromFlightplan(*flightplan, !isAltitudeIncluded() /* adjustAltitude */, false /* changed */, true /* undo */);
    // Return QDialog::Accepted and close
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "ROUTEDESCR.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Close) ||
          button == ui->buttonBoxRouteString->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void RouteStringDialog::textEditRouteStringPrepend(const QString& text, bool newline)
{
  // Do not delay update in RouteStringDialog::textChanged()
  immediateUpdate = true;

  /// Do not add for empty documents
  if(ui->textEditRouteString->document()->isEmpty())
    newline = false;

  QTextCursor cursor = ui->textEditRouteString->textCursor();

  // Insert string at start of document and create one undo step
  cursor.beginEditBlock();
  cursor.movePosition(QTextCursor::Start);
  cursor.insertText(text);
  if(newline)
    cursor.insertText("\n\n");
  cursor.endEditBlock();

  // Move back to start
  cursor.movePosition(QTextCursor::Start);

  ui->textEditRouteString->setTextCursor(cursor);
}

void RouteStringDialog::updateButtonState()
{
  ui->pushButtonRouteStringUpdate->setEnabled(!NavApp::getRouteConst().isEmpty());
  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setDisabled(flightplan->isEmpty());
  ui->pushButtonRouteStringToClipboard->setDisabled(ui->textEditRouteString->toPlainText().isEmpty());
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
  // Hide tear off menu - otherwise it cannot be opened again
  ui->toolButtonRouteStringOptions->menu()->hideTearOffMenu();
  position = geometry().topLeft();
}
