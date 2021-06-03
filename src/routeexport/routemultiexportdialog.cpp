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

#include "routeexport/routemultiexportdialog.h"
#include "ui_routemultiexportdialog.h"

#include "routeexport/routeexportformat.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "settings/settings.h"
#include "atools.h"
#include "navapp.h"
#include "gui/dialog.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QDir>
#include <QStyledItemDelegate>
#include <QPainter>

using atools::settings::Settings;

// Properties set to buttons for RouteExportFormatType and table row
const static char FORMAT_PROP_NAME[] = "format";
const static char ROW_PROP_NAME[] = "row";

// Data role for checkbox status set in model for first BUTTONS column
// Needed for sorting
static int CHECK_STATE_ROLE = Qt::UserRole;

// Data role for RouteExportFormatType set in model for all columns
static int FORMAT_TYPE_ROLE = Qt::UserRole + 1;

enum Columns
{
  FIRST_COL,
  BUTTONS = FIRST_COL,
  CATEGORY,
  DESCRIPTION,
  EXTENSION,
  PATH,
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
  const RouteExportFormat& format =
    (*formats)[static_cast<rexp::RouteExportFormatType>(index.data(FORMAT_TYPE_ROLE).toInt())];
  QStyleOptionViewItem opt(option);

  if(format.isSelected())
  {
    // Set whole row to bold if selected for multiexport =========================
    opt.font.setBold(true);

    if(index.column() == PATH && !format.isPathValid())
    {
      // Path is invalid - draw dark red for selection and light red warning for normal state in background
      opt.palette.setColor(QPalette::Highlight, QColor(Qt::red).darker(130));
      opt.palette.setColor(QPalette::HighlightedText, Qt::white);
      opt.palette.setColor(QPalette::Text, Qt::white);

      // Delegate does not paint background
      painter->fillRect(option.rect, Qt::red);
    }
  }

  QStyledItemDelegate::paint(painter, opt, index);
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
  : QDialog(parent), formatMapOrig(exportFormatMap), ui(new Ui::RouteMultiExportDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);
  formatMap = new RouteExportFormatMap;

  // Create own item model
  itemModel = new QStandardItemModel(this);

  // Set delegate for background color
  ui->tableViewRouteExport->setItemDelegate(new TableItemDelegate(this, formatMap));

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

  // Detect inline edit by double click
  connect(itemModel, &QStandardItemModel::itemChanged, this, &RouteMultiExportDialog::itemChanged);

  // Set up context menu for table ==================================================
  ui->tableViewRouteExport->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableViewRouteExport, &QWidget::customContextMenuRequested,
          this, &RouteMultiExportDialog::tableContextMenu);

  QItemSelectionModel *selectionModel = ui->tableViewRouteExport->selectionModel();
  if(selectionModel != nullptr)
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
            [ = ](const QModelIndex&, const QModelIndex&) -> void {
      formatMap->updatePathErrors();
      updateTableColors();
    });

  // Action shortcuts only for table
  ui->actionSelectExportPath->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionResetExportPath->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionExportFileNow->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionEditPath->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionResetView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionResetPathsAndSelection->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionIncreaseTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionDefaultTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionDecreaseTextSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->tableViewRouteExport->addActions({ui->actionSelectExportPath, ui->actionResetExportPath,
                                        ui->actionExportFileNow, ui->actionEditPath, ui->actionResetView,
                                        ui->actionResetPathsAndSelection, ui->actionIncreaseTextSize,
                                        ui->actionDefaultTextSize, ui->actionDecreaseTextSize});
}

RouteMultiExportDialog::~RouteMultiExportDialog()
{
  delete formatMap;
  delete zoomHandler;
  delete ui;
  delete itemModel;
  delete proxyModel;
}

int RouteMultiExportDialog::exec()
{
  // Create a copy of the format map
  *formatMap = *formatMapOrig;

  // Reload default paths which are used by the reset function. Paths can change due to selected simulator.
  formatMap->updateDefaultPaths();

  // Check if selected paths exist
  formatMap->updatePathErrors();

  // Backup options combo box
  exportOptions = getExportOptions();

  // Update table
  updateModel();

  int retval = QDialog::exec();
  formatMap->updatePathErrors();
  saveDialogState();
  return retval;
}

void RouteMultiExportDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Ok))
  {
    // Copied modified map to system map
    *formatMapOrig = *formatMap;
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Cancel))
  {
    saveDialogState();
    setExportOptions(exportOptions);
    QDialog::reject();
  }
  else if(button == ui->buttonBoxRouteExport->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "ROUTEEXPORTALL.html",
                                             lnm::helpLanguageOnline());
}

void RouteMultiExportDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.restore({this, ui->tableViewRouteExport, ui->comboBoxRouteExportOptions});

  // Check if there is no save widget state - fix sorting if not
  firstStart = !widgetState.contains(ui->tableViewRouteExport);
}

void RouteMultiExportDialog::saveDialogState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.save({this, ui->tableViewRouteExport});
}

void RouteMultiExportDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_EXPORT_DIALOG, false);
  widgetState.save({this, ui->tableViewRouteExport, ui->comboBoxRouteExportOptions});
}

void RouteMultiExportDialog::updateTableColors()
{
  changingTable = true;
  for(int row = 0; row < itemModel->rowCount(); row++)
  {
    const RouteExportFormat& fmt =
      (*formatMap)[static_cast<rexp::RouteExportFormatType>(
                     itemModel->item(row, CATEGORY)->data(FORMAT_TYPE_ROLE).toInt())];

    for(int col = FIRST_COL; col <= LAST_COL; col++)
    {
      QStandardItem *item = itemModel->item(row, col);
      if(item != nullptr && col == PATH)
      {
        // Add tooltips to path column
        QString errorMessage;
        if(fmt.isSelected() && !fmt.isPathValid(&errorMessage))
          item->setToolTip(tr("Error: %1.\nPress F2 or double click to edit path.").arg(errorMessage));
        else if(fmt.isSelected())
          item->setToolTip(tr("Format selected for export.\n"
                              "Press F2 or double click to edit path."));
        else
          item->setToolTip(tr("Press F2 or double click to edit path"));
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
  itemModel->setRowCount(formatMap->size());
  itemModel->setColumnCount(LAST_COL + 1);

  // Set header ============================================
  itemModel->setHeaderData(BUTTONS, Qt::Horizontal, tr("Enable / Change path /\nExport now / Reset path"));
  itemModel->setHeaderData(CATEGORY, Qt::Horizontal, tr("Category"));
  itemModel->setHeaderData(DESCRIPTION, Qt::Horizontal, tr("Usage"));
  itemModel->setHeaderData(EXTENSION, Qt::Horizontal, tr("Extension\nor Filename"));
  itemModel->setHeaderData(PATH, Qt::Horizontal, tr("Export Path"));

  QList<RouteExportFormat> values = formatMap->values();

  std::sort(values.begin(), values.end(), [](const RouteExportFormat& f1, const RouteExportFormat& f2) -> bool
  {
    return QString::localeAwareCompare(f1.getComment(), f2.getComment()) < 0;
  });

  // Fill model ============================================
  int row = 0;
  for(const RouteExportFormat& format : values)
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

    checkBox->setToolTip(tr("Flight plan format will be exported with multiexport when checked"));
    checkBox->setProperty(FORMAT_PROP_NAME, userdata);
    checkBox->setProperty(ROW_PROP_NAME, row);
    checkBox->setAutoFillBackground(true);
    checkBox->setCheckState(format.isSelected() ? Qt::Checked : Qt::Unchecked);
    connect(checkBox, &QCheckBox::toggled, this, &RouteMultiExportDialog::selectForExportToggled);
    selectCheckBoxIndex.insert(format.getType(), checkBox);

    QPushButton *selectButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/fileopen.svg"),
                                                QString(), cellWidget);
    selectButton->setToolTip(tr("Select %1 that will be used to export the flight plan").
                             arg(format.isAppendToFile() ? tr("an existing file") : tr("a directory")));
    selectButton->setProperty(FORMAT_PROP_NAME, userdata);
    selectButton->setProperty(ROW_PROP_NAME, row);
    selectButton->setAutoFillBackground(true);
    connect(selectButton, &QPushButton::clicked, this, &RouteMultiExportDialog::selectPathClicked);

    QPushButton *saveButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/filesaveas.svg"),
                                              QString(), cellWidget);
    saveButton->setToolTip(tr("Export flight plan now"));
    saveButton->setProperty(FORMAT_PROP_NAME, userdata);
    saveButton->setProperty(ROW_PROP_NAME, row);
    saveButton->setAutoFillBackground(true);
    connect(saveButton, &QPushButton::clicked, this, &RouteMultiExportDialog::saveNowClicked);

    QPushButton *resetButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/reset.svg"),
                                               QString(), cellWidget);
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

    // File extension =============================================================
    item = new QStandardItem(format.getFormat());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    item->setToolTip(format.isAppendToFile() || format.isReplaceFile() ? tr("Filename") : tr("File extension"));
    itemModel->setItem(row, EXTENSION, item);

    // Path =============================================================
    item = new QStandardItem(format.getPath());
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata, FORMAT_TYPE_ROLE);
    item->setToolTip(tr("Press F2 or double click to edit path"));
    itemModel->setItem(row, PATH, item);

    row++;
  }

  // Reload layout
  atools::gui::WidgetState(lnm::ROUTE_EXPORT_DIALOG, false).restore(ui->tableViewRouteExport);

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
  // Checkbox signal does the rest and calls method below
  selectCheckBoxIndex[type]->setChecked(checked);
}

