#include <QObject>

struct FileInfo
{
    int id = 0;
    QString fileName;
    QString downloadUrl;
};

struct ModInfo
{
    int id = 0;

    QString name;
    QString author;
    QString summary;

    QString iconUrl;
    QString websiteUrl;

    quint64 downloadCount = 0;

    QStringList gameVersions;
};

class ModsAPI : public QObject
{
    Q_OBJECT

public:
    explicit ModsAPI(QObject *parent = nullptr) : QObject(parent) {}
    ~ModsAPI() override = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QColor accentColor() const = 0;

    [[nodiscard]] virtual QStringList minecraftVersions() const = 0;
    [[nodiscard]] virtual QStringList loaders() const = 0;

    virtual void searchMods(const QString &name, const QString &mcVersion, const QString &loader) = 0;

    virtual void searchModpacks(const QString &name, const QString &mcVersion) = 0;

    virtual void getProjectFiles(int projectId, const QString &mcVersion, const QString &loader) = 0;

signals:
    void modsReceived(const QVector<ModInfo>& mods);
    void modpacksReceived(const QVector<ModInfo>& packs);

    void filesReceived(int projectId, const QVector<FileInfo>& files);

    void errorOccurred(const QString &error);
};