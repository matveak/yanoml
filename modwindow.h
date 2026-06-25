#pragma once

#include <QDialog>
#include <QVector>
#include <QUrl>

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

private slots:
    void onDownloadLinks(const QVector<QUrl>& urls);
    void applyFilters();
    void installMod(const Mod& mod);
    void updateVersionsForLoader();
    void onAvailableVersions(const QString& loader, const QStringList& versions);

private:
    void clearCards();
    void addModCards(const QVector<Mod>& mods);
    void setupAdvancedFilters();

    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;

    QScrollArea* scrollArea = nullptr;
    QWidget* cardsWidget = nullptr;
    QVBoxLayout* cardsLayout = nullptr;

    QComboBox* versionFilter = nullptr;
    QComboBox* loaderFilter = nullptr;
    QComboBox* categoryFilter = nullptr;
    QComboBox* environmentFilter = nullptr;

    ModrithAPI* api = nullptr;
    SettingsWindow* settingsWindow = nullptr;
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};