void RouteMultiExportDialog::selectForExportToggled()
{
  QCheckBox *checkBox = dynamic_cast<QCheckBox *>(sender());
  if(checkBox != nullptr)
  {
    // Select checkbox clicked ============================
    rexp::RouteExportFormatType type =
      static_cast<rexp::RouteExportFormatType>(checkBox->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << static_cast<int>(type);

    // Checkbox clicked - update format in map and colors
    // Update user role for sorting
    itemModel->setData(itemModel->index(checkBox->property(ROW_PROP_NAME).toInt(), BUTTONS), checkBox->isChecked(),
                       CHECK_STATE_ROLE);
    formatMap->setSelected(type, checkBox->checkState() == Qt::Checked);
    updateTableColors();
    formatMap->updatePathErrors();
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
  const RouteExportFormat fmt = formatMap->value(type);
  QString filepath;

  if(fmt.isAppendToFile())
  {
    // Format use a file to append plan
    filepath = atools::gui::Dialog(this).openFileDialog(tr("Select Export File for %1").arg(fmt.getComment()),
                                                        tr("Flight Plan Files %1;;All Files (*)").
                                                        arg(fmt.getFilter()), QString(), fmt.getPath());
  }
  else
  {
    // Format uses a directory to save a file
    filepath = atools::gui::Dialog(this).
               openDirectoryDialog(tr("Select Export Directory for %1").
                                   arg(fmt.getComment()), QString(), fmt.getPath());
  }

  if(!filepath.isEmpty())
  {
    QStandardItem *item = itemModel->item(row, PATH);

    filepath = QDir::toNativeSeparators(filepath);

    // Update custom path
    formatMap->updatePath(type, filepath);

    // Update table item
    item->setText(filepath);
    updateTableColors();
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
  // Copy default to path
  formatMap->clearPath(type);

  // Update table item
  changingTable = true;
  itemModel->item(row, PATH)->setText(formatMap->value(type).getDefaultPath());
  changingTable = false;

  updateTableColors();
}

void RouteMultiExportDialog::resetPathsAndSelection()
{
  // Ask before resetting user data ==============
  QMessageBox msgBox(this);
  msgBox.setWindowTitle(QApplication::applicationName());
  msgBox.setText(tr("Reset selection and paths back to default?"));
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);

  if(msgBox.exec() == QMessageBox::Yes)
  {
    // Reset values in map ========================================
    for(rexp::RouteExportFormatType type: formatMap->keys())
    {
      formatMap->clearPath(type);
      formatMap->setSelected(type, false);
    }

    // Fill model/view again from table
    updateModel();
  }
}

void RouteMultiExportDialog::saveNowClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    // Save button clicked ============================
    rexp::RouteExportFormatType type =
      static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << static_cast<int>(type);
    emit saveNowButtonClicked(formatMap->value(type));
  }
}

