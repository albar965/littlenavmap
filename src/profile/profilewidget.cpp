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

#include "profile/profilewidget.h"

#include "atools.h"
#include "navapp.h"
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "profile/profilescrollarea.h"
#include "profile/profilelabelwidget.h"
#include "ui_mainwindow.h"
#include "common/symbolpainter.h"
#include "util/htmlbuilder.h"
#include "route/routecontroller.h"
#include "route/routealtitude.h"
#include "common/aircrafttrack.h"
#include "common/unit.h"
#include "common/formatter.h"
#include "settings/settings.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"
#include "common/elevationprovider.h"
#include "common/vehicleicons.h"
#include "util/paintercontextsaver.h"
#include "common/jumpback.h"
#include "weather/windreporter.h"
#include "grib/windquery.h"
#include "options/optiondata.h"
#include "perf/aircraftperfcontroller.h"

#include <QPainter>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLineString.h>

/* Maximum delta values depending on update rate in options */
// int manhattanLengthDelta;
// float altitudeDelta;
static QHash<opts::SimUpdateRate, ProfileWidget::SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
{
  {
    opts::FAST, {1, 1.f}
  },
  {
    opts::MEDIUM, {2, 10.f}
  },
  {
    opts::LOW, {4, 20.f}
  }
});

using Marble::GeoDataCoordinates;
using Marble::GeoDataLineString;
using atools::geo::Pos;
using atools::geo::LineString;

// =======================================================================================

/* Route leg storing all elevation points */
struct ElevationLeg
{
  atools::geo::LineString elevation, /* Ground elevation (Pos.altitude) and position */
                          geometry; /* Route geometry. Normally only start and endpoint.
                                     * More complex for procedures. */
  QVector<double> distances; /* Distances along the route for each elevation point.
                              *  Measured from departure point. Nautical miles. */
  float maxElevation = 0.f; /* Max ground altitude for this leg */
};

struct ElevationLegList
{
  Route route; /* Copy from route controller.
                * Need a copy to avoid thread synchronization problems. */
  QList<ElevationLeg> elevationLegs; /* Elevation data for each route leg */
  float maxElevationFt = 0.f /* Maximum ground elevation for the route */,
        totalDistance = 0.f /* Total route distance in nautical miles */;
  int totalNumPoints = 0; /* Number of elevation points in whole flight plan */
};

// =======================================================================================

ProfileWidget::ProfileWidget(QWidget *parent)
  : QWidget(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

  setContextMenuPolicy(Qt::DefaultContextMenu);
  setFocusPolicy(Qt::WheelFocus);

  legList = new ElevationLegList;

  ui->labelProfileError->setVisible(false);

  scrollArea = new ProfileScrollArea(this, ui->scrollAreaProfile);
  scrollArea->setProfileLeftOffset(left);
  scrollArea->setProfileTopOffset(TOP);

  jumpBack = new JumpBack(this);
  connect(jumpBack, &JumpBack::jumpBack, this, &ProfileWidget::jumpBackToAircraftTimeout);

  connect(scrollArea, &ProfileScrollArea::showPosAlongFlightplan, this, &ProfileWidget::showPosAlongFlightplan);
  connect(scrollArea, &ProfileScrollArea::hideRubberBand, this, &ProfileWidget::hideRubberBand);
  connect(scrollArea, &ProfileScrollArea::jumpBackToAircraftStart, this, &ProfileWidget::jumpBackToAircraftStart);
  connect(scrollArea, &ProfileScrollArea::jumpBackToAircraftCancel, this, &ProfileWidget::jumpBackToAircraftCancel);
  connect(ui->actionProfileCenterAircraft, &QAction::toggled, this, &ProfileWidget::jumpBackToAircraftCancel);

  // Create single shot timer that will restart the thread after a delay
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  // Marble will let us know when updates are available
  connect(NavApp::getElevationProvider(), &ElevationProvider::updateAvailable,
          this, &ProfileWidget::elevationUpdateAvailable);

  // Notification from thread that it has finished and we can get the result from the future
  connect(&watcher, &QFutureWatcher<ElevationLegList>::finished, this, &ProfileWidget::updateThreadFinished);

  // Want mouse events even when no button is pressed
  setMouseTracking(true);
}

ProfileWidget::~ProfileWidget()
{
  qDebug() << Q_FUNC_INFO << "delete jumpBack";
  delete jumpBack;

  updateTimer->stop();
  updateTimer->deleteLater();
  terminateThread();
  delete scrollArea;
  delete legList;
}

void ProfileWidget::aircraftTrackPruned()
{
  if(!widgetVisible)
    return;

  updateScreenCoords();
  update();
}

float ProfileWidget::aircraftAlt(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  return aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT ?
         aircraft.getIndicatedAltitudeFt() :
         aircraft.getPosition().getAltitude();
}

void ProfileWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(databaseLoadStatus || !simulatorData.getUserAircraftConst().isValid())
    return;

  bool updateWidget = false;

  // Need route with update active leg and aircraft position
  const Route& route = NavApp::getRouteConst();

  if(!route.isFlightplanEmpty())
  {
    simData = simulatorData;

    bool lastPosValid = lastSimData.getUserAircraftConst().isValid();
    bool simPosValid = simData.getUserAircraftConst().isValid();

    float lastAlt = aircraftAlt(lastSimData.getUserAircraftConst());
    float simAlt = aircraftAlt(simData.getUserAircraftConst());

    aircraftDistanceFromStart = route.getProjectionDistance();

    // Update if new or last distance is/was invalid
    updateWidget = (aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) !=
                   (lastAircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) && widgetVisible;

    if(aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
    {
#ifdef DEBUG_INFORMATION_PROFILE_SIMDATA
      if(simData.getUserAircraft().isDebug())
        qDebug() << Q_FUNC_INFO << aircraftDistanceFromStart;
#endif

      // Get point (NM/ft) for current update
      QPointF currentPoint(aircraftDistanceFromStart, simAlt);

      // Add track point if delta value between last and current update is large enough
      if(simPosValid)
      {
        if(aircraftTrackPoints.isEmpty())
          aircraftTrackPoints.append(currentPoint);
        else
        {
          QLineF delta(aircraftTrackPoints.last(), currentPoint);

          if(std::abs(delta.dx()) > 0.1 /* NM */ || std::abs(delta.dy()) > 50. /* ft */)
            aircraftTrackPoints.append(currentPoint);

          if(aircraftTrackPoints.size() > OptionData::instance().getAircraftTrackMaxPoints())
            aircraftTrackPoints.removeFirst();
        }
      }

      if(widgetVisible)
      {
        const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

        using atools::almostNotEqual;
        // Get point (NM/ft) from last update
        QPointF lastPoint;
        if(lastPosValid)
          lastPoint = QPointF(lastAircraftDistanceFromStart, lastAlt);

        // Update widget if delta value between last and current update is large enough
        if(!lastPosValid || // No last position
           (toScreen(lastPoint) - toScreen(currentPoint)).manhattanLength() >= deltas.manhattanLengthDelta || // Position change on screen
           almostNotEqual(lastAlt, simAlt, deltas.altitudeDelta) // Altitude change
           )
        {
          movingBackwards = (lastAircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) &&
                            (lastAircraftDistanceFromStart > aircraftDistanceFromStart);

          lastSimData = simData;
          lastAircraftDistanceFromStart = aircraftDistanceFromStart;

          if(simAlt > maxWindowAlt)
            // Scale up to keep the aircraft visible
            updateScreenCoords();

          // Probably center aircraft on scroll area
          if(NavApp::getMainUi()->actionProfileCenterAircraft->isChecked() && !jumpBack->isActive())
            scrollArea->centerAircraft(toScreen(currentPoint),
                                       simData.getUserAircraftConst().getVerticalSpeedFeetPerMin());

          // Aircraft position has changed enough
          updateWidget = true;
        } // if(!lastPosValid ...
      } // if(widgetVisible)
    } // if(aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
  } // if(!NavApp::getRouteConst().isFlightplanEmpty())

  if(updateWidget)
    update();

  if(widgetVisible)
    updateLabel();
}

void ProfileWidget::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  jumpBack->cancel();
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateLabel();
}

void ProfileWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  jumpBack->cancel();
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateLabel();
  scrollArea->hideTooltip();
}

float ProfileWidget::calcGroundBuffer(float maxElevation)
{
  float groundBuffer = Unit::rev(OptionData::instance().getRouteGroundBuffer(), Unit::altFeetF);
  float roundBuffer = Unit::rev(500.f, Unit::altFeetF);

  return std::ceil((maxElevation + groundBuffer) / roundBuffer) * roundBuffer;
}

void ProfileWidget::updateScreenCoords()
{
  /* Update all screen coordinates and scale factors */

  calcLeftMargin();

  // Widget drawing region width and height
  int w = rect().width() - left * 2, h = rect().height() - TOP;

  // Need scale to determine track length on screen
  horizontalScale = w / legList->totalDistance;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = calcGroundBuffer(legList->maxElevationFt);
  maxWindowAlt = std::max(minSafeAltitudeFt, legList->route.getCruisingAltitudeFeet());

  if(simData.getUserAircraftConst().isValid() &&
     (showAircraft || showAircraftTrack) && !NavApp::getRouteConst().isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, aircraftAlt(simData.getUserAircraftConst()));

  // if(showAircraftTrack)
  // maxWindowAlt = std::max(maxWindowAlt, maxTrackAltitudeFt);

  scrollArea->routeAltitudeChanged();

  verticalScale = h / maxWindowAlt;

  // Calculate the landmass polygon
  waypointX.clear();
  landPolygon.clear();

  // First point
  landPolygon.append(QPoint(left, h + TOP));

  for(const ElevationLeg& leg : legList->elevationLegs)
  {
    if(leg.distances.isEmpty() || leg.elevation.isEmpty())
      continue;

    waypointX.append(left + static_cast<int>(leg.distances.first() * horizontalScale));

    QPoint lastPt;
    for(int i = 0; i < leg.elevation.size(); i++)
    {
      float alt = leg.elevation.at(i).getAltitude();
      QPoint pt(left + static_cast<int>(leg.distances.at(i) * horizontalScale),
                TOP + static_cast<int>(h - alt * verticalScale));

      if(lastPt.isNull() || i == leg.elevation.size() - 1 || (lastPt - pt).manhattanLength() > 2)
      {
        landPolygon.append(pt);
        lastPt = pt;
      }
    }
  }

  // Destination point
  waypointX.append(left + w);

  // Last point closing polygon
  landPolygon.append(QPoint(left + w, h + TOP));
}

