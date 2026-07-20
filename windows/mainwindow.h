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
    QWidget      *centralwidget;
    QPushButton  *PlayButton;
    QPushButton  *PlatformButton;    // слитая кнопка выбора источника модов (Modrinth / CurseForge)
    QPushButton  *UpdateButton;
    QPushButton  *SolitaireGameButton; //
    QPushButton  *ElyByButton;
    QComboBox    *UpdateBox;         // используется как LoaderBox
    QPushButton  *SettingsButton;
    QPushButton  *PickAccountButton;
    QPushButton  *InstallerButton;
    QPushButton  *ModpackButton;
    QComboBox    *VersionBox;
    QMenuBar     *menubar;
    QMenu        *menulauncher;
    QStatusBar   *statusbar;
    QVBoxLayout  *rightPanelLayout;

    WindowFrame* frame = nullptr;
    QProgressBar*        progressBar      = nullptr;
    QSystemTrayIcon*     trayIcon         = nullptr;
    SettingsWindow*      settingsWindow   = nullptr;
    MinecraftDownloader* downloader       = nullptr;
    MinecraftLauncher*   launcher         = nullptr;
    QComboBox*           LoaderBox        = nullptr;
    bool               m_modLoaderPending = false;
    void setupUI();
    void setupConnections();
    void setupTrayIcon();
    void loadVersions();
    void showCrashDialog(int neededJava, const QString& javaPath);

    void startLoaderInstall(const QString& loader,
                            const QString& mcVersion,
                            const QString& gameDir);
};
