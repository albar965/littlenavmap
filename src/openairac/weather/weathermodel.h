/*****************************************************************************
* OpenAIRAC Map — Weather Domain Models
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

#ifndef OPENAIRAC_WEATHERMODEL_H
#define OPENAIRAC_WEATHERMODEL_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>

namespace openairac {

enum class FlightCategory {
    Vfr,
    Mvfr,
    Ifr,
    Lifr,
    Unknown,
};

enum class WeatherStaleness {
    Fresh,   // < 30 min
    Aging,   // 30 - 60 min
    Stale,   // 60 - 120 min
    Expired, // > 120 min
    Unknown,
};

struct CloudLayerInfo {
    QString cover; // FEW, SCT, BKN, OVC, VV
    int baseFeet = 0;
};

struct MetarInfo {
    QString stationId;
    QDateTime observationTime;
    QString rawText;
    FlightCategory flightCategory = FlightCategory::Unknown;
    double temperatureC = 0.0;
    double dewpointC = 0.0;
    int windDirDeg = 0;
    int windSpeedKts = 0;
    int windGustKts = 0;
    bool windVariable = false;
    double visibilitySm = 10.0;
    double altimeterHpa = 1013.2;
    double altimeterInHg = 29.92;
    QList<CloudLayerInfo> clouds;
    QStringList weatherPhenomena;
    QDateTime fetchTime;
    QString providerId = QStringLiteral("NOAA_AWC");

    bool isValid() const { return !stationId.isEmpty(); }
    int ageMinutes(const QDateTime& now = QDateTime::currentDateTimeUtc()) const;
    WeatherStaleness staleness(const QDateTime& now = QDateTime::currentDateTimeUtc()) const;
    QString flightCategoryString() const;
    QString flightCategoryColorHex() const;

    static MetarInfo fromJson(const QJsonObject& obj);
};

struct TafPeriodInfo {
    QDateTime validFrom;
    QDateTime validTo;
    QString changeType;
    int windDirDeg = 0;
    int windSpeedKts = 0;
    int windGustKts = 0;
    double visibilitySm = 6.0;
    FlightCategory flightCategory = FlightCategory::Unknown;
    QList<CloudLayerInfo> clouds;
    QStringList weatherPhenomena;
    QString rawPeriod;

    static TafPeriodInfo fromJson(const QJsonObject& obj);
};

struct TafInfo {
    QString stationId;
    QDateTime issueTime;
    QDateTime validFrom;
    QDateTime validTo;
    QString rawText;
    QList<TafPeriodInfo> forecastPeriods;
    QDateTime fetchTime;
    QString providerId = QStringLiteral("NOAA_AWC");

    bool isValid() const { return !stationId.isEmpty(); }
    bool isExpired(const QDateTime& now = QDateTime::currentDateTimeUtc()) const { return now > validTo; }
    const TafPeriodInfo* forecastAtEta(const QDateTime& eta) const;

    static TafInfo fromJson(const QJsonObject& obj);
};

struct SigmetAdvisory {
    QString id;
    QString firId;
    QString firName;
    QString hazard; // TS, TURB, ICE, VA
    QString qualifier;
    QDateTime validFrom;
    QDateTime validTo;
    int baseAltitudeFt = 0;
    int topAltitudeFt = 0;
    QList<QPair<double, double>> polygon; // (lon, lat)
    QString rawText;
    QString providerId = QStringLiteral("NOAA_AWC");

    bool isActiveAt(const QDateTime& now = QDateTime::currentDateTimeUtc()) const {
        return now >= validFrom && now <= validTo;
    }

    static SigmetAdvisory fromGeoJson(const QJsonObject& feat);
};

struct PirepInfo {
    QDateTime obsTime;
    QString aircraftType;
    double latitude = 0.0;
    double longitude = 0.0;
    int flightLevel = 0;
    QString turbulence;
    QString icing;
    double tempC = 0.0;
    QString rawText;

    static PirepInfo fromJson(const QJsonObject& obj);
};

} // namespace openairac

#endif // OPENAIRAC_WEATHERMODEL_H
