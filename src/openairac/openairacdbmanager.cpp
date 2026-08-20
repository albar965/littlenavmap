/*****************************************************************************
* OpenAIRAC Map — OpenAIRAC Database Manager Implementation
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

#include "openairac/openairacdbmanager.h"
#include "fs/db/databasemeta.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "db/dbtools.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace openairac {

OpenAiracDbManager& OpenAiracDbManager::instance()
{
    static OpenAiracDbManager s_instance;
    return s_instance;
}

QString OpenAiracDbManager::computeSha256(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return QString::fromLatin1(hash.result().toHex());
    }
    return QString();
}

bool OpenAiracDbManager::validateDatabaseCompatibility(const QString& dbPath, QString* errorOut) const
{
    if (!QFile::exists(dbPath)) {
        if (errorOut) *errorOut = QStringLiteral("Database file does not exist: ") + dbPath;
        return false;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("OPENAIRAC_VALIDATE_TEMP"));
        dbtools::openDatabaseFile(&db, dbPath, true /* readonly */, false /* createSchema */);

        atools::fs::db::DatabaseMeta meta(&db);
        if (!meta.isValid()) {
            if (errorOut) *errorOut = QStringLiteral("Invalid metadata or missing metadata table");
            dbtools::closeDatabaseFile(&db);
            return false;
        }

        if (!meta.isDatabaseCompatible()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Database version ") + meta.getDatabaseVersion().getVersionString() +
                            QStringLiteral(" is incompatible with required schema v14.29");
            }
            dbtools::closeDatabaseFile(&db);
            return false;
        }

        // Check required tables
        atools::sql::SqlQuery q(&db);
        q.exec("SELECT COUNT(*) FROM airport");
        if (!q.next()) {
            if (errorOut) *errorOut = QStringLiteral("Failed to query airport table");
            dbtools::closeDatabaseFile(&db);
            return false;
        }

        dbtools::closeDatabaseFile(&db);
        return true;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

DatabaseStatusInfo OpenAiracDbManager::checkDatabaseStatus(const QString& dbPath) const
{
    DatabaseStatusInfo info;
    info.path = dbPath;

    if (!QFile::exists(dbPath)) {
        info.installed = false;
        info.valid = false;
        return info;
    }

    info.installed = true;
    QFileInfo fi(dbPath);
    info.sizeBytes = fi.size();
    info.sha256Hash = computeSha256(dbPath);

    try {
        atools::sql::SqlDatabase db(QStringLiteral("OPENAIRAC_STATUS_TEMP"));
        dbtools::openDatabaseFile(&db, dbPath, true /* readonly */, false /* createSchema */);

        atools::fs::db::DatabaseMeta meta(&db);
        if (meta.isValid()) {
            info.valid = meta.isDatabaseCompatible();
            info.cycle = meta.getAiracCycle();
            info.compiledDate = meta.getLastLoadTime();
            info.dataSource = meta.getDataSource();
            info.compilerVersion = meta.getCompilerVersion();
            info.majorVersion = meta.getMajorVersionDb();
            info.minorVersion = meta.getMinorVersionDb();
            info.hasSidStar = meta.hasSidStar();

            atools::sql::SqlQuery q(&db);
            if (q.exec("SELECT COUNT(*) FROM airport") && q.next()) info.airportCount = q.valueInt(0);
            if (q.exec("SELECT (SELECT COUNT(*) FROM vor) + (SELECT COUNT(*) FROM ndb)") && q.next()) info.navaidCount = q.valueInt(0);
            if (q.exec("SELECT COUNT(*) FROM airway") && q.next()) info.airwayCount = q.valueInt(0);
            if (q.exec("SELECT COUNT(*) FROM approach") && q.next()) info.approachCount = q.valueInt(0);
        } else {
            info.valid = false;
            info.lastError = QStringLiteral("Invalid database metadata");
        }

        dbtools::closeDatabaseFile(&db);
    } catch (const std::exception& e) {
        info.valid = false;
        info.lastError = QString::fromUtf8(e.what());
    }

    return info;
}

bool OpenAiracDbManager::atomicReplaceDatabase(
    const QString& candidatePath,
    const QString& targetPath,
    QString* errorOut
)
{
    // 1. Validate candidate database first
    QString valErr;
    if (!validateDatabaseCompatibility(candidatePath, &valErr)) {
        if (errorOut) *errorOut = QStringLiteral("Candidate validation failed: ") + valErr;
        emit databaseUpdateFailed(targetPath, valErr);
        return false;
    }

    QString backupPath = targetPath + QStringLiteral(".backup");

    // 2. Create backup of existing database if it exists
    if (QFile::exists(targetPath)) {
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }
        if (!QFile::copy(targetPath, backupPath)) {
            if (errorOut) *errorOut = QStringLiteral("Failed to create rollback backup at ") + backupPath;
            emit databaseUpdateFailed(targetPath, *errorOut);
            return false;
        }
    }

    // 3. Atomically replace target database
    QString tempTarget = targetPath + QStringLiteral(".tmp_swap");
    if (QFile::exists(tempTarget)) {
        QFile::remove(tempTarget);
    }

    if (!QFile::copy(candidatePath, tempTarget)) {
        if (errorOut) *errorOut = QStringLiteral("Failed to stage candidate database");
        emit databaseUpdateFailed(targetPath, *errorOut);
        return false;
    }

    // Remove old target and rename temp target
    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        if (errorOut) *errorOut = QStringLiteral("Failed to remove old target database");
        // Roll back
        rollbackDatabase(targetPath, nullptr);
        emit databaseUpdateFailed(targetPath, *errorOut);
        return false;
    }

    if (!QFile::rename(tempTarget, targetPath)) {
        if (errorOut) *errorOut = QStringLiteral("Failed to commit target database");
        // Roll back
        rollbackDatabase(targetPath, nullptr);
        emit databaseUpdateFailed(targetPath, *errorOut);
        return false;
    }

    // 4. Verify new target database
    if (!validateDatabaseCompatibility(targetPath, &valErr)) {
        if (errorOut) *errorOut = QStringLiteral("Post-install verification failed: ") + valErr;
        rollbackDatabase(targetPath, nullptr);
        emit databaseUpdateFailed(targetPath, *errorOut);
        return false;
    }

    DatabaseStatusInfo st = checkDatabaseStatus(targetPath);
    emit databaseUpdated(targetPath, st.cycle);
    return true;
}

bool OpenAiracDbManager::rollbackDatabase(const QString& targetPath, QString* errorOut)
{
    QString backupPath = targetPath + QStringLiteral(".backup");
    if (!QFile::exists(backupPath)) {
        if (errorOut) *errorOut = QStringLiteral("No backup file available for rollback: ") + backupPath;
        return false;
    }

    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }

    if (!QFile::copy(backupPath, targetPath)) {
        if (errorOut) *errorOut = QStringLiteral("Failed to restore from backup");
        return false;
    }

    return true;
}

} // namespace openairac
