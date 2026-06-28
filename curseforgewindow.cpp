#include "curseforgewindow.h"
#include "darktheme.h"
#include "settingswindow.h"

#include <QVBoxLayout>
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
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>

using namespace DarkTheme;

// ── Общие версии MC (упрощённо, реальный список грузить через API дорого) ────
static QStringList defaultMCVersions() {
    return {"Любая версия","1.21.4","1.21.3","1.21.1","1.21",
            "1.20.6","1.20.4","1.20.1","1.20",
            "1.19.4","1.19.2","1.19",
            "1.18.2","1.18","1.17.1","1.16.5","1.16.1",
            "1.15.2","1.14.4","1.12.2","1.8.9","1.7.10"};
}

// ─────────────────────────────────────────────────────────────────────────────
CurseForgeWindow::CurseForgeWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("CurseForge — Моды и Модпаки");
    resize(1300, 860);
    setStyleSheet(dialogStyle());

    m_cf  = new CurseForgeClient(this);
    m_nam = new QNetworkAccessManager(this);

    connect(m_cf, &CurseForgeClient::modsReceived,     this, &CurseForgeWindow::onModsReceived);
    connect(m_cf, &CurseForgeClient::modpacksReceived, this, &CurseForgeWindow::onModpacksReceived);
    connect(m_cf, &CurseForgeClient::filesReceived,    this, &CurseForgeWindow::onFilesReceived);
    connect(m_cf, &CurseForgeClient::errorOccurred,    this, [this](const QString& e){
        m_status->setText("⚠ " + e);
        m_progress->hide();
    });

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16,16,16,16);
    root->setSpacing(12);

    // Заголовок
    QHBoxLayout* header = new QHBoxLayout();
    QLabel* logo = new QLabel("🔥 CurseForge");
    logo->setStyleSheet(QString("font-size:22px; font-weight:bold; color:%1;").arg(kAccentCF));
    QLabel* sub  = new QLabel("Моды и модпаки для Minecraft");
    sub->setStyleSheet(QString("color:%1; font-size:13px;").arg(kTextDim));
    header->addWidget(logo);
    header->addSpacing(12);
    header->addWidget(sub, 1);
    root->addLayout(header);

    // Прогресс и статус
    m_progress = new QProgressBar(this);
    m_progress->setRange(0,0); // indeterminate
    m_progress->setFixedHeight(6);
    m_progress->setStyleSheet(
        "QProgressBar{background:#26292F;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:#F16436;border-radius:3px;}");
    m_progress->hide();
    root->addWidget(m_progress);

    m_status = new QLabel("", this);
    m_status->setStyleSheet(QString("color:%1; font-size:12px;").arg(kTextDim));
    root->addWidget(m_status);

    // Вкладки
    m_tabs = new QTabWidget(this);
    buildModsTab();
    buildModpacksTab();
    root->addWidget(m_tabs, 1);

    connect(m_tabs, &QTabWidget::currentChanged, this, &CurseForgeWindow::onTabChanged);

    // Первый поиск
    m_cf->searchMods("", "", "", CFProjectType::Mod, 20);
    m_progress->show();
    m_status->setText("Загрузка модов...");
}

// ── Вкладка «Моды» ────────────────────────────────────────────────────────────
void CurseForgeWindow::buildModsTab()
{
    QWidget* w = new QWidget();
    QVBoxLayout* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12,12,12,12);
    vl->setSpacing(10);

    // Поиск
    QHBoxLayout* row = new QHBoxLayout();
    m_modSearch = new QLineEdit();
    m_modSearch->setPlaceholderText("Поиск модов на CurseForge...");
    m_modSearch->setFixedHeight(38);
    row->addWidget(m_modSearch, 1);

    m_modVersion = new QComboBox();
    m_modVersion->addItems(defaultMCVersions());
    m_modVersion->setFixedWidth(150);
    row->addWidget(m_modVersion);

    m_modLoader = new QComboBox();
    m_modLoader->addItems({"Любой загрузчик","Forge","Fabric","NeoForge","Quilt"});
    m_modLoader->setFixedWidth(150);
    row->addWidget(m_modLoader);

    QPushButton* btn = new QPushButton("Найти");
    btn->setFixedHeight(38);
    btn->setStyleSheet(cfButton());
    connect(btn, &QPushButton::clicked, this, &CurseForgeWindow::onSearch);
    connect(m_modSearch, &QLineEdit::returnPressed, this, &CurseForgeWindow::onSearch);
    row->addWidget(btn);
    vl->addLayout(row);

    QScrollArea* sa = new QScrollArea();
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    QWidget* cw = new QWidget();
    m_modCards = new QVBoxLayout(cw);
    m_modCards->setSpacing(10);
    m_modCards->setAlignment(Qt::AlignTop);
    sa->setWidget(cw);
    vl->addWidget(sa, 1);

    m_tabs->addTab(w, "⚙ Моды");
}

