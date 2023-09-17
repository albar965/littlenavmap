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

#include "routeexport/routemultiexportdialog.h"
#include "ui_routemultiexportdialog.h"

#include "atools.h"
#include "common/constants.h"
#include "fs/pln/flightplan.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "routeexport/routeexportformat.h"
#include "settings/settings.h"
#include "util/htmlbuilder.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QDir>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStringBuilder>

using atools::settings::Settings;

// Properties set to buttons for RouteExportFormatType and table row
const static char FORMAT_PROP_NAME[] = "format";
const static char ROW_PROP_NAME[] = "row";

// Object names of buttons which are children of the cell widget
const static QLatin1String OBJ_NAME_CHECKBOX("MultiExpCheckBox");
const static QLatin1String OBJ_NAME_EXP_SELECT("MultiExpSelectButton");
const static QLatin1String OBJ_NAME_EXP_SAVE("MultiExpSaveButton");
const static QLatin1String OBJ_NAME_EXP_RESET("MultiExpResetButton");

// Data role for checkbox status set in model for first BUTTONS column
// Needed for sorting
static int CHECK_STATE_ROLE = Qt::UserRole;

// Data role for RouteExportFormatType set in model for all columns
static int FORMAT_TYPE_ROLE = Qt::UserRole + 1;

enum Columns
{
  FIRST_COL,
  BUTTONS = FIRST_COL, /* Three buttons in a layout as item */
  CATEGORY, /* Simulator, aircraft, etc. */
  DESCRIPTION, /* Short description with tooltip */
  PATTERN, /* File export pattern including suffix */
  PATH, /* Full path to file */
  LAST_COL = PATH
};

// TableItemDelegate  ==================================================================================================

/* Delegate to paint errors in normal and selected state in red */
class TableItemDelegate :
  public QStyledItemDelegate
{
public:
  explicit TableItemDelegate(QObject *parent, const RouteExportFormatMap *formatMap)
    : QStyledItemDelegate(parent), formats(formatMap)
  {
  }

  virtual ~TableItemDelegate() override
  {

  }

private:
  virtual void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  const RouteExportFormatMap *formats;
};

void TableItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  const RouteExportFormat& format = (*formats)[static_cast<rexp::RouteExportFormatType>(index.data(FORMAT_TYPE_ROLE).toInt())];
  QStyleOptionViewItem styleItem(option);

  bool invalid = false;

  if(index.column() == PATTERN && !format.isPatternValid())
    // Pattern is invalid - draw dark red for selection and light red warning for normal state in background
    invalid = true;

  if(format.isSelected())
  {
    // Set whole row to bold if selected for multiexport =========================
    styleItem.font.setBold(true);

    if(index.column() == PATH && !format.isPathValid())
      // Path is invalid - draw dark red for selection and light red warning for normal state in background
      invalid = true;
  }

  if(invalid)
  {
    styleItem.palette.setColor(QPalette::Highlight, QColor(Qt::red).darker(130));
    styleItem.palette.setColor(QPalette::HighlightedText, Qt::white);
    styleItem.palette.setColor(QPalette::Text, Qt::white);

    // Delegate does not paint background
    painter->fillRect(option.rect, Qt::red);
  }

  QStyledItemDelegate::paint(painter, styleItem, index);
}

// TableSortProxyModel  ==================================================================================================

/* Proxy is needed to allow sorting by checkbox state */
class TableSortProxyModel
  : public QSortFilterProxyModel
{
public:
  explicit TableSortProxyModel(QTableView *tableView)
    : QSortFilterProxyModel(tableView)
  {

  }

  virtual ~TableSortProxyModel() override
  {

  }

private:
  virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const override;

};

bool TableSortProxyModel::lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
{
  if(leftIndex.column() == BUTTONS && rightIndex.column() == BUTTONS)
  {
    // Button column uses user role data for checkbox status
    bool leftData = sourceModel()->data(leftIndex, CHECK_STATE_ROLE).toBool();
    bool rightData = sourceModel()->data(rightIndex, CHECK_STATE_ROLE).toBool();

    // Workaround a bug in older Qt versions where rows jump after changing
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    if(leftData == rightData)
    {
      QString leftData = sourceModel()->data(sourceModel()->index(leftIndex.row(), DESCRIPTION)).toString();
      QString rightData = sourceModel()->data(sourceModel()->index(rightIndex.row(), DESCRIPTION)).toString();
      return QString::localeAwareCompare(leftData, rightData) < 0;
    }
    else
      return leftData > rightData;

#else
    return leftData < rightData;

#endif
  }
  else
    return QString::localeAwareCompare(sourceModel()->data(leftIndex).toString(),
                                       sourceModel()->data(rightIndex).toString()) < 0;
}

