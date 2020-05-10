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

#include "routemultiexportdialog.h"
#include "ui_routemultiexportdialog.h"

#include "gui/widgetstate.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "settings/settings.h"
#include "atools.h"
#include "gui/dialog.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QFile>
#include <QCheckBox>

using atools::settings::Settings;

const static char FORMAT_PROP_NAME[] = "format";
const static char ROW_PROP_NAME[] = "row";

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

private:
  virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const override;

};

bool TableSortProxyModel::lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
{
  if(leftIndex.column() == BUTTONS && rightIndex.column() == BUTTONS)
    // Button column uses user role data for checkbox status
    return sourceModel()->data(leftIndex, Qt::UserRole).toBool() <
           sourceModel()->data(rightIndex, Qt::UserRole).toBool();
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

  // Create model and sort proxy
  itemModel = new QStandardItemModel(this);
  proxy = new TableSortProxyModel(ui->tableViewRouteExport);
  proxy->setSourceModel(itemModel);
  ui->tableViewRouteExport->setModel(proxy);

  // Allow moving of section except the first BUTTON section
  ui->tableViewRouteExport->horizontalHeader()->setSectionsMovable(true);
  ui->tableViewRouteExport->horizontalHeader()->setFirstSectionMovable(false);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableViewRouteExport);

  connect(ui->buttonBoxRouteExport, &QDialogButtonBox::clicked, this, &RouteMultiExportDialog::buttonBoxClicked);

  // Detect inline edit by double click
  connect(itemModel, &QStandardItemModel::itemChanged, this, &RouteMultiExportDialog::itemChanged);
}

RouteMultiExportDialog::~RouteMultiExportDialog()
{
  saveDialogState();
  delete formatMap;
  delete zoomHandler;
  delete ui;
  delete itemModel;
  delete proxy;
}

int RouteMultiExportDialog::exec()
{
  // Create a copy of the format map
  *formatMap = *formatMapOrig;

  // Backup options combo box
  exportOptions = getExportOptions();

  // Update table
  updateModel();

  return QDialog::exec();
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
    RouteExportFormat fmt =
      formatMap->value(static_cast<rexp::RouteExportFormatType>(itemModel->item(row, CATEGORY)->data().toInt()));

    for(int col = FIRST_COL; col <= LAST_COL; col++)
    {
      QStandardItem *item = itemModel->item(row, col);
      if(item != nullptr)
      {
        item->setToolTip(QString());
        // Color style path =========================
        if(col == PATH)
        {
          item->setToolTip(tr("Double click to edit"));
          if(fmt.isSelected() && !fmt.isPathValid())
          {
            // Red for invalid paths/files =====================
            item->setForeground(QColor(Qt::red));
            item->setToolTip(tr("Error: File or directory does not exist.\nDouble click to edit."));
          }
          else
          {
            if(fmt.getPath().isEmpty())
            {
              // Dark gray for default paths =====================
              item->setForeground(QColor(Qt::darkGray));
              if(fmt.isSelected())
                item->setToolTip(tr("Default file or directory selected for export.\nDouble click to edit."));
            }
            else
            {
              // Black for user changed paths =====================
              item->setForeground(QApplication::palette().color(QPalette::Text));
              if(fmt.isSelected())
                item->setToolTip(tr("File or directory selected for export and changed by user.\n"
                                    "Will be used for all simulators.\n"
                                    "Double click to edit."));
            }
          }
        }

        // Set whole row to bold if selected for multiexport =========================
        QFont font = item->font();
        font.setBold(fmt.isSelected());
        item->setFont(font);
      }
    }
  }
  changingTable = false;
}

