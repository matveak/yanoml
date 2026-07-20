#include "curseforgewindow.h"
#include "settingswindow.h"
#include "modwindow.h"
#include "../ui/theme.h"
#include "../ui/windowframe.h"
#include "../ui/titlebar.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QProgressBar>
#include <QTabWidget>
#include <QMessageBox>
#include <QDir>
#include <QSaveFile>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QSize>
#include <QColor>

#include "../settings.h"

// ==================== Constants ====================

namespace {
    constexpr int ICON_SIZE = 64;
    constexpr int CARD_MIN_HEIGHT = 90;
    constexpr int SEARCH_FIELD_HEIGHT = 38;
    constexpr int BUTTON_HEIGHT = 36;
    constexpr int INSTALL_BUTTON_WIDTH = 130;
    constexpr int WEB_BUTTON_WIDTH = 60;
    constexpr int WEB_BUTTON_HEIGHT = 28;
    constexpr int MAX_DESCRIPTION_LENGTH = 120;
    constexpr int MAX_VERSIONS_DISPLAY = 4;

    const QString USER_AGENT = "ZXCrackLauncher/1.0 (Qt)";
    const QString FALLBACK_URL_TEMPLATE = "https://edge.forgecdn.net/files/%1/%2/%3";
}

// ==================== Helper Functions ====================

static QStringList defaultMCVersions() {
    return {"Любая версия", "1.21.4", "1.21.3", "1.21.1", "1.21",
            "1.20.6", "1.20.4", "1.20.1", "1.20",
            "1.19.4", "1.19.2", "1.19",
            "1.18.2", "1.18", "1.17.1", "1.16.5", "1.16.1",
            "1.15.2", "1.14.4", "1.12.2", "1.8.9", "1.7.10"};
}

static QString formatDownloadCount(quint64 count) {
    if (count >= 1'000'000ULL) {
        return QString::number(count / 1'000'000.0, 'f', 1) + "M";
    }
    if (count >= 1'000ULL) {
        return QString::number(count / 1'000.0, 'f', 1) + "K";
    }
    return QString::number(count);
}

static QString buildFallbackDownloadUrl(const CFFileInfo& file) {
    int part1 = file.id / 1000;
    int part2 = file.id % 1000;
    return FALLBACK_URL_TEMPLATE
        .arg(part1)
        .arg(QString::number(part2).rightJustified(3, '0'))
        .arg(file.fileName);
}

// ==================== Constructor ====================

CurseForgeWindow::CurseForgeWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setStyleSheet(Theme::dialogStyle());
    resize(1300, 860);

    // Window m_frame
    m_frame = new WindowFrame(this);
    m_frame->setTitle("CurseForge — Моды и Модпаки");

    // ── Переключатель источника модов — прямо в шапке окна ───────────
    // (слева от кнопок свернуть/закрыть, без выпадающего меню)
    {
        auto* switcherLayout = m_frame->titleBar()->rightLayout();

        const QString switchBtnStyle =
            "QPushButton { border: none; border-radius: 6px; background: transparent; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 26); }"
            "QPushButton:pressed { background: rgba(255, 255, 255, 42); }"
            "QPushButton:disabled { background: rgba(241, 100, 54, 40); }";

        auto* modrinthBtn = new QPushButton(m_frame->titleBar());
        modrinthBtn->setIcon(Theme::platformIcon(QColor("#1BD96A"), "M"));
        modrinthBtn->setIconSize(QSize(18, 18));
        modrinthBtn->setFixedSize(28, 26);
        modrinthBtn->setCursor(Qt::PointingHandCursor);
        modrinthBtn->setFlat(true);
        modrinthBtn->setToolTip("Открыть Modrinth");
        modrinthBtn->setStyleSheet(switchBtnStyle);

        auto* curseforgeBtn = new QPushButton(m_frame->titleBar());
        curseforgeBtn->setIcon(Theme::platformIcon(QColor("#F16436"), "C"));
        curseforgeBtn->setIconSize(QSize(18, 18));
        curseforgeBtn->setFixedSize(28, 26);
        curseforgeBtn->setFlat(true);
        curseforgeBtn->setEnabled(false); // мы уже находимся в CurseForge
        curseforgeBtn->setToolTip("CurseForge — текущий раздел");
        curseforgeBtn->setStyleSheet(switchBtnStyle);

        connect(modrinthBtn, &QPushButton::clicked, this, [this] {
            auto* window = new ModWindow(parentWidget());
            window->setSettingsWindow(m_settings);
            window->setAttribute(Qt::WA_DeleteOnClose);
            window->exec();
            close();
        });

        switcherLayout->insertWidget(0, modrinthBtn);
        switcherLayout->insertWidget(1, curseforgeBtn);

        auto* sep = new QFrame(m_frame->titleBar());
        sep->setFixedWidth(1);
        sep->setFixedHeight(16);
        sep->setStyleSheet(QString("background-color: %1;").arg(Theme::border().name()));
        switcherLayout->insertWidget(2, sep);
    }

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_frame);

    auto* contentLayout = new QVBoxLayout(m_frame->contentWidget());
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(12);

    // Network clients
    m_cf = new CurseForgeClient(this);
    m_nam = new QNetworkAccessManager(this);

    // Connections
    setupConnections();

    // UI
    buildHeader(contentLayout);
    buildProgressSection(contentLayout);
    buildTabs(contentLayout);

    // Initial load
    m_cf->searchMods("", "", "", CFProjectType::Mod, 20);
    m_progress->show();
    m_status->setText("Загрузка модов...");
}

