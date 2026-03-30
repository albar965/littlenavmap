/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "search/abstractsearch.h"

#include "app/navapp.h"
#include "atools.h"
#include "gui/mainwindow.h"
#include "gui/widgetzoomhandler.h"
#include "options/optiondata.h"

AbstractSearch::AbstractSearch(MainWindow *parent, QWidget *resultWidgetParam, si::TabSearchId tabWidgetIndex)
  : QObject(parent), tabIndex(tabWidgetIndex), mainWindow(parent), parentWidget(parent)
{
  ui = NavApp::getMainUi();
  zoomHandler = new atools::gui::WidgetZoomHandler(resultWidgetParam);

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
}

AbstractSearch::~AbstractSearch()
{
  ATOOLS_DELETE_LOG(zoomHandler);
}

void AbstractSearch::fontChanged(const QFont&)
{
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
}
