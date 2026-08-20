/*****************************************************************************
* OpenAIRAC Map — Chart Client Implementation
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

#include "openairac/charts/chartclient.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "db/dbtools.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace openairac {

ChartClient& ChartClient::instance()
{
    static ChartClient s_instance;
    return s_instance;
}

ChartClient::ChartClient()
{
    m_netManager = new QNetworkAccessManager(this);

    // Default paths
    QString basePath = atools::settings::Settings::getPath();
    m_catalogDbPath = basePath + QDir::separator() + QStringLiteral("openairac_charts.sqlite");
    m_cacheDir = basePath + QDir::separator() + QStringLiteral("charts_cache");

    QDir().mkpath(m_cacheDir);
    ensureDefaultCatalog();
}

QString ChartClient::catalogDatabasePath() const
{
    return m_catalogDbPath;
}

void ChartClient::setCatalogDatabasePath(const QString& path)
{
    m_catalogDbPath = path;
}

QString ChartClient::cacheDirectory() const
{
    return m_cacheDir;
}

void ChartClient::setCacheDirectory(const QString& path)
{
    m_cacheDir = path;
    QDir().mkpath(m_cacheDir);
}

void ChartClient::ensureDefaultCatalog() const
{
    if (QFile::exists(m_catalogDbPath)) {
        return;
    }

    // Initialize catalog SQLite schema
    try {
        atools::sql::SqlDatabase db(QStringLiteral("CHARTS_INIT_TEMP"));
        dbtools::openDatabaseFile(&db, m_catalogDbPath, false /* readonly */, true /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.exec(
            "CREATE TABLE IF NOT EXISTS chart_documents ("
            "  id TEXT PRIMARY KEY NOT NULL,"
            "  provider_id TEXT NOT NULL,"
            "  airport_icao TEXT NOT NULL,"
            "  airport_iata TEXT,"
            "  chart_type TEXT NOT NULL,"
            "  provider_chart_type TEXT NOT NULL,"
            "  title TEXT NOT NULL,"
            "  procedure_name TEXT,"
            "  runway TEXT,"
            "  effective_from TEXT,"
            "  effective_to TEXT,"
            "  revision_date TEXT,"
            "  airac_cycle TEXT NOT NULL,"
            "  language TEXT,"
            "  source_url TEXT NOT NULL,"
            "  source_document_id TEXT,"
            "  license_policy TEXT NOT NULL,"
            "  attribution TEXT NOT NULL,"
            "  mime_type TEXT NOT NULL,"
            "  asset_sha256 TEXT,"
            "  file_size_bytes INTEGER,"
            "  georeference_status TEXT NOT NULL,"
            "  change_flag TEXT"
            ")"
        );

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize charts catalog database:" << e.what();
    }
}

QList<ChartEntry> ChartClient::getChartsForAirport(const QString& airportIcao) const
{
    QList<ChartEntry> result;
    QString cleanIcao = airportIcao.trimmed().toUpper();
    if (cleanIcao.isEmpty()) return result;

    if (!QFile::exists(m_catalogDbPath)) {
        return result;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("CHARTS_QUERY_TEMP"));
        dbtools::openDatabaseFile(&db, m_catalogDbPath, true /* readonly */, false /* createSchema */);

        atools::sql::SqlQuery q(&db);
        q.prepare(
            "SELECT id, provider_id, airport_icao, airport_iata, chart_type, provider_chart_type, "
            "       title, procedure_name, runway, effective_from, effective_to, revision_date, "
            "       airac_cycle, language, source_url, source_document_id, license_policy, "
            "       attribution, mime_type, asset_sha256, file_size_bytes, georeference_status, change_flag "
            "FROM chart_documents "
            "WHERE airport_icao = :icao OR airport_iata = :icao "
            "ORDER BY chart_type, title"
        );
        q.bindValue(QStringLiteral(":icao"), cleanIcao);
        q.exec();

        while (q.next()) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = q.valueStr(0);
            obj[QStringLiteral("provider_id")] = q.valueStr(1);
            obj[QStringLiteral("airport_icao")] = q.valueStr(2);
            obj[QStringLiteral("airport_iata")] = q.valueStr(3);
            obj[QStringLiteral("chart_type")] = q.valueStr(4);
            obj[QStringLiteral("provider_chart_type")] = q.valueStr(5);
            obj[QStringLiteral("title")] = q.valueStr(6);
            obj[QStringLiteral("procedure_name")] = q.valueStr(7);
            obj[QStringLiteral("runway")] = q.valueStr(8);
            obj[QStringLiteral("effective_from")] = q.valueStr(9);
            obj[QStringLiteral("effective_to")] = q.valueStr(10);
            obj[QStringLiteral("airac_cycle")] = q.valueStr(12);
            obj[QStringLiteral("source_url")] = q.valueStr(14);
            obj[QStringLiteral("source_document_id")] = q.valueStr(15);
            obj[QStringLiteral("license_policy")] = q.valueStr(16);
            obj[QStringLiteral("attribution")] = q.valueStr(17);
            obj[QStringLiteral("mime_type")] = q.valueStr(18);
            obj[QStringLiteral("georeference_status")] = q.valueStr(21);

            ChartEntry entry = ChartEntry::fromJson(obj);
            entry.isCached = isChartCached(entry);
            if (entry.isCached) {
                entry.localCachePath = getCachedFilePath(entry);
            }
            result.append(entry);
        }

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        qWarning() << "Error querying charts for airport:" << e.what();
    }

    return result;
}

