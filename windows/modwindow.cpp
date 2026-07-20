//
// Created by matveak on 20.07.2026.
//

// You may need to build the project (run Qt uic code generator) to get "ui_modwindow.h" resolved

#include "modwindow.h"
#include "ui_modwindow.h"


modwindow::modwindow(QWidget *parent) : QWidget(parent), m_ui(new Ui::modwindow) {
    m_ui->setupUi(this);
}

modwindow::~modwindow() {
    delete m_ui;
}