void RouteMultiExportDialog::itemChanged(QStandardItem *item)
{
  if(!changingTable)
  {
    if(item->column() == PATH)
    {
      // Double click on path for editing ====================================
      qDebug() << Q_FUNC_INFO << item->column() << item->row() << item->text();

      // Update custom path
      formatMap->updatePath(static_cast<rexp::RouteExportFormatType>(item->data(FORMAT_TYPE_ROLE).toInt()),
                            item->text());
      item->setText(QDir::toNativeSeparators(item->text()));

      updateTableColors();
    }
  }
}

void RouteMultiExportDialog::tableContextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  formatMap->updatePathErrors();

  QPoint menuPos = QCursor::pos();

  // Use widget center if position is not inside widget
  if(!ui->tableViewRouteExport->rect().contains(ui->tableViewRouteExport->mapFromGlobal(QCursor::pos())))
    menuPos = ui->tableViewRouteExport->mapToGlobal(ui->tableViewRouteExport->rect().center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  // Get row below cursor ==================================
  QModelIndex index = ui->tableViewRouteExport->indexAt(pos);
  if(index.isValid())
    index = proxyModel->mapToSource(index);
  int row = -1;
  if(index.isValid())
    row = index.row();

  rexp::RouteExportFormatType type = rexp::NO_TYPE;

  if(row != -1)
  {
    QStandardItem *item = itemModel->item(row, PATH);
    if(item != nullptr)
      type = static_cast<rexp::RouteExportFormatType>(item->data(FORMAT_TYPE_ROLE).toInt());
  }

  // Disable actions if no row found =============
  ui->actionSelect->blockSignals(true);
  ui->actionSelect->setEnabled(row != -1);
  ui->actionSelect->setChecked(formatMap->value(type).isSelected());
  ui->actionSelect->blockSignals(false);
  if(type != rexp::NO_TYPE)
    ui->actionSelect->setText(tr("&Enable Export for %1").
                              arg(atools::elideTextShort(formatMap->value(type).getComment(), 40)));
  else
    ui->actionSelect->setText(tr("&Enable for Export"));

  ui->actionSelectExportPath->setEnabled(row != -1);
  ui->actionResetExportPath->setEnabled(row != -1);
  ui->actionExportFileNow->setEnabled(row != -1);
  ui->actionEditPath->setEnabled(row != -1);

  // Build menu ===================================================
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  menu.addAction(ui->actionSelect);
  menu.addSeparator();
  menu.addAction(ui->actionSelectExportPath);
  menu.addAction(ui->actionExportFileNow);
  menu.addAction(ui->actionResetExportPath);
  menu.addSeparator();
  menu.addAction(ui->actionEditPath);
  menu.addSeparator();
  menu.addAction(ui->actionResetPathsAndSelection);
  menu.addAction(ui->actionResetView);
  menu.addSeparator();
  menu.addAction(ui->actionIncreaseTextSize);
  menu.addAction(ui->actionDefaultTextSize);
  menu.addAction(ui->actionDecreaseTextSize);

  QAction *action = menu.exec(menuPos);

  if(action == ui->actionSelect)
    selectForExport(type, ui->actionSelect->isChecked());
  if(action == ui->actionSelectExportPath)
    selectPath(type, row);
  else if(action == ui->actionResetExportPath)
    resetPath(type, row);
  else if(action == ui->actionExportFileNow)
    emit saveNowButtonClicked(formatMap->value(type));
  else if(action == ui->actionEditPath)
  {
    // Trigger path editing mode
    QModelIndex idx = proxyModel->mapFromSource(itemModel->index(row, PATH));
    ui->tableViewRouteExport->setCurrentIndex(idx);
    ui->tableViewRouteExport->edit(idx);
  }
  else if(action == ui->actionResetPathsAndSelection)
    resetPathsAndSelection();
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