// RouteMultiExportDialog methods ==============================================================================

RouteMultiExportDialog::RouteMultiExportDialog(QWidget *parent, RouteExportFormatMap *exportFormatMap)
  : QDialog(parent), formatMapSystem(exportFormatMap), ui(new Ui::RouteMultiExportDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);
  formatMapDialog = new RouteExportFormatMap;

  ui->tableViewRouteExport->addActions({ui->actionSelectExportPath, ui->actionExportFileNow, ui->actionResetView,
                                        ui->actionIncreaseTextSize, ui->actionDecreaseTextSize, ui->actionEditPath, ui->actionEditPattern,
                                        ui->actionDefaultTextSize, ui->actionSelect});

  ui->actionSelectExportPath->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionExportFileNow->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionResetView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionIncreaseTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionDecreaseTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionEditPath->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionEditPattern->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionDefaultTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSelect->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Create own item model
  itemModel = new QStandardItemModel(this);

  // Set delegate for background color
  ui->tableViewRouteExport->setItemDelegate(new TableItemDelegate(this, formatMapDialog));

  // Create model and sort proxy
  proxyModel = new TableSortProxyModel(ui->tableViewRouteExport);
  proxyModel->setSourceModel(itemModel);
  ui->tableViewRouteExport->setModel(proxyModel);

  // Allow moving of section except the first BUTTON section
  ui->tableViewRouteExport->horizontalHeader()->setSectionsMovable(true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  ui->tableViewRouteExport->horizontalHeader()->setFirstSectionMovable(false);
#endif

  // Resize widget to get rid of the too large default margins and allow to change size from context menu
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableViewRouteExport, ui->actionIncreaseTextSize,
                                                     ui->actionDecreaseTextSize, ui->actionDefaultTextSize,
                                                     lnm::ROUTE_EXPORT_DIALOG_ZOOM, 1.);

  connect(ui->buttonBoxRouteExport, &QDialogButtonBox::clicked, this, &RouteMultiExportDialog::buttonBoxClicked);
  ui->buttonBoxRouteExport->button(QDialogButtonBox::SaveAll)->setText(tr("&Export Selected Formats"));

  // Detect inline edit by double click
  connect(itemModel, &QStandardItemModel::itemChanged, this, &RouteMultiExportDialog::itemChanged);
  connect(ui->tableViewRouteExport->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &RouteMultiExportDialog::selectionChanged);

  // Set up context menu for table ==================================================
  ui->tableViewRouteExport->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableViewRouteExport, &QWidget::customContextMenuRequested, this, &RouteMultiExportDialog::tableContextMenu);

  connect(ui->actionSelectExportPath, &QAction::triggered, this, &RouteMultiExportDialog::actionSelectExportPathTriggered);
  connect(ui->actionResetExportPath, &QAction::triggered, this, &RouteMultiExportDialog::actionResetExportPathTriggered);
  connect(ui->actionResetFilePattern, &QAction::triggered, this, &RouteMultiExportDialog::actionResetExportPatternTriggered);
  connect(ui->actionExportFileNow, &QAction::triggered, this, &RouteMultiExportDialog::actionExportFileNowTriggered);
  connect(ui->actionEditPath, &QAction::triggered, this, &RouteMultiExportDialog::actionEditPathTriggered);
  connect(ui->actionEditPattern, &QAction::triggered, this, &RouteMultiExportDialog::actionEditPatternTriggered);
  connect(ui->actionSelect, &QAction::triggered, this, &RouteMultiExportDialog::actionSelectTriggered);
}

RouteMultiExportDialog::~RouteMultiExportDialog()
{
  saveDialogState();

  delete formatMapDialog;
  delete zoomHandler;
  delete ui;
  delete itemModel;
  delete proxyModel;
}

int RouteMultiExportDialog::exec()
{
  // Create a copy of the format map
  *formatMapDialog = *formatMapSystem;

  // Reload default paths which are used by the reset function. Paths can change due to selected simulator.
  formatMapDialog->updateDefaultPaths();

  // Check if selected paths exist
  formatMapDialog->updatePathErrors();

  // Backup options combo box
  exportOptions = getExportOptions();

  // Update table
  updateModel();

  // Reload layout
  loadTableLayout();

  int retval = QDialog::exec();
  formatMapDialog->updatePathErrors();

  saveDialogState();
  saveTableLayout();

  return retval;
}

void RouteMultiExportDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Ok))
  {
    // Copied modified map to system map
    *formatMapSystem = *formatMapDialog;
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::SaveAll))
  {
    // Create a copy to use the current (temporary) state of the dialog for saving
    RouteExportFormatMap sysBackup(*formatMapSystem);
    *formatMapSystem = *formatMapDialog;
    emit saveSelectedButtonClicked();
    *formatMapSystem = sysBackup;
  }
  else if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Cancel))
  {
    setExportOptions(exportOptions);
    QDialog::reject();
  }
  else if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "ROUTEEXPORTALL.html", lnm::helpLanguageOnline());

#ifdef DEBUG_INFORMATION
  for(auto it = formatMapDialog->constBegin(); it != formatMapDialog->constEnd(); ++it)
    qDebug() << Q_FUNC_INFO << it.value().getPath() << it.value().getPattern();
#endif
}

void RouteMultiExportDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.restore({this, ui->comboBoxRouteExportOptions});

  // Check if there is no save widget state - fix sorting if not
  firstStart = !widgetState.contains(ui->tableViewRouteExport);

  formatMapDialog->updatePathErrors();
  updateTableColors();
  updateLabel();
  updateActions();

  loadTableLayout();
}

void RouteMultiExportDialog::saveDialogState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.save(this);
}

void RouteMultiExportDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.save({this, ui->comboBoxRouteExportOptions});
}

void RouteMultiExportDialog::updateLabel()
{
  rexp::RouteExportFormatType type = selectedType();
  QStringList texts;
  if(type != rexp::NO_TYPE)
  {
    RouteExportFormat format = formatMapDialog->value(type);

    // Collect path errors
    QString pathMsg;
    format.isPathValid(&pathMsg);

    // Collect pattern errors and generate example
    QString patternMsg;
    QString example = atools::fs::pln::Flightplan::getFilenamePatternExample(format.getPattern(),
                                                                             QFileInfo(format.getDefaultPattern()).suffix(),
                                                                             true /* html */, &patternMsg);
    QString errorMsg = pathMsg % patternMsg;

    // Get header / short description
    texts.append("<b>" % format.getComment() % "</b>");

    // Add either example or error message
    if(errorMsg.isEmpty())
      texts.append(tr("<b>Example export path and file:</b> &quot;%1&quot;").
                   arg(atools::nativeCleanPath(format.getPath() % atools::SEP % example)));
    else
      texts.append(atools::util::HtmlBuilder::errorMessage(errorMsg));

    // Remaining description from the comment
    texts.append(format.getComment2().split("\n"));

    ui->actionSelect->setChecked(format.isSelected());
  }
  else
    texts.append(tr("Click on a row in the table above to see the resulting filename, path and other information."));

  // Remove empty string and append empty to avoid label resizing while scrolling
  texts.removeAll(QString());
  for(int i = texts.size(); i < 6; i++)
    texts.append(QString());

  ui->labelRouteExportPath->setText(texts.join("<br/>"));
}

void RouteMultiExportDialog::updateActions()
{
  // Update actions ========================
  bool routeEmpty = NavApp::isRouteEmpty();
  int selRow = selectedRow();
  ui->actionSelectExportPath->setEnabled(selRow != -1);
  ui->actionResetExportPath->setEnabled(selRow != -1);
  ui->actionResetFilePattern->setEnabled(selRow != -1);
  ui->actionEditPattern->setEnabled(selRow != -1);
  ui->actionExportFileNow->setEnabled(selRow != -1 && !routeEmpty);
  ui->actionEditPath->setEnabled(selRow != -1);
  ui->actionSelect->setEnabled(selRow != -1);

  // Update table cell buttons ========================
  int numSelected = 0;
  for(int row = 0; row < ui->tableViewRouteExport->model()->rowCount(); row++)
  {
    QWidget *cellWidget = ui->tableViewRouteExport->indexWidget(proxyModel->mapFromSource(itemModel->index(row, BUTTONS)));
    if(cellWidget != nullptr)
    {
      // Buttons are named children of the cell widget
      QPushButton *exportNowButton = cellWidget->findChild<QPushButton *>(OBJ_NAME_EXP_SAVE, Qt::FindDirectChildrenOnly);
      if(exportNowButton != nullptr)
        exportNowButton->setDisabled(routeEmpty);

      QCheckBox *checkBox = cellWidget->findChild<QCheckBox *>(OBJ_NAME_CHECKBOX, Qt::FindDirectChildrenOnly);
      if(checkBox != nullptr)
        numSelected += checkBox->isChecked();
    }
  }

  // Update "Export Selected Formats"
  ui->buttonBoxRouteExport->button(QDialogButtonBox::SaveAll)->setDisabled(routeEmpty || numSelected == 0);

  rexp::RouteExportFormatType type = selectedType();
  if(type != rexp::NO_TYPE)
  {
    ui->actionSelect->blockSignals(true);
    ui->actionSelect->setChecked(formatMapDialog->value(type).isSelected());
    ui->actionSelect->blockSignals(false);
  }
}

