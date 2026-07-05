//
// Created by ghhg6 on 05.07.2026.
//

#include "MinecraftLauncher.h"

#include <QCryptographicHash>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QVersionNumber>

static QString mavenNameToPath(const QString& name)
{
	// group:artifact:version[:classifier][@ext]
	QString work = name;
	QString ext = "jar";

	const int at = work.indexOf('@');
	if (at >= 0)
	{
		ext  = work.mid(at + 1);
		work = work.left(at);
	}

	const QStringList parts = work.split(':');
	if (parts.size() < 3)
		return QString();

	QString group      = parts[0];
	const QString artifact   = parts[1];
	const QString version    = parts[2];
	const QString classifier = parts.size() >= 4 ? parts[3] : QString();

	QString file = artifact + "-" + version;
	if (!classifier.isEmpty())
		file += "-" + classifier;
	file += "." + ext;

	return group.replace('.', '/') + "/" + artifact + "/" + version + "/" + file;
}

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

//TODO: use ArchiveReader(Zip/Jar one)

// ── Portable ZIP reader (без private Qt API) ────────────────────────────────
// JAR — обычный ZIP. Читаем Central Directory в конце файла.

struct ZipEntry { QString name; quint32 localOffset; };

static QVector<ZipEntry> zipCentralDir(QFile& f)
{
    QVector<ZipEntry> entries;
    const qint64 sz = f.size();
    if (sz < 22) return entries;

    // Ищем EOCD сигнатуру 0x06054b50 с конца файла.
    const qint64 searchLen = qMin((qint64)65557, sz);
    f.seek(sz - searchLen);
    const QByteArray tail = f.read(searchLen);

    int eocd = -1;
    for (int i = (int)tail.size() - 22; i >= 0; --i)
    {
        if ((quint8)tail[i]==0x50 && (quint8)tail[i+1]==0x4b &&
            (quint8)tail[i+2]==0x05 && (quint8)tail[i+3]==0x06)
        { eocd = i; break; }
    }
    if (eocd < 0) return entries;

    const quint8* e = (const quint8*)tail.constData() + eocd;
    quint16 num   = e[8]  | (e[9]  << 8);
    quint32 cdOff = e[16] | (e[17]<<8) | (e[18]<<16) | (e[19]<<24);

    if (!f.seek(cdOff)) return entries;

    for (int i = 0; i < num; ++i)
    {
        QByteArray hdr = f.read(46);
        if (hdr.size() < 46) break;
        const quint8* h = (const quint8*)hdr.constData();
        if (h[0]!=0x50||h[1]!=0x4b||h[2]!=0x01||h[3]!=0x02) break;

        quint16 nl  = h[28]|(h[29]<<8);
        quint16 el  = h[30]|(h[31]<<8);
        quint16 cl  = h[32]|(h[33]<<8);
        quint32 off = h[42]|(h[43]<<8)|(h[44]<<16)|(h[45]<<24);

        ZipEntry ze;
        ze.name        = QString::fromUtf8(f.read(nl));
        ze.localOffset = off;
        f.skip(el + cl);
        entries.append(ze);
    }
    return entries;
}

