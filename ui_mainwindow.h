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
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include "darktheme.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget      *centralwidget;
    QPushButton  *PlayButton;
    QPushButton  *ModPlatformButton;
    QPushButton  *CurseForgeButton;  // ← CurseForge
    QPushButton  *UpdateButton;
    QPushButton  *ElyByButton;
    QComboBox    *UpdateBox;         // используется как LoaderBox
    QPushButton  *SettingsButton;
    QPushButton  *PickAccountButton;
    QPushButton  *InstallerButton;
    QPushButton  *ModpackButton;
    QComboBox    *VersionBox;
    QMenuBar     *menubar;
    QMenu        *menulauncher;
    QStatusBar   *statusbar;

    QVBoxLayout  *rightPanelLayout;  // для добавления progressBar из MainWindow

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1280, 720);
        MainWindow->setMinimumSize(900, 600);

        // ─── Тёмная тема главного окна ──────────────────────────────────────
        MainWindow->setStyleSheet(QString(R"(
            QMainWindow, #centralwidget { background-color: #1a1c20; }
            QWidget { color: #E8EAED; font-family: "Segoe UI", Arial, sans-serif; }
            QPushButton {
                background-color: #26292F; color: #E8EAED;
                border: 1px solid #3A3E45; border-radius: 8px;
                padding: 8px 14px; font-size: 13px;
            }
            QPushButton:hover  { background-color: #3A3E45; border-color: #1BD96A; }
            QPushButton:pressed { background-color: #1BD96A; color: #0A0A0A; }
            QPushButton#PlayButton {
                background-color: #1BD96A; color: #0A0A0A;
                font-size: 32px; font-weight: bold;
                border-radius: 12px; border: none;
            }
            QPushButton#PlayButton:hover  { background-color: #15C25E; }
            QPushButton#PlayButton:pressed { background-color: #0FA34E; }
            QPushButton#InstallerButton {
                background-color: #2563EB; color: #fff;
                border: none; font-weight: bold;
            }
            QPushButton#InstallerButton:hover  { background-color: #1D4ED8; }
            QPushButton#ModpackButton {
                background-color: #7C3AED; color: #fff;
                border: none; font-weight: bold;
            }
            QPushButton#ModpackButton:hover  { background-color: #6D28D9; }
            QPushButton#ModPlatformButton {
                background-color: #16A34A; color: #fff;
                border: none; font-weight: bold;
            }
            QPushButton#ModPlatformButton:hover { background-color: #15803D; }
            QPushButton#CurseForgeButton {
                background-color: #F16436; color: #fff;
                border: none; font-weight: bold;
            }
            QPushButton#CurseForgeButton:hover { background-color: #D95A2D; }
            QPushButton#ElyByButton {
                background-color: #0F7ADF; color: #fff;
                border: none; font-weight: bold;
            }
            QComboBox {
                background-color: #26292F; color: #E8EAED;
                border: 1px solid #3A3E45; border-radius: 6px;
                padding: 4px 10px; min-height: 28px;
            }
            QComboBox:hover { border-color: #1BD96A; }
            QComboBox::drop-down { border: none; }
            QComboBox QAbstractItemView {
                background-color: #26292F; color: #E8EAED;
                selection-background-color: #1BD96A; selection-color: #0A0A0A;
            }
            QMenuBar { background-color: #16181C; color: #E8EAED; }
            QMenuBar::item:selected { background-color: #26292F; }
            QMenu { background-color: #26292F; color: #E8EAED; border: 1px solid #3A3E45; }
            QMenu::item:selected { background-color: #1BD96A; color: #0A0A0A; }
            QStatusBar { background-color: #16181C; color: #9CA3AF; }
            QFrame#rightPanel { background-color: #16181C; border-left: 1px solid #2C2F36; }
            QProgressBar {
                background: #26292F; border: none; border-radius: 6px; height: 10px;
            }
            QProgressBar::chunk { background: #1BD96A; border-radius: 6px; }
        )"));

        // ─── Виджеты ─────────────────────────────────────────────────────────
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");

        QHBoxLayout *rootLayout = new QHBoxLayout(centralwidget);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // Левая область (фон / скриншот мира)
        QWidget *bgWidget = new QWidget();
        bgWidget->setObjectName("bgWidget");
        bgWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        bgWidget->setStyleSheet("background-color: #0e1014;");

        // Лого по центру фона
        QVBoxLayout *bgLayout = new QVBoxLayout(bgWidget);
        QLabel *bgLogo = new QLabel("");
        bgLogo->setStyleSheet("font-size: 96px;");
        bgLogo->setAlignment(Qt::AlignCenter);
        QLabel *bgSub = new QLabel("Launcher");
        bgSub->setStyleSheet("font-size: 28px; font-weight: bold; color: #1BD96A;");
        bgSub->setAlignment(Qt::AlignCenter);
        bgLayout->addStretch(1);
        bgLayout->addWidget(bgLogo);
        bgLayout->addWidget(bgSub);
        bgLayout->addStretch(1);

        rootLayout->addWidget(bgWidget, 1);

        // ── Правая панель ─────────────────────────────────────────────────────
        QFrame *rightPanel = new QFrame();
        rightPanel->setObjectName("rightPanel");
        rightPanel->setFixedWidth(390);

        rightPanelLayout = new QVBoxLayout(rightPanel);
        rightPanelLayout->setContentsMargins(16, 16, 16, 16);
        rightPanelLayout->setSpacing(10);

        // Заголовок
        QLabel *title = new QLabel("");
        title->setStyleSheet("font-size:22px; font-weight:bold; color:#1BD96A;");
        title->setAlignment(Qt::AlignCenter);
        rightPanelLayout->addWidget(title);

        // ── Ряд 1: Modrinth + CurseForge ─────────────────────────────────────
        QHBoxLayout *platformRow = new QHBoxLayout();
        platformRow->setSpacing(8);

        ModPlatformButton = new QPushButton("Modrinth");
        ModPlatformButton->setObjectName("ModPlatformButton");
        ModPlatformButton->setMinimumHeight(42);
        ModPlatformButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        platformRow->addWidget(ModPlatformButton);

        CurseForgeButton = new QPushButton("CurseForge");
        CurseForgeButton->setObjectName("CurseForgeButton");
        CurseForgeButton->setMinimumHeight(42);
        CurseForgeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        platformRow->addWidget(CurseForgeButton);

        rightPanelLayout->addLayout(platformRow);

        // ── Ряд 2: Модпаки ────────────────────────────────────────────────────
        ModpackButton = new QPushButton("📦 Создать сборку");
        ModpackButton->setObjectName("ModpackButton");
        ModpackButton->setMinimumHeight(40);
        rightPanelLayout->addWidget(ModpackButton);

        // ── Разделитель ───────────────────────────────────────────────────────
        auto makeSep = [&]() {
            QFrame* sep = new QFrame();
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color:#2C2F36; background:#2C2F36; max-height:1px;");
            return sep;
        };
        rightPanelLayout->addWidget(makeSep());

        // ── Загрузчик ─────────────────────────────────────────────────────────
        QLabel *loaderLbl = new QLabel("Загрузчик");
        loaderLbl->setStyleSheet("color:#9CA3AF; font-size:11px;");
        rightPanelLayout->addWidget(loaderLbl);

        UpdateBox = new QComboBox();   // выступает как LoaderBox
        UpdateBox->setObjectName("UpdateBox");
        UpdateBox->setMinimumHeight(32);
        rightPanelLayout->addWidget(UpdateBox);

        QLabel *versionLbl = new QLabel("Версия Minecraft");
        versionLbl->setStyleSheet("color:#9CA3AF; font-size:11px;");
        rightPanelLayout->addWidget(versionLbl);

        VersionBox = new QComboBox();
        VersionBox->setObjectName("VersionBox");
        VersionBox->setMinimumHeight(32);
        rightPanelLayout->addWidget(VersionBox);

        rightPanelLayout->addWidget(makeSep());

        // ── Установить + Ely.by ───────────────────────────────────────────────
        QHBoxLayout *installRow = new QHBoxLayout();
        installRow->setSpacing(8);

        InstallerButton = new QPushButton("Установить");
        InstallerButton->setObjectName("InstallerButton");
        InstallerButton->setMinimumHeight(46);
        InstallerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        installRow->addWidget(InstallerButton);

        ElyByButton = new QPushButton("Ely.by");
        ElyByButton->setObjectName("ElyByButton");
        ElyByButton->setMinimumHeight(46);
        ElyByButton->setFixedWidth(80);
        installRow->addWidget(ElyByButton);

        rightPanelLayout->addLayout(installRow);

        rightPanelLayout->addWidget(makeSep());

        // ── Аккаунт + Настройки ───────────────────────────────────────────────
        QHBoxLayout *accountRow = new QHBoxLayout();
        accountRow->setSpacing(8);

        PickAccountButton = new QPushButton("Аккаунт");
        PickAccountButton->setObjectName("PickAccountButton");
        PickAccountButton->setMinimumHeight(36);
        PickAccountButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        accountRow->addWidget(PickAccountButton);

        SettingsButton = new QPushButton("Настройки");
        SettingsButton->setObjectName("SettingsButton");
        SettingsButton->setMinimumHeight(36);
        SettingsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        accountRow->addWidget(SettingsButton);

        rightPanelLayout->addLayout(accountRow);

        UpdateButton = new QPushButton("Проверить обновления");
        UpdateButton->setObjectName("UpdateButton");
        UpdateButton->setMinimumHeight(32);
        rightPanelLayout->addWidget(UpdateButton);

        rightPanelLayout->addStretch(1);

        // ── Кнопка В БОЙ ─────────────────────────────────────────────────────
        PlayButton = new QPushButton("В БОЙ");
        PlayButton->setObjectName("PlayButton");
        PlayButton->setMinimumHeight(86);
        QFont pf; pf.setPointSize(30); pf.setBold(true);
        PlayButton->setFont(pf);
        rightPanelLayout->addWidget(PlayButton);

        rootLayout->addWidget(rightPanel);
        MainWindow->setCentralWidget(centralwidget);

        // ── Меню ──────────────────────────────────────────────────────────────
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menulauncher = new QMenu("launcher", menubar);
        menulauncher->setObjectName("menulauncher");
        menubar->addAction(menulauncher->menuAction());
        MainWindow->setMenuBar(menubar);

        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        QMetaObject::connectSlotsByName(MainWindow);
    }
};

namespace Ui { class MainWindow : public Ui_MainWindow {}; }

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H