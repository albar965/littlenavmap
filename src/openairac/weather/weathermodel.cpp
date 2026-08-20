/*****************************************************************************
* OpenAIRAC Map — Weather Domain Models Implementation
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

#include "openairac/weather/weathermodel.h"

namespace openairac {

int MetarInfo::ageMinutes(const QDateTime& now) const
{
    if (!observationTime.isValid()) return 0;
    qint64 diff = observationTime.secsTo(now);
    return static_cast<int>(qMax(0LL, diff / 60));
}

WeatherStaleness MetarInfo::staleness(const QDateTime& now) const
{
    int age = ageMinutes(now);
    if (age <= 30) return WeatherStaleness::Fresh;
    if (age <= 60) return WeatherStaleness::Aging;
    if (age <= 120) return WeatherStaleness::Stale;
    return WeatherStaleness::Expired;
}

QString MetarInfo::flightCategoryString() const
{
    switch (flightCategory) {
        case FlightCategory::Vfr: return QStringLiteral("VFR");
        case FlightCategory::Mvfr: return QStringLiteral("MVFR");
        case FlightCategory::Ifr: return QStringLiteral("IFR");
        case FlightCategory::Lifr: return QStringLiteral("LIFR");
        case FlightCategory::Unknown: return QStringLiteral("UNKNOWN");
    }
    return QStringLiteral("UNKNOWN");
}

QString MetarInfo::flightCategoryColorHex() const
{
    switch (flightCategory) {
        case FlightCategory::Vfr: return QStringLiteral("#28a745");
        case FlightCategory::Mvfr: return QStringLiteral("#007bff");
        case FlightCategory::Ifr: return QStringLiteral("#dc3545");
        case FlightCategory::Lifr: return QStringLiteral("#6f42c1");
        case FlightCategory::Unknown: return QStringLiteral("#6c757d");
    }
    return QStringLiteral("#6c757d");
}

MetarInfo MetarInfo::fromJson(const QJsonObject& obj)
{
    MetarInfo info;
    info.stationId = obj.value(QStringLiteral("icaoId")).toString().trimmed().toUpper();
    info.rawText = obj.value(QStringLiteral("rawOb")).toString().trimmed();

    if (obj.contains(QStringLiteral("obsTime"))) {
        qint64 ts = obj.value(QStringLiteral("obsTime")).toVariant().toLongLong();
        info.observationTime = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
    } else if (obj.contains(QStringLiteral("reportTime"))) {
        info.observationTime = QDateTime::fromString(obj.value(QStringLiteral("reportTime")).toString(), Qt::ISODate);
    }

    info.temperatureC = obj.value(QStringLiteral("temp")).toDouble(0.0);
    info.dewpointC = obj.value(QStringLiteral("dewp")).toDouble(0.0);
    info.windDirDeg = obj.value(QStringLiteral("wdir")).toInt(0);
    info.windSpeedKts = obj.value(QStringLiteral("wspd")).toInt(0);
    info.windGustKts = obj.value(QStringLiteral("wgst")).toInt(0);
    info.windVariable = (info.windDirDeg == 0 && info.windSpeedKts > 0);

    QJsonValue visVal = obj.value(QStringLiteral("visib"));
    if (visVal.isString() && visVal.toString().contains(QLatin1Char('+'))) {
        info.visibilitySm = 10.0;
    } else {
        info.visibilitySm = visVal.toDouble(10.0);
    }

    info.altimeterHpa = obj.value(QStringLiteral("altim")).toDouble(1013.2);
    info.altimeterInHg = info.altimeterHpa * 0.0295299830714;

    QString fltcat = obj.value(QStringLiteral("fltcat")).toString().toUpper();
    if (fltcat == QStringLiteral("VFR")) info.flightCategory = FlightCategory::Vfr;
    else if (fltcat == QStringLiteral("MVFR")) info.flightCategory = FlightCategory::Mvfr;
    else if (fltcat == QStringLiteral("IFR")) info.flightCategory = FlightCategory::Ifr;
    else if (fltcat == QStringLiteral("LIFR")) info.flightCategory = FlightCategory::Lifr;
    else info.flightCategory = FlightCategory::Vfr;

    QJsonArray clouds = obj.value(QStringLiteral("clouds")).toArray();
    for (const QJsonValue& c : clouds) {
        QJsonObject co = c.toObject();
        CloudLayerInfo cl;
        cl.cover = co.value(QStringLiteral("cover")).toString();
        cl.baseFeet = co.value(QStringLiteral("base")).toInt(0);
        info.clouds.append(cl);
    }

    info.fetchTime = QDateTime::currentDateTimeUtc();
    return info;
}

TafPeriodInfo TafPeriodInfo::fromJson(const QJsonObject& obj)
{
    TafPeriodInfo p;
    qint64 tFrom = obj.value(QStringLiteral("timeFrom")).toVariant().toLongLong();
    qint64 tTo = obj.value(QStringLiteral("timeTo")).toVariant().toLongLong();
    p.validFrom = QDateTime::fromSecsSinceEpoch(tFrom, Qt::UTC);
    p.validTo = QDateTime::fromSecsSinceEpoch(tTo, Qt::UTC);
    p.changeType = obj.value(QStringLiteral("change")).toString();
    p.windDirDeg = obj.value(QStringLiteral("wdir")).toInt(0);
    p.windSpeedKts = obj.value(QStringLiteral("wspd")).toInt(0);
    p.windGustKts = obj.value(QStringLiteral("wgst")).toInt(0);
    p.visibilitySm = obj.value(QStringLiteral("visib")).toDouble(6.0);
    p.rawPeriod = obj.value(QStringLiteral("rawWx")).toString();

    if (p.visibilitySm >= 5.0) p.flightCategory = FlightCategory::Vfr;
    else if (p.visibilitySm >= 3.0) p.flightCategory = FlightCategory::Mvfr;
    else p.flightCategory = FlightCategory::Ifr;

    return p;
}

const TafPeriodInfo* TafInfo::forecastAtEta(const QDateTime& eta) const
{
    for (const TafPeriodInfo& p : forecastPeriods) {
        if (eta >= p.validFrom && eta < p.validTo) {
            return &p;
        }
    }
    if (!forecastPeriods.isEmpty()) {
        return &forecastPeriods.first();
    }
    return nullptr;
}

TafInfo TafInfo::fromJson(const QJsonObject& obj)
{
    TafInfo taf;
    taf.stationId = obj.value(QStringLiteral("icaoId")).toString().trimmed().toUpper();
    taf.rawText = obj.value(QStringLiteral("rawTAF")).toString().trimmed();

    QString issueStr = obj.value(QStringLiteral("issueTime")).toString();
    taf.issueTime = QDateTime::fromString(issueStr, Qt::ISODate);

    qint64 vFrom = obj.value(QStringLiteral("validTimeFrom")).toVariant().toLongLong();
    qint64 vTo = obj.value(QStringLiteral("validTimeTo")).toVariant().toLongLong();
    taf.validFrom = QDateTime::fromSecsSinceEpoch(vFrom, Qt::UTC);
    taf.validTo = QDateTime::fromSecsSinceEpoch(vTo, Qt::UTC);

    QJsonArray fcsts = obj.value(QStringLiteral("fcsts")).toArray();
    for (const QJsonValue& f : fcsts) {
        taf.forecastPeriods.append(TafPeriodInfo::fromJson(f.toObject()));
    }

    taf.fetchTime = QDateTime::currentDateTimeUtc();
    return taf;
}

SigmetAdvisory SigmetAdvisory::fromGeoJson(const QJsonObject& feat)
{
    SigmetAdvisory sig;
    QJsonObject props = feat.value(QStringLiteral("properties")).toObject();
    sig.firId = props.value(QStringLiteral("firId")).toString();
    sig.firName = props.value(QStringLiteral("firName")).toString();
    sig.hazard = props.value(QStringLiteral("hazard")).toString();
    sig.qualifier = props.value(QStringLiteral("qualifier")).toString();
    sig.validFrom = QDateTime::fromString(props.value(QStringLiteral("validTimeFrom")).toString(), Qt::ISODate);
    sig.validTo = QDateTime::fromString(props.value(QStringLiteral("validTimeTo")).toString(), Qt::ISODate);
    sig.baseAltitudeFt = props.value(QStringLiteral("base")).toInt(0);
    sig.topAltitudeFt = props.value(QStringLiteral("top")).toInt(0);
    sig.rawText = props.value(QStringLiteral("rawSigmet")).toString();

    QString series = props.value(QStringLiteral("seriesId")).toString();
    sig.id = sig.firId + QStringLiteral(":") + series;

    QJsonObject geom = feat.value(QStringLiteral("geometry")).toObject();
    QJsonArray coords = geom.value(QStringLiteral("coordinates")).toArray();
    if (!coords.isEmpty()) {
        QJsonArray ring = coords.first().toArray();
        for (const QJsonValue& pt : ring) {
            QJsonArray ptArr = pt.toArray();
            if (ptArr.size() >= 2) {
                sig.polygon.append(qMakePair(ptArr[0].toDouble(), ptArr[1].toDouble()));
            }
        }
    }
    return sig;
}

PirepInfo PirepInfo::fromJson(const QJsonObject& obj)
{
    PirepInfo p;
    qint64 ts = obj.value(QStringLiteral("obsTime")).toVariant().toLongLong();
    p.obsTime = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
    p.aircraftType = obj.value(QStringLiteral("acType")).toString();
    p.latitude = obj.value(QStringLiteral("lat")).toDouble();
    p.longitude = obj.value(QStringLiteral("lon")).toDouble();
    p.flightLevel = obj.value(QStringLiteral("fltLvl")).toInt(0);
    p.turbulence = obj.value(QStringLiteral("tbType")).toString();
    p.icing = obj.value(QStringLiteral("icgType")).toString();
    p.tempC = obj.value(QStringLiteral("temp")).toDouble(0.0);
    p.rawText = obj.value(QStringLiteral("rawOb")).toString();
    return p;
}

} // namespace openairac
