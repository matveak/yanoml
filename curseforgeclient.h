#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QUrl>
#include <QUrlQuery>
#include "modsapi.h"

class CurseForgeClient : public ModsAPI
{
    Q_OBJECT

public:

    explicit CurseForgeClient(QObject *parent = nullptr);

    QString name() const override;
    QColor accentColor() const override;

    QStringList minecraftVersions() const override;
    QStringList loaders() const override;

    void searchMods(const QString& query, const QString& mcVersion, const QString& loader) override;

    void searchModpacks(const QString& query, const QString& mcVersion) override;

    void getProjectFiles(int projectId, const QString& mcVersion, const QString& loader) override;

private:

    QNetworkReply* apiGet(const QString& path, const QUrlQuery& query = {});

    static ModInfo parseMod(const QJsonObject& object);

    static FileInfo parseFile(const QJsonObject& object);

    void search(const QString& query, const QString& mcVersion, const QString& loader, bool modpacks, int pageSize = 20, int index = 0);

    QNetworkAccessManager m_nam;

    // Публичный CF API key (proxy key от CFWidget — без него работает для чтения)
    // Если у вас есть свой ключ — замените.
    const QString m_apiKey = "$2a$10$HY0vVz3lE0tURUI5QFDCNeWrbnjS3MMAKX3NHlOJmubSZKNGv4Cai";
    const QString m_base   = "https://api.curseforge.com/v1";
};