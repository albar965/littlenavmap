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

#ifndef LITTLENAVMAP_ABSTRACTSEARCH_H
#define LITTLENAVMAP_ABSTRACTSEARCH_H

#include <QObject>

#include "common/tabindexes.h"

namespace Ui {
class MainWindow;
}

namespace map {
struct MapResult;

}

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

class QMainWindow;

class AbstractSearch :
  public QObject
{
  Q_OBJECT

public:
  explicit AbstractSearch(QMainWindow *parent, si::TabSearchId tabWidgetIndex);
  virtual ~AbstractSearch() override;

  /* Disconnect and reconnect queries on database change */
  virtual void preDatabaseLoad() = 0;
  virtual void postDatabaseLoad() = 0;

  /* Save and restore state of table header, sort column, search criteria and more */
  virtual void saveState() = 0;
  virtual void restoreState() = 0;

  /* Get all selected map objects (MapAirport will be only partially filled */
  virtual void getSelectedMapObjects(map::MapResult& result) const = 0;

  /* Options dialog has changed some options */
  virtual void optionsChanged() = 0;

  /* GUI style has changed */
  virtual void styleChanged() = 0;

  /* Has to be called by the derived classes. Connects double click, context menu and some other actions */
  virtual void connectSearchSlots() = 0;

  virtual void updateUnits() = 0;

  /* Causes a selectionChanged signal to be emitted so map highlights and status label can be updated */
  virtual void updateTableSelection(bool noFollow) = 0;
  virtual void tabDeactivated() = 0;

  virtual void clearSelection() = 0;
  virtual bool hasSelection() const = 0;

  /* Same as show information and show on map */
  virtual void showSelectedEntry() = 0;

  /*Activate list or tree widget */
  virtual void activateView() = 0;

  /* Zoom to first in the list */
  virtual void showFirstEntry() = 0;

  si::TabSearchId getTabIndex() const
  {
    return tabIndex;
  }

protected:
  /* Used to make the table rows smaller and also used to adjust font size */
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;
  /* Tab index of this search tab on the search dock window */
  si::TabSearchId tabIndex;
  QMainWindow *mainWindow = nullptr;
  Ui::MainWindow *ui = nullptr;
};

#endif // LITTLENAVMAP_ABSTRACTSEARCH_H
