#pragma once

#include <QDialog>
#include <QVector>
#include <QHash>
#include <QUrl>
#include <QSaveFile>
#include "../curseforgeclient.h"

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

class CurseForgeWindow : public QDialog
{
    Q_OBJECT

public:
    explicit CurseForgeWindow(QWidget* parent = nullptr);
    void setSettingsWindow(SettingsWindow* sw) { m_settings = sw; }

private slots:
    // Слоты — только те методы, что подключаются к сигналам Qt
    void onSearch();
    void onTabChanged(int index);
    void onModsReceived(const QVector<CFMod>& mods);
    void onModpacksReceived(const QVector<CFMod>& packs);
    void onError(const QString& error);

private:
    // === Инициализация UI ===
    void setupConnections();
    void buildHeader(QVBoxLayout* layout);
    void buildProgressSection(QVBoxLayout* layout);
    void buildTabs(QVBoxLayout* layout);
    void buildModsTab();
    void buildModpacksTab();

    // === Создание виджетов ===
    QScrollArea* createScrollArea();
    QFrame*      createModCard(const CFMod& mod, bool isModpack);
    QLabel*      createIconLabel(const QString& iconUrl);
    QLabel*      createVersionsLabel(const QStringList& versions);
    QPushButton* createInstallButton(const CFMod& mod, bool isModpack);
    QPushButton* createWebsiteButton(const QString& url);

    // === Создание layout-ов карточки ===
    QVBoxLayout* createInfoLayout(const CFMod& mod);
    QVBoxLayout* createActionsLayout(const CFMod& mod, bool isModpack);

    // === Отображение и очистка ===
    void displayMods(const QVector<CFMod>& mods,
                     QVBoxLayout* layout,
                     QHash<int, CFMod>& store,
                     bool isModpack);
    void clearLayout(QVBoxLayout* layout);
    void clearNestedLayout(QLayout* layout);

    // === Сеть и загрузка ===
    void downloadIconAsync(const QString& url, QLabel* target);
    void downloadFile(const QUrl& url, const QString& fileName, bool isModpack);
    void handleDownloadFinished(QNetworkReply* reply,
                                QSaveFile* saveFile,
                                const QString& fileName,
                                const QString& savePath);
    void installItem(const CFMod& mod, bool isModpack);
    void handleFilesReceived(const CFMod& mod,
                             const QVector<CFFileInfo>& files,
                             bool isModpack);

    // === Данные ===
    WindowFrame*            frame       = nullptr;
    CurseForgeClient*       m_cf        = nullptr;
    SettingsWindow*         m_settings  = nullptr;
    QNetworkAccessManager*  m_nam       = nullptr;

    QTabWidget*  m_tabs     = nullptr;

    // Вкладка «Моды»
    QLineEdit*   m_modSearch   = nullptr;
    QComboBox*   m_modVersion  = nullptr;
    QComboBox*   m_modLoader   = nullptr;
    QVBoxLayout* m_modCards    = nullptr;
    QHash<int, CFMod> m_modStore;

    // Вкладка «Модпаки»
    QLineEdit*   m_packSearch  = nullptr;
    QComboBox*   m_packVersion = nullptr;
    QVBoxLayout* m_packCards   = nullptr;
    QHash<int, CFMod> m_packStore;

    // Статус и прогресс
    QProgressBar* m_progress = nullptr;
    QLabel*       m_status   = nullptr;
};