#include "MinecraftDownloader.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QTimer>
#include <QProcess>

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

void MinecraftDownloader::downloadFile(const QUrl& url,
                                       const QString& outputPath)
{
    if (QFileInfo::exists(outputPath))
    {
        emit fileDownloaded(outputPath);
        return;
    }

    downloadQueue.enqueue({url, outputPath});
    startNextDownload();
}

void MinecraftDownloader::startNextDownload()
{
    while (activeDownloads < MaxParallelDownloads &&
           !downloadQueue.isEmpty())
    {
        DownloadTask task = downloadQueue.dequeue();
        startDownload(task);
    }
}

void MinecraftDownloader::startDownload(const DownloadTask& task)
{
    QDir().mkpath(QFileInfo(task.outputPath).path());

    QNetworkReply* reply =
        manager.get(QNetworkRequest(task.url));

    QSaveFile* file = new QSaveFile(task.outputPath);

    if (!file->open(QIODevice::WriteOnly))
    {
        emit errorOccurred(
            "Не удалось открыть файл для записи: " +
            task.outputPath);

        file->deleteLater();
        reply->deleteLater();

        startNextDownload();
        return;
    }

    ++activeDownloads;

    connect(reply,
            &QNetworkReply::readyRead,
            this,
            [reply, file]()
            {
                file->write(reply->readAll());
            });

    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            &MinecraftDownloader::downloadProgress);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, file, task]()
            {
                QByteArray tail = reply->readAll();
                if (!tail.isEmpty())
                    file->write(tail);

                if (reply->error() != QNetworkReply::NoError)
                {
                    file->cancelWriting();
                    emit errorOccurred(reply->errorString());
                }
                else
                {
                    file->commit();
                    emit fileDownloaded(task.outputPath);
                }

                file->deleteLater();
                reply->deleteLater();

                --activeDownloads;

                startNextDownload();
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
                    qDebug() << reply->errorString();
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
                                            int     expSize = it.value().toObject()["size"].toInt();
                                            QString subdir = hash.left(2);

                                            QString objectUrl =
                                                "https://resources.download.minecraft.net/" +
                                                subdir + "/" + hash;

                                            QString objectPath =
                                                objectsDir + "/" + subdir + "/" + hash;

                                            QDir().mkpath(QFileInfo(objectPath).path());

                                            // Используем кэш только если файл цел (совпадает размер).
                                            // Битые/обрезанные объекты (например иконка окна)
                                            // перекачиваются заново — иначе краш "Could not load icon".
                                            if (QFileInfo::exists(objectPath))
                                            {
                                                if (expSize <= 0 ||
                                                    QFileInfo(objectPath).size() == expSize)
                                                {
                                                    markAssetDone(downloaded, total, instancePath);
                                                    continue;
                                                }

                                                QFile::remove(objectPath);
                                            }

                                            downloadAssetObject(QUrl(objectUrl), objectPath, hash,
                                                                downloaded, total, instancePath, 0);
                                        }
                                    });
                        });
            });
}

// ==================== ASSET OBJECT ====================

void MinecraftDownloader::markAssetDone(int* downloaded, int total,
                                        const QString& instancePath)
{
    ++(*downloaded);
    emit totalProgress((*downloaded * 100) / total);

    if (*downloaded >= total)
    {
        emit instanceCreated(instancePath);
        delete downloaded;
    }
}

