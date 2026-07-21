#pragma once

#include <QDialog>
#include <QHash>
#include <QSaveFile>

#include "../curseforgeclient.h"
#include "../modsapi.h"

// Forward declarations
class WindowFrame;
class SettingsWindow;
class QNetworkAccessManager;
class QNetworkReply;

class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QComboBox;
class QProgressBar;
class QTabWidget;
class QFrame;
class QLayout;


class ModsBrowserWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModsBrowserWindow(QWidget* parent = nullptr);

private slots:
    void onSearch();
    void onTabChanged(int index);
    void onModsReceived(const QVector<ModInfo>& mods);
    void onModpacksReceived(const QVector<ModInfo>& packs);
    void onError(const QString& error);

private:
    void setupConnections();
    void buildHeader(QVBoxLayout* layout);
    void buildProgressSection(QVBoxLayout* layout);
    void buildTabs(QVBoxLayout* layout);
    void buildModsTab();
    void buildModpacksTab();

    QScrollArea *createScrollArea();
    QFrame      *createModCard(const ModInfo& mod, bool isModpack);
    QLabel      *createIconLabel(const QString& iconUrl);
    QLabel      *createVersionsLabel(const QStringList& versions);
    QPushButton *createInstallButton(const ModInfo& mod, bool isModpack);
    QPushButton *createWebsiteButton(const QString& url);
    QVBoxLayout *createInfoLayout(const ModInfo& mod);
    QVBoxLayout *createActionsLayout(const ModInfo& mod, bool isModpack);

    void displayMods(const QVector<ModInfo>& mods, QVBoxLayout* layout, QHash<int, ModInfo>& store, bool isModpack);
    void clearLayout(QVBoxLayout* layout);
    void clearNestedLayout(QLayout* layout);

    void buildAPITabs();
    ModsAPI *currentAPI() const;

    void downloadIconAsync(const QString& url, QLabel* target);
    void downloadFile(const QUrl& url, const QString& fileName, bool isModpack);
    void handleDownloadFinished(QNetworkReply* reply, QSaveFile* saveFile, const QString& fileName, const QString& savePath);
    void installItem(const ModInfo& mod, bool isModpack);

    void handleFilesReceived(const ModInfo& mod, const QVector<FileInfo>& files, bool isModpack);


    ModsAPI               *m_apis[2]     = {new CurseForgeClient};
    WindowFrame           *m_frame       = nullptr;
    QNetworkAccessManager *m_nam         = nullptr;
    QTabWidget            *m_tabs        = nullptr;
    QLineEdit             *m_modSearch   = nullptr;
    QComboBox             *m_modVersion  = nullptr;
    QComboBox             *m_modLoader   = nullptr;
    QVBoxLayout           *m_modCards    = nullptr;
    QLineEdit             *m_packSearch  = nullptr;
    QComboBox             *m_packVersion = nullptr;
    QVBoxLayout           *m_packCards   = nullptr;
    QProgressBar          *m_progress    = nullptr;
    QLabel                *m_status      = nullptr;
    QTabWidget            *m_apiTabs;
    QHash<int, ModInfo>      m_modStore;
    QHash<int, ModInfo>      m_packStore;
};