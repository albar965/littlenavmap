/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

namespace map {
struct MapSearchResult;

}

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

class QMainWindow;

enum SearchTabIndex
{
  SEARCH_AIRPORT = 0,
  SEARCH_NAV = 1,
  SEARCH_PROC = 2,
  SEARCH_USER = 3
};

class AbstractSearch :
  public QObject
{
  Q_OBJECT

public:
  AbstractSearch(QMainWindow *parent, SearchTabIndex tabWidgetIndex);
  virtual ~AbstractSearch();

  /* Disconnect and reconnect queries on database change */
  virtual void preDatabaseLoad() = 0;
  virtual void postDatabaseLoad() = 0;

  /* Save and restore state of table header, sort column, search criteria and more */
  virtual void saveState() = 0;
  virtual void restoreState() = 0;

  /* Get all selected map objects (MapAirport will be only partially filled */
  virtual void getSelectedMapObjects(map::MapSearchResult& result) const = 0;

  /* Options dialog has changed some options */
  virtual void optionsChanged() = 0;

  /* Has to be called by the derived classes. Connects double click, context menu and some other actions */
  virtual void connectSearchSlots() = 0;

  virtual void updateUnits() = 0;

  /* Causes a selectionChanged signal to be emitted so map hightlights and status label can be updated */
  virtual void updateTableSelection() = 0;
  virtual void tabDeactivated() = 0;

  SearchTabIndex getTabIndex() const
  {
    return tabIndex;
  }

protected:
  /* Used to make the table rows smaller and also used to adjust font size */
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;
  /* Tab index of this search tab on the search dock window */
  SearchTabIndex tabIndex;
  QMainWindow *mainWindow = nullptr;

};

#endif // LITTLENAVMAP_ABSTRACTSEARCH_H
