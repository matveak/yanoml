#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QVersionNumber>
#include <algorithm>

static bool versionGreater(const MinecraftVersion& a,
                           const MinecraftVersion& b)
{
    return QVersionNumber::fromString(a.gameVersion) >
           QVersionNumber::fromString(b.gameVersion);
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{

    setupUi(this);                    // Загружает дизайн из .ui
    downloader = new MinecraftDownloader(this);
    settingsWindow = new SettingsWindow(this);
    setupConnections();
    loadVersions();                   // Загружаем версии Minecraft
}

MainWindow::~MainWindow() = default;

void MainWindow::setupConnections()
{
    connect(settingsWindow,
            &SettingsWindow::settingsChanged,
            this,
            [this]()
            {
                if (LoaderBox->currentText() == "Vanilla")
                {
                    VersionBox->clear();
                    VersionBox->addItem("Обновление списка...");
                    downloader->fetchVanillaVersions();
                }
            });
    // Кнопки
    // Сигналы от downloader
    connect(downloader, &MinecraftDownloader::vanillaVersionsReceived,
            this, &MainWindow::onVanillaVersionsReceived);

    connect(downloader, &MinecraftDownloader::errorOccurred,
            this, [](const QString& error) {
                QMessageBox::warning(nullptr, "Ошибка сети", error);
            });
    connect(downloader,
            &MinecraftDownloader::fabricVersionsReceived,
            this,
            &MainWindow::onFabricVersionsReceived);

    connect(downloader,
            &MinecraftDownloader::forgeVersionsReceived,
            this,
            &MainWindow::onForgeVersionsReceived);

    connect(downloader,
            &MinecraftDownloader::neoforgeVersionReceived,
            this,
            &MainWindow::onNeoForgeVersionReceived);
}

// ======================== СЛОТЫ ========================

void MainWindow::on_PlayButton_clicked()
{
    QMessageBox::information(this, "Запуск",
                             "Запускаем: " + VersionBox->currentText());
    // Здесь позже будет код запуска игры
}

void MainWindow::on_ModPlatformButton_clicked()
{
    qDebug() << "OPEN MODS";

    ModWindow window(this);
    window.exec();

    qDebug() << "MODS CLOSED";
}

void MainWindow::on_UpdateButton_clicked()
{
    QMessageBox::information(this, "Обновление", "Проверка обновлений...");
}

void MainWindow::on_ElyByButton_clicked()
{
    QMessageBox::information(this, "Ely.by", "Авторизация через Ely.by...");
}

void MainWindow::on_SettingsButton_clicked()
{
    qDebug() << "OPEN SETTINGS";
    settingsWindow->exec();
    qDebug() << "SETTINGS CLOSED";
}
void MainWindow::on_PickAccountButton_clicked()
{
    QMessageBox::information(this, "Аккаунт", "Выбор аккаунта...");
}

void MainWindow::on_InstallerButton_clicked()
{
    downloader->createInstance("1.20.1","","","C:/Users/matveak/Downloads/zxc");
}

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

void MainWindow::onVanillaVersionsReceived(
    const QVector<MinecraftVersion>& versions)
{
    VersionBox->clear();

    bool showSnapshots =
        settingsWindow &&
        settingsWindow->showSnapshots();

    QVector<MinecraftVersion> sorted =
        versions;

    std::sort(
        sorted.begin(),
        sorted.end(),
        versionGreater);

    for(const auto& ver : sorted)
    {
        if(!showSnapshots &&
            ver.loaderType != "release")
        {
            continue;
        }

        VersionBox->addItem(
            ver.gameVersion,
            ver.url);
    }
}

void MainWindow::onFabricVersionsReceived(
    const QJsonArray& versions)
{
    VersionBox->clear();

    QStringList mcVersions;

    for (const auto& value : versions)
    {
        QJsonObject obj = value.toObject();

        mcVersions.append(
            obj["gameVersion"].toString());
    }

    std::sort(
        mcVersions.begin(),
        mcVersions.end(),
        [](const QString& a,
           const QString& b)
        {
            return QVersionNumber::fromString(a) >
                   QVersionNumber::fromString(b);
        });

    for (const QString& version : mcVersions)
    {
        VersionBox->addItem(
            "Fabric " + version);
    }
}

void MainWindow::onForgeVersionsReceived(
    const QJsonObject& json)
{
    VersionBox->clear();

    QJsonObject promos =
        json["promos"].toObject();

    for (auto it = promos.begin();
         it != promos.end();
         ++it)
    {
        VersionBox->addItem(
            it.key() +
            " -> " +
            it.value().toString());
    }
}

void MainWindow::onNeoForgeVersionReceived(
    const QString& xml)
{
    VersionBox->clear();

    QStringList lines =
        xml.split('\n');

    for (const QString& line : lines)
    {
        if (line.contains("<version>"))
        {
            QString version = line;

            version.remove("<version>");
            version.remove("</version>");

            VersionBox->addItem(
                "NeoForge " +
                version.trimmed());
        }
    }
}

void MainWindow::onShowSnapshotsChanged(int state)
{
    // Перезагружаем версии при изменении галочки
    VersionBox->clear();
    VersionBox->addItem("Обновление списка...");

    downloader->fetchVanillaVersions();
}

void MainWindow::loadVersions()
{
    VersionBox->clear();
    VersionBox->addItem("Загрузка версий...");

    // Выбор ядра
    if (!LoaderBox)
    {
        LoaderBox = new QComboBox(centralwidget);
        LoaderBox->setGeometry(QRect(1325, 640, 245, 31));

        LoaderBox->addItem("Vanilla");
        LoaderBox->addItem("Fabric");
        LoaderBox->addItem("Forge");
        LoaderBox->addItem("NeoForge");

        connect(LoaderBox,
                &QComboBox::currentTextChanged,
                this,
                &MainWindow::onLoaderChanged);
    }

    // Галочка снапшотов

    downloader->fetchVanillaVersions();
}
