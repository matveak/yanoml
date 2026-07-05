//
// Created by ghhg6 on 04.07.2026.
//

#include "manifestdownloader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>



ManifestDownloader::ManifestDownloader(QNetworkAccessManager *manager, QObject* parent) : Downloader(manager, parent) {

}

ManifestDownloader::ManifestDownloader(Downloader &d) : Downloader(d) {}

void ManifestDownloader::fetchVanillaVersions()
{
	qDebug() << "Requesting versions...";

	QUrl url("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
	QNetworkReply* reply = manager->get(QNetworkRequest(url));

	connect(reply, &QNetworkReply::finished, this, [this, reply]
			{
				reply->deleteLater();

				if (reply->error() != QNetworkReply::NoError)
				{
					emit errorOccurred(reply->errorString());
					return;
				}

				handleVanillaManifest(reply);
			});
}

void ManifestDownloader::fetchFabricVersions()
{
    qDebug() << "Request Fabric versions";

    QNetworkReply* reply = manager->get(
        QNetworkRequest(QUrl("https://meta.fabricmc.net/v2/versions/game")));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                QByteArray data = reply->readAll();
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(data);

                if (!doc.isArray())
                {
                    emit errorOccurred("Fabric API returned invalid JSON");
                    return;
                }

                emit fabricVersionsReceived(doc.array());
            });
}

// ==================== FORGE ====================

void ManifestDownloader::fetchForgeVersions()
{
    qDebug() << "Request Forge versions";

    QUrl url("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
    QNetworkReply* reply = manager->get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                qDebug() << "Forge finished" << reply->errorString();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                emit forgeVersionsReceived(
                    QJsonDocument::fromJson(reply->readAll()).object());
            });
}

// ==================== NEOFORGE ====================

void ManifestDownloader::fetchNeoForgeVersions()
{
    qDebug() << "Request NeoForge versions";

    QUrl url("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    QNetworkReply* reply = manager->get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();

                qDebug() << "NeoForge finished" << reply->errorString();

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit errorOccurred(reply->errorString());
                    return;
                }

                emit neoforgeVersionReceived(QString(reply->readAll()));
            });
}


void ManifestDownloader::handleVanillaManifest(QNetworkReply* reply)
{
	QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

	if (!doc.isObject())
	{
		emit errorOccurred("Некорректный ответ Mojang");
		return;
	}

	QJsonArray versionsArray = doc.object()["versions"].toArray();

	QVector<MinecraftVersion> versions;

	for (const auto& value : versionsArray)
	{
		QJsonObject obj = value.toObject();

		MinecraftVersion ver;
		ver.gameVersion  = obj["id"].toString();
		ver.loaderVersion = "";
		ver.loaderType   = obj["type"].toString();
		ver.url          = obj["url"].toString();
		ver.releaseTime  = obj["releaseTime"].toString();

		versions.push_back(ver);
	}

	qDebug() << "Loaded versions:" << versions.size();

	emit vanillaVersionsReceived(versions);
}
