#include "MinecraftDownloader.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

MinecraftDownloader::MinecraftDownloader(QObject* parent)
    : QObject(parent)
{
    manager.setTransferTimeout(30000);
}

// ==================== HELPERS ====================

static bool libraryAllowedOnCurrentOS(const QJsonObject& lib)
{
    QJsonArray rules = lib["rules"].toArray();
    if (rules.isEmpty())
        return true;

    bool allowed = false;

    for (const auto& ruleVal : rules)
    {
        QJsonObject rule = ruleVal.toObject();
        QString action  = rule["action"].toString();

        if (rule.contains("os"))
        {
            QString osName = rule["os"].toObject()["name"].toString();

#ifdef Q_OS_WIN
            QString currentOS = "windows";
#elif defined(Q_OS_MAC)
            QString currentOS = "osx";
#else
            QString currentOS = "linux";
#endif
            if (osName == currentOS)
                allowed = (action == "allow");
        }
        else
        {
            allowed = (action == "allow");
        }
    }

    return allowed;
}

// ==================== VANILLA ====================

void MinecraftDownloader::fetchVanillaVersions()
{
    qDebug() << "Requesting versions...";

    QUrl url("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                handleVanillaManifest(reply);
            });
}

void MinecraftDownloader::handleVanillaManifest(QNetworkReply* reply)
{
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

    if (!doc.isObject())
    {
        emit errorOccurred("Некорректный ответ Mojang");
        return;
    }

    QJsonArray versionsArray = doc.object()["versions"].toArray();

    QVector<MinecraftVersion> versions;

    for (const auto& value : versionsArray)
    {
        QJsonObject obj = value.toObject();

        MinecraftVersion ver;
        ver.gameVersion  = obj["id"].toString();
        ver.loaderVersion = "";
        ver.loaderType   = obj["type"].toString();
        ver.url          = obj["url"].toString();
        ver.releaseTime  = obj["releaseTime"].toString();

        versions.push_back(ver);
    }

    qDebug() << "Loaded versions:" << versions.size();

    emit vanillaVersionsReceived(versions);
}

// ==================== FABRIC ====================

void MinecraftDownloader::fetchFabricVersions()
{
    qDebug() << "Request Fabric versions";

    QNetworkReply* reply = manager.get(
        QNetworkRequest(QUrl("https://meta.fabricmc.net/v2/versions/game")));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                QByteArray data = reply->readAll();
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(data);

                if (!doc.isArray())
                {
                    emit errorOccurred("Fabric API returned invalid JSON");
                    return;
                }

                emit fabricVersionsReceived(doc.array());
            });
}

// ==================== FORGE ====================

void MinecraftDownloader::fetchForgeVersions()
{
    qDebug() << "Request Forge versions";

    QUrl url("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                qDebug() << "Forge finished" << reply->errorString();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                emit forgeVersionsReceived(
                    QJsonDocument::fromJson(reply->readAll()).object());
            });
}

// ==================== NEOFORGE ====================

void MinecraftDownloader::fetchNeoForgeVersions()
{
    qDebug() << "Request NeoForge versions";

    QUrl url("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                qDebug() << "NeoForge finished" << reply->errorString();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                emit neoforgeVersionReceived(QString(reply->readAll()));
            });
}

// ==================== DOWNLOAD FILE ====================

