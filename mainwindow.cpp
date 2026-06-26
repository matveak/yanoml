#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QVersionNumber>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <algorithm>
#include <memory>
#include <QSet>
#include <QJsonValue>
#include <private/qzipreader_p.h>

// ==================== Java Auto-Detection ====================

// Таблица совместимости: какая версия Java нужна для каждой версии MC
// MC <= 1.12.2  -> Java 8
// MC 1.13-1.16  -> Java 8 (или 11, но 8 надёжнее)
// MC 1.17       -> Java 16+
// MC 1.18-1.20  -> Java 17+
// MC 1.21+      -> Java 21+
static int requiredJavaMajor(const QString& mcVersion)
{
    QVersionNumber ver = QVersionNumber::fromString(mcVersion);

    if (ver >= QVersionNumber(1, 21)) return 21;
    if (ver >= QVersionNumber(1, 18)) return 17;
    if (ver >= QVersionNumber(1, 17)) return 17;
    if (ver >= QVersionNumber(1, 13)) return 8;
    return 8;
}

// Проверяет версию javaw/java по выводу `java -version` и возвращает major (8, 11, 17, 21...)
// Возвращает -1 если не удалось определить
static int getJavaMajorVersion(const QString& javaExe)
{
    QProcess proc;
    proc.start(javaExe, {"-version"});
    if (!proc.waitForFinished(3000))
        return -1;

    // `java -version` пишет в stderr
    QString output = proc.readAllStandardError() + proc.readAllStandardOutput();

    // Формат: 'version "21.0.1"' или 'version "1.8.0_391"'
    QRegularExpression re(R"(version\s+"(\d+)(?:\.(\d+))?)");
    QRegularExpressionMatch m = re.match(output);
    if (!m.hasMatch()) return -1;

    int major = m.captured(1).toInt();

    // Старый формат: 1.8 -> major=8
    if (major == 1)
        major = m.captured(2).toInt();

    return major;
}

// Ищет javaw.exe / java в стандартных местах установки
// Возвращает карту: major -> полный путь к исполняемому файлу
static QMap<int, QString> findInstalledJavas()
{
    QMap<int, QString> found;

#ifdef Q_OS_WIN
    const QString exe = "javaw.exe";
    QStringList searchRoots = {
        "C:/Program Files/Java",
        "C:/Program Files/Eclipse Adoptium",
        "C:/Program Files/Microsoft",
        "C:/Program Files/BellSoft",
        "C:/Program Files/Azul Systems/Zulu",
        QDir::homePath() + "/.jdks",          // IntelliJ-managed JDKs
    };
#elif defined(Q_OS_MAC)
    const QString exe = "java";
    QStringList searchRoots = {
        "/Library/Java/JavaVirtualMachines",
        QDir::homePath() + "/Library/Java/JavaVirtualMachines",
    };
#else
    const QString exe = "java";
    QStringList searchRoots = {
        "/usr/lib/jvm",
        "/usr/local/lib/jvm",
        QDir::homePath() + "/.jdks",
    };
#endif

    for (const QString& root : searchRoots)
    {
        QDir rootDir(root);
        if (!rootDir.exists()) continue;

        for (const QString& entry : rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
#ifdef Q_OS_WIN
            QString candidate = root + "/" + entry + "/bin/" + exe;
#elif defined(Q_OS_MAC)
            QString candidate = root + "/" + entry + "/Contents/Home/bin/" + exe;
#else
            QString candidate = root + "/" + entry + "/bin/" + exe;
#endif
            if (!QFileInfo::exists(candidate)) continue;

            int major = getJavaMajorVersion(candidate);
            if (major > 0 && !found.contains(major))
            {
                found[major] = candidate;
                qDebug() << "Found Java" << major << "at" << candidate;
            }
        }
    }

    return found;
}

// Совместима ли установленная Java с версией, которую требует Minecraft.
// Старые версии (LWJGL 3.2.x и раньше) падают на слишком новой Java,
// поэтому им нужна именно major-версия, указанная Mojang.
// Современные (Java 17+, LWJGL 3.3+) спокойно работают на более новой Java.
static bool javaIsCompatible(int installedMajor, int requiredMajor)
{
    if (installedMajor <= 0)
        return false;
    if (installedMajor == requiredMajor)
        return true;
    if (requiredMajor >= 17)
        return installedMajor >= requiredMajor;
    return false;
}

// Ищет среди установленных Java ту, что совместима с требуемой major-версией.
// Возвращает пустую строку, если подходящей нет (тогда качаем Java от Mojang).
static QString pickCompatibleInstalledJava(const QMap<int, QString>& javas,
                                           int requiredMajor,
                                           const QString& userJavaPath)
{
    if (!userJavaPath.isEmpty() && QFileInfo::exists(userJavaPath))
    {
        int userMajor = getJavaMajorVersion(userJavaPath);
        if (javaIsCompatible(userMajor, requiredMajor))
            return userJavaPath;
    }

    if (javas.contains(requiredMajor))
        return javas[requiredMajor];

    QString best;
    int bestMajor = INT_MAX;
    for (auto it = javas.constBegin(); it != javas.constEnd(); ++it)
    {
        if (javaIsCompatible(it.key(), requiredMajor) && it.key() < bestMajor)
        {
            bestMajor = it.key();
            best = it.value();
        }
    }
    return best;
}

// По требуемой major-версии возвращает имя Java-компонента Mojang
// (используется, если в version.json нет поля javaVersion.component).
static QString javaComponentForMajor(int requiredMajor)
{
    if (requiredMajor <= 8)  return "jre-legacy";
    if (requiredMajor <= 16) return "java-runtime-alpha";
    if (requiredMajor <= 17) return "java-runtime-gamma";
    return "java-runtime-delta";
}

