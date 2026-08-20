/*****************************************************************************
* OpenAIRAC Map — Online Flight Network Domain Models
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

#ifndef OPENAIRAC_ONLINENETWORKMODEL_H
#define OPENAIRAC_ONLINENETWORKMODEL_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QList>
#include <QPair>

namespace openairac {

enum class NetworkFreshnessState {
    Live,
    Delayed,
    Stale,
    Offline
};

enum class OnlineFacilityType {
    Unknown = 0,
    Delivery = 1,
    Ground = 2,
    Tower = 3,
    Approach = 4,
    Center = 5,
    Fss = 6
};

struct OnlinePilotItem {
    quint64 cid = 0;
    QString callsign;
    double latitude = 0.0;
    double longitude = 0.0;
    int altitudeFt = 0;
    int groundspeedKt = 0;
    int headingDeg = 0;
    QString transponder;
    QString aircraftType;
    QString departureIcao;
    QString arrivalIcao;
    QString alternateIcao;
    QString flightRules;
    QString route;
    int plannedAltitudeFt = 0;
    int plannedTasKt = 0;
    QString remarks;
    QDateTime logonTime;
    QDateTime lastUpdated;

    // Display interpolation state
    double prevLatitude = 0.0;
    double prevLongitude = 0.0;
    QDateTime prevUpdated;

    bool isAirborne() const {
        return groundspeedKt >= 40 || altitudeFt > 500;
    }

    QPair<double, double> interpolatedPosition(const QDateTime& currentTime) const;
};

struct OnlineControllerItem {
    quint64 cid = 0;
    QString callsign;
    QString frequency;
    OnlineFacilityType facilityType = OnlineFacilityType::Unknown;
    QString facilityTypeName;
    int rating = 0;
    int visualRangeNm = 50;
    QStringList textAtis;
    QString associatedAirport;
    bool isEnroute = false;
    QDateTime logonTime;
    QDateTime lastUpdated;
};

struct OnlineAtisItem {
    quint64 cid = 0;
    QString callsign;
    QString frequency;
    QChar atisCode;
    QStringList textAtis;
    QString airportIdent;
    QDateTime lastUpdated;
};

struct OnlineEventItem {
    quint64 id = 0;
    QString name;
    QString eventType;
    QDateTime startTime;
    QDateTime endTime;
    QStringList airports;
    QStringList routes;
    QString link;
    QString description;

    bool isActiveAt(const QDateTime& time) const {
        return time >= startTime && time <= endTime;
    }

    bool matchesAirport(const QString& icao) const {
        QString needle = icao.trimmed().toUpper();
        for (const QString& a : airports) {
            if (a.trimmed().toUpper() == needle) {
                return true;
            }
        }
        return false;
    }
};

struct OnlineSnapshotItem {
    QString providerName = QStringLiteral("VATSIM");
    QDateTime generatedAt;
    QDateTime receivedAt;
    int connectedClients = 0;
    QList<OnlinePilotItem> pilots;
    QList<OnlineControllerItem> controllers;
    QList<OnlineAtisItem> atis;
    QList<OnlineEventItem> events;
    NetworkFreshnessState freshness = NetworkFreshnessState::Offline;
    int ageSeconds = 0;
};

struct AirportOnlineItem {
    QString airportIdent;
    QList<OnlineControllerItem> atcControllers;
    OnlineAtisItem atis;
    bool hasAtis = false;
    QList<OnlinePilotItem> filedArrivals;
    QList<OnlinePilotItem> filedDepartures;
    QList<OnlinePilotItem> onGroundTraffic;
};

struct RouteOnlineItem {
    QString departureIcao;
    QString arrivalIcao;
    QList<OnlineControllerItem> departureAtc;
    OnlineAtisItem departureAtis;
    bool hasDepartureAtis = false;
    QList<OnlineControllerItem> enrouteAtc;
    QList<OnlineControllerItem> arrivalAtc;
    OnlineAtisItem arrivalAtis;
    bool hasArrivalAtis = false;
    QList<OnlinePilotItem> trafficInCorridor;
    QList<OnlinePilotItem> trafficNearDestination;
    QList<OnlineEventItem> matchingEvents;
};

} // namespace openairac

#endif // OPENAIRAC_ONLINENETWORKMODEL_H
