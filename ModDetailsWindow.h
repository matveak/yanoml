#pragma once

#include <QDialog>

#include "modrinthapi.h"

class QLabel;
class QTextBrowser;
class QPushButton;
class QVBoxLayout;
class QNetworkAccessManager;

class ModDetailsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModDetailsWindow(
        const Mod& mod,
        QWidget* parent = nullptr);

private slots:
    void onProjectReceived(
        const ModProject& project);

private:
    Mod currentMod;

    ModrithAPI* api = nullptr;
    QNetworkAccessManager* manager = nullptr;

    QLabel* iconLabel = nullptr;
    QLabel* titleLabel = nullptr;
    QLabel* authorLabel = nullptr;
    QLabel* downloadsLabel = nullptr;
    QLabel* categoriesLabel = nullptr;
    QLabel* versionsLabel = nullptr;

    QTextBrowser* descriptionBrowser = nullptr;

    QPushButton* installButton = nullptr;

    QVBoxLayout* galleryLayout = nullptr;
};