// ── Вкладка «Модпаки» ─────────────────────────────────────────────────────────
void CurseForgeWindow::buildModpacksTab()
{
    QWidget* w = new QWidget();
    QVBoxLayout* vl = new QVBoxLayout(w);
    vl->setContentsMargins(12,12,12,12);
    vl->setSpacing(10);

    QHBoxLayout* row = new QHBoxLayout();
    m_packSearch = new QLineEdit();
    m_packSearch->setPlaceholderText("Поиск модпаков (ATM, FTB, Revelation...)");
    m_packSearch->setFixedHeight(38);
    row->addWidget(m_packSearch, 1);

    m_packVersion = new QComboBox();
    m_packVersion->addItems(defaultMCVersions());
    m_packVersion->setFixedWidth(150);
    row->addWidget(m_packVersion);

    QPushButton* btn = new QPushButton("Найти");
    btn->setFixedHeight(38);
    btn->setStyleSheet(cfButton());
    connect(btn, &QPushButton::clicked, this, &CurseForgeWindow::onSearch);
    connect(m_packSearch, &QLineEdit::returnPressed, this, &CurseForgeWindow::onSearch);
    row->addWidget(btn);
    vl->addLayout(row);

    QScrollArea* sa = new QScrollArea();
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    QWidget* cw = new QWidget();
    m_packCards = new QVBoxLayout(cw);
    m_packCards->setSpacing(10);
    m_packCards->setAlignment(Qt::AlignTop);
    sa->setWidget(cw);
    vl->addWidget(sa, 1);

    m_tabs->addTab(w, "📦 Модпаки");
}

// ── Поиск ─────────────────────────────────────────────────────────────────────
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

// ── Отображение карточек ──────────────────────────────────────────────────────
void CurseForgeWindow::onModsReceived(const QVector<CFMod>& mods)
{
    m_progress->hide();
    m_status->setText(QString("Найдено: %1 модов").arg(mods.size()));
    clearLayout(m_modCards);
    addCards(mods, m_modCards, m_modStore, false);
}

void CurseForgeWindow::onModpacksReceived(const QVector<CFMod>& packs)
{
    m_progress->hide();
    m_status->setText(QString("Найдено: %1 модпаков").arg(packs.size()));
    clearLayout(m_packCards);
    addCards(packs, m_packCards, m_packStore, true);
}

