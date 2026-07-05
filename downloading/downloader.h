//
// Created by ghhg6 on 04.07.2026.
//
#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QNetworkAccessManager>
#include <QUrl>
#include <QString>
#include <QQueue>


class Downloader : public QObject {
	Q_OBJECT
protected:
	struct DownloadTask
	{
		QUrl url;
		QString outputPath;
	};

	QQueue<DownloadTask> downloadQueue;
	int activeDownloads = 0;


	static constexpr int MaxParallelDownloads = 6;

public:
	//TODO: protected
	QNetworkAccessManager *manager;

	explicit Downloader(QNetworkAccessManager *manager, QObject *parent = nullptr);

	Downloader(Downloader &other);

	void startNextDownload();

	void downloadFile(const QUrl &url, const QString &outputPath);

	void startDownload(const DownloadTask& task);
	void downloadLibrariesFromVersionJson(const QString& versionJsonPath, const QString& gameDir, std::function<void()> onFinished);

signals:
	void fileDownloaded(const QString& filePath);
	void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void errorOccurred(const QString& errorString);
};



#endif //DOWNLOADER_H