void MinecraftDownloader::downloadFile(const QUrl& url, const QString& outputPath)
{
    // Skip if already downloaded
    if (QFileInfo::exists(outputPath))
    {
        emit fileDownloaded(outputPath);
        return;
    }

    QDir().mkpath(QFileInfo(outputPath).path());

    QNetworkReply* reply = manager.get(QNetworkRequest(url));
    QSaveFile* file = new QSaveFile(outputPath);

    if (!file->open(QIODevice::WriteOnly))
    {
        emit errorOccurred("Не удалось открыть файл для записи: " + outputPath);
        file->deleteLater();
        reply->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file]()
            {
                file->write(reply->readAll());
            });

    connect(reply, &QNetworkReply::downloadProgress,
            this, &MinecraftDownloader::downloadProgress);

    connect(reply, &QNetworkReply::finished, this, [this, reply, file, outputPath]()
            {
                // Flush any remaining bytes (readyRead may not have caught the last chunk)
                QByteArray remaining = reply->readAll();
                if (!remaining.isEmpty())
                    file->write(remaining);

                if (reply->error() != QNetworkReply::NoError)
                {
                    file->cancelWriting();
                    emit errorOccurred(reply->errorString());
                }
                else
                {
                    file->commit();
                    emit fileDownloaded(outputPath);
                }

                file->deleteLater();
                reply->deleteLater();
            });
}

// ==================== DOWNLOAD VANILLA VERSION ====================

void MinecraftDownloader::downloadVanillaVersion(
    const QString& versionJsonUrl,
    const QString& outputJar)
{
    QNetworkReply* reply = manager.get(QNetworkRequest(QUrl(versionJsonUrl)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, outputJar]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QString clientUrl = doc.object()["downloads"]
                                        .toObject()["client"]
                                        .toObject()["url"]
                                        .toString();

                if (!clientUrl.isEmpty())
                    downloadFile(QUrl(clientUrl), outputJar);
                else
                    emit errorOccurred("Не удалось найти ссылку на клиент");
            });
}

// ==================== CREATE INSTANCE ====================

