#pragma once
#include <QDialog>
#include <QVector>
#include <QHash>
#include <QUrl>
#include "curseforgeclient.h"

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
class SettingsWindow;
class QNetworkAccessManager;

class CurseForgeWindow : public QDialog
{
    Q_OBJECT
public:
    explicit CurseForgeWindow(QWidget* parent = nullptr);
    void setSettingsWindow(SettingsWindow* sw) { m_settings = sw; }

private slots:
    void onSearch();
    void onModsReceived(const QVector<CFMod>& mods);
    void onModpacksReceived(const QVector<CFMod>& packs);
    void onFilesReceived(int projectId, const QVector<CFFileInfo>& files);
    void downloadFile(const QUrl& url, const QString& fileName, bool isModpack);
    void onTabChanged(int index);

private:
    void buildModsTab();
    void buildModpacksTab();
    void addCards(const QVector<CFMod>& mods, QVBoxLayout* layout,
                  QHash<int, CFMod>& store, bool isModpack);
    void clearLayout(QVBoxLayout* l);
    void installItem(const CFMod& mod, bool isModpack);

    static QString formatCount(quint64 v);

    CurseForgeClient* m_cf   = nullptr;
    SettingsWindow*   m_settings = nullptr;
    QNetworkAccessManager* m_nam = nullptr;

    QTabWidget*  m_tabs = nullptr;

    // Моды вкладка
    QLineEdit*  m_modSearch   = nullptr;
    QComboBox*  m_modVersion  = nullptr;
    QComboBox*  m_modLoader   = nullptr;
    QVBoxLayout* m_modCards   = nullptr;
    QHash<int, CFMod> m_modStore;

    // Модпаки вкладка
    QLineEdit*  m_packSearch  = nullptr;
    QComboBox*  m_packVersion = nullptr;
    QVBoxLayout* m_packCards  = nullptr;
    QHash<int, CFMod> m_packStore;

    QProgressBar* m_progress  = nullptr;
    QLabel*       m_status    = nullptr;
};