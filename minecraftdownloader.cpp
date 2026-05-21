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

    emit vanillaVersionsReceived(
        versions);
}

// ==================== FABRIC ====================
void MinecraftDownloader::fetchFabricVersions()
{
    QUrl url("https://meta.fabricmc.net/v2/versions/loader");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }
        emit fabricVersionsReceived(QJsonDocument::fromJson(reply->readAll()).array());
    });
}

// ==================== FORGE ====================
void MinecraftDownloader::fetchForgeVersions()
{
    QUrl url("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
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
    QUrl url("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
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

void MinecraftDownloader::createInstance(const QString& minecraftVersion, const QString& modLoader, const QString& modLoaderVersion, const QString& instancePath){
    // =====================================================
    // CREATE DIRECTORIES
    // =====================================================

    QDir dir;

    dir.mkpath(instancePath);
    dir.mkpath(instancePath + "/versions");
    dir.mkpath(instancePath + "/libraries");
    dir.mkpath(instancePath + "/assets");
    dir.mkpath(instancePath + "/mods");

    // =====================================================
    // DOWNLOAD VERSION MANIFEST
    // =====================================================

    QUrl manifestUrl(
        "https://piston-meta.mojang.com/"
        "mc/game/version_manifest_v2.json"
        );

    QNetworkReply* manifestReply = manager.get(QNetworkRequest(manifestUrl));

    connect(manifestReply,&QNetworkReply::finished,this,[=](){
        QByteArray manifestData =
            manifestReply->readAll();

        manifestReply->deleteLater();

        if(manifestReply->error() !=
            QNetworkReply::NoError)
        {
            emit errorOccurred(
                manifestReply->errorString()
                );

            return;
        }

        QJsonDocument manifestDoc =
            QJsonDocument::fromJson(
                manifestData
                );

        QJsonArray versions =
            manifestDoc.object()
                ["versions"]
                    .toArray();

        QString versionJsonUrl;

        for(const auto& v : versions)
        {
            QJsonObject obj =
                v.toObject();

            if(obj["id"].toString() ==
                minecraftVersion)
            {
                versionJsonUrl =
                    obj["url"].toString();

                break;
            }
        }

        if(versionJsonUrl.isEmpty())
        {
            emit errorOccurred(
                "Minecraft version not found"
                );

            return;
        }

        // =============================================
        // DOWNLOAD VERSION JSON
        // =============================================

        QNetworkReply* versionReply =
            manager.get(
                QNetworkRequest(
                    QUrl(versionJsonUrl)
                    )
                );

        connect(versionReply,
                &QNetworkReply::finished,
                this,
                [=]()
                {
                    QByteArray versionData =
                        versionReply->readAll();

                    versionReply->deleteLater();

                    if(versionReply->error() !=
                        QNetworkReply::NoError)
                    {
                        emit errorOccurred(
                            versionReply->errorString()
                            );

                        return;
                    }

                    QJsonDocument versionDoc =
                        QJsonDocument::fromJson(
                            versionData
                            );

                    QJsonObject versionObj =
                        versionDoc.object();

                    // =========================================
                    // DOWNLOAD CLIENT JAR
                    // =========================================

                    QString clientUrl =
                        versionObj["downloads"]
                            .toObject()["client"]
                            .toObject()["url"]
                            .toString();

                    QString clientJarPath =
                        instancePath +
                        "/versions/" +
                        minecraftVersion +
                        ".jar";

                    downloadFile(
                        QUrl(clientUrl),
                        clientJarPath
                        );

                    // =========================================
                    // DOWNLOAD LIBRARIES
                    // =========================================

                    QJsonArray libraries =
                        versionObj["libraries"]
                            .toArray();

                    for(const auto& l : libraries)
                    {
                        QJsonObject lib =
                            l.toObject();

                        if(!lib.contains("downloads"))
                            continue;

                        QJsonObject artifact =
                            lib["downloads"]
                                .toObject()["artifact"]
                                .toObject();

                        QString url =
                            artifact["url"]
                                .toString();

                        QString path =
                            artifact["path"]
                                .toString();

                        if(url.isEmpty())
                            continue;

                        QString fullPath =
                            instancePath +
                            "/libraries/" +
                            path;

                        QFileInfo info(fullPath);

                        dir.mkpath(
                            info.path()
                            );

                        downloadFile(
                            QUrl(url),
                            fullPath
                            );
                    }

                    // =========================================
                    // FABRIC
                    // =========================================

                    if(modLoader == "fabric")
                    {
                        QString fabricMetaUrl =
                            "https://meta.fabricmc.net/"
                            "v2/versions/loader/" +
                            minecraftVersion +
                            "/" +
                            modLoaderVersion +
                            "/profile/json";

                        downloadFile(
                            QUrl(fabricMetaUrl),
                            instancePath +
                                "/fabric-loader.json"
                            );
                    }

                    // =========================================
                    // FORGE
                    // =========================================

                    else if(modLoader == "forge")
                    {
                        QString forgeInstallerUrl =
                            "https://maven.minecraftforge.net/"
                            "net/minecraftforge/forge/" +
                            minecraftVersion +
                            "-" +
                            modLoaderVersion +
                            "/forge-" +
                            minecraftVersion +
                            "-" +
                            modLoaderVersion +
                            "-installer.jar";

                        downloadFile(
                            QUrl(forgeInstallerUrl),
                            instancePath +
                                "/forge-installer.jar"
                            );
                    }

                    // =========================================
                    // NEOFORGE
                    // =========================================

                    else if(modLoader == "neoforge")
                    {
                        QString neoForgeUrl =
                            "https://maven.neoforged.net/"
                            "releases/net/neoforged/"
                            "neoforge/" +
                            modLoaderVersion +
                            "/neoforge-" +
                            modLoaderVersion +
                            "-installer.jar";

                        downloadFile(QUrl(neoForgeUrl), instancePath + "/neoforge-installer.jar");
                    }

                    emit instanceCreated(instancePath);
                });
    });
}