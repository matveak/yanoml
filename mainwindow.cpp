#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QVersionNumber>
#include <algorithm>
#include <QDir>

static bool versionGreater(const MinecraftVersion& a,
                           const MinecraftVersion& b)
{
    return QVersionNumber::fromString(a.gameVersion) >
           QVersionNumber::fromString(b.gameVersion);
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    progressBar = new QProgressBar(this);

    progressBar->setGeometry(
        QRect(1050, 680, 250, 25));

    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide();
    setupUi(this);                    // Загружает дизайн из .ui
    downloader = new MinecraftDownloader(this);
    settingsWindow = new SettingsWindow(this);
    setupConnections();
    loadVersions();                   // Загружаем версии Minecraft
}

MainWindow::~MainWindow() = default;

void MainWindow::setupConnections()
{
    connect(downloader,
            &MinecraftDownloader::downloadProgress,
            this,
            [this](qint64 received,
                   qint64 total)
            {
                if(total <= 0)
                    return;

                progressBar->show();

                int percent =
                    static_cast<int>(
                        (received * 100) / total);

                progressBar->setValue(percent);
            });
    connect(
        downloader,
        &MinecraftDownloader::vanillaVersionsReceived,
        this,
        &MainWindow::onVanillaVersionsReceived);
    connect(
        downloader,
        &MinecraftDownloader::fabricVersionsReceived,
        this,
        &MainWindow::onFabricVersionsReceived);

    connect(
        downloader,
        &MinecraftDownloader::forgeVersionsReceived,
        this,
        &MainWindow::onForgeVersionsReceived);

    connect(
        downloader,
        &MinecraftDownloader::neoforgeVersionReceived,
        this,
        &MainWindow::onNeoForgeVersionReceived);

    connect(downloader,
            &MinecraftDownloader::instanceCreated,
            this,
            [this](const QString& path)
            {
                QMessageBox::information(
                    this,
                    "Готово",
                    "Игра установлена:\n" + path);
            });

    connect(downloader,
            &MinecraftDownloader::errorOccurred,
            this,
            [this](const QString& error)
            {
                QMessageBox::warning(
                    this,
                    "Ошибка",
                    error);
            });

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
}

void MainWindow::on_InstallerButton_clicked()
{
    QString version =
        VersionBox->currentText();

    QString loader =
        LoaderBox->currentText().toLower();

    // Fabric 1.21.1 -> 1.21.1
    if(version.startsWith("Fabric "))
    {
        version.remove("Fabric ");
    }

    // NeoForge 21.1.100 -> 21.1.100
    if(version.startsWith("NeoForge "))
    {
        version.remove("NeoForge ");
    }

    QString instancePath =
        "C:/Users/matveak/Downloads/zxc";

    downloader->createInstance(
        version,
        loader == "vanilla"
            ? ""
            : loader,
        "",
        instancePath);

    QMessageBox::information(
        this,
        "Установка",
        "Начато скачивание " + version);
}

void MainWindow::onLoaderChanged(const QString& loader)
{
    qDebug() << "Loader changed:" << loader;

    VersionBox->clear();

    if(loader == "Vanilla")
        downloader->fetchVanillaVersions();
    else if(loader == "Fabric")
        downloader->fetchFabricVersions();
    else if(loader == "Forge")
        downloader->fetchForgeVersions();
    else if(loader == "NeoForge")
        downloader->fetchNeoForgeVersions();
}

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

void MainWindow::requestElyProfile()
{
    QNetworkRequest request(
        QUrl("https://account.ely.by/api/account/v1/info"));

    request.setRawHeader(
        "Authorization",
        "Bearer " +
            elyAuth->token().toUtf8());

    QNetworkReply* reply =
        networkManager.get(request);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            QByteArray data =
                reply->readAll();

            qDebug() << data;

            QJsonDocument doc =
                QJsonDocument::fromJson(data);

            QString username =
                doc.object()["username"]
                    .toString();

            QMessageBox::information(
                this,
                "Ely.by",
                "Вход выполнен:\n" +
                    username);

            reply->deleteLater();
        });
}

void MainWindow::on_ElyByButton_clicked()
{
    if (!elyAuth)
    {
        elyAuth =
            new QOAuth2AuthorizationCodeFlow(this);

        elyAuth->setAuthorizationUrl(
            QUrl("https://account.ely.by/oauth2/v1"));

        elyAuth->setAccessTokenUrl(
            QUrl("https://account.ely.by/api/oauth2/v1/token"));

        elyAuth->setClientIdentifier(
            "yanoml1");

        elyAuth->setClientIdentifierSharedKey(
            "CXglpRel3S8wNFSqqJmoldvMw2JEjakYQMZ3-apRcEE0MurMW6x4fsLvUAvF9bU0");

        auto* replyHandler =
            new QOAuthHttpServerReplyHandler(
                8080,
                this);

        elyAuth->setReplyHandler(
            replyHandler);

        connect(
            elyAuth,
            &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
            &QDesktopServices::openUrl);

        connect(
            elyAuth,
            &QOAuth2AuthorizationCodeFlow::granted,
            this,
            [this]()
            {
                qDebug()
                << "Авторизация успешна";

                requestElyProfile();
            });
    }

    elyAuth->grant();
}

