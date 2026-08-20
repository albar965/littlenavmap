/*****************************************************************************
* OpenAIRAC Map — Charts Dock Panel Implementation
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

#include "openairac/charts/chartsdock.h"
#include "openairac/charts/chartclient.h"
#include "openairac/navigationprovider.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

namespace openairac {

ChartsDock::ChartsDock(QWidget *parent)
    : QDockWidget(tr("OpenAIRAC Charts"), parent)
{
    setObjectName(QStringLiteral("OpenAiracChartsDock"));
    setupUi();

    connect(&ChartClient::instance(), &ChartClient::chartReady, this, &ChartsDock::onChartAssetReady);
    connect(&ChartClient::instance(), &ChartClient::chartDownloadFailed, this, &ChartsDock::onChartAssetFailed);
}

void ChartsDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 1. Top Search Bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    m_airportSearch = new QLineEdit(container);
    m_airportSearch->setPlaceholderText(tr("Enter ICAO / IATA (e.g. KJFK, LFPG, KLAX)..."));
    m_airportSearch->setMaximumWidth(250);
    searchLayout->addWidget(m_airportSearch);

    m_searchButton = new QPushButton(tr("Find Charts"), container);
    searchLayout->addWidget(m_searchButton);

    m_downloadAllButton = new QPushButton(tr("Download All (Offline Pack)"), container);
    searchLayout->addWidget(m_downloadAllButton);

    m_autoSyncCheck = new QCheckBox(tr("Auto-Sync Map Selection"), container);
    m_autoSyncCheck->setChecked(true);
    searchLayout->addWidget(m_autoSyncCheck);
    searchLayout->addStretch();

    mainLayout->addLayout(searchLayout);

    // 2. Navdata vs Charts distinction status bar
    m_navdataStatusLabel = new QLabel(container);
    m_navdataStatusLabel->setTextFormat(Qt::RichText);
    m_navdataStatusLabel->setStyleSheet(QStringLiteral("background: palette(alternate-base); padding: 4px; border: 1px solid palette(mid); font-size: 11px;"));
    mainLayout->addWidget(m_navdataStatusLabel);

    // 3. Category Filter TabBar
    m_categoryFilterBar = new QTabBar(container);
    m_categoryFilterBar->addTab(tr("All Charts"));
    m_categoryFilterBar->addTab(tr("Airport"));
    m_categoryFilterBar->addTab(tr("Departure (SID)"));
    m_categoryFilterBar->addTab(tr("Arrival (STAR)"));
    m_categoryFilterBar->addTab(tr("Approach"));
    m_categoryFilterBar->addTab(tr("Minima & Info"));
    mainLayout->addWidget(m_categoryFilterBar);

    // 4. Pinned Charts Bar
    m_pinnedTabBar = new QTabBar(container);
    m_pinnedTabBar->setTabsClosable(true);
    m_pinnedTabBar->setVisible(false);
    mainLayout->addWidget(m_pinnedTabBar);

    // 5. Main Splitter View (Chart Tree on left, Viewer on right)
    m_splitter = new QSplitter(Qt::Horizontal, container);

    m_chartTree = new QTreeWidget(m_splitter);
    m_chartTree->setHeaderLabels(QStringList() << tr("Title") << tr("RWY") << tr("Src") << tr("Status"));
    m_chartTree->setColumnWidth(0, 220);
    m_chartTree->setColumnWidth(1, 60);
    m_chartTree->setColumnWidth(2, 50);
    m_chartTree->setColumnWidth(3, 70);
    m_chartTree->setRootIsDecorated(true);
    m_splitter->addWidget(m_chartTree);

    m_viewer = new ChartViewerWidget(m_splitter);
    m_splitter->addWidget(m_viewer);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(m_splitter);

    setWidget(container);

    // Connect signals
    connect(m_searchButton, &QPushButton::clicked, this, &ChartsDock::onSearchTriggered);
    connect(m_airportSearch, &QLineEdit::returnPressed, this, &ChartsDock::onSearchTriggered);
    connect(m_downloadAllButton, &QPushButton::clicked, this, &ChartsDock::onDownloadAllClicked);
    connect(m_chartTree, &QTreeWidget::itemClicked, this, &ChartsDock::onChartItemClicked);
    connect(m_categoryFilterBar, &QTabBar::currentChanged, this, &ChartsDock::onCategoryFilterChanged);
    connect(m_pinnedTabBar, &QTabBar::currentChanged, this, &ChartsDock::onPinnedTabClicked);
    connect(m_pinnedTabBar, &QTabBar::tabCloseRequested, this, [this](int idx) {
        if (idx >= 0 && idx < m_pinnedCharts.size()) {
            m_pinnedCharts.removeAt(idx);
            updatePinnedTabBar();
        }
    });

    setAirport(QStringLiteral("KJFK"));
}

void ChartsDock::setAirport(const QString& airportIcao)
{
    QString clean = airportIcao.trimmed().toUpper();
    if (clean.isEmpty()) return;

    m_currentAirport = clean;
    m_airportSearch->setText(clean);
    m_currentCharts = ChartClient::instance().getChartsForAirport(clean);

    updateNavdataStatusLabel();
    populateChartTree();

    // Auto-select Airport Diagram if available
    for (const ChartEntry& c : m_currentCharts) {
        if (c.category == ChartCategory::AirportDiagram) {
            selectChartById(c.id);
            break;
        }
    }
}

void ChartsDock::updateNavdataStatusLabel()
{
    int totalCharts = m_currentCharts.size();
    QString icao = m_currentAirport;

    QString text = tr("<b>%1</b>: <b>%2</b> published charts available.").arg(icao).arg(totalCharts);

    if (icao.startsWith(QLatin1String("LF")) || icao == QStringLiteral("LFPG")) {
        text += tr(" | <i>OpenAIRAC Navdata Note: Public DGAC/SIA dataset contains 0 machine-readable terminal procedures; official eAIP charts are provided separately.</i>");
    } else if (icao.startsWith(QLatin1Char('K'))) {
        text += tr(" | <i>OpenAIRAC Navdata: Full FAA CIFP machine-readable procedures and FAA d-TPP charts active.</i>");
    }

    m_navdataStatusLabel->setText(text);
}

void ChartsDock::populateChartTree()
{
    m_chartTree->clear();
    if (m_currentCharts.isEmpty()) {
        QTreeWidgetItem *emptyItem = new QTreeWidgetItem(m_chartTree);
        emptyItem->setText(0, tr("No charts published for %1").arg(m_currentAirport));
        return;
    }

    int currentTab = m_categoryFilterBar->currentIndex();

    // Create category nodes
    QHash<ChartCategory, QTreeWidgetItem*> categoryNodes;
    auto getOrCreateNode = [this, &categoryNodes](ChartCategory cat, const QString& title) -> QTreeWidgetItem* {
        if (categoryNodes.contains(cat)) return categoryNodes.value(cat);
        QTreeWidgetItem *node = new QTreeWidgetItem(m_chartTree);
        node->setText(0, title);
        node->setExpanded(true);
        categoryNodes.insert(cat, node);
        return node;
    };

    for (const ChartEntry& c : m_currentCharts) {
        // Filter by category tab
        if (currentTab == 1 && c.category != ChartCategory::AirportDiagram) continue;
        if (currentTab == 2 && c.category != ChartCategory::Departure) continue;
        if (currentTab == 3 && c.category != ChartCategory::Arrival) continue;
        if (currentTab == 4 && c.category != ChartCategory::Approach) continue;
        if (currentTab == 5 && (c.category != ChartCategory::Minima && c.category != ChartCategory::General)) continue;

        QTreeWidgetItem *parentNode = nullptr;
        if (currentTab == 0) {
            parentNode = getOrCreateNode(c.category, c.categoryName());
        } else {
            parentNode = m_chartTree->invisibleRootItem();
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(parentNode);
        item->setText(0, c.title);
        item->setText(1, c.runway.isEmpty() ? QStringLiteral("-") : c.runway);
        item->setText(2, c.providerBadge());
        item->setText(3, c.isCached ? tr("✓ Cached") : tr("⬇ Cloud"));
        item->setData(0, Qt::UserRole, c.id);
    }
}

void ChartsDock::selectChartById(const QString& chartId)
{
    for (const ChartEntry& c : m_currentCharts) {
        if (c.id == chartId) {
            if (ChartClient::instance().isChartCached(c)) {
                m_viewer->loadChart(c, ChartClient::instance().getCachedFilePath(c));
            } else {
                m_viewer->showLoading(c);
                ChartClient::instance().fetchChartAsset(c);
            }
            break;
        }
    }
}

void ChartsDock::selectChartForProcedure(const QString& airportIcao, QChar kind, const QString& procedureIdent, const QString& runway)
{
    if (m_currentAirport != airportIcao.trimmed().toUpper()) {
        setAirport(airportIcao);
    }

    QList<ProcedureChartMatch> matches = ChartClient::instance().matchProcedureCharts(m_currentAirport, kind, procedureIdent, runway);
    if (!matches.isEmpty()) {
        selectChartById(matches.first().chartId);
        pinChart(matches.first().chart);
    }
}

void ChartsDock::pinChart(const ChartEntry& chart)
{
    for (const ChartEntry& p : m_pinnedCharts) {
        if (p.id == chart.id) return;
    }
    m_pinnedCharts.append(chart);
    updatePinnedTabBar();
}

void ChartsDock::updatePinnedTabBar()
{
    m_pinnedTabBar->clear();
    for (const ChartEntry& c : m_pinnedCharts) {
        m_pinnedTabBar->addTab(QStringLiteral("[") + c.providerBadge() + QStringLiteral("] ") + c.displayTitle());
    }
    m_pinnedTabBar->setVisible(!m_pinnedCharts.isEmpty());
}

void ChartsDock::onSearchTriggered()
{
    setAirport(m_airportSearch->text());
}

void ChartsDock::onChartItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    QString chartId = item->data(0, Qt::UserRole).toString();
    if (!chartId.isEmpty()) {
        selectChartById(chartId);
    }
}

void ChartsDock::onCategoryFilterChanged(int /*tabIndex*/)
{
    populateChartTree();
}