void RouteMultiExportDialog::updateTableColors()
{
  const QString patternHelp(tr("The pattern is used to build filenames when exporting flight plans.\n"
                               "The file suffix like \".lnmpln\", \".pln\" or \".fgfp\" is a part of the pattern.\n"
                               "\n"
                               "PLANTYPE: \"IFR\" or \"VFR\"\n"
                               "DEPARTIDENT: Departure airport ident\n"
                               "DEPARTNAME: Departure airport name\n"
                               "DESTIDENT: Destination airport ident\n"
                               "DESTNAME: Destination airport name\n"
                               "CRUISEALT: Cruise altitude"));

  changingTable = true;
  for(int row = 0; row < itemModel->rowCount(); row++)
  {
    const RouteExportFormat& fmt =
      (*formatMapDialog)[static_cast<rexp::RouteExportFormatType>(itemModel->item(row, CATEGORY)->data(FORMAT_TYPE_ROLE).toInt())];

    for(int col = FIRST_COL; col <= LAST_COL; col++)
    {
      QStandardItem *item = itemModel->item(row, col);
      if(item != nullptr)
      {
        if(col == PATTERN)
        {
          // Add tooltips to pattern column - errors also for not selected rows
          QString errorMessage;
          if(!fmt.isPatternValid(&errorMessage))
            // Not valid
            item->setToolTip(tr("Error: %1\nPress F2 or double click to edit filename pattern.\n\n%2").arg(errorMessage).arg(patternHelp));
          else if(fmt.isSelected())
            // Selected and valid
            item->setToolTip(tr("Format selected for export.\n"
                                "Press F2 or double click to edit filename pattern.\n\n%1").arg(patternHelp));
          else
            item->setToolTip(tr("Press F2 or double click to edit filename pattern.\n\n%1").arg(patternHelp));
        }
        else if(col == PATH)
        {
          // Add tooltips to path column
          QString errorMessage;
          if(fmt.isSelected() && !fmt.isPathValid(&errorMessage))
            item->setToolTip(tr("Error: %1.\nPress F3 or double click to edit path.").arg(errorMessage));
          else if(fmt.isSelected())
            item->setToolTip(tr("Format selected for export.\n"
                                "Press F3 or double click to edit path."));
          else
            item->setToolTip(tr("Press F3 or double click to edit path"));
        }
      }
    }
  }
  changingTable = false;

  // Need to update the whole view since the tooltip change affects only the path cell
  ui->tableViewRouteExport->viewport()->update();
}