// ==================== Natives Extraction ====================

// Проверяет правила библиотеки для текущей ОС (allow/disallow).
static bool nativeLibraryAllowedOnCurrentOS(const QJsonObject& lib)
{
    QJsonArray rules = lib["rules"].toArray();
    if (rules.isEmpty())
        return true;

    bool allowed = false;

    for (const auto& rv : rules)
    {
        QJsonObject rule = rv.toObject();
        QString action   = rule["action"].toString();

        if (rule.contains("os"))
        {
            QString osName = rule["os"].toObject()["name"].toString();
#ifdef Q_OS_WIN
            QString cur = "windows";
#elif defined(Q_OS_MAC)
            QString cur = "osx";
#else
            QString cur = "linux";
#endif
            if (osName == cur) allowed = (action == "allow");
        }
        else
        {
            allowed = (action == "allow");
        }
    }

    return allowed;
}

// Распаковывает нативные библиотеки (LWJGL .dll/.so/.dylib) из скачанных
// classifier-джарников в каталог natives. Без этого шага старые версии
// (до 1.19, использующие classifiers) падают: java.library.path указывает
// на пустую папку и LWJGL не может загрузить нативные библиотеки.
static void extractNativesForVersion(const QJsonObject& root,
                                     const QString& gameDir,
                                     const QString& nativesDir)
{
    QDir().mkpath(nativesDir);

    QJsonArray libraries = root["libraries"].toArray();

    for (const auto& value : libraries)
    {
        QJsonObject lib = value.toObject();

        if (!nativeLibraryAllowedOnCurrentOS(lib))
            continue;

        QJsonObject downloads = lib["downloads"].toObject();
        if (!downloads.contains("classifiers"))
            continue;

        QJsonObject classifiers = downloads["classifiers"].toObject();

        // Выбираем classifier для текущей ОС — так же, как при скачивании.
        QString nativeKey;
#ifdef Q_OS_WIN
        if (classifiers.contains("natives-windows"))
            nativeKey = "natives-windows";
        else if (classifiers.contains("natives-windows-64"))
            nativeKey = "natives-windows-64";
#elif defined(Q_OS_MAC)
        if (classifiers.contains("natives-osx"))
            nativeKey = "natives-osx";
        else if (classifiers.contains("natives-macos"))
            nativeKey = "natives-macos";
#else
        if (classifiers.contains("natives-linux"))
            nativeKey = "natives-linux";
#endif
        if (nativeKey.isEmpty())
            continue;

        QString relPath = classifiers[nativeKey].toObject()["path"].toString();
        if (relPath.isEmpty())
            continue;

        QString jarPath = gameDir + "/libraries/" + relPath;
        if (!QFileInfo::exists(jarPath))
        {
            qWarning() << "Native jar not found, skipping:" << jarPath;
            continue;
        }

        // Список исключений из version.json (обычно META-INF/).
        QStringList excludes;
        QJsonArray excludeArr =
            lib["extract"].toObject()["exclude"].toArray();
        for (const auto& ev : excludeArr)
            excludes << ev.toString();
        if (excludes.isEmpty())
            excludes << "META-INF/";

        QZipReader zip(jarPath);
        if (!zip.isReadable())
        {
            qWarning() << "Cannot read native jar:" << jarPath;
            continue;
        }

        const auto entries = zip.fileInfoList();
        for (const auto& entry : entries)
        {
            if (!entry.isFile)
                continue;

            bool excluded = false;
            for (const QString& ex : excludes)
            {
                if (entry.filePath.startsWith(ex))
                {
                    excluded = true;
                    break;
                }
            }
            if (excluded)
                continue;

            // Раскладываем только сами библиотеки в плоскую папку natives.
            QString outName = QFileInfo(entry.filePath).fileName();
            if (outName.isEmpty())
                continue;

            QString outPath = nativesDir + "/" + outName;
            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly))
            {
                out.write(zip.fileData(entry.filePath));
                out.close();
            }
            else
            {
                qWarning() << "Cannot write native:" << outPath;
            }
        }
    }
}

// ==================== MainWindow ====================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    progressBar = new QProgressBar(this);
    progressBar->setGeometry(QRect(780, 710, 500, 25));
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->hide();

    setupUi(this);
    downloader = new MinecraftDownloader(this);
    settingsWindow = new SettingsWindow(this);

    setupTrayIcon();
    setupConnections();
    loadVersions();

    // Сканируем Java в фоне при старте
    QTimer::singleShot(500, this, [this]() {
        installedJavas = findInstalledJavas();
        qDebug() << "Java scan complete, found" << installedJavas.size() << "installations";
    });
}

// ==================== Tray ====================

void MainWindow::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/minecraft.png"));

    if (trayIcon->icon().isNull())
        trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));

    QMenu* trayMenu = new QMenu(this);
    trayMenu->addAction("Открыть лаунчер", this, &MainWindow::show);
    trayMenu->addAction("Выход", this, &QWidget::close);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::DoubleClick)
                    this->show();
            });
}

// ==================== Connections ====================

