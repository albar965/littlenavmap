/*****************************************************************************
* OpenAIRAC Map — Online Network Client Implementation
*
* Copyright 2026 OpenAIRAC Contributors
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
*****************************************************************************/

#include "openairac/online/onlineclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QtMath>
#include <QDebug>

namespace openairac {

static const double EARTH_RADIUS_NM = 3440.065;

static double gcDistanceNm(double lon1, double lat1, double lon2, double lat2) {
    double rlat1 = qDegreesToRadians(lat1);
    double rlon1 = qDegreesToRadians(lon1);
    double rlat2 = qDegreesToRadians(lat2);
    double rlon2 = qDegreesToRadians(lon2);

    double dlat = rlat2 - rlat1;
    double dlon = rlon2 - rlon1;

    double a = qSin(dlat / 2.0) * qSin(dlat / 2.0) +
               qCos(rlat1) * qCos(rlat2) * qSin(dlon / 2.0) * qSin(dlon / 2.0);
    double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));

    return EARTH_RADIUS_NM * c;
}

static double pointToSegmentDistNm(double px, double py, double x1, double y1, double x2, double y2) {
    double segLen = gcDistanceNm(x1, y1, x2, y2);
    if (segLen < 1e-4) {
        return gcDistanceNm(px, py, x1, y1);
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    double t = qBound(0.0, ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy), 1.0);
    double projX = x1 + t * dx;
    double projY = y1 + t * dy;

    return gcDistanceNm(px, py, projX, projY);
}

static double minDistanceToRoute(double px, double py, const QList<QPair<double, double>>& route) {
    if (route.isEmpty()) {
        return 999999.0;
    }
    if (route.size() == 1) {
        return gcDistanceNm(px, py, route[0].first, route[0].second);
    }

    double minD = 999999.0;
    for (int i = 0; i < route.size() - 1; ++i) {
        double d = pointToSegmentDistNm(px, py, route[i].first, route[i].second, route[i + 1].first, route[i + 1].second);
        if (d < minD) {
            minD = d;
        }
    }
    return minD;
}

OnlineClient& OnlineClient::instance() {
    static OnlineClient inst;
    return inst;
}

OnlineClient::OnlineClient()
    : QObject(nullptr) {
    m_netManager = new QNetworkAccessManager(this);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(m_refreshIntervalSecs * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &OnlineClient::onRefreshTimer);

    m_eventsTimer = new QTimer(this);
    m_eventsTimer->setInterval(600 * 1000); // 10 minutes for events
    connect(m_eventsTimer, &QTimer::timeout, this, &OnlineClient::onEventsTimer);

    if (m_enabled) {
        m_refreshTimer->start();
        m_eventsTimer->start();
        // Initial requests
        QTimer::singleShot(500, this, [this]() {
            requestSnapshot(true);
            requestEvents(true);
        });
    }
}

void OnlineClient::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (m_enabled) {
        m_refreshTimer->start();
        m_eventsTimer->start();
        requestSnapshot(true);
    } else {
        m_refreshTimer->stop();
        m_eventsTimer->stop();
    }
}

void OnlineClient::setRefreshIntervalSecs(int secs) {
    m_refreshIntervalSecs = qMax(10, secs);
    m_refreshTimer->setInterval(m_refreshIntervalSecs * 1000);
}

OnlineSnapshotItem OnlineClient::latestSnapshot() const {
    return m_latestSnapshot;
}

QList<OnlineEventItem> OnlineClient::latestEvents() const {
    return m_latestEvents;
}

void OnlineClient::onRefreshTimer() {
    if (m_enabled) {
        requestSnapshot(false);
    }
}

void OnlineClient::onEventsTimer() {
    if (m_enabled) {
        requestEvents(false);
    }
}

void OnlineClient::requestSnapshot(bool force) {
    Q_UNUSED(force);
    QUrl url(m_dataUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.4.0 (aviation-map; contact: maintainers@openairac.org)"));

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            parseVatsimDataJson(reply->readAll());
        } else {
            emit onlineRequestFailed(QStringLiteral("VATSIM Data API"), reply->errorString());
            emit networkStatusChanged(QStringLiteral("OFFLINE"), 0, 0);
        }
    });
}