void RouteMultiExportDialog::updateModel()
{
  changingTable = true;

  // Remove items
  itemModel->clear();
  selectCheckBoxIndex.clear();

  // Reset sorting
  proxyModel->invalidate();

  // Fill table dimensions
  itemModel->setRowCount(formatMapDialog->size());
  itemModel->setColumnCount(LAST_COL + 1);

  // Set header ============================================
  itemModel->setHeaderData(BUTTONS, Qt::Horizontal, tr("Enable / Change Path /\nExport Now / Reset Path"));
  itemModel->setHeaderData(CATEGORY, Qt::Horizontal, tr("Category"));
  itemModel->setHeaderData(DESCRIPTION, Qt::Horizontal, tr("Usage"));
  itemModel->setHeaderData(PATTERN, Qt::Horizontal, tr("Filename Pattern\nand Extension"));
  itemModel->setHeaderData(PATH, Qt::Horizontal, tr("Export Path"));

  QList<RouteExportFormat> values = formatMapDialog->values();

  std::sort(values.begin(), values.end(), [](const RouteExportFormat& f1, const RouteExportFormat& f2) -> bool
  {
    return QString::localeAwareCompare(f1.getComment(), f2.getComment()) < 0;
  });

  // Fill model ============================================
  int row = 0;
  for(const RouteExportFormat& format : qAsConst(values))
  {
    // Userdata needed in callbacks
    int userdata = format.getTypeAsInt();

    // Reset and select buttons =============================================================
    // Top level dummy widget
    QWidget *cellWidget = new QWidget();
    cellWidget->setAutoFillBackground(true);

#ifdef Q_OS_MACOS
    QCheckBox *checkBox = new QCheckBox("   ", cellWidget);
#else
    QCheckBox *checkBox = new QCheckBox(cellWidget);
#endif

    checkBox->setObjectName(OBJ_NAME_CHECKBOX);
    checkBox->setToolTip(tr("Flight plan format will be exported with multiexport when checked"));
    checkBox->setProperty(FORMAT_PROP_NAME, userdata);
    checkBox->setProperty(ROW_PROP_NAME, row);
    checkBox->setAutoFillBackground(true);
    checkBox->setCheckState(format.isSelected() ? Qt::Checked : Qt::Unchecked);
    connect(checkBox, &QCheckBox::toggled, this, &RouteMultiExportDialog::selectForExportToggled);
    selectCheckBoxIndex.insert(format.getType(), checkBox);

    QPushButton *selectButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/fileopen.svg"), QString(), cellWidget);
    selectButton->setObjectName(OBJ_NAME_EXP_SELECT);
    selectButton->setToolTip(tr("Select %1 that will be used to export the flight plan").
                             arg(format.isAppendToFile() ? tr("an existing file") : tr("a directory")));
    selectButton->setProperty(FORMAT_PROP_NAME, userdata);
    selectButton->setProperty(ROW_PROP_NAME, row);
    selectButton->setAutoFillBackground(true);
    connect(selectButton, &QPushButton::clicked, this, &RouteMultiExportDialog::selectPathClicked);

    QPushButton *saveButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/filesaveas.svg"), QString(), cellWidget);
    saveButton->setObjectName(OBJ_NAME_EXP_SAVE);
    saveButton->setToolTip(tr("Export flight plan now"));
    saveButton->setProperty(FORMAT_PROP_NAME, userdata);
    saveButton->setProperty(ROW_PROP_NAME, row);
    saveButton->setAutoFillBackground(true);
    connect(saveButton, &QPushButton::clicked, this, &RouteMultiExportDialog::saveNowClicked);

    QPushButton *resetButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/reset.svg"), QString(), cellWidget);
    resetButton->setObjectName(OBJ_NAME_EXP_RESET);
    resetButton->setToolTip(tr("Reset path back to default.\n"
                               "The default path is determined by the current scenery library or simulator selection.\n"
                               "If not applicable, the best estimate from installed simulators is used."));
    resetButton->setProperty(FORMAT_PROP_NAME, userdata);
    resetButton->setProperty(ROW_PROP_NAME, row);
    resetButton->setAutoFillBackground(true);
    connect(resetButton, &QPushButton::clicked, this, &RouteMultiExportDialog::resetPathClicked);

    // Layout =========
    QLayout *layout = new QHBoxLayout(cellWidget);
    layout->addWidget(checkBox);
    layout->addWidget(selectButton);
    layout->addWidget(saveButton);
    layout->addWidget(resetButton);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->setSpacing(5);
    ui->tableViewRouteExport->setIndexWidget(proxyModel->mapFromSource(itemModel->index(row, BUTTONS)), cellWidget);
    // Update user role which is used for sorting
    itemModel->setData(itemModel->index(row, BUTTONS), format.isSelected(), CHECK_STATE_ROLE);

    // Category =============================================================
    QStandardItem *item = new QStandardItem(format.getCategory());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    itemModel->setItem(row, CATEGORY, item);

    // Description =============================================================
    item = new QStandardItem(format.getComment());
    item->setToolTip(format.getToolTip());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    itemModel->setItem(row, DESCRIPTION, item);

    // File pattern =============================================================
    item = new QStandardItem(format.getPattern());
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    item->setToolTip(format.isAppendToFile() || format.isReplaceFile() ? tr("Filename") : tr("Filename pattern"));
    itemModel->setItem(row, PATTERN, item);

    // Path =============================================================
    item = new QStandardItem(format.getPath());
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    item->setToolTip(tr("Press F2 or double click to edit path"));
    itemModel->setItem(row, PATH, item);

    row++;
  }

  // No moving of first columns
  ui->tableViewRouteExport->horizontalHeader()->setSectionResizeMode(BUTTONS, QHeaderView::ResizeToContents);

  if(firstStart)
  {
    // Avoid activation of sorting for first column if there is not widget state saved
    ui->tableViewRouteExport->horizontalHeader()->setSortIndicator(CATEGORY, Qt::AscendingOrder);
    firstStart = false;
  }
  changingTable = false;

  updateTableColors();
  updateLabel();
  updateActions();
}

