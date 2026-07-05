//
// Created by ghhg6 on 04.07.2026.
//

#ifndef MANIFESTDOWNLOADER_H
#define MANIFESTDOWNLOADER_H
#include <QObject>
#include "downloader.h"


struct MinecraftVersion;

class ManifestDownloader : public Downloader
{
	Q_OBJECT

public:
	explicit ManifestDownloader(
		QNetworkAccessManager* manager,
		QObject* parent = nullptr);

	void fetchVanillaVersions();
	void fetchFabricVersions();
	void fetchForgeVersions();
	void fetchNeoForgeVersions();

signals:
	void vanillaVersionsReceived(const QVector<MinecraftVersion>&);
	void fabricVersionsReceived(const QJsonArray&);
	void forgeVersionsReceived(const QJsonObject&);
	void neoforgeVersionReceived(const QString&);

private:
	void handleVanillaManifest(QNetworkReply*);
	void handleFabricManifest(QNetworkReply*);
	void handleForgeManifest(QNetworkReply*);
	void handleNeoForgeManifest(QNetworkReply*);
};



#endif //MANIFESTDOWNLOADER_H