void OnlineClient::requestEvents(bool force) {
    Q_UNUSED(force);
    QUrl url(m_eventsUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.4.0"));

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            parseVatsimEventsJson(reply->readAll());
        }
    });
}

void OnlineClient::parseVatsimDataJson(const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject gen = root[QStringLiteral("general")].toObject();

    QDateTime now = QDateTime::currentDateTimeUtc();
    QString genTimeStr = gen[QStringLiteral("update_timestamp")].toString();
    if (genTimeStr.isEmpty()) {
        genTimeStr = gen[QStringLiteral("update")].toString();
    }

    QDateTime genTime = QDateTime::fromString(genTimeStr, Qt::ISODate);
    if (!genTime.isValid()) {
        genTime = now;
    }

    OnlineSnapshotItem snapshot;
    snapshot.providerName = QStringLiteral("VATSIM");
    snapshot.generatedAt = genTime;
    snapshot.receivedAt = now;
    snapshot.connectedClients = gen[QStringLiteral("connected_clients")].toInt();
    snapshot.ageSeconds = qMax(0, static_cast<int>(genTime.secsTo(now)));

    if (snapshot.ageSeconds <= 35) {
        snapshot.freshness = NetworkFreshnessState::Live;
    } else if (snapshot.ageSeconds <= 90) {
        snapshot.freshness = NetworkFreshnessState::Delayed;
    } else if (snapshot.ageSeconds <= 300) {
        snapshot.freshness = NetworkFreshnessState::Stale;
    } else {
        snapshot.freshness = NetworkFreshnessState::Offline;
    }

    // 1. Pilots
    QJsonArray pilotsArr = root[QStringLiteral("pilots")].toArray();
    for (const QJsonValue& val : pilotsArr) {
        QJsonObject p = val.toObject();
        OnlinePilotItem pilot;
        pilot.cid = p[QStringLiteral("cid")].toVariant().toULongLong();
        pilot.callsign = p[QStringLiteral("callsign")].toString().trimmed().toUpper();
        if (pilot.callsign.isEmpty()) continue;

        pilot.latitude = p[QStringLiteral("latitude")].toDouble();
        pilot.longitude = p[QStringLiteral("longitude")].toDouble();
        pilot.altitudeFt = p[QStringLiteral("altitude")].toInt();
        pilot.groundspeedKt = p[QStringLiteral("groundspeed")].toInt();
        pilot.headingDeg = p[QStringLiteral("heading")].toInt();
        pilot.transponder = p[QStringLiteral("transponder")].toString();

        QJsonObject fp = p[QStringLiteral("flight_plan")].toObject();
        if (!fp.isEmpty()) {
            pilot.aircraftType = fp[QStringLiteral("aircraft_short")].toString();
            if (pilot.aircraftType.isEmpty()) pilot.aircraftType = fp[QStringLiteral("aircraft")].toString();
            pilot.departureIcao = fp[QStringLiteral("departure")].toString().trimmed().toUpper();
            pilot.arrivalIcao = fp[QStringLiteral("arrival")].toString().trimmed().toUpper();
            pilot.alternateIcao = fp[QStringLiteral("alternate")].toString().trimmed().toUpper();
            pilot.flightRules = fp[QStringLiteral("flight_rules")].toString();
            pilot.route = fp[QStringLiteral("route")].toString();
            pilot.remarks = fp[QStringLiteral("remarks")].toString();

            QString altStr = fp[QStringLiteral("altitude")].toString().trimmed().toUpper();
            if (altStr.startsWith(QStringLiteral("FL"))) {
                pilot.plannedAltitudeFt = altStr.mid(2).toInt() * 100;
            } else {
                pilot.plannedAltitudeFt = altStr.toInt();
            }
            pilot.plannedTasKt = fp[QStringLiteral("cruise_tas")].toString().toInt();
        }

        pilot.logonTime = QDateTime::fromString(p[QStringLiteral("logon_time")].toString(), Qt::ISODate);
        pilot.lastUpdated = QDateTime::fromString(p[QStringLiteral("last_updated")].toString(), Qt::ISODate);
        if (!pilot.lastUpdated.isValid()) pilot.lastUpdated = now;

        // Preserve previous position for interpolation
        if (m_previousPilots.contains(pilot.callsign)) {
            const OnlinePilotItem& old = m_previousPilots[pilot.callsign];
            pilot.prevLatitude = old.latitude;
            pilot.prevLongitude = old.longitude;
            pilot.prevUpdated = old.lastUpdated;
        } else {
            pilot.prevLatitude = pilot.latitude;
            pilot.prevLongitude = pilot.longitude;
            pilot.prevUpdated = pilot.lastUpdated;
        }

        m_previousPilots[pilot.callsign] = pilot;
        snapshot.pilots.append(pilot);
    }

    // 2. Controllers
    QJsonArray ctrlArr = root[QStringLiteral("controllers")].toArray();
    for (const QJsonValue& val : ctrlArr) {
        QJsonObject c = val.toObject();
        OnlineControllerItem ctrl;
        ctrl.cid = c[QStringLiteral("cid")].toVariant().toULongLong();
        ctrl.callsign = c[QStringLiteral("callsign")].toString().trimmed().toUpper();
        if (ctrl.callsign.isEmpty()) continue;

        ctrl.frequency = c[QStringLiteral("frequency")].toString();
        int fac = c[QStringLiteral("facility")].toInt();
        ctrl.rating = c[QStringLiteral("rating")].toInt();
        ctrl.visualRangeNm = c[QStringLiteral("visual_range")].toInt();

        // Facility type detection
        if (fac == 1 || ctrl.callsign.endsWith(QStringLiteral("_DEL")) || ctrl.callsign.endsWith(QStringLiteral("_CLR"))) {
            ctrl.facilityType = OnlineFacilityType::Delivery;
            ctrl.facilityTypeName = QStringLiteral("DEL");
        } else if (fac == 2 || ctrl.callsign.endsWith(QStringLiteral("_GND"))) {
            ctrl.facilityType = OnlineFacilityType::Ground;
            ctrl.facilityTypeName = QStringLiteral("GND");
        } else if (fac == 3 || ctrl.callsign.endsWith(QStringLiteral("_TWR"))) {
            ctrl.facilityType = OnlineFacilityType::Tower;
            ctrl.facilityTypeName = QStringLiteral("TWR");
        } else if (fac == 4 || ctrl.callsign.endsWith(QStringLiteral("_APP")) || ctrl.callsign.endsWith(QStringLiteral("_DEP"))) {
            ctrl.facilityType = OnlineFacilityType::Approach;
            ctrl.facilityTypeName = QStringLiteral("APP");
        } else if (fac == 5 || ctrl.callsign.endsWith(QStringLiteral("_CTR")) || ctrl.callsign.endsWith(QStringLiteral("_ACC"))) {
            ctrl.facilityType = OnlineFacilityType::Center;
            ctrl.facilityTypeName = QStringLiteral("CTR");
            ctrl.isEnroute = true;
        } else {
            ctrl.facilityType = OnlineFacilityType::Unknown;
            ctrl.facilityTypeName = QStringLiteral("ATC");
        }

        QStringList parts = ctrl.callsign.split(QLatin1Char('_'));
        if (!parts.isEmpty() && !ctrl.isEnroute) {
            QString prefix = parts[0];
            if (prefix.length() == 4) {
                ctrl.associatedAirport = prefix;
            } else if (prefix.length() == 3) {
                ctrl.associatedAirport = QStringLiteral("K") + prefix;
            }
        }

        QJsonArray atisLines = c[QStringLiteral("text_atis")].toArray();
        for (const QJsonValue& line : atisLines) {
            ctrl.textAtis.append(line.toString());
        }

        ctrl.logonTime = QDateTime::fromString(c[QStringLiteral("logon_time")].toString(), Qt::ISODate);
        ctrl.lastUpdated = QDateTime::fromString(c[QStringLiteral("last_updated")].toString(), Qt::ISODate);

        snapshot.controllers.append(ctrl);
    }

    // 3. ATIS
    QJsonArray atisArr = root[QStringLiteral("atis")].toArray();
    for (const QJsonValue& val : atisArr) {
        QJsonObject a = val.toObject();
        OnlineAtisItem atis;
        atis.cid = a[QStringLiteral("cid")].toVariant().toULongLong();
        atis.callsign = a[QStringLiteral("callsign")].toString().trimmed().toUpper();
        if (atis.callsign.isEmpty()) continue;

        atis.frequency = a[QStringLiteral("frequency")].toString();
        QString codeStr = a[QStringLiteral("atis_code")].toString();
        if (!codeStr.isEmpty()) atis.atisCode = codeStr.at(0).toUpper();

        QStringList parts = atis.callsign.split(QLatin1Char('_'));
        if (!parts.isEmpty()) {
            QString prefix = parts[0];
            if (prefix.length() == 4) atis.airportIdent = prefix;
            else if (prefix.length() == 3) atis.airportIdent = QStringLiteral("K") + prefix;
            else atis.airportIdent = prefix;
        }

        QJsonArray atisLines = a[QStringLiteral("text_atis")].toArray();
        for (const QJsonValue& line : atisLines) {
            atis.textAtis.append(line.toString());
        }

        atis.lastUpdated = QDateTime::fromString(a[QStringLiteral("last_updated")].toString(), Qt::ISODate);
        snapshot.atis.append(atis);
    }

    snapshot.events = m_latestEvents;
    m_latestSnapshot = snapshot;

    QString statusStr = QStringLiteral("LIVE");
    if (snapshot.freshness == NetworkFreshnessState::Delayed) statusStr = QStringLiteral("DELAYED");
    else if (snapshot.freshness == NetworkFreshnessState::Stale) statusStr = QStringLiteral("STALE");
    else if (snapshot.freshness == NetworkFreshnessState::Offline) statusStr = QStringLiteral("OFFLINE");

    emit snapshotReady(m_latestSnapshot);
    emit networkStatusChanged(statusStr, snapshot.connectedClients, snapshot.ageSeconds);
}