RouteMultiExportDialog::ExportOptions RouteMultiExportDialog::getExportOptions() const
{
  return static_cast<ExportOptions>(ui->comboBoxRouteExportOptions->currentIndex());
}

void RouteMultiExportDialog::setExportOptions(RouteMultiExportDialog::ExportOptions options)
{
  ui->comboBoxRouteExportOptions->setCurrentIndex(options);
}

void RouteMultiExportDialog::selectForExport(rexp::RouteExportFormatType type, bool checked)
{
  if(type == rexp::NO_TYPE)
    return;

  // Checkbox signal does the rest and calls method below
  selectCheckBoxIndex[type]->setChecked(checked);
}

void RouteMultiExportDialog::selectForExportToggled()
{
  QCheckBox *checkBox = dynamic_cast<QCheckBox *>(sender());
  if(checkBox != nullptr)
  {
    // Select checkbox clicked ============================
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(checkBox->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << static_cast<int>(type);

    // Checkbox clicked - update format in map and colors
    // Update user role for sorting
    itemModel->setData(itemModel->index(checkBox->property(ROW_PROP_NAME).toInt(), BUTTONS), checkBox->isChecked(), CHECK_STATE_ROLE);
    formatMapDialog->setSelected(type, checkBox->checkState() == Qt::Checked);
    formatMapDialog->updatePathErrors();
    updateTableColors();
    updateLabel();
    updateActions();
  }
}

void RouteMultiExportDialog::selectPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
    // Choose path button clicked ============================
    selectPath(static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt()),
               button->property(ROW_PROP_NAME).toInt());
}

void RouteMultiExportDialog::selectPath(rexp::RouteExportFormatType type, int row)
{
  if(type == rexp::NO_TYPE)
    return;

  const RouteExportFormat format = formatMapDialog->value(type);

  QString filepath, filter;
  QString suffix = format.getSuffix(); // Get suffix plus dot
  if(!suffix.isEmpty())
    filter = tr("%1 Files %2;;All Files (*)").arg(format.getFormat()).arg(format.getFilter());
  else
    filter = tr("All Files (*)");

  if(format.isAppendToFile())
    // Format use a file to append plan
    filepath = atools::gui::Dialog(this).openFileDialog(tr("Select Export File for %1").arg(format.getComment()),
                                                        filter, QString(), format.getPath());
  else
    // Format uses a directory to save a file
    filepath = atools::gui::Dialog(this).openDirectoryDialog(tr("Select Export Directory for %1").
                                                             arg(format.getComment()), QString(), format.getPath());

  if(!filepath.isEmpty())
  {
    if(format.isAppendToFile())
    {
      // Update filename in pattern too
      QFileInfo fi(filepath);
      formatMapDialog->updatePattern(type, fi.fileName());
      formatMapDialog->updatePath(type, fi.absolutePath());

      itemModel->item(row, PATTERN)->setText(fi.fileName());
      itemModel->item(row, PATH)->setText(fi.absolutePath());
    }
    else
    {
      filepath = atools::nativeCleanPath(filepath);
      formatMapDialog->updatePath(type, filepath);

      itemModel->item(row, PATH)->setText(filepath);
    }

    updateTableColors();
    updateLabel();
  }
}

void RouteMultiExportDialog::resetPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
    // Reset path button clicked ============================
    resetPath(static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt()),
              button->property(ROW_PROP_NAME).toInt());
}

void RouteMultiExportDialog::resetPath(rexp::RouteExportFormatType type, int row)
{
  if(type == rexp::NO_TYPE)
    return;

  // Copy default to path
  formatMapDialog->clearPath(type);

  // Update table item
  changingTable = true;
  itemModel->item(row, PATH)->setText(formatMapDialog->value(type).getDefaultPath());
  changingTable = false;

  updateTableColors();
  updateLabel();
}

void RouteMultiExportDialog::resetPattern(rexp::RouteExportFormatType type, int row)
{
  if(type == rexp::NO_TYPE)
    return;

  // Copy default to path
  formatMapDialog->clearPattern(type);

  // Update table item
  changingTable = true;
  itemModel->item(row, PATTERN)->setText(formatMapDialog->value(type).getDefaultPattern());
  changingTable = false;

  updateTableColors();
  updateLabel();
}

#ifdef DEBUG_INFORMATION_MULTIEXPORT

void RouteMultiExportDialog::resetPathsAndSelectionDebug()
{
  resetPathsAndSelection();

  for(auto it = formatMapDialog->constBegin(); it != formatMapDialog->constEnd(); ++it)
    formatMapDialog->setDebugOptions(it.key());
  updateModel();

  // Reload layout
  loadTableLayout();
}