void CurseForgeWindow::addCards(const QVector<CFMod>& mods,
                                QVBoxLayout* layout,
                                QHash<int, CFMod>& store,
                                bool isModpack)
{
    store.clear();

    for (const CFMod& mod : mods) {
        store.insert(mod.id, mod);

        QFrame* card = new QFrame();
        card->setStyleSheet(QString(
                                "QFrame { background-color:%1; border:1px solid %2;"
                                " border-radius:10px; }"
                                "QFrame:hover { border-color:%3; }")
                                .arg(kPanel, kBorder, isModpack ? kAccentCF : kAccent));
        card->setMinimumHeight(90);

        QHBoxLayout* hl = new QHBoxLayout(card);
        hl->setContentsMargins(14,10,14,10);
        hl->setSpacing(14);

        // Иконка
        QLabel* icon = new QLabel();
        icon->setFixedSize(64, 64);
        icon->setStyleSheet(QString("border-radius:8px; background:%1; border:none;").arg(kPanelHi));
        icon->setScaledContents(true);
        hl->addWidget(icon, 0, Qt::AlignTop);

        if (!mod.iconUrl.isEmpty()) {
            QNetworkReply* r = m_nam->get(QNetworkRequest(QUrl(mod.iconUrl)));
            connect(r, &QNetworkReply::finished, this, [icon, r]() {
                QPixmap px; px.loadFromData(r->readAll());
                if (!px.isNull()) icon->setPixmap(px.scaled(64,64,Qt::KeepAspectRatio,Qt::SmoothTransformation));
                r->deleteLater();
            });
        }

        // Инфо
        QVBoxLayout* info = new QVBoxLayout();
        info->setSpacing(4);

        QLabel* title = new QLabel(mod.name);
        title->setStyleSheet("font-size:15px; font-weight:bold; border:none;");
        info->addWidget(title);

        QLabel* author = new QLabel("by " + mod.author);
        author->setStyleSheet(QString("color:%1; font-size:12px; border:none;").arg(kTextDim));
        info->addWidget(author);

        QString descText = mod.summary.length() > 120
                               ? mod.summary.left(120) + "..." : mod.summary;
        QLabel* desc = new QLabel(descText);
        desc->setWordWrap(true);
        desc->setStyleSheet(QString("color:%1; font-size:12px; border:none;").arg(kTextDim));
        info->addWidget(desc);

        // Версии
        if (!mod.gameVersions.isEmpty()) {
            QStringList vers = mod.gameVersions;
            std::sort(vers.begin(), vers.end(), std::greater<QString>());
            QString verStr = vers.mid(0, 4).join(", ");
            if (vers.size() > 4) verStr += "...";
            QLabel* verLbl = new QLabel("MC: " + verStr);
            verLbl->setStyleSheet(QString("color:%1; font-size:11px; border:none;").arg(kTextDim));
            info->addWidget(verLbl);
        }

        hl->addLayout(info, 1);

        // Правая колонка
        QVBoxLayout* right = new QVBoxLayout();
        right->setSpacing(8);

        QLabel* dl = new QLabel("⬇ " + formatCount(mod.downloadCount));
        dl->setAlignment(Qt::AlignRight);
        dl->setStyleSheet(QString("color:%1; border:none;").arg(kTextDim));
        right->addWidget(dl);

        right->addStretch(1);

        QPushButton* installBtn = new QPushButton(isModpack ? "⬇ Скачать" : "+ Установить");
        installBtn->setFixedWidth(130);
        installBtn->setFixedHeight(36);
        installBtn->setCursor(Qt::PointingHandCursor);
        if (isModpack) installBtn->setStyleSheet(cfButton());
        else installBtn->setStyleSheet(
                QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                        "border-radius:8px;font-weight:bold;}"
                        "QPushButton:hover{background:%1;color:#0A0A0A;}").arg(kAccent));

        // Копируем mod для лямбды
        connect(installBtn, &QPushButton::clicked, this, [this, mod, isModpack]() {
            installItem(mod, isModpack);
        });
        right->addWidget(installBtn, 0, Qt::AlignRight);

        // Кнопка «На CurseForge»
        if (!mod.websiteUrl.isEmpty()) {
            QPushButton* webBtn = new QPushButton("🌐 CF");
            webBtn->setFixedWidth(60);
            webBtn->setFixedHeight(28);
            webBtn->setStyleSheet(
                QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                        "border-radius:6px;font-size:11px;}"
                        "QPushButton:hover{background:%1;color:#0A0A0A;}").arg(kAccentCF));
            connect(webBtn, &QPushButton::clicked, this, [url = mod.websiteUrl](){
                QDesktopServices::openUrl(QUrl(url));
            });
            right->addWidget(webBtn, 0, Qt::AlignRight);
        }

        hl->addLayout(right);
        layout->addWidget(card);
    }
}