void MinecraftDownloader::downloadAssetObject(const QUrl& url,
                                              const QString& outputPath,
                                              const QString& expectedHash,
                                              int* downloaded,
                                              int total,
                                              const QString& instancePath,
                                              int attempt)
{
    QNetworkReply*      reply = manager.get(QNetworkRequest(url));
    QSaveFile*          file  = new QSaveFile(outputPath);
    QCryptographicHash* sha1  = new QCryptographicHash(QCryptographicHash::Sha1);

    if (!file->open(QIODevice::WriteOnly))
    {
        delete sha1;
        file->deleteLater();
        reply->deleteLater();
        emit errorOccurred("Не удалось открыть файл для записи: " + outputPath);
        markAssetDone(downloaded, total, instancePath);
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file, sha1]()
            {
                QByteArray chunk = reply->readAll();
                file->write(chunk);
                sha1->addData(chunk);
            });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, sha1, url, outputPath, expectedHash,
             downloaded, total, instancePath, attempt]()
            {
                QByteArray tail = reply->readAll();
                if (!tail.isEmpty())
                {
                    file->write(tail);
                    sha1->addData(tail);
                }

                bool    netOk   = (reply->error() == QNetworkReply::NoError);
                QString gotHash = QString::fromLatin1(sha1->result().toHex());
                bool    hashOk  = expectedHash.isEmpty() || (gotHash == expectedHash);

                delete sha1;
                if (!netOk)
                {
                    qDebug() << reply->error() << reply->errorString();
                }

                if (netOk && !hashOk)
                {
                    qDebug() << "SHA1 mismatch";
                }
                reply->deleteLater();

                if (netOk && hashOk)
                {
                    file->commit();
                    file->deleteLater();
                    markAssetDone(downloaded, total, instancePath);
                    return;
                }



                // Не оставляем битый файл на диске.
                file->cancelWriting();
                file->deleteLater();

                if (attempt < 10)
                {
                    QTimer::singleShot(1000 * (attempt + 1), this,
                        [=]
                        {
                            qDebug() << "Ошибка при установки файла \"" + outputPath + "\". Попытка " + QString::fromStdString(std::to_string(attempt)) + "/10";
                            downloadAssetObject(url,
                                                outputPath,
                                                expectedHash,
                                                downloaded,
                                                total,
                                                instancePath,
                                                attempt + 1);
                        });
                    return;
                }

                emit errorOccurred("Не удалось скачать ассет (битый файл): " + outputPath);
                markAssetDone(downloaded, total, instancePath);
            });
}

// ==================== JAVA RUNTIME ====================

