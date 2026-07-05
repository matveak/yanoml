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
#include <QQueue>
#include "downloading/downloader.h"
#include "downloading/javadownloader.h"
#include "downloading/manifestdownloader.h"

struct MinecraftVersion;

class MinecraftDownloader : public QObject
{
    Q_OBJECT
public:
    explicit MinecraftDownloader(QObject* parent = nullptr);

    void downloadVanillaVersion(const QString& versionJsonUrl, const QString& outputJarPath);
    void createInstance(const QString& minecraftVersion, const QString& modLoader, const QString& modLoaderVersion, const QString& instancePath);

    // Скачивает официальную Java от Mojang (component, например "jre-legacy").
    // По завершении испускает javaRuntimeReady с путём к java/javaw.
    void downloadJavaRuntime(const QString& component, const QString& outputDir);

    // ── Установка модлоадеров ───────────────────────────────────────────────
    // Скачивает профиль Fabric и его библиотеки, сохраняет версию загрузчика
    // в versions/<id>/. По завершении испускает loaderInstalled(id).
    void installFabric(const QString& mcVersion, const QString& gameDir);

    // Скачивает официальный установщик Forge/NeoForge и запускает его в режиме
    // --installClient через переданный javaExe. По завершении — loaderInstalled.
    void installForgeLike(const QString& mcVersion, const QString& loader,
                          const QString& javaExe, const QString& gameDir);

    // Преобразует Maven-координаты (group:artifact:version[:classifier][@ext])
    // в относительный путь внутри libraries/.
    static QString mavenNameToPath(const QString& name);

    // Ищет установленную версию загрузчика (по inheritsFrom == mcVersion и
    // имени каталога, содержащему имя загрузчика). Возвращает id или "".
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

    // Загрузчик (Fabric/Forge/NeoForge) установлен; versionId — id версии.
    void loaderInstalled(const QString& versionId);
private:

    int totalFiles = 0;
    int completedFiles = 0;

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

    // Скачивает установщик Forge/NeoForge (если ещё нет) и запускает его
    // в режиме --installClient.
    void runLoaderInstaller(const QUrl& installerUrl, const QString& mcVersion,
                            const QString& loader, const QString& javaExe,
                            const QString& gameDir);
};
