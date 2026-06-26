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

    QString type;
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

    // Скачивает официальную Java от Mojang (component, например "jre-legacy").
    // По завершении испускает javaRuntimeReady с путём к java/javaw.
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
private:
    QNetworkAccessManager manager;
    int totalFiles = 0;
    int completedFiles = 0;
    void handleVanillaManifest(QNetworkReply* reply);
    void handleFabricManifest(QNetworkReply* reply);
    void handleForgeManifest(QNetworkReply* reply);
    void handleNeoForgeManifest(QNetworkReply* reply);

    // Скачивает один asset-объект с атомарной записью и проверкой SHA1.
    // При сбое/несовпадении хэша повторяет загрузку (до 3 попыток).
    void downloadAssetObject(const QUrl& url,
                             const QString& outputPath,
                             const QString& expectedHash,
                             int* downloaded,
                             int total,
                             const QString& instancePath,
                             int attempt);

    // Отмечает завершение одного ассета, двигает прогресс и испускает
    // instanceCreated, когда скачаны все объекты.
    void markAssetDone(int* downloaded, int total, const QString& instancePath);
};