void MinecraftDownloader::downloadJavaRuntime(const QString& component,
                                              const QString& outputDir)
{
#if defined(Q_OS_WIN)
#  if defined(Q_PROCESSOR_ARM)
    const QString platformKey = "windows-arm64";
#  else
    const QString platformKey = "windows-x64";
#  endif
#elif defined(Q_OS_MAC)
#  if defined(Q_PROCESSOR_ARM)
    const QString platformKey = "mac-os-arm64";
#  else
    const QString platformKey = "mac-os";
#  endif
#else
#  if defined(Q_PROCESSOR_X86_32)
    const QString platformKey = "linux-i386";
#  else
    const QString platformKey = "linux";
#  endif
#endif

    QDir().mkpath(outputDir);

    // Список всех доступных Java runtime от Mojang.
    const QString allUrl =
        "https://launchermeta.mojang.com/v1/products/java-runtime/"
        "2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";

    QNetworkReply* allReply = manager.get(QNetworkRequest(QUrl(allUrl)));

    connect(allReply, &QNetworkReply::finished, this,
            [this, allReply, platformKey, component, outputDir]()
            {
                allReply->deleteLater();

                if (allReply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(allReply->errorString());
                    return;
                }

                QJsonObject root =
                    QJsonDocument::fromJson(allReply->readAll()).object();

                QJsonArray comps =
                    root[platformKey].toObject()[component].toArray();

                if (comps.isEmpty())
                {
                    emit errorOccurred("Java runtime не найдена: " +
                                       platformKey + "/" + component);
                    return;
                }

                QString manifestUrl = comps[0].toObject()["manifest"]
                                          .toObject()["url"].toString();

                if (manifestUrl.isEmpty())
                {
                    emit errorOccurred("Нет ссылки на manifest Java runtime");
                    return;
                }

                // ── Manifest со списком файлов runtime ───────────────────────
                QNetworkReply* manReply =
                    manager.get(QNetworkRequest(QUrl(manifestUrl)));

                connect(manReply, &QNetworkReply::finished, this,
                        [this, manReply, outputDir]()
                        {
                            manReply->deleteLater();

                            if (manReply->error() != QNetworkReply::NoError)
                            {
                                emit errorOccurred(manReply->errorString());
                                return;
                            }

                            QJsonObject files =
                                QJsonDocument::fromJson(manReply->readAll())
                                    .object()["files"].toObject();

                            struct DlItem { QString url; QString path; bool executable; };
                            QVector<DlItem> items;
                            QString javaExe;

                            for (auto it = files.begin(); it != files.end(); ++it)
                            {
                                const QString rel  = it.key();
                                QJsonObject entry  = it.value().toObject();
                                const QString type = entry["type"].toString();
                                const QString outPath = outputDir + "/" + rel;

                                if (type == "directory")
                                {
                                    QDir().mkpath(outPath);
                                    continue;
                                }
                                if (type != "file")
                                    continue; // symlink — пропускаем

                                QDir().mkpath(QFileInfo(outPath).path());

                                const bool exe = entry["executable"].toBool();

                                if (rel.endsWith("bin/javaw.exe") ||
                                    (javaExe.isEmpty() && rel.endsWith("bin/java")))
                                    javaExe = outPath;

                                QString rawUrl = entry["downloads"].toObject()["raw"]
                                                     .toObject()["url"].toString();
                                if (rawUrl.isEmpty())
                                    continue;

                                items.push_back({ rawUrl, outPath, exe });
                            }

                            if (items.isEmpty())
                            {
                                emit errorOccurred("Manifest Java runtime пуст");
                                return;
                            }
                            if (javaExe.isEmpty())
                                javaExe = outputDir + "/bin/javaw.exe";

                            const int total = items.size();
                            int* done = new int(0);

                            auto finishOne = [this, total, done, javaExe]()
                            {
                                ++(*done);
                                emit javaRuntimeProgress((*done * 100) / total);
                                if (*done >= total)
                                {
                                    emit javaRuntimeReady(javaExe);
                                    delete done;
                                }
                            };

                            for (const DlItem& item : items)
                            {
                                if (QFileInfo::exists(item.path))
                                {
                                    finishOne();
                                    continue;
                                }

                                QNetworkReply* fileReply =
                                    manager.get(QNetworkRequest(QUrl(item.url)));
                                QFile* f = new QFile(item.path);

                                if (!f->open(QIODevice::WriteOnly))
                                {
                                    f->deleteLater();
                                    fileReply->deleteLater();
                                    finishOne();
                                    continue;
                                }

                                connect(fileReply, &QNetworkReply::readyRead, this,
                                        [fileReply, f]()
                                        {
                                            f->write(fileReply->readAll());
                                        });

                                connect(fileReply, &QNetworkReply::finished, this,
                                        [=]()
                                        {
                                            QByteArray tail = fileReply->readAll();
                                            if (!tail.isEmpty())
                                                f->write(tail);
                                            f->close();

                                            if (item.executable)
                                                f->setPermissions(f->permissions()
                                                    | QFileDevice::ExeOwner
                                                    | QFileDevice::ExeGroup
                                                    | QFileDevice::ExeOther);

                                            f->deleteLater();
                                            fileReply->deleteLater();
                                            finishOne();
                                        });
                            }
                        });
            });
}

// ==================== MOD LOADERS ====================

QString MinecraftDownloader::mavenNameToPath(const QString& name)
{
    // group:artifact:version[:classifier][@ext]
    QString work = name;
    QString ext = "jar";

    const int at = work.indexOf('@');
    if (at >= 0)
    {
        ext  = work.mid(at + 1);
        work = work.left(at);
    }

    const QStringList parts = work.split(':');
    if (parts.size() < 3)
        return QString();

    QString group      = parts[0];
    const QString artifact   = parts[1];
    const QString version    = parts[2];
    const QString classifier = parts.size() >= 4 ? parts[3] : QString();

    QString file = artifact + "-" + version;
    if (!classifier.isEmpty())
        file += "-" + classifier;
    file += "." + ext;

    return group.replace('.', '/') + "/" + artifact + "/" + version + "/" + file;
}

