//
// Created by ghhg6 on 04.07.2026.
//

#include "javadownloader.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>

// ==================== JAVA RUNTIME ====================

JavaDownloader::JavaDownloader(Downloader &downloader) : Downloader(downloader) {}


void JavaDownloader::downloadJavaRuntime(const QString& component,
                                         const QString& outputDir) {
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

    QNetworkReply* allReply = manager->get(QNetworkRequest(QUrl(allUrl)));

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
                    manager->get(QNetworkRequest(QUrl(manifestUrl)));

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
                                    manager->get(QNetworkRequest(QUrl(item.url)));
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
