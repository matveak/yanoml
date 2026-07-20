//
// Created by ghhg6 on 05.07.2026.
//

#ifndef MINECRAFTLAUNCHER_H
#define MINECRAFTLAUNCHER_H
#include <QMap>
#include <QProcess>

#include "minecraftdownloader.h"

class MinecraftLauncher : public QObject {
	Q_OBJECT

public:
	void startMinecraftProcess(const QString& javaPath, const QStringList& jvmArgs, const QStringList& gameArgs, const QString& gameDir, const QString& version, int neededJava);

	void MinecraftFinished(int exitCode, QProcess::ExitStatus);

	QString getCrashHint(int neededJava, const QString& javaPath) const;

	void ensureJava(const QString &mcVersion, const QString &gameDir,
	                std::function<void(QString)> cb, const QString& javaPath);

	void launchGame(const QJsonObject &root, const QString &gameDir, const QString &version, const QString &versionDir,
	                const QString &mainClass, const QString &javaPath, QString username, int ram,
	                int neededJava);

	void launchModded(const QJsonObject &parentRoot, const QJsonObject &childRoot, const QString &gameDir,
	                  const QString &mcVersion, const QString &versionId, const QString &javaPath, QString username,
	                  int ram, int neededJava);

	QString m_crashLog;
	signals:
	void onMinecraftFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void onMineCraftCrash(int neededJava, const QString& javaPath);
private:
	QProcess *m_minecraftProcess = nullptr;
	MinecraftDownloader *m_downloader = nullptr;
	QMap<int, QString>   m_installedJavas;
};

#endif //MINECRAFTLAUNCHER_H