QVector<std::pair<int, int> > ProfileWidget::calcScaleValues()
{
  int h = rect().height() - TOP;
  // Create a temporary scale based on current units
  float tempScale = Unit::rev(verticalScale, Unit::altFeetF);

  // Find best scale for elevation lines
  float step = 10000.f;
  for(int stp : SCALE_STEPS)
  {
    // Loop through all scale steps and check which one fits best
    if(stp * tempScale > MIN_SCALE_SCREEN_DISTANCE)
    {
      step = stp;
      break;
    }
  }

  // Now collect all scale positions by iterating step-wise from bottom to top
  QVector<std::pair<int, int> > steps;
  for(float y = TOP + h, alt = 0; y > TOP; y -= step * tempScale, alt += step)
    steps.append(std::make_pair(y, alt));

  return steps;
}

int ProfileWidget::getMinSafeAltitudeY() const
{
  return altitudeY(minSafeAltitudeFt);
}

int ProfileWidget::getFlightplanAltY() const
{
  return altitudeY(legList->route.getCruisingAltitudeFeet());
}

bool ProfileWidget::hasValidRouteForDisplay() const
{
  return widgetVisible && !legList->elevationLegs.isEmpty() && legList->route.getSizeWithoutAlternates() >= 2 &&
         legList->route.getAltitudeLegs().size() >= 2 &&
         legList->route.size() == legList->route.getAltitudeLegs().size();
}

int ProfileWidget::altitudeY(float altitudeFt) const
{
  if(altitudeFt < map::INVALID_DISTANCE_VALUE)
    return static_cast<int>(rect().height() - altitudeFt * verticalScale);
  else
    return map::INVALID_INDEX_VALUE;
}

QPoint ProfileWidget::toScreen(const QPointF& pt) const
{
  return QPoint(distanceX(static_cast<float>(pt.x())), altitudeY(static_cast<float>(pt.y())));
}

QPolygon ProfileWidget::toScreen(const QPolygonF& leg) const
{
  QPolygon retval;
  for(const QPointF& point : leg)
  {
    QPoint pt = toScreen(point);
    if(retval.isEmpty())
      retval.append(pt);
    else if((retval.last() - point).manhattanLength() > 3)
      retval.append(pt);
  }
  return retval;
}

int ProfileWidget::distanceX(float distanceNm) const
{
  if(distanceNm < map::INVALID_DISTANCE_VALUE)
    return left + atools::roundToInt(distanceNm * horizontalScale);
  else
    return map::INVALID_INDEX_VALUE;
}

void ProfileWidget::showPosAlongFlightplan(int x, bool doubleClick)
{
  // Check range before
  if(x > left && x < width() - left)
    emit showPos(calculatePos(x), 0.f, doubleClick);
}

void ProfileWidget::paintIls(QPainter& painter, const Route& route)
{
  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  const QVector<map::MapIls>& ilsVector = route.getDestRunwayIlsProfile();
  if(!ilsVector.isEmpty())
  {
    // Get origin
    int x = distanceX(altitudeLegs.getDestinationDistance());
    int y = altitudeY(altitudeLegs.getDestinationAltitude());

    if(x < map::INVALID_INDEX_VALUE && y < map::INVALID_INDEX_VALUE)
    {
      // Draw all ILS
      for(const map::MapIls& ils : ilsVector)
      {
        bool isIls = !ils.isAnyGls();

        QColor fillColor = isIls ? mapcolors::ilsFillColor : mapcolors::glsFillColor;
        QColor symColor = isIls ? mapcolors::ilsSymbolColor : mapcolors::glsSymbolColor;
        QColor textColor = isIls ? mapcolors::ilsTextColor : mapcolors::glsTextColor;
        QPen centerPen = isIls ? mapcolors::ilsCenterPen : mapcolors::glsCenterPen;

        painter.setBrush(fillColor);
        painter.setPen(QPen(symColor, 2, Qt::SolidLine, Qt::FlatCap));
        painter.setBackgroundMode(Qt::OpaqueMode);
        painter.setBackground(Qt::transparent);

        // ILS feather length is 9 NM
        float featherLen = atools::geo::nmToFeet(9.f);

        // Calculate altitude at end of feather
        // tan a = GK / AK; // tan a * AK = GK;
        float ydiff1 = std::tan(atools::geo::toRadians(ils.slope)) * featherLen;

        // Calculate screen points for end of feather
        int y2 = altitudeY(altitudeLegs.getDestinationAltitude() + ydiff1);
        int x2 = distanceX(altitudeLegs.getDestinationDistance() - 9.f);

        // Screen difference for +/- 0.35°
        // float ydiffUpper = std::tan(atools::geo::toRadians(ils.slope + 1.f)) * featherLen - ydiff1;

        // Construct geometry
        QLineF centerLine(x, y, x2, y2);
        QLineF lowerLine(x, y, x2, y2 + 25 /*(ydiffUpper *verticalScale)*/);
        QLineF upperLine(x, y, x2, y2 - 25 /*(ydiffUpper *verticalScale)*/);

        // Make all the same length
        lowerLine.setLength(centerLine.length());
        upperLine.setLength(centerLine.length());

        // Shorten the center line
        centerLine.setLength(centerLine.length() - QLineF(lowerLine.p2(), upperLine.p2()).length() / 2.);

        // Draw feather
        painter.drawPolygon(QPolygonF({lowerLine.p1(), lowerLine.p2(), upperLine.p2(), upperLine.p1()}));

        // Draw small indicator for ILS
        painter.drawPolyline(QPolygonF({lowerLine.p2(), centerLine.p2(), upperLine.p2()}));

        // Dashed center line
        painter.setPen(centerPen);
        painter.drawLine(centerLine);

        // Calculate text ==================
        painter.setPen(textColor);
        if(OptionData::instance().getFlags2() & opts2::MAP_NAVAID_TEXT_BACKGROUND)
        {
          painter.setBackground(Qt::white);
          painter.setBackgroundMode(Qt::OpaqueMode);
        }
        else
          painter.setBackgroundMode(Qt::TransparentMode);

        // Place near p2 at end of feather
        double angle = atools::geo::angleFromQt(upperLine.angle());
        painter.translate(upperLine.p2());
        painter.rotate(angle + 90.);
        painter.drawText(10, -painter.fontMetrics().descent(), map::ilsText(ils) + tr(" ►"));
        painter.resetTransform();
      }
    }
  }
}

void ProfileWidget::paintVasi(QPainter& painter, const Route& route)
{
  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  const map::MapRunwayEnd& runwayEnd = altitudeLegs.getDestRunwayEnd();

  if(runwayEnd.isValid())
  {
    // Get origin on screen
    int x = distanceX(altitudeLegs.getDestinationDistance());
    int y = altitudeY(altitudeLegs.getDestinationAltitude());

    // Collect left and right VASI
    QVector<std::pair<float, QString> > vasiList;

    if(runwayEnd.rightVasiPitch > 0.f)
      vasiList.append(std::make_pair(runwayEnd.rightVasiPitch,
                                     runwayEnd.rightVasiType == "UNKN" ? QString() : runwayEnd.rightVasiType));

    if(runwayEnd.leftVasiPitch > 0.f)
      vasiList.append(std::make_pair(runwayEnd.leftVasiPitch,
                                     runwayEnd.leftVasiType == "UNKN" ? QString() : runwayEnd.leftVasiType));

    if(vasiList.isEmpty())
      return;

    if(vasiList.size() == 2)
    {
      // VASI on both sides
      if(atools::almostEqual(vasiList.at(0).first, vasiList.at(1).first))
      {
        if(vasiList.at(0).second != vasiList.at(1).second)
          // Merge type if the angle is the same but type is different
          vasiList[0].second = tr("%1 / %2").arg(vasiList.at(0).second).arg(vasiList.at(1).second);

        // Remove duplicate
        vasiList.removeLast();
      }
    }

    // Draw all VASI =================================================
    for(const std::pair<float, QString>& vasi : vasiList)
    {
      // VASI has shorted visibility than ILS range
      float featherLen = atools::geo::nmToFeet(6.f);

      // Calculate altitude at end of guide
      // tan a = GK / AK; // tan a * AK = GK;
      float ydiff1 = std::tan(atools::geo::toRadians(vasi.first)) * featherLen;

      // Calculate screen points for end of guide
      int yUpper = altitudeY(altitudeLegs.getDestinationAltitude() + ydiff1);
      int xUpper = distanceX(altitudeLegs.getDestinationDistance() - 6.f);

      if(xUpper < map::INVALID_INDEX_VALUE && yUpper < map::INVALID_INDEX_VALUE)
      {
        // Screen difference for +/- one degree
        // float ydiffUpper = std::tan(atools::geo::toRadians(vasi.first + 1.5f)) * featherLen - ydiff1;

        // Build geometry
        QLineF center(x, y, xUpper, yUpper);
        QLineF lower(x, y, xUpper, yUpper + 30 /*(ydiffUpper *verticalScale)*/);
        QLineF upper(x, y, xUpper, yUpper - 30 /*(ydiffUpper *verticalScale)*/);

        lower.setLength(center.length());
        upper.setLength(center.length());

        // Draw lower red guide
        painter.setPen(Qt::NoPen);
        painter.setBrush(mapcolors::profileVasiBelowColor);
        painter.drawPolygon(QPolygonF({lower.p1(), lower.p2(), center.p2(), center.p1()}));

        // Draw above white guide
        painter.setBrush(mapcolors::profileVasiAboveColor);
        painter.drawPolygon(QPolygonF({upper.p1(), upper.p2(), center.p2(), center.p1()}));

        // Draw dashed center line
        painter.setPen(mapcolors::profileVasiCenterPen);
        painter.drawLine(center);

        painter.setPen(Qt::black);
        if(OptionData::instance().getFlags2() & opts2::MAP_NAVAID_TEXT_BACKGROUND)
        {
          painter.setBackground(Qt::white);
          painter.setBackgroundMode(Qt::OpaqueMode);
        }
        else
          painter.setBackgroundMode(Qt::TransparentMode);

        // Draw VASI text ========================
        double angle = atools::geo::angleFromQt(upper.angle());
        painter.translate(upper.p2());
        painter.rotate(angle + 90.);

        QString txt;
        if(vasi.second.isEmpty())
          txt = tr("%1° ►").arg(QLocale().toString(vasi.first, 'f', 1));
        else
          txt = tr("%1° / %2 ►").arg(QLocale().toString(vasi.first, 'f', 1)).arg(vasi.second);
        painter.drawText(10, -painter.fontMetrics().descent(), txt);
        painter.resetTransform();
      }
    }
  }
}

