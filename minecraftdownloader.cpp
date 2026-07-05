#include "MinecraftDownloader.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QTimer>
#include <QProcess>

MinecraftDownloader::MinecraftDownloader(QObject* parent) : d{new QNetworkAccessManager(parent), parent}, md{d}, jd{d}
{

}

// ==================== DOWNLOAD VANILLA VERSION ====================

void MinecraftDownloader::downloadVanillaVersion(
    const QString& versionJsonUrl,
    const QString& outputJar)
{
    QNetworkReply* reply = d.manager->get(QNetworkRequest(QUrl(versionJsonUrl)));

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
                    d.downloadFile(QUrl(clientUrl), outputJar);
                else
                    emit errorOccurred("Не удалось найти ссылку на клиент");
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
    QNetworkReply*      reply = d.manager->get(QNetworkRequest(url));
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

// ==================== MOD LOADERS ====================

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

/*void MinecraftDownloader::installFabric(const QString& mcVersion, const QString& gameDir)
{
    const QString loaderListUrl =
        "https://meta.fabricmc.net/v2/versions/loader/" + mcVersion;

    QNetworkReply* listReply =
        d.manager->get(QNetworkRequest(QUrl(loaderListUrl)));

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
                    d.manager->get(QNetworkRequest(QUrl(profileUrl)));

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
                                    d.manager->get(QNetworkRequest(item.url));
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
}*/

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
        QNetworkReply* promoReply = d.manager->get(QNetworkRequest(QUrl(
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
        QNetworkReply* metaReply = d.manager->get(QNetworkRequest(QUrl(
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
                    d.downloadLibrariesFromVersionJson(versionJson, gameDir, [=]
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

    QNetworkReply* r = d.manager->get(QNetworkRequest(installerUrl));
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