void MinecraftDownloader::createInstance(
    const QString& minecraftVersion,
    const QString& modLoader,
    const QString& modLoaderVersion,
    const QString& instancePath)
{
    // instancePath is the root game directory (e.g. .minecraft).
    // We store version files flat under versions/<id>/ inside it,
    // matching the path that on_PlayButton_clicked expects.

    QString versionsDir  = instancePath + "/versions/" + minecraftVersion;
    QString librariesDir = instancePath + "/libraries";
    QString assetsDir    = instancePath + "/assets";
    QString indexesDir   = assetsDir + "/indexes";
    QString objectsDir   = assetsDir + "/objects";
    QString nativesDir   = versionsDir + "/natives";

    QDir().mkpath(versionsDir);
    QDir().mkpath(librariesDir);
    QDir().mkpath(indexesDir);
    QDir().mkpath(objectsDir);
    QDir().mkpath(nativesDir);

    emit totalProgress(0);

    // ── Step 1: fetch version manifest ──────────────────────────────────────

    QNetworkReply* manifestReply = manager.get(
        QNetworkRequest(QUrl(
            "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")));

    connect(manifestReply, &QNetworkReply::finished, this, [=]()
            {
                manifestReply->deleteLater();

                if (manifestReply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(manifestReply->errorString());
                    return;
                }

                QJsonDocument manifestDoc =
                    QJsonDocument::fromJson(manifestReply->readAll());

                QJsonArray versions =
                    manifestDoc.object()["versions"].toArray();

                QString versionUrl;

                for (const auto& value : versions)
                {
                    QJsonObject obj = value.toObject();
                    if (obj["id"].toString() == minecraftVersion)
                    {
                        versionUrl = obj["url"].toString();
                        break;
                    }
                }

                if (versionUrl.isEmpty())
                {
                    emit errorOccurred("Version not found: " + minecraftVersion);
                    return;
                }

                // ── Step 2: fetch version JSON ───────────────────────────────────────

                QNetworkReply* versionReply =
                    manager.get(QNetworkRequest(QUrl(versionUrl)));

                connect(versionReply, &QNetworkReply::finished, this, [=]()
                        {
                            versionReply->deleteLater();

                            if (versionReply->error() != QNetworkReply::NoError)
                            {
                                emit errorOccurred(versionReply->errorString());
                                return;
                            }

                            QByteArray versionData = versionReply->readAll();

                            // Save version JSON
                            QString versionJsonPath =
                                versionsDir + "/" + minecraftVersion + ".json";

                            QFile versionFile(versionJsonPath);
                            if (versionFile.open(QIODevice::WriteOnly))
                            {
                                versionFile.write(versionData);
                                versionFile.close();
                            }

                            QJsonObject root =
                                QJsonDocument::fromJson(versionData).object();

                            // ── Step 3: download client JAR ──────────────────────────────────

                            QString clientUrl =
                                root["downloads"].toObject()["client"]
                                    .toObject()["url"].toString();

                            QString jarPath =
                                versionsDir + "/" + minecraftVersion + ".jar";

                            if (!clientUrl.isEmpty())
                                downloadFile(QUrl(clientUrl), jarPath);
                            else
                                emit errorOccurred("Не найдена ссылка на client.jar");

                            // ── Step 4: download libraries (with OS rules check) ─────────────

                            QJsonArray libraries = root["libraries"].toArray();

                            totalFiles    = 0;
                            completedFiles = 0;

                            for (const auto& value : libraries)
                            {
                                QJsonObject lib = value.toObject();

                                // Skip libraries not meant for this OS
                                if (!libraryAllowedOnCurrentOS(lib))
                                    continue;

                                QJsonObject downloads = lib["downloads"].toObject();

                                // Artifact (regular jar)
                                if (downloads.contains("artifact"))
                                {
                                    QJsonObject artifact = downloads["artifact"].toObject();
                                    QString url  = artifact["url"].toString();
                                    QString path = artifact["path"].toString();

                                    if (!url.isEmpty() && !path.isEmpty())
                                    {
                                        QString fullPath = librariesDir + "/" + path;
                                        QDir().mkpath(QFileInfo(fullPath).path());
                                        ++totalFiles;
                                        downloadFile(QUrl(url), fullPath);
                                    }
                                }

#ifdef Q_OS_WIN
                                // Native classifiers (Windows)
                                if (downloads.contains("classifiers"))
                                {
                                    QJsonObject classifiers =
                                        downloads["classifiers"].toObject();

                                    QString nativeKey;

                                    if (classifiers.contains("natives-windows"))
                                        nativeKey = "natives-windows";
                                    else if (classifiers.contains("natives-windows-64"))
                                        nativeKey = "natives-windows-64";

                                    if (!nativeKey.isEmpty())
                                    {
                                        QJsonObject nativeObj =
                                            classifiers[nativeKey].toObject();

                                        QString nativeUrl  = nativeObj["url"].toString();
                                        QString nativePath =
                                            librariesDir + "/" +
                                            nativeObj["path"].toString();

                                        QDir().mkpath(QFileInfo(nativePath).path());
                                        ++totalFiles;
                                        downloadFile(QUrl(nativeUrl), nativePath);
                                    }
                                }
#elif defined(Q_OS_MAC)
                                if (downloads.contains("classifiers"))
                                {
                                    QJsonObject classifiers =
                                        downloads["classifiers"].toObject();

                                    QString nativeKey;

                                    if (classifiers.contains("natives-osx"))
                                        nativeKey = "natives-osx";
                                    else if (classifiers.contains("natives-macos"))
                                        nativeKey = "natives-macos";

                                    if (!nativeKey.isEmpty())
                                    {
                                        QJsonObject nativeObj =
                                            classifiers[nativeKey].toObject();

                                        QString nativeUrl  = nativeObj["url"].toString();
                                        QString nativePath =
                                            librariesDir + "/" +
                                            nativeObj["path"].toString();

                                        QDir().mkpath(QFileInfo(nativePath).path());
                                        ++totalFiles;
                                        downloadFile(QUrl(nativeUrl), nativePath);
                                    }
                                }
#else
                                if (downloads.contains("classifiers"))
                                {
                                    QJsonObject classifiers =
                                        downloads["classifiers"].toObject();

                                    if (classifiers.contains("natives-linux"))
                                    {
                                        QJsonObject nativeObj =
                                            classifiers["natives-linux"].toObject();

                                        QString nativeUrl  = nativeObj["url"].toString();
                                        QString nativePath =
                                            librariesDir + "/" +
                                            nativeObj["path"].toString();

                                        QDir().mkpath(QFileInfo(nativePath).path());
                                        ++totalFiles;
                                        downloadFile(QUrl(nativeUrl), nativePath);
                                    }
                                }
#endif
                            }

                            // ── Step 5: fetch asset index ────────────────────────────────────

                            QJsonObject assetIndex  = root["assetIndex"].toObject();
                            QString assetIndexId    = assetIndex["id"].toString();
                            QString assetIndexUrl   = assetIndex["url"].toString();
                            QString assetIndexPath  = indexesDir + "/" + assetIndexId + ".json";

                            QNetworkReply* assetReply =
                                manager.get(QNetworkRequest(QUrl(assetIndexUrl)));

                            connect(assetReply, &QNetworkReply::finished, this, [=]()
                                    {
                                        assetReply->deleteLater();

                                        if (assetReply->error() != QNetworkReply::NoError)
                                        {
                                            emit errorOccurred(assetReply->errorString());
                                            return;
                                        }

                                        QByteArray assetData = assetReply->readAll();

                                        // Save asset index JSON
                                        QFile assetFile(assetIndexPath);
                                        if (assetFile.open(QIODevice::WriteOnly))
                                        {
                                            assetFile.write(assetData);
                                            assetFile.close();
                                        }

                                        QJsonObject objects =
                                            QJsonDocument::fromJson(assetData)
                                                .object()["objects"].toObject();

                                        int total      = objects.size();
                                        int* downloaded = new int(0); // heap so lambda captures work

                                        if (total == 0)
                                        {
                                            emit instanceCreated(instancePath);
                                            delete downloaded;
                                            return;
                                        }

                                        for (auto it = objects.begin(); it != objects.end(); ++it)
                                        {
                                            QString hash   = it.value().toObject()["hash"].toString();
                                            QString subdir = hash.left(2);

                                            QString objectUrl =
                                                "https://resources.download.minecraft.net/" +
                                                subdir + "/" + hash;

                                            QString objectPath =
                                                objectsDir + "/" + subdir + "/" + hash;

                                            QDir().mkpath(QFileInfo(objectPath).path());

                                            // Skip already-cached assets
                                            if (QFileInfo::exists(objectPath))
                                            {
                                                ++(*downloaded);
                                                emit totalProgress((*downloaded * 100) / total);
                                                if (*downloaded >= total)
                                                {
                                                    emit instanceCreated(instancePath);
                                                    delete downloaded;
                                                }
                                                continue;
                                            }

                                            QNetworkReply* objReply =
                                                manager.get(QNetworkRequest(QUrl(objectUrl)));

                                            QFile* file = new QFile(objectPath);
                                            file->open(QIODevice::WriteOnly);

                                            connect(objReply, &QNetworkReply::readyRead, this,
                                                    [objReply, file]()
                                                    {
                                                        file->write(objReply->readAll());
                                                    });

                                            connect(objReply, &QNetworkReply::finished, this,
                                                    [=]() mutable
                                                    {
                                                        // Flush any final bytes
                                                        QByteArray tail = objReply->readAll();
                                                        if (!tail.isEmpty())
                                                            file->write(tail);

                                                        file->close();
                                                        file->deleteLater();
                                                        objReply->deleteLater();

                                                        ++(*downloaded);
                                                        emit totalProgress((*downloaded * 100) / total);

                                                        if (*downloaded >= total)
                                                        {
                                                            emit instanceCreated(instancePath);
                                                            delete downloaded;
                                                        }
                                                    });
                                        }
                                    });
                        });
            });
}