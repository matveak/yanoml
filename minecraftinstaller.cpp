#include "minecraftinstaller.h"

#include "downloading/minecraftdownloader.h"
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <QProcess>
#include <QSaveFile>

MinecraftInstaller::MinecraftInstaller(QObject *parent) : m_manager(new QNetworkAccessManager(parent)),
                                                          m_manifestDownloader(m_manager),
                                                          m_minecraftDownloader(m_manager),
                                                          m_javaDownloader(m_manager),
                                                          m_loaderInstaller(m_manager){}
