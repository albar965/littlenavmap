/*****************************************************************************
* OpenAIRAC Map — Weather Client Interface
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

#ifndef OPENAIRAC_WEATHERCLIENT_H
#define OPENAIRAC_WEATHERCLIENT_H

#include "openairac/weather/weathermodel.h"
#include <QObject>
#include <QList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace openairac {

class WeatherClient : public QObject {
    Q_OBJECT

public:
    static WeatherClient& instance();

    QString cacheDatabasePath() const;
    void setCacheDatabasePath(const QString& path);

    MetarInfo getCachedMetar(const QString& stationId) const;
    TafInfo getCachedTaf(const QString& stationId) const;
    QList<SigmetAdvisory> getCachedSigmets() const;

    void requestAirportWeather(const QString& stationId, bool forceNetwork = false);
    void requestActiveSigmets(bool forceNetwork = false);
    void requestPireps(const QString& stationId, int distanceNm = 150);

signals:
    void metarReady(const QString& stationId, const openairac::MetarInfo& metar);
    void tafReady(const QString& stationId, const openairac::TafInfo& taf);
    void airportWeatherReady(const QString& stationId, const openairac::MetarInfo& metar, const openairac::TafInfo& taf);
    void sigmetsReady(const QList<openairac::SigmetAdvisory>& sigmets);
    void pirepsReady(const QList<openairac::PirepInfo>& pireps);
    void weatherRequestFailed(const QString& requestType, const QString& errorString);

private:
    WeatherClient();
    QString m_cacheDbPath;
    QString m_baseUrl = QStringLiteral("https://aviationweather.gov/api/data");
    QNetworkAccessManager *m_netManager = nullptr;

    QHash<QString, qint64> m_lastRequestTimes;

    void ensureCacheDatabase() const;
    void saveMetarToCache(const MetarInfo& metar) const;
    void saveTafToCache(const TafInfo& taf) const;
    void saveSigmetsToCache(const QList<SigmetAdvisory>& sigmets) const;
};

} // namespace openairac

#endif // OPENAIRAC_WEATHERCLIENT_H
