
#include <QCheckBox>
#include "ui_MainWindow.h"
#include "modwindow.h"
#include "MinecraftDownloader.h"
#include <QMainWindow>
#include "moddetailwindow.h"
#include "settingswindow.h"
//#include <QOAuth2AuthorizationCodeFlow>
//#include <QOAuthHttpServerReplyHandler>
#include <QNetworkAccessManager>
#include <QDesktopServices>
#include <QUrl>
#include <QProgressBar>
#define MAINWINDOW_H
#include <QCheckBox>
#include "ui_MainWindow.h"
#include "modwindow.h"
#include "MinecraftDownloader.h"
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QProcess>

class MainWindow : public QMainWindow, private Ui::MainWindow {
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

    void onMinecraftFinished(int exitCode, QProcess::ExitStatus exitStatus); // ← Новый слот

private:
    QProgressBar* progressBar = nullptr;
    QSystemTrayIcon* trayIcon = nullptr;        // ← Новый
    QProcess* minecraftProcess = nullptr;       // ← Новый

    SettingsWindow* settingsWindow = nullptr;
    MinecraftDownloader* downloader = nullptr;
    QComboBox* LoaderBox = nullptr;

    void setupConnections();
    void setupTrayIcon();                       // ← Новый метод
    void loadVersions();
};
