#pragma once

#include <QObject>
#include <QUrl>
#include <QRgb>
#include <QUrlQuery>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

enum class SortOrder
{
    relevance,
    downloads,
    follows,
    newest,
    updated
};

QString Order2String(SortOrder order);

struct ModProject
{
    QString title;
    QString description;
    QString body;

    QString author;

    QString license;

    QString iconUrl;

    QString updated;

    int downloads = 0;

    QVector<QString> categories;

    QVector<QString> gallery;
};
struct Mod
{
    QString id;
    QString name;
    QString description;

    QVector<QString> categories;

    size_t downloads = 0;

    QUrl iconURL;
    QRgb color = 0;

    QString author;

    QVector<QString> versions;

    QString dateCreated;
    QString dateUpdated;
};

struct ModFile
{
    QString fileName;
    QUrl url;
    bool primary = false;
};

struct ModVersion
{
    QString id;
    QString versionNumber;
    QString minecraftVersion;
    QString loader;

    QVector<ModFile> files;
};

class ModrithAPI : public QObject
{
    Q_OBJECT

public:
    explicit ModrithAPI(QObject* parent = nullptr);

    void getMods(QString query,
                 SortOrder order = SortOrder::relevance,
                 int first = 0,
                 int count = 10);

    void getDownloadLinks(QString slug,
                          QString minecraftVersion,
                          QString loader = "");
    void getProject(QString slug);
signals:
    void ProjectReceived(
        const ModProject& project);
    void ModList(const QVector<Mod>& mods);

    void DownloadLinks(const QVector<QUrl>& urls);
    void OnError(const QString& error);
public:
    QNetworkAccessManager manager;

private:

    QString apiUrl =
        "https://api.modrinth.com/v2";
};