void MainWindow::setupConnections()
{
    connect(downloader, &MinecraftDownloader::downloadProgress, this,
            [this](qint64 received, qint64 total)
            {
                if (total <= 0) return;
                progressBar->show();
                int percent = static_cast<int>((received * 100) / total);
                progressBar->setValue(percent);
            });

    connect(downloader, &MinecraftDownloader::vanillaVersionsReceived,
            this, &MainWindow::onVanillaVersionsReceived);
    connect(downloader, &MinecraftDownloader::fabricVersionsReceived,
            this, &MainWindow::onFabricVersionsReceived);
    connect(downloader, &MinecraftDownloader::forgeVersionsReceived,
            this, &MainWindow::onForgeVersionsReceived);
    connect(downloader, &MinecraftDownloader::neoforgeVersionReceived,
            this, &MainWindow::onNeoForgeVersionReceived);

    connect(downloader, &MinecraftDownloader::instanceCreated, this,
            [this](const QString& path)
            {
                // При установке с загрузчиком базовое сообщение пропускаем —
                // финальное покажем после установки самого загрузчика.
                if (m_modLoaderPending)
                    return;
                progressBar->hide();
                QMessageBox::information(this, "Готово", "Игра установлена:\n" + path);
            });

    connect(downloader, &MinecraftDownloader::errorOccurred, this,
            [this](const QString& error)
            {
                QMessageBox::warning(this, "Ошибка", error);
            });

    connect(downloader, &MinecraftDownloader::totalProgress, this,
            [this](int percent)
            {
                progressBar->show();
                progressBar->setValue(percent);
            });

    connect(downloader, &MinecraftDownloader::javaRuntimeProgress, this,
            [this](int percent)
            {
                progressBar->show();
                progressBar->setValue(percent);
            });
}

// ==================== Installer ====================

void MainWindow::on_InstallerButton_clicked()
{
    QString versionText = VersionBox->currentText();
    QString loader = LoaderBox->currentText().toLower();

    QString cleanVersion = versionText;
    cleanVersion.remove("Fabric ");
    cleanVersion.remove("Forge ");
    cleanVersion.remove("NeoForge ");

    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    const bool modded = (loader != "vanilla" && !loader.isEmpty());

    progressBar->setValue(0);
    progressBar->show();

    if (!modded)
    {
        // Чистая ваниль.
        downloader->createInstance(cleanVersion, "", "", gameDir);
        QMessageBox::information(this, "Установка",
                                "Начато скачивание " + versionText);
        return;
    }

    // С загрузчиком: сначала ставим ванильную базу, затем сам загрузчик.
    m_modLoaderPending = true;

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::instanceCreated, this,
        [this, conn, loader, cleanVersion, gameDir](const QString&)
        {
            disconnect(*conn);
            startLoaderInstall(loader, cleanVersion, gameDir);
        });

    downloader->createInstance(cleanVersion, "", "", gameDir);
    QMessageBox::information(this, "Установка",
        "Начато скачивание " + versionText +
        ".\nПосле базовой версии будет установлен загрузчик "
        + loader + ".");
}

void MainWindow::startLoaderInstall(const QString& loader,
                                    const QString& mcVersion,
                                    const QString& gameDir)
{
    progressBar->setValue(0);
    progressBar->show();

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::loaderInstalled, this,
        [this, conn](const QString& versionId)
        {
            disconnect(*conn);
            m_modLoaderPending = false;
            progressBar->hide();
            QMessageBox::information(this, "Готово",
                "Загрузчик установлен:\n" + versionId);
        });

    if (loader == "fabric")
    {
        downloader->installFabric(mcVersion, gameDir);
    }
    else // forge / neoforge — нужен Java для запуска установщика
    {
        int requiredMajor = requiredJavaMajor(mcVersion);
        ensureJava(requiredMajor, mcVersion, gameDir,
            [this, loader, mcVersion, gameDir](const QString& javaExe)
            {
                downloader->installForgeLike(mcVersion, loader, javaExe, gameDir);
            });
    }
}

// ==================== Play ====================

void MainWindow::on_PlayButton_clicked()
{
    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    QString loader = LoaderBox ? LoaderBox->currentText().toLower() : "vanilla";

    QString versionText = VersionBox->currentText();
    QString version = versionText;
    version.remove("Fabric ");
    version.remove("Forge ");
    version.remove("NeoForge ");

    // ── Modded: запуск через профиль установленного загрузчика ──────────────
    if (loader != "vanilla" && !loader.isEmpty())
    {
        QString versionId =
            MinecraftDownloader::findInstalledLoaderId(gameDir, loader, version);

        if (versionId.isEmpty())
        {
            QMessageBox::warning(this, "Ошибка",
                "Загрузчик " + loader + " для " + version +
                " не установлен.\nНажмите «Установить».");
            return;
        }

        QString childPath =
            gameDir + "/versions/" + versionId + "/" + versionId + ".json";
        QFile childFile(childPath);
        if (!childFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "Ошибка",
                "Не удалось открыть профиль загрузчика:\n" + childPath);
            return;
        }
        QJsonObject childRoot =
            QJsonDocument::fromJson(childFile.readAll()).object();
        childFile.close();

        QString parentVersion = childRoot["inheritsFrom"].toString();
        if (parentVersion.isEmpty()) parentVersion = version;

        QString parentPath =
            gameDir + "/versions/" + parentVersion + "/" + parentVersion + ".json";
        QFile parentFile(parentPath);
        if (!parentFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, "Ошибка",
                "Базовая версия " + parentVersion + " не установлена.");
            return;
        }
        QJsonObject parentRoot =
            QJsonDocument::fromJson(parentFile.readAll()).object();
        parentFile.close();

        int requiredMajor =
            parentRoot["javaVersion"].toObject()["majorVersion"].toInt();
        if (requiredMajor <= 0)
            requiredMajor = requiredJavaMajor(parentVersion);

        ensureJava(requiredMajor, parentVersion, gameDir,
            [this, parentRoot, childRoot, gameDir, parentVersion, versionId,
             requiredMajor](const QString& javaExe)
            {
                launchModded(parentRoot, childRoot, gameDir, parentVersion,
                             versionId, javaExe, requiredMajor);
            });
        return;
    }

    // ── Vanilla ─────────────────────────────────────────────────────────────
    QString versionDir = gameDir + "/versions/" + version;
    QString jsonPath   = versionDir + "/" + version + ".json";

    if (!QFileInfo::exists(jsonPath))
    {
        QMessageBox::warning(this, "Ошибка", "Версия не установлена:\n" + jsonPath);
        return;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть version.json");
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QString mainClass = root["mainClass"].toString();
    if (mainClass.isEmpty())
    {
        QMessageBox::warning(this, "Ошибка", "mainClass не найден в version.json");
        return;
    }

    int requiredMajor = root["javaVersion"].toObject()["majorVersion"].toInt();
    if (requiredMajor <= 0)
        requiredMajor = requiredJavaMajor(version);

    ensureJava(requiredMajor, version, gameDir,
        [this, root, gameDir, version, versionDir, mainClass, requiredMajor]
        (const QString& javaExe)
        {
            launchGame(root, gameDir, version, versionDir, mainClass,
                       javaExe, requiredMajor);
        });
}