#endif

void RouteMultiExportDialog::resetPathsAndSelection()
{
  // Ask before resetting user data ==============
  QMessageBox msgBox(this);
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText(tr("Reset selection, paths and filename patterns back to default?"));
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if(msgBox.exec() == QMessageBox::Yes)
  {
    // Save layout before changing table
    saveTableLayout();

    // Reset values in map ========================================
    for(auto it = formatMapDialog->constBegin(); it != formatMapDialog->constEnd(); ++it)
    {
      formatMapDialog->clearPath(it.key());
      formatMapDialog->clearPattern(it.key());
      formatMapDialog->setSelected(it.key(), false);
    }

    // Fill model/view again from table
    updateModel();

    // Reload layout
    loadTableLayout();
  }
}

void RouteMultiExportDialog::saveNowClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    // Save button clicked ============================
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << static_cast<int>(type);
    emit saveNowButtonClicked(formatMapDialog->value(type));
  }
}

rexp::RouteExportFormatType RouteMultiExportDialog::selectedType()
{
  QStandardItem *item = selectedItem(PATH);

  if(item != nullptr)
    return static_cast<rexp::RouteExportFormatType>(item->data(FORMAT_TYPE_ROLE).toInt());
  else
    return rexp::NO_TYPE;
}

QStandardItem *RouteMultiExportDialog::selectedItem(int col)
{
  int row = selectedRow();
  return row != -1 ? itemModel->item(selectedRow(), col) : nullptr;
}

int RouteMultiExportDialog::selectedRow()
{
  QModelIndex idx = selectedIndex();
  return idx.isValid() ? idx.row() : -1;
}

QModelIndex RouteMultiExportDialog::selectedIndex()
{
  QItemSelectionModel *selectionModel = ui->tableViewRouteExport->selectionModel();
  if(selectionModel != nullptr && selectionModel->hasSelection())
  {
    QModelIndex index = selectionModel->selectedIndexes().constFirst();
    return proxyModel->mapToSource(index);
  }
  return QModelIndex();
}

void RouteMultiExportDialog::selectionChanged(const QItemSelection&, const QItemSelection&)
{
  formatMapDialog->updatePathErrors();
  updateTableColors();
  updateLabel();
  updateActions();
}

void RouteMultiExportDialog::itemChanged(QStandardItem *item)
{
  if(!changingTable)
  {
    // Double click on path for editing ====================================
    qDebug() << Q_FUNC_INFO << item->column() << item->row() << item->text();

    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(item->data(FORMAT_TYPE_ROLE).toInt());
    if(item->column() == PATTERN)
    {
      // Update file pattern
      formatMapDialog->updatePattern(type, item->text());
      selectionChanged(ui->tableViewRouteExport->selectionModel()->selection(), QItemSelection());
    }
    else if(item->column() == PATH)
    {
      // Update custom path
      formatMapDialog->updatePath(type, item->text());
      item->setText(atools::nativeCleanPath(item->text()));
      updateTableColors();
      updateLabel();
    }
  }
}

