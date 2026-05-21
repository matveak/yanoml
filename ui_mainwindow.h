#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QPushButton *PlayButton;
    QPushButton *ModPlatformButton;
    QPushButton *UpdateButton;
    QPushButton *ElyByButton;
    QComboBox *UpdateBox;
    QPushButton *SettingsButton;
    QPushButton *PickAccountButton;
    QPushButton *InstallerButton;
    QComboBox *VersionBox;
    QMenuBar *menubar;
    QMenu *menulauncher;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1575, 943);

        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");

        // PlayButton
        PlayButton = new QPushButton(centralwidget);
        PlayButton->setObjectName("PlayButton");
        PlayButton->setGeometry(QRect(1110, 720, 461, 171));
        QFont font;
        font.setPointSize(72);
        PlayButton->setFont(font);
        PlayButton->setText("В БОЙ");

        // ModPlatformButton
        ModPlatformButton = new QPushButton(centralwidget);
        ModPlatformButton->setObjectName("ModPlatformButton");
        ModPlatformButton->setGeometry(QRect(1110, 0, 461, 111));
        ModPlatformButton->setText("Моды");

        // UpdateButton
        UpdateButton = new QPushButton(centralwidget);
        UpdateButton->setObjectName("UpdateButton");
        UpdateButton->setGeometry(QRect(1380, 110, 191, 71));
        UpdateButton->setText("Обновить");

        // ElyByButton
        ElyByButton = new QPushButton(centralwidget);
        ElyByButton->setObjectName("ElyByButton");
        ElyByButton->setGeometry(QRect(1320, 530, 251, 141));
        ElyByButton->setText("Ely.by");

        // UpdateBox
        UpdateBox = new QComboBox(centralwidget);
        UpdateBox->setObjectName("UpdateBox");
        UpdateBox->setGeometry(QRect(1110, 110, 271, 71));

        // SettingsButton
        SettingsButton = new QPushButton(centralwidget);
        SettingsButton->setObjectName("SettingsButton");
        SettingsButton->setGeometry(QRect(1320, 670, 251, 51));
        SettingsButton->setText("Настройки");

        // PickAccountButton
        PickAccountButton = new QPushButton(centralwidget);
        PickAccountButton->setObjectName("PickAccountButton");
        PickAccountButton->setGeometry(QRect(1110, 670, 211, 51));
        PickAccountButton->setText("Аккаунт");

        // InstallerButton
        InstallerButton = new QPushButton(centralwidget);
        InstallerButton->setObjectName("InstallerButton");
        InstallerButton->setGeometry(QRect(1110, 530, 211, 111));
        InstallerButton->setText("Установить");

        // VersionBox
        VersionBox = new QComboBox(centralwidget);
        VersionBox->setObjectName("VersionBox");
        VersionBox->setGeometry(QRect(1110, 640, 211, 31));

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1575, 21));
        menulauncher = new QMenu(menubar);
        menulauncher->setObjectName("menulauncher");
        menulauncher->setTitle("launcher");
        menubar->addAction(menulauncher->menuAction());
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        QMetaObject::connectSlotsByName(MainWindow);
    }
};

namespace Ui {
class MainWindow : public Ui_MainWindow {};
}

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