// ==================== Setup Methods ====================

void CurseForgeWindow::setupConnections()
{
    connect(m_cf, &CurseForgeClient::modsReceived,
            this, &CurseForgeWindow::onModsReceived);
    connect(m_cf, &CurseForgeClient::modpacksReceived,
            this, &CurseForgeWindow::onModpacksReceived);
    connect(m_cf, &CurseForgeClient::errorOccurred,
            this, &CurseForgeWindow::onError);
}

void CurseForgeWindow::buildHeader(QVBoxLayout* layout)  // Изменили QLayout* на QVBoxLayout*
{
    auto* header = new QHBoxLayout();

    auto* logo = new QLabel("CurseForge");
    logo->setStyleSheet(QString("font-size:22px; font-weight:bold; color:%1;")
        .arg(Theme::accentCurseForge().name()));

    auto* subtitle = new QLabel("Моды и модпаки для Minecraft");
    subtitle->setStyleSheet(QString("color:%1; font-size:13px;")
        .arg(Theme::textDim().name()));

    header->addWidget(logo);
    header->addSpacing(12);
    header->addWidget(subtitle, 1);
    layout->addLayout(header);  // Теперь работает
}

void CurseForgeWindow::buildProgressSection(QVBoxLayout* layout)
{
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setFixedHeight(6);
    m_progress->setStyleSheet(
        "QProgressBar { background:#26292F; border:none; border-radius:3px; }"
        "QProgressBar::chunk { background:#F16436; border-radius:3px; }");
    m_progress->hide();
    layout->addWidget(m_progress);

    m_status = new QLabel("", this);
    m_status->setStyleSheet(QString("color:%1; font-size:12px;")
        .arg(Theme::textDim().name()));
    layout->addWidget(m_status);
}

void CurseForgeWindow::buildTabs(QVBoxLayout* layout)  // Изменили QLayout* на QVBoxLayout*
{
    m_tabs = new QTabWidget(this);
    buildModsTab();
    buildModpacksTab();
    layout->addWidget(m_tabs, 1);  // Теперь работает

    connect(m_tabs, &QTabWidget::currentChanged,
            this, &CurseForgeWindow::onTabChanged);
}

// ==================== Tab Builders ====================