void RouteMultiExportDialog::updateModel()
{
  changingTable = true;
  QByteArray state = ui->tableViewRouteExport->horizontalHeader()->saveState();

  // Remove items
  itemModel->clear();

  // Reset sorting
  proxy->invalidate();

  // Fill table dimensions
  itemModel->setRowCount(formatMap->size());
  itemModel->setColumnCount(LAST_COL + 1);

  // Set header ============================================
  itemModel->setHeaderData(BUTTONS, Qt::Horizontal, tr("Select/Path/\nReset/Save"));
  itemModel->setHeaderData(CATEGORY, Qt::Horizontal, tr("Category"));
  itemModel->setHeaderData(DESCRIPTION, Qt::Horizontal, tr("Usage"));
  itemModel->setHeaderData(EXTENSION, Qt::Horizontal, tr("Extension\nor Filename"));
  itemModel->setHeaderData(PATH, Qt::Horizontal,
                           tr("Default Export Path - can depend on currently selected Simulator"));

  // Fill model ============================================
  int row = 0;
  for(const RouteExportFormat& format : *formatMap)
  {
    // Userdata needed in callbacks
    int userdata = format.getTypeAsInt();

    // Reset and select buttons =============================================================
    // Top level dummy widget
    QWidget *cellWidget = new QWidget();
    cellWidget->setAutoFillBackground(true);

    QCheckBox *checkBox = new QCheckBox(cellWidget);
    checkBox->setToolTip(tr("Format will be exported with multiexport if checked."));
    checkBox->setProperty(FORMAT_PROP_NAME, userdata);
    checkBox->setProperty(ROW_PROP_NAME, row);
    checkBox->setAutoFillBackground(true);
    checkBox->setCheckState(format.isSelected() ? Qt::Checked : Qt::Unchecked);
    connect(checkBox, &QCheckBox::toggled, this, &RouteMultiExportDialog::selectToggled);

    QPushButton *selectButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/fileopen.svg"),
                                                QString(), cellWidget);
    selectButton->setToolTip(tr("Select a path or file that will be used for saving the files.\n"
                                "This path applies to all simulators once chosen."));
    selectButton->setProperty(FORMAT_PROP_NAME, userdata);
    selectButton->setProperty(ROW_PROP_NAME, row);
    selectButton->setAutoFillBackground(true);
    connect(selectButton, &QPushButton::clicked, this, &RouteMultiExportDialog::selectPathClicked);

    QPushButton *resetButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/reset.svg"),
                                               QString(), cellWidget);
    resetButton->setToolTip(tr("Reset path back to default."));
    resetButton->setProperty(FORMAT_PROP_NAME, userdata);
    resetButton->setProperty(ROW_PROP_NAME, row);
    resetButton->setAutoFillBackground(true);
    connect(resetButton, &QPushButton::clicked, this, &RouteMultiExportDialog::resetPathClicked);

    QPushButton *saveButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/filesaveas.svg"),
                                              QString(), cellWidget);
    saveButton->setToolTip(tr("Save now."));
    saveButton->setProperty(FORMAT_PROP_NAME, userdata);
    saveButton->setProperty(ROW_PROP_NAME, row);
    saveButton->setAutoFillBackground(true);
    connect(saveButton, &QPushButton::clicked, this, &RouteMultiExportDialog::saveNowClicked);

    // Layout =========
    QLayout *layout = new QHBoxLayout(cellWidget);
    layout->addWidget(checkBox);
    layout->addWidget(selectButton);
    layout->addWidget(resetButton);
    layout->addWidget(saveButton);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->setSpacing(5);
    ui->tableViewRouteExport->setIndexWidget(proxy->mapFromSource(itemModel->index(row, BUTTONS)), cellWidget);
    // Update user role which is used for sorting
    itemModel->setData(itemModel->index(row, BUTTONS), format.isSelected(), Qt::UserRole);

    // Category =============================================================
    QStandardItem *item = new QStandardItem(format.getCategory());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata);
    itemModel->setItem(row, CATEGORY, item);

    // Description =============================================================
    item = new QStandardItem(format.getComment());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata);
    itemModel->setItem(row, DESCRIPTION, item);

    // File extension =============================================================
    item = new QStandardItem(format.getFormat());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata);
    item->setToolTip(tr("File extension or filename"));
    itemModel->setItem(row, EXTENSION, item);

    // Path =============================================================
    item = new QStandardItem(format.getPathOrDefault());
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata);
    item->setToolTip(tr("Double click to edit"));
    itemModel->setItem(row, PATH, item);

    row++;
  }
  ui->tableViewRouteExport->horizontalHeader()->restoreState(state);

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

void RouteMultiExportDialog::selectToggled()
{
  QCheckBox *checkBox = dynamic_cast<QCheckBox *>(sender());
  if(checkBox != nullptr)
  {
    // Select checkbox clicked ============================
    rexp::RouteExportFormatType type =
      static_cast<rexp::RouteExportFormatType>(checkBox->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << type;

    // Checkbox clicked - update format in map and colors
    // Update user role for sorting
    itemModel->setData(itemModel->index(checkBox->property(ROW_PROP_NAME).toInt(), BUTTONS), checkBox->isChecked(),
                       Qt::UserRole);
    formatMap->setSelected(type, checkBox->checkState() == Qt::Checked);
    updateTableColors();
  }
}

void RouteMultiExportDialog::selectPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    // Choose path button clicked ============================
    rexp::RouteExportFormatType type =
      static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << type;

    const RouteExportFormat fmt = formatMap->value(type);
    QString filepath;

    if(fmt.isFile())
    {
      // Format use a file to append plan
      filepath = atools::gui::Dialog(this).openFileDialog(tr("Select Export File for %1").arg(fmt.getComment()),
                                                          tr("Flight Plan Files %1;;All Files (*)").
                                                          arg(fmt.getFilter()), QString(), fmt.getPathOrDefault());
    }
    else
    {
      // Format uses a directory to save a file
      filepath = atools::gui::Dialog(this).
                 openDirectoryDialog(tr("Select Export Directory for %1").
                                     arg(fmt.getComment()), QString(), fmt.getPathOrDefault());
    }

    if(!filepath.isEmpty())
    {
      QStandardItem *item = itemModel->item(button->property(ROW_PROP_NAME).toInt(), PATH);

      // Update custom path
      formatMap->updatePath(type, item->text());

      // Update table item
      item->setText(filepath);
      updateTableColors();
    }
  }
}

void RouteMultiExportDialog::resetPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    // Reset path button clicked ============================
    rexp::RouteExportFormatType type =
      static_cast<rexp::RouteExportFormatType>(button->property(FORMAT_PROP_NAME).toInt());
    qDebug() << Q_FUNC_INFO << type;

    // Set custom path to empty
    formatMap->clearPath(type);

    // Update table item
    itemModel->item(button->property(ROW_PROP_NAME).toInt(), PATH)->setText(formatMap->value(type).getDefaultPath());
    updateTableColors();
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
    qDebug() << Q_FUNC_INFO << type;
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
      rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(item->data().toInt());

      // Update custom path
      formatMap->updatePath(type, item->text());

      updateTableColors();
    }
  }
}
