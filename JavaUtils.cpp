//
// Created by ghhg6 on 05.07.2026.
//

#include "JavaUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QVersionNumber>

#include "Zip.h"


QString mavenNameToPath(const QString& name)
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

bool nativeLibraryAllowedOnCurrentOS(const QJsonObject& lib)
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
void extractNativesForVersion(const QJsonObject& root,
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
        for (const auto&[name, localOffset] : zipEntries)
        {
            if (name.endsWith('/'))
                continue; // каталог

            bool excluded = false;
            for (const QString& ex : excludes)
                if (name.startsWith(ex)) { excluded = true; break; }
            if (excluded)
                continue;

            // Сохраняем в плоскую папку natives (только имя файла, без пути).
            QString outName = QFileInfo(name).fileName();
            if (outName.isEmpty())
                continue;

            QString outPath = nativesDir + "/" + outName;
            if (QFileInfo::exists(outPath))
                continue; // уже извлечено

            QByteArray data = zipReadEntry(jarFile, localOffset);
            qDebug() << "Extract:" << name << "from" << jarPath;
            if (data.isEmpty())
            {
                qWarning() << "Failed to decompress native:" << name;
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
int requiredJavaMajor(const QString& mcVersion)
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
int getJavaMajorVersion(const QString& javaExe)
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

QMap<int, QString> findInstalledJavas()
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

bool javaIsCompatible(int installedMajor, int requiredMajor)
{
	if (installedMajor <= 0)
		return false;
	if (installedMajor == requiredMajor)
		return true;
	if (requiredMajor >= 17)
		return installedMajor >= requiredMajor;
	return false;
}

bool argRulesAllow(const QJsonObject& argObj)
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
QString mavenKey(const QString& name)
{
	const QStringList parts = name.split(':');
	if (parts.size() < 2)
		return QString();
	return parts[0] + ":" + parts[1];
}

// Ищет среди установленных Java ту, что совместима с требуемой major-версией.
// Возвращает пустую строку, если подходящей нет (тогда качаем Java от Mojang).
QString pickCompatibleInstalledJava(const QMap<int, QString>& javas,
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
QString javaComponentForMajor(int requiredMajor)
{
	if (requiredMajor <= 8)  return "jre-legacy";
	if (requiredMajor <= 16) return "java-runtime-alpha";
	if (requiredMajor <= 17) return "java-runtime-gamma";
	return "java-runtime-delta";
}
