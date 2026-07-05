#include "mainwindow.h"
#include "createmodpackwindow.h"
#include "curseforgewindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <algorithm>
#include <memory>
#include <QSet>
#include <QJsonValue>

#include "modwindow.h"

// ==================== Java Auto-Detection ====================


// Ищет javaw.exe / java в стандартных местах установки
// Возвращает карту: major -> полный путь к исполняемому файлу


// Совместима ли установленная Java с версией, которую требует Minecraft.
// Старые версии (LWJGL 3.2.x и раньше) падают на слишком новой Java,
// поэтому им нужна именно major-версия, указанная Mojang.
// Современные (Java 17+, LWJGL 3.3+) спокойно работают на более новой Java.




// ==================== Natives Extraction ====================

// Проверяет правила библиотеки для текущей ОС (allow/disallow).

// ==================== MainWindow ====================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi(this);

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

    // Сканируем Java в фоне при старте
    //QTimer::singleShot(500, this, [this]() {
    //    installedJavas = findInstalledJavas();
    //    qDebug() << "Java scan complete, found" << installedJavas.size() << "installations";
    //});
}

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
        // Чистая ваниль.
        //downloader->createInstance(cleanVersion, "", "", gameDir);
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

    //downloader->createInstance(cleanVersion, "", "", gameDir);
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
        int requiredMajor = requiredJavaMajor(mcVersion);
        launcher->ensureJava(requiredMajor, mcVersion, gameDir,
                   [this, loader, mcVersion, gameDir](const QString& javaExe)
                   {
                       downloader->installForgeLike(mcVersion, loader, javaExe, gameDir);
                   });
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

        int requiredMajor =
            parentRoot["javaVersion"].toObject()["majorVersion"].toInt();
        if (requiredMajor <= 0)
            requiredMajor = requiredJavaMajor(parentVersion);

        ensureJava(requiredMajor, parentVersion, gameDir,
                   [this, parentRoot, childRoot, gameDir, parentVersion, versionId,
                    requiredMajor](const QString& javaExe)
                   {
                       launchModded(parentRoot, childRoot, gameDir, parentVersion,
                                    versionId, javaExe, requiredMajor);
                   });
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

    int requiredMajor = root["javaVersion"].toObject()["majorVersion"].toInt();
    if (requiredMajor <= 0)
        requiredMajor = requiredJavaMajor(version);

    ensureJava(requiredMajor, version, gameDir,
               [this, root, gameDir, version, versionDir, mainClass, requiredMajor]
               (const QString& javaExe)
               {
                   launcher->launchGame(root, gameDir, version, versionDir, mainClass,
                              javaExe, requiredMajor);
               });
}

// ==================== Java provisioning ====================
// ==================== Launch ====================
// ==================== Launch (modded) ====================

// Проверка os-правил аргумента (современный формат arguments.jvm/game).


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
    std::ranges::sort(sorted,
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
    // UpdateBox в новом UI — это комбобокс загрузчика в правой панели
    if (!LoaderBox)
    {
        LoaderBox = UpdateBox;
        LoaderBox->clear();
        LoaderBox->addItems({"Vanilla", "Fabric", "Forge", "NeoForge"});
        connect(LoaderBox, &QComboBox::currentTextChanged,
                this, &MainWindow::onLoaderChanged);
    }

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