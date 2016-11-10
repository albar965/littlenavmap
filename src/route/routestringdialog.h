#ifndef ROUTESTRINGDIALOG_H
#define ROUTESTRINGDIALOG_H

#include <QDialog>

namespace Ui {
class RouteStringDialog;
}

class RouteStringDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RouteStringDialog(QWidget *parent = 0);
  ~RouteStringDialog();

private:
  Ui::RouteStringDialog *ui;
};

#endif // ROUTESTRINGDIALOG_H
