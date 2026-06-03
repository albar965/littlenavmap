/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

class MapQuery;
namespace map {
struct MapRunway;
struct MapRunwayEnd;
struct MapAirport;
}

class QLabel;
class QTableWidget;

namespace atools {
namespace gui {
class WidgetZoomHandler;
}
}

struct RunwayIdxEntry;

/*
 * Provides methods to fill a table widget with runway information from an airport and to select a runway or the airport.
 */
class RunwayTable
  : public QObject
{
  Q_OBJECT

public:
  /* The table should allow only single row selection */
  explicit RunwayTable(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableWidgetParam, bool navdataParam,
                       bool addAirportParam);
  virtual ~RunwayTable() override;

  RunwayTable(const RunwayTable& other) = delete;
  RunwayTable& operator=(const RunwayTable& other) = delete;

  /* Load runway information into the table and select rows for setPreSelected() */
  void init();

  /* Optional label widget for airport information */
  void setAirportLabel(QLabel *value)
  {
    airportLabel = value;
  }

  /* Runway end id to be selected. Has to match navdataParam. Ignored if -1. Selected in restoreState().
   Defaults to first row which is airport */
  void setPreSelectedRunwayEnd(int runwayEndIdSelectedParam)
  {
    runwayEndIdPreSelected = runwayEndIdSelectedParam;
  }

  /* Runway end name to be selected. Name is looked up in a fuzzy way. Selected in restoreState().
   Defaults to first row which is airport */
  void setPreSelectedRunwayEnd(const QString& name, QString *error);

  /* Get currently selected runway and runway end and optionally airport selection */
  void getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end, bool *airportSelected = nullptr) const;

  /* Get currently selected runway name and optionally airport selection */
  QString getCurrentSelectedName(bool *airportSelected = nullptr) const;

  /* Get airport as passed in into the constructor */
  const map::MapAirport& getAirport() const;

  /* Set to true to show pattern altitude in the list. Otherwise omitted */
  void setShowPattern(bool value)
  {
    showPattern = value;
  }

  /* true if given airport has runways, i.e. is no heliport or other */
  bool hasRunways() const;

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

  // Add airport on top of list
  void addAirportItem(const map::MapAirport& airport, int tableIndex);

  // Add runway
  void addRunwayItem(const RunwayIdxEntry& entry, const QString& windText, int tableIndex, int runwayListIndex);

  /* Apply exact match runway filter. Prefix RW can be used */
  bool includeRunway(const QString& runwayName);

  /* List of runways (once per end) and runway ends. Index is attached to table item in the first column */
  QList<RunwayIdxEntry> runways;
  map::MapAirport *airport = nullptr;
  bool showPattern = false;

  QStringList runwayNameFilter;
  QLabel *airportLabel = nullptr;
  QTableWidget *runwayTableWidget = nullptr;
  atools::gui::WidgetZoomHandler *zoomHandler = nullptr;

  MapQuery *mapQuery;
  bool navdata, addAirport;

  int runwayEndIdPreSelected = -1;
};

#endif // LNM_RUNWAYSELECTION_H
