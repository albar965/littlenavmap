/*****************************************************************************
* OpenAIRAC Map — OpenAIRAC Database Manager
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

#ifndef OPENAIRAC_OPENAIRACDBMANAGER_H
#define OPENAIRAC_OPENAIRACDBMANAGER_H

#include <QString>
#include <QDateTime>
#include <QObject>

namespace openairac {

struct DatabaseStatusInfo {
    bool installed = false;
    bool valid = false;
    QString path;
    QString cycle;
    QDateTime compiledDate;
    qint64 sizeBytes = 0;
    QString sha256Hash;
    QString dataSource;
    QString compilerVersion;
    int majorVersion = 0;
    int minorVersion = 0;
    bool hasSidStar = false;
    int airportCount = 0;
    int navaidCount = 0;
    int airwayCount = 0;
    int approachCount = 0;
    QString lastError;
};

class OpenAiracDbManager : public QObject {
    Q_OBJECT

public:
    static OpenAiracDbManager& instance();

    DatabaseStatusInfo checkDatabaseStatus(const QString& dbPath) const;

    /**
     * Atomically replace the target database with a new candidate database.
     * Creates a rollback backup (.backup) before replacing.
     * If validation or replacement fails, rolls back to original database.
     */
    bool atomicReplaceDatabase(
        const QString& candidatePath,
        const QString& targetPath,
        QString* errorOut = nullptr
    );

    /**
     * Roll back target database from its .backup file.
     */
    bool rollbackDatabase(const QString& targetPath, QString* errorOut = nullptr);

    /**
     * Validate schema compatibility against v14.29.
     */
    bool validateDatabaseCompatibility(const QString& dbPath, QString* errorOut = nullptr) const;

    QString computeSha256(const QString& filePath) const;

signals:
    void databaseUpdated(const QString& path, const QString& cycle);
    void databaseUpdateFailed(const QString& path, const QString& error);

private:
    OpenAiracDbManager() = default;
};

} // namespace openairac

#endif // OPENAIRAC_OPENAIRACDBMANAGER_H