void OnlineClient::parseVatsimEventsJson(const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject() && doc.object()[QStringLiteral("data")].isArray()) {
        arr = doc.object()[QStringLiteral("data")].toArray();
    }

    QList<OnlineEventItem> events;
    for (const QJsonValue& val : arr) {
        QJsonObject e = val.toObject();
        OnlineEventItem ev;
        ev.id = e[QStringLiteral("id")].toVariant().toULongLong();
        ev.name = e[QStringLiteral("name")].toString();
        if (ev.name.isEmpty()) continue;

        ev.eventType = e[QStringLiteral("type")].toObject()[QStringLiteral("name")].toString();
        ev.startTime = QDateTime::fromString(e[QStringLiteral("start_time")].toString(), Qt::ISODate);
        ev.endTime = QDateTime::fromString(e[QStringLiteral("end_time")].toString(), Qt::ISODate);
        ev.link = e[QStringLiteral("link")].toString();
        ev.description = e[QStringLiteral("short_description")].toString();

        QJsonArray apts = e[QStringLiteral("airports")].toArray();
        for (const QJsonValue& aptVal : apts) {
            if (aptVal.isObject()) {
                QString icao = aptVal.toObject()[QStringLiteral("icao")].toString().trimmed().toUpper();
                if (!icao.isEmpty()) ev.airports.append(icao);
            } else if (aptVal.isString()) {
                ev.airports.append(aptVal.toString().trimmed().toUpper());
            }
        }

        events.append(ev);
    }

    m_latestEvents = events;
    m_latestSnapshot.events = events;
    emit eventsReady(m_latestEvents);
}

