/*****************************************************************************
* OpenAIRAC Map — Chart Viewer Widget Implementation
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

#include "openairac/charts/chartviewerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QMessageBox>

namespace openairac {

ChartViewerWidget::ChartViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ChartViewerWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    // 1. Header Info Bar
    m_headerLabel = new QLabel(this);
    m_headerLabel->setTextFormat(Qt::RichText);
    m_headerLabel->setStyleSheet(QStringLiteral("background: palette(window); padding: 4px; font-weight: bold;"));
    mainLayout->addWidget(m_headerLabel);

    // Outdated warning banner (hidden by default)
    m_outdatedWarning = new QLabel(this);
    m_outdatedWarning->setTextFormat(Qt::RichText);
    m_outdatedWarning->setStyleSheet(QStringLiteral("background: #ffe6e6; color: #cc0000; border: 1px solid #ff9999; padding: 4px; font-weight: bold;"));
    m_outdatedWarning->setText(tr("⚠️ WARNING: This chart belongs to an older or expired AIRAC cycle. Verify current navigation notices."));
    m_outdatedWarning->setVisible(false);
    mainLayout->addWidget(m_outdatedWarning);

    // 2. Toolbar
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(18, 18));

    m_actionZoomIn = m_toolBar->addAction(tr("Zoom +"), this, &ChartViewerWidget::zoomIn);
    m_actionZoomOut = m_toolBar->addAction(tr("Zoom -"), this, &ChartViewerWidget::zoomOut);
    m_actionFitWidth = m_toolBar->addAction(tr("Fit Width"), this, &ChartViewerWidget::fitWidth);
    m_actionFitPage = m_toolBar->addAction(tr("Fit Page"), this, &ChartViewerWidget::fitPage);
    m_toolBar->addSeparator();

    m_actionRotateLeft = m_toolBar->addAction(tr("Rotate ↶"), this, &ChartViewerWidget::rotateLeft);
    m_actionRotateRight = m_toolBar->addAction(tr("Rotate ↷"), this, &ChartViewerWidget::rotateRight);
    m_actionNightMode = m_toolBar->addAction(tr("Night Mode"), this, &ChartViewerWidget::toggleNightMode);
    m_actionNightMode->setCheckable(true);
    m_toolBar->addSeparator();

    m_actionExternal = m_toolBar->addAction(tr("Open in PDF Reader"), this, &ChartViewerWidget::openExternal);
    m_actionPrint = m_toolBar->addAction(tr("Print"), this, &ChartViewerWidget::printChart);

    mainLayout->addWidget(m_toolBar);

    // Progress bar for downloads
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // 3. Scroll area & display canvas
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setBackgroundRole(QPalette::Dark);
    m_scrollArea->setWidgetResizable(true);

    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setText(tr("Select an airport and chart to display."));
    m_imageLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 14px;"));
    m_scrollArea->setWidget(m_imageLabel);

    mainLayout->addWidget(m_scrollArea);
}

void ChartViewerWidget::loadChart(const ChartEntry& chart, const QString& localFilePath)
{
    m_currentChart = chart;
    m_localPath = localFilePath;
    m_scaleFactor = 1.0;
    m_rotationDegrees = 0;
    m_progressBar->setVisible(false);

    updateHeaderInfo();
    updateImageDisplay();
}

void ChartViewerWidget::showLoading(const ChartEntry& chart)
{
    m_currentChart = chart;
    m_localPath.clear();
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_imageLabel->setText(tr("Downloading chart: %1 ...").arg(chart.displayTitle()));
    updateHeaderInfo();
}

void ChartViewerWidget::showError(const QString& chartTitle, const QString& errorMessage)
{
    m_progressBar->setVisible(false);
    m_imageLabel->setText(tr("<h3>Failed to load chart</h3><p>%1</p><p><font color='red'>%2</font></p>").arg(chartTitle, errorMessage));
}

void ChartViewerWidget::clear()
{
    m_currentChart = ChartEntry();
    m_localPath.clear();
    m_headerLabel->clear();
    m_outdatedWarning->setVisible(false);
    m_progressBar->setVisible(false);
    m_imageLabel->setText(tr("No chart selected."));
}

void ChartViewerWidget::updateHeaderInfo()
{
    if (m_currentChart.id.isEmpty()) {
        m_headerLabel->clear();
        m_outdatedWarning->setVisible(false);
        return;
    }

    QString badge = QStringLiteral("<span style='background: #0a4b78; color: white; padding: 2px 6px; border-radius: 3px;'>[") +
                    m_currentChart.providerBadge() + QStringLiteral("]</span> ");

    QString cycleBadge = QStringLiteral("<span style='background: #e0e0e0; color: #333333; padding: 2px 6px; border-radius: 3px;'>AIRAC ") +
                         m_currentChart.airacCycle + QStringLiteral("</span>");

    QString rwyText = m_currentChart.runway.isEmpty() ? QString() : (QStringLiteral(" — Runway: <b>") + m_currentChart.runway + QStringLiteral("</b>"));

    m_headerLabel->setText(
        badge + QStringLiteral("<b>") + m_currentChart.airportIcao + QStringLiteral("</b>: ") +
        m_currentChart.displayTitle() + rwyText + QStringLiteral("  ") + cycleBadge
    );

    m_outdatedWarning->setVisible(m_currentChart.isOutdated);
}

void ChartViewerWidget::updateImageDisplay()
{
    if (m_localPath.isEmpty() || !QFile::exists(m_localPath)) {
        m_imageLabel->setText(tr("Chart document ready. Click 'Open in PDF Reader' to view."));
        return;
    }

    // Display document preview / status banner
    QString details;
    details += QStringLiteral("<h2>") + m_currentChart.displayTitle() + QStringLiteral("</h2>");
    details += QStringLiteral("<p><b>Airport:</b> ") + m_currentChart.airportIcao + QStringLiteral("<br/>");
    details += QStringLiteral("<b>Provider:</b> ") + m_currentChart.attribution + QStringLiteral("<br/>");
    details += QStringLiteral("<b>AIRAC Cycle:</b> ") + m_currentChart.airacCycle + QStringLiteral("<br/>");
    details += QStringLiteral("<b>Local Asset:</b> ") + m_localPath + QStringLiteral("</p>");
    details += QStringLiteral("<p><a href='#open'>Click 'Open in PDF Reader' or use the toolbar button above to inspect high-resolution vector PDF plate.</a></p>");

    m_imageLabel->setText(details);
}

void ChartViewerWidget::zoomIn()
{
    m_scaleFactor *= 1.25;
    updateImageDisplay();
}

void ChartViewerWidget::zoomOut()
{
    m_scaleFactor *= 0.8;
    updateImageDisplay();
}

void ChartViewerWidget::fitWidth()
{
    m_scaleFactor = 1.0;
    updateImageDisplay();
}

void ChartViewerWidget::fitPage()
{
    m_scaleFactor = 1.0;
    updateImageDisplay();
}

void ChartViewerWidget::rotateLeft()
{
    m_rotationDegrees = (m_rotationDegrees - 90) % 360;
    updateImageDisplay();
}

void ChartViewerWidget::rotateRight()
{
    m_rotationDegrees = (m_rotationDegrees + 90) % 360;
    updateImageDisplay();
}

void ChartViewerWidget::toggleNightMode()
{
    m_nightMode = m_actionNightMode->isChecked();
    if (m_nightMode) {
        m_scrollArea->setStyleSheet(QStringLiteral("background-color: #1a1a1a; color: #dddddd;"));
    } else {
        m_scrollArea->setStyleSheet(QString());
    }
    updateImageDisplay();
}

void ChartViewerWidget::openExternal()
{
    if (!m_localPath.isEmpty() && QFile::exists(m_localPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_localPath));
    } else if (!m_currentChart.sourceUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_currentChart.sourceUrl));
    }
}

void ChartViewerWidget::printChart()
{
    if (m_localPath.isEmpty()) {
        QMessageBox::information(this, tr("Print Chart"), tr("No chart asset loaded to print."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        openExternal();
    }
}

} // namespace openairac
