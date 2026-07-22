#include "mainwindow.h"
#include "modwindow.h"
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
#include <QFile>
#include <QScreen>
#include <QPainter>
#include <QPixmap>
#include <QAction>
#include <QColor>
#include "../settings.h"


// ==================== Helpers ====================

class MinecraftDownloader;
// Небольшая цветная иконка-«бейдж» с буквой (как значок платформы в селекторе)
static QIcon makePlatformIcon(const QColor &color, const QString &letter)
{
    constexpr int size = 20;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(0, 0, size, size, 6, 6);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);

    return {pixmap};
}

// ==================== MainWindow ====================

void MainWindow::setupUI() {
    if (this->objectName().isEmpty())
        this->setObjectName("MainWindow");

    // Адаптивные размеры на основе экрана
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();
    
    // Размеры окна: 80% экрана, минимум 900x600
    int windowWidth = qMax(900, static_cast<int>(screenWidth * 0.8));
    int windowHeight = qMax(600, static_cast<int>(screenHeight * 0.8));
    
    this->resize(windowWidth, windowHeight);
    this->setMinimumSize(900, 600);

    // Убираем системную рамку и заголовок окна
    setWindowFlags(Qt::FramelessWindowHint);

    // Создаем WindowFrame и используем его как основной контейнер
    m_frame = new WindowFrame(this);
    m_frame->setTitle("YANOML Launcher");

    // Получаем контент-виджет из фрейма
    QWidget* contentWidget = m_frame->contentWidget();
    auto* root = new QVBoxLayout(contentWidget);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ─── Загружаем стили ───────────────────────────────────────
    QFile styleFile(":/ui/styles.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        this->setStyleSheet(style);
        styleFile.close();
    }

    // ─── Виджеты ──────────────────────────────────────────────
    m_centralwidget = new QWidget(this);
    m_centralwidget->setObjectName("centralwidget");

    auto *rootLayout = new QHBoxLayout(m_centralwidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ─── ЛЕВАЯ ОБЛАСТЬ (фон / логотип) ────────────────────────
    auto *bgWidget = new QWidget();
    bgWidget->setObjectName("bgWidget");
    bgWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *bgLayout = new QVBoxLayout(bgWidget);
    bgLayout->setContentsMargins(0, 0, 0, 0);
    bgLayout->setSpacing(12);

    auto *bgLogo = new QLabel("🎮");
    bgLogo->setStyleSheet("font-size: 96px;");
    bgLogo->setAlignment(Qt::AlignCenter);
    
    auto *bgSub = new QLabel("YANOML Launcher");
    bgSub->setObjectName("titleLabel");
    bgSub->setStyleSheet("font-size: 28px; font-weight: bold; color: #1BD96A;");
    bgSub->setAlignment(Qt::AlignCenter);
    
    auto *bgDesc = new QLabel("Play Your Minecraft Adventure");
    bgDesc->setObjectName("subtitleLabel");
    bgDesc->setStyleSheet("font-size: 13px; color: #9CA3AF; font-weight: 400;");
    bgDesc->setAlignment(Qt::AlignCenter);

    bgLayout->addStretch(1);
    bgLayout->addWidget(bgLogo);
    bgLayout->addWidget(bgSub);
    bgLayout->addWidget(bgDesc);
    bgLayout->addStretch(1);

    rootLayout->addWidget(bgWidget, 1);

    // ─── ПРАВАЯ ПАНЕЛЬ (управление) ────────────────────────────
    auto *rightPanel = new QFrame();
    rightPanel->setObjectName("rightPanel");
    
    // Адаптивная ширина: 25-35% от ширины окна
    int rightPanelWidth = qBound(300, static_cast<int>(windowWidth * 0.28), 450);
    rightPanel->setFixedWidth(rightPanelWidth);

    m_rightPanelLayout = new QVBoxLayout(rightPanel);
    m_rightPanelLayout->setContentsMargins(16, 16, 16, 16);
    m_rightPanelLayout->setSpacing(10);

    // ─── Заголовок ─────────────────────────────────────────────
    auto *title = new QLabel("Launcher");
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);
    m_rightPanelLayout->addWidget(title);

    // ─── Разделитель ───────────────────────────────────────────
    auto makeSep = [&] {
        auto* sep = new QFrame();
        sep->setObjectName("separator");
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        return sep;
    };
    m_rightPanelLayout->addWidget(makeSep());

    // ─── Источник модов (слитая кнопка-селектор) ────────────────
    m_platformButton = new QPushButton(this);
    m_platformButton->setObjectName("PlatformButton");
    m_platformButton->setMinimumHeight(42);
    m_platformButton->setCursor(Qt::PointingHandCursor);
    m_platformButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_platformButton->setLayoutDirection(Qt::LeftToRight);

    auto *platformMenu = new QMenu(m_platformButton);
    platformMenu->setObjectName("PlatformMenu");

    QAction *modrinthAction = platformMenu->addAction(
        makePlatformIcon(QColor("#1BD96A"), "M"), "Modrinth");
    QAction *curseforgeAction = platformMenu->addAction(
        makePlatformIcon(QColor("#F16436"), "C"), "CurseForge");

    auto selectPlatform = [this](const QColor &color, const QString &letter, const QString &name) {
        m_platformButton->setIcon(makePlatformIcon(color, letter));
        m_platformButton->setText("  " + name);
    };

    connect(modrinthAction, &QAction::triggered, this, [this, selectPlatform] {
        selectPlatform(QColor("#1BD96A"), "M", "Modrinth");
        on_ModPlatformButton_clicked();
    });
    connect(curseforgeAction, &QAction::triggered, this, [this, selectPlatform] {
        selectPlatform(QColor("#F16436"), "C", "CurseForge");
        on_CurseForgeButton_clicked();
    });

    m_platformButton->setMenu(platformMenu);
    m_platformButton->setIconSize(QSize(20, 20));
    selectPlatform(QColor("#1BD96A"), "M", "Modrinth");

    m_rightPanelLayout->addWidget(m_platformButton);

    // ─── Ряд: Модпаки и игры ─────────────────────────────────
    auto *gamesRow = new QHBoxLayout();
    gamesRow->setSpacing(8);
    gamesRow->setContentsMargins(0, 0, 0, 0);

    m_modpackButton = new QPushButton("📦 Modpack");
    m_modpackButton->setObjectName("ModpackButton");
    m_modpackButton->setMinimumHeight(40);
    m_modpackButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    gamesRow->addWidget(m_modpackButton);

    m_rightPanelLayout->addLayout(gamesRow);

    m_rightPanelLayout->addWidget(makeSep());

    // ─── Загрузчик ─────────────────────────────────────────────
    auto *loaderLbl = new QLabel("Loader");
    loaderLbl->setObjectName("sectionLabel");
    m_rightPanelLayout->addWidget(loaderLbl);

    m_LoaderBox = new QComboBox();
    m_LoaderBox->setObjectName("LoaderBox");
    m_LoaderBox->addItems({"Vanilla", "Fabric", "Forge", "NeoForge"});
    m_LoaderBox->setMinimumHeight(32);
    m_rightPanelLayout->addWidget(m_LoaderBox);
    connect(m_LoaderBox, &QComboBox::currentTextChanged, this, &MainWindow::onLoaderChanged);

    auto versionLbl = new QLabel("Minecraft Version");
    versionLbl->setObjectName("sectionLabel");
    m_rightPanelLayout->addWidget(versionLbl);

    m_versionBox = new QComboBox();
    m_versionBox->setObjectName("VersionBox");
    m_versionBox->setMinimumHeight(32);
    m_rightPanelLayout->addWidget(m_versionBox);

    m_rightPanelLayout->addWidget(makeSep());

    // ─── Установить + Ely.by ───────────────────────────────────
    auto installRow = new QHBoxLayout();
    installRow->setSpacing(8);
    installRow->setContentsMargins(0, 0, 0, 0);

    m_installerButton = new QPushButton("⬇️ Install");
    m_installerButton->setObjectName("InstallerButton");
    m_installerButton->setMinimumHeight(46);
    m_installerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    installRow->addWidget(m_installerButton);

    m_elyByButton = new QPushButton("🇷🇺 Ely.by");
    m_elyByButton->setObjectName("ElyByButton");
    m_elyByButton->setMinimumHeight(46);
    m_elyByButton->setFixedWidth(90);
    installRow->addWidget(m_elyByButton);

    m_rightPanelLayout->addLayout(installRow);

    m_rightPanelLayout->addWidget(makeSep());

    // ─── Аккаунт + Настройки ───────────────────────────────────
    auto accountRow = new QHBoxLayout();
    accountRow->setSpacing(8);
    accountRow->setContentsMargins(0, 0, 0, 0);

    m_pickAccountButton = new QPushButton("👤 Account");
    m_pickAccountButton->setObjectName("PickAccountButton");
    m_pickAccountButton->setMinimumHeight(36);
    m_pickAccountButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    accountRow->addWidget(m_pickAccountButton);

    m_settingsButton = new QPushButton("⚙️ Settings");
    m_settingsButton->setObjectName("SettingsButton");
    m_settingsButton->setMinimumHeight(36);
    m_settingsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    accountRow->addWidget(m_settingsButton);

    m_rightPanelLayout->addLayout(accountRow);

    m_updateButton = new QPushButton("🔄 Check Updates");
    m_updateButton->setObjectName("UpdateButton");
    m_updateButton->setMinimumHeight(32);
    m_rightPanelLayout->addWidget(m_updateButton);

    m_rightPanelLayout->addStretch(1);

    // ─── Кнопка В БОЙ (PLAY) ───────────────────────────────────
    m_playButton = new QPushButton("▶️ PLAY");
    m_playButton->setObjectName("PlayButton");
    m_playButton->setMinimumHeight(86);
    QFont pf;
    pf.setPointSize(28);
    pf.setBold(true);
    m_playButton->setFont(pf);
    m_rightPanelLayout->addWidget(m_playButton);

    rootLayout->addWidget(rightPanel);

    // Добавляем centralwidget в contentWidget фрейма
    root->addWidget(m_centralwidget);

    // Устанавливаем frame как central widget
    this->setCentralWidget(m_frame);

    QMetaObject::connectSlotsByName(this);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_rightPanelLayout(nullptr) {
    setupUI();

    // progressBar добавляем в правую панель (над кнопкой Play)
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(10);
    m_progressBar->hide();
    m_rightPanelLayout->insertWidget(m_rightPanelLayout->count() - 1, m_progressBar);

    m_downloader = new MinecraftInstaller(this);
    m_settingsWindow = new SettingsWindow(this);

    setupTrayIcon();
    setupConnections();
    loadVersions();
}

// ==================== MainWindow ====================

// ==================== Tray ====================

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/minecraft.png"));

    if (m_trayIcon->icon().isNull())
        m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));

    auto* trayMenu = new QMenu(this);
    trayMenu->addAction("Open Launcher", this, &MainWindow::show);
    trayMenu->addAction("Exit", this, &QWidget::close);

    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::DoubleClick)
                    this->show();
            });
}

