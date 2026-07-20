//
// Created by matveak on 20.07.2026.
//

// You may need to build the project (run Qt uic code generator) to get "ui_modwindow.h" resolved

#include "modwindow.h"
#include "ui_modwindow.h"


modwindow::modwindow(QWidget *parent) : QWidget(parent), ui(new Ui::modwindow) {
    ui->setupUi(this);
}

modwindow::~modwindow() {
    delete ui;
}