// ── Установка ─────────────────────────────────────────────────────────────────
void CurseForgeWindow::installItem(const CFMod& mod, bool isModpack)
{
    m_progress->show();
    m_status->setText(QString("Получение файлов для «%1»...").arg(mod.name));

    // Запрашиваем файлы проекта
    QString mcVer = isModpack
                        ? m_packVersion->currentText()
                        : m_modVersion->currentText();
    QString loader = isModpack ? "" : m_modLoader->currentText();

    if (mcVer == "Любая версия") mcVer = "";
    if (loader == "Любой загрузчик") loader = "";

    m_cf->getProjectFiles(mod.id, mcVer, loader);

    // Ждём filesReceived — соединяем одноразово
    auto* conn = new QMetaObject::Connection();
    *conn = connect(m_cf, &CurseForgeClient::filesReceived,
                    this, [this, conn, mod, isModpack](int pid, const QVector<CFFileInfo>& files) {
                        if (pid != mod.id) return;
                        disconnect(*conn);
                        delete conn;

                        if (files.isEmpty()) {
                            m_progress->hide();
                            m_status->setText("⚠ Файлы не найдены для этой версии/загрузчика");
                            QMessageBox::warning(this, "CurseForge",
                                                 "Файлы не найдены.\nПопробуйте выбрать другую версию MC или загрузчик.");
                            return;
                        }

                        // Берём первый (самый новый) файл
                        const CFFileInfo& file = files.first();
                        QString url = file.downloadUrl;

                        if (url.isEmpty()) {
                            // Fallback: строим URL по известному шаблону
                            int part1 = file.id / 1000;
                            int part2 = file.id % 1000;
                            url = QString("https://edge.forgecdn.net/files/%1/%2/%3")
                                      .arg(part1)
                                      .arg(QString::number(part2).rightJustified(3,'0'))
                                      .arg(file.fileName);
                        }

                        m_status->setText(QString("Скачивание: %1").arg(file.fileName));
                        downloadFile(QUrl(url), file.fileName, isModpack);
                    });
}

void CurseForgeWindow::onFilesReceived(int, const QVector<CFFileInfo>&)
{
    // Обработка в лямбде installItem — здесь пусто
}

void CurseForgeWindow::downloadFile(const QUrl& url,
                                    const QString& fileName,
                                    bool isModpack)
{
    QString gameDir = m_settings ? m_settings->minecraftPath() : "";
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    // Модпак → mods/modpacks/, мод → mods/
    QString subdir = isModpack ? "/modpacks" : "/mods";
    QString savePath = gameDir + subdir;
    QDir().mkpath(savePath);

    QString fn = fileName.isEmpty() ? QFileInfo(url.path()).fileName() : fileName;
    if (fn.isEmpty()) fn = "file.zip";
    savePath += "/" + fn;

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent","ZXCrackLauncher/1.0 (Qt)");
    QNetworkReply* reply = m_nam->get(req);

    QSaveFile* sf = new QSaveFile(savePath, this);
    if (!sf->open(QIODevice::WriteOnly)) {
        m_progress->hide();
        m_status->setText("⚠ Не удалось открыть файл для записи");
        reply->deleteLater();
        sf->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, sf]() {
        sf->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 recv, qint64 total) {
                if (total > 0) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(int(recv * 100 / total));
                }
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, sf, fn, isModpack, savePath]() {
        QByteArray tail = reply->readAll();
        if (!tail.isEmpty()) sf->write(tail);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            sf->cancelWriting();
            sf->deleteLater();
            m_progress->hide();
            m_status->setText("⚠ Ошибка скачивания: " + reply->errorString());
            return;
        }
        if (sf->commit()) {
            m_progress->hide();
            m_status->setText("✓ Скачано: " + fn);
            QMessageBox::information(this, "Готово",
                                     QString("%1 скачан!\n\nСохранён в:\n%2")
                                         .arg(fn, savePath));
        } else {
            m_progress->hide();
            m_status->setText("⚠ Ошибка сохранения файла");
        }
        sf->deleteLater();
    });
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void CurseForgeWindow::clearLayout(QVBoxLayout* l)
{
    QLayoutItem* item;
    while ((item = l->takeAt(0))) {
        if (item->widget()) delete item->widget();
        delete item;
    }
}

QString CurseForgeWindow::formatCount(quint64 v)
{
    if (v >= 1'000'000ULL) return QString::number(v/1'000'000.0,'f',1)+"M";
    if (v >= 1'000ULL)     return QString::number(v/1'000.0,'f',1)+"K";
    return QString::number(v);
}