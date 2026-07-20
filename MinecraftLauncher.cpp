//
// Created by ghhg6 on 05.07.2026.
//

#include "MinecraftLauncher.h"

#include <QCryptographicHash>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QMessageBox>
#include <QProcess>
#include <QVersionNumber>
#include "JavaUtils.h"



void MinecraftLauncher::startMinecraftProcess(const QString& javaPath,
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

	m_crashLog.clear();

	// Сохраняем параметры запуска в лог сразу — полезно при диагностике
	m_crashLog += "=== Параметры запуска ===\n";
	m_crashLog += "Java:    " + javaPath + "\n";
	m_crashLog += "MC:      " + version  + "\n";
	m_crashLog += "GameDir: " + gameDir  + "\n";
	m_crashLog += "JVM:     " + jvmArgs.join(" ") + "\n";
	m_crashLog += "Args:    " + gameArgs.join(" ") + "\n\n";
	m_crashLog += "=========================\n";


	m_minecraftProcess = new QProcess(this);
	m_minecraftProcess->setWorkingDirectory(gameDir);

	// Собираем весь вывод в crashLog
	connect(m_minecraftProcess, &QProcess::readyReadStandardOutput, this, [this]()
			{
				m_crashLog += m_minecraftProcess->readAllStandardOutput();
			});

	connect(m_minecraftProcess, &QProcess::readyReadStandardError, this, [this]()
			{
				m_crashLog += m_minecraftProcess->readAllStandardError();
			});

	connect(m_minecraftProcess,
			QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, &MinecraftLauncher::MinecraftFinished);

	m_minecraftProcess->start(javaPath, allArgs);

	if (!m_minecraftProcess->waitForStarted(5000))
	{
		m_crashLog += "\n[ОШИБКА] Процесс не запустился (waitForStarted timeout)\n";
		emit onMineCraftCrash(neededJava, javaPath);
		delete m_minecraftProcess;
		m_minecraftProcess = nullptr;
		return;
	}

}


void MinecraftLauncher::MinecraftFinished(int exitCode, QProcess::ExitStatus st)
{
	// Дочитываем остатки вывода
	if (m_minecraftProcess)
	{
		m_crashLog += m_minecraftProcess->readAllStandardOutput();
		m_crashLog += m_minecraftProcess->readAllStandardError();
		m_minecraftProcess->deleteLater();
		m_minecraftProcess = nullptr;
	}

	emit onMinecraftFinished(exitCode, st);

	if (exitCode != 0)
		emit onMineCraftCrash(0, "");
}

QString MinecraftLauncher::getCrashHint(int neededJava, const QString& javaPath) const {
	QString hintText;
	if (m_crashLog.contains("UnsupportedClassVersionError"))
		hintText = "⚠ Неподходящая версия Java!\n"
				   "Для этой версии MC нужна Java " + QString::number(neededJava) +
				   " или новее.\nТекущая Java: " + javaPath;
	else if (m_crashLog.contains("Could not find or load main class"))
		hintText = "⚠ Classpath неверный — не найден главный класс.\n"
				   "Возможно, игра установлена не полностью. Попробуйте переустановить.";
	else if (m_crashLog.contains("natives") || m_crashLog.contains("lwjgl"))
		hintText = "⚠ Ошибка нативных библиотек (LWJGL/natives).\n"
				   "Попробуйте переустановить версию.";
	else if (m_crashLog.contains("OutOfMemoryError"))
		hintText = "⚠ Недостаточно оперативной памяти.\n"
				   "Уменьшите количество RAM в настройках.";
	else if (m_crashLog.contains("Invalid maximum heap size") || m_crashLog.contains("Invalid initial heap size"))
		hintText = "⚠ Неверный размер памяти.\n"
				   "Проверьте настройки RAM — значение слишком большое для вашей системы.";
	else if (m_crashLog.contains("Error occurred during initialization of VM"))
		hintText = "⚠ JVM не смогла инициализироваться.\n"
				   "Проверьте путь к Java и объём RAM в настройках.";
	else if (m_crashLog.contains("processNotStarted") || m_crashLog.contains("timeout"))
		hintText = "⚠ Java не найдена или не запустилась.\n"
				   "Путь к Java: " + javaPath + "\n"
								"Установите Java " + QString::number(neededJava) + " и укажите путь в настройках.";
	else
		hintText = "Minecraft завершился с ошибкой. Смотрите лог ниже.";
	return hintText;
}

void MinecraftLauncher::ensureJava(const QString& mcVersion,
                            const QString& gameDir,
                            const std::function<void(QString)>& cb, const QString& jp)
{
	int requiredMajor = requiredJavaMajor(mcVersion);

    const QString javaComponent = javaComponentForMajor(requiredMajor);

    // 1) Подходящая Java уже установлена в системе?
    QString javaPath = pickCompatibleInstalledJava(
        m_installedJavas, requiredMajor, jp);
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
        m_installedJavas[requiredMajor] = runtimeExe;
        cb(runtimeExe);
        return;
    }

    // 3) Качаем официальную Java от Mojang и вызываем cb после загрузки.
    //TODO: idk what to do with this
    //progressBar->setValue(0);
    //progressBar->show();
    //QMessageBox::information(this, "Java",
    //                         "Нужна Java " + QString::number(requiredMajor) +
    //                             ".\nСкачиваю официальную Java от Mojang — это разовая операция, "
    //                             "дождитесь завершения загрузки.");

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_downloader, &MinecraftDownloader::javaRuntimeReady, this,
                    [this, conn, requiredMajor, cb](const QString& javaExe)
                    {
                        disconnect(*conn);
                        m_installedJavas[requiredMajor] = javaExe;
                        cb(javaExe);
                    });

    m_downloader->jd.downloadJavaRuntime(javaComponent, runtimeDir);
}

void MinecraftLauncher::launchGame(const QJsonObject& root,
                            const QString& gameDir,
                            const QString& version,
                            const QString& versionDir,
                            const QString& mainClass,
                            const QString& javaPath,
                            QString username,
                            int ram,
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

    if (ram < 1) ram = 2;
    int ramMin = qMax(1, ram / 2);

    // ── UUID (offline) ─────────────────────────────────────────────────────

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

void MinecraftLauncher::launchModded(const QJsonObject& parentRoot,
                              const QJsonObject& childRoot,
                              const QString& gameDir,
                              const QString& mcVersion,
                              const QString& versionId,
                              const QString& javaPath,
                              QString username,
                              int ram,
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
                rel = mavenNameToPath(name);
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
    if (ram < 1) ram = 2;
    const int ramMin = qMax(1, ram / 2);

    // ── UUID (offline) ──
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