// ==================== Java provisioning ====================

void MainWindow::ensureJava(int requiredMajor,
                            const QString& mcVersion,
                            const QString& gameDir,
                            std::function<void(QString)> cb)
{
    Q_UNUSED(mcVersion);

    const QString javaComponent = javaComponentForMajor(requiredMajor);

    // 1) Подходящая Java уже установлена в системе?
    QString javaPath = pickCompatibleInstalledJava(
        installedJavas, requiredMajor, settingsWindow->javaPath());
    if (!javaPath.isEmpty())
    {
        cb(javaPath);
        return;
    }

    // 2) Уже скачивали Java от Mojang для этого компонента?
    QString runtimeDir = gameDir + "/runtime/" + javaComponent;
#if defined(Q_OS_WIN)
    QString runtimeExe = runtimeDir + "/bin/javaw.exe";
#elif defined(Q_OS_MAC)
    QString runtimeExe = runtimeDir + "/jre.bundle/Contents/Home/bin/java";
#else
    QString runtimeExe = runtimeDir + "/bin/java";
#endif
    if (QFileInfo::exists(runtimeExe))
    {
        installedJavas[requiredMajor] = runtimeExe;
        cb(runtimeExe);
        return;
    }

    // 3) Качаем официальную Java от Mojang и вызываем cb после загрузки.
    progressBar->setValue(0);
    progressBar->show();
    QMessageBox::information(this, "Java",
        "Нужна Java " + QString::number(requiredMajor) +
        ".\nСкачиваю официальную Java от Mojang — это разовая операция, "
        "дождитесь завершения загрузки.");

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::javaRuntimeReady, this,
        [this, conn, requiredMajor, cb](const QString& javaExe)
        {
            disconnect(*conn);
            progressBar->hide();
            installedJavas[requiredMajor] = javaExe;
            cb(javaExe);
        });

    downloader->downloadJavaRuntime(javaComponent, runtimeDir);
}

// ==================== Launch ====================