void CurseForgeWindow::buildModsTab()
{
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Search row
    auto* searchRow = new QHBoxLayout();

    m_modSearch = new QLineEdit();
    m_modSearch->setPlaceholderText("Поиск модов на CurseForge...");
    m_modSearch->setFixedHeight(SEARCH_FIELD_HEIGHT);
    searchRow->addWidget(m_modSearch, 1);

    m_modVersion = new QComboBox();
    m_modVersion->addItems(defaultMCVersions());
    m_modVersion->setFixedWidth(150);
    searchRow->addWidget(m_modVersion);

    m_modLoader = new QComboBox();
    m_modLoader->addItems({"Любой загрузчик", "Forge", "Fabric", "NeoForge", "Quilt"});
    m_modLoader->setFixedWidth(150);
    searchRow->addWidget(m_modLoader);

    auto* searchButton = new QPushButton("Найти");
    searchButton->setFixedHeight(SEARCH_FIELD_HEIGHT);
    searchButton->setStyleSheet(Theme::curseForgeButtonStyle());
    connect(searchButton, &QPushButton::clicked, this, &CurseForgeWindow::onSearch);
    connect(m_modSearch, &QLineEdit::returnPressed, this, &CurseForgeWindow::onSearch);
    searchRow->addWidget(searchButton);

    layout->addLayout(searchRow);

    // Scroll area with cards
    auto* scrollArea = createScrollArea();
    m_modCards = new QVBoxLayout(scrollArea->widget());
    m_modCards->setSpacing(10);
    m_modCards->setAlignment(Qt::AlignTop);
    layout->addWidget(scrollArea, 1);

    m_tabs->addTab(widget, "⚙ Моды");
}

void CurseForgeWindow::buildModpacksTab()
{
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Search row
    auto* searchRow = new QHBoxLayout();

    m_packSearch = new QLineEdit();
    m_packSearch->setPlaceholderText("Поиск модпаков (ATM, FTB, Revelation...)");
    m_packSearch->setFixedHeight(SEARCH_FIELD_HEIGHT);
    searchRow->addWidget(m_packSearch, 1);

    m_packVersion = new QComboBox();
    m_packVersion->addItems(defaultMCVersions());
    m_packVersion->setFixedWidth(150);
    searchRow->addWidget(m_packVersion);

    auto* searchButton = new QPushButton("Найти");
    searchButton->setFixedHeight(SEARCH_FIELD_HEIGHT);
    searchButton->setStyleSheet(Theme::curseForgeButtonStyle());
    connect(searchButton, &QPushButton::clicked, this, &CurseForgeWindow::onSearch);
    connect(m_packSearch, &QLineEdit::returnPressed, this, &CurseForgeWindow::onSearch);
    searchRow->addWidget(searchButton);

    layout->addLayout(searchRow);

    // Scroll area with cards
    auto* scrollArea = createScrollArea();
    m_packCards = new QVBoxLayout(scrollArea->widget());
    m_packCards->setSpacing(10);
    m_packCards->setAlignment(Qt::AlignTop);
    layout->addWidget(scrollArea, 1);

    m_tabs->addTab(widget, "📦 Модпаки");
}

QScrollArea* CurseForgeWindow::createScrollArea()
{
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget();
    scrollArea->setWidget(contentWidget);

    return scrollArea;
}

// ==================== Event Handlers ====================

void CurseForgeWindow::onSearch()
{
    m_progress->show();

    if (m_tabs->currentIndex() == 0) {
        m_status->setText("Поиск модов...");
        clearLayout(m_modCards);
        m_cf->searchMods(m_modSearch->text(),
                         m_modVersion->currentText(),
                         m_modLoader->currentText());
    } else {
        m_status->setText("Поиск модпаков...");
        clearLayout(m_packCards);
        m_cf->searchModpacks(m_packSearch->text(),
                             m_packVersion->currentText());
    }
}

