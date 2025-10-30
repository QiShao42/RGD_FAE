#include "arona.h"
#include "./ui_arona.h"

arona::arona(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::arona)
{
    ui->setupUi(this);
}

arona::~arona()
{
    delete ui;
}
