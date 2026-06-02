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

void MinecraftDownloader::createInstance(
    const QString& minecraftVersion,
    const QString& modLoader,
    const QString& modLoaderVersion,
    const QString& instancePath)
{
    QDir().mkpath(instancePath);

    QString versionsDir =
        instancePath + "/versions/" + minecraftVersion;

    QString librariesDir =
        instancePath + "/libraries";

    QString assetsDir =
        instancePath + "/assets";

    QString indexesDir =
        assetsDir + "/indexes";

    QString objectsDir =
        assetsDir + "/objects";

    QString nativesDir =
        instancePath + "/natives";

    QDir().mkpath(versionsDir);
    QDir().mkpath(librariesDir);
    QDir().mkpath(indexesDir);
    QDir().mkpath(objectsDir);
    QDir().mkpath(nativesDir);

    emit totalProgress(0);

    // =========================
    // VERSION MANIFEST
    // =========================

    QUrl manifestUrl(
        "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

    QNetworkReply* manifestReply =
        manager.get(QNetworkRequest(manifestUrl));

    connect(manifestReply,
            &QNetworkReply::finished,
            this,
            [=]()
            {
                manifestReply->deleteLater();

                if(manifestReply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(
                        manifestReply->errorString());

                    return;
                }

                QJsonDocument manifestDoc =
                    QJsonDocument::fromJson(
                        manifestReply->readAll());

                QJsonArray versions =
                    manifestDoc.object()["versions"]
                        .toArray();

                QString versionUrl;

                for(const auto& value : versions)
                {
                    QJsonObject obj =
                        value.toObject();

                    if(obj["id"].toString()
                        == minecraftVersion)
                    {
                        versionUrl =
                            obj["url"].toString();

                        break;
                    }
                }

                if(versionUrl.isEmpty())
                {
                    emit errorOccurred(
                        "Version not found");

                    return;
                }

                // =========================
                // VERSION JSON
                // =========================

                QNetworkReply* versionReply =
                    manager.get(
                        QNetworkRequest(
                            QUrl(versionUrl)));

                connect(versionReply,
                        &QNetworkReply::finished,
                        this,
                        [=]()
                        {
                            versionReply->deleteLater();

                            if(versionReply->error()
                                != QNetworkReply::NoError)
                            {
                                emit errorOccurred(
                                    versionReply->errorString());

                                return;
                            }

                            QByteArray versionData =
                                versionReply->readAll();

                            QString versionJsonPath =
                                versionsDir + "/" +
                                minecraftVersion +
                                ".json";

                            QFile versionFile(
                                versionJsonPath);

                            if(versionFile.open(
                                    QIODevice::WriteOnly))
                            {
                                versionFile.write(
                                    versionData);

                                versionFile.close();
                            }

                            QJsonDocument versionDoc =
                                QJsonDocument::fromJson(
                                    versionData);

                            QJsonObject root =
                                versionDoc.object();

                            // =========================
                            // CLIENT JAR
                            // =========================

                            QString clientUrl =
                                root["downloads"]
                                    .toObject()["client"]
                                    .toObject()["url"]
                                    .toString();

                            QString jarPath =
                                versionsDir + "/" +
                                minecraftVersion +
                                ".jar";

                            downloadFile(
                                QUrl(clientUrl),
                                jarPath);

                            // =========================
                            // LIBRARIES
                            // =========================

                            QJsonArray libraries =
                                root["libraries"]
                                    .toArray();

                            totalFiles =
                                libraries.size();

                            completedFiles = 0;

                            for(const auto& value
                                 : libraries)
                            {
                                QJsonObject lib =
                                    value.toObject();

                                QJsonObject downloads =
                                    lib["downloads"]
                                        .toObject();

                                // artifact
                                if(downloads.contains(
                                        "artifact"))
                                {
                                    QJsonObject artifact =
                                        downloads["artifact"]
                                            .toObject();

                                    QString url =
                                        artifact["url"]
                                            .toString();

                                    QString path =
                                        artifact["path"]
                                            .toString();

                                    QString fullPath =
                                        librariesDir +
                                        "/" +
                                        path;

                                    QDir().mkpath(
                                        QFileInfo(fullPath)
                                            .path());

                                    downloadFile(
                                        QUrl(url),
                                        fullPath);
                                }

#ifdef Q_OS_WIN

                                // natives
                                if(downloads.contains(
                                        "classifiers"))
                                {
                                    QJsonObject classifiers =
                                        downloads["classifiers"]
                                            .toObject();

                                    QString nativeKey;

                                    if(classifiers.contains(
                                            "natives-windows"))
                                    {
                                        nativeKey =
                                            "natives-windows";
                                    }
                                    else if(classifiers.contains(
                                                 "natives-windows-64"))
                                    {
                                        nativeKey =
                                            "natives-windows-64";
                                    }

                                    if(!nativeKey.isEmpty())
                                    {
                                        QJsonObject nativeObj =
                                            classifiers[nativeKey]
                                                .toObject();

                                        QString nativeUrl =
                                            nativeObj["url"]
                                                .toString();

                                        QString nativePath =
                                            librariesDir +
                                            "/" +
                                            nativeObj["path"]
                                                .toString();

                                        QDir().mkpath(
                                            QFileInfo(nativePath)
                                                .path());

                                        downloadFile(
                                            QUrl(nativeUrl),
                                            nativePath);
                                    }
                                }

#endif
                            }

                            // =========================
                            // ASSET INDEX
                            // =========================

                            QJsonObject assetIndex =
                                root["assetIndex"]
                                    .toObject();

                            QString assetIndexId =
                                assetIndex["id"]
                                    .toString();

                            QString assetIndexUrl =
                                assetIndex["url"]
                                    .toString();

                            QString assetIndexPath =
                                indexesDir +
                                "/" +
                                assetIndexId +
                                ".json";

                            QNetworkReply* assetReply =
                                manager.get(
                                    QNetworkRequest(
                                        QUrl(assetIndexUrl)));

                            connect(assetReply,
                                    &QNetworkReply::finished,
                                    this,
                                    [=]()
                                    {
                                        assetReply->deleteLater();

                                        if(assetReply->error()
                                            != QNetworkReply::NoError)
                                        {
                                            emit errorOccurred(
                                                assetReply->errorString());

                                            return;
                                        }

                                        QByteArray assetData =
                                            assetReply->readAll();

                                        QFile assetFile(
                                            assetIndexPath);

                                        if(assetFile.open(
                                                QIODevice::WriteOnly))
                                        {
                                            assetFile.write(
                                                assetData);

                                            assetFile.close();
                                        }

                                        QJsonDocument assetDoc =
                                            QJsonDocument::fromJson(
                                                assetData);

                                        QJsonObject objects =
                                            assetDoc.object()["objects"]
                                                .toObject();

                                        int downloaded = 0;
                                        int total =
                                            objects.size();

                                        for(auto it =
                                             objects.begin();
                                             it != objects.end();
                                             ++it)
                                        {
                                            QString hash =
                                                it.value()
                                                    .toObject()["hash"]
                                                    .toString();

                                            QString subdir =
                                                hash.left(2);

                                            QString objectUrl =
                                                "https://resources.download.minecraft.net/"
                                                + subdir +
                                                "/" +
                                                hash;

                                            QString objectPath =
                                                objectsDir +
                                                "/" +
                                                subdir +
                                                "/" +
                                                hash;

                                            QDir().mkpath(
                                                QFileInfo(objectPath)
                                                    .path());

                                            QNetworkReply* objReply =
                                                manager.get(
                                                    QNetworkRequest(
                                                        QUrl(objectUrl)));

                                            QFile* file =
                                                new QFile(
                                                    objectPath);

                                            file->open(
                                                QIODevice::WriteOnly);

                                            connect(
                                                objReply,
                                                &QNetworkReply::readyRead,
                                                this,
                                                [objReply, file]()
                                                {
                                                    file->write(
                                                        objReply->readAll());
                                                });

                                            connect(
                                                objReply,
                                                &QNetworkReply::finished,
                                                this,
                                                [=]() mutable
                                                {
                                                    file->write(
                                                        objReply->readAll());

                                                    file->close();

                                                    file->deleteLater();

                                                    objReply->deleteLater();

                                                    downloaded++;

                                                    int progress =
                                                        (downloaded * 100)
                                                        / total;

                                                    emit totalProgress(
                                                        progress);

                                                    if(downloaded
                                                        >= total)
                                                    {
                                                        emit instanceCreated(
                                                            instancePath);
                                                    }
                                                });
                                        }
                                    });
                        });
            });
}