void CurseForgeWindow::onTabChanged(int index)
{
    if (index == 1 && m_packCards->count() == 0) {
        m_progress->show();
        m_status->setText("Загрузка модпаков...");
        m_cf->searchModpacks("", "");
    }
}

void CurseForgeWindow::onModsReceived(const QVector<CFMod>& mods)
{
    m_progress->hide();
    m_status->setText(QString("Найдено: %1 модов").arg(mods.size()));
    clearLayout(m_modCards);
    displayMods(mods, m_modCards, m_modStore, false);
}

void CurseForgeWindow::onModpacksReceived(const QVector<CFMod>& packs)
{
    m_progress->hide();
    m_status->setText(QString("Найдено: %1 модпаков").arg(packs.size()));
    clearLayout(m_packCards);
    displayMods(packs, m_packCards, m_packStore, true);
}

void CurseForgeWindow::onError(const QString& error)
{
    m_status->setText("⚠ " + error);
    m_progress->hide();
}

// ==================== Display Methods ====================

void CurseForgeWindow::displayMods(const QVector<CFMod>& mods,
                                   QVBoxLayout* layout,
                                   QHash<int, CFMod>& store,
                                   bool isModpack)
{
    store.clear();

    for (const auto& mod : mods) {
        store.insert(mod.id, mod);
        auto* card = createModCard(mod, isModpack);
        layout->addWidget(card);
    }
}

QFrame* CurseForgeWindow::createModCard(const CFMod& mod, bool isModpack)
{
    auto* card = new QFrame();
    card->setStyleSheet(QString(
        "QFrame { background-color:%1; border:1px solid %2; border-radius:10px; }"
        "QFrame:hover { border-color:%3; }")
        .arg(Theme::panel().name(),
             Theme::border().name(),
             isModpack ? Theme::accentCurseForge().name() : Theme::accent().name()));
    card->setMinimumHeight(CARD_MIN_HEIGHT);

    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(14);

    // Icon
    auto* iconLabel = createIconLabel(mod.iconUrl);
    layout->addWidget(iconLabel, 0, Qt::AlignTop);

    // Info section
    auto* infoLayout = createInfoLayout(mod);
    layout->addLayout(infoLayout, 1);

    // Actions section
    auto* actionsLayout = createActionsLayout(mod, isModpack);
    layout->addLayout(actionsLayout);

    return card;
}

QLabel* CurseForgeWindow::createIconLabel(const QString& iconUrl)
{
    auto* iconLabel = new QLabel();
    iconLabel->setFixedSize(ICON_SIZE, ICON_SIZE);
    iconLabel->setStyleSheet(QString("border-radius:8px; background:%1; border:none;")
        .arg(Theme::panelHighlight().name()));
    iconLabel->setScaledContents(true);

    if (!iconUrl.isEmpty()) {
        downloadIconAsync(iconUrl, iconLabel);
    }

    return iconLabel;
}

void CurseForgeWindow::downloadIconAsync(const QString& url, QLabel* target)
{
    auto* reply = m_nam->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [reply, target] {
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull()) {
            target->setPixmap(pixmap.scaled(ICON_SIZE, ICON_SIZE,
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        }
        reply->deleteLater();
    });
}

QVBoxLayout* CurseForgeWindow::createInfoLayout(const CFMod& mod)
{
    auto* layout = new QVBoxLayout();
    layout->setSpacing(4);

    // Title
    auto* titleLabel = new QLabel(mod.name);
    titleLabel->setStyleSheet("font-size:15px; font-weight:bold; border:none;");
    layout->addWidget(titleLabel);

    // Author
    auto* authorLabel = new QLabel("by " + mod.author);
    authorLabel->setStyleSheet(QString("color:%1; font-size:12px; border:none;")
        .arg(Theme::textDim().name()));
    layout->addWidget(authorLabel);

    // Description
    QString description = mod.summary.length() > MAX_DESCRIPTION_LENGTH
        ? mod.summary.left(MAX_DESCRIPTION_LENGTH) + "..."
        : mod.summary;
    auto* descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QString("color:%1; font-size:12px; border:none;")
        .arg(Theme::textDim().name()));
    layout->addWidget(descLabel);

    // Versions
    if (!mod.gameVersions.isEmpty()) {
        auto* versionsLabel = createVersionsLabel(mod.gameVersions);
        layout->addWidget(versionsLabel);
    }

    return layout;
}

