#pragma once

#include <QDialog>
#include <QVector>
#include <QUrl>
#include <QEvent>

#include "modrinthapi.h"

class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;
class QVBoxLayout;
class QComboBox;
class SettingsWindow;

class ModWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModWindow(QWidget *parent = nullptr);

    void setSettingsWindow(SettingsWindow* sw)
    {
        settingsWindow = sw;
    }

    void setMinecraftVersion(const QString& version);
private slots:
    void onDownloadLinks(
        const QVector<QUrl>& urls);

    void applyFilters();
    void installMod(const Mod& mod);
private:
    QString selectedLoader;
    void clearCards();
    void addModCards(const QList<Mod>& mods);
    QString minecraftVersion;

    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;

    QScrollArea* scrollArea = nullptr;
    QWidget* cardsWidget = nullptr;
    QVBoxLayout* cardsLayout = nullptr;
    QComboBox* versionFilter = nullptr;
    QComboBox* loaderFilter = nullptr;

    ModrithAPI* api = nullptr;
    SettingsWindow* settingsWindow = nullptr;

    QVector<Mod> cachedMods;
};