QList<ProcedureChartMatch> ChartClient::matchProcedureCharts(
    const QString& airportIcao,
    QChar kind,
    const QString& procedureIdent,
    const QString& runwayHint
) const
{
    QList<ProcedureChartMatch> matches;
    QList<ChartEntry> airportCharts = getChartsForAirport(airportIcao);
    QString cleanProc = procedureIdent.trimmed().toUpper();
    QString cleanRwy = runwayHint.trimmed().toUpper();

    // Auto extract runway from procedure ident if hint was empty
    if (cleanRwy.isEmpty() && cleanProc.length() >= 2) {
        QString cand = cleanProc.mid(1);
        if (!cand.isEmpty() && cand.at(0).isDigit()) {
            cleanRwy = cand;
        }
    }

    for (const ChartEntry& chart : airportCharts) {
        QString titleUpper = chart.title.toUpper();
        bool matched = false;
        QString conf = QStringLiteral("Likely");
        QString reason;

        if (kind == QChar('F')) {
            // Approach
            if (chart.category == ChartCategory::Approach) {
                bool isIls = cleanProc.startsWith(QLatin1Char('I')) || cleanProc.startsWith(QLatin1String("ILS"));
                bool isRnav = cleanProc.startsWith(QLatin1Char('R')) || cleanProc.startsWith(QLatin1String("RNAV"));
                bool isVor = cleanProc.startsWith(QLatin1Char('V')) || cleanProc.startsWith(QLatin1String("VOR"));

                bool rwyMatches = (!cleanRwy.isEmpty() && (chart.runway == cleanRwy || titleUpper.contains(cleanRwy)));

                if (isIls && (titleUpper.contains(QStringLiteral("ILS")) || titleUpper.contains(QStringLiteral("LOC")))) {
                    if (rwyMatches || cleanRwy.isEmpty()) {
                        matched = true;
                        conf = rwyMatches ? QStringLiteral("Exact") : QStringLiteral("Likely");
                        reason = QStringLiteral("Matching ILS approach for procedure ") + cleanProc;
                    }
                } else if (isRnav && (titleUpper.contains(QStringLiteral("RNAV")) || titleUpper.contains(QStringLiteral("GPS")))) {
                    if (rwyMatches || cleanRwy.isEmpty()) {
                        matched = true;
                        conf = rwyMatches ? QStringLiteral("Exact") : QStringLiteral("Likely");
                        reason = QStringLiteral("Matching RNAV approach for procedure ") + cleanProc;
                    }
                } else if (isVor && titleUpper.contains(QStringLiteral("VOR"))) {
                    if (rwyMatches || cleanRwy.isEmpty()) {
                        matched = true;
                        conf = rwyMatches ? QStringLiteral("Exact") : QStringLiteral("Likely");
                        reason = QStringLiteral("Matching VOR approach for procedure ") + cleanProc;
                    }
                }
            }
        } else if (kind == QChar('D')) {
            // SID
            if (chart.category == ChartCategory::Departure) {
                QString namePrefix = cleanProc;
                while (!namePrefix.isEmpty() && namePrefix.at(namePrefix.length() - 1).isDigit()) {
                    namePrefix.chop(1);
                }
                if (!namePrefix.isEmpty() && titleUpper.contains(namePrefix)) {
                    matched = true;
                    conf = QStringLiteral("Exact");
                    reason = QStringLiteral("Matching SID departure on name ") + namePrefix;
                }
            }
        } else if (kind == QChar('E')) {
            // STAR
            if (chart.category == ChartCategory::Arrival) {
                QString namePrefix = cleanProc;
                while (!namePrefix.isEmpty() && namePrefix.at(namePrefix.length() - 1).isDigit()) {
                    namePrefix.chop(1);
                }
                if (!namePrefix.isEmpty() && titleUpper.contains(namePrefix)) {
                    matched = true;
                    conf = QStringLiteral("Exact");
                    reason = QStringLiteral("Matching STAR arrival on name ") + namePrefix;
                }
            }
        }

        if (matched) {
            ProcedureChartMatch m;
            m.procedureIdent = procedureIdent;
            m.procedureKind = kind;
            m.airportIcao = airportIcao;
            m.runway = chart.runway;
            m.chartId = chart.id;
            m.confidence = conf;
            m.matchReason = reason;
            m.chart = chart;
            matches.append(m);
        }
    }

    return matches;
}