QLabel* CurseForgeWindow::createVersionsLabel(const QStringList& versions)
{
    QStringList sorted = versions;
    std::ranges::sort(sorted, std::greater<QString>());

    QString versionText = sorted.mid(0, MAX_VERSIONS_DISPLAY).join(", ");
    if (sorted.size() > MAX_VERSIONS_DISPLAY) {
        versionText += "...";
    }

    auto* label = new QLabel("MC: " + versionText);
    label->setStyleSheet(QString("color:%1; font-size:11px; border:none;")
        .arg(Theme::textDim().name()));
    return label;
}

QVBoxLayout* CurseForgeWindow::createActionsLayout(const CFMod& mod, bool isModpack)
{
    auto* layout = new QVBoxLayout();
    layout->setSpacing(8);

    // Download count
    auto* downloadLabel = new QLabel("⬇ " + formatDownloadCount(mod.downloadCount));
    downloadLabel->setAlignment(Qt::AlignRight);
    downloadLabel->setStyleSheet(QString("color:%1; border:none;")
        .arg(Theme::textDim().name()));
    layout->addWidget(downloadLabel);

    layout->addStretch(1);

    // Install button
    auto* installButton = createInstallButton(mod, isModpack);
    layout->addWidget(installButton, 0, Qt::AlignRight);

    // Website button
    if (!mod.websiteUrl.isEmpty()) {
        auto* webButton = createWebsiteButton(mod.websiteUrl);
        layout->addWidget(webButton, 0, Qt::AlignRight);
    }

    return layout;
}

QPushButton* CurseForgeWindow::createInstallButton(const CFMod& mod, bool isModpack)
{
    auto* button = new QPushButton(isModpack ? "⬇ Скачать" : "+ Установить");
    button->setFixedSize(INSTALL_BUTTON_WIDTH, BUTTON_HEIGHT);
    button->setCursor(Qt::PointingHandCursor);

    if (isModpack) {
        button->setStyleSheet(Theme::curseForgeButtonStyle());
    } else {
        button->setStyleSheet(QString(
            "QPushButton { background:transparent; color:%1; border:1px solid %1;"
            "border-radius:8px; font-weight:bold; }"
            "QPushButton:hover { background:%1; color:#0A0A0A; }")
            .arg(Theme::accent().name()));
    }

    connect(button, &QPushButton::clicked, this, [this, mod, isModpack] {
        installItem(mod, isModpack);
    });

    return button;
}

QPushButton* CurseForgeWindow::createWebsiteButton(const QString& url)
{
    auto* button = new QPushButton("🌐 CF");
    button->setFixedSize(WEB_BUTTON_WIDTH, WEB_BUTTON_HEIGHT);
    button->setStyleSheet(QString(
        "QPushButton { background:transparent; color:%1; border:1px solid %1;"
        "border-radius:6px; font-size:11px; }"
        "QPushButton:hover { background:%1; color:#0A0A0A; }")
        .arg(Theme::accentCurseForge().name()));

    connect(button, &QPushButton::clicked, this, [url] {
        QDesktopServices::openUrl(QUrl(url));
    });

    return button;
}

// ==================== Installation ====================