// ==================== Connections ====================

void MainWindow::setupConnections()
{
}

// ==================== Installer ====================

void MainWindow::on_InstallerButton_clicked()
{
    QString versionText = m_versionBox->currentText();
    QString loader = m_LoaderBox->currentText().toLower();

    QString cleanVersion = versionText;
    cleanVersion.remove("Fabric ");
    cleanVersion.remove("Forge ");
    cleanVersion.remove("NeoForge ");

    QString gameDir = globalSettings.minecraftPath;
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    const bool modded = (loader != "vanilla" && !loader.isEmpty());

    m_progressBar->setValue(0);
    m_progressBar->show();

    if (!modded)
    {
        QMessageBox::information(this, "Install",
                                 "Started downloading " + versionText);
        return;
    }

    m_modLoaderPending = true;

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_downloader, &MinecraftInstaller::instanceCreated, this,
                    [this, conn, loader, cleanVersion, gameDir](const QString&)
                    {
                        disconnect(*conn);
                        startLoaderInstall(loader, cleanVersion, gameDir);
                    });

    QMessageBox::information(this, "Install",
                             "Started downloading " + versionText +
                             ".\nAfter base version, loader " + loader + " will be installed.");
}

void MainWindow::startLoaderInstall(const QString& loader,
                                    const QString& mcVersion,
                                    const QString& gameDir)
{
    m_progressBar->setValue(0);
    m_progressBar->show();

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_downloader, &MinecraftDownloader::loaderInstalled, this,
                    [this, conn](const QString& versionId)
                    {
                        disconnect(*conn);
                        m_modLoaderPending = false;
                        m_progressBar->hide();
                        QMessageBox::information(this, "Done",
                                                 "Loader installed:\n" + versionId);
                    });

    if (loader == "fabric")
    {
        m_downloader->installFabric(mcVersion, gameDir);
    }
    else
    {
        m_launcher->ensureJava(mcVersion, gameDir,
                             [this, loader, mcVersion, gameDir](const QString& javaExe)
                             {
                                 m_progressBar->hide();
                                 m_downloader->installForgeLike(mcVersion, loader, javaExe, gameDir);
                             }, globalSettings.javaPath);
    }
}

