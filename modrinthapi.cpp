#include "modrinthapi.h"

#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <algorithm>

ModrithAPI::ModrithAPI(QObject* parent)
    : QObject(parent)
{
}

QString Order2String(SortOrder order)
{
    switch(order)
    {
    case SortOrder::relevance:
        return "relevance";

    case SortOrder::downloads:
        return "downloads";

    case SortOrder::follows:
        return "follows";

    case SortOrder::newest:
        return "newest";

    case SortOrder::updated:
        return "updated";
    }

    Q_UNREACHABLE();
    return "relevance";
}

void ModrithAPI::getMods(QString query,
                         QString mcVersion,
                         QString loader,
                         SortOrder order,
                         int first,
                         int count)
{
    QUrl url(apiUrl + "/search");

    QUrlQuery q;

    q.addQueryItem(
        "query",
        query);

    q.addQueryItem(
        "index",
        Order2String(order));

    q.addQueryItem(
        "offset",
        QString::number(first));

    q.addQueryItem(
        "limit",
        QString::number(count));

    // =====================
    // Фильтры Modrinth
    // =====================

    QJsonArray facets;
    QJsonArray group;

    if(!mcVersion.isEmpty())
    {
        group.append(
            QString("versions:%1")
                .arg(mcVersion));
    }

    if(!loader.isEmpty())
    {
        group.append(
            QString("categories:%1")
                .arg(loader));
    }

    if(!group.isEmpty())
    {
        facets.append(group);

        q.addQueryItem(
            "facets",
            QString::fromUtf8(
                QJsonDocument(facets)
                    .toJson(QJsonDocument::Compact)));
    }

    url.setQuery(q);

    QNetworkRequest req(url);

    req.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json");

    req.setRawHeader(
        "User-Agent",
        "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply =
        manager.get(req);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            reply->deleteLater();

            if(reply->error() !=
                QNetworkReply::NoError)
            {
                emit OnError(
                    reply->errorString());

                return;
            }

            QJsonParseError parseError;

            QJsonDocument doc =
                QJsonDocument::fromJson(
                    reply->readAll(),
                    &parseError);

            if(parseError.error !=
                QJsonParseError::NoError)
            {
                emit OnError(
                    parseError.errorString());

                return;
            }

            QJsonObject json =
                doc.object();

            if(!json.contains("hits"))
            {
                emit OnError(
                    "Invalid response");

                return;
            }

            QVector<Mod> mods;

            QJsonArray hits =
                json["hits"].toArray();

            for(const auto& hit : hits)
            {
                QJsonObject obj =
                    hit.toObject();

                Mod mod;

                mod.id =
                    obj["slug"]
                        .toString();

                mod.name =
                    obj["title"]
                        .toString();

                mod.description =
                    obj["description"]
                        .toString();

                mod.downloads =
                    static_cast<size_t>(
                        obj["downloads"]
                            .toDouble());

                mod.iconURL =
                    QUrl(
                        obj["icon_url"]
                            .toString());

                mod.author =
                    obj["author"]
                        .toString();

                mod.dateCreated =
                    obj["date_created"]
                        .toString();

                mod.dateUpdated =
                    obj["date_modified"]
                        .toString();

                mod.color =
                    static_cast<QRgb>(
                        obj["color"]
                            .toDouble());

                QJsonArray categories =
                    obj["categories"]
                        .toArray();

                for(const auto& c :
                     categories)
                {
                    mod.categories
                        .push_back(
                            c.toString());
                }

                QJsonArray versions =
                    obj["versions"]
                        .toArray();

                for(const auto& v :
                     versions)
                {
                    mod.versions
                        .push_back(
                            v.toString());
                }

                mods.push_back(mod);
            }

            std::sort(
                mods.begin(),
                mods.end(),
                [](const Mod& a,
                   const Mod& b)
                {
                    return a.downloads >
                           b.downloads;
                });

            emit ModList(mods);
        });
}