AirportOnlineItem OnlineClient::getAirportOnlineSummary(const QString& icao) const {
    QString target = icao.trimmed().toUpper();
    AirportOnlineItem summary;
    summary.airportIdent = target;

    // ATC
    for (const OnlineControllerItem& c : m_latestSnapshot.controllers) {
        if (c.associatedAirport == target) {
            summary.atcControllers.append(c);
        }
    }

    // ATIS
    for (const OnlineAtisItem& a : m_latestSnapshot.atis) {
        if (a.airportIdent == target) {
            summary.atis = a;
            summary.hasAtis = true;
            break;
        }
    }

    // Traffic
    for (const OnlinePilotItem& p : m_latestSnapshot.pilots) {
        if (p.arrivalIcao == target) {
            summary.filedArrivals.append(p);
        }
        if (p.departureIcao == target) {
            summary.filedDepartures.append(p);
        }
    }

    return summary;
}

RouteOnlineItem OnlineClient::getRouteOnlineAwareness(
    const QString& depIcao,
    const QString& arrIcao,
    const QList<QPair<double, double>>& routeCoordinates,
    double corridorHalfWidthNm
) const {
    QString dep = depIcao.trimmed().toUpper();
    QString arr = arrIcao.trimmed().toUpper();

    RouteOnlineItem awareness;
    awareness.departureIcao = dep;
    awareness.arrivalIcao = arr;

    // 1. ATIS
    for (const OnlineAtisItem& a : m_latestSnapshot.atis) {
        if (a.airportIdent == dep) {
            awareness.departureAtis = a;
            awareness.hasDepartureAtis = true;
        } else if (a.airportIdent == arr) {
            awareness.arrivalAtis = a;
            awareness.hasArrivalAtis = true;
        }
    }

    // 2. Controllers
    for (const OnlineControllerItem& c : m_latestSnapshot.controllers) {
        if (c.associatedAirport == dep) {
            awareness.departureAtc.append(c);
        } else if (c.associatedAirport == arr) {
            awareness.arrivalAtc.append(c);
        } else if (c.isEnroute) {
            // Check likely relevance for route
            if ((c.callsign.startsWith(QStringLiteral("NY_")) || c.callsign.startsWith(QStringLiteral("ZNY_"))) && (dep.startsWith(QLatin1Char('K')) || arr.startsWith(QLatin1Char('K')))) {
                awareness.enrouteAtc.append(c);
            } else if ((c.callsign.startsWith(QStringLiteral("LON_")) || c.callsign.startsWith(QStringLiteral("EGTT_"))) && (dep.startsWith(QStringLiteral("EG")) || arr.startsWith(QStringLiteral("EG")))) {
                awareness.enrouteAtc.append(c);
            } else if ((c.callsign.startsWith(QStringLiteral("LFFF_")) || c.callsign.startsWith(QStringLiteral("PARIS_"))) && (dep.startsWith(QStringLiteral("LF")) || arr.startsWith(QStringLiteral("LF")))) {
                awareness.enrouteAtc.append(c);
            }
        }
    }

    // 3. Traffic in corridor
    for (const OnlinePilotItem& p : m_latestSnapshot.pilots) {
        if (!routeCoordinates.isEmpty()) {
            double d = minDistanceToRoute(p.longitude, p.latitude, routeCoordinates);
            if (d <= corridorHalfWidthNm) {
                awareness.trafficInCorridor.append(p);
            }
        }

        if (!routeCoordinates.isEmpty()) {
            const QPair<double, double>& destPt = routeCoordinates.last();
            double dDest = gcDistanceNm(p.longitude, p.latitude, destPt.first, destPt.second);
            if (dDest <= 100.0) {
                awareness.trafficNearDestination.append(p);
            }
        }
    }

    // 4. Events
    QDateTime now = QDateTime::currentDateTimeUtc();
    for (const OnlineEventItem& ev : m_latestSnapshot.events) {
        if ((ev.isActiveAt(now) || ev.startTime <= now.addDays(1)) && (ev.matchesAirport(dep) || ev.matchesAirport(arr))) {
            awareness.matchingEvents.append(ev);
        }
    }

    return awareness;
}

} // namespace openairac
