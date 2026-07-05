#pragma once

#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QDesktopServices>
#include <QMap>
#include <QTimer>
#include <QTextEdit>

#include "ui_MainWindow.h"
#include "../minecraftdownloader.h"
#include "settingswindow.h"
#include "../MinecraftLauncher.h"

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
public slots:
    //void onMinecraftFinished(int exitCode, QProcess::ExitStatus exitStatus);
private slots:
    void onLoaderChanged(const QString& loader);
    void onShowSnapshotsChanged(int state);

    void onVanillaVersionsReceived(const QVector<MinecraftVersion>& versions);
    void onFabricVersionsReceived(const QJsonArray& versions);
    void onForgeVersionsReceived(const QJsonObject& versions);
    void onNeoForgeVersionReceived(const QString& xml);

    void on_PlayButton_clicked();
    void on_ModPlatformButton_clicked();
    void on_CurseForgeButton_clicked();   // ← новый
    void on_ModpackButton_clicked();
    void on_UpdateButton_clicked();
    void on_ElyByButton_clicked();
    void on_SettingsButton_clicked();
    void on_PickAccountButton_clicked();
    void on_InstallerButton_clicked();



private:
    QProgressBar*        progressBar      = nullptr;
    QSystemTrayIcon*     trayIcon         = nullptr;
    SettingsWindow*      settingsWindow   = nullptr;
    MinecraftDownloader* downloader       = nullptr;
    MinecraftLauncher*   launcher         = nullptr;
    QComboBox*           LoaderBox        = nullptr;
    bool                 m_modLoaderPending = false;

    void setupConnections();
    void setupTrayIcon();
    void loadVersions();
    void showCrashDialog(int neededJava, const QString& javaPath);

    void startLoaderInstall(const QString& loader,
                            const QString& mcVersion,
                            const QString& gameDir);
};
