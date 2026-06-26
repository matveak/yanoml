#include "modrinthapi.h"
#include <QJsonParseError>
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <QRegularExpression>

QString Order2String(SortOrder order)
{
    switch(order)
    {
    case SortOrder::relevance: return "relevance";
    case SortOrder::downloads: return "downloads";
    case SortOrder::follows:   return "follows";
    case SortOrder::newest:    return "newest";
    case SortOrder::updated:   return "updated";
    }
    return "relevance";
}

ModrithAPI::ModrithAPI(QObject* parent) : QObject(parent) {}

// ===================== GET MODS =====================
void ModrithAPI::getMods(QString query,
                         QString mcVersion,
                         QString loader,
                         QString category,
                         QString environment,
                         SortOrder order,
                         int first,
                         int count)
{
    QUrl url(apiUrl + "/search");
    QUrlQuery q;

    q.addQueryItem("query", query.isEmpty() ? "minecraft" : query);
    q.addQueryItem("index", Order2String(order));
    q.addQueryItem("offset", QString::number(first));
    q.addQueryItem("limit", QString::number(count));

    QJsonArray facets;

    // Ищем только моды.
    {
        QJsonArray typeGroup;
        typeGroup.append(QString("project_type:mod"));
        facets.append(typeGroup);
    }

    // Строгие фильтры
    if (!mcVersion.isEmpty() && mcVersion != "Любая версия")
    {
        QJsonArray versionGroup;
        versionGroup.append(QString("versions:%1").arg(mcVersion));
        facets.append(versionGroup);
    }

    if (!loader.isEmpty() && loader != "Любой загрузчик")
    {
        QJsonArray loaderGroup;
        loaderGroup.append(QString("categories:%1").arg(loader));
        facets.append(loaderGroup);
    }

    if (!category.isEmpty() && category != "Любая категория")
    {
        QJsonArray categoryGroup;
        categoryGroup.append(QString("categories:%1").arg(category));
        facets.append(categoryGroup);
    }

    // Окружение: client_side/server_side бывают required/optional —
    // оба значения подходят, поэтому это OR-группа внутри одного facet.
    if (!environment.isEmpty() && environment != "Любая среда")
    {
        QString sideKey = (environment == "server") ? "server_side" : "client_side";

        QJsonArray envGroup;
        envGroup.append(QString("%1:required").arg(sideKey));
        envGroup.append(QString("%1:optional").arg(sideKey));
        facets.append(envGroup);
    }

    if (!facets.isEmpty())
    {
        q.addQueryItem("facets", QString::fromUtf8(
                                     QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    }

    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply = manager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mcVersion, loader]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit OnError(reply->errorString());
                    return;
                }

                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    emit OnError(parseError.errorString());
                    return;
                }

                QVector<Mod> mods;
                QJsonArray hits = doc.object()["hits"].toArray();

                for (const auto& hit : hits)
                {
                    QJsonObject obj = hit.toObject();
                    Mod mod;

                    mod.id          = obj["slug"].toString();
                    mod.name        = obj["title"].toString();
                    mod.description = obj["description"].toString();
                    mod.downloads   = static_cast<size_t>(obj["downloads"].toDouble());
                    mod.follows     = static_cast<size_t>(obj["follows"].toDouble());
                    mod.iconURL     = QUrl(obj["icon_url"].toString());
                    mod.author      = obj["author"].toString();
                    mod.color       = static_cast<QRgb>(obj["color"].toDouble());
                    mod.dateCreated = obj["date_created"].toString();
                    mod.dateUpdated = obj["date_modified"].toString();

                    QJsonArray cats = obj["categories"].toArray();
                    for (const auto& c : cats)
                        mod.categories.push_back(c.toString());

                    QJsonArray vers = obj["versions"].toArray();
                    for (const auto& v : vers)
                        mod.versions.push_back(v.toString());

                    // Дополнительная клиентская проверка
                    bool versionOk = mcVersion.isEmpty() || mcVersion == "Любая версия";
                    if (!versionOk)
                    {
                        for (const QString& v : mod.versions)
                        {
                            if (v == mcVersion)
                            {
                                versionOk = true;
                                break;
                            }
                        }
                    }

                    if (versionOk)
                        mods.push_back(mod);
                }

                emit ModList(mods);
            });
}