// ==================== Play ====================

void MainWindow::on_PlayButton_clicked()
{
    QString gameDir = globalSettings.minecraftPath;
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    QString loader = m_LoaderBox ? m_LoaderBox->currentText().toLower() : "vanilla";

    QString versionText = m_versionBox->currentText();
    QString version = versionText;
    version.remove("Fabric ");
    version.remove("Forge ");
    version.remove("NeoForge ");

    if (loader != "vanilla" && !loader.isEmpty())
    {
        QString versionId =
            MinecraftDownloader::findInstalledLoaderId(gameDir, loader, version);

        if (versionId.isEmpty())
        {
            QMessageBox::warning(this, "Error",
                                 "Loader " + loader + " for " + version +
                                 " not installed.\nClick «Install».");
            return;
        }

        QString childPath =
            gameDir + "/versions/" + versionId + "/" + versionId + ".json";
        QFile childFile(childPath);
        if (!childFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "Error",
                                 "Cannot open loader profile:\n" + childPath);
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
            QMessageBox::warning(this, "Error",
                                 "Base version " + parentVersion + " not installed.");
            return;
        }
        QJsonObject parentRoot =
            QJsonDocument::fromJson(parentFile.readAll()).object();
        parentFile.close();

        m_launcher->ensureJava(parentVersion, gameDir,
                             [this, parentRoot, childRoot, gameDir, parentVersion, versionId](const QString& javaExe)
                             {
                                 m_progressBar->hide();
                                 int requiredMajor =
                                         parentRoot["javaVersion"].toObject()["majorVersion"].toInt();
                                 m_launcher->launchModded(parentRoot, childRoot, gameDir, parentVersion,
                                                        versionId, javaExe, globalSettings.m_username(), m_settingsWindow->m_ramAmount, requiredMajor);
                             }, globalSettings.javaPath);
        return;
    }

    QString versionDir = gameDir + "/versions/" + version;
    QString jsonPath   = versionDir + "/" + version + ".json";

    if (!QFileInfo::exists(jsonPath))
    {
        QMessageBox::warning(this, "Error", "Version not installed:\n" + jsonPath);
        return;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Error", "Cannot open version.json");
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QString mainClass = root["mainClass"].toString();
    if (mainClass.isEmpty())
    {
        QMessageBox::warning(this, "Error", "mainClass not found in version.json");
        return;
    }

    m_launcher->ensureJava(version, gameDir,
                         [this, root, gameDir, version, versionDir, mainClass]
                 (const QString& javaExe)
                         {
                             m_progressBar->hide();
                             int requiredMajor = root["javaVersion"].toObject()["majorVersion"].toInt();
                            m_launcher->launchGame(root, gameDir, version, versionDir, mainClass,
                                                  javaExe, globalSettings.username(), m_settingsWindow->ramAmount, requiredMajor);
                         }, globalSettings.javaPath);
}

