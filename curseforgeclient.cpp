#include "curseforgeclient.h"

#include <QColor>
#include <QNetworkRequest>
#include <QSet>

//=====================================================
// Constructor
//=====================================================

CurseForgeClient::CurseForgeClient(QObject *parent) : ModsAPI(parent) {}

//=====================================================
// API info
//=====================================================

QString CurseForgeClient::name() const {
    return "CurseForge";
}

QColor CurseForgeClient::accentColor() const {
    return {"#F16436"};
}

QStringList CurseForgeClient::minecraftVersions() const {
    return {
        "Любая версия",

        "1.21.4",
        "1.21.3",
        "1.21.1",
        "1.21",

        "1.20.6",
        "1.20.4",
        "1.20.1",
        "1.20",

        "1.19.4",
        "1.19.2",
        "1.19",

        "1.18.2",
        "1.18",

        "1.17.1",

        "1.16.5",
        "1.16.1",

        "1.15.2",
        "1.14.4",
        "1.12.2",
        "1.8.9",
        "1.7.10"
    };
}

QStringList CurseForgeClient::loaders() const {
    return {
        "Любой загрузчик",
        "Forge",
        "Fabric",
        "NeoForge",
        "Quilt"
    };
}

QNetworkReply *CurseForgeClient::apiGet(const QString &path, const QUrlQuery &query) {
    QUrl url(m_base + path);

    if (!query.isEmpty()) {
        url.setQuery(query);
    }

    QNetworkRequest request(url);

    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("User-Agent", "ZXCrackLauncher/1.0 (Qt)");

    return m_nam.get(request);
}

ModInfo CurseForgeClient::parseMod(const QJsonObject &object) {
    ModInfo mod;

    mod.id = object["id"].toInt();
    mod.name = object["name"].toString();
    mod.summary = object["summary"].toString();
    mod.downloadCount = static_cast<quint64>(object["downloadCount"].toDouble());

    mod.websiteUrl = object["links"].toObject()["websiteUrl"].toString();

    const auto logo = object["logo"].toObject();

    mod.iconUrl = logo["thumbnailUrl"].toString();

    if (mod.iconUrl.isEmpty()) {
        mod.iconUrl = logo["url"].toString();
    }

    //----------------------------
    // author
    //----------------------------

    const auto authors = object["authors"].toArray();

    if (!authors.isEmpty()) {
        mod.author = authors.first().toObject()["name"].toString();
    }

    //----------------------------
    // versions
    //----------------------------

    QSet<QString> versions;

    const auto latestFiles = object["latestFilesIndexes"].toArray();

    for (const auto &version: latestFiles) {
        const QString v = version.toObject()["gameVersion"].toString();

        if (!v.isEmpty()) {
            versions.insert(v);
        }
    }

    mod.gameVersions = QStringList(versions.begin(),versions.end());

    return mod;
}

FileInfo CurseForgeClient::parseFile(
    const QJsonObject &object) {
    FileInfo file;

    file.id  = object["id"].toInt();

    file.fileName  = object["fileName"].toString();

    file.downloadUrl  = object["downloadUrl"].toString();

    return file;
}

//=====================================================
// Search
//=====================================================

void CurseForgeClient::searchMods(
    const QString &query,
    const QString &mcVersion,
    const QString &loader) {
    search(
        query,
        mcVersion,
        loader,
        false);
}

void CurseForgeClient::searchModpacks(
    const QString &query,
    const QString &mcVersion) {
    search(
        query,
        mcVersion,
        "",
        true);
}

void CurseForgeClient::search(
    const QString &query,
    const QString &mcVersion,
    const QString &loader,
    bool modpacks,
    int pageSize,
    int index) {
    QUrlQuery q;

    q.addQueryItem("gameId", "432");
    q.addQueryItem(
        "classId",
        modpacks ? "4471" : "6");

    q.addQueryItem(
        "searchFilter",
        query);

    q.addQueryItem(
        "pageSize",
        QString::number(pageSize));

    q.addQueryItem(
        "index",
        QString::number(index));

    q.addQueryItem(
        "sortField",
        "2");

    q.addQueryItem(
        "sortOrder",
        "desc");

    //-----------------------------------
    // MC version
    //-----------------------------------

    if (!mcVersion.isEmpty() &&
        mcVersion != "Любая версия") {
        q.addQueryItem(
            "gameVersion",
            mcVersion);
    }

    //-----------------------------------
    // Loader
    //-----------------------------------

    if (!loader.isEmpty() &&
        loader != "Любой загрузчик") {
        const QString l  = loader.toLower();

        if (l == "forge")
            q.addQueryItem(
                "modLoaderType", "1");

        else if (l == "fabric")
            q.addQueryItem(
                "modLoaderType", "4");

        else if (l == "neoforge")
            q.addQueryItem(
                "modLoaderType", "6");

        else if (l == "quilt")
            q.addQueryItem(
                "modLoaderType", "5");
    }

    //-----------------------------------
    // Request
    //-----------------------------------

    auto *reply  = apiGet("/mods/search", q);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, modpacks]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(
                    "CurseForge: "
                    + reply->errorString());

                return;
            }

            const auto document  = QJsonDocument::fromJson(
                        reply->readAll());

            const auto data  = document.object()["data"]
                    .toArray();

            QVector<ModInfo> result;

            for (const auto &item: data) {
                result.push_back(
                    parseMod(
                        item.toObject()));
            }

            if (modpacks) {
                emit modpacksReceived(
                    result);
            } else {
                emit modsReceived(
                    result);
            }
        });
}

//=====================================================
// Files
//=====================================================

void CurseForgeClient::getProjectFiles(
    int projectId,
    const QString &mcVersion,
    const QString &loader) {
    QUrlQuery q;

    q.addQueryItem(
        "pageSize",
        "50");

    //-----------------------------------
    // MC version
    //-----------------------------------

    if (!mcVersion.isEmpty() &&
        mcVersion != "Любая версия") {
        q.addQueryItem(
            "gameVersion",
            mcVersion);
    }

    //-----------------------------------
    // Loader
    //-----------------------------------

    if (!loader.isEmpty() &&
        loader != "Любой загрузчик") {
        const QString l  = loader.toLower();

        if (l == "forge")
            q.addQueryItem(
                "modLoaderType",
                "1");

        else if (l == "fabric")
            q.addQueryItem(
                "modLoaderType",
                "4");

        else if (l == "neoforge")
            q.addQueryItem(
                "modLoaderType",
                "6");

        else if (l == "quilt")
            q.addQueryItem(
                "modLoaderType",
                "5");
    }

    //-----------------------------------
    // Request
    //-----------------------------------

    auto *reply  = apiGet(
                QString(
                    "/mods/%1/files")
                .arg(projectId),
                q);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, projectId] {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred("CurseForge: " + reply->errorString());

                return;
            }

            const auto document  = QJsonDocument::fromJson(reply->readAll());

            const auto data  = document.object()["data"].toArray();

            QVector<FileInfo> files;

            for (const auto &item: data) {
                FileInfo file  = parseFile(item.toObject());

                // fallback url
                if (file.downloadUrl.isEmpty()) {
                    const int part1  = file.id / 1000;

                    const int part2  = file.id % 1000;

                    file.downloadUrl = QString("https://edge.forgecdn.net/files/%1/%2/%3").arg(part1).arg(
                        QString::number(part2).rightJustified(3, '0')).arg(file.fileName);
                }

                files.push_back(file);
            }

            emit filesReceived(projectId, files);
        });
}