// Читает данные одного файла из local-заголовка (stored или deflate).
static QByteArray zipReadEntry(QFile& f, quint32 localOff)
{
    if (!f.seek(localOff)) return {};
    QByteArray lh = f.read(30);
    if (lh.size() < 30) return {};
    const quint8* h = (const quint8*)lh.constData();
    if (h[0]!=0x50||h[1]!=0x4b||h[2]!=0x03||h[3]!=0x04) return {};

    quint16 method   = h[8] |(h[9] <<8);
    quint32 compSz   = h[18]|(h[19]<<8)|(h[20]<<16)|(h[21]<<24);
    quint32 uncompSz = h[22]|(h[23]<<8)|(h[24]<<16)|(h[25]<<24);
    quint16 nl       = h[26]|(h[27]<<8);
    quint16 el       = h[28]|(h[29]<<8);
    f.skip(nl + el);

    QByteArray data = f.read(compSz);

    if (method == 0)          // stored
        return data;

    if (method == 8)          // deflate → оборачиваем в zlib-обёртку для qUncompress
    {
        // qUncompress ждёт 4 байта big-endian несжатого размера + zlib-поток
        QByteArray zlibStream;
        zlibStream.resize(4);
        quint8* sz4 = (quint8*)zlibStream.data();
        sz4[0] = (uncompSz>>24)&0xff; sz4[1] = (uncompSz>>16)&0xff;
        sz4[2] = (uncompSz>> 8)&0xff; sz4[3] =  uncompSz     &0xff;
        // zlib-заголовок (CMF=0x78 FLG=0x9C) + raw deflate + adler32 заглушка
        zlibStream += (char)0x78; zlibStream += (char)0x9C;
        zlibStream += data;
        zlibStream += QByteArray(4, '\x00'); // adler32 (игнорируется qUncompress)
        QByteArray out = qUncompress(zlibStream);
        return out;
    }

    qWarning() << "Unsupported ZIP compression method:" << method;
    return {};
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
        qDebug() << "Checking native:" << jarPath;
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

        QFile jarFile(jarPath);
        if (!jarFile.open(QIODevice::ReadOnly))
        {
            qWarning() << "Cannot open native jar:" << jarPath;
            continue;
        }

        const QVector<ZipEntry> zipEntries = zipCentralDir(jarFile);
        for (const ZipEntry& ze : zipEntries)
        {
            if (ze.name.endsWith('/'))
                continue; // каталог

            bool excluded = false;
            for (const QString& ex : excludes)
                if (ze.name.startsWith(ex)) { excluded = true; break; }
            if (excluded)
                continue;

            // Сохраняем в плоскую папку natives (только имя файла, без пути).
            QString outName = QFileInfo(ze.name).fileName();
            if (outName.isEmpty())
                continue;

            QString outPath = nativesDir + "/" + outName;
            if (QFileInfo::exists(outPath))
                continue; // уже извлечено

            QByteArray data = zipReadEntry(jarFile, ze.localOffset);
            qDebug() << "Extract:" << ze.name << "from" << jarPath;
            if (data.isEmpty())
            {
                qWarning() << "Failed to decompress native:" << ze.name;
                continue;
            }

            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly))
                out.write(data);
            else
                qWarning() << "Cannot write native:" << outPath;
        }
        jarFile.close();
    }
}

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

	crashLog.clear();

	// Сохраняем параметры запуска в лог сразу — полезно при диагностике
	crashLog += "=== Параметры запуска ===\n";
	crashLog += "Java:    " + javaPath + "\n";
	crashLog += "MC:      " + version  + "\n";
	crashLog += "GameDir: " + gameDir  + "\n";
	crashLog += "JVM:     " + jvmArgs.join(" ") + "\n";
	crashLog += "Args:    " + gameArgs.join(" ") + "\n\n";
	crashLog += "=========================\n";


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
			this, &MinecraftLauncher::MinecraftFinished);

	minecraftProcess->start(javaPath, allArgs);

	if (!minecraftProcess->waitForStarted(5000))
	{
		crashLog += "\n[ОШИБКА] Процесс не запустился (waitForStarted timeout)\n";
		emit onMineCraftCrash(neededJava, javaPath);
		delete minecraftProcess;
		minecraftProcess = nullptr;
		return;
	}

}


void MinecraftLauncher::MinecraftFinished(int exitCode, QProcess::ExitStatus st)
{
	// Дочитываем остатки вывода
	if (minecraftProcess)
	{
		crashLog += minecraftProcess->readAllStandardOutput();
		crashLog += minecraftProcess->readAllStandardError();
		minecraftProcess->deleteLater();
		minecraftProcess = nullptr;
	}

	emit onMinecraftFinished(exitCode, st);

	if (exitCode != 0)
		emit onMineCraftCrash(0, "");
}

QString MinecraftLauncher::getCrashHint(int neededJava, QString javaPath) const {
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
	return hintText;
}

void MinecraftLauncher::ensureJava(int requiredMajor,
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

    downloader->jd.downloadJavaRuntime(javaComponent, runtimeDir);
}

void MinecraftLauncher::launchGame(const QJsonObject& root,
                            const QString& gameDir,
                            const QString& version,
                            const QString& versionDir,
                            const QString& mainClass,
                            const QString& javaPath,
                            const QString& username,
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