void ChartsDock::onPinnedTabClicked(int tabIndex)
{
    if (tabIndex >= 0 && tabIndex < m_pinnedCharts.size()) {
        selectChartById(m_pinnedCharts.at(tabIndex).id);
    }
}

void ChartsDock::onDownloadAllClicked()
{
    ChartClient::instance().downloadAllForAirport(m_currentAirport);
    QMessageBox::information(this, tr("Offline Pack"), tr("Downloading all published charts for %1 into local cache.").arg(m_currentAirport));
}

void ChartsDock::onChartAssetReady(const QString& chartId, const QString& localPath)
{
    if (m_viewer->currentChart().id == chartId) {
        m_viewer->loadChart(m_viewer->currentChart(), localPath);
    }
    // Update status in tree
    for (int i = 0; i < m_chartTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = m_chartTree->topLevelItem(i);
        for (int j = 0; j < top->childCount(); ++j) {
            QTreeWidgetItem *child = top->child(j);
            if (child->data(0, Qt::UserRole).toString() == chartId) {
                child->setText(3, tr("✓ Cached"));
            }
        }
    }
}

void ChartsDock::onChartAssetFailed(const QString& chartId, const QString& errorString)
{
    if (m_viewer->currentChart().id == chartId) {
        m_viewer->showError(m_viewer->currentChart().displayTitle(), errorString);
    }
}

} // namespace openairac