void ProfileWidget::calcLeftMargin()
{
  QFontMetrics metrics(OptionData::instance().getMapFont());
  left = 30;
  if(!legList->route.isEmpty())
  {
    // Calculate departure altitude text size
    float departAlt = legList->route.getDepartureAirportLeg().getPosition().getAltitude();
    if(departAlt < map::INVALID_ALTITUDE_VALUE / 2.f)
      left = std::max(metrics.width(Unit::altFeet(departAlt)), left);

    // Calculate destination altitude text size
    float destAlt = legList->route.getDestinationAirportLeg().getPosition().getAltitude();
    if(destAlt < map::INVALID_ALTITUDE_VALUE / 2.f)
      left = std::max(metrics.width(Unit::altFeet(destAlt)), left);
    left += 8;
    left = std::max(left, 30);
  }
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  // Show only ident in labels
  static const textflags::TextFlags TEXTFLAGS = textflags::IDENT | textflags::ROUTE_TEXT | textflags::ABS_POS;

  // Saved route that was used to create the geometry
  const Route& route = legList->route;

  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  const OptionData& optData = OptionData::instance();

  // Keep margin to left, right and top
  int w = rect().width() - left * 2, h = rect().height() - TOP;

  SymbolPainter symPainter;
  QPainter painter(this);

  // Nothing to show label =========================
  if(route.isEmpty())
  {
    setFont(optData.getGuiFont());
    painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));
    symPainter.textBox(&painter, {tr("No Flight Plan.")}, QColor(255, 80, 0),
                       left + w / 2, TOP + h / 2, textatt::BOLD | textatt::CENTER, 0);
    scrollArea->updateLabelWidget();
    return;
  }
  else if(!hasValidRouteForDisplay())
  {
    setFont(optData.getGuiFont());
    painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));
    symPainter.textBox(&painter, {tr("Flight Plan not valid.")}, QColor(255, 80, 0),
                       left + w / 2, TOP + h / 2, textatt::BOLD | textatt::CENTER, 0);
    scrollArea->updateLabelWidget();
    return;
  }

  if(legList->route.size() != route.size() ||
     atools::almostNotEqual(legList->route.getTotalDistance(), route.getTotalDistance()))
    // Do not draw if route is updated to avoid invalid indexes
    return;

  if(altitudeLegs.size() != route.size())
  {
    // Do not draw if route altitudes are not updated to avoid invalid indexes
    qWarning() << Q_FUNC_INFO << "Route altitudes not updated";
    return;
  }

  // Draw grey vertical lines for waypoints
  int flightplanY = getFlightplanAltY();
  int safeAltY = getMinSafeAltitudeY();
  if(flightplanY == map::INVALID_INDEX_VALUE || safeAltY == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO << "No flight plan elevation";
    return;
  }

  setFont(optData.getMapFont());

  // Fill background sky blue ====================================================
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.fillRect(left, 0, rect().width() - left * 2, rect().height(), mapcolors::profileSkyColor);

  // Draw the ground ======================================================
  painter.setBrush(mapcolors::profileLandColor);
  painter.setPen(mapcolors::profileLandOutlinePen);
  painter.drawPolygon(landPolygon);

  int flightplanTextY = flightplanY + 14;
  painter.setPen(mapcolors::profileWaypointLinePen);
  for(int wpx : waypointX)
    painter.drawLine(wpx, flightplanY, wpx, TOP + h);

  // Draw elevation scale lines ======================================================
  painter.setPen(mapcolors::profileElevationScalePen);
  for(const std::pair<int, int>& scale : calcScaleValues())
    painter.drawLine(0, scale.first, left + static_cast<int>(w), scale.first);

  // Draw one line for the label to the left
  painter.drawLine(0, flightplanY, left + static_cast<int>(w), flightplanY);
  painter.drawLine(0, safeAltY, left, safeAltY);

  // Draw orange minimum safe altitude lines for each segment ======================================================
  painter.setPen(mapcolors::profileSafeAltLegLinePen);
  for(int i = 0; i < legList->elevationLegs.size(); i++)
  {
    if(waypointX.value(i, 0) == waypointX.value(i + 1, 0))
      // Skip zero length segments to avoid dots on the graph
      continue;

    const ElevationLeg& leg = legList->elevationLegs.at(i);
    int lineY = TOP + static_cast<int>(h - calcGroundBuffer(leg.maxElevation) * verticalScale);
    painter.drawLine(waypointX.value(i, 0), lineY, waypointX.value(i + 1, 0), lineY);
  }

  // Draw the red minimum safe altitude line ======================================================
  painter.setPen(mapcolors::profileSafeAltLinePen);
  painter.drawLine(left, safeAltY, left + static_cast<int>(w), safeAltY);

  // Get TOD position from active route  ======================================================

  // Calculate line y positions ======================================================
  bool showTodToc = NavApp::getMapWidget()->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN_TOC_TOD);
  QVector<QPolygon> altLegs; /* Flight plan waypoint screen coordinates. x = distance and y = altitude */

  for(int i = 0; i < altitudeLegs.size(); i++)
  {
    const RouteAltitudeLeg& routeAlt = altitudeLegs.value(i);
    if(routeAlt.isMissed() || routeAlt.isAlternate())
      break;

    QPolygonF geo = routeAlt.getGeometry();
    if(!showTodToc)
    {
      // Set all points to flight plan cruise altitude if no TOD and TOC wanted
      for(QPointF& pt : geo)
        pt.setY(legList->route.getCruisingAltitudeFeet());
    }
    altLegs.append(toScreen(geo));
  }

  // Collect indexes in reverse (painting) order without missed ================
  QVector<int> indexes;
  for(int i = 0; i <= route.getDestinationLegIndex(); i++)
  {
    if(route.value(i).getProcedureLeg().isMissed() || route.value(i).isAlternate())
      break;
    indexes.prepend(i);
  }

  if(NavApp::getMapWidget()->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN))
  {
    // Draw altitude restriction bars ============================
    painter.setBackground(mapcolors::profileAltRestrictionFill);
    painter.setBackgroundMode(Qt::OpaqueMode);
    QBrush diagPatternBrush(mapcolors::profileAltRestrictionOutline, Qt::BDiagPattern);
    QPen thinLinePen(mapcolors::profileAltRestrictionOutline, 1., Qt::SolidLine, Qt::FlatCap);
    QPen thickLinePen(mapcolors::profileAltRestrictionOutline, 4., Qt::SolidLine, Qt::FlatCap);

    for(int routeIndex : indexes)
    {
      if(route.value(routeIndex).isAnyProcedure() && route.value(routeIndex).getRunwayEnd().isValid())
        continue;

      const proc::MapAltRestriction& restriction = altitudeLegs.value(routeIndex).getRestriction();

      if(restriction.isValid() && restriction.descriptor != proc::MapAltRestriction::ILS_AT &&
         restriction.descriptor != proc::MapAltRestriction::ILS_AT_OR_ABOVE)
      {
        // Use 5 NM width and minimum of 10 pix and maximum of 40 pix
        int rectWidth = atools::roundToInt(std::min(std::max(5.f * horizontalScale, 10.f), 40.f));
        int rectHeight = 16;

        // Start and end of line
        int wpx = waypointX.value(routeIndex, 0);
        int x11 = wpx - rectWidth / 2;
        int x12 = wpx + rectWidth / 2;
        int y1 = altitudeY(restriction.alt1);

        if(restriction.isValid())
        {
          proc::MapAltRestriction::Descriptor descr = restriction.descriptor;

          // Connect waypoint with restriction with a thin vertical line
          painter.setPen(thinLinePen);
          painter.drawLine(QPoint(wpx, y1), altLegs.at(routeIndex).last());

          if(descr == proc::MapAltRestriction::AT_OR_ABOVE ||
             descr == proc::MapAltRestriction::AT)
            // Draw diagonal pattern rectangle above
            painter.fillRect(x11, y1, rectWidth, rectHeight, diagPatternBrush);

          if(descr == proc::MapAltRestriction::AT_OR_BELOW ||
             descr == proc::MapAltRestriction::AT)
            // Draw diagonal pattern rectangle below
            painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);

          if(descr == proc::MapAltRestriction::BETWEEN)
          {
            // At or above alt2 and at or below alt1

            // Draw diagonal pattern rectangle for below alt1
            painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);

            int x21 = waypointX.value(routeIndex, 0) - rectWidth / 2;
            int x22 = waypointX.value(routeIndex, 0) + rectWidth / 2;
            int y2 = altitudeY(restriction.alt2);
            // Draw diagonal pattern rectangle for above alt2
            painter.fillRect(x21, y2, rectWidth, rectHeight, diagPatternBrush);

            // Connect above and below with a thin line
            painter.drawLine(waypointX.value(routeIndex, 0), y1, waypointX.value(routeIndex, 0), y2);
            painter.setPen(thickLinePen);

            // Draw line for alt2
            painter.drawLine(x21, y2, x22, y2);
          }

          if(descr != proc::MapAltRestriction::NONE)
          {
            // Draw line for alt1 if any restriction
            painter.setPen(thickLinePen);
            painter.drawLine(x11, y1, x12, y1);
          }
        }
      }
    }
  }

  // Draw ILS or VASI guidance ============================
  mapcolors::scaleFont(&painter, 0.9f);

  if(NavApp::getMainUi()->actionProfileShowVasi->isChecked())
    paintVasi(painter, route);

  if(NavApp::getMainUi()->actionProfileShowIls->isChecked())
    paintIls(painter, route);

  // Get active route leg but ignore alternate legs
  const Route& curRoute = NavApp::getRoute();
  bool activeValid = curRoute.isActiveValid();

  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg =
    activeValid ? atools::minmax(0, waypointX.size() - 1, curRoute.getActiveLegIndex()) : 0;
  int passedRouteLeg = optData.getFlags2() & opts2::MAP_ROUTE_DIM_PASSED ? activeRouteLeg : 0;

  if(curRoute.isActiveAlternate())
  {
    // Disable active leg and show all legs as passed if an alternate is enabled
    activeRouteLeg = 0;
    passedRouteLeg = optData.getFlags2() & opts2::MAP_ROUTE_DIM_PASSED ?
                     std::min(passedRouteLeg + 1, waypointX.size()) : 0;
  }

  if(NavApp::getMapWidget()->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN))
  {
    // Draw background line ======================================================
    float flightplanOutlineWidth = (optData.getDisplayThicknessFlightplan() / 100.f) * 7;
    float flightplanWidth = (optData.getDisplayThicknessFlightplan() / 100.f) * 4;
    painter.setPen(QPen(mapcolors::routeOutlineColor, flightplanOutlineWidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    for(int i = passedRouteLeg; i < waypointX.size(); i++)
    {
      const proc::MapProcedureLeg& leg = route.value(i).getProcedureLeg();
      if(i > 0 && !leg.isCircleToLand() && !leg.isStraightIn() && !leg.isVectors() && !leg.isManual())
        painter.drawPolyline(altLegs.at(i));
    }

    // Draw passed ======================================================
    painter.setPen(QPen(optData.getFlightplanPassedSegmentColor(), flightplanWidth,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      for(int i = 1; i < passedRouteLeg; i++)
        painter.drawPolyline(altLegs.at(i));
    }
    else
      qWarning() << Q_FUNC_INFO;

    // Draw ahead ======================================================
    QPen flightplanPen(optData.getFlightplanColor(), flightplanWidth,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen procedurePen(optData.getFlightplanProcedureColor(), flightplanWidth,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    painter.setBackgroundMode(Qt::OpaqueMode);
    painter.setBackground(Qt::white);
    painter.setBrush(Qt::NoBrush);

    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      for(int i = passedRouteLeg + 1; i < waypointX.size(); i++)
      {
        painter.setPen(altitudeLegs.value(i).isAnyProcedure() ? procedurePen : flightplanPen);
        const proc::MapProcedureLeg& leg = route.value(i).getProcedureLeg();
        if(leg.isCircleToLand() || leg.isStraightIn())
          mapcolors::adjustPenForCircleToLand(&painter);
        else if(leg.isVectors())
          mapcolors::adjustPenForVectors(&painter);
        else if(leg.isManual())
          mapcolors::adjustPenForManual(&painter);

        painter.drawPolyline(altLegs.at(i));
      }
    }
    else
      qWarning() << Q_FUNC_INFO;

    if(activeRouteLeg > 0 && activeRouteLeg < route.size())
    {
      // Draw active  ======================================================
      painter.setPen(QPen(optData.getFlightplanActiveSegmentColor(), flightplanWidth,
                          Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      const proc::MapProcedureLeg& actProcLeg = route.value(activeRouteLeg).getProcedureLeg();
      if(actProcLeg.isCircleToLand() || actProcLeg.isStraightIn())
        mapcolors::adjustPenForCircleToLand(&painter);
      else if(actProcLeg.isVectors())
        mapcolors::adjustPenForVectors(&painter);
      else if(actProcLeg.isManual())
        mapcolors::adjustPenForManual(&painter);

      painter.drawPolyline(altLegs.at(activeRouteLeg));
    }

    // =============================================================================
    // Draw flightplan symbols and labels ======================================================

    // Calculate symbol sizes
    float sizeScaleSymbol = 1.f;
    int waypointSize = atools::roundToInt((optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.) * 8.);
    int navaidSize = atools::roundToInt((optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.) * 12.);
    int airportSize = atools::roundToInt((optData.getDisplaySymbolSizeAirport() * sizeScaleSymbol / 100.) * 10.);

    painter.setBackgroundMode(Qt::TransparentMode);
    mapcolors::scaleFont(&painter, optData.getDisplayTextSizeFlightplan() / 100.f, &painter.font());

    // Draw the most unimportant symbols and texts first - userpoints, invalid and procedure ============================
    int waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);

      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      QColor color;
      QStringList texts;
      bool procSymbol = false;
      textsAndColorForLeg(texts, color, procSymbol, leg);

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).last());
      if(!procSymbol)
      {
        // Draw all except airport, waypoint, VOR and NDB
        map::MapTypes type = leg.getMapObjectType();
        if(type == map::AIRPORT || leg.getAirport().isValid() ||
           type == map::WAYPOINT || leg.getWaypoint().isValid() ||
           type == map::VOR || leg.getVor().isValid() ||
           type == map::NDB || leg.getNdb().isValid())
          continue;
        else if(type == map::USERPOINTROUTE)
          symPainter.drawUserpointSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true);
        else if(type == map::INVALID)
          symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symPt.x(), symPt.y(), 9, true);
        else if(type == map::PROCEDURE)
          // Missed is not included
          symPainter.drawProcedureSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true);
      }
      else
        symPainter.drawProcedureSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true);

      if(routeIndex >= activeRouteLeg - 1)
      {
        // Procedure symbols ========================================================
        if(leg.isAnyProcedure())
          symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6,
                                           leg.getProcedureLeg().flyover, leg.getProcedureLeg().malteseCross);

        // Labels ========================
        symPainter.textBox(&painter, texts, color, symPt.x() + 5,
                           std::min(symPt.y() + 14, h), textatt::ROUTE_BG_COLOR, 255);
      }
    }

    // Draw waypoints ============================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);

      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      QColor color;
      QStringList texts;
      bool procSymbol = false;
      textsAndColorForLeg(texts, color, procSymbol, leg);

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).last());
      if(!procSymbol)
      {
        // Draw all except airport, VOR, NDB and userpoint
        map::MapTypes type = leg.getMapObjectType();
        if(type == map::AIRPORT || leg.getAirport().isValid() ||
           type == map::VOR || leg.getVor().isValid() ||
           type == map::NDB || leg.getNdb().isValid() ||
           type == map::USERPOINTROUTE || type == map::INVALID)
          continue;
        else if(type == map::WAYPOINT || leg.getWaypoint().isValid())
          symPainter.drawWaypointSymbol(&painter, QColor(), symPt.x(), symPt.y(), waypointSize, true);
      }
      // else procedure symbols drawn before

      // Labels ========================
      if(routeIndex >= activeRouteLeg - 1)
      {
        // Procedure symbols ========================================================
        if(leg.isAnyProcedure())
          symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6,
                                           leg.getProcedureLeg().flyover, leg.getProcedureLeg().malteseCross);

        symPainter.textBox(&painter, texts, color, symPt.x() + 5,
                           std::min(symPt.y() + 14, h), textatt::ROUTE_BG_COLOR, 255);
      }
    }

    // Draw the more important radio navaids =======================================================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);
      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      QColor color;
      QStringList texts;
      bool procSymbol = false;
      textsAndColorForLeg(texts, color, procSymbol, leg);

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).last());
      if(!procSymbol)
      {
        // Draw all except airport, waypoint and userpoint
        map::MapTypes type = leg.getMapObjectType();
        if(type == map::AIRPORT || leg.getAirport().isValid() ||
           type == map::WAYPOINT || leg.getWaypoint().isValid() ||
           type == map::USERPOINTROUTE || type == map::INVALID)
          continue;
        else if(type == map::NDB || leg.getNdb().isValid())
          symPainter.drawNdbSymbol(&painter, symPt.x(), symPt.y(), navaidSize, true, false);
        else if(type == map::VOR || leg.getVor().isValid())
          symPainter.drawVorSymbol(&painter, leg.getVor(), symPt.x(), symPt.y(), navaidSize, true, false, false);
      }

      // Labels ========================
      if(routeIndex >= activeRouteLeg - 1)
      {
        // Procedure symbols ========================================================
        if(leg.isAnyProcedure())
          symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6,
                                           leg.getProcedureLeg().flyover, leg.getProcedureLeg().malteseCross);
        symPainter.textBox(&painter, texts, color, symPt.x() + 5,
                           std::min(symPt.y() + 14, h), textatt::ROUTE_BG_COLOR, 255);
      }
    }

    // Draw the most important airport symbols on top ============================================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);
      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;
      QPoint symPt(altLegs.at(waypointIndex).last());

      // Draw all airport except destination and departure
      if(leg.getMapObjectType() == map::AIRPORT && routeIndex > 0 && routeIndex < route.getDestinationAirportLegIndex())
      {
        symPainter.drawAirportSymbol(&painter, leg.getAirport(), symPt.x(), symPt.y(), airportSize, false, false,
                                     false);

        // Labels ========================
        if(routeIndex >= activeRouteLeg - 1)
          symPainter.drawAirportText(&painter, leg.getAirport(), symPt.x() - 5, std::min(symPt.y() + 14, h),
                                     optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }
    }

    if(!route.isEmpty())
    {
      // Draw departure always on the left also if there are departure procedures
      const RouteLeg& departureLeg = route.getDepartureAirportLeg();
      if(departureLeg.getMapObjectType() == map::AIRPORT)
      {
        int textW = painter.fontMetrics().width(departureLeg.getDisplayIdent());
        symPainter.drawAirportSymbol(&painter,
                                     departureLeg.getAirport(), left, flightplanY, airportSize, false, false, false);
        symPainter.drawAirportText(&painter, departureLeg.getAirport(), left - textW / 2, flightplanTextY,
                                   optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }

      // Draw destination always on the right also if there are approach procedures
      const RouteLeg& destinationLeg = route.getDestinationAirportLeg();
      if(destinationLeg.getMapObjectType() == map::AIRPORT)
      {
        int textW = painter.fontMetrics().width(destinationLeg.getDisplayIdent());
        symPainter.drawAirportSymbol(&painter, destinationLeg.getAirport(), left + w, flightplanY, airportSize, false,
                                     false, false);
        symPainter.drawAirportText(&painter, destinationLeg.getAirport(), left + w - textW / 2, flightplanTextY,
                                   optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }
    }

    if(!route.isFlightplanEmpty())
    {
      if(NavApp::getMapWidget()->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN_TOC_TOD))
      {
        float tocDist = altitudeLegs.getTopOfClimbDistance();
        float todDist = altitudeLegs.getTopOfDescentDistance();
        float width = optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.f * 3.f;
        int radius = atools::roundToInt(optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100. * 6.);

        painter.setBackgroundMode(Qt::TransparentMode);
        painter.setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
        painter.setBrush(Qt::NoBrush);

        if(!(OptionData::instance().getFlags2() & opts2::MAP_ROUTE_DIM_PASSED) ||
           activeRouteLeg == map::INVALID_INDEX_VALUE || route.getTopOfClimbLegIndex() > activeRouteLeg - 1)
        {
          if(tocDist > 0.2f)
          {
            int tocX = distanceX(altitudeLegs.getTopOfClimbDistance());
            if(tocX < map::INVALID_INDEX_VALUE)
            {
              // Draw the top of climb point and text =========================================================
              painter.drawEllipse(QPoint(tocX, flightplanY), radius, radius);

              QStringList txt;
              txt.append(tr("TOC"));
              txt.append(Unit::distNm(route.getTopOfClimbDistance()));

              symPainter.textBox(&painter, txt, QPen(Qt::black),
                                 tocX + 8, flightplanY + 8,
                                 textatt::ROUTE_BG_COLOR, 255);
            }
          }
        }

        if(!(OptionData::instance().getFlags2() & opts2::MAP_ROUTE_DIM_PASSED) ||
           activeRouteLeg == map::INVALID_INDEX_VALUE || route.getTopOfDescentLegIndex() > activeRouteLeg - 1)
        {
          if(todDist < route.getTotalDistance() - 0.2f)
          {
            int todX = distanceX(altitudeLegs.getTopOfDescentDistance());
            if(todX < map::INVALID_INDEX_VALUE)
            {
              // Draw the top of descent point and text =========================================================
              painter.drawEllipse(QPoint(todX, flightplanY), radius, radius);

              QStringList txt;
              txt.append(tr("TOD"));
              txt.append(Unit::distNm(route.getTopOfDescentFromDestination()));

              symPainter.textBox(&painter, txt, QPen(Qt::black),
                                 todX + 8, flightplanY + 8,
                                 textatt::ROUTE_BG_COLOR, 255);
            }
          }
        }
      }
    } // if(NavApp::getMapWidget()->getShownMapFeatures() & map::FLIGHTPLAN)

    // Departure altitude label =========================================================
    QColor labelColor = mapcolors::profileLabelColor;
    float departureAlt = legList->route.getDepartureAirportLeg().getPosition().getAltitude();
    int departureAltTextY = TOP + atools::roundToInt(h - departureAlt * verticalScale);
    departureAltTextY = std::min(departureAltTextY, TOP + h - painter.fontMetrics().height() / 2);
    QString startAltStr = Unit::altFeet(departureAlt);
    symPainter.textBox(&painter, {startAltStr}, labelColor, left - 4, departureAltTextY,
                       textatt::BOLD | textatt::RIGHT, 255);

    // Destination altitude label =========================================================
    float destAlt = route.getDestinationAirportLeg().getPosition().getAltitude();
    int destinationAltTextY = TOP + static_cast<int>(h - destAlt * verticalScale);
    destinationAltTextY = std::min(destinationAltTextY, TOP + h - painter.fontMetrics().height() / 2);
    QString destAltStr = Unit::altFeet(destAlt);
    symPainter.textBox(&painter, {destAltStr}, labelColor, left + w + 4, destinationAltTextY,
                       textatt::BOLD | textatt::LEFT, 255);
  } // if(NavApp::getMapWidget()->getShownMapFeatures() & map::FLIGHTPLAN)

  // Draw user aircraft track =========================================================
  if(!aircraftTrackPoints.isEmpty() && showAircraftTrack)
  {
    painter.setPen(mapcolors::aircraftTrailPen(optData.getDisplayThicknessTrail() / 100.f * 2.f));
    painter.drawPolyline(toScreen(aircraftTrackPoints));
  }

  // Draw user aircraft =========================================================
  if(simData.getUserAircraftConst().isValid() && showAircraft &&
     aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE && !curRoute.isActiveMissed() &&
     !curRoute.isActiveAlternate())
  {
    float acx = distanceX(aircraftDistanceFromStart);
    float acy = altitudeY(aircraftAlt(simData.getUserAircraftConst()));

    // Draw aircraft symbol
    int acsize = atools::roundToInt(optData.getDisplaySymbolSizeAircraftUser() / 100. * 32.);
    painter.translate(acx, acy);
    painter.rotate(90);
    painter.scale(0.75, 1.);
    painter.shear(0.0, 0.5);

    // Turn aircraft if distance shrinks
    if(movingBackwards)
      // Reflection is a special case of scaling matrix
      painter.scale(1., -1.);

    const QPixmap *pixmap = NavApp::getVehicleIcons()->pixmapFromCache(simData.getUserAircraftConst(), acsize, 0);
    painter.drawPixmap(QPointF(-acsize / 2., -acsize / 2.), *pixmap);
    painter.resetTransform();

    // Draw aircraft label
    mapcolors::scaleFont(&painter, optData.getDisplayTextSizeAircraftUser() / 100.f, &painter.font());

    int vspeed = atools::roundToInt(simData.getUserAircraftConst().getVerticalSpeedFeetPerMin());
    QString upDown;
    if(vspeed > 100.f)
      upDown = tr(" ▲");
    else if(vspeed < -100.f)
      upDown = tr(" ▼");

    QStringList texts;
    texts.append(Unit::altFeet(aircraftAlt(simData.getUserAircraftConst())));

    if(vspeed > 10.f || vspeed < -10.f)
      texts.append(Unit::speedVertFpm(vspeed) + upDown);

    textatt::TextAttributes att = textatt::NONE;
    float textx = acx, texty = acy + 20.f;

    QRect rect = symPainter.textBoxSize(&painter, texts, att);
    if(textx + rect.right() > left + w)
      // Move text to the left when approaching the right corner
      att |= textatt::RIGHT;

    att |= textatt::ROUTE_BG_COLOR;

    if(acy - rect.height() > scrollArea->getOffset().y() + TOP)
      texty -= rect.bottom() + 20.f; // Text at top

    symPainter.textBoxF(&painter, texts, QPen(Qt::black), textx, texty, att, 255);
  }

  // Dim the map by drawing a semi-transparent black rectangle
  mapcolors::darkenPainterRect(painter);

  scrollArea->updateLabelWidget();
}

