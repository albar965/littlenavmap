/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_ROUTEEXPORTALLDIALOG_H
#define LNM_ROUTEEXPORTALLDIALOG_H

#include "routeexport/routeexportflags.h"

#include <QDialog>
#include <QMap>

namespace Ui {
class RouteMultiExportDialog;
}

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

class QAbstractButton;
class QStandardItem;
class QStandardItemModel;
class QCheckBox;
class QItemSelection;
class QStandardItem;
class TableSortProxyModel;
class RouteExportFormatMap;
class RouteExportFormat;

/*
 * Dialog for multiexport. Allows to modify selection status and custom paths of export formats.
 */
class RouteMultiExportDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RouteMultiExportDialog(QWidget *parent, RouteExportFormatMap *exportFormatMap);
  virtual ~RouteMultiExportDialog() override;

  RouteMultiExportDialog(const RouteMultiExportDialog& other) = delete;
  RouteMultiExportDialog& operator=(const RouteMultiExportDialog& other) = delete;

  virtual int exec() override;

  /* Save and restore dialog state */
  void saveState() const;
  void restoreState();

  void fontChanged(const QFont&);

  enum ExportOptions
  {
    FILEDIALOG, /* Open file dialog on every save */
    RENAME_EXISTING, /* Create rolling backups */
    OVERWRITE /* Overwrite without asking */
  };

  /* Get combo box selection */
  ExportOptions getExportOptions()const;

  /* Set current and default path for the LNMPLN export */
  void setLnmplnExportDir(const QString& dir);

signals:
  /* Save now button list is clicked */
  void saveNowButtonClicked(const RouteExportFormat& format);

  /* Export all selected in button box clicked */
  void saveSelectedButtonClicked();

private:
  void buttonBoxClicked(QAbstractButton *button);

  /* Select path in list clicked */
  void selectPathClicked();
  void selectPath(rexp::RouteExportFormatType type, int row);

  /* Reset path back to default in list clicked */
  void resetPathClicked();
  void resetPath(rexp::RouteExportFormatType type, int row);

  /* Reset from action */
  void actionResetExportPatternTriggered();
  void resetPattern(rexp::RouteExportFormatType type, int row);

  /* Save now in list clicked */
  void saveNowClicked();

  /* Update objects in format map on edit on double click */
  void itemChanged(QStandardItem *item);

  /* Update table colors for state */
  void updateTableColors();

  /* Change combo box */
  void setExportOptions(RouteMultiExportDialog::ExportOptions options);

  /* Save only dialog size */
  void saveDialogState();

  /* Update table - clear and fill again */
  void updateModel();

  /* Update bottom label */
  void updateLabel();

  /* Enable or disable actions depending on selection */
  void updateActions();

  void selectionChanged(const QItemSelection&, const QItemSelection&);

  /* Checkbox toggled */
  void selectForExportToggled();
  void selectForExport(rexp::RouteExportFormatType type, bool checked);

  void tableContextMenu(const QPoint&);

  /* Called from context menu method */
  void resetPathsAndSelection();

  /* Called by connected actions */
  void actionEditPathTriggered();
  void actionEditPatternTriggered();
  void actionExportFileNowTriggered();
  void actionResetExportPathTriggered();
  void actionSelectExportPathTriggered();
  void actionSelectTriggered();

  void saveTableLayout();
  void loadTableLayout();

#ifdef DEBUG_INFORMATION_MULTIEXPORT
  void resetPathsAndSelectionDebug();

#endif

  /* Get elements, indexes and row for current selection or -1 if nothing selected */
  QModelIndex selectedIndex();
  int selectedRow();
  QStandardItem *selectedItem(int col);
  rexp::RouteExportFormatType selectedType();

  /* Item model for unsorted entries */
  QStandardItemModel *itemModel = nullptr;

  /* Proxy model for sorting (also checkbox state) */
  TableSortProxyModel *proxyModel = nullptr;

  /* Used to fix excessive default margins in table */
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  RouteExportFormatMap *formatMapDialog = nullptr, /* Map that will be modified in the dialog */
                       *formatMapSystem = nullptr; /* Map backup that will be used to restore in case of cancel */

  /* Keep index for checkbox in field */
  QMap<rexp::RouteExportFormatType, QCheckBox *> selectCheckBoxIndex;

  /* Avoid updates in itemChanged() when filling table */
  bool changingTable = false;

  /* Avoid sorting of the first column on first startup */
  bool firstStart = false;

  /* Combox box status */
  ExportOptions exportOptions = FILEDIALOG;

  Ui::RouteMultiExportDialog *ui;
};

#endif // LNM_ROUTEEXPORTALLDIALOG_H
