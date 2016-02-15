#ifndef NAVMAPWIDGET_H
#define NAVMAPWIDGET_H

#include <marble/MarbleWidget.h>

#include <QWidget>
class QContextMenuEvent;
class NavMapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  NavMapWidget(QWidget *parent);

  // QWidget interface

};

#endif // NAVMAPWIDGET_H
