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

// ── CurseForge project types ──────────────────────────────────────────────────
enum class CFProjectType { Mod = 6, ModPack = 4471 };

struct CFMod {
    int    id          = 0;
    QString name;
    QString summary;
    QString author;
    QString iconUrl;
    quint64 downloadCount = 0;
    int    gameVersionLatest = 0; // major only
    QVector<QString> gameVersions;
    QVector<QString> categories;
    QString websiteUrl;
    QString slug;
    int    classId = 0; // 6 = mod, 4471 = modpack
};

struct CFModpack {
    int    id = 0;
    QString name;
    QString summary;
    QString author;
    QString iconUrl;
    quint64 downloadCount = 0;
    QVector<QString> gameVersions;
    QVector<QString> categories;
    QString websiteUrl;
    QString slug;
};

struct CFFileInfo {
    int     id = 0;
    QString displayName;
    QString fileName;
    QString downloadUrl;
    QVector<QString> gameVersions;
};

class CurseForgeClient : public QObject
{
    Q_OBJECT
public:
    explicit CurseForgeClient(QObject* parent = nullptr);

    void searchMods(const QString& query,
                    const QString& mcVersion = "",
                    const QString& loader    = "",
                    CFProjectType  type      = CFProjectType::Mod,
                    int pageSize             = 20,
                    int index                = 0);

    void searchModpacks(const QString& query,
                        const QString& mcVersion = "",
                        int pageSize             = 20,
                        int index                = 0);

    // Получает список файлов проекта для выбора версии для скачивания
    void getProjectFiles(int projectId,
                         const QString& mcVersion = "",
                         const QString& loader    = "");

    // Возвращает прямую ссылку на скачивание файла
    void getDownloadUrl(int projectId, int fileId);

signals:
    void modsReceived(const QVector<CFMod>& mods);
    void modpacksReceived(const QVector<CFMod>& modpacks);
    void filesReceived(int projectId, const QVector<CFFileInfo>& files);
    void downloadUrlReady(const QUrl& url, const QString& fileName);
    void errorOccurred(const QString& msg);

private:
    QNetworkReply* apiGet(const QString& path, const QUrlQuery& q = QUrlQuery());
    static CFMod parseMod(const QJsonObject& obj);
    static CFFileInfo parseFile(const QJsonObject& obj);

    QNetworkAccessManager m_nam;
    // Публичный CF API key (proxy key от CFWidget — без него работает для чтения)
    // Если у вас есть свой ключ — замените.
    const QString m_apiKey = "$2a$10$bL4bIL5pUWqfcO7KwQnL.eMRWn.VH7mXIv0Cl5mDHJd3jkFMlnACa";
    const QString m_base   = "https://api.curseforge.com/v1";
};