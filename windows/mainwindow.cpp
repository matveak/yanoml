#include "mainwindow.h"
#include "createmodpackwindow.h"
#include "curseforgewindow.h"
#include "modwindow.h"

// Предполагается, что эти заголовки существуют, так как классы используются:
// #include "settingswindow.h"
// #include "windowframe.h"
// #include "minecraftdownloader.h"
// #include "launcher.h"
// #include "solitairegame.h"

#include <QProcess>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QTextCursor>
#include <QComboBox>
#include <QProgressBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>
#include <QTimer>
#include <QSet>
#include <algorithm>
#include <memory>
#include <QMessageBox>
#include <QDir>
#include <QThread>
#include <QApplication>
#include <QClipboard>


// ==================== MainWindow ====================

void MainWindow::setupUI() {
    if (this->objectName().isEmpty())
        this->setObjectName("MainWindow");

    this->resize(1280, 720);
    this->setMinimumSize(900, 600);

    // Убираем системную рамку и заголовок окна
    setWindowFlags(Qt::FramelessWindowHint);

    // Остальной код без изменений...

    // Создаем WindowFrame и используем его как основной контейнер
    frame = new WindowFrame(this);
    frame->setTitle("launcher");

    // Получаем контент-виджет из фрейма
    QWidget* contentWidget = frame->contentWidget();
    auto* root = new QVBoxLayout(contentWidget);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ─── Тёмная тема главного окна ─────────────────────────────────────
    this->setStyleSheet(QString(R"(
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

        QPushButton#SolitaireGameButton:hover  { background-color: #1D4ED8; }

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

    // ─── Виджеты ────────────────────────────────────────────────────────
    centralwidget = new QWidget(this);
    centralwidget->setObjectName("centralwidget");

    QHBoxLayout *rootLayout = new QHBoxLayout(centralwidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Левая область (фон / скриншот мира)
    QWidget *bgWidget = new QWidget();
    bgWidget->setObjectName("bgWidget");
    bgWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bgWidget->setStyleSheet("background-color: #0e1014;");

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

    // ── Ряд 2: Модпаки и игры ────────────────────────────────────────────
    ModpackButton = new QPushButton("Создать сборку");
    ModpackButton->setObjectName("ModpackButton");
    ModpackButton->setMinimumHeight(40);
    rightPanelLayout->addWidget(ModpackButton);

    SolitaireGameButton = new QPushButton("SolitaireGame");
    SolitaireGameButton->setObjectName("SolitaireGameButton");
    SolitaireGameButton->setMinimumHeight(40);
    rightPanelLayout->addWidget(SolitaireGameButton);

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

    LoaderBox = new QComboBox();
    LoaderBox->setObjectName("LoaderBox");
    LoaderBox->addItems({"Vanilla", "Fabric", "Forge", "NeoForge"});
    LoaderBox->setMinimumHeight(32);
    rightPanelLayout->addWidget(LoaderBox);
    connect(LoaderBox, &QComboBox::currentTextChanged, this, &MainWindow::onLoaderChanged);

    QLabel *versionLbl = new QLabel("Версия Minecraft");
    versionLbl->setStyleSheet("color:#9CA3AF; font-size:11px;");
    rightPanelLayout->addWidget(versionLbl);

    VersionBox = new QComboBox();
    VersionBox->setObjectName("VersionBox");
    VersionBox->setMinimumHeight(32);
    rightPanelLayout->addWidget(VersionBox);

    rightPanelLayout->addWidget(makeSep());

    // ─ Установить + Ely.by ───────────────────────────────────────────────
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

    // ─ Аккаунт + Настройки ───────────────────────────────────────────────
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
    QFont pf;
    pf.setPointSize(30);
    pf.setBold(true);
    PlayButton->setFont(pf);
    rightPanelLayout->addWidget(PlayButton);

    rootLayout->addWidget(rightPanel);

    // Добавляем centralwidget в contentWidget фрейма
    root->addWidget(centralwidget);

    // Устанавливаем frame как central widget
    this->setCentralWidget(frame);

    // Меню (опционально - можно закомментировать если не нужно)
    /*
    menubar = new QMenuBar(this);
    menubar->setObjectName("menubar");
    menulauncher = new QMenu("launcher", menubar);
    menulauncher->setObjectName("menulauncher");
    menubar->addAction(menulauncher->menuAction());
    this->setMenuBar(menubar);
    */

    // Status bar (опционально)
    /*
    statusbar = new QStatusBar(this);
    statusbar->setObjectName("statusbar");
    this->setStatusBar(statusbar);
    */

    QMetaObject::connectSlotsByName(this);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUI();

    // progressBar добавляем в правую панель (над кнопкой В БОЙ)
    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFixedHeight(10);
    progressBar->hide();
    rightPanelLayout->insertWidget(rightPanelLayout->count() - 1, progressBar);

    downloader = new MinecraftDownloader(this);
    settingsWindow = new SettingsWindow(this);

    setupTrayIcon();
    setupConnections();
    loadVersions();
}
// ==================== MainWindow ====================

// ==================== Tray ====================

void MainWindow::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/minecraft.png"));

    if (trayIcon->icon().isNull())
        trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));

    QMenu* trayMenu = new QMenu(this);
    trayMenu->addAction("Открыть лаунчер", this, &MainWindow::show);
    trayMenu->addAction("Выход", this, &QWidget::close);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::DoubleClick)
                    this->show();
            });
}

