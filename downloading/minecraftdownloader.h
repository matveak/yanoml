//
// Created by ghhg6 on 04.07.2026.
//

#ifndef MINECRAFTDOWNLOADER_H
#define MINECRAFTDOWNLOADER_H
#include "downloader.h"


struct MinecraftVersion;

class MinecraftDownloader : public Downloader {
    Q_OBJECT
public:
    void downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJarPath);
    void downloadAssetObject(const QUrl &url, const QString &outputPath, const QString &expectedHash, int *downloaded, int total, const QString &instancePath, int attempt);
    void markAssetDone(int* downloaded, int total, const QString& instancePath);

    MinecraftDownloader(QNetworkAccessManager *manager);
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



#endif //MINECRAFTDOWNLOADER_H
