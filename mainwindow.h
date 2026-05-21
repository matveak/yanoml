#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QCheckBox>
#include "ui_MainWindow.h"
#include "modwindow.h"
#include "MinecraftDownloader.h"
#include <QMainWindow>
#include "moddetailswindow.h"
#include "settingswindow.h"
class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoaderChanged(const QString& loader);
    void onShowSnapshotsChanged(int state);

    void onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions);
    void onFabricVersionsReceived(const QJsonArray& versions);
    void onForgeVersionsReceived(const QJsonObject& versions);
    void onNeoForgeVersionReceived(const QString& xml);

    void on_PlayButton_clicked();
    void on_ModPlatformButton_clicked();
    void on_UpdateButton_clicked();
    void on_ElyByButton_clicked();
    void on_SettingsButton_clicked();
    void on_PickAccountButton_clicked();
    void on_InstallerButton_clicked();
private:
    SettingsWindow* settingsWindow = nullptr;
    QComboBox* LoaderBox = nullptr;

    MinecraftDownloader* downloader = nullptr;

    void setupConnections();
    void loadVersions();                    // Загрузка версий при старте
};

#endif // MAINWINDOW_H
