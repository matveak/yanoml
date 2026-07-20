//
// Created by ghhg6 on 04.07.2026.
//

#include "downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QObject>


static bool libraryAllowedOnCurrentOS(const QJsonObject& lib)
{
	QJsonArray rules = lib["rules"].toArray();
	if (rules.isEmpty())
		return true;

	bool allowed = false;

	for (const auto& ruleVal : rules)
	{
		QJsonObject rule = ruleVal.toObject();
		QString action  = rule["action"].toString();

		if (rule.contains("os"))
		{
			QString osName = rule["os"].toObject()["name"].toString();

#ifdef Q_OS_WIN
			QString currentOS = "windows";
#elif defined(Q_OS_MAC)
			QString currentOS = "osx";
#else
			QString currentOS = "linux";
#endif
			if (osName == currentOS)
				allowed = (action == "allow");
		}
		else
		{
			allowed = (action == "allow");
		}
	}

	return allowed;
}


Downloader::Downloader(QNetworkAccessManager *manager, QObject *parent) {
	this->m_manager = manager;
	manager->setTransferTimeout(3000);
}

Downloader::Downloader(const Downloader &other) {
	this->m_manager = other.m_manager;
}


void Downloader::startNextDownload()
{
	while (m_activeDownloads < MaxParallelDownloads &&
		   !m_downloadQueue.isEmpty())
	{
		DownloadTask task = m_downloadQueue.dequeue();
		startDownload(task);
	}
}

void Downloader::downloadFile(const QUrl& url,
									   const QString& outputPath)
{
	if (QFileInfo::exists(outputPath))
	{
		emit fileDownloaded(outputPath);
		return;
	}

	m_downloadQueue.enqueue({.url = url, .outputPath = outputPath});
	startNextDownload();
}

void Downloader::startDownload(const DownloadTask& task)
{
	QDir().mkpath(QFileInfo(task.outputPath).path());

	QNetworkReply* reply =
		m_manager->get(QNetworkRequest(task.url));

	auto* file = new QSaveFile(task.outputPath);

	if (!file->open(QIODevice::WriteOnly))
	{
		emit errorOccurred(
			"Не удалось открыть файл для записи: " +
			task.outputPath);

		file->deleteLater();
		reply->deleteLater();

		startNextDownload();
		return;
	}

	++m_activeDownloads;

	connect(reply,
			&QNetworkReply::readyRead,
			[reply, file]
			{
				file->write(reply->readAll());
			});

	connect(reply,
			&QNetworkReply::downloadProgress,
			this,
			&Downloader::downloadProgress);

	connect(reply,
			&QNetworkReply::finished,
			[this, reply, file, task]
			{
				QByteArray tail = reply->readAll();
				if (!tail.isEmpty())
					file->write(tail);

				if (reply->error() != QNetworkReply::NoError)
				{
					file->cancelWriting();
					emit errorOccurred(reply->errorString());
				}
				else
				{
					file->commit();
					emit fileDownloaded(task.outputPath);
				}

				file->deleteLater();
				reply->deleteLater();

				--m_activeDownloads;

				startNextDownload();
			});
}

void Downloader::downloadLibrariesFromVersionJson(
    const QString& versionJsonPath,
    const QString& gameDir,
    std::function<void()> onFinished) {
	QFile file(versionJsonPath);

	if (!file.open(QIODevice::ReadOnly))
	{
		emit errorOccurred("Не удалось открыть " + versionJsonPath);
		return;
	}

	const QJsonObject root =
		QJsonDocument::fromJson(file.readAll()).object();

	file.close();

	struct Item
	{
		QUrl url;
		QString path;
	};

	QVector<Item> items;

	const QString librariesDir = gameDir + "/libraries";

	const QJsonArray libraries = root["libraries"].toArray();

	for (const auto& value : libraries)
	{
		const QJsonObject lib = value.toObject();

		if (!libraryAllowedOnCurrentOS(lib))
			continue;

		const QJsonObject downloads = lib["downloads"].toObject();

		//
		// обычная библиотека
		//
		if (downloads.contains("artifact"))
		{
			const QJsonObject artifact =
				downloads["artifact"].toObject();

			const QString url  = artifact["url"].toString();
			const QString path = artifact["path"].toString();

			if (!url.isEmpty() && !path.isEmpty())
			{
				items.push_back({
					.url = QUrl(url),
					.path = librariesDir + "/" + path
				});
			}
		}

		//
		// natives
		//
		if (downloads.contains("classifiers"))
		{
			const QJsonObject classifiers =
				downloads["classifiers"].toObject();

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

			if (!nativeKey.isEmpty())
			{
				const QJsonObject native =
					classifiers[nativeKey].toObject();

				const QString url  = native["url"].toString();
				const QString path = native["path"].toString();

				if (!url.isEmpty() && !path.isEmpty())
				{
					items.push_back({
						.url = QUrl(url),
						.path = librariesDir + "/" + path
					});
				}
			}
		}
	}

	if (items.isEmpty())
	{
		if (onFinished)
			onFinished();
		return;
	}

	auto remaining = std::make_shared<int>(items.size());

	qDebug() << "amount of items to be installed:" << items.size();

	for (const Item& item : items)
	{
		if (QFileInfo::exists(item.path))
		{
			qDebug() << "file" << item.path << "already exists";
			if (--(*remaining) == 0 && onFinished) {
				onFinished();
			}
			continue;
		}

		connect(this,
				&Downloader::fileDownloaded,
				this,
				[this, remaining, item, onFinished](const QString& path)
				{
					if (path != item.path)
						return;

					disconnect(this, nullptr, this, nullptr);

					if (--(*remaining) == 0 && onFinished) {
						qDebug() << "file" << item.path << "downloaded";
						onFinished();
					}
				},
				Qt::SingleShotConnection);
		qDebug() << "downloading file " << item.path;
		downloadFile(item.url, item.path);
	}
}