// ==================== Crash Dialog ====================

void MainWindow::showCrashDialog(int neededJava, const QString& javaPath)
{
    show();
    activateWindow();
    raise();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Minecraft — Error");
    dlg->resize(900, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto lay = new QVBoxLayout(dlg);

    auto hint = new QLabel(dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-weight: bold; color: #c0392b;");

    QString hintText = m_launcher->getCrashHint(neededJava, javaPath);
    hint->setText(hintText);
    lay->addWidget(hint);

    auto logEdit = new QTextEdit(dlg);
    logEdit->setReadOnly(true);
    logEdit->setFont(QFont("Courier New", 9));
    logEdit->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    logEdit->setPlainText(m_launcher->m_crashLog);
    logEdit->moveCursor(QTextCursor::End);
    lay->addWidget(logEdit, 1);

    auto btnRow = new QHBoxLayout();

    auto copyBtn = new QPushButton("📋 Copy Log", dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [this]
            {
                QApplication::clipboard()->setText(m_launcher->m_crashLog);
            });

    auto closeBtn = new QPushButton("Close", dlg);
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
}

void MainWindow::on_UpdateButton_clicked()
{
    QMessageBox::information(this, "Update", "Checking for updates...");
}

void MainWindow::on_ElyByButton_clicked() {}

void MainWindow::on_SettingsButton_clicked()
{
    globalSettings.exec;
}

void MainWindow::on_PickAccountButton_clicked()
{
    QMessageBox::information(this, "Account", "Selecting account...");
}

// ==================== Versions ====================

void MainWindow::onLoaderChanged(const QString& loader)
{
    m_versionBox->clear();

    if (loader == "Vanilla")        m_downloader->md.fetchVanillaVersions();
    else if (loader == "Fabric")    m_downloader->md.fetchFabricVersions();
    else if (loader == "Forge")     m_downloader->md.fetchForgeVersions();
    else if (loader == "NeoForge")  m_downloader->md.fetchNeoForgeVersions();
}

void MainWindow::onShowSnapshotsChanged(int)
{
    m_versionBox->clear();
    m_versionBox->addItem("Updating list...");
    m_downloader->md.fetchVanillaVersions();
}

void MainWindow::onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions)
{
    m_versionBox->clear();

    QVector<MinecraftVersion> sorted = versions;
    std::sort(sorted.begin(), sorted.end(),
              [](const MinecraftVersion& a, const MinecraftVersion& b) {
                  return QVersionNumber::fromString(a.m_gameVersion)
                         > QVersionNumber::fromString(b.m_gameVersion);
              });

    bool showSnapshots = m_settingsWindow && globalSettings.showSnapshots;

    for (const auto& ver : sorted)
    {
        if (!showSnapshots && ver.m_loaderType != "release") continue;
        m_versionBox->addItem(ver.m_gameVersion);
    }
}

void MainWindow::onFabricVersionsReceived(const QJsonArray& versions)
{
    m_versionBox->clear();
    for (const auto& value : versions)
    {
        QJsonObject obj = value.toObject();
        if (globalSettings.showSnapshots() || obj["stable"].toBool) continue;
        m_versionBox->addItem("Fabric " + obj["version"].toString());
    }
}

void MainWindow::onForgeVersionsReceived(const QJsonObject& json)
{
    m_versionBox->clear();
    QSet<QString> mcVersions;
    QJsonObject promos = json["promos"].toObject();

    for (auto it = promos.begin(); it != promos.end(); ++it)
        mcVersions.insert(it.key().section('-', 0, 0));

    QStringList versions = mcVersions.values();
    std::ranges::sort(versions,
                      [](const QString& a, const QString& b) {
                          return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
                      });

    for (const QString& v : versions)
        m_versionBox->addItem("Forge " + v);
}

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
    m_versionBox->clear();
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
            && !globalSettings.showSnapshots)
            continue;

        const QString mcVersion = neoForgeToMcVersion(version);

        if (!added.contains(mcVersion))
        {
            added.insert(mcVersion);
            mcVersions << mcVersion;
        }
    }

    std::ranges::sort(mcVersions,
                      [](const QString& a, const QString& b) {
                          return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
                      });

    for (const QString& v : mcVersions)
        m_versionBox->addItem(v);
}

void MainWindow::loadVersions()
{
    m_versionBox->clear();
    m_versionBox->addItem("Loading versions...");
    m_downloader->md.fetchVanillaVersions();
}