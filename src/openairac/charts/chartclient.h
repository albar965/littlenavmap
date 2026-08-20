/*****************************************************************************
* OpenAIRAC Map — Chart Client Interface
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

#ifndef OPENAIRAC_CHARTCLIENT_H
#define OPENAIRAC_CHARTCLIENT_H

#include "openairac/charts/chartmodel.h"
#include <QObject>
#include <QList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace openairac {

class ChartClient : public QObject {
    Q_OBJECT

public:
    static ChartClient& instance();

    QString catalogDatabasePath() const;
    void setCatalogDatabasePath(const QString& path);

    QString cacheDirectory() const;
    void setCacheDirectory(const QString& path);

    QList<ChartEntry> getChartsForAirport(const QString& airportIcao) const;
    QList<ProcedureChartMatch> matchProcedureCharts(const QString& airportIcao, QChar kind, const QString& procedureIdent, const QString& runwayHint = QString()) const;

    bool isChartCached(const ChartEntry& chart) const;
    QString getCachedFilePath(const ChartEntry& chart) const;

    void fetchChartAsset(const ChartEntry& chart);
    void downloadAllForAirport(const QString& airportIcao);

signals:
    void chartReady(const QString& chartId, const QString& localPath);
    void chartDownloadProgress(const QString& chartId, qint64 bytesReceived, qint64 bytesTotal);
    void chartDownloadFailed(const QString& chartId, const QString& errorString);
    void airportChartsUpdated(const QString& airportIcao);

private:
    ChartClient();
    QString m_catalogDbPath;
    QString m_cacheDir;
    QNetworkAccessManager *m_netManager = nullptr;
    QHash<QNetworkReply*, ChartEntry> m_pendingDownloads;

    void ensureDefaultCatalog() const;
};

} // namespace openairac

#endif // OPENAIRAC_CHARTCLIENT_H