// ==================== Connections ====================

void MainWindow::setupConnections()
{
    connect(downloader, &MinecraftDownloader::downloadProgress, this,
            [this](qint64 received, qint64 total)
            {
                if (total <= 0) return;
                progressBar->show();
                int percent = static_cast<int>((received * 100) / total);
                progressBar->setValue(percent);
            });

    connect(downloader, &MinecraftDownloader::vanillaVersionsReceived,
            this, &MainWindow::onVanillaVersionsReceived);
    connect(downloader, &MinecraftDownloader::fabricVersionsReceived,
            this, &MainWindow::onFabricVersionsReceived);
    connect(downloader, &MinecraftDownloader::forgeVersionsReceived,
            this, &MainWindow::onForgeVersionsReceived);
    connect(downloader, &MinecraftDownloader::neoforgeVersionReceived,
            this, &MainWindow::onNeoForgeVersionReceived);

    connect(SolitaireGameButton, &QPushButton::clicked, this, &MainWindow::on_solitaireGameButton_clicked);

    connect(downloader, &MinecraftDownloader::instanceCreated, this,
            [this](const QString& path)
            {
                // При установке с загрузчиком базовое сообщение пропускаем —
                // финальное покажем после установки самого загрузчика.
                if (m_modLoaderPending)
                    return;
                progressBar->hide();
                QMessageBox::information(this, "Готово", "Игра установлена:\n" + path);
            });

    connect(downloader, &MinecraftDownloader::errorOccurred, this,
            [this](const QString& error)
            {
                QMessageBox::warning(this, "Ошибка", error);
            });

    connect(downloader, &MinecraftDownloader::totalProgress, this,
            [this](int percent)
            {
                progressBar->show();
                progressBar->setValue(percent);
            });

    connect(downloader, &MinecraftDownloader::javaRuntimeProgress, this,
            [this](int percent)
            {
                progressBar->show();
                progressBar->setValue(percent);
            });
}

void MainWindow::on_solitaireGameButton_clicked() {
    // Исправлена утечка памяти: создаем окно только если его еще нет
    if (!m_game) {
        m_game = new SolitaireGame(this);
        m_game->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_game, &QObject::destroyed, this, [this]() { m_game = nullptr; });
    }
    m_game->show();
    m_game->raise();
    m_game->activateWindow();
}

// ==================== Installer ====================

