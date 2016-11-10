#include "routestringdialog.h"
#include "ui_routestringdialog.h"

RouteStringDialog::RouteStringDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::RouteStringDialog)
{
  ui->setupUi(this);
}

RouteStringDialog::~RouteStringDialog()
{
  delete ui;
}