QString MinecraftDownloader::findInstalledLoaderId(const QString& gameDir,
                                                   const QString& loader,
                                                   const QString& mcVersion)
{
    const QString versionsRoot = gameDir + "/versions";
    QDir d(versionsRoot);
    if (!d.exists())
        return QString();

    const QString loaderLower = loader.toLower();
    QString best;

    const QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& dir : dirs)
    {
        const QString lower = dir.toLower();

        bool match = false;
        if (loaderLower == "fabric")        match = lower.contains("fabric");
        else if (loaderLower == "quilt")    match = lower.contains("quilt");
        else if (loaderLower == "neoforge") match = lower.contains("neoforge");
        else if (loaderLower == "forge")    match = lower.contains("forge") &&
                                                    !lower.contains("neoforge");
        if (!match)
            continue;

        QFile f(versionsRoot + "/" + dir + "/" + dir + ".json");
        if (!f.open(QIODevice::ReadOnly))
            continue;

        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();

        if (obj["inheritsFrom"].toString() != mcVersion)
            continue;

        // Берём лексикографически «наибольший» — обычно это более свежий билд.
        if (best.isEmpty() || dir > best)
            best = dir;
    }

    return best;
}

void MinecraftDownloader::installFabric(const QString& mcVersion, const QString& gameDir)
{
    const QString loaderListUrl =
        "https://meta.fabricmc.net/v2/versions/loader/" + mcVersion;

    QNetworkReply* listReply =
        manager.get(QNetworkRequest(QUrl(loaderListUrl)));

    connect(listReply, &QNetworkReply::finished, this, [=]()
            {
                listReply->deleteLater();

                if (listReply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(listReply->errorString());
                    return;
                }

                const QJsonArray arr =
                    QJsonDocument::fromJson(listReply->readAll()).array();

                if (arr.isEmpty())
                {
                    emit errorOccurred("Fabric не поддерживает версию " + mcVersion);
                    return;
                }

                const QString loaderVersion =
                    arr[0].toObject()["loader"].toObject()["version"].toString();

                if (loaderVersion.isEmpty())
                {
                    emit errorOccurred("Не удалось определить версию Fabric Loader");
                    return;
                }

                const QString profileUrl =
                    "https://meta.fabricmc.net/v2/versions/loader/" + mcVersion +
                    "/" + loaderVersion + "/profile/json";

                QNetworkReply* profReply =
                    manager.get(QNetworkRequest(QUrl(profileUrl)));

                connect(profReply, &QNetworkReply::finished, this, [=]()
                        {
                            profReply->deleteLater();

                            if (profReply->error() != QNetworkReply::NoError)
                            {
                                emit errorOccurred(profReply->errorString());
                                return;
                            }

                            const QByteArray profData = profReply->readAll();
                            const QJsonObject profile =
                                QJsonDocument::fromJson(profData).object();

                            QString versionId = profile["id"].toString();
                            if (versionId.isEmpty())
                                versionId = "fabric-loader-" + loaderVersion +
                                            "-" + mcVersion;

                            // Сохраняем профиль загрузчика как отдельную версию.
                            const QString verDir =
                                gameDir + "/versions/" + versionId;
                            QDir().mkpath(verDir);

                            QFile vf(verDir + "/" + versionId + ".json");
                            if (vf.open(QIODevice::WriteOnly))
                            {
                                vf.write(profData);
                                vf.close();
                            }

                            // Скачиваем библиотеки Fabric.
                            const QJsonArray libs =
                                profile["libraries"].toArray();
                            const QString libDir = gameDir + "/libraries";

                            struct Item { QUrl url; QString path; };
                            QVector<Item> items;

                            for (const auto& lv : libs)
                            {
                                const QJsonObject lib = lv.toObject();
                                const QString name = lib["name"].toString();
                                if (name.isEmpty())
                                    continue;

                                const QString rel = mavenNameToPath(name);
                                if (rel.isEmpty())
                                    continue;

                                QString base = lib["url"].toString();
                                if (base.isEmpty())
                                    base = "https://maven.fabricmc.net/";
                                if (!base.endsWith('/'))
                                    base += '/';

                                items.push_back({ QUrl(base + rel),
                                                  libDir + "/" + rel });
                            }

                            if (items.isEmpty())
                            {
                                emit loaderInstalled(versionId);
                                return;
                            }

                            const int total = items.size();
                            int* done = new int(0);

                            auto finishOne = [this, total, done, versionId]()
                            {
                                ++(*done);
                                emit totalProgress((*done * 100) / total);
                                if (*done >= total)
                                {
                                    emit loaderInstalled(versionId);
                                    delete done;
                                }
                            };

                            for (const Item& item : items)
                            {
                                if (QFileInfo::exists(item.path))
                                {
                                    finishOne();
                                    continue;
                                }

                                QDir().mkpath(QFileInfo(item.path).path());

                                QNetworkReply* r =
                                    manager.get(QNetworkRequest(item.url));
                                QSaveFile* f = new QSaveFile(item.path);

                                if (!f->open(QIODevice::WriteOnly))
                                {
                                    f->deleteLater();
                                    r->deleteLater();
                                    finishOne();
                                    continue;
                                }

                                connect(r, &QNetworkReply::readyRead, this,
                                        [r, f]() { f->write(r->readAll()); });

                                connect(r, &QNetworkReply::finished, this, [=]()
                                        {
                                            const QByteArray tail = r->readAll();
                                            if (!tail.isEmpty())
                                                f->write(tail);

                                            if (r->error() == QNetworkReply::NoError)
                                                f->commit();
                                            else
                                                f->cancelWriting();

                                            f->deleteLater();
                                            r->deleteLater();
                                            finishOne();
                                        });
                            }
                        });
            });
}

