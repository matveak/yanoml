//
// Created by kamanodzu on 21.07.2026.
//

#ifndef YANOML_LOADERINSTALLER_H
#define YANOML_LOADERINSTALLER_H
#include <QString>

#include "downloader.h"


struct MinecraftVersion;
class QNetworkAccessManager;
class QString;
class QUrl;

class LoaderInstaller : public Downloader{
public:
    static QString findInstalledLoaderId(const QString& gameDir, const QString& loader, const QString& mcVersion);
    void installFabric(const QString& mcVersion, const QString& gameDir);
    void installForgeLike(const QString& mcVersion, const QString& loader, const QString& javaExe, const QString& gameDir);
    void runLoaderInstaller(const QUrl& installerUrl, const QString& mcVersion, const QString& loader, const QString& javaExe, const QString& gameDir);

    explicit LoaderInstaller(QNetworkAccessManager *manager);
signals:
    void vanillaVersionsReceived(const QVector<MinecraftVersion>& versions);
    void fabricVersionsReceived(const QJsonArray& versions);
    void forgeVersionsReceived(const QJsonObject& promotions);
    void neoforgeVersionReceived(const QString& latestVersion);
    void totalProgress(int percent);
    void fileDownloaded(const QString& filePath);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void errorOccurred(const QString& errorString);
    void instanceCreated(QString path);
    void javaRuntimeProgress(int percent);
    void javaRuntimeReady(const QString& javaExecutable);
    void loaderInstalled(const QString& versionId);
};


#endif //YANOML_LOADERINSTALLER_H
