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

#include "markerdialog.h"

#include "common/mapresult.h"
#include "common/unit.h"
#include "gui/tools.h"

#include <QColorDialog>
#include <QLabel>
#include <QPushButton>

MarkerDialogPrivate::MarkerDialogPrivate(QWidget *parent, const QString& titleType, const map::MapResult& resultParam, bool editModeParam)
  :parentWidget(parent), editMode(editModeParam), title(titleType)
{
  result = new map::MapResult;
  *result = resultParam;

  if(result->size() > 1)
    qWarning() << Q_FUNC_INFO << "Result for marker dialog has more than one object" << result;
}

MarkerDialogPrivate::~MarkerDialogPrivate()
{
  delete result;
}

void MarkerDialogPrivate::updateHeader(map::MapType navType, atools::geo::Pos& pos)
{
  if(labelHeader != nullptr)
  {
    QString text;

    if(navType == map::AIRPORT && result->hasAirports())
      text = map::airportText(result->airports.constFirst());
    else if(navType == map::VOR && result->hasVor())
    {
      text = map::vorText(result->vors.constFirst());
      if(result->vors.constFirst().range < 1)
        text.append(tr(" (no range)"));
    }
    else if(navType == map::NDB && result->hasNdb())
    {
      text = map::ndbText(result->ndbs.constFirst());
      if(result->ndbs.constFirst().range < 1)
        text.append(tr(" (no range)"));

    }
    else if(navType == map::WAYPOINT && result->hasWaypoints())
      text = map::waypointText(result->waypoints.constFirst());
    else if(navType == map::USERPOINT && result->hasUserpoints())
      text = map::userpointText(result->userpoints.constFirst());
    else
      text = tr("Coordinates %1").arg(Unit::coords(pos));

    labelHeader->setText(tr("<p><b>%1</b></p>").arg(text));
  }
}

void MarkerDialogPrivate::colorButtonClicked(QColor& markerColorRef)
{
  QColor color = QColorDialog::getColor(markerColorRef, parentWidget,
                                        QCoreApplication::applicationName() % tr(" - Select Color for %1").arg(title),
                                        QColorDialog::ShowAlphaChannel);
  if(color.isValid())
  {
    markerColorRef = color;
    updateButtonColor(markerColorRef);
  }
}

void MarkerDialogPrivate::updateButtonColor(QColor& markerColorRef)
{
  if(pushButtonColor != nullptr)
    atools::gui::changeWidgetColor(pushButtonColor, markerColorRef);
}