void RouteMultiExportDialog::tableContextMenu(const QPoint&)
{
  qDebug() << Q_FUNC_INFO;

  formatMapDialog->updatePathErrors();

  QPoint menuPos = QCursor::pos();

  // Position relative to viewport
  QPoint tablePos = ui->tableViewRouteExport->viewport()->mapFromGlobal(QCursor::pos());

  // Use widget center if position is not inside widget
  if(!ui->tableViewRouteExport->rect().contains(tablePos))
    menuPos = ui->tableViewRouteExport->mapToGlobal(ui->tableViewRouteExport->rect().center());

  QModelIndex posIndex = ui->tableViewRouteExport->indexAt(tablePos);

  if(posIndex.isValid())
    // Select row manually since the automatic selection is not reliable
    ui->tableViewRouteExport->selectRow(posIndex.row());
  else
    ui->tableViewRouteExport->clearSelection();

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  // Get row below cursor ==================================
  rexp::RouteExportFormatType type = selectedType();

  // Disable actions if no row found =============
  if(type != rexp::NO_TYPE)
    ui->actionSelect->setText(tr("&Enable Export for %1").arg(atools::elideTextShort(formatMapDialog->value(type).getComment(), 40)));
  else
    ui->actionSelect->setText(tr("&Enable for Export"));

  updateActions();

  // Build menu ===================================================
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  menu.addAction(ui->actionSelect);
  menu.addSeparator();
  menu.addAction(ui->actionSelectExportPath);
  menu.addAction(ui->actionExportFileNow);
  menu.addSeparator();
  menu.addAction(ui->actionEditPath);
  menu.addAction(ui->actionResetExportPath);
  menu.addSeparator();
  menu.addAction(ui->actionEditPattern);
  menu.addAction(ui->actionResetFilePattern);
  menu.addSeparator();
  menu.addAction(ui->actionResetPathsAndSelection);
  menu.addAction(ui->actionResetView);
  menu.addSeparator();
  menu.addAction(ui->actionIncreaseTextSize);
  menu.addAction(ui->actionDefaultTextSize);
  menu.addAction(ui->actionDecreaseTextSize);

#ifdef DEBUG_INFORMATION_MULTIEXPORT
  menu.addSeparator();

  QAction *initDebugAction = new QAction("Debug Init", &menu);
  menu.addAction(initDebugAction);

  QAction *allDebugAction = new QAction("Debug Select all", &menu);
  menu.addAction(allDebugAction);

  QAction *noneDebugAction = new QAction("Debug Select none", &menu);
  menu.addAction(noneDebugAction);
#endif

  QAction *action = menu.exec(menuPos);

  // Handle actions which are not connected to methods
  if(action == ui->actionResetPathsAndSelection)
    resetPathsAndSelection();
#ifdef DEBUG_INFORMATION_MULTIEXPORT
  else if(action == initDebugAction)
    resetPathsAndSelectionDebug();
  else if(action == allDebugAction)
  {
    for(auto it = selectCheckBoxIndex.begin(); it != selectCheckBoxIndex.end(); ++it)
      it.value()->setChecked(true);
  }
  else if(action == noneDebugAction)
  {
    for(auto it = selectCheckBoxIndex.begin(); it != selectCheckBoxIndex.end(); ++it)
      it.value()->setChecked(false);
  }
#endif
  else if(action == ui->actionResetView)
  {
    // Reorder columns to match model order
    QHeaderView *header = ui->tableViewRouteExport->horizontalHeader();
    for(int i = 0; i < header->count(); i++)
      header->moveSection(header->visualIndex(i), i);

    // Reset font and cell size
    zoomHandler->zoomDefault();

    // Reset column widths
    ui->tableViewRouteExport->resizeColumnsToContents();

    // Workaround for Qt but which results in huge colum width for the last one
    ui->tableViewRouteExport->setColumnWidth(PATH, 100);

    // Reset sorting back to category
    proxyModel->invalidate();
    ui->tableViewRouteExport->horizontalHeader()->setSortIndicator(CATEGORY, Qt::AscendingOrder);
  }
}

void RouteMultiExportDialog::actionSelectExportPathTriggered()
{
  selectPath(selectedType(), selectedRow());
}

void RouteMultiExportDialog::actionResetExportPathTriggered()
{
  resetPath(selectedType(), selectedRow());
}

void RouteMultiExportDialog::actionResetExportPatternTriggered()
{
  resetPattern(selectedType(), selectedRow());
}

void RouteMultiExportDialog::actionExportFileNowTriggered()
{
  rexp::RouteExportFormatType type = selectedType();
  if(type != rexp::NO_TYPE)
    emit saveNowButtonClicked(formatMapDialog->value(type));
}

void RouteMultiExportDialog::actionEditPathTriggered()
{
  QModelIndex idx = proxyModel->mapFromSource(itemModel->index(selectedRow(), PATH));
  if(idx.isValid())
  {
    ui->tableViewRouteExport->setCurrentIndex(idx);
    ui->tableViewRouteExport->edit(idx);
  }
}

void RouteMultiExportDialog::actionEditPatternTriggered()
{
  QModelIndex idx = proxyModel->mapFromSource(itemModel->index(selectedRow(), PATTERN));
  if(idx.isValid())
  {
    ui->tableViewRouteExport->setCurrentIndex(idx);
    ui->tableViewRouteExport->edit(idx);
  }
}

void RouteMultiExportDialog::actionSelectTriggered()
{
  selectForExport(selectedType(), ui->actionSelect->isChecked());
}

void RouteMultiExportDialog::saveTableLayout()
{
  atools::gui::WidgetState(lnm::ROUTE_EXPORT_DIALOG, false).save(ui->tableViewRouteExport);
}

void RouteMultiExportDialog::loadTableLayout()
{
  atools::gui::WidgetState(lnm::ROUTE_EXPORT_DIALOG, false).restore(ui->tableViewRouteExport);
}

void RouteMultiExportDialog::setLnmplnExportDir(const QString& dir)
{
  formatMapSystem->setLnmplnExportDir(dir);
}