void ProfileWidget::textsAndColorForLeg(QStringList& texts, QColor& color, bool& procSymbol, const RouteLeg& leg)
{
  map::MapTypes type = leg.getMapObjectType();
  QString ident;
  color = mapcolors::routeUserPointColor;

  if(type == map::WAYPOINT || leg.getWaypoint().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::waypointSymbolColor;
  }
  else if(type == map::VOR || leg.getVor().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::vorSymbolColor;
  }
  else if(type == map::NDB || leg.getNdb().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::ndbSymbolColor;
  }
  else if(type == map::USERPOINTROUTE)
  {
    ident = leg.getIdent();
    color = mapcolors::routeUserPointColor;
  }
  else if(type == map::INVALID)
    ident = leg.getIdent();
  else if(type == map::PROCEDURE && !leg.getProcedureLeg().fixIdent.isEmpty())
    // Custom approach
    ident = leg.getIdent();

  if(!leg.isAnyProcedure() ||
     (!leg.getProcedureLeg().noIdentDisplay() && proc::procedureLegDrawIdent(leg.getProcedureLegType())))
    texts.append(ident);

  if(leg.isAnyProcedure())
  {
    if(leg.getProcedureLeg().noIdentDisplay() || !proc::procedureLegDrawIdent(leg.getProcedureLegType()))
      color = mapcolors::routeUserPointColor;

    procSymbol = !proc::procedureLegDrawIdent(leg.getProcedureLegType());
  }
  else
    procSymbol = false;

  if(leg.getProcedureLegType() != proc::START_OF_PROCEDURE)
    texts.append(leg.getProcedureLeg().displayText);
  texts.removeAll(QString());
  texts = atools::elideTextShort(texts, 15);
}

