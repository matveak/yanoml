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

// Выбирает лучший подходящий путь к Java для данной версии MC
// Приоритет: минимальная подходящая версия (не берём 21 там, где хватит 8)
static QString selectJavaForVersion(const QString& mcVersion,
                                    const QMap<int, QString>& javas,
                                    const QString& userJavaPath)
{
    int needed = requiredJavaMajor(mcVersion);

    // Сначала проверяем путь пользователя из настроек
    if (!userJavaPath.isEmpty() && QFileInfo::exists(userJavaPath))
    {
        int userMajor = getJavaMajorVersion(userJavaPath);
        if (userMajor >= needed)
        {
            qDebug() << "Using user-specified Java" << userMajor << "for MC" << mcVersion;
            return userJavaPath;
        }
        qDebug() << "User Java" << userMajor << "too old for MC" << mcVersion
                 << "(need" << needed << "), searching for better...";
    }

    // Ищем минимальную подходящую версию из найденных
    QString best;
    int bestMajor = INT_MAX;

    for (auto it = javas.constBegin(); it != javas.constEnd(); ++it)
    {
        int major = it.key();
        if (major >= needed && major < bestMajor)
        {
            bestMajor = major;
            best = it.value();
        }
    }

    if (!best.isEmpty())
    {
        qDebug() << "Auto-selected Java" << bestMajor << "for MC" << mcVersion;
        return best;
    }

    // Ничего не нашли — возвращаем системную java и надеемся на лучшее
#ifdef Q_OS_WIN
    return "javaw";
#else
    return "java";
#endif
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

    downloader->createInstance(
        cleanVersion,
        loader == "vanilla" ? "" : loader,
        "",
        gameDir);

    progressBar->setValue(0);
    progressBar->show();

    QMessageBox::information(this, "Установка", "Начато скачивание " + versionText);
}

// ==================== Play ====================

void MainWindow::on_PlayButton_clicked()
{
    QString gameDir = settingsWindow->minecraftPath();
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    QString versionText = VersionBox->currentText();
    QString version = versionText;
    version.remove("Fabric ");
    version.remove("Forge ");
    version.remove("NeoForge ");

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

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject root = doc.object();

    QString mainClass = root["mainClass"].toString();
    if (mainClass.isEmpty())
    {
        QMessageBox::warning(this, "Ошибка", "mainClass не найден в version.json");
        return;
    }

    // ── Java: автовыбор ────────────────────────────────────────────────────

    QString javaPath = selectJavaForVersion(
        version,
        installedJavas,
        settingsWindow->javaPath());

    int neededJava  = requiredJavaMajor(version);
    int selectedMajor = getJavaMajorVersion(javaPath);

    qDebug() << "MC" << version << "needs Java" << neededJava
             << "-> using Java" << selectedMajor << "at" << javaPath;

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

    QStringList allArgs = jvmArgs + gameArgs;

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

void MainWindow::onNeoForgeVersionReceived(const QString& xml)
{
    VersionBox->clear();
    QStringList lines = xml.split('\n');
    QSet<QString> added;

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

        QString mcVersion = version;
        if (mcVersion.startsWith("21.")) mcVersion = "1." + mcVersion;

        if (!added.contains(mcVersion))
        {
            added.insert(mcVersion);
            VersionBox->addItem(mcVersion);
        }
    }
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