#pragma once

#include "modrinthapi.h"

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QNetworkAccessManager>

class ModWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModWindow(QWidget *parent = nullptr);

private slots:
    void onDownloadLinks(const QVector<QUrl>& urls);

private:
    // Поиск
    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;

    // Список модов
    QListWidget* modList = nullptr;

    // Информация
    QLabel* descriptionLabel = nullptr;

    // Установка
    QPushButton* installButton = nullptr;

    // API Modrinth
    ModrithAPI* api = nullptr;

    // Скачивание файлов
    QNetworkAccessManager downloadManager;

    // Кэш найденных модов
    QVector<Mod> cachedMods;
};