/* Update signal from Marble elevation model */
void ProfileWidget::elevationUpdateAvailable()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  // Do not terminate thread here since this can lead to starving updates

  // Start thread after long delay to calculate new data
  // Calls ProfileWidget::updateTimeout()
  updateTimer->start(NavApp::getElevationProvider()->isGlobeOfflineProvider() ?
                     ELEVATION_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ELEVATION_CHANGE_ONLINE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::routeAltitudeChanged(int altitudeFeet)
{
  Q_UNUSED(altitudeFeet)

  if(!widgetVisible || databaseLoadStatus)
    return;

  routeChanged(true, false);
}

void ProfileWidget::aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *)
{
  routeChanged(true, false);
}

void ProfileWidget::routeChanged(bool geometryChanged, bool newFlightPlan)
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  scrollArea->routeChanged(geometryChanged);

  if(newFlightPlan)
    scrollArea->expandWidget();

  if(geometryChanged)
  {
    // Start thread after short delay to calculate new data
    // Calls ProfileWidget::updateTimeout()
    updateTimer->start(NavApp::getElevationProvider()->isGlobeOfflineProvider() ?
                       ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
  }
  else
    update();

  updateErrorLabel();
  updateLabel();
}

/* Called by updateTimer after any route or elevation updates and starts the thread */
void ProfileWidget::updateTimeout()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  // Terminate and wait for thread
  terminateThread();
  terminateThreadSignal = false;

  // Need a copy of the leg list before starting thread to avoid synchronization problems
  // Start the computation in background
  ElevationLegList legs;
  legs.route = NavApp::getRoute();

  // Start thread
  future = QtConcurrent::run(this, &ProfileWidget::fetchRouteElevationsThread, legs);

  // Watcher will call ProfileWidget::updateThreadFinished() when finished
  watcher.setFuture(future);
}