void MainWindow::launchGame(const QJsonObject& root,
                            const QString& gameDir,
                            const QString& version,
                            const QString& versionDir,
                            const QString& mainClass,
                            const QString& javaPath,
                            int neededJava)
{
    int selectedMajor = getJavaMajorVersion(javaPath);

    qDebug() << "MC" << version << "launching with Java" << selectedMajor
             << "at" << javaPath << "(needs" << neededJava << ")";

    // ── Classpath ──────────────────────────────────────────────────────────

#ifdef Q_OS_WIN
    const QString sep = ";";
#else
    const QString sep = ":";
#endif

    QString classPath;
    QJsonArray libraries = root["libraries"].toArray();

    for (const auto& value : libraries)
    {
        QJsonObject lib       = value.toObject();
        QJsonObject downloads = lib["downloads"].toObject();

        QJsonArray rules = lib["rules"].toArray();
        if (!rules.isEmpty())
        {
            bool allowed = false;
            for (const auto& rv : rules)
            {
                QJsonObject rule = rv.toObject();
                QString action   = rule["action"].toString();
                if (rule.contains("os"))
                {
                    QString osName = rule["os"].toObject()["name"].toString();
#ifdef Q_OS_WIN
                    QString cur = "windows";
#elif defined(Q_OS_MAC)
                    QString cur = "osx";
#else
                    QString cur = "linux";
#endif
                    if (osName == cur) allowed = (action == "allow");
                }
                else
                {
                    allowed = (action == "allow");
                }
            }
            if (!allowed) continue;
        }

        if (downloads.contains("artifact"))
        {
            QString path = downloads["artifact"].toObject()["path"].toString();
            if (!path.isEmpty())
            {
                QString fullPath = gameDir + "/libraries/" + path;
                if (QFileInfo::exists(fullPath))
                {
                    if (!classPath.isEmpty()) classPath += sep;
                    classPath += fullPath;
                }
            }
        }
    }

    if (!classPath.isEmpty()) classPath += sep;
    classPath += versionDir + "/" + version + ".jar";

    // ── Natives ──────────────────────────────────────────────────────────────
    // Распаковываем нативные библиотеки в versions/<id>/natives.
    // Без этого старые версии (использующие classifiers) падают при запуске.
    extractNativesForVersion(root, gameDir, versionDir + "/natives");

    // ── RAM ────────────────────────────────────────────────────────────────

    int ram = settingsWindow->ramAmount();
    if (ram < 1) ram = 2;
    int ramMin = qMax(1, ram / 2);

    // ── UUID (offline) ─────────────────────────────────────────────────────

    QString username = settingsWindow->username();
    if (username.isEmpty()) username = "Player";

    QString offlineUUID = QCryptographicHash::hash(
                              ("OfflinePlayer:" + username).toUtf8(),
                              QCryptographicHash::Md5).toHex();

    offlineUUID.insert(8,  '-');
    offlineUUID.insert(13, '-');
    offlineUUID.insert(18, '-');
    offlineUUID.insert(23, '-');

    // ── Game arguments ─────────────────────────────────────────────────────

    bool modernArgs = root.contains("arguments");
    QStringList gameArgs;

    auto replaceVars = [&](QString arg) -> QString {
        arg.replace("${auth_player_name}",  username);
        arg.replace("${version_name}",      version);
        arg.replace("${game_directory}",    gameDir);
        arg.replace("${assets_root}",       gameDir + "/assets");
        arg.replace("${assets_index_name}", root["assets"].toString());
        arg.replace("${auth_uuid}",         offlineUUID);
        arg.replace("${auth_access_token}", "0");
        arg.replace("${user_type}",         "legacy");
        arg.replace("${version_type}",      root["type"].toString());
        arg.replace("${clientid}",          "0");
        arg.replace("${auth_xuid}",         "0");
        arg.replace("${user_properties}",   "{}");
        return arg;
    };

    if (modernArgs)
    {
        QJsonArray argArray = root["arguments"].toObject()["game"].toArray();
        for (const auto& av : argArray)
            if (av.isString())
                gameArgs << replaceVars(av.toString());
    }
    else
    {
        QString minecraftArgs = root["minecraftArguments"].toString();
        gameArgs = replaceVars(minecraftArgs).split(' ', Qt::SkipEmptyParts);
    }

    // ── JVM arguments ──────────────────────────────────────────────────────

    QStringList jvmArgs;
    jvmArgs << "-Xms" + QString::number(ramMin) + "G"
            << "-Xmx" + QString::number(ram)    + "G"
            << "-Djava.library.path=" + versionDir + "/natives"
            << "-Dfile.encoding=UTF-8"
            << "-Dlog4j2.formatMsgNoLookups=true";

    QVersionNumber ver = QVersionNumber::fromString(version);

    // Для Java 17+ нужны дополнительные флаги открытия модулей (1.18+)
    if (selectedMajor >= 17)
    {
        jvmArgs << "--add-opens=java.base/java.util=ALL-UNNAMED"
                << "--add-opens=java.base/java.lang=ALL-UNNAMED"
                << "--add-opens=java.base/java.lang.reflect=ALL-UNNAMED"
                << "--add-opens=java.base/java.io=ALL-UNNAMED"
                << "--add-exports=java.base/sun.security.util=ALL-UNNAMED"
                << "--add-exports=jdk.naming.dns/com.sun.jndi.dns=ALL-UNNAMED";
    }

    // Для старых версий (до 1.13) lwjgl стабильнее с IPv4
    if (ver <= QVersionNumber(1, 12, 2))
        jvmArgs << "-Djava.net.preferIPv4Stack=true";

    jvmArgs << "-cp" << classPath << mainClass;

    startMinecraftProcess(javaPath, jvmArgs, gameArgs, gameDir, version, neededJava);
}

void MainWindow::startMinecraftProcess(const QString& javaPath,
                                       const QStringList& jvmArgs,
                                       const QStringList& gameArgs,
                                       const QString& gameDir,
                                       const QString& version,
                                       int neededJava)
{
    const QStringList allArgs = jvmArgs + gameArgs;

    qDebug() << "Launching:" << javaPath;
    qDebug() << "Args:" << allArgs;

    // ── Launch ─────────────────────────────────────────────────────────────

    crashLog.clear();

    // Сохраняем параметры запуска в лог сразу — полезно при диагностике
    crashLog += "=== Параметры запуска ===\n";
    crashLog += "Java:    " + javaPath + "\n";
    crashLog += "MC:      " + version  + "\n";
    crashLog += "GameDir: " + gameDir  + "\n";
    crashLog += "JVM:     " + jvmArgs.join(" ") + "\n";
    crashLog += "Args:    " + gameArgs.join(" ") + "\n\n";

    minecraftProcess = new QProcess(this);
    minecraftProcess->setWorkingDirectory(gameDir);

    // Собираем весь вывод в crashLog
    connect(minecraftProcess, &QProcess::readyReadStandardOutput, this, [this]()
            {
                crashLog += minecraftProcess->readAllStandardOutput();
            });

    connect(minecraftProcess, &QProcess::readyReadStandardError, this, [this]()
            {
                crashLog += minecraftProcess->readAllStandardError();
            });

    connect(minecraftProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onMinecraftFinished);

    minecraftProcess->start(javaPath, allArgs);

    if (!minecraftProcess->waitForStarted(5000))
    {
        crashLog += "\n[ОШИБКА] Процесс не запустился (waitForStarted timeout)\n";
        showCrashDialog(neededJava, javaPath);
        delete minecraftProcess;
        minecraftProcess = nullptr;
        return;
    }

    hide();
}

// ==================== Launch (modded) ====================

// Проверка os-правил аргумента (современный формат arguments.jvm/game).
static bool argRulesAllow(const QJsonObject& argObj)
{
    const QJsonArray rules = argObj["rules"].toArray();
    if (rules.isEmpty())
        return true;

    bool allowed = false;
    for (const auto& rv : rules)
    {
        const QJsonObject rule = rv.toObject();
        const QString action = rule["action"].toString();

        // Аргументы под фичи (demo, has_custom_resolution, quick_play) — пропуск.
        if (rule.contains("features"))
            return false;

        if (rule.contains("os"))
        {
            const QString osName = rule["os"].toObject()["name"].toString();
#ifdef Q_OS_WIN
            const QString cur = "windows";
#elif defined(Q_OS_MAC)
            const QString cur = "osx";
#else
            const QString cur = "linux";
#endif
            if (osName.isEmpty() || osName == cur)
                allowed = (action == "allow");
        }
        else
        {
            allowed = (action == "allow");
        }
    }
    return allowed;
}

