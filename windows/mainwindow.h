#pragma once

#include <QMainWindow>
#include  <QStatusBar>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QDesktopServices>
#include <QMenuBar>
#include <QBoxLayout>
#include "../ui/theme.h"
#include "../ui/titlebar.h"
#include "../ui/windowframe.h"
#include <QMap>
#include <QTimer>
#include <QTextEdit>
#include  <QComboBox>
#include "../minecraftdownloader.h"
#include "settingswindow.h"
#include "../MinecraftLauncher.h"

class MinecraftDownloader;

class MainWindow : public QMainWindow
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
    QWidget             *m_centralwidget{};
    QPushButton         *m_playButton{};
    QPushButton         *m_platformButton{};
    QPushButton         *m_updateButton{};
    QPushButton         *m_elyByButton{};
    QComboBox           *m_updateBox{};
    QPushButton         *m_settingsButton{};
    QPushButton         *m_pickAccountButton{};
    QPushButton         *m_installerButton{};
    QPushButton         *m_modpackButton{};
    QComboBox           *m_versionBox{};
    QMenuBar            *m_menubar{};
    QMenu               *m_menulauncher{};
    QStatusBar          *m_statusbar{};
    QVBoxLayout         *m_rightPanelLayout;
    WindowFrame         *m_frame            = nullptr;
    QProgressBar        *m_progressBar      = nullptr;
    QSystemTrayIcon     *m_trayIcon         = nullptr;
    SettingsWindow      *m_settingsWindow   = nullptr;
    MinecraftDownloader *m_downloader       = nullptr;
    MinecraftLauncher   *m_launcher         = nullptr;
    QComboBox           *m_LoaderBox        = nullptr;
    bool                 m_modLoaderPending = false;
    void setupUI();
    void setupConnections();
    void setupTrayIcon();
    void loadVersions();
    void showCrashDialog(int neededJava, const QString& javaPath);

    void startLoaderInstall(const QString& loader,
                            const QString& mcVersion,
                            const QString& gameDir);
};
