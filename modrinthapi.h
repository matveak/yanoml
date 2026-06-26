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
#include <QStringList>
#include <QVersionNumber>

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

struct TagInfo
{
    QString name;   // слаг, напр. "adventure" / "fabric"
    QString icon;   // сырой SVG-маркап из API Modrinth
    QString header; // группа категории ("categories" и т.п.)
};

struct Mod
{
    QString id;
    QString name;
    QString description;
    QVector<QString> categories;
    size_t downloads = 0;
    size_t follows = 0;
    QUrl iconURL;
    QRgb color = 0;
    QString author;
    QVector<QString> versions;
    QString dateCreated;
    QString dateUpdated;
};

class ModrithAPI : public QObject
{
    Q_OBJECT

public:
    explicit ModrithAPI(QObject* parent = nullptr);

    void getMods(QString query,
                 QString mcVersion = "",
                 QString loader = "",
                 QString category = "",
                 QString environment = "",
                 SortOrder order = SortOrder::relevance,
                 int first = 0,
                 int count = 10);

    void getDownloadLinks(QString slug,
                          QString minecraftVersion,
                          QString loader = "");

    void getProject(QString slug);
    void fetchAvailableVersions(const QString& loader);
    void fetchCategories();
    void fetchLoaders();

signals:
    void ProjectReceived(const ModProject& project);
    void ModList(const QVector<Mod>& mods);
    void DownloadLinks(const QVector<QUrl>& urls);
    void OnError(const QString& error);
    void AvailableVersions(const QString& loader, const QStringList& versions);
    void CategoriesReceived(const QVector<TagInfo>& categories);
    void LoadersReceived(const QVector<TagInfo>& loaders);

public:
    QNetworkAccessManager manager;

private:
    void useFallbackVersions(const QString& loader);
    QString apiUrl = "https://api.modrinth.com/v2";
};