// group:artifact из Maven-координат — для дедупликации classpath.
static QString mavenKey(const QString& name)
{
    const QStringList parts = name.split(':');
    if (parts.size() < 2)
        return QString();
    return parts[0] + ":" + parts[1];
}

void MainWindow::launchModded(const QJsonObject& parentRoot,
                              const QJsonObject& childRoot,
                              const QString& gameDir,
                              const QString& mcVersion,
                              const QString& versionId,
                              const QString& javaPath,
                              int neededJava)
{
    const int selectedMajor = getJavaMajorVersion(javaPath);

    const QString versionDir = gameDir + "/versions/" + versionId;
    const QString nativesDir = versionDir + "/natives";

#ifdef Q_OS_WIN
    const QString sep = ";";
#else
    const QString sep = ":";
#endif

    QString mainClass = childRoot["mainClass"].toString();
    if (mainClass.isEmpty())
        mainClass = parentRoot["mainClass"].toString();

    // ── Classpath: библиотеки загрузчика (приоритет) + ванильные + клиент ──
    QStringList cpEntries;
    QSet<QString> seenKeys;

    auto addLibs = [&](const QJsonObject& root)
    {
        const QJsonArray libs = root["libraries"].toArray();
        for (const auto& lv : libs)
        {
            const QJsonObject lib = lv.toObject();
            if (!nativeLibraryAllowedOnCurrentOS(lib))
                continue;

            const QString name = lib["name"].toString();
            const QString key  = mavenKey(name);
            if (!key.isEmpty() && seenKeys.contains(key))
                continue;

            QString rel;
            const QJsonObject dl = lib["downloads"].toObject();
            if (dl.contains("artifact"))
                rel = dl["artifact"].toObject()["path"].toString();
            if (rel.isEmpty() && !name.isEmpty())
                rel = MinecraftDownloader::mavenNameToPath(name);
            if (rel.isEmpty())
                continue;

            const QString full = gameDir + "/libraries/" + rel;
            if (!QFileInfo::exists(full))
                continue;

            if (!key.isEmpty())
                seenKeys.insert(key);
            cpEntries << full;
        }
    };

    addLibs(childRoot);   // загрузчик переопределяет ванильные версии библиотек
    addLibs(parentRoot);

    cpEntries << gameDir + "/versions/" + mcVersion + "/" + mcVersion + ".jar";

    const QString classPath = cpEntries.join(sep);

    // ── Natives из ванильных библиотек ──
    extractNativesForVersion(parentRoot, gameDir, nativesDir);

    // ── RAM ──
    int ram = settingsWindow->ramAmount();
    if (ram < 1) ram = 2;
    const int ramMin = qMax(1, ram / 2);

    // ── UUID (offline) ──
    QString username = settingsWindow->username();
    if (username.isEmpty()) username = "Player";

    QString offlineUUID = QCryptographicHash::hash(
                              ("OfflinePlayer:" + username).toUtf8(),
                              QCryptographicHash::Md5).toHex();
    offlineUUID.insert(8,  '-');
    offlineUUID.insert(13, '-');
    offlineUUID.insert(18, '-');
    offlineUUID.insert(23, '-');

    const QString assetsIndex = parentRoot["assets"].toString();
    const QString versionType = childRoot.contains("type")
                                    ? childRoot["type"].toString()
                                    : parentRoot["type"].toString();

    auto replaceVars = [&](QString arg) -> QString {
        arg.replace("${auth_player_name}",    username);
        arg.replace("${version_name}",        versionId);
        arg.replace("${game_directory}",      gameDir);
        arg.replace("${assets_root}",         gameDir + "/assets");
        arg.replace("${game_assets}",         gameDir + "/assets");
        arg.replace("${assets_index_name}",   assetsIndex);
        arg.replace("${auth_uuid}",           offlineUUID);
        arg.replace("${auth_access_token}",   "0");
        arg.replace("${auth_session}",        "0");
        arg.replace("${user_type}",           "legacy");
        arg.replace("${version_type}",        versionType);
        arg.replace("${clientid}",            "0");
        arg.replace("${auth_xuid}",           "0");
        arg.replace("${user_properties}",     "{}");
        arg.replace("${natives_directory}",   nativesDir);
        arg.replace("${library_directory}",   gameDir + "/libraries");
        arg.replace("${classpath_separator}", sep);
        arg.replace("${classpath}",           classPath);
        arg.replace("${launcher_name}",       "yanoml");
        arg.replace("${launcher_version}",    "1.0");
        return arg;
    };

    // ── JVM аргументы ──
    QStringList jvmArgs;
    jvmArgs << "-Xms" + QString::number(ramMin) + "G"
            << "-Xmx" + QString::number(ram)    + "G"
            << "-Djava.library.path=" + nativesDir
            << "-Dfile.encoding=UTF-8"
            << "-Dlog4j2.formatMsgNoLookups=true";

    const QVersionNumber ver = QVersionNumber::fromString(mcVersion);

    if (selectedMajor >= 17)
    {
        jvmArgs << "--add-opens=java.base/java.util=ALL-UNNAMED"
                << "--add-opens=java.base/java.lang=ALL-UNNAMED"
                << "--add-opens=java.base/java.lang.reflect=ALL-UNNAMED"
                << "--add-opens=java.base/java.io=ALL-UNNAMED"
                << "--add-exports=java.base/sun.security.util=ALL-UNNAMED"
                << "--add-exports=jdk.naming.dns/com.sun.jndi.dns=ALL-UNNAMED";
    }

    if (ver <= QVersionNumber(1, 12, 2))
        jvmArgs << "-Djava.net.preferIPv4Stack=true";

    // Доп. JVM-аргументы загрузчика (модульный путь Forge/NeoForge и т.п.).
    if (childRoot.contains("arguments"))
    {
        const QJsonArray jvm = childRoot["arguments"].toObject()["jvm"].toArray();
        bool skipNext = false;
        for (const auto& av : jvm)
        {
            if (av.isString())
            {
                const QString s = av.toString();
                if (skipNext) { skipNext = false; continue; }
                if (s == "-cp" || s == "-classpath" || s == "--class-path")
                {
                    skipNext = true;
                    continue;
                }
                if (s.contains("${classpath}"))
                    continue;
                jvmArgs << replaceVars(s);
            }
            else
            {
                const QJsonObject o = av.toObject();
                if (!argRulesAllow(o)) continue;
                const QJsonValue val = o["value"];
                if (val.isString())
                    jvmArgs << replaceVars(val.toString());
                else
                    for (const auto& vv : val.toArray())
                        jvmArgs << replaceVars(vv.toString());
            }
        }
    }

    jvmArgs << "-cp" << classPath << mainClass;

    // ── Game аргументы ──
    QStringList gameArgs;

    auto appendGameFromRoot = [&](const QJsonObject& root)
    {
        if (root.contains("minecraftArguments"))
        {
            gameArgs << replaceVars(root["minecraftArguments"].toString())
                            .split(' ', Qt::SkipEmptyParts);
        }
        else if (root.contains("arguments"))
        {
            const QJsonArray g = root["arguments"].toObject()["game"].toArray();
            for (const auto& av : g)
                if (av.isString())
                    gameArgs << replaceVars(av.toString());
        }
    };

    if (childRoot.contains("minecraftArguments"))
    {
        // Legacy Forge: строка уже содержит базовые аргументы + --tweakClass.
        appendGameFromRoot(childRoot);
    }
    else
    {
        appendGameFromRoot(parentRoot);
        if (childRoot.contains("arguments"))
        {
            const QJsonArray g =
                childRoot["arguments"].toObject()["game"].toArray();
            for (const auto& av : g)
                if (av.isString())
                    gameArgs << replaceVars(av.toString());
        }
    }

    startMinecraftProcess(javaPath, jvmArgs, gameArgs, gameDir,
                          versionId, neededJava);
}

