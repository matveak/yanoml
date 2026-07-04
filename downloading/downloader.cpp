//
// Created by ghhg6 on 04.07.2026.
//

#include "downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QSaveFile>
#include <QObject>

Downloader::Downloader() {
	manager.setTransferTimeout(3000);
}

void Downloader::startNextDownload()
{
	while (activeDownloads < MaxParallelDownloads &&
		   !downloadQueue.isEmpty())
	{
		DownloadTask task = downloadQueue.dequeue();
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

	downloadQueue.enqueue({url, outputPath});
	startNextDownload();
}

void Downloader::startDownload(const DownloadTask& task)
{
	QDir().mkpath(QFileInfo(task.outputPath).path());

	QNetworkReply* reply =
		manager.get(QNetworkRequest(task.url));

	QSaveFile* file = new QSaveFile(task.outputPath);

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

	++activeDownloads;

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

				--activeDownloads;

				startNextDownload();
			});
}