/* Called by watcher when the thread is finished */
void ProfileWidget::updateThreadFinished()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  if(!terminateThreadSignal)
  {
    // Was not terminated in the middle of calculations - get result from the future
    *legList = future.result();
    updateScreenCoords();
    updateErrorLabel();
    updateLabel();
    update();
    updateTooltip();

    // Update scroll bars
    scrollArea->routeChanged(true);
  }
}

/* Get elevation points between the two points. This returns also correct results if the antimeridian is crossed
 * @return true if not aborted */
bool ProfileWidget::fetchRouteElevations(atools::geo::LineString& elevations,
                                         const atools::geo::LineString& geometry) const
{
  ElevationProvider *elevationProvider = NavApp::getElevationProvider();
  for(int i = 0; i < geometry.size() - 1; i++)
  {
    // Create a line string from the two points and split it at the date line if crossing
    GeoDataLineString coords;
    coords.setTessellate(true);
    coords << GeoDataCoordinates(geometry.at(i).getLonX(), geometry.at(i).getLatY(),
                                 0., GeoDataCoordinates::Degree)
           << GeoDataCoordinates(geometry.at(i + 1).getLonX(), geometry.at(i + 1).getLatY(),
                          0., GeoDataCoordinates::Degree);

    QVector<Marble::GeoDataLineString *> coordsCorrected = coords.toDateLineCorrected();
    for(const Marble::GeoDataLineString *ls : coordsCorrected)
    {
      for(int j = 1; j < ls->size(); j++)
      {
        if(terminateThreadSignal)
          return false;

        const Marble::GeoDataCoordinates& c1 = ls->at(j - 1);
        const Marble::GeoDataCoordinates& c2 = ls->at(j);
        Pos p1(c1.longitude(), c1.latitude());
        Pos p2(c2.longitude(), c2.latitude());

        p1.toDeg();
        p2.toDeg();
        elevationProvider->getElevations(elevations, atools::geo::Line(p1, p2));
      }
    }
    qDeleteAll(coordsCorrected);
  }

  if(!elevations.isEmpty())
  {
    // Add start or end point if heightProfile omitted these - check only lat lon not alt
    if(!elevations.first().almostEqual(geometry.first()))
      elevations.prepend(Pos(geometry.first().getLonX(), geometry.first().getLatY(), elevations.first().getAltitude()));

    if(!elevations.last().almostEqual(geometry.last()))
      elevations.append(Pos(geometry.last().getLonX(), geometry.last().getLatY(), elevations.last().getAltitude()));
  }

  return true;
}

/* Background thread. Fetches elevation points from Marble elevation model and updates totals. */
ElevationLegList ProfileWidget::fetchRouteElevationsThread(ElevationLegList legs) const
{
  QThread::currentThread()->setPriority(QThread::LowestPriority);
  // qDebug() << "priority" << QThread::currentThread()->priority();

  using atools::geo::meterToNm;
  using atools::geo::nmToMeter;
  using atools::geo::meterToFeet;

  legs.totalNumPoints = 0;
  legs.totalDistance = 0.f;
  legs.maxElevationFt = 0.f;
  legs.elevationLegs.clear();

  if(legs.route.getSizeWithoutAlternates() <= 1)
    // Return empty result
    return ElevationLegList();

  if(legs.route.getAltitudeLegs().isEmpty())
    // Return empty result
    return ElevationLegList();

  // Total calculated distance across all legs
  double totalDistanceMeter = 0.;

  // Loop over all route legs - first is departure airport point
  for(int i = 1; i <= legs.route.getDestinationLegIndex(); i++)
  {
    if(terminateThreadSignal)
      // Return empty result
      return ElevationLegList();

    const RouteAltitudeLeg& altLeg = legs.route.getAltitudeLegAt(i);
    if(altLeg.isMissed() || altLeg.isAlternate())
      break;

    ElevationLeg leg;

    // Used to adapt distances of all legs to total distance due to inaccuracies
    double scale = 1.;

    // Skip for too long segments when using the marble online provider
    if(altLeg.getDistanceTo() < ELEVATION_MAX_LEG_NM || NavApp::getElevationProvider()->isGlobeOfflineProvider())
    {
      LineString geometry = altLeg.getGeoLineString();

      geometry.removeInvalid();
      if(geometry.size() == 1)
        geometry.append(geometry.first());

      // Includes first and last point
      LineString elevations;
      if(!fetchRouteElevations(elevations, geometry))
        return ElevationLegList();

      leg.geometry = geometry;

      double distMeter = totalDistanceMeter;
      // Loop over all elevation points for the current leg
      Pos lastPos;
      for(int j = 0; j < elevations.size(); j++)
      {
        if(terminateThreadSignal)
          return ElevationLegList();

        Pos& coord = elevations[j];
        float altFeet = meterToFeet(coord.getAltitude());
        coord.setAltitude(altFeet);

        // Adjust maximum
        if(altFeet > leg.maxElevation)
          leg.maxElevation = altFeet;
        if(altFeet > legs.maxElevationFt)
          legs.maxElevationFt = altFeet;

        if(j > 0)
          // Update total distance
          distMeter += lastPos.distanceMeterToDouble(coord);

        // Coordinate with elevation
        leg.elevation.append(coord);

        // Distance to elevation point from departure
        leg.distances.append(distMeter);

        legs.totalNumPoints++;
        lastPos = coord;
      }

      totalDistanceMeter += nmToMeter(altLeg.getDistanceTo());

      if(!leg.distances.isEmpty() && atools::almostNotEqual(leg.distances.last(), totalDistanceMeter))
        // Accumulated distance is different from route total distance - adjust
        // This can happen with the online elevation provider which does not return all points exactly on the leg
        scale = totalDistanceMeter / leg.distances.last();
    }
    else
    {
      // Skip long segment
      leg.distances.append(totalDistanceMeter);
      totalDistanceMeter += altLeg.getGeoLineString().lengthMeter();
      leg.distances.append(totalDistanceMeter);
      leg.elevation = altLeg.getGeoLineString();
      leg.elevation.setAltitude(0.f);
      leg.geometry = altLeg.getGeoLineString();
    }

    // Convert distances to NM and apply correction
    for(int j = 0; j < leg.distances.size(); j++)
      leg.distances[j] = meterToNm(leg.distances.at(j) * scale);

    legs.elevationLegs.append(leg);
  }

  legs.totalDistance = static_cast<float>(meterToNm(totalDistanceMeter));
  return legs;
}

