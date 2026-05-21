#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QVector>

struct MinecraftVersion
{
    QString gameVersion;
    QString loaderVersion;
    QString loaderType;

    QString url;
    QString releaseTime;
};
class MinecraftDownloader : public QObject
{
    Q_OBJECT
public:
    explicit MinecraftDownloader(QObject* parent = nullptr);

    void fetchVanillaVersions();
    void fetchFabricVersions();
    void fetchForgeVersions();
    void fetchNeoForgeVersions();

    void downloadFile(const QUrl& url, const QString& outputPath);
    void downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJarPath);
    void createInstance(const QString& minecraftVersion, const QString& modLoader, const QString& modLoaderVersion, const QString& instancePath);

signals:
    void vanillaVersionsReceived(const QVector<MinecraftVersion>& versions);
    void fabricVersionsReceived(const QJsonArray& versions);
    void forgeVersionsReceived(const QJsonObject& promotions);
    void neoforgeVersionReceived(const QString& latestVersion);

    void fileDownloaded(const QString& filePath);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void errorOccurred(const QString& errorString);
    void instanceCreated(QString path);
private:
    QNetworkAccessManager manager;

    void handleVanillaManifest(QNetworkReply* reply);
    void handleFabricManifest(QNetworkReply* reply);
    void handleForgeManifest(QNetworkReply* reply);
    void handleNeoForgeManifest(QNetworkReply* reply);
};