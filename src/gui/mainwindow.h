#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class NavMapWidget;

class MainWindow :
  public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  void tableContextMenu(const QPoint& pos);

private:
  Ui::MainWindow *ui;
  NavMapWidget *mapWidget;
};

#endif // MAINWINDOW_H
