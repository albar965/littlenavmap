/*****************************************************************************
* OpenAIRAC Map — Chart Viewer Widget
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

#ifndef OPENAIRAC_CHARTVIEWERWIDGET_H
#define OPENAIRAC_CHARTVIEWERWIDGET_H

#include "openairac/charts/chartmodel.h"
#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QToolBar>
#include <QAction>
#include <QProgressBar>

namespace openairac {

class ChartViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChartViewerWidget(QWidget *parent = nullptr);
    virtual ~ChartViewerWidget() override = default;

    void loadChart(const ChartEntry& chart, const QString& localFilePath);
    void showLoading(const ChartEntry& chart);
    void showError(const QString& chartTitle, const QString& errorMessage);
    void clear();

    const ChartEntry& currentChart() const { return m_currentChart; }

public slots:
    void zoomIn();
    void zoomOut();
    void fitWidth();
    void fitPage();
    void rotateLeft();
    void rotateRight();
    void toggleNightMode();
    void openExternal();
    void printChart();

private:
    ChartEntry m_currentChart;
    QString m_localPath;
    double m_scaleFactor = 1.0;
    int m_rotationDegrees = 0;
    bool m_nightMode = false;

    // UI elements
    QLabel *m_headerLabel = nullptr;
    QLabel *m_outdatedWarning = nullptr;
    QToolBar *m_toolBar = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_imageLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QAction *m_actionZoomIn = nullptr;
    QAction *m_actionZoomOut = nullptr;
    QAction *m_actionFitWidth = nullptr;
    QAction *m_actionFitPage = nullptr;
    QAction *m_actionRotateLeft = nullptr;
    QAction *m_actionRotateRight = nullptr;
    QAction *m_actionNightMode = nullptr;
    QAction *m_actionExternal = nullptr;
    QAction *m_actionPrint = nullptr;

    void setupUi();
    void updateImageDisplay();
    void updateHeaderInfo();
};

} // namespace openairac

#endif // OPENAIRAC_CHARTVIEWERWIDGET_H
