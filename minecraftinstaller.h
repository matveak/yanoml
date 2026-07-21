#pragma once
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QVector>
#include "downloading/downloader.h"
#include "downloading/javadownloader.h"
#include "downloading/manifestdownloader.h"

struct MinecraftVersion;

class MinecraftInstaller : public QObject
{
    Q_OBJECT
public:
    explicit MinecraftInstaller(QObject* parent = nullptr);

    void downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJarPath);
    void createInstance(const QString& minecraftVersion, const QString& modLoader, const QString& modLoaderVersion, const QString& instancePath);
    void downloadJavaRuntime(const QString& component, const QString& outputDir);
    void installFabric(const QString& mcVersion, const QString& gameDir);
    void installForgeLike(const QString& mcVersion, const QString& loader,
                          const QString& javaExe, const QString& gameDir);
    static QString findInstalledLoaderId(const QString& gameDir,
                                         const QString& loader,
                                         const QString& mcVersion);
    //TODO: private
    Downloader d;
    ManifestDownloader md;
    JavaDownloader jd;

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
private:

    int totalFiles = 0;
    int completedFiles = 0;
    void downloadAssetObject(const QUrl& url,
                             const QString& outputPath,
                             const QString& expectedHash,
                             int* downloaded,
                             int total,
                             const QString& instancePath,
                             int attempt);
    void markAssetDone(int* downloaded, int total, const QString& instancePath);
    void runLoaderInstaller(const QUrl& installerUrl, const QString& mcVersion,
                            const QString& loader, const QString& javaExe,
                            const QString& gameDir);
};
