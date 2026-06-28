#include "curseforgeclient.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

CurseForgeClient::CurseForgeClient(QObject* parent) : QObject(parent) {}

// ── Вспомогательный GET ───────────────────────────────────────────────────────
QNetworkReply* CurseForgeClient::apiGet(const QString& path, const QUrlQuery& q)
{
    QUrl url(m_base + path);
    if (!q.isEmpty()) url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Accept",    "application/json");
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("User-Agent","ZXCrackLauncher/1.0 (Qt)");
    return m_nam.get(req);
}

// ── Парсинг одного мода/модпака ───────────────────────────────────────────────
CFMod CurseForgeClient::parseMod(const QJsonObject& obj)
{
    CFMod m;
    m.id          = obj["id"].toInt();
    m.name        = obj["name"].toString();
    m.summary     = obj["summary"].toString();
    m.slug        = obj["slug"].toString();
    m.classId     = obj["classId"].toInt();
    m.downloadCount = static_cast<quint64>(obj["downloadCount"].toDouble());
    m.websiteUrl  = obj["links"].toObject()["websiteUrl"].toString();

    // Иконка
    QJsonObject logo = obj["logo"].toObject();
    m.iconUrl = logo["thumbnailUrl"].toString();
    if (m.iconUrl.isEmpty()) m.iconUrl = logo["url"].toString();

    // Авторы
    QJsonArray authors = obj["authors"].toArray();
    if (!authors.isEmpty())
        m.author = authors.first().toObject()["name"].toString();

    // Версии Minecraft
    QJsonArray latestFiles = obj["latestFilesIndexes"].toArray();
    QSet<QString> versionSet;
    for (const auto& fv : latestFiles) {
        QString v = fv.toObject()["gameVersion"].toString();
        if (!v.isEmpty()) versionSet.insert(v);
    }
    m.gameVersions = QVector<QString>(versionSet.begin(), versionSet.end());

    // Категории
    QJsonArray cats = obj["categories"].toArray();
    for (const auto& c : cats)
        m.categories.push_back(c.toObject()["name"].toString());

    return m;
}

// ── Парсинг файла ─────────────────────────────────────────────────────────────
CFFileInfo CurseForgeClient::parseFile(const QJsonObject& obj)
{
    CFFileInfo f;
    f.id          = obj["id"].toInt();
    f.displayName = obj["displayName"].toString();
    f.fileName    = obj["fileName"].toString();
    f.downloadUrl = obj["downloadUrl"].toString();

    QJsonArray gv = obj["gameVersions"].toArray();
    for (const auto& v : gv)
        f.gameVersions.push_back(v.toString());

    return f;
}

// ── Поиск модов ───────────────────────────────────────────────────────────────
void CurseForgeClient::searchMods(const QString& query,
                                  const QString& mcVersion,
                                  const QString& loader,
                                  CFProjectType  type,
                                  int pageSize,
                                  int index)
{
    QUrlQuery q;
    q.addQueryItem("gameId",       "432"); // Minecraft
    q.addQueryItem("classId",      QString::number(static_cast<int>(type)));
    q.addQueryItem("searchFilter", query.isEmpty() ? "" : query);
    q.addQueryItem("pageSize",     QString::number(pageSize));
    q.addQueryItem("index",        QString::number(index));
    q.addQueryItem("sortField",    "2"); // Popularity
    q.addQueryItem("sortOrder",    "desc");

    if (!mcVersion.isEmpty() && mcVersion != "Любая версия")
        q.addQueryItem("gameVersion", mcVersion);

    // modLoaderType: 1=Forge, 4=Fabric, 6=NeoForge, 5=Quilt
    if (!loader.isEmpty() && loader != "Любой загрузчик") {
        QString l = loader.toLower();
        if      (l == "forge")    q.addQueryItem("modLoaderType", "1");
        else if (l == "fabric")   q.addQueryItem("modLoaderType", "4");
        else if (l == "neoforge") q.addQueryItem("modLoaderType", "6");
        else if (l == "quilt")    q.addQueryItem("modLoaderType", "5");
    }

    QNetworkReply* reply = apiGet("/mods/search", q);
    connect(reply, &QNetworkReply::finished, this, [this, reply, type]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("CurseForge: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray data = doc.object()["data"].toArray();
        QVector<CFMod> mods;
        for (const auto& item : data)
            mods.push_back(parseMod(item.toObject()));

        if (type == CFProjectType::ModPack)
            emit modpacksReceived(mods);
        else
            emit modsReceived(mods);
    });
}

void CurseForgeClient::searchModpacks(const QString& query,
                                      const QString& mcVersion,
                                      int pageSize, int index)
{
    searchMods(query, mcVersion, "", CFProjectType::ModPack, pageSize, index);
}

// ── Файлы проекта ─────────────────────────────────────────────────────────────
void CurseForgeClient::getProjectFiles(int projectId,
                                       const QString& mcVersion,
                                       const QString& loader)
{
    QUrlQuery q;
    q.addQueryItem("pageSize", "50");
    if (!mcVersion.isEmpty() && mcVersion != "Любая версия")
        q.addQueryItem("gameVersion", mcVersion);
    if (!loader.isEmpty() && loader != "Любой загрузчик") {
        QString l = loader.toLower();
        if      (l == "forge")    q.addQueryItem("modLoaderType", "1");
        else if (l == "fabric")   q.addQueryItem("modLoaderType", "4");
        else if (l == "neoforge") q.addQueryItem("modLoaderType", "6");
        else if (l == "quilt")    q.addQueryItem("modLoaderType", "5");
    }

    QNetworkReply* reply = apiGet(QString("/mods/%1/files").arg(projectId), q);
    connect(reply, &QNetworkReply::finished, this, [this, reply, projectId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("CurseForge: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray data = doc.object()["data"].toArray();
        QVector<CFFileInfo> files;
        for (const auto& item : data)
            files.push_back(parseFile(item.toObject()));
        emit filesReceived(projectId, files);
    });
}

// ── URL скачивания ────────────────────────────────────────────────────────────
void CurseForgeClient::getDownloadUrl(int projectId, int fileId)
{
    QNetworkReply* reply = apiGet(
        QString("/mods/%1/files/%2/download-url").arg(projectId).arg(fileId));

    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("CurseForge download: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString url = doc.object()["data"].toString();
        // Некоторые файлы CurseForge не раскрывают прямой URL —
        // в таком случае строим по известному паттерну
        if (url.isEmpty()) {
            int part1 = fileId / 1000;
            int part2 = fileId % 1000;
            url = QString("https://edge.forgecdn.net/files/%1/%2/")
                      .arg(part1).arg(QString::number(part2).rightJustified(3,'0'));
        }
        // Имя файла нужно передать снаружи — сигнал без fileName
        emit downloadUrlReady(QUrl(url), QString());
    });
}