void MainWindow::on_InstallerButton_clicked()
{
    QString versionText = VersionBox->currentText();
    QString loader = LoaderBox->currentText().toLower();

    QString cleanVersion = versionText;
    cleanVersion.remove("Fabric ");
    cleanVersion.remove("Forge ");
    cleanVersion.remove("NeoForge ");

    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    const bool modded = (loader != "vanilla" && !loader.isEmpty());

    progressBar->setValue(0);
    progressBar->show();

    if (!modded)
    {
        QMessageBox::information(this, "Установка",
                                 "Начато скачивание " + versionText);
        return;
    }

    // С загрузчиком: сначала ставим ванильную базу, затем сам загрузчик.
    m_modLoaderPending = true;

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::instanceCreated, this,
                    [this, conn, loader, cleanVersion, gameDir](const QString&)
                    {
                        disconnect(*conn);
                        startLoaderInstall(loader, cleanVersion, gameDir);
                    });

    QMessageBox::information(this, "Установка",
                             "Начато скачивание " + versionText +
                             ".\nПосле базовой версии будет установлен загрузчик "
                             + loader + ".");
}

void MainWindow::startLoaderInstall(const QString& loader,
                                    const QString& mcVersion,
                                    const QString& gameDir)
{
    progressBar->setValue(0);
    progressBar->show();

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::loaderInstalled, this,
                    [this, conn](const QString& versionId)
                    {
                        disconnect(*conn);
                        m_modLoaderPending = false;
                        progressBar->hide();
                        QMessageBox::information(this, "Готово",
                                                 "Загрузчик установлен:\n" + versionId);
                    });

    if (loader == "fabric")
    {
        downloader->installFabric(mcVersion, gameDir);
    }
    else // forge / neoforge — нужен Java для запуска установщика
    {
        launcher->ensureJava(mcVersion, gameDir,
                             [this, loader, mcVersion, gameDir](const QString& javaExe)
                             {
                                 progressBar->hide();
                                 downloader->installForgeLike(mcVersion, loader, javaExe, gameDir);
                             }, settingsWindow->javaPath());
    }
}

// ==================== Play ====================

void MainWindow::on_PlayButton_clicked()
{
    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    QString loader = LoaderBox ? LoaderBox->currentText().toLower() : "vanilla";

    QString versionText = VersionBox->currentText();
    QString version = versionText;
    version.remove("Fabric ");
    version.remove("Forge ");
    version.remove("NeoForge ");

    // ── Modded: запуск через профиль установленного загрузчика ──────────────
    if (loader != "vanilla" && !loader.isEmpty())
    {
        QString versionId =
            MinecraftDownloader::findInstalledLoaderId(gameDir, loader, version);

        if (versionId.isEmpty())
        {
            QMessageBox::warning(this, "Ошибка",
                                 "Загрузчик " + loader + " для " + version +
                                 " не установлен.\nНажмите «Установить».");
            return;
        }

        QString childPath =
            gameDir + "/versions/" + versionId + "/" + versionId + ".json";
        QFile childFile(childPath);
        if (!childFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "Ошибка",
                                 "Не удалось открыть профиль загрузчика:\n" + childPath);
            return;
        }
        QJsonObject childRoot =
            QJsonDocument::fromJson(childFile.readAll()).object();
        childFile.close();

        QString parentVersion = childRoot["inheritsFrom"].toString();
        if (parentVersion.isEmpty()) parentVersion = version;

        QString parentPath =
            gameDir + "/versions/" + parentVersion + "/" + parentVersion + ".json";
        QFile parentFile(parentPath);
        if (!parentFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "Ошибка",
                                 "Базовая версия " + parentVersion + " не установлена.");
            return;
        }
        QJsonObject parentRoot =
            QJsonDocument::fromJson(parentFile.readAll()).object();
        parentFile.close();

        launcher->ensureJava(parentVersion, gameDir,
                             [this, parentRoot, childRoot, gameDir, parentVersion, versionId](const QString& javaExe)
                             {
                                 progressBar->hide();
                                 int requiredMajor =
                                         parentRoot["javaVersion"].toObject()["majorVersion"].toInt();
                                 launcher->launchModded(parentRoot, childRoot, gameDir, parentVersion,
                                                        versionId, javaExe, settingsWindow->username(), settingsWindow->ramAmount(), requiredMajor);
                             }, settingsWindow->javaPath());
        return;
    }

    // ── Vanilla ─────────────────────────────────────────────────────────────
    QString versionDir = gameDir + "/versions/" + version;
    QString jsonPath   = versionDir + "/" + version + ".json";

    if (!QFileInfo::exists(jsonPath))
    {
        QMessageBox::warning(this, "Ошибка", "Версия не установлена:\n" + jsonPath);
        return;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть version.json");
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QString mainClass = root["mainClass"].toString();
    if (mainClass.isEmpty())
    {
        QMessageBox::warning(this, "Ошибка", "mainClass не найден в version.json");
        return;
    }

    launcher->ensureJava(version, gameDir,
                         [this, root, gameDir, version, versionDir, mainClass]
                 (const QString& javaExe)
                         {
                             progressBar->hide();
                             int requiredMajor = root["javaVersion"].toObject()["majorVersion"].toInt();
                            launcher->launchGame(root, gameDir, version, versionDir, mainClass,
                                                  javaExe, settingsWindow->username(), settingsWindow->ramAmount(), requiredMajor);
                         }, settingsWindow->javaPath());
}

