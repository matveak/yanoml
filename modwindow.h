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

protected:
    bool eventFilter(QObject* obj,
                     QEvent* event) override;

private slots:
    void onDownloadLinks(
        const QVector<QUrl>& urls);

    void applyFilters();
private:
    QString selectedModId;
    QString selectedLoader;
private:
    void clearCards();

    void addModCards(
        const QList<Mod>& mods);

    void installSelectedMod();

private:
    QString minecraftVersion;

    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;

    QScrollArea* scrollArea = nullptr;
    QWidget* cardsWidget = nullptr;
    QVBoxLayout* cardsLayout = nullptr;

    QPushButton* installButton = nullptr;

    QComboBox* versionFilter = nullptr;
    QComboBox* loaderFilter = nullptr;

    ModrithAPI* api = nullptr;
    SettingsWindow* settingsWindow = nullptr;

    QVector<Mod> cachedMods;
};