// ===================== FETCH VERSIONS (FROM TAG ENDPOINT) =====================
void ModrithAPI::fetchAvailableVersions(const QString& loader)
{
    if (loader.isEmpty()) return;

    // Используем endpoint для получения всех версий Minecraft
    QUrl url(apiUrl + "/tag/game_version");

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply = manager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, loader]()
            {
                reply->deleteLater();

                QSet<QString> versionsSet;

                if (reply->error() == QNetworkReply::NoError)
                {
                    QJsonParseError err;
                    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);
                    
                    if (err.error == QJsonParseError::NoError && doc.isArray())
                    {
                        QJsonArray versions = doc.array();
                        qDebug() << "Получено версий с API:" << versions.size();
                        
                        // Регулярное выражение для Minecraft версий (1.XX, 1.XX.X и т.д.)
                        QRegularExpression mcVersionRegex("^1\\.(\\d+)(\\.(\\d+))?$");
                        
                        int validVersions = 0;

                        // Собираем только версии Minecraft (формата 1.XX.X)
                        for (int i = 0; i < versions.size(); ++i)
                        {
                            QJsonObject obj = versions[i].toObject();
                            QString version = obj["version"].toString();

                            // Проверяем, что это версия Minecraft (начинается с "1.")
                            if (mcVersionRegex.match(version).hasMatch())
                            {
                                versionsSet.insert(version);
                                validVersions++;
                                
                                if (validVersions <= 10)
                                {
                                    qDebug() << "Добавлена Minecraft версия:" << version;
                                }
                            }
                        }
                        
                        qDebug() << "Найдено уникальных Minecraft версий:" << versionsSet.size();
                    }
                }
                else
                {
                    qDebug() << "Ошибка при загрузке версий:" << reply->errorString();
                }

                // === Если почти ничего не найдено, используем fallback ===
                if (versionsSet.size() < 5)
                {
                    qDebug() << "Используется fallback список версий";
                    
                    if (loader == "forge")
                    {
                        versionsSet << "1.21.1" << "1.21" << "1.20.1" << "1.20.2" << "1.20"
                                    << "1.19.2" << "1.19.4" << "1.18.2" << "1.18.1" << "1.17.1"
                                    << "1.16.5" << "1.16.4" << "1.15.2" << "1.14.4" << "1.12.2";
                    }
                    else if (loader == "neoforge")
                    {
                        versionsSet << "1.21.1" << "1.21" << "1.20.1" << "1.20.2" << "1.20";
                    }
                    else if (loader == "fabric" || loader == "quilt")
                    {
                        versionsSet << "1.21.1" << "1.21" << "1.20.1" << "1.20.2" << "1.19.2"
                                    << "1.18.2" << "1.17.1" << "1.16.5";
                    }
                }

                QStringList versions = versionsSet.values();
                std::sort(versions.begin(), versions.end(), [](const QString& a, const QString& b) {
                    return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
                });

                qDebug() << "Итого версий для отправки:" << versions.size();
                qDebug() << "Версии:" << versions;

                emit AvailableVersions(loader, versions);
            });
}

// Вспомогательная функция с fallback'ом
void ModrithAPI::useFallbackVersions(const QString& loader)
{
    QStringList versions;

    if (loader == "neoforge")
    {
        versions << "1.21.1" << "1.21" << "1.20.1";
    }
    else if (loader == "forge")
    {
        versions << "1.21.1" << "1.20.1" << "1.20" << "1.19.2" << "1.18.2";
    }
    else if (loader == "fabric" || loader == "quilt")
    {
        versions << "1.21.1" << "1.21" << "1.20.1" << "1.20" << "1.19.2" << "1.18.2";
    }

    emit AvailableVersions(loader, versions);
}

