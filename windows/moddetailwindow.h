#pragma once

#include <QDialog>

#include "../modrinthapi.h"

class QLabel;
class QTextBrowser;
class QPushButton;
class QVBoxLayout;
class QNetworkAccessManager;

class ModDetailsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModDetailsWindow(const Mod& mod, QWidget* parent = nullptr);

signals:
    void installRequested(const Mod& mod);

private slots:
    void onProjectReceived(const ModProject& project);

private:
    ModrithAPI            *m_api                = nullptr;
    QNetworkAccessManager *m_manager            = nullptr;
    QLabel                *m_iconLabel          = nullptr;
    QLabel                *m_titleLabel         = nullptr;
    QLabel                *m_authorLabel        = nullptr;
    QLabel                *m_downloadsLabel     = nullptr;
    QLabel                *m_categoriesLabel    = nullptr;
    QLabel                *m_versionsLabel      = nullptr;
    QTextBrowser          *m_descriptionBrowser = nullptr;
    QPushButton           *m_installButton      = nullptr;
    QVBoxLayout           *m_galleryLayout      = nullptr;
    Mod                    m_currentMod;
};