void ProfileWidget::showEvent(QShowEvent *)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(size().isNull())
    resize(ui->scrollAreaProfile->viewport()->size());

  widgetVisible = true;
  // Start update immediately
  // Calls ProfileWidget::updateTimeout()
  updateTimer->start(ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::hideEvent(QHideEvent *)
{
  // Stop all updates
  widgetVisible = false;
  updateTimer->stop();
  terminateThread();
}

atools::geo::Pos ProfileWidget::calculatePos(int x)
{
  atools::geo::Pos pos;
  int index;
  float distance, distanceToGo, alt, maxElev;
  calculateDistancesAndPos(x, pos, index, distance, distanceToGo, alt, maxElev);
  return pos;
}

void ProfileWidget::calculateDistancesAndPos(int x, atools::geo::Pos& pos, int& routeIndex, float& distance,
                                             float& distanceToGo, float& groundElevation, float& maxElev)
{
  // Get index for leg
  routeIndex = 0;

  if(x > waypointX.last())
    x = waypointX.last();

  QVector<int>::iterator it = std::lower_bound(waypointX.begin(), waypointX.end(), x);
  if(it != waypointX.end())
  {
    routeIndex = static_cast<int>(std::distance(waypointX.begin(), it)) - 1;
    if(routeIndex < 0)
      routeIndex = 0;
  }
  const ElevationLeg& leg = legList->elevationLegs.at(routeIndex);

  // Calculate distance from screen coordinates
  distance = (x - left) / horizontalScale;
  distance = atools::minmax(0.f, legList->totalDistance, distance);
  distanceToGo = legList->totalDistance - distance;

  // Get distance value index for lower and upper bound at cursor position
  int indexLowDist = 0;
  QVector<double>::const_iterator lowDistIt = std::lower_bound(leg.distances.begin(),
                                                               leg.distances.end(), distance);
  if(lowDistIt != leg.distances.end())
  {
    indexLowDist = static_cast<int>(std::distance(leg.distances.begin(), lowDistIt));
    if(indexLowDist < 0)
      indexLowDist = 0;
  }
  int indexUpperDist = 0;
  QVector<double>::const_iterator upperDistIt = std::upper_bound(leg.distances.begin(),
                                                                 leg.distances.end(), distance);
  if(upperDistIt != leg.distances.end())
  {
    indexUpperDist = static_cast<int>(std::distance(leg.distances.begin(), upperDistIt));
    if(indexUpperDist < 0)
      indexUpperDist = 0;
  }

  // Get altitude values before and after cursor and interpolate - gives 0 if index is not valid
  float alt1 = leg.elevation.value(indexLowDist, atools::geo::EMPTY_POS).getAltitude();
  float alt2 = leg.elevation.value(indexUpperDist, atools::geo::EMPTY_POS).getAltitude();
  groundElevation = (alt1 + alt2) / 2.f;

  // Calculate min altitude for this leg
  maxElev = calcGroundBuffer(leg.maxElevation);

  // Get Position for highlight on map
  float legdistpart = distance - static_cast<float>(leg.distances.first());
  float legdist = static_cast<float>(leg.distances.last() - leg.distances.first());

  // Calculate position along the flight plan
  pos = leg.geometry.interpolate(legdistpart / legdist);
}

void ProfileWidget::mouseMoveEvent(QMouseEvent *mouseEvent)
{
  if(!widgetVisible || legList->elevationLegs.isEmpty() || legList->route.isEmpty())
    return;

  if(rubberBand == nullptr)
    // Create rubber band line
    rubberBand = new QRubberBand(QRubberBand::Line, this);

  // Limit x to drawing region
  int x = mouseEvent->pos().x();
  x = std::max(x, left);
  x = std::min(x, rect().width() - left);

  rubberBand->setGeometry(x - 1, 0, 2, rect().height());
  rubberBand->show();

  buildTooltip(x, false /* force */);
  lastTooltipScreenPos = mouseEvent->globalPos();
  // Show tooltip
  if(!lastTooltipString.isEmpty())
    scrollArea->showTooltip(lastTooltipScreenPos, lastTooltipString);

  // Allow event to propagate to scroll widget
  mouseEvent->ignore();

  // Follow cursor on map if enabled ==========================
  if(NavApp::getMainUi()->actionProfileFollow->isChecked())
  {
    atools::geo::Pos pos = calculatePos(x);
    if(pos.isValid())
      emit showPos(pos, map::INVALID_DISTANCE_VALUE, false);
  }

  // Tell map widget to create a rubberband rectangle on the map
  emit highlightProfilePoint(lastTooltipPos);
}

void ProfileWidget::updateTooltip()
{
  if(scrollArea->isTooltipVisible())
  {
    buildTooltip(lastTooltipX, true /* force */);
    if(!lastTooltipString.isEmpty())
      scrollArea->showTooltip(lastTooltipScreenPos, lastTooltipString);
  }
}

void ProfileWidget::buildTooltip(int x, bool force)
{
  // Nothing to show label =========================
  if(!hasValidRouteForDisplay())
    return;

  if(!NavApp::getMainUi()->actionProfileShowTooltip->isChecked())
    return;

  if(atools::almostEqual(lastTooltipX, x, 3) && !force)
    // Almost same position - do not update except if forced
    return;
  else
    lastTooltipX = x;

  // Get from/to text
  // QString fromWaypoint = atools::elideTextShort(  legList->route.value(index).getIdent(), 20);

  QString fromTo(tr("to"));

  // Fetch all parameters =======================
  int index;
  float distance, distanceToGo, groundElevation, maxElev;
  calculateDistancesAndPos(x, lastTooltipPos, index, distance, distanceToGo, groundElevation, maxElev);

  const RouteLeg& routeLeg = legList->route.value(index + 1);
  if(routeLeg.isAnyProcedure() && proc::procedureLegFrom(routeLeg.getProcedureLegType()))
    fromTo = tr("from");

  QString toWaypoint = atools::elideTextShort(routeLeg.getDisplayIdent(), 20);

  // Create text for tooltip ==========================
  atools::util::HtmlBuilder html;

  // Header from and to distance, altitude and next waypoint ============
  float altitude = legList->route.getAltitudeForDistance(distanceToGo);
  const RouteLeg *leg = index < legList->route.size() - 1 ? &legList->route.value(index + 1) : nullptr;
  WindReporter *windReporter = NavApp::getWindReporter();
  atools::grib::Wind wind;
  if((windReporter->hasOnlineWindData() || windReporter->isWindManual()) && leg != nullptr)
    wind = windReporter->getWindForPosRoute(lastTooltipPos.alt(altitude));

  html.p(atools::util::html::NOBR_WHITESPACE);
  html.b(Unit::distNm(distance, false) + tr(" ► ") + Unit::distNm(distanceToGo));
  if(altitude < map::INVALID_ALTITUDE_VALUE)
    html.b(tr(", %1, %2 %3").arg(Unit::altFeet(altitude)).arg(fromTo).arg(toWaypoint));

  // Course ========================================
  if(routeLeg.getCourseToMag() < map::INVALID_COURSE_VALUE)
  {
    html.br().b(tr("Course: ")).
    text(formatter::courseTextFromMag(routeLeg.getCourseToMag(), routeLeg.getMagvar(), false /* magBold */),
         atools::util::html::NO_ENTITIES);

    // Crab angle / heading ========================================
    if(wind.isValid() && !wind.isNull())
    {
      float tas = legList->route.getSpeedForDistance(distanceToGo);
      if(tas < map::INVALID_SPEED_VALUE)
      {
#ifdef DEBUG_INFORMATION
        html.brText(QString("[TAS %1 kts, %2°T %3 kts]").
                    arg(tas, 0, 'f', 0).arg(wind.dir, 0, 'f', 0).arg(wind.speed, 0, 'f', 0));
#endif

        float headingTrue = atools::geo::windCorrectedHeading(wind.speed, wind.dir, routeLeg.getCourseToTrue(), tas);
        if(headingTrue < map::INVALID_COURSE_VALUE &&
           atools::almostNotEqual(headingTrue, routeLeg.getCourseToTrue(), 1.f))
          html.br().b(tr("Heading: ")).
          text(formatter::courseTextFromTrue(headingTrue, routeLeg.getMagvar()),
               atools::util::html::NO_ENTITIES);
      }
    }
  }

  // Ground ===============================================================
  QStringList groundText;
  if(groundElevation < map::INVALID_ALTITUDE_VALUE)
    groundText.append(Unit::altFeet(groundElevation));

  if(altitude < map::INVALID_ALTITUDE_VALUE && groundElevation < map::INVALID_ALTITUDE_VALUE)
  {
    float aboveGround = map::INVALID_ALTITUDE_VALUE;
    aboveGround = std::max(0.f, altitude - groundElevation);
    if(aboveGround < map::INVALID_ALTITUDE_VALUE)
      groundText.append(tr("%1 above").arg(Unit::altFeet(aboveGround)));
  }

  if(!groundText.isEmpty())
    html.br().b(tr("Ground: ")).text(atools::strJoin(groundText, tr(", ")));

  // Safe altitude ===============================================================
  if(maxElev < map::INVALID_ALTITUDE_VALUE)
    html.br().b(tr("Leg Safe Altitude: ")).text(Unit::altFeet(maxElev));

  // Show wind at altitude ===============================================
  if(wind.isValid() && !wind.isNull())
  {
    float headWind = 0.f, crossWind = 0.f;
    atools::geo::windForCourse(headWind, crossWind, wind.speed, wind.dir, leg->getCourseToTrue());

    float magVar = NavApp::getMagVar(lastTooltipPos);

    QString windText = tr("%1°M, %2").
                       arg(atools::geo::normalizeCourse(wind.dir - magVar), 0, 'f', 0).
                       arg(Unit::speedKts(wind.speed));
    html.br().b(tr("%1Wind: ").arg(windReporter->isWindManual() ? tr("Manual ") : QString())).text(windText);

    // Add tail/headwind if sufficient =======================================
    if(std::abs(headWind) >= 1.f)
    {
      QString windPtr;
      if(headWind >= 1.f)
        windPtr = tr("▼");
      else if(headWind <= -1.f)
        windPtr = tr("▲");
      html.text(tr(", %1 %2").arg(windPtr).arg(Unit::speedKts(std::abs(headWind))));
    }
  }

#ifdef DEBUG_INFORMATION_PROFILE
  using namespace formatter;
  using namespace map;

  FuelTimeResult result;
  NavApp::getAircraftPerfController()->calculateFuelAndTimeTo(result, distanceToGo, INVALID_DISTANCE_VALUE,
                                                              index + 1);

  variableLabelText.append(QString("<br/><code>[alt %1,idx %2, crs %3, "
                                     "fuel dest %4/%5, fuel TOD %6/%7, "
                                     "time dest %8 (%9), time TOD %10 (%11)]</code>").
                           arg(NavApp::getRoute().getAltitudeForDistance(distanceToGo)).
                           arg(index).
                           arg(leg != nullptr ? QString::number(leg->getCourseToTrue()) : "-").
                           arg(result.fuelLbsToDest, 0, 'f', 2).
                           arg(result.fuelGalToDest, 0, 'f', 2).
                           arg(result.fuelLbsToTod < INVALID_WEIGHT_VALUE ? result.fuelLbsToTod : -1., 0, 'f', 2).
                           arg(result.fuelGalToTod < INVALID_VOLUME_VALUE ? result.fuelGalToTod : -1., 0, 'f', 2).
                           arg(result.timeToDest, 0, 'f', 2).
                           arg(formatMinutesHours(result.timeToDest)).
                           arg(result.timeToTod < map::INVALID_TIME_VALUE ? result.timeToTod : -1., 0, 'f', 2).
                           arg(result.timeToTod < INVALID_TIME_VALUE ? formatMinutesHours(result.timeToTod) : "-1"));
#endif
  html.pEnd(); // html.p(atools::util::html::NOBR_WHITESPACE);
  lastTooltipString = html.getHtml();
}

void ProfileWidget::contextMenuEvent(QContextMenuEvent *event)
{
  QPoint globalpoint;
  if(event->reason() == QContextMenuEvent::Keyboard)
    // Event does not contain position is triggered by keyboard
    globalpoint = QCursor::pos();
  else
    globalpoint = event->globalPos();

  showContextMenu(globalpoint);
}

void ProfileWidget::showContextMenu(const QPoint& globalPoint)
{
  qDebug() << Q_FUNC_INFO << globalPoint;
  contextMenuActive = true;

  // Position on the whole map widget
  QPoint mapPoint = mapFromGlobal(globalPoint);

  // Local visible widget position
  QPoint pointArea = scrollArea->getScrollArea()->mapFromGlobal(globalPoint);
  QRect rectArea = scrollArea->getScrollArea()->rect();

  // Local visible widget position
  QPoint pointLabel = scrollArea->getLabelWidget()->mapFromGlobal(globalPoint);
  QRect rectLabel = scrollArea->getLabelWidget()->rect();

  bool hasPosition = mapPoint.x() > left && mapPoint.x() < width() - left && rectArea.contains(pointArea);

  // Do not show context menu if point is neither on the visible widget and not on the label
  QPoint menuPos = globalPoint;
  if(!rectArea.contains(pointArea) && !rectLabel.contains(pointLabel))
    menuPos = scrollArea->getScrollArea()->mapToGlobal(rectArea.center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionProfileShowOnMap->setEnabled(hasPosition && hasValidRouteForDisplay());
  ui->actionProfileDeleteAircraftTrack->setEnabled(hasTrackPoints());

  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  menu.addAction(ui->actionProfileShowOnMap);
  menu.addSeparator();
  menu.addAction(ui->actionProfileExpand);
  menu.addSeparator();
  menu.addAction(ui->actionProfileCenterAircraft);
  menu.addAction(ui->actionProfileDeleteAircraftTrack);
  menu.addSeparator();
  menu.addAction(ui->actionProfileShowVasi);
  menu.addAction(ui->actionProfileShowIls);
  menu.addAction(ui->actionMapShowTocTod);
  menu.addSeparator();
  menu.addAction(ui->actionProfileFollow);
  menu.addSeparator();
  menu.addAction(ui->actionProfileShowTooltip);
  menu.addAction(ui->actionProfileShowZoom);
  menu.addAction(ui->actionProfileShowLabels);
  menu.addAction(ui->actionProfileShowScrollbars);

  QAction *action = menu.exec(menuPos);

  if(action == ui->actionProfileShowOnMap)
  {
    atools::geo::Pos pos = calculatePos(mapPoint.x());
    if(pos.isValid())
      emit showPos(pos, 0.f, false);
  }
  else if(action == ui->actionProfileCenterAircraft || action == ui->actionProfileFollow)
    scrollArea->update();
  else if(action == ui->actionProfileShowIls || action == ui->actionProfileShowVasi ||
          action == ui->actionMapShowTocTod)
    update();
  else if(action == ui->actionProfileDeleteAircraftTrack)
    deleteAircraftTrack();

  // Other actions are connected to methods or used during updates
  // else if(action == ui->actionProfileFit)
  // else if(action == ui->actionProfileExpand)
  // menu.addAction(ui->actionProfileShowZoom);
  // else if(action == ui->actionProfileShowLabels)
  // else if(action == ui->actionProfileShowScrollbars)

  contextMenuActive = false;
}

void ProfileWidget::updateLabel()
{
  float distFromStartNm = 0.f, distToDestNm = 0.f, nearestLegDistance = 0.f;
  const Route& curRoute = NavApp::getRoute();
  if(simData.getUserAircraftConst().isValid())
  {
    if(curRoute.getRouteDistances(&distFromStartNm, &distToDestNm, &nearestLegDistance))
    {
      if(curRoute.isActiveMissed())
        distToDestNm = 0.f;

      if(curRoute.isActiveAlternate())
        // Use distance to alternate instead of destination
        fixedLabelText = tr("<b>Alternate: %1.</b>&nbsp;&nbsp;").arg(Unit::distNm(nearestLegDistance));
      else
      {
        if(NavApp::getMapWidget()->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN_TOC_TOD) &&
           curRoute.getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE)
        {
          // Fuel and time calculated or estimated
          FuelTimeResult fuelTime;
          NavApp::getAircraftPerfController()->calculateFuelAndTimeTo(fuelTime, distToDestNm, nearestLegDistance,
                                                                      curRoute.getActiveLegIndex());

          float toTod = curRoute.getTopOfDescentDistance() - distFromStartNm;

          fixedLabelText = tr("<b>Destination: %1 (%2). Top of Descent: %3%4.</b>&nbsp;&nbsp;").
                           arg(Unit::distNm(distToDestNm)).
                           arg(formatter::formatMinutesHoursLong(fuelTime.timeToDest)).
                           arg(toTod > 0.f ? Unit::distNm(toTod) : tr("Passed")).
                           arg(toTod > 0.f ? tr(" (%1)").
                               arg(formatter::formatMinutesHoursLong(fuelTime.timeToTod)) : QString());
        }
        else
          fixedLabelText = tr("<b>Destination: %1.</b>&nbsp;&nbsp;").arg(Unit::distNm(distToDestNm));
      }
    }
    NavApp::getMainUi()->labelProfileInfo->setVisible(true);
  }
  else
  {
    NavApp::getMainUi()->labelProfileInfo->setVisible(false);
    fixedLabelText.clear();
  }

  NavApp::getMainUi()->labelProfileInfo->setText(fixedLabelText);
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  hideRubberBand();
  scrollArea->hideTooltip();
}

void ProfileWidget::hideRubberBand()
{
  if(!widgetVisible || legList->elevationLegs.isEmpty() || legList->route.isEmpty())
    return;

  delete rubberBand;
  rubberBand = nullptr;

  updateLabel();

  // Tell map widget to erase highlight
  emit highlightProfilePoint(atools::geo::EMPTY_POS);
}

/* Resizing needs an update of the screen coordinates */
void ProfileWidget::resizeEvent(QResizeEvent *)
{
  updateScreenCoords();
}

/* Deleting aircraft track needs an update of the screen coordinates */
void ProfileWidget::deleteAircraftTrack()
{
  aircraftTrackPoints.clear();

  updateScreenCoords();
  update();
}

/* Stop thread */
void ProfileWidget::preDatabaseLoad()
{
  jumpBack->cancel();
  updateTimer->stop();
  scrollArea->hideTooltip();
  terminateThread();
  databaseLoadStatus = true;
}

/* Force new thread creation */
void ProfileWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  scrollArea->hideTooltip();
  routeChanged(true /* geometry changed */, true /* new flight plan */);
}