void ModrithAPI::getDownloadLinks(
    QString slug,
    QString minecraftVersion,
    QString loader)
{
    QUrl url(
        apiUrl +
        "/project/" +
        slug +
        "/version");

    QNetworkRequest req(url);

    req.setRawHeader(
        "User-Agent",
        "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply =
        manager.get(req);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this,
         reply,
         minecraftVersion,
         loader]()
        {
            reply->deleteLater();

            if(reply->error() !=
                QNetworkReply::NoError)
            {
                emit OnError(
                    reply->errorString());

                return;
            }

            QJsonParseError err;

            QJsonDocument doc =
                QJsonDocument::fromJson(
                    reply->readAll(),
                    &err);

            if(err.error !=
                QJsonParseError::NoError)
            {
                emit OnError(
                    err.errorString());

                return;
            }

            if(!doc.isArray())
            {
                emit OnError(
                    "Invalid response");

                return;
            }

            QVector<QUrl> links;

            QJsonArray versions =
                doc.array();

            for(const auto& v :
                 versions)
            {
                QJsonObject version =
                    v.toObject();

                bool versionMatched =
                    false;

                QJsonArray gameVersions =
                    version["game_versions"]
                        .toArray();

                for(const auto& gv :
                     gameVersions)
                {
                    if(gv.toString() ==
                        minecraftVersion)
                    {
                        versionMatched =
                            true;
                        break;
                    }
                }

                if(!versionMatched)
                    continue;

                if(!loader.isEmpty())
                {
                    bool loaderMatched =
                        false;

                    QJsonArray loaders =
                        version["loaders"]
                            .toArray();

                    for(const auto& l :
                         loaders)
                    {
                        if(l.toString() ==
                            loader)
                        {
                            loaderMatched =
                                true;
                            break;
                        }
                    }

                    if(!loaderMatched)
                        continue;
                }

                QJsonArray files =
                    version["files"]
                        .toArray();

                for(const auto& f :
                     files)
                {
                    QJsonObject file =
                        f.toObject();

                    if(file["primary"]
                            .toBool())
                    {
                        QString fileUrl =
                            file["url"]
                                .toString();

                        if(!fileUrl.isEmpty())
                        {
                            links.push_back(
                                QUrl(fileUrl));
                        }

                        break;
                    }
                }
            }

            if(links.isEmpty())
            {
                emit OnError(
                    "No matching versions found");

                return;
            }

            emit DownloadLinks(links);
        });
}

void ModrithAPI::getProject(QString slug)
{
    QUrl url(
        apiUrl +
        "/project/" +
        slug);

    QNetworkReply* reply =
        manager.get(
            QNetworkRequest(url));

    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply]()
            {
                reply->deleteLater();

                if(reply->error() !=
                    QNetworkReply::NoError)
                {
                    emit OnError(
                        reply->errorString());

                    return;
                }

                QJsonDocument doc =
                    QJsonDocument::fromJson(
                        reply->readAll());

                if(!doc.isObject())
                {
                    emit OnError(
                        "Invalid JSON");

                    return;
                }

                QJsonObject obj =
                    doc.object();

                ModProject project;

                project.title =
                    obj["title"].toString();

                project.description =
                    obj["description"].toString();

                project.body =
                    obj["body"].toString();

                project.iconUrl =
                    obj["icon_url"].toString();

                project.license =
                    obj["license"]
                        .toObject()["id"]
                        .toString();

                project.downloads =
                    obj["downloads"].toInt();

                project.updated =
                    obj["updated"].toString();

                QJsonArray categories =
                    obj["categories"].toArray();

                for(const auto& cat :
                     categories)
                {
                    project.categories
                        .push_back(
                            cat.toString());
                }

                QJsonArray gallery =
                    obj["gallery"].toArray();

                for(const auto& image :
                     gallery)
                {
                    project.gallery
                        .push_back(
                            image.toObject()["url"]
                                .toString());
                }

                emit ProjectReceived(
                    project);
            });
}