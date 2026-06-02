#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QVersionNumber>
#include <algorithm>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>

// ==================== MainWindow ====================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    progressBar = new QProgressBar(this);
    progressBar->setGeometry(QRect(780, 710, 500, 25));  // середина низа
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide();

    setupUi(this);
    downloader = new MinecraftDownloader(this);
    settingsWindow = new SettingsWindow(this);

    setupConnections();
    loadVersions();
}


void MainWindow::setupConnections()
{
    connect(downloader, &MinecraftDownloader::downloadProgress, this,
            [this](qint64 received, qint64 total) {
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
            [this](const QString& path) {
                QMessageBox::information(this, "Готово", "Игра установлена:\n" + path);
            });

    connect(downloader, &MinecraftDownloader::errorOccurred, this,
            [this](const QString& error) {
                QMessageBox::warning(this, "Ошибка", error);
            });

    // Подключение кнопок из .ui
    //connect(PlayButton, &QPushButton::clicked, this, &MainWindow::on_PlayButton_clicked);
    //connect(ModPlatformButton, &QPushButton::clicked, this, &MainWindow::on_ModPlatformButton_clicked);
    //connect(UpdateButton, &QPushButton::clicked, this, &MainWindow::on_UpdateButton_clicked);
    //connect(ElyByButton, &QPushButton::clicked, this, &MainWindow::on_ElyByButton_clicked);
    //connect(SettingsButton, &QPushButton::clicked, this, &MainWindow::on_SettingsButton_clicked);
    //connect(PickAccountButton, &QPushButton::clicked, this, &MainWindow::on_PickAccountButton_clicked);
    //connect(InstallerButton, &QPushButton::clicked, this, &MainWindow::on_InstallerButton_clicked);
}

void MainWindow::on_InstallerButton_clicked()
{
    QString versionText = VersionBox->currentText();
    QString loader = LoaderBox->currentText().toLower();

    QString cleanVersion = versionText;
    if (cleanVersion.startsWith("Fabric ")) cleanVersion.remove("Fabric ");
    if (cleanVersion.startsWith("Forge ")) cleanVersion.remove("Forge ");
    if (cleanVersion.startsWith("NeoForge ")) cleanVersion.remove("NeoForge ");

    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty()) {
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";
    }

    QString instancePath = gameDir + "/versions/" + cleanVersion;

    downloader->createInstance(cleanVersion, loader == "vanilla" ? "" : loader, "", instancePath);

    QMessageBox::information(this, "Установка", "Начато скачивание " + versionText);
}

void MainWindow::on_ModPlatformButton_clicked()
{
    ModWindow* window = new ModWindow(this);   // ← new + this
    window->setSettingsWindow(settingsWindow);
    window->setAttribute(Qt::WA_DeleteOnClose); // чтобы не открывалось дважды
    window->exec();
}

void MainWindow::on_PlayButton_clicked()
{
    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty()) {
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";
    }

    QString versionText = VersionBox->currentText();
    QString version = versionText;

    if (version.startsWith("Fabric ")) version.remove("Fabric ");
    if (version.startsWith("Forge ")) version.remove("Forge ");
    if (version.startsWith("NeoForge ")) version.remove("NeoForge ");

    QString versionDir = gameDir + "/versions/" + version;
    QString jsonPath = versionDir + "/" + version + ".json";

    if (!QFileInfo::exists(jsonPath)) {
        QMessageBox::warning(this, "Ошибка", "Версия не установлена:\n" + jsonPath);
        return;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть version.json");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject root = doc.object();
    QString mainClass = root["mainClass"].toString();

    if (mainClass.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "mainClass не найден");
        return;
    }

    QString nativesPath = versionDir + "/natives";

#ifdef Q_OS_WIN
    QString separator = ";";
#else
    QString separator = ":";
#endif

    QString classPath;
    QJsonArray libraries = root["libraries"].toArray();

    for (const auto& value : libraries) {
        QJsonObject lib = value.toObject();
        QJsonObject downloads = lib["downloads"].toObject();

        if (downloads.contains("artifact")) {
            QJsonObject artifact = downloads["artifact"].toObject();
            QString path = artifact["path"].toString();
            if (!path.isEmpty()) {
                QString fullPath = gameDir + "/libraries/" + path;
                if (QFileInfo::exists(fullPath)) {
                    if (!classPath.isEmpty()) classPath += separator;
                    classPath += fullPath;
                }
            }
        }
    }

    if (!classPath.isEmpty()) classPath += separator;
    classPath += versionDir + "/" + version + ".jar";

    QString javaPath = settingsWindow->javaPath();
    if (javaPath.isEmpty()) {
        javaPath = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/java/bin/javaw.exe");
    }

    if (!QFileInfo::exists(javaPath)) {
        javaPath = "javaw";  // системный
    }

    QStringList args = {
        "-Xms2G",
        "-Xmx6G",
        "-Djava.library.path=" + nativesPath,
        "-Dorg.lwjgl.util.Debug=true",
        "-cp", classPath,
        mainClass,
        "--username", "Player",
        "--version", version,
        "--gameDir", gameDir,
        "--assetsDir", gameDir + "/assets",
        "--assetIndex", root["assets"].toString(),
        "--accessToken", "0",
        "--userType", "legacy"
    };

    QThread* thread = new QThread;
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    connect(thread, &QThread::started, [args, javaPath] {
        QProcess* process = new QProcess(nullptr);
        connect(process, &QProcess::readyReadStandardOutput, [process] {
            qDebug().noquote() << process->readAllStandardOutput();
        });

        connect(process, &QProcess::readyReadStandardError, [process] {
            qCritical().noquote() << process->readAllStandardError();
        });

        process->start(javaPath, args);
        if (!process->waitForStarted()) {
            qCritical() << "Failed to start Java:" << process->errorString();
            return;
        }

        process->waitForFinished(-1);

        qDebug() << "Java exited with code" << process->exitCode();
    });

    thread->start();
}

