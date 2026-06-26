#pragma once

#include <QDialog>
#include <QVector>
#include <QUrl>
#include <QList>

#include "modrinthapi.h"

class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;
class QVBoxLayout;
class QHBoxLayout;
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

    // Сайдбар фильтров
    QWidget* buildSidebar();
    QWidget* makeSection(const QString& title, QWidget* content,
                         QLabel** summaryOut, bool expanded);
    void updateSidebarSummaries();
    void buildCategoryList();
    void selectCategory(const QString& category);
    void selectEnvironment(const QString& environment);

    // Активные фильтры (чипы)
    void rebuildActiveFilters();
    void clearAllFilters();

    // Текущие значения фильтров
    QString currentVersion() const;
    QString currentLoader() const;

    // Форматирование
    static QString formatCount(quint64 value);
    static QString formatRelativeDate(const QString& iso);

    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;

    QScrollArea* scrollArea = nullptr;
    QWidget* cardsWidget = nullptr;
    QVBoxLayout* cardsLayout = nullptr;

    // Строка над карточками
    QComboBox* sortCombo = nullptr;
    QComboBox* viewCombo = nullptr;
    QWidget* chipsBar = nullptr;
    QHBoxLayout* chipsLayout = nullptr;

    // Сайдбар
    QComboBox* versionFilter = nullptr;
    QComboBox* loaderFilter = nullptr;
    QVBoxLayout* categoryListLayout = nullptr;
    QList<QPushButton*> categoryButtons;
    QPushButton* clientButton = nullptr;
    QPushButton* serverButton = nullptr;

    // Чипы-сводки под заголовками свёрнутых секций
    QLabel* versionSummary = nullptr;
    QLabel* loaderSummary = nullptr;
    QLabel* categorySummary = nullptr;
    QLabel* environmentSummary = nullptr;

    QString selectedCategory;      // "" = любая
    QString selectedEnvironment;   // "" = любая ("client"/"server")

    ModrithAPI* api = nullptr;
    SettingsWindow* settingsWindow = nullptr;
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};
