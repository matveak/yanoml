#include "MinecraftDownloader.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QDir>

MinecraftDownloader::MinecraftDownloader(QObject* parent)
    : QObject(parent)
{
    // Опционально: увеличиваем лимит одновременных соединений
    manager.setTransferTimeout(30000); // 30 секунд
}

void MinecraftDownloader::fetchVanillaVersions()
{
    qDebug() << "Requesting versions...";
    QUrl url("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        handleVanillaManifest(reply);
    });
}

void MinecraftDownloader::handleVanillaManifest(QNetworkReply* reply)
{
    QJsonDocument doc =
        QJsonDocument::fromJson(reply->readAll());

    if(!doc.isObject())
    {
        emit errorOccurred(
            "Некорректный ответ Mojang");
        return;
    }

    QJsonArray versionsArray =
        doc.object()["versions"].toArray();

    QVector<MinecraftVersion> versions;

    for(const auto& value : versionsArray)
    {
        QJsonObject obj =
            value.toObject();

        MinecraftVersion ver;

        ver.gameVersion =
            obj["id"].toString();

        ver.loaderVersion = "";

        ver.loaderType =
            obj["type"].toString();

        ver.url =
            obj["url"].toString();

        ver.releaseTime =
            obj["releaseTime"].toString();

        versions.push_back(ver);
    }
    qDebug() << "Loaded versions:"
             << versions.size();

    emit vanillaVersionsReceived(
        versions);
}

// ==================== FABRIC ====================
void MinecraftDownloader::fetchFabricVersions()
{
    qDebug() << "Request Fabric versions";

    QNetworkReply* reply =
        manager.get(
            QNetworkRequest(
                QUrl("https://meta.fabricmc.net/v2/versions/game")));

    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply]()
            {
                QByteArray data = reply->readAll();

                if(reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(
                        reply->errorString());

                    reply->deleteLater();
                    return;
                }

                QJsonDocument doc =
                    QJsonDocument::fromJson(data);

                if(!doc.isArray())
                {
                    emit errorOccurred(
                        "Fabric API returned invalid JSON");

                    reply->deleteLater();
                    return;
                }

                emit fabricVersionsReceived(
                    doc.array());

                reply->deleteLater();
            });
}
// ==================== FORGE ====================
void MinecraftDownloader::fetchForgeVersions()
{
    qDebug() << "Request Forge versions";
    QUrl url("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        qDebug() << "Forge finished";
        qDebug() << reply->errorString();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit forgeVersionsReceived(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

// ==================== NEOFORGE ====================
void MinecraftDownloader::fetchNeoForgeVersions()
{
    qDebug() << "Request NeoForge versions";
    QUrl url("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        qDebug() << "NeoForge finished";
        qDebug() << reply->errorString();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit neoforgeVersionReceived(QString(reply->readAll()));
    });
}

// ==================== СКАЧИВАНИЕ ФАЙЛА ====================
void MinecraftDownloader::downloadFile(const QUrl& url, const QString& outputPath)
{
    QNetworkReply* reply = manager.get(QNetworkRequest(url));
    QSaveFile* file = new QSaveFile(outputPath);

    if (!file->open(QIODevice::WriteOnly)) {
        emit errorOccurred("Не удалось открыть файл для записи: " + outputPath);
        file->deleteLater();
        reply->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this, &MinecraftDownloader::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, file, outputPath]() {
        file->write(reply->readAll());
        file->commit();  // Важно! Гарантирует целостность файла

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
        } else {
            emit fileDownloaded(outputPath);
        }

        file->deleteLater();
        reply->deleteLater();
    });
}

void MinecraftDownloader::downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJar)
{
    QNetworkReply* reply = manager.get(QNetworkRequest(QUrl(versionJsonUrl)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, outputJar]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString clientUrl = doc.object()["downloads"]
                                .toObject()["client"]
                                .toObject()["url"]
                                .toString();

        if (!clientUrl.isEmpty()) {
            downloadFile(QUrl(clientUrl), outputJar);
        } else {
            emit errorOccurred("Не удалось найти ссылку на клиент");
        }
    });
}