// ==================== Crash Dialog ====================

void MainWindow::showCrashDialog(int neededJava, const QString& javaPath)
{
    show();
    activateWindow();
    raise();

    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle("Minecraft — ошибка запуска");
    dlg->resize(900, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* lay = new QVBoxLayout(dlg);

    QLabel* hint = new QLabel(dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-weight: bold; color: #c0392b;");

    // Анализируем лог и даём понятную подсказку
    QString hintText = launcher->getCrashHint(neededJava, javaPath);
    hint->setText(hintText);
    lay->addWidget(hint);

    QTextEdit* logEdit = new QTextEdit(dlg);
    logEdit->setReadOnly(true);
    logEdit->setFont(QFont("Courier New", 9));
    logEdit->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    logEdit->setPlainText(launcher->crashLog);
    // Прокрутить к концу — там обычно самое важное
    logEdit->moveCursor(QTextCursor::End);
    lay->addWidget(logEdit, 1);

    QHBoxLayout* btnRow = new QHBoxLayout();

    QPushButton* copyBtn = new QPushButton("📋 Копировать лог", dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [this]()
            {
                QApplication::clipboard()->setText(launcher->crashLog);
            });

    QPushButton* closeBtn = new QPushButton("Закрыть", dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    dlg->exec();
}

// ==================== Other buttons ====================

void MainWindow::on_ModPlatformButton_clicked()
{
    ModWindow* window = new ModWindow(this);
    window->setSettingsWindow(settingsWindow);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->exec();
}

void MainWindow::on_UpdateButton_clicked()
{
    QMessageBox::information(this, "Обновление", "Проверка обновлений...");
}

void MainWindow::on_ElyByButton_clicked() {}

void MainWindow::on_SettingsButton_clicked()
{
    settingsWindow->exec();
}

void MainWindow::on_PickAccountButton_clicked()
{
    QMessageBox::information(this, "Аккаунт", "Выбор аккаунта...");
}

// ==================== Versions ====================

void MainWindow::onLoaderChanged(const QString& loader)
{
    VersionBox->clear();

    if (loader == "Vanilla")        downloader->md.fetchVanillaVersions();
    else if (loader == "Fabric")    downloader->md.fetchFabricVersions();
    else if (loader == "Forge")     downloader->md.fetchForgeVersions();
    else if (loader == "NeoForge")  downloader->md.fetchNeoForgeVersions();
}

void MainWindow::onShowSnapshotsChanged(int)
{
    VersionBox->clear();
    VersionBox->addItem("Обновление списка...");
    downloader->md.fetchVanillaVersions();
}

void MainWindow::onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions)
{
    VersionBox->clear();

    QVector<MinecraftVersion> sorted = versions;
    std::sort(sorted.begin(), sorted.end(),
              [](const MinecraftVersion& a, const MinecraftVersion& b) {
                  return QVersionNumber::fromString(a.gameVersion)
                         > QVersionNumber::fromString(b.gameVersion);
              });

    bool showSnapshots = settingsWindow && settingsWindow->showSnapshots();

    for (const auto& ver : sorted)
    {
        if (!showSnapshots && ver.loaderType != "release") continue;
        VersionBox->addItem(ver.gameVersion);
    }
}

void MainWindow::onFabricVersionsReceived(const QJsonArray& versions)
{
    VersionBox->clear();
    for (const auto& value : versions)
    {
        QJsonObject obj = value.toObject();
        if (!settingsWindow->showSnapshots() && !obj["stable"].toBool()) continue;
        VersionBox->addItem("Fabric " + obj["version"].toString());
    }
}

void MainWindow::onForgeVersionsReceived(const QJsonObject& json)
{
    VersionBox->clear();
    QSet<QString> mcVersions;
    QJsonObject promos = json["promos"].toObject();

    for (auto it = promos.begin(); it != promos.end(); ++it)
        mcVersions.insert(it.key().section('-', 0, 0));

    QStringList versions = mcVersions.values();
    std::sort(versions.begin(), versions.end(),
              [](const QString& a, const QString& b) {
                  return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
              });

    for (const QString& v : versions)
        VersionBox->addItem("Forge " + v);
}

// Преобразует версию NeoForge из Maven в версию Minecraft.
// Новая схема (1.20.2+): MAJOR.MINOR.PATCH -> 1.MAJOR.MINOR (при MINOR == 0 это
// 1.MAJOR, напр. 21.0.x -> 1.21). Версии вида 47.x.x — это NeoForge для 1.20.1
// (ответвление от Forge 47).
static QString neoForgeToMcVersion(const QString& neoVersion)
{
    if (neoVersion.startsWith("47."))
        return "1.20.1";

    const QStringList parts = neoVersion.split('.');
    if (parts.size() >= 2)
    {
        bool okMajor = false, okMinor = false;
        const int major = parts[0].toInt(&okMajor);
        const int minor = parts[1].section('-', 0, 0).toInt(&okMinor);
        if (okMajor && okMinor)
            return minor == 0 ? QString("1.%1").arg(major)
                              : QString("1.%1.%2").arg(major).arg(minor);
    }
    return neoVersion;
}

void MainWindow::onNeoForgeVersionReceived(const QString& xml)
{
    VersionBox->clear();
    QStringList lines = xml.split('\n');
    QSet<QString> added;
    QStringList mcVersions;

    for (const QString& line : lines)
    {
        if (!line.contains("<version>")) continue;

        QString version = QString(line)
                              .remove("<version>")
                              .remove("</version>")
                              .trimmed();

        if ((version.contains("beta",  Qt::CaseInsensitive) ||
             version.contains("alpha", Qt::CaseInsensitive) ||
             version.contains("rc",    Qt::CaseInsensitive))
            && !settingsWindow->showSnapshots())
            continue;

        const QString mcVersion = neoForgeToMcVersion(version);

        if (!added.contains(mcVersion))
        {
            added.insert(mcVersion);
            mcVersions << mcVersion;
        }
    }

    std::sort(mcVersions.begin(), mcVersions.end(),
              [](const QString& a, const QString& b) {
                  return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
              });

    for (const QString& v : mcVersions)
        VersionBox->addItem(v);
}

void MainWindow::loadVersions()
{
    // LoaderBox теперь инициализирован в setupUI, хак больше не нужен
    VersionBox->clear();
    VersionBox->addItem("Загрузка версий...");
    downloader->md.fetchVanillaVersions();
}

void MainWindow::on_CurseForgeButton_clicked()
{
    CurseForgeWindow* w = new CurseForgeWindow(this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setSettingsWindow(settingsWindow);
    w->exec();
}

void MainWindow::on_ModpackButton_clicked()
{
    CreateModpackWindow* w = new CreateModpackWindow(this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setSettingsWindow(settingsWindow);
    MinecraftDownloader* dlForModpack = new MinecraftDownloader(w);
    w->setDownloader(dlForModpack);
    connect(dlForModpack, &MinecraftDownloader::instanceCreated, this,
            [this](const QString& path) {
                QMessageBox::information(this, "Сборка создана",
                                         "Сборка успешно создана:\n" + path);
            });
    w->exec();
}