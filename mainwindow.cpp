#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <marble/MarbleWidget.h>

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  // Create a Marble QWidget without a parent
  Marble::MarbleWidget *mapWidget = new Marble::MarbleWidget(this);

  // Load the OpenStreetMap map
  mapWidget->setMapThemeId("earth/openstreetmap/openstreetmap.dgml");

  ui->gridLayout->replaceWidget(ui->widget, mapWidget);
}

MainWindow::~MainWindow()
{
  delete ui;
}