void MinecraftDownloader::createInstance(const QString& minecraftVersion,
                                         const QString& modLoader,
                                         const QString& modLoaderVersion,
                                         const QString& instancePath)
{
    QDir dir(instancePath);
    if (!dir.exists())
        dir.mkpath(".");

    emit downloadProgress(0, 100);

    // ==================== 1. Скачиваем version.json ====================
    QUrl manifestUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

    QNetworkReply* manifestReply = manager.get(QNetworkRequest(manifestUrl));

    connect(manifestReply, &QNetworkReply::finished, this, [=]() {
        manifestReply->deleteLater();

        if (manifestReply->error() != QNetworkReply::NoError) {
            emit errorOccurred("Не удалось скачать манифест версий: " + manifestReply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(manifestReply->readAll());
        QJsonArray versions = doc.object()["versions"].toArray();

        QString versionUrl;

        for (const auto& v : versions) {
            QJsonObject obj = v.toObject();
            if (obj["id"].toString() == minecraftVersion) {
                versionUrl = obj["url"].toString();
                break;
            }
        }

        if (versionUrl.isEmpty()) {
            emit errorOccurred("Версия " + minecraftVersion + " не найдена");
            return;
        }

        // ==================== 2. Скачиваем client.json ====================
        QNetworkReply* versionReply = manager.get(QNetworkRequest(QUrl(versionUrl)));

        connect(versionReply, &QNetworkReply::finished, this, [=]() {
            versionReply->deleteLater();

            if (versionReply->error() != QNetworkReply::NoError) {
                emit errorOccurred(versionReply->errorString());
                return;
            }

            QJsonDocument verDoc = QJsonDocument::fromJson(versionReply->readAll());
            QJsonObject verObj = verDoc.object();

            // Скачиваем client.jar
            QString clientUrl = verObj["downloads"].toObject()["client"].toObject()["url"].toString();
            QString clientPath = instancePath + "/versions/" + minecraftVersion + "/" + minecraftVersion + ".jar";

            QDir().mkpath(instancePath + "/versions/" + minecraftVersion);

            if (!clientUrl.isEmpty()) {
                downloadFile(QUrl(clientUrl), clientPath);
            }

            // Скачиваем библиотеки
            QJsonArray libraries = verObj["libraries"].toArray();
            for (const auto& lib : libraries) {
                QJsonObject libObj = lib.toObject();
                QJsonObject downloads = libObj["downloads"].toObject();
                if (downloads.contains("artifact")) {
                    QJsonObject artifact = downloads["artifact"].toObject();
                    QString url = artifact["url"].toString();
                    QString path = artifact["path"].toString();

                    if (!url.isEmpty() && !path.isEmpty()) {
                        QString fullPath = instancePath + "/libraries/" + path;
                        QDir().mkpath(QFileInfo(fullPath).path());
                        downloadFile(QUrl(url), fullPath);
                    }
                }
            }

            // ==================== 3. Установка загрузчика ====================
            if (modLoader == "fabric") {
                QString fabricUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                .arg(minecraftVersion, modLoaderVersion.isEmpty() ? "latest" : modLoaderVersion);

                QString fabricJsonPath = instancePath + "/versions/" + minecraftVersion + "-fabric/" + minecraftVersion + "-fabric.json";
                QDir().mkpath(QFileInfo(fabricJsonPath).path());
                downloadFile(QUrl(fabricUrl), fabricJsonPath);
            }
            else if (modLoader == "forge" || modLoader == "neoforge") {
                QString loaderName = (modLoader == "forge") ? "forge" : "neoforge";
                QString versionStr = modLoaderVersion.isEmpty() ? minecraftVersion : modLoaderVersion;

                QString installerUrl = (modLoader == "forge") ?
                                           QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1-%2/forge-%1-%2-installer.jar")
                                               .arg(minecraftVersion, versionStr) :
                                           QString("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar")
                                               .arg(versionStr);

                QString installerPath = instancePath + "/" + loaderName + "-installer.jar";
                downloadFile(QUrl(installerUrl), installerPath);

                emit errorOccurred("Установщик " + loaderName.toUpper() + " скачан.\nЗапустите его вручную для завершения установки.");
            }

            emit instanceCreated(instancePath);
        });
    });
}