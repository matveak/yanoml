//
// Created by ghhg6 on 04.07.2026.
//

#include "minecraftdownloader.h"

#include <QNetworkReply>
#include <QSaveFile>

void MinecraftDownloader::downloadAssetObject(const QUrl& url, const QString& outputPath, const QString& expectedHash, const int* downloaded, int total, const QString& instancePath, int attempt) {
    QNetworkReply *reply = d.m_manager->get(QNetworkRequest(url));
    auto file = new QSaveFile(outputPath);
    auto sha1 = new QCryptographicHash(QCryptographicHash::Sha1);

    if (!file->open(QIODevice::WriteOnly)) {
        delete sha1;
        file->deleteLater();
        reply->deleteLater();
        emit errorOccurred("Не удалось открыть файл для записи: " + outputPath);
        markAssetDone(downloaded, total, instancePath);
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file, sha1] {
        QByteArray chunk = reply->readAll();
        file->write(chunk);
        sha1->addData(chunk);
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, sha1, url, outputPath, expectedHash,
                downloaded, total, instancePath, attempt] {
                QByteArray tail = reply->readAll();
                if (!tail.isEmpty()) {
                    file->write(tail);
                    sha1->addData(tail);
                }

                bool netOk = (reply->error() == QNetworkReply::NoError);
                QString gotHash = QString::fromLatin1(sha1->result().toHex());
                bool hashOk = expectedHash.isEmpty() || (gotHash == expectedHash);

                delete sha1;
                if (!netOk) {
                    qDebug() << reply->error() << reply->errorString();
                }

                if (netOk && !hashOk) {
                    qDebug() << "SHA1 mismatch";
                }
                reply->deleteLater();

                if (netOk && hashOk) {
                    file->commit();
                    file->deleteLater();
                    markAssetDone(downloaded, total, instancePath);
                    return;
                }


                // Не оставляем битый файл на диске.
                file->cancelWriting();
                file->deleteLater();

                if (attempt < 10) {
                    QTimer::singleShot(1000 * (attempt + 1), this,
                                       [=] {
                                           qDebug() << "Ошибка при установки файла \"" + outputPath + "\". Попытка " +
                                                   QString::fromStdString(std::to_string(attempt)) + "/10";
                                           downloadAssetObject(url, outputPath, expectedHash, downloaded, total,
                                                               instancePath, attempt + 1);
                                       });
                    return;
                }

                emit errorOccurred("Не удалось скачать ассет (битый файл): " + outputPath);
                markAssetDone(downloaded, total, instancePath);
            });
}

void MinecraftDownloader::markAssetDone(int* downloaded, int total,
                                        const QString& instancePath)
{
    ++(*downloaded);
    emit totalProgress(*downloaded * 100 / total);

    if (*downloaded >= total)
    {
        emit instanceCreated(instancePath);
        delete downloaded;
    }
}

void MinecraftDownloader::downloadVanillaVersion(
    const QString& versionJsonUrl,
    const QString& outputJar)
{
    QNetworkReply* reply = d.m_manager->get(QNetworkRequest(QUrl(versionJsonUrl)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, outputJar]
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    qDebug() << reply->errorString();
                    return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                QString clientUrl = doc.object()["downloads"]
                                        .toObject()["client"]
                                        .toObject()["url"]
                                        .toString();

                if (!clientUrl.isEmpty())
                    d.downloadFile(QUrl(clientUrl), outputJar);
                else
                    emit errorOccurred("Не удалось найти ссылку на клиент");
            });
}