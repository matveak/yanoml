#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

#include "../minecraftdownloader.h"

class MinecraftDownloader;
class WindowFrame;
class QLineEdit;
class QComboBox;
class QPushButton;
class SettingsWindow;

class CreateModpackWindow : public QDialog
{
    Q_OBJECT

public:
    explicit CreateModpackWindow(QWidget* parent = nullptr);

    void setDownloader(MinecraftDownloader* d);
    void setSettingsWindow(SettingsWindow* settings);

private slots:
    void onCreate();

    void onVersionsLoaded(
        const QVector<MinecraftVersion>& versions);

    void onFabricVersions(
        const QJsonArray& versions);

    void onForgeVersions(
        const QJsonObject& json);

    void onNeoForgeVersions(
        const QString& xml);

    void loadLoaderVersions();

private:
    WindowFrame         *m_frame           = nullptr;
    MinecraftDownloader *m_downloader       = nullptr;
    SettingsWindow      *m_settingsWindow   = nullptr;
    QLineEdit           *m_nameEdit         = nullptr;
    QComboBox           *m_versionBox       = nullptr;
    QComboBox           *m_loaderBox        = nullptr;
    QComboBox           *m_loaderVersionBox = nullptr;
    QPushButton         *m_createButton     = nullptr;
};