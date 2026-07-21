#pragma once
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QVector>
#include "downloading/downloader.h"
#include "downloading/javadownloader.h"
#include "downloading/loaderinstaller.h"
#include "downloading/manifestdownloader.h"
#include "downloading/minecraftdownloader.h"

struct MinecraftVersion;

class MinecraftInstaller : public QObject
{
    Q_OBJECT
public:
    explicit MinecraftInstaller(QObject* parent = nullptr);

    void createInstance(const QString& minecraftVersion, const QString& modLoader, const QString& modLoaderVersion, const QString& instancePath);
    void downloadJavaRuntime(const QString& component, const QString& outputDir);

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
    QNetworkAccessManager *m_manager;
    ManifestDownloader m_manifestDownloader;
    MinecraftDownloader m_minecraftDownloader;
    JavaDownloader m_javaDownloader;
    LoaderInstaller m_loaderInstaller;
};