void MainWindow::on_SettingsButton_clicked(){
    settingsWindow->exec();
}

void MainWindow::on_PickAccountButton_clicked(){
    QMessageBox::information(this, "Аккаунт", "Выбор аккаунта...");
}

void MainWindow::onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions)
{
    VersionBox->clear();

    QVector<MinecraftVersion> sorted = versions;

    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const MinecraftVersion& a,
           const MinecraftVersion& b)
        {
            return QVersionNumber::fromString(a.gameVersion) >
                   QVersionNumber::fromString(b.gameVersion);
        });

    bool showSnapshots =
        settingsWindow &&
        settingsWindow->showSnapshots();

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

void MainWindow::onFabricVersionsReceived(const QJsonArray& versions)
{
    VersionBox->clear();

    QStringList mcVersions;

    for(const auto& value : versions)
    {
        QJsonObject obj = value.toObject();

        bool stable =
            obj["stable"].toBool();

        if(!settingsWindow->showSnapshots() &&
            !stable)
        {
            continue;
        }

        QString version =
            obj["version"].toString();

        VersionBox->addItem(
            "Fabric " + version);
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

    for(const QString& version : mcVersions)
    {
        VersionBox->addItem(
            "Fabric " + version);
    }

    qDebug() << "Fabric versions loaded:"
             << VersionBox->count();
}

void MainWindow::onForgeVersionsReceived(
    const QJsonObject& json)
{
    VersionBox->clear();

    QSet<QString> mcVersions;

    QJsonObject promos =
        json["promos"].toObject();

    for(auto it = promos.begin();
         it != promos.end();
         ++it)
    {
        QString key = it.key();

        // 1.21.1-latest -> 1.21.1
        QString mcVersion =
            key.section('-', 0, 0);

        mcVersions.insert(mcVersion);
    }

    QStringList versions =
        mcVersions.values();

    std::sort(
        versions.begin(),
        versions.end(),
        [](const QString& a,
           const QString& b)
        {
            return QVersionNumber::fromString(a) >
                   QVersionNumber::fromString(b);
        });

    for(const QString& version : versions)
    {
        VersionBox->addItem(
            "Forge " + version);
    }
}

void MainWindow::onNeoForgeVersionReceived(const QString& xml)
{
    VersionBox->clear();

    QStringList lines = xml.split('\n');

    QSet<QString> addedVersions;

    for(const QString& line : lines)
    {
        if(!line.contains("<version>"))
            continue;

        QString version = line;
        version.remove("<version>");
        version.remove("</version>");
        version = version.trimmed();

        // Пропускаем служебные версии
        if(version.contains("beta", Qt::CaseInsensitive) ||
            version.contains("alpha", Qt::CaseInsensitive) ||
            version.contains("rc", Qt::CaseInsensitive))
        {
            if(!settingsWindow->showSnapshots())
                continue;
        }

        QStringList parts = version.split('.');

        if(parts.size() < 2)
            continue;

        bool okMajor = false;
        bool okMinor = false;

        int major = parts[0].toInt(&okMajor);
        int minor = parts[1].toInt(&okMinor);

        if(!okMajor || !okMinor)
            continue;

        QString mcVersion;

        // NeoForge 21.1.x -> Minecraft 1.21.1
        if(minor > 0)
        {
            mcVersion =
                QString("1.%1.%2")
                    .arg(major)
                    .arg(minor);
        }
        else
        {
            // NeoForge 21.0.x -> Minecraft 1.21
            mcVersion =
                QString("1.%1")
                    .arg(major);
        }

        if(addedVersions.contains(mcVersion))
            continue;

        addedVersions.insert(mcVersion);

        VersionBox->addItem(mcVersion);
    }

    // Сортировка версий
    QStringList versions;

    for(int i = 0; i < VersionBox->count(); ++i)
        versions << VersionBox->itemText(i);

    std::sort(
        versions.begin(),
        versions.end(),
        [](const QString& a, const QString& b)
        {
            return QVersionNumber::fromString(a) >
                   QVersionNumber::fromString(b);
        });

    VersionBox->clear();

    for(const QString& v : versions)
        VersionBox->addItem(v);
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
    if(!LoaderBox)
    {
        LoaderBox = new QComboBox(centralwidget);

        LoaderBox->setGeometry(
            QRect(780, 640, 250, 30));

        LoaderBox->addItem("Vanilla");
        LoaderBox->addItem("Fabric");
        LoaderBox->addItem("Forge");
        LoaderBox->addItem("NeoForge");

        connect(
            LoaderBox,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::onLoaderChanged);
    }

    if(!VersionBox)
    {
        VersionBox = new QComboBox(centralwidget);

        VersionBox->setGeometry(
            QRect(1050, 640, 250, 30));
    }

    VersionBox->clear();
    VersionBox->addItem("Загрузка версий...");

    downloader->fetchVanillaVersions();
}