void MainWindow::on_UpdateButton_clicked()
{
    QMessageBox::information(this, "Обновление", "Проверка обновлений...");
}

void MainWindow::on_ElyByButton_clicked()
{
    // ... (твой текущий код Ely.by)
    //if (!elyAuth) {
        // ... (оставь как было)
    //}
    //elyAuth->grant();
}

void MainWindow::on_SettingsButton_clicked()
{
    settingsWindow->exec();
}

void MainWindow::on_PickAccountButton_clicked()
{
    QMessageBox::information(this, "Аккаунт", "Выбор аккаунта...");
}

// ==================== Версии ====================

void MainWindow::onLoaderChanged(const QString& loader)
{
    VersionBox->clear();

    if (loader == "Vanilla")
        downloader->fetchVanillaVersions();
    else if (loader == "Fabric")
        downloader->fetchFabricVersions();
    else if (loader == "Forge")
        downloader->fetchForgeVersions();
    else if (loader == "NeoForge")
        downloader->fetchNeoForgeVersions();
}

void MainWindow::onShowSnapshotsChanged(int state)
{
    VersionBox->clear();
    VersionBox->addItem("Обновление списка...");
    downloader->fetchVanillaVersions();
}

void MainWindow::onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions)
{
    VersionBox->clear();

    QVector<MinecraftVersion> sorted = versions;
    std::sort(sorted.begin(), sorted.end(), [](const MinecraftVersion& a, const MinecraftVersion& b) {
        return QVersionNumber::fromString(a.gameVersion) > QVersionNumber::fromString(b.gameVersion);
    });

    bool showSnapshots = settingsWindow && settingsWindow->showSnapshots();

    for (const auto& ver : sorted) {
        if (!showSnapshots && ver.loaderType != "release") continue;
        VersionBox->addItem(ver.gameVersion);
    }
}

void MainWindow::onFabricVersionsReceived(const QJsonArray& versions)
{
    VersionBox->clear();
    for (const auto& value : versions) {
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

    for (auto it = promos.begin(); it != promos.end(); ++it) {
        QString key = it.key();
        QString mcVersion = key.section('-', 0, 0);
        mcVersions.insert(mcVersion);
    }

    QStringList versions = mcVersions.values();
    std::sort(versions.begin(), versions.end(), [](const QString& a, const QString& b) {
        return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
    });

    for (const QString& v : versions)
        VersionBox->addItem("Forge " + v);
}

void MainWindow::onNeoForgeVersionReceived(const QString& xml)
{
    VersionBox->clear();
    QStringList lines = xml.split('\n');
    QSet<QString> added;

    for (const QString& line : lines) {
        if (!line.contains("<version>")) continue;
        QString version = line;
        version.remove("<version>").remove("</version>").trimmed();

        if (version.contains("beta", Qt::CaseInsensitive) ||
            version.contains("alpha", Qt::CaseInsensitive) ||
            version.contains("rc", Qt::CaseInsensitive)) {
            if (!settingsWindow->showSnapshots()) continue;
        }

        // Простая обработка версии Minecraft
        QString mcVersion = version;
        if (mcVersion.startsWith("21.")) mcVersion = "1." + mcVersion;
        if (!added.contains(mcVersion)) {
            added.insert(mcVersion);
            VersionBox->addItem(mcVersion);
        }
    }
}

void MainWindow::loadVersions()
{
    if (!LoaderBox) {
        LoaderBox = new QComboBox(centralwidget);
        LoaderBox->setGeometry(QRect(780, 640, 250, 30));
        LoaderBox->addItems({"Vanilla", "Fabric", "Forge", "NeoForge"});
        connect(LoaderBox, &QComboBox::currentTextChanged, this, &MainWindow::onLoaderChanged);
    }

    if (!VersionBox) {
        VersionBox = new QComboBox(centralwidget);
        VersionBox->setGeometry(QRect(1050, 640, 250, 30));
    }

    VersionBox->clear();
    VersionBox->addItem("Загрузка версий...");
    downloader->fetchVanillaVersions();
}