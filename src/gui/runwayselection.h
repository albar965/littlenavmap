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

#ifndef LNM_RUNWAYSELECTION_H
#define LNM_RUNWAYSELECTION_H

#include <QObject>

namespace map {
struct MapRunway;
struct MapRunwayEnd;
struct MapAirport;
}

class QLabel;
class QTableWidget;

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

struct RunwayIdxEntry;

/*
 * Provides methods to fill a table widget with runway information from an airport and to select a runway.
 */
class RunwaySelection :
  public QObject
{
  Q_OBJECT

public:
  /* The table should allow only single row selection */
  explicit RunwaySelection(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableWidgetParam);
  virtual ~RunwaySelection() override;

  RunwaySelection(const RunwaySelection& other) = delete;
  RunwaySelection& operator=(const RunwaySelection& other) = delete;

  /* Call to restore settings and to load runway information into the widget */
  void restoreState();

  /* Optional label widget for airport information */
  void setAirportLabel(QLabel *value)
  {
    airportLabel = value;
  }

  /* Get currently selected runway and runway end */
  void getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const;

  QString getCurrentSelectedName() const;

  /* Get airport as passed in into the constructor */
  const map::MapAirport& getAirport() const;

  /* Set to true to show pattern altitude in the list. Otherwise omitted */
  void setShowPattern(bool value)
  {
    showPattern = value;
  }

  bool hasRunways() const
  {
    return !runways.isEmpty();
  }

  /* Set a list of runways to use as filter. Can uase RW prefix or not. All are shown if empty. No wildcards. */
  void setRunwayNameFilter(const QStringList& value)
  {
    runwayNameFilter = value;
  }

signals:
  /* Selection in the table has changed. */
  void itemSelectionChanged();

  /* An entry was double clicked in the table */
  void doubleClicked();

private:
  void fillAirportLabel();
  void fillRunwayList();
  void addItem(const RunwayIdxEntry& entry, const QString& windText, int index);

  /* Apply exact match runway filter. Prefix RW can be used */
  bool includeRunway(const QString& runwayName);

  /* List of runways (once per end) and runway ends. Index is attached to table item in the first column */
  QList<RunwayIdxEntry> runways;
  map::MapAirport *airport = nullptr;
  bool showPattern = false;

  QStringList runwayNameFilter;
  QLabel *airportLabel = nullptr;
  QTableWidget *runwayTableWidget = nullptr;
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;
};

#endif // LNM_RUNWAYSELECTION_H