// Выбирает Maven-версию NeoForge, соответствующую версии Minecraft.
static QString pickNeoForgeMavenForMc(const QString& xml, const QString& mcVersion,
                                      bool allowPrerelease)
{
    auto toMc = [](const QString& v) -> QString
    {
        if (v.startsWith("47."))
            return "1.20.1";
        const QStringList parts = v.split('.');
        if (parts.size() >= 2)
        {
            bool okMajor = false, okMinor = false;
            const int major = parts[0].toInt(&okMajor);
            const int minor = parts[1].section('-', 0, 0).toInt(&okMinor);
            if (okMajor && okMinor)
                return minor == 0 ? QString("1.%1").arg(major)
                                  : QString("1.%1.%2").arg(major).arg(minor);
        }
        return v;
    };

    QString best;
    const QStringList lines = xml.split('\n');
    for (const QString& line : lines)
    {
        if (!line.contains("<version>"))
            continue;

        const QString v = QString(line)
                              .remove("<version>")
                              .remove("</version>")
                              .trimmed();

        const bool pre = v.contains("beta",  Qt::CaseInsensitive) ||
                         v.contains("alpha", Qt::CaseInsensitive) ||
                         v.contains("rc",    Qt::CaseInsensitive);
        if (pre && !allowPrerelease)
            continue;

        if (toMc(v) != mcVersion)
            continue;

        // Версии в metadata идут по возрастанию — берём последнюю подходящую.
        best = v;
    }

    return best;
}

void MinecraftDownloader::installForgeLike(const QString& mcVersion,
                                           const QString& loader,
                                           const QString& javaExe,
                                           const QString& gameDir)
{
    if (loader == "forge")
    {
        QNetworkReply* promoReply = manager.get(QNetworkRequest(QUrl(
            "https://files.minecraftforge.net/net/minecraftforge/forge/"
            "promotions_slim.json")));

        connect(promoReply, &QNetworkReply::finished, this, [=]()
                {
                    promoReply->deleteLater();

                    if (promoReply->error() != QNetworkReply::NoError)
                    {
                        emit errorOccurred(promoReply->errorString());
                        return;
                    }

                    const QJsonObject promos =
                        QJsonDocument::fromJson(promoReply->readAll())
                            .object()["promos"].toObject();

                    QString fv = promos.value(mcVersion + "-recommended").toString();
                    if (fv.isEmpty())
                        fv = promos.value(mcVersion + "-latest").toString();

                    if (fv.isEmpty())
                    {
                        emit errorOccurred("Нет версии Forge для " + mcVersion);
                        return;
                    }

                    const QString full = mcVersion + "-" + fv;
                    const QString url =
                        "https://maven.minecraftforge.net/net/minecraftforge/"
                        "forge/" + full + "/forge-" + full + "-installer.jar";

                    runLoaderInstaller(QUrl(url), mcVersion, "forge",
                                       javaExe, gameDir);
                });
    }
    else // neoforge
    {
        QNetworkReply* metaReply = manager.get(QNetworkRequest(QUrl(
            "https://maven.neoforged.net/releases/net/neoforged/neoforge/"
            "maven-metadata.xml")));

        connect(metaReply, &QNetworkReply::finished, this, [=]()
                {
                    metaReply->deleteLater();

                    if (metaReply->error() != QNetworkReply::NoError)
                    {
                        emit errorOccurred(metaReply->errorString());
                        return;
                    }

                    const QString xml = QString::fromUtf8(metaReply->readAll());
                    QString chosen = pickNeoForgeMavenForMc(xml, mcVersion, false);
                    if (chosen.isEmpty())
                        chosen = pickNeoForgeMavenForMc(xml, mcVersion, true);

                    if (chosen.isEmpty())
                    {
                        emit errorOccurred("Нет версии NeoForge для " + mcVersion);
                        return;
                    }

                    const QString url =
                        "https://maven.neoforged.net/releases/net/neoforged/"
                        "neoforge/" + chosen + "/neoforge-" + chosen +
                        "-installer.jar";

                    runLoaderInstaller(QUrl(url), mcVersion, "neoforge",
                                       javaExe, gameDir);
                });
    }
}

