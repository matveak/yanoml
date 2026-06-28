#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QDesktopServices>
#include <QUrl>
#include <QMap>
#include <QTimer>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDialog>
#include <QTextEdit>
#include <QJsonObject>
#include <functional>

#include "ui_MainWindow.h"
#include "modwindow.h"
#include "MinecraftDownloader.h"
#include "moddetailwindow.h"
#include "settingswindow.h"
#include "createmodpackwindow.h"
#include "curseforgewindow.h"

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

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

    void onMinecraftFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProgressBar*        progressBar      = nullptr;
    QSystemTrayIcon*     trayIcon         = nullptr;
    QProcess*            minecraftProcess = nullptr;

    SettingsWindow*      settingsWindow   = nullptr;
    MinecraftDownloader* downloader       = nullptr;
    QComboBox*           LoaderBox        = nullptr;

    QMap<int, QString>   installedJavas;
    QString              crashLog;
    bool                 m_modLoaderPending = false;

    void setupConnections();
    void setupTrayIcon();
    void loadVersions();
    void showCrashDialog(int neededJava, const QString& javaPath);

    void launchGame(const QJsonObject& root,
                    const QString& gameDir,
                    const QString& version,
                    const QString& versionDir,
                    const QString& mainClass,
                    const QString& javaPath,
                    int neededJava);

    void launchModded(const QJsonObject& parentRoot,
                      const QJsonObject& childRoot,
                      const QString& gameDir,
                      const QString& mcVersion,
                      const QString& versionId,
                      const QString& javaPath,
                      int neededJava);

    void startMinecraftProcess(const QString& javaPath,
                               const QStringList& jvmArgs,
                               const QStringList& gameArgs,
                               const QString& gameDir,
                               const QString& version,
                               int neededJava);

    void ensureJava(int requiredMajor,
                    const QString& mcVersion,
                    const QString& gameDir,
                    std::function<void(QString)> cb);

    void startLoaderInstall(const QString& loader,
                            const QString& mcVersion,
                            const QString& gameDir);
};