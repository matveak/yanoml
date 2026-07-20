//
// Created by matveak on 20.07.2026.
//

#ifndef YANOML_MODWINDOW_H
#define YANOML_MODWINDOW_H

#include <QWidget>


QT_BEGIN_NAMESPACE

namespace Ui {
    class modwindow;
}

QT_END_NAMESPACE

class modwindow : public QWidget {
    Q_OBJECT

public:
    explicit modwindow(QWidget *parent = nullptr);

    ~modwindow() override;

private:
    Ui::modwindow *m_ui;
};


#endif //YANOML_MODWINDOW_H