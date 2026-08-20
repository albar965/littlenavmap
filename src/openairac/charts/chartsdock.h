/*****************************************************************************
* OpenAIRAC Map — Charts Dock Panel
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

#ifndef OPENAIRAC_CHARTSDOCK_H
#define OPENAIRAC_CHARTSDOCK_H

#include "openairac/charts/chartmodel.h"
#include "openairac/charts/chartviewerwidget.h"
#include <QDockWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTabBar>
#include <QPushButton>
#include <QCheckBox>
#include <QSplitter>
#include <QLabel>

namespace openairac {

class ChartsDock : public QDockWidget {
    Q_OBJECT

public:
    explicit ChartsDock(QWidget *parent = nullptr);
    virtual ~ChartsDock() override = default;

    void setAirport(const QString& airportIcao);
    void selectChartById(const QString& chartId);
    void selectChartForProcedure(const QString& airportIcao, QChar kind, const QString& procedureIdent, const QString& runway = QString());

    void pinChart(const ChartEntry& chart);

public slots:
    void onSearchTriggered();
    void onChartItemClicked(QTreeWidgetItem *item, int column);
    void onCategoryFilterChanged(int tabIndex);
    void onPinnedTabClicked(int tabIndex);
    void onDownloadAllClicked();
    void onChartAssetReady(const QString& chartId, const QString& localPath);
    void onChartAssetFailed(const QString& chartId, const QString& errorString);

private:
    QString m_currentAirport;
    QList<ChartEntry> m_currentCharts;
    QList<ChartEntry> m_pinnedCharts;

    // UI Widgets
    QLineEdit *m_airportSearch = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_downloadAllButton = nullptr;
    QCheckBox *m_autoSyncCheck = nullptr;
    QLabel *m_navdataStatusLabel = nullptr;

    QTabBar *m_categoryFilterBar = nullptr;
    QTabBar *m_pinnedTabBar = nullptr;

    QSplitter *m_splitter = nullptr;
    QTreeWidget *m_chartTree = nullptr;
    ChartViewerWidget *m_viewer = nullptr;

    void setupUi();
    void populateChartTree();
    void updateNavdataStatusLabel();
    void updatePinnedTabBar();
};

} // namespace openairac

#endif // OPENAIRAC_CHARTSDOCK_H
