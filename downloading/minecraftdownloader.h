//
// Created by ghhg6 on 04.07.2026.
//

#ifndef MINECRAFTDOWNLOADER_H
#define MINECRAFTDOWNLOADER_H
#include "downloader.h"



class MinecraftDownloader : public Downloader {
    void downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJarPath);
    void downloadAssetObject(const QUrl &url, const QString &outputPath, const QString &expectedHash, const int *downloaded, int total, const
                             QString &instancePath, int attempt);
    void markAssetDone(int* downloaded, int total, const QString& instancePath);

    MinecraftDownloader(QNetworkAccessManager *manager, QObject * parent);
};



#endif //MINECRAFTDOWNLOADER_H