void MinecraftDownloader::runLoaderInstaller(const QUrl& installerUrl,
                                             const QString& mcVersion,
                                             const QString& loader,
                                             const QString& javaExe,
                                             const QString& gameDir)
{
    const QString installersDir = gameDir + "/.installers";
    QDir().mkpath(installersDir);
    const QString installerPath =
        installersDir + "/" + loader + "-" + mcVersion + "-installer.jar";

    auto runProc = [=]()
    {
        // Установщик Forge/NeoForge требует launcher_profiles.json в каталоге.
        const QString lp = gameDir + "/launcher_profiles.json";
        if (!QFileInfo::exists(lp))
        {
            QFile f(lp);
            if (f.open(QIODevice::WriteOnly))
            {
                f.write("{\n"
                        "  \"profiles\": {},\n"
                        "  \"selectedProfile\": \"\",\n"
                        "  \"clientToken\": \"\",\n"
                        "  \"authenticationDatabase\": {},\n"
                        "  \"launcherVersion\": { \"name\": \"yanoml\", "
                        "\"format\": 21 }\n"
                        "}");
                f.close();
            }
        }

        QProcess* proc = new QProcess(this);
        proc->setWorkingDirectory(gameDir);

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus)
                {
                    const QString out = proc->readAllStandardOutput() +
                                        proc->readAllStandardError();
                    proc->deleteLater();

                    if (code != 0)
                    {
                        emit errorOccurred("Установщик " + loader +
                                           " завершился с ошибкой:\n" +
                                           out.right(2000));
                        return;
                    }

                    const QString id =
                        findInstalledLoaderId(gameDir, loader, mcVersion);

                    if (id.isEmpty())
                    {
                        emit errorOccurred("Установщик " + loader +
                            " завершился, но версия не найдена в versions/.");
                        return;
                    }

                    const QString versionJson = gameDir + "/versions/" + id + "/" + id + ".json";
                    qDebug() << "downloading libraries";
                    downloadLibrariesFromVersionJson(versionJson, gameDir, [=]
                    {
                        emit loaderInstalled(id);
                    });
                });

        proc->start(javaExe, { "-jar", installerPath,
                               "--installClient", gameDir });

        if (!proc->waitForStarted(5000))
        {
            emit errorOccurred("Не удалось запустить установщик " + loader +
                               " (Java: " + javaExe + ")");
            proc->deleteLater();
        }
    };

    if (QFileInfo::exists(installerPath))
    {
        runProc();
        return;
    }

    QNetworkReply* r = manager.get(QNetworkRequest(installerUrl));
    QSaveFile* f = new QSaveFile(installerPath);

    if (!f->open(QIODevice::WriteOnly))
    {
        f->deleteLater();
        r->deleteLater();
        emit errorOccurred("Не удалось сохранить установщик " + loader);
        return;
    }

    connect(r, &QNetworkReply::downloadProgress,
            this, &MinecraftDownloader::downloadProgress);
    connect(r, &QNetworkReply::readyRead, this,
            [r, f]() { f->write(r->readAll()); });

    connect(r, &QNetworkReply::finished, this, [=]()
            {
                const QByteArray tail = r->readAll();
                if (!tail.isEmpty())
                    f->write(tail);

                const bool ok = (r->error() == QNetworkReply::NoError);
                if (ok)
                    f->commit();
                else
                    f->cancelWriting();

                f->deleteLater();
                r->deleteLater();

                if (!ok)
                {
                    emit errorOccurred("Не удалось скачать установщик " + loader);
                    return;
                }

                runProc();
            });
}