void ProfileWidget::optionsChanged()
{
  jumpBack->cancel();
  scrollArea->hideTooltip();
  updateScreenCoords();
  updateErrorLabel();
  updateLabel();
  update();
}

void ProfileWidget::styleChanged()
{
  scrollArea->styleChanged();
}

void ProfileWidget::saveState()
{
  scrollArea->saveState();

  saveAircraftTrack();
}

void ProfileWidget::restoreState()
{
  scrollArea->restoreState();

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_TRAIL)
    loadAircraftTrack();
}

void ProfileWidget::preRouteCalc()
{
  updateTimer->stop();
  terminateThread();
}

void ProfileWidget::mainWindowShown()
{
  updateScreenCoords();
  scrollArea->routeChanged(true);
  scrollArea->expandWidget();
  update();
}

void ProfileWidget::updateProfileShowFeatures()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Compare own values with action values
  bool updateProfile = showAircraft != ui->actionMapShowAircraft->isChecked() ||
                       showAircraftTrack != ui->actionMapShowAircraftTrack->isChecked();

  showAircraft = ui->actionMapShowAircraft->isChecked();
  showAircraftTrack = ui->actionMapShowAircraftTrack->isChecked();

  if(updateProfile)
  {
    updateScreenCoords();
    update();
  }
}

void ProfileWidget::terminateThread()
{
  if(future.isRunning() || future.isStarted())
  {
    terminateThreadSignal = true;
    future.waitForFinished();
  }
}

void ProfileWidget::jumpBackToAircraftStart()
{
  if(NavApp::getMainUi()->actionProfileCenterAircraft->isChecked())
    jumpBack->start();
}

void ProfileWidget::jumpBackToAircraftCancel()
{
  jumpBack->cancel();
}

void ProfileWidget::jumpBackToAircraftTimeout()
{
  if(NavApp::getMainUi()->actionProfileCenterAircraft->isChecked() && NavApp::isConnectedAndAircraft() &&
     OptionData::instance().getFlags2() & opts2::ROUTE_NO_FOLLOW_ON_MOVE &&
     aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
  {
    if(QApplication::mouseButtons() & Qt::LeftButton || contextMenuActive)
      // Restart as long as menu is active or user is dragging around
      jumpBack->restart();
    else
    {
      jumpBack->cancel();

      QPoint pt = toScreen(QPointF(aircraftDistanceFromStart, aircraftAlt(simData.getUserAircraft())));
      bool centered = scrollArea->centerAircraft(pt, simData.getUserAircraft().getVerticalSpeedFeetPerMin());

      if(centered)
        NavApp::setStatusMessage(tr("Jumped back to aircraft."));
    }
  }
  else
    jumpBack->cancel();
}

void ProfileWidget::updateErrorLabel()
{
  NavApp::updateErrorLabels();
}

void ProfileWidget::saveAircraftTrack()
{
  QFile trackFile(atools::settings::Settings::getConfigFilename("_profile.track"));

  if(trackFile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&trackFile);
    out.setVersion(QDataStream::Qt_5_5);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << FILE_MAGIC_NUMBER << FILE_VERSION << aircraftTrackPoints;
    trackFile.close();
  }
  else
    qWarning() << "Cannot write track" << trackFile.fileName() << ":" << trackFile.errorString();
}

void ProfileWidget::loadAircraftTrack()
{
  QFile trackFile(atools::settings::Settings::getConfigFilename("_profile.track"));
  if(trackFile.exists())
  {
    if(trackFile.open(QIODevice::ReadOnly))
    {
      quint32 magic;
      quint16 version;
      QDataStream in(&trackFile);
      in.setVersion(QDataStream::Qt_5_5);
      in.setFloatingPointPrecision(QDataStream::SinglePrecision);
      in >> magic;

      if(magic == FILE_MAGIC_NUMBER)
      {
        in >> version;
        if(version == FILE_VERSION)
          in >> aircraftTrackPoints;
        else
          qWarning() << "Cannot read track" << trackFile.fileName() << ". Invalid version number:" << version;
      }
      else
        qWarning() << "Cannot read track" << trackFile.fileName() << ". Invalid magic number:" << magic;
      trackFile.close();
    }
    else
      qWarning() << "Cannot read track" << trackFile.fileName() << ":" << trackFile.errorString();
  }
}