void MainWindow::showCrashDialog(int neededJava, const QString& javaPath)
{
    show();
    activateWindow();
    raise();

    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle("Minecraft — ошибка запуска");
    dlg->resize(900, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* lay = new QVBoxLayout(dlg);

    QLabel* hint = new QLabel(dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-weight: bold; color: #c0392b;");

    // Анализируем лог и даём понятную подсказку
    QString hintText;

    if (crashLog.contains("UnsupportedClassVersionError"))
        hintText = "⚠ Неподходящая версия Java!\n"
                   "Для этой версии MC нужна Java " + QString::number(neededJava) +
                   " или новее.\nТекущая Java: " + javaPath;
    else if (crashLog.contains("Could not find or load main class"))
        hintText = "⚠ Classpath неверный — не найден главный класс.\n"
                   "Возможно, игра установлена не полностью. Попробуйте переустановить.";
    else if (crashLog.contains("natives") || crashLog.contains("lwjgl"))
        hintText = "⚠ Ошибка нативных библиотек (LWJGL/natives).\n"
                   "Попробуйте переустановить версию.";
    else if (crashLog.contains("OutOfMemoryError"))
        hintText = "⚠ Недостаточно оперативной памяти.\n"
                   "Уменьшите количество RAM в настройках.";
    else if (crashLog.contains("Invalid maximum heap size") || crashLog.contains("Invalid initial heap size"))
        hintText = "⚠ Неверный размер памяти.\n"
                   "Проверьте настройки RAM — значение слишком большое для вашей системы.";
    else if (crashLog.contains("Error occurred during initialization of VM"))
        hintText = "⚠ JVM не смогла инициализироваться.\n"
                   "Проверьте путь к Java и объём RAM в настройках.";
    else if (crashLog.contains("processNotStarted") || crashLog.contains("timeout"))
        hintText = "⚠ Java не найдена или не запустилась.\n"
                   "Путь к Java: " + javaPath + "\n"
                                "Установите Java " + QString::number(neededJava) + " и укажите путь в настройках.";
    else
        hintText = "Minecraft завершился с ошибкой. Смотрите лог ниже.";

    hint->setText(hintText);
    lay->addWidget(hint);

    QTextEdit* logEdit = new QTextEdit(dlg);
    logEdit->setReadOnly(true);
    logEdit->setFont(QFont("Courier New", 9));
    logEdit->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    logEdit->setPlainText(crashLog);
    // Прокрутить к концу — там обычно самое важное
    logEdit->moveCursor(QTextCursor::End);
    lay->addWidget(logEdit, 1);

    QHBoxLayout* btnRow = new QHBoxLayout();

    QPushButton* copyBtn = new QPushButton("📋 Копировать лог", dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [this]()
            {
                QApplication::clipboard()->setText(crashLog);
            });

    QPushButton* closeBtn = new QPushButton("Закрыть", dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    dlg->exec();
}

void MainWindow::onMinecraftFinished(int exitCode, QProcess::ExitStatus)
{
    // Дочитываем остатки вывода
    if (minecraftProcess)
    {
        crashLog += minecraftProcess->readAllStandardOutput();
        crashLog += minecraftProcess->readAllStandardError();
        minecraftProcess->deleteLater();
        minecraftProcess = nullptr;
    }

    show();
    activateWindow();
    raise();

    if (exitCode != 0)
        showCrashDialog(0, "");
}

// ==================== Other buttons ====================

void MainWindow::on_ModPlatformButton_clicked()
{
    ModWindow* window = new ModWindow(this);
    window->setSettingsWindow(settingsWindow);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->exec();
}

void MainWindow::on_UpdateButton_clicked()
{
    QMessageBox::information(this, "Обновление", "Проверка обновлений...");
}

void MainWindow::on_ElyByButton_clicked() {}

void MainWindow::on_SettingsButton_clicked()
{
    settingsWindow->exec();
}

void MainWindow::on_PickAccountButton_clicked()
{
    QMessageBox::information(this, "Аккаунт", "Выбор аккаунта...");
}

// ==================== Versions ====================

void MainWindow::onLoaderChanged(const QString& loader)
{
    VersionBox->clear();

    if (loader == "Vanilla")        downloader->fetchVanillaVersions();
    else if (loader == "Fabric")    downloader->fetchFabricVersions();
    else if (loader == "Forge")     downloader->fetchForgeVersions();
    else if (loader == "NeoForge")  downloader->fetchNeoForgeVersions();
}

void MainWindow::onShowSnapshotsChanged(int)
{
    VersionBox->clear();
    VersionBox->addItem("Обновление списка...");
    downloader->fetchVanillaVersions();
}

void MainWindow::onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions)
{
    VersionBox->clear();

    QVector<MinecraftVersion> sorted = versions;
    std::sort(sorted.begin(), sorted.end(),
              [](const MinecraftVersion& a, const MinecraftVersion& b) {
                  return QVersionNumber::fromString(a.gameVersion)
                  > QVersionNumber::fromString(b.gameVersion);
              });

    bool showSnapshots = settingsWindow && settingsWindow->showSnapshots();

    for (const auto& ver : sorted)
    {
        if (!showSnapshots && ver.loaderType != "release") continue;
        VersionBox->addItem(ver.gameVersion);
    }
}

void MainWindow::onFabricVersionsReceived(const QJsonArray& versions)
{
    VersionBox->clear();
    for (const auto& value : versions)
    {
        QJsonObject obj = value.toObject();
        if (!settingsWindow->showSnapshots() && !obj["stable"].toBool()) continue;
        VersionBox->addItem("Fabric " + obj["version"].toString());
    }
}

void MainWindow::onForgeVersionsReceived(const QJsonObject& json)
{
    VersionBox->clear();
    QSet<QString> mcVersions;
    QJsonObject promos = json["promos"].toObject();

    for (auto it = promos.begin(); it != promos.end(); ++it)
        mcVersions.insert(it.key().section('-', 0, 0));

    QStringList versions = mcVersions.values();
    std::sort(versions.begin(), versions.end(),
              [](const QString& a, const QString& b) {
                  return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
              });

    for (const QString& v : versions)
        VersionBox->addItem("Forge " + v);
}

// Преобразует версию NeoForge из Maven в версию Minecraft.
// Новая схема (1.20.2+): MAJOR.MINOR.PATCH -> 1.MAJOR.MINOR (при MINOR == 0 это
// 1.MAJOR, напр. 21.0.x -> 1.21). Версии вида 47.x.x — это NeoForge для 1.20.1
// (ответвление от Forge 47).
static QString neoForgeToMcVersion(const QString& neoVersion)
{
    if (neoVersion.startsWith("47."))
        return "1.20.1";

    const QStringList parts = neoVersion.split('.');
    if (parts.size() >= 2)
    {
        bool okMajor = false, okMinor = false;
        const int major = parts[0].toInt(&okMajor);
        const int minor = parts[1].section('-', 0, 0).toInt(&okMinor);
        if (okMajor && okMinor)
            return minor == 0 ? QString("1.%1").arg(major)
                              : QString("1.%1.%2").arg(major).arg(minor);
    }
    return neoVersion;
}

void MainWindow::onNeoForgeVersionReceived(const QString& xml)
{
    VersionBox->clear();
    QStringList lines = xml.split('\n');
    QSet<QString> added;
    QStringList mcVersions;

    for (const QString& line : lines)
    {
        if (!line.contains("<version>")) continue;

        QString version = QString(line)
                              .remove("<version>")
                              .remove("</version>")
                              .trimmed();

        if ((version.contains("beta",  Qt::CaseInsensitive) ||
             version.contains("alpha", Qt::CaseInsensitive) ||
             version.contains("rc",    Qt::CaseInsensitive))
            && !settingsWindow->showSnapshots())
            continue;

        const QString mcVersion = neoForgeToMcVersion(version);

        if (!added.contains(mcVersion))
        {
            added.insert(mcVersion);
            mcVersions << mcVersion;
        }
    }

    std::sort(mcVersions.begin(), mcVersions.end(),
              [](const QString& a, const QString& b) {
                  return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
              });

    for (const QString& v : mcVersions)
        VersionBox->addItem(v);
}

void MainWindow::loadVersions()
{
    if (!LoaderBox)
    {
        LoaderBox = new QComboBox(centralwidget);
        LoaderBox->setGeometry(QRect(780, 640, 250, 30));
        LoaderBox->addItems({"Vanilla", "Fabric", "Forge", "NeoForge"});
        connect(LoaderBox, &QComboBox::currentTextChanged,
                this, &MainWindow::onLoaderChanged);
    }

    if (!VersionBox)
    {
        VersionBox = new QComboBox(centralwidget);
        VersionBox->setGeometry(QRect(1050, 640, 250, 30));
    }

    VersionBox->clear();
    VersionBox->addItem("Загрузка версий...");
    downloader->fetchVanillaVersions();
}