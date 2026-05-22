#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

#include "minecraftdownloader.h"

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
    MinecraftDownloader* downloader = nullptr;
    SettingsWindow* settingsWindow = nullptr;

    QLineEdit* nameEdit = nullptr;

    QComboBox* versionBox = nullptr;
    QComboBox* loaderBox = nullptr;
    QComboBox* loaderVersionBox = nullptr;

    QPushButton* createButton = nullptr;
};