QString ChartClient::getCachedFilePath(const ChartEntry& chart) const
{
    QString ext = chart.mimeType.contains(QStringLiteral("pdf")) ? QStringLiteral("pdf") : QStringLiteral("bin");
    QString filename = chart.sourceDocumentId.isEmpty() ? (chart.id + QStringLiteral(".") + ext) : chart.sourceDocumentId;
    return m_cacheDir + QDir::separator() + filename;
}

bool ChartClient::isChartCached(const ChartEntry& chart) const
{
    QString path = getCachedFilePath(chart);
    return QFile::exists(path) && QFileInfo(path).size() > 0;
}

void ChartClient::fetchChartAsset(const ChartEntry& chart)
{
    if (isChartCached(chart)) {
        emit chartReady(chart.id, getCachedFilePath(chart));
        return;
    }

    if (chart.sourceUrl.isEmpty()) {
        emit chartDownloadFailed(chart.id, QStringLiteral("No source URL available for chart"));
        return;
    }

    QNetworkRequest request(QUrl(chart.sourceUrl));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenAIRAC-Map/0.2 (open charts client)"));

    QNetworkReply *reply = m_netManager->get(request);
    m_pendingDownloads.insert(reply, chart);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 rcv, qint64 total) {
        if (m_pendingDownloads.contains(reply)) {
            emit chartDownloadProgress(m_pendingDownloads.value(reply).id, rcv, total);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!m_pendingDownloads.contains(reply)) {
            reply->deleteLater();
            return;
        }

        ChartEntry chart = m_pendingDownloads.take(reply);
        if (reply->error() != QNetworkReply::NoError) {
            emit chartDownloadFailed(chart.id, reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        if (data.isEmpty()) {
            emit chartDownloadFailed(chart.id, QStringLiteral("Downloaded empty file"));
            return;
        }

        // Validate PDF magic bytes
        if (chart.mimeType.contains(QStringLiteral("pdf")) && !data.startsWith("%PDF-")) {
            emit chartDownloadFailed(chart.id, QStringLiteral("Invalid PDF file signature"));
            return;
        }

        QString targetPath = getCachedFilePath(chart);
        QFile file(targetPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            emit chartReady(chart.id, targetPath);
        } else {
            emit chartDownloadFailed(chart.id, QStringLiteral("Failed to write to cache file: ") + targetPath);
        }
    });
}

void ChartClient::downloadAllForAirport(const QString& airportIcao)
{
    QList<ChartEntry> charts = getChartsForAirport(airportIcao);
    for (const ChartEntry& c : charts) {
        if (!isChartCached(c)) {
            fetchChartAsset(c);
        }
    }
}

} // namespace openairac