// ===================== DOWNLOAD LINKS =====================
void ModrithAPI::getDownloadLinks(QString slug,
                                  QString minecraftVersion,
                                  QString loader)
{
    QUrl url(apiUrl + "/project/" + slug + "/version");

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply = manager.get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, minecraftVersion, loader]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit OnError(reply->errorString());
                    return;
                }

                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);

                if (err.error != QJsonParseError::NoError || !doc.isArray())
                {
                    emit OnError("Invalid response from Modrinth");
                    return;
                }

                QVector<QUrl> links;
                QJsonArray versionsArray = doc.array();

                bool anyVersion = minecraftVersion.isEmpty() || minecraftVersion == "Любая версия";

                for (const auto& v : versionsArray)
                {
                    QJsonObject versionObj = v.toObject();

                    // === Проверка версии Minecraft ===
                    bool versionMatched = anyVersion;
                    if (!anyVersion)
                    {
                        QJsonArray gameVersions = versionObj["game_versions"].toArray();
                        for (const auto& gv : gameVersions)
                        {
                            if (gv.toString() == minecraftVersion)
                            {
                                versionMatched = true;
                                break;
                            }
                        }
                    }
                    if (!versionMatched) continue;

                    // === Проверка загрузчика ===
                    if (!loader.isEmpty())
                    {
                        bool loaderMatched = false;
                        QJsonArray loaders = versionObj["loaders"].toArray();
                        for (const auto& l : loaders)
                        {
                            if (l.toString().toLower() == loader.toLower())
                            {
                                loaderMatched = true;
                                break;
                            }
                        }
                        if (!loaderMatched) continue;
                    }

                    // === Берём primary файл ===
                    QJsonArray files = versionObj["files"].toArray();
                    for (const auto& f : files)
                    {
                        QJsonObject fileObj = f.toObject();
                        if (fileObj["primary"].toBool())
                        {
                            QString fileUrl = fileObj["url"].toString();
                            if (!fileUrl.isEmpty())
                            {
                                links.push_back(QUrl(fileUrl));
                                break;  // Найден primary файл в этой версии
                            }
                        }
                    }

                    if (!links.isEmpty())
                        break; // нашли подходящий — выходим из цикла версий
                }

                if (links.isEmpty())
                {
                    qDebug() << "DEBUG: Не найдено ссылок для загрузки. Версия:" << minecraftVersion << "Загрузчик:" << loader;
                    emit OnError("Не найдено подходящей версии мода для:\n"
                                 "Minecraft: " + (minecraftVersion.isEmpty() ? "Любая" : minecraftVersion) + "\n"
                                 "Загрузчик: " + (loader.isEmpty() ? "Любой" : loader) + "\n\n"
                                 "Мод может не поддерживать эту комбинацию версии и загрузчика.");
                    return;
                }

                emit DownloadLinks(links);
            });
}

// ===================== GET PROJECT =====================
void ModrithAPI::getProject(QString slug)
{
    QUrl url(apiUrl + "/project/" + slug);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MinecraftLauncher/1.0 (Qt)");

    QNetworkReply* reply = manager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit OnError(reply->errorString());
                    return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.isObject())
                {
                    emit OnError("Invalid JSON");
                    return;
                }

                QJsonObject obj = doc.object();
                ModProject project;

                project.title = obj["title"].toString();
                project.description = obj["description"].toString();
                project.body = obj["body"].toString();
                project.iconUrl = obj["icon_url"].toString();
                project.license = obj["license"].toObject()["id"].toString();
                project.downloads = obj["downloads"].toInt();
                project.updated = obj["date_modified"].toString();

                QJsonArray cats = obj["categories"].toArray();
                for (const auto& c : cats)
                    project.categories.push_back(c.toString());

                QJsonArray gallery = obj["gallery"].toArray();
                for (const auto& img : gallery)
                    project.gallery.push_back(img.toObject()["url"].toString());

                emit ProjectReceived(project);
            });
}