void MinecraftDownloader::downloadLibrariesFromVersionJson(
    const QString& versionJsonPath,
    const QString& gameDir,
    std::function<void()> onFinished)
{
    QFile file(versionJsonPath);

    if (!file.open(QIODevice::ReadOnly))
    {
        emit errorOccurred("Не удалось открыть " + versionJsonPath);
        return;
    }

    const QJsonObject root =
        QJsonDocument::fromJson(file.readAll()).object();

    file.close();

    struct Item
    {
        QUrl url;
        QString path;
    };

    QVector<Item> items;

    const QString librariesDir = gameDir + "/libraries";

    const QJsonArray libraries = root["libraries"].toArray();

    for (const auto& value : libraries)
    {
        const QJsonObject lib = value.toObject();

        if (!libraryAllowedOnCurrentOS(lib))
            continue;

        const QJsonObject downloads = lib["downloads"].toObject();

        //
        // обычная библиотека
        //
        if (downloads.contains("artifact"))
        {
            const QJsonObject artifact =
                downloads["artifact"].toObject();

            const QString url  = artifact["url"].toString();
            const QString path = artifact["path"].toString();

            if (!url.isEmpty() && !path.isEmpty())
            {
                items.push_back({
                    QUrl(url),
                    librariesDir + "/" + path
                });
            }
        }

        //
        // natives
        //
        if (downloads.contains("classifiers"))
        {
            const QJsonObject classifiers =
                downloads["classifiers"].toObject();

            QString nativeKey;

#ifdef Q_OS_WIN
            if (classifiers.contains("natives-windows"))
                nativeKey = "natives-windows";
            else if (classifiers.contains("natives-windows-64"))
                nativeKey = "natives-windows-64";
#elif defined(Q_OS_MAC)
            if (classifiers.contains("natives-osx"))
                nativeKey = "natives-osx";
            else if (classifiers.contains("natives-macos"))
                nativeKey = "natives-macos";
#else
            if (classifiers.contains("natives-linux"))
                nativeKey = "natives-linux";
#endif

            if (!nativeKey.isEmpty())
            {
                const QJsonObject native =
                    classifiers[nativeKey].toObject();

                const QString url  = native["url"].toString();
                const QString path = native["path"].toString();

                if (!url.isEmpty() && !path.isEmpty())
                {
                    items.push_back({
                        QUrl(url),
                        librariesDir + "/" + path
                    });
                }
            }
        }
    }

    if (items.isEmpty())
    {
        if (onFinished)
            onFinished();
        return;
    }

    auto remaining = std::make_shared<int>(items.size());

    qDebug() << "amount of items to be installed:" << items.size();

    for (const Item& item : items)
    {
        if (QFileInfo::exists(item.path))
        {
            qDebug() << "file" << item.path << "already exists";
            if (--(*remaining) == 0 && onFinished) {
                onFinished();
            }
            continue;
        }

        connect(this,
                &MinecraftDownloader::fileDownloaded,
                this,
                [this, remaining, item, onFinished](const QString& path)
                {
                    if (path != item.path)
                        return;

                    disconnect(this, nullptr, this, nullptr);

                    if (--(*remaining) == 0 && onFinished) {
                        qDebug() << "file" << item.path << "downloaded";
                        onFinished();
                    }
                },
                Qt::SingleShotConnection);
        qDebug() << "downloading file " << item.path;
        downloadFile(item.url, item.path);
    }
}