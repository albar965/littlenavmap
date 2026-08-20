/*****************************************************************************
* OpenAIRAC Map — Weather Client Implementation
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

#include "openairac/weather/weatherclient.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "db/dbtools.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

namespace openairac {

WeatherClient& WeatherClient::instance()
{
    static WeatherClient s_instance;
    return s_instance;
}

WeatherClient::WeatherClient()
{
    m_netManager = new QNetworkAccessManager(this);

    QString basePath = atools::settings::Settings::getPath();
    m_cacheDbPath = basePath + QDir::separator() + QStringLiteral("openairac_weather.sqlite");

    ensureCacheDatabase();
}

QString WeatherClient::cacheDatabasePath() const
{
    return m_cacheDbPath;
}

void WeatherClient::setCacheDatabasePath(const QString& path)
{
    m_cacheDbPath = path;
    ensureCacheDatabase();
}

void WeatherClient::ensureCacheDatabase() const
{
    if (QFile::exists(m_cacheDbPath)) {
        return;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_INIT_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, false /* readonly */, true /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.exec(
            "CREATE TABLE IF NOT EXISTS metar_cache ("
            "  station_id TEXT PRIMARY KEY NOT NULL,"
            "  json_payload TEXT NOT NULL,"
            "  fetched_at TEXT NOT NULL"
            ")"
        );
        q.exec(
            "CREATE TABLE IF NOT EXISTS taf_cache ("
            "  station_id TEXT PRIMARY KEY NOT NULL,"
            "  json_payload TEXT NOT NULL,"
            "  fetched_at TEXT NOT NULL"
            ")"
        );
        q.exec(
            "CREATE TABLE IF NOT EXISTS sigmet_cache ("
            "  id TEXT PRIMARY KEY NOT NULL,"
            "  json_payload TEXT NOT NULL,"
            "  valid_to TEXT NOT NULL"
            ")"
        );

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize weather cache database:" << e.what();
    }
}

MetarInfo WeatherClient::getCachedMetar(const QString& stationId) const
{
    MetarInfo info;
    QString cleanId = stationId.trimmed().toUpper();
    if (!QFile::exists(m_cacheDbPath) || cleanId.isEmpty()) return info;

    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_METAR_QUERY_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, true /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.prepare("SELECT json_payload FROM metar_cache WHERE station_id = :id LIMIT 1");
        q.bindValue(QStringLiteral(":id"), cleanId);
        if (q.exec() && q.next()) {
            QJsonDocument doc = QJsonDocument::fromJson(q.valueStr(0).toUtf8());
            info = MetarInfo::fromJson(doc.object());
        }

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Error reading METAR cache:" << e.what();
    }
    return info;
}

TafInfo WeatherClient::getCachedTaf(const QString& stationId) const
{
    TafInfo info;
    QString cleanId = stationId.trimmed().toUpper();
    if (!QFile::exists(m_cacheDbPath) || cleanId.isEmpty()) return info;

    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_TAF_QUERY_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, true /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.prepare("SELECT json_payload FROM taf_cache WHERE station_id = :id LIMIT 1");
        q.bindValue(QStringLiteral(":id"), cleanId);
        if (q.exec() && q.next()) {
            QJsonDocument doc = QJsonDocument::fromJson(q.valueStr(0).toUtf8());
            info = TafInfo::fromJson(doc.object());
        }

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Error reading TAF cache:" << e.what();
    }
    return info;
}

QList<SigmetAdvisory> WeatherClient::getCachedSigmets() const
{
    QList<SigmetAdvisory> list;
    if (!QFile::exists(m_cacheDbPath)) return list;

    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_SIGMET_QUERY_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, true /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        QString nowStr = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        q.prepare("SELECT json_payload FROM sigmet_cache WHERE valid_to >= :now");
        q.bindValue(QStringLiteral(":now"), nowStr);
        if (q.exec()) {
            while (q.next()) {
                QJsonDocument doc = QJsonDocument::fromJson(q.valueStr(0).toUtf8());
                list.append(SigmetAdvisory::fromGeoJson(doc.object()));
            }
        }

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Error reading SIGMET cache:" << e.what();
    }
    return list;
}

void WeatherClient::saveMetarToCache(const MetarInfo& metar) const
{
    if (!metar.isValid() || !QFile::exists(m_cacheDbPath)) return;
    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_SAVE_METAR_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, false /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.prepare("INSERT OR REPLACE INTO metar_cache (station_id, json_payload, fetched_at) VALUES (:id, :payload, :fetched)");
        q.bindValue(QStringLiteral(":id"), metar.station_id);

        QJsonObject obj;
        obj[QStringLiteral("icaoId")] = metar.station_id;
        obj[QStringLiteral("rawOb")] = metar.rawText;
        obj[QStringLiteral("obsTime")] = metar.observationTime.toSecsSinceEpoch();
        obj[QStringLiteral("temp")] = metar.temperatureC;
        obj[QStringLiteral("dewp")] = metar.dewpointC;
        obj[QStringLiteral("wdir")] = metar.windDirDeg;
        obj[QStringLiteral("wspd")] = metar.windSpeedKts;
        obj[QStringLiteral("wgst")] = metar.windGustKts;
        obj[QStringLiteral("visib")] = metar.visibilitySm;
        obj[QStringLiteral("altim")] = metar.altimeterHpa;
        obj[QStringLiteral("fltcat")] = metar.flightCategoryString();

        QJsonArray clouds;
        for (const CloudLayerInfo& cl : metar.clouds) {
            QJsonObject co;
            co[QStringLiteral("cover")] = cl.cover;
            co[QStringLiteral("base")] = cl.baseFeet;
            clouds.append(co);
        }
        obj[QStringLiteral("clouds")] = clouds;

        q.bindValue(QStringLiteral(":payload"), QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        q.bindValue(QStringLiteral(":fetched"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        q.exec();

        dbtools::closeDatabaseFile(&db);
    } catch (...) {}
}

void WeatherClient::saveTafToCache(const TafInfo& taf) const
{
    if (!taf.isValid() || !QFile::exists(m_cacheDbPath)) return;
    try {
        atools::sql::SqlDatabase db(QStringLiteral("WEATHER_SAVE_TAF_TEMP"));
        dbtools::openDatabaseFile(&db, m_cacheDbPath, false /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.prepare("INSERT OR REPLACE INTO taf_cache (station_id, json_payload, fetched_at) VALUES (:id, :payload, :fetched)");
        q.bindValue(QStringLiteral(":id"), taf.station_id);

        QJsonObject obj;
        obj[QStringLiteral("icaoId")] = taf.station_id;
        obj[QStringLiteral("rawTAF")] = taf.rawText;
        obj[QStringLiteral("issueTime")] = taf.issueTime.toString(Qt::ISODate);
        obj[QStringLiteral("validTimeFrom")] = taf.validFrom.toSecsSinceEpoch();
        obj[QStringLiteral("validTimeTo")] = taf.validTo.toSecsSinceEpoch();

        QJsonArray fcsts;
        for (const TafPeriodInfo& p : taf.forecastPeriods) {
            QJsonObject fo;
            fo[QStringLiteral("timeFrom")] = p.validFrom.toSecsSinceEpoch();
            fo[QStringLiteral("timeTo")] = p.validTo.toSecsSinceEpoch();
            fo[QStringLiteral("change")] = p.changeType;
            fo[QStringLiteral("wdir")] = p.windDirDeg;
            fo[QStringLiteral("wspd")] = p.windSpeedKts;
            fo[QStringLiteral("wgst")] = p.windGustKts;
            fo[QStringLiteral("visib")] = p.visibilitySm;
            fo[QStringLiteral("rawWx")] = p.rawPeriod;
            fcsts.append(fo);
        }
        obj[QStringLiteral("fcsts")] = fcsts;

        q.bindValue(QStringLiteral(":payload"), QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        q.bindValue(QStringLiteral(":fetched"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        q.exec();

        dbtools::closeDatabaseFile(&db);
    } catch (...) {}
}

void WeatherClient::requestAirportWeather(const QString& stationId, bool forceNetwork)
{
    QString cleanId = stationId.trimmed().toUpper();
    if (cleanId.isEmpty()) return;

    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!forceNetwork && m_lastRequestTimes.contains(cleanId)) {
        if (nowMs - m_lastRequestTimes.value(cleanId) < 15000) { // 15s debounce
            MetarInfo cachedM = getCachedMetar(cleanId);
            TafInfo cachedT = getCachedTaf(cleanId);
            if (cachedM.isValid() || cachedT.isValid()) {
                emit airportWeatherReady(cleanId, cachedM, cachedT);
                return;
            }
        }
    }
    m_lastRequestTimes.insert(cleanId, nowMs);

    // 1. Fetch METAR
    QUrl metarUrl(m_baseUrl + QStringLiteral("/metar?ids=") + cleanId + QStringLiteral("&format=json"));
    QNetworkRequest metarReq(metarUrl);
    metarReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.3 (open weather client)"));

    QNetworkReply *metarReply = m_netManager->get(metarReq);
    connect(metarReply, &QNetworkReply::finished, this, [this, metarReply, cleanId]() {
        MetarInfo metar;
        if (metarReply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(metarReply->readAll());
            QJsonArray arr = doc.array();
            if (!arr.isEmpty()) {
                metar = MetarInfo::fromJson(arr.first().toObject());
                saveMetarToCache(metar);
                emit metarReady(cleanId, metar);
            }
        }
        metarReply->deleteLater();

        // Emit combined signal with current TAF
        TafInfo currentTaf = getCachedTaf(cleanId);
        emit airportWeatherReady(cleanId, metar, currentTaf);
    });

    // 2. Fetch TAF
    QUrl tafUrl(m_baseUrl + QStringLiteral("/taf?ids=") + cleanId + QStringLiteral("&format=json"));
    QNetworkRequest tafReq(tafUrl);
    tafReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.3 (open weather client)"));

    QNetworkReply *tafReply = m_netManager->get(tafReq);
    connect(tafReply, &QNetworkReply::finished, this, [this, tafReply, cleanId]() {
        TafInfo taf;
        if (tafReply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(tafReply->readAll());
            QJsonArray arr = doc.array();
            if (!arr.isEmpty()) {
                taf = TafInfo::fromJson(arr.first().toObject());
                saveTafToCache(taf);
                emit tafReady(cleanId, taf);
            }
        }
        tafReply->deleteLater();

        // Emit combined signal with current METAR
        MetarInfo currentMetar = getCachedMetar(cleanId);
        emit airportWeatherReady(cleanId, currentMetar, taf);
    });
}

void WeatherClient::requestActiveSigmets(bool forceNetwork)
{
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!forceNetwork && m_lastRequestTimes.contains(QStringLiteral("SIGMETS"))) {
        if (nowMs - m_lastRequestTimes.value(QStringLiteral("SIGMETS")) < 60000) { // 1 min debounce
            QList<SigmetAdvisory> cached = getCachedSigmets();
            if (!cached.isEmpty()) {
                emit sigmetsReady(cached);
                return;
            }
        }
    }
    m_lastRequestTimes.insert(QStringLiteral("SIGMETS"), nowMs);

    QUrl url(m_baseUrl + QStringLiteral("/isigmet?format=geojson"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.3 (open weather client)"));

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray feats = doc.object().value(QStringLiteral("features")).toArray();
            QList<SigmetAdvisory> sigmets;
            for (const QJsonValue& f : feats) {
                sigmets.append(SigmetAdvisory::fromGeoJson(f.toObject()));
            }
            emit sigmetsReady(sigmets);
        } else {
            emit weatherRequestFailed(QStringLiteral("SIGMET"), reply->errorString());
        }
        reply->deleteLater();
    });
}

void WeatherClient::requestPireps(const QString& stationId, int distanceNm)
{
    QString cleanId = stationId.trimmed().toUpper();
    if (cleanId.isEmpty()) return;

    QUrl url(m_baseUrl + QStringLiteral("/pirep?id=") + cleanId + QStringLiteral("&distance=") + QString::number(distanceNm) + QStringLiteral("&format=json"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.3 (open weather client)"));

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray arr = doc.array();
            QList<PirepInfo> pireps;
            for (const QJsonValue& v : arr) {
                pireps.append(PirepInfo::fromJson(v.toObject()));
            }
            emit pirepsReady(pireps);
        }
        reply->deleteLater();
    });
}

} // namespace openairac
