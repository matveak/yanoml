#pragma once
#define MAINWINDOW_H

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

#include "ui_MainWindow.h"
#include "modwindow.h"
#include "MinecraftDownloader.h"
#include "moddetailwindow.h"
#include "settingswindow.h"

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

    // Кэш найденных Java: major версия -> путь к исполняемому файлу
    QMap<int, QString>   installedJavas;

    // Накопленный вывод процесса Minecraft для показа при крэше
    QString              crashLog;

    void setupConnections();
    void setupTrayIcon();
    void loadVersions();
    void showCrashDialog(int neededJava, const QString& javaPath);
};