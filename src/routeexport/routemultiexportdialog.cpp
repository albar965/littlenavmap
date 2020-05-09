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

using atools::settings::Settings;

enum Columns
{
  FIRST_COL,
  CHECKBOX = FIRST_COL,
  CATEGORY,
  DESCRIPTION,
  EXTENSION,
  PATH_BUTTONS,
  PATH,
  LAST_COL = PATH
};

// TableSortProxyModel  ==================================================================================================

/* Proxy is needed to allow sorting by checkbox state */
class TableSortProxyModel
  : public QSortFilterProxyModel
{
public:
  explicit TableSortProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
  {

  }

private:
  virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const override;

};

bool TableSortProxyModel::lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
{
  QModelIndex left(leftIndex), right(rightIndex);

  if(leftIndex.column() == CHECKBOX && rightIndex.column() == CHECKBOX)
  {
    // Sort by check state role
    return sourceModel()->data(left, Qt::CheckStateRole).toInt() >
           sourceModel()->data(right, Qt::CheckStateRole).toInt();
  }
  else
  {
    if(leftIndex.column() == PATH_BUTTONS && rightIndex.column() == PATH_BUTTONS)
    {
      // Sort by path when clicking button header
      left = sourceModel()->index(left.row(), PATH);
      right = sourceModel()->index(right.row(), PATH);
    }
    return QString::localeAwareCompare(sourceModel()->data(left).toString(), sourceModel()->data(right).toString()) < 0;
  }
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

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableViewRouteExport);

  connect(ui->buttonBoxRouteExport, &QDialogButtonBox::clicked, this, &RouteMultiExportDialog::buttonBoxClicked);
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
      formatMap->value(static_cast<rexp::RouteExportFormatType>(itemModel->item(row, FIRST_COL)->data().toInt()));

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
  itemModel->setHeaderData(CHECKBOX, Qt::Horizontal, tr("Export"));
  itemModel->setHeaderData(CATEGORY, Qt::Horizontal, tr("Category"));
  itemModel->setHeaderData(DESCRIPTION, Qt::Horizontal, tr("Usage"));
  itemModel->setHeaderData(EXTENSION, Qt::Horizontal, tr("Extension\nor Filename"));
  itemModel->setHeaderData(PATH_BUTTONS, Qt::Horizontal, tr("Select Path/\nReset/Save"));
  itemModel->setHeaderData(PATH, Qt::Horizontal,
                           tr("Default Export Path - can depend on currently selected Simulator"));

  // Fill model ============================================
  int row = 0;
  for(const RouteExportFormat& format : *formatMap)
  {
    // Userdata needed in callbacks
    int userdata = format.getTypeAsInt();

    // Active/inactive checkbox =============================================================
    QStandardItem *item = new QStandardItem();
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setCheckState(format.isSelected() ? Qt::Checked : Qt::Unchecked);
    item->setData(userdata);
    item->setToolTip(tr("Check to include in export"));
    itemModel->setItem(row, CHECKBOX, item);

    // Category =============================================================
    item = new QStandardItem(format.getCategory());
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

    // Reset and select buttons =============================================================
    QPushButton *selectButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/fileopen.svg"), QString());
    selectButton->setToolTip(tr("Select a path or file that will be used for saving the files.\n"
                                "This path applies to all simulators once chosen."));
    selectButton->setProperty("format", userdata);
    selectButton->setProperty("row", row);
    selectButton->setAutoFillBackground(true);
    connect(selectButton, &QPushButton::clicked, this, &RouteMultiExportDialog::selectPathClicked);

    QPushButton *resetButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/reset.svg"), QString());
    resetButton->setToolTip(tr("Reset path back to default."));
    resetButton->setProperty("format", userdata);
    resetButton->setProperty("row", row);
    resetButton->setAutoFillBackground(true);
    connect(resetButton, &QPushButton::clicked, this, &RouteMultiExportDialog::resetPathClicked);

    QPushButton *saveButton = new QPushButton(QIcon(":/littlenavmap/resources/icons/filesaveas.svg"), QString());
    saveButton->setToolTip(tr("Save now."));
    saveButton->setProperty("format", userdata);
    saveButton->setProperty("row", row);
    saveButton->setAutoFillBackground(true);
    connect(saveButton, &QPushButton::clicked, this, &RouteMultiExportDialog::saveNowClicked);

    // Top level widget and layout =========
    QWidget *cellWidget = new QWidget();
    cellWidget->setAutoFillBackground(true);
    QLayout *layout = new QHBoxLayout(cellWidget);
    layout->addWidget(selectButton);
    layout->addWidget(resetButton);
    layout->addWidget(saveButton);
    layout->setAlignment(Qt::AlignLeft);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    ui->tableViewRouteExport->setIndexWidget(proxy->mapFromSource(itemModel->index(row, PATH_BUTTONS)), cellWidget);

    // Path =============================================================
    item = new QStandardItem(format.getPathOrDefault());
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(userdata);
    item->setToolTip(tr("Double click to edit"));
    itemModel->setItem(row, PATH, item);

    row++;
  }
  ui->tableViewRouteExport->horizontalHeader()->restoreState(state);
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

void RouteMultiExportDialog::saveNowClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(button->property("format").toInt());
    qDebug() << Q_FUNC_INFO << type;
    emit saveNowButtonClicked(formatMap->value(type));
  }
}

void RouteMultiExportDialog::resetPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(button->property("format").toInt());
    qDebug() << Q_FUNC_INFO << type;

    // Set custom path to empty
    formatMap->updatePath(type, QString());

    // Update table item
    itemModel->item(button->property("row").toInt(), PATH)->setText(formatMap->value(type).getDefaultPath());
    updateTableColors();
  }
}

void RouteMultiExportDialog::selectPathClicked()
{
  QPushButton *button = dynamic_cast<QPushButton *>(sender());
  if(button != nullptr)
  {
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(button->property("format").toInt());
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
      QStandardItem *item = itemModel->item(button->property("row").toInt(), PATH);

      if(item->text() == formatMap->value(type).getDefaultPath())
        // Path is the same as default - clear custom path
        formatMap->updatePath(type, QString());
      else
        // Update custom path
        formatMap->updatePath(type, item->text());

      // Update table item
      item->setText(filepath);
      updateTableColors();
    }
  }
}

void RouteMultiExportDialog::itemChanged(QStandardItem *item)
{
  if(!changingTable)
  {
    qDebug() << Q_FUNC_INFO << item->column() << item->row() << item->text();
    rexp::RouteExportFormatType type = static_cast<rexp::RouteExportFormatType>(item->data().toInt());
    if(item->column() == CHECKBOX)
    {
      // Checkbox clicked - update format in map and colors
      formatMap->setSelected(type, item->checkState() == Qt::Checked);
      updateTableColors();
    }
    else if(item->column() == PATH)
    {

      if(item->text() == formatMap->value(type).getDefaultPath())
        // Path is the same as default - clear custom path
        formatMap->updatePath(type, QString());
      else
        // Update custom path
        formatMap->updatePath(type, item->text());
      updateTableColors();
    }
  }
}