void CurseForgeWindow::installItem(const CFMod& mod, bool isModpack)
{
    m_progress->show();
    m_status->setText(QString("Получение файлов для «%1»...").arg(mod.name));

    QString mcVersion = isModpack ? m_packVersion->currentText() : m_modVersion->currentText();
    QString loader = isModpack ? "" : m_modLoader->currentText();

    if (mcVersion == "Любая версия") mcVersion = "";
    if (loader == "Любой загрузчик") loader = "";

    m_cf->getProjectFiles(mod.id, mcVersion, loader);

    // One-time connection for files
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_cf, &CurseForgeClient::filesReceived,
        this, [this, connection, mod, isModpack](int projectId, const QVector<CFFileInfo>& files) {
            if (projectId != mod.id) return;

            disconnect(*connection);
            handleFilesReceived(mod, files, isModpack);
        });
}

void CurseForgeWindow::handleFilesReceived(const CFMod& mod,
                                           const QVector<CFFileInfo>& files,
                                           bool isModpack)
{
    if (files.isEmpty()) {
        m_progress->hide();
        m_status->setText("⚠ Файлы не найдены для этой версии/загрузчика");
        QMessageBox::warning(this, "CurseForge",
            "Файлы не найдены.\nПопробуйте выбрать другую версию MC или загрузчик.");
        return;
    }

    const auto& file = files.first();
    QString downloadUrl = file.downloadUrl.isEmpty()
        ? buildFallbackDownloadUrl(file)
        : file.downloadUrl;

    m_status->setText(QString("Скачивание: %1").arg(file.fileName));
    downloadFile(QUrl(downloadUrl), file.fileName, isModpack);
}

void CurseForgeWindow::downloadFile(const QUrl& url,
                                    const QString& fileName,
                                    bool isModpack)
{
    QString gameDir = globalSettings.minecraftPath;
    if (gameDir.isEmpty()) {
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";
    }

    QString subdir = isModpack ? "/modpacks" : "/mods";
    QString savePath = gameDir + subdir;
    QDir().mkpath(savePath);

    QString finalFileName = fileName.isEmpty()
        ? QFileInfo(url.path()).fileName()
        : fileName;
    if (finalFileName.isEmpty()) {
        finalFileName = "file.zip";
    }
    savePath += "/" + finalFileName;

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", USER_AGENT.toUtf8());
    auto* reply = m_nam->get(request);

    auto* saveFile = new QSaveFile(savePath, this);
    if (!saveFile->open(QIODevice::WriteOnly)) {
        m_progress->hide();
        m_status->setText("⚠ Не удалось открыть файл для записи");
        reply->deleteLater();
        saveFile->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, saveFile] {
        saveFile->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            if (total > 0) {
                m_progress->setRange(0, 100);
                m_progress->setValue(int(received * 100 / total));
            }
        });

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, saveFile, finalFileName, savePath] {
            handleDownloadFinished(reply, saveFile, finalFileName, savePath);
        });
}

void CurseForgeWindow::handleDownloadFinished(QNetworkReply* reply,
                                              QSaveFile* saveFile,
                                              const QString& fileName,
                                              const QString& savePath)
{
    QByteArray remaining = reply->readAll();
    if (!remaining.isEmpty()) {
        saveFile->write(remaining);
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        saveFile->cancelWriting();
        saveFile->deleteLater();
        m_progress->hide();
        m_status->setText("⚠ Ошибка скачивания: " + reply->errorString());
        return;
    }

    if (saveFile->commit()) {
        m_progress->hide();
        m_status->setText("✓ Скачано: " + fileName);
        QMessageBox::information(this, "Готово",
            QString("%1 скачан!\n\nСохранён в:\n%2").arg(fileName, savePath));
    } else {
        m_progress->hide();
        m_status->setText("⚠ Ошибка сохранения файла");
    }
    saveFile->deleteLater();
}

// ==================== Utility Methods ====================

void CurseForgeWindow::clearLayout(QVBoxLayout* layout)
{
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearNestedLayout(item->layout());
        }
        delete item;
    }
}

void CurseForgeWindow::clearNestedLayout(QLayout* layout)
{
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearNestedLayout(item->